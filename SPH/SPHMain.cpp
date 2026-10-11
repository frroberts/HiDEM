//
// Created by Fredrik Robertsen on 16/06/2021.
//



#include <string>
#include <chrono>
#include <iostream>
//#include <filesystem>

#include <math.h>
#include <mpi.h>

#include "../Geometry/HeightMapLoader.h"
#include "Generators/FCCParticleGenerator.h"
#include "../Iterators/Iterator.h"
#include "Generators/LinkGenerator.h"
#include "../PLiB/GlobalLocalMapper.h"
#include "../PLiB/World.h"
#include "../Distribution/Distribution.h"

#include "../Logger/Logger.h"
#include "../Logger/EventCollection.h"

#include "../H5IO/H5Checkpoint.h"

//#include "Distribution/DistributionBuilder.h"

#include "../Util/ArgParser.h"
#include "../HiDEM/HiDEMBuilder.h"
#include "../Util/Util.h"
#include "Generators/CubicParticleGenerator.h"
#include "SPHParticleData.h"
#include "SPHTransportParticle.h"


int main(int argc, char *argv[]) {

    ArgParser arguments(argc, argv);

    //exit(0);
    MPI_Init(&argc, &argv);

    EventCollection::syncClocks();
    EventCollection::setTimestep(-1);

    auto workingPathStr = arguments.getArg<std::string>("input.workingPath").value_or("");
    auto outParticleName = arguments.getArg<std::string>("input.outParticleName").value_or("pOut");

    int timeSteps = arguments.getArg<int>("iteration.timeSteps").value_or(10000);

    int outputInterval = arguments.getArg<int>("iteration.outputInterval").value_or(1);

    // grid spacing, particle and like lengths and neighbour search parameters
    double fccSpacing = arguments.getArg<double>("geometry.fccSpacing").value_or(pow(2.0, 2.0 / 3.0));
    auto searchDistanceAbs = arguments.getArg<double>("grid.searchDistanceAbs");
    auto searchDistanceScale = arguments.getArg<double>("grid.searchDistanceScale");
    auto gridRegenInterval = arguments.getArg<int>("grid.gridRegenInterval").value_or(100);

    double particleDiameter = sqrt(pow((fccSpacing / 2.0), 2.0) * 2.0);

    double dt = arguments.getArg<double>("dt").value_or(0.0001);
    double scale = arguments.getArg<double>("scale").value_or(1.0);

    particleDiameter *= scale;

    double g = arguments.getArg<double>("g").value_or(9.81);

    
    auto xInputUpper = arguments.getArg<double>("geometry.xInputUpper");
    auto yInputUpper = arguments.getArg<double>("geometry.yInputUpper");
    auto zInputUpper = arguments.getArg<double>("geometry.zInputUpper");
    auto xInputLower = arguments.getArg<double>("geometry.xInputLower");
    auto yInputLower = arguments.getArg<double>("geometry.yInputLower");
    auto zInputLower = arguments.getArg<double>("geometry.zInputLower");
    auto spacing = arguments.getArg<double>("geometry.spacing");

    bool transport = arguments.getArg<bool>("distribution.transport").value_or(false);
    bool reBalance = arguments.getArg<bool>("distribution.reBalance").value_or(false);
    double haloScale = arguments.getArg<double>("distribution.haloScale").value_or(100.0);
    int redistributionInterval = arguments.getArg<double>("distribution.redistributionInterval").value_or(100);

    Distribution distr(reBalance);

    EventCollection::setupOutput("timing_" + std::to_string(distr.world.self) + ".csv");

    double lift;
    double particleMass;
    double particleMomentInertia;

    double searchDistance;
    // if the absolute serach distance is defined use it
    if (searchDistanceAbs) {
        searchDistance = searchDistanceAbs.value_or(1);
        if (distr.world.self == 0) {
            std::cout << "searchDistance: " << searchDistance << " set from absolute distance " << std::endl;
        }
    }
        // else use the serach distance scale from the fcc spacing
    else {
        searchDistance = searchDistanceScale.value_or(1.2) * particleDiameter;
        if (distr.world.self == 0) {
            std::cout << "searchDistance: " << searchDistance << " set from searchDistanceScale * particleDiameter " << std::endl;
        }
    }

    int startTs = 0;


    Event setupEvent;
    setupEvent.start();

    HeightMapLoader loader;
    if (distr.world.self == 0) {
        std::cout << "---------------------------------------" << "\n";
    }

    Real3 lowerLimit = {xInputLower.value_or(0), yInputLower.value_or(0), zInputLower.value_or(0)};
    Real3 upperLimit = {xInputUpper.value_or(0), yInputUpper.value_or(0), zInputUpper.value_or(0)};

    distr.generateDomains(1, 1, lowerLimit, upperLimit, searchDistance * haloScale);

    if (distr.world.self == 0) {
        for (auto d: distr.world.domains) {
            d.print();
        }
    }

    CubicParticleGenerator<SPHTransportParticle> pGen;

    auto particles = pGen.generate((upperLimit - lowerLimit) / 2.0, (upperLimit - lowerLimit) / 4.0, spacing.value_or(1), Domain());

    std::cout << "generated " << particles.size() << " particles " << std::endl;

    SPHParticleData *pDataPtr = new SPHParticleData;
    pDataPtr->appendTransportParticles(particles);

    GlobalLocalMapper pMapper;
    pDataPtr->remap(pMapper);


    ParticleTrasporter<SPHParticleData, SPHTransportParticle> pTrans;
    pTrans.getOutgoingHalo(distr.world, *pDataPtr);
    pTrans.getIncomming();
    pTrans.appendIncomming(*pDataPtr);


    SPHParticleData &pData = *pDataPtr;

    pMapper.clear();

    pData.remap(pMapper);

    GlobalLocalMapper lMapper;

    auto gridPtr = new NeighbourGrid(searchDistance);

    gridPtr->generateNeighbourGrid(pData);
    gridPtr->colourParticles(pData);
    gridPtr->generateIterationList(pData);

    // needs a tight grid to get the best performance, there has to be a better way ....
    auto gridPtrReorder = new NeighbourGrid(searchDistance);

    NeighbourGrid *grids[4];
    grids[0] = new NeighbourGrid(searchDistance);
    grids[1] = new NeighbourGrid(searchDistance * 2);
    grids[2] = new NeighbourGrid(searchDistance * 4);
    grids[3] = new NeighbourGrid(searchDistance * 8);


    double invDt = 1.0 / dt;
    double dt2 = 2.0 * dt;
    double invDt2 = 1.0 / (2.0 * dt);
    double dtSqr = pow(dt, 2.0);
    double invDtSqr = 1.0 / pow(dt, 2.0);


    setupEvent.end();
    MPI_Barrier(MPI_COMM_WORLD);
    Logger::writeAtFirst("Setup done, setup time: " + Logger::getLimits(setupEvent.getLastDuration(), "s") + "\n");

    MPI_Barrier(MPI_COMM_WORLD);

    bool indirectNeighbours = true;

    NeighbourGrid &grid = *gridPtr;

    std::mt19937 gen(1);
    int self = distr.world.self;


    // ------------------------------------------------
    //           Iteration loop
    // ------------------------------------------------
    MPI_Barrier(MPI_COMM_WORLD);

    auto iterStart = std::chrono::steady_clock::now();

    ManVec<ind_t> reOrder;
    ManVec<ind_t> reOrderIndexes;
    for (int ts = startTs; ts < timeSteps; ++ts) {
        EventCollection::setTimestep(ts);
        // std::cout << "timestep start " << ts << std::endl;
        // get a reference to the grid, not the prettiest // dont touch
        NeighbourGrid &grid = *gridPtr;

        bool needsRedistribute = false;
        bool needsGridRegen = false;
        bool needsSorting = false; // todo not implemented

        needsGridRegen = ts % gridRegenInterval == 0;
        needsRedistribute = ts % redistributionInterval == 0;

        if (needsGridRegen) {
            // regenerate neighbour list
            EventCollection::startEvent("grid generate");
            gridPtr->generateNeighbourGrid(pData);
            gridPtr->colourParticles(pData);
            gridPtr->generateIterationList(pData);
            grid = *gridPtr;
            EventCollection::endEvent("grid generate");
        }

        MPI_Barrier(MPI_COMM_WORLD);

        auto tsStart = std::chrono::steady_clock::now();

        pData.iterateAllParticles(OffloadExec(), [=] DEVCALLABLE (ind_t pInd, SPHParticleData &pData, NeighbourGrid &grid){
            // zero forces
            pData.xForce[pInd] = 0;
            pData.yForce[pInd] = 0;
            pData.zForce[pInd] = 0;

            }, pData, grid);

        pData.iterateAllParticles(OffloadExec(), [=] DEVCALLABLE (ind_t pInd, SPHParticleData &pData, NeighbourGrid &grid){

            /*
            grid.iteratePossibleNeighbours([&](size_t nInd) {
                // whatever we need fro the neighbours
            }, grid.getPraticleColor(pInd), true);
*/
            }, pData, grid);


        // apply forces
        pData.iterateAllParticles(OffloadExec(), [=] DEVCALLABLE (ind_t pInd, SPHParticleData &pData, NeighbourGrid &grid){

            }, pData, grid);

        Iterator::syncAll();
        auto tsStop = std::chrono::steady_clock::now();
        // stats and output below this ....
        std::chrono::duration<double> elapsedTs = tsStop - tsStart;
        if ((ts % 200 == 0 && ts != 0) || ts == 39) {
            Logger::writeAtFirst("timestep done " + std::to_string(ts) + "\n");
            EventCollection::writeAccumulated();
            //EventCollection::dumpHistory();
            EventCollection::clearHistory();
            Logger::writeAtFirst("  timestep time: " + Logger::getLimits(elapsedTs.count(), "s") + "\n");
        }

        // print out timestep time every Xth step
        if (ts % outputInterval == 0) {
            //
        }

    }
    MPI_Barrier(MPI_COMM_WORLD);
    auto iterEnd = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsedIter = iterEnd - iterStart;
    Logger::writeAtFirst(fmt::format("Runtime {} \n", Logger::getLimits(elapsedIter.count(), "s")));

    auto outputStart = std::chrono::steady_clock::now();

    //pData.toCSV(workingPathStr + "/" + (std::to_string(distr.world.self) + "_" + outParticleName + ".csv"));

    auto outputStop = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsedOutput = outputStop - outputStart;
    std::cout << "output time: " << elapsedOutput.count() << "\n";

    // for asan to not cry as much
    delete pDataPtr;
    delete gridPtr;

    MPI_Finalize();

    return 0;
}
