//
// Created by Fredrik Robertsen on 30/08/2022.
//

#include <iostream>
#include <random>

#include "../HiDEM/HiDEMParticleData.h"

#ifdef GPU
#include "DevPortabilityLayer.h"
#endif

__global__ void copyKern(double *a, double *b, double *c, int count) {
    //
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx >= count)
        return;

    a[idx] = b[idx] + 10.0 * c[idx];

}


__global__ void  copyKernCTDummy(
        double *xCord,
        double *yCord,
        double *zCord,
        double *xTheta,
        double *yTheta,
        double *zTheta,
        double *xCordPrev,
        double *yCordPrev,
        double *zCordPrev,
        double *xThetaPrev,
        double *yThetaPrev,
        double *zThetaPrev,
        int count, int offset) {
    //
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx >= count)
        return;

    int structStart = blockIdx.x * blockDim.x * 12;
    for (int i = threadIdx.y; i < 12; i = i + blockDim.y) {
        lInd_t ctInd = (blockIdx.x * blockDim.x * 12) + threadIdx.x + (i*blockDim.x);  // no need to reread size here // set ctind to -1 if link broken ?

        double CT0 = xCord[ctInd];
        double CT1 = yCord[ctInd];
        double CT2 = zCord[ctInd];
        double CT3 = xTheta[ctInd];
        double CT4 = yTheta[ctInd];
        double CT5 = zTheta[ctInd];

        xCord[ctInd] = CT0+1;
        yCord[ctInd] = CT1+1;
        zCord[ctInd] = CT2+1;
        xTheta[ctInd] = CT3+1;
        yTheta[ctInd] = CT4+1;
        zTheta[ctInd] = CT5+1;
    }
/*
    for (int i = threadIdx.y; i < 12; i = i + blockDim.y) {
        lInd_t ctInd = idx + (i*offset);  // no need to reread size here // set ctind to -1 if link broken ?

        double CT0 = xCord[ctInd];
        double CT1 = yCord[ctInd];
        double CT2 = zCord[ctInd];
        double CT3 = xTheta[ctInd];
        double CT4 = yTheta[ctInd];
        double CT5 = zTheta[ctInd];

        xCord[ctInd] = CT0+1;
        yCord[ctInd] = CT1+1;
        zCord[ctInd] = CT2+1;
        xTheta[ctInd] = CT3+1;
        yTheta[ctInd] = CT4+1;
        zTheta[ctInd] = CT5+1;
    }
*/
}

__global__ void  copyKern6(
        double *xCord,
        double *yCord,
        double *zCord,
        double *xTheta,
        double *yTheta,
        double *zTheta,
        double *xCordPrev,
        double *yCordPrev,
        double *zCordPrev,
        double *xThetaPrev,
        double *yThetaPrev,
        double *zThetaPrev,
        double *xForce,
        double *yForce,
        double *zForce,
        double *uForce,
        double *vForce,
        double *wForce,
        int count) {
    //
    int idx = blockIdx.x * blockDim.x + threadIdx.x;

    if (idx >= count)
        return;

    double xPrev = xCordPrev[idx];
    double yPrev = yCordPrev[idx];
    double xF =  xForce[idx];
    double yF =  yForce[idx];
    xCord[idx] = xPrev + 10.0 * xF;
    yCord[idx] = yPrev + 10.0 * yF;
//    zCord[idx] = zCordPrev[idx] + 10.0 * zForce[idx];
//    xTheta[idx] = xThetaPrev[idx] + 10.0 * uForce[idx];
//    yTheta[idx] = yThetaPrev[idx] + 10.0 * vForce[idx];
//    zTheta[idx] = zThetaPrev[idx] + 10.0 * wForce[idx];

}

int main(int argc, char *argv[]) {

    MPI_Init(&argc, &argv);

    int self;
    int size;
    int selectedDevice = -1;

    // get the number of devices
    int devices;
    std::cout <<  devGetDeviceCount(&devices) << std::endl;
    std::cout << "found " << devices << " devices" << std::endl;

    // split MPI_COMM_WORLD into shared memory communicator
    MPI_Comm shardMemComm;
    MPI_Info info;
    MPI_Info_create(&info);
    MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, info, &shardMemComm);

    // get the id of the rank within the shared memory communicator
    // this effectively gives the id of the rank on its compute node
    int myRankOnNode;
    MPI_Comm_rank(shardMemComm, &myRankOnNode);

    // set the device based on the id of the rank on the node
    // % number of devices to allow for oversubscription
    devSetDevice(myRankOnNode%devices);

    // printing of device selection
    devGetDevice(&selectedDevice);
    char hostname[128];
    int resLen;
    MPI_Get_processor_name(hostname, &resLen);
    std::cout << "rank " << self << " is rank " << myRankOnNode << " on host " << std::string(hostname) << " and was assigned device " << selectedDevice << std::endl;

    HiDEMParticleData *pDataPtr = new HiDEMParticleData();
    HiDEMParticleData &pData = *pDataPtr;

    Real3 lowerLimit;
    Real3 upperLimit;


    lowerLimit.x = -50;
    lowerLimit.y = -50;
    lowerLimit.z = -50;
    upperLimit.x = 50;
    upperLimit.y = 50;
    upperLimit.z = 50;

    int count = std::atoi(argv[1]); // 26066880
    pData.xCord.resize(count+10240);
    pData.yCord.resize(count+10240);
    pData.zCord.resize(count+10240);
    pData.xCordPrev.resize(count+10240);
    pData.yCordPrev.resize(count+10240);
    pData.zCordPrev.resize(count+10240);

    pData.xTheta.resize(count+10240);
    pData.yTheta.resize(count+10240);
    pData.zTheta.resize(count+10240);
    pData.xThetaPrev.resize(count+10240);
    pData.yThetaPrev.resize(count+10240);
    pData.zThetaPrev.resize(count+10240);

    pData.xForce.resize(count+10240);
    pData.yForce.resize(count+10240);
    pData.zForce.resize(count+10240);
    pData.uForce.resize(count+10240);
    pData.vForce.resize(count+10240);
    pData.wForce.resize(count+10240);

    pData.xDiff.resize(count+10240);
    pData.yDiff.resize(count+10240);
    pData.zDiff.resize(count+10240);
    pData.uDiff.resize(count+10240);
    pData.vDiff.resize(count+10240);
    pData.wDiff.resize(count+10240);

    std::default_random_engine e1(1);
    std::uniform_real_distribution<double> uniform_dist(-50, 50);

    for (int i = 0; i < count; ++i) {
        pData.xCord[i] = uniform_dist(e1);
        pData.yCord[i] = uniform_dist(e1);
        pData.zCord[i] = uniform_dist(e1);
        pData.xTheta[i] = uniform_dist(e1);
        pData.yTheta[i] = uniform_dist(e1);
        pData.zTheta[i] = uniform_dist(e1);

        pData.xCordPrev[i] = pData.xCord[i];
        pData.yCordPrev[i] = pData.yCord[i];
        pData.zCordPrev[i] = pData.zCord[i];
        pData.xThetaPrev[i] = pData.xTheta[i];
        pData.yThetaPrev[i] = pData.yTheta[i];
        pData.zThetaPrev[i] = pData.zTheta[i];

    }

    double bedStiffness = 2e+08;

    double waterLevel = 0.0;

    double dt = 0.0001;
    double g = 9.81;
    double rhoWater = 1030.0;
    double rhoIce = 910.0;

    double lift;
    double particleMass;
    double particleMomentInertia;

    lift = (rhoWater - rhoIce) / rhoIce * g;
    particleMass = rhoIce;
    particleMomentInertia = particleMass / 6.0;

    double invDt = 1.0 / dt;
    double invDt2 = 1.0 / (2.0 * dt);
    double dtSqr = pow(dt, 2.0);
    double invDtSqr = 1.0 / pow(dt, 2.0);

    double pMassDtSqr = particleMass / dtSqr;
    double pMemDtSqr = (particleMomentInertia / dtSqr);
    double maxVel = 20;
    double maxRot = 5;

    double *xCordPtr = &pData.xCord[0];
    double *yCordPtr = &pData.yCord[0];
    double *zCordPtr = &pData.zCord[0];
    double *uCordPtr = &pData.xTheta[0];
    double *vCordPtr = &pData.yTheta[0];
    double *wCordPtr = &pData.zTheta[0];

    double *xCordPrevPtr = &pData.xCordPrev[0];
    double *yCordPrevPtr = &pData.yCordPrev[0];
    double *zCordPrevPtr = &pData.zCordPrev[0];
    double *uCordPrevPtr = &pData.xThetaPrev[0];
    double *vCordPrevPtr = &pData.yThetaPrev[0];
    double *wCordPrevPtr = &pData.zThetaPrev[0];

    double *xForcePtr = &pData.xForce[0];
    double *yForcePtr = &pData.yForce[0];
    double *zForcePtr = &pData.zForce[0];
    double *uForcePtr = &pData.uForce[0];
    double *vForcePtr = &pData.vForce[0];
    double *wForcePtr = &pData.wForce[0];

    std::cout << xCordPtr << std::endl;
    std::cout << yCordPtr << std::endl;
    std::cout << zCordPtr << std::endl;
    std::cout << uCordPtr << std::endl;
    std::cout << vCordPtr << std::endl;
    std::cout << wCordPtr << std::endl;

    for (int i = 0; i < 10; ++i) {
        //         static void iterate([[maybe_unused]] OffloadExec pol, F fun, Tind max, [[maybe_unused]] Args &... args) {
        auto start = std::chrono::steady_clock::now();

        uint32_t blockSizeX = 128;
        uint32_t grd = 1+(count/128);

        dim3 threads{blockSizeX, 1, 1};

        copyKern<<<grd,threads>>>(pData.xCord.ptr, pData.xCordPrev.ptr, pData.xForce.ptr, count);
        Iterator::syncAll();

        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;

        std::cout << "achieved BW " << (double) count * 3 * 8 / 1000.0 / 1000.0 / 1000.0 / elapsed_seconds.count() << "GB/s "<< elapsed_seconds.count() <<"\n";
    }
    std::cout << std::endl;


    for (int i = 0; i < 10; ++i) {
        auto start = std::chrono::steady_clock::now();

        int NAccelxGrid = 64;
        int blockSize = 768;
        uint32_t blockSizeXN = NAccelxGrid;
        uint32_t blockSizeYN = blockSize/NAccelxGrid;
        uint32_t grdN = 1+(count/12/NAccelxGrid);
        dim3 threadsN{blockSizeXN, blockSizeYN, 1};


        copyKernCTDummy<<<grdN,threadsN>>>(
                pData.xCord.ptr,
                pData.yCord.ptr,
                pData.zCord.ptr,
                pData.xTheta.ptr,
                pData.yTheta.ptr,
                pData.zTheta.ptr,
                pData.xCordPrev.ptr,
                pData.yCordPrev.ptr,
                pData.zCordPrev.ptr,
                pData.xThetaPrev.ptr,
                pData.yThetaPrev.ptr,
                pData.zThetaPrev.ptr,
                count/12,count/12);

        Iterator::syncAll();

        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;

        std::cout << "achieved BW CT " << (double) count * 2 * 8 / 1000.0 / 1000.0 / 1000.0 / elapsed_seconds.count() *6 << "GB/s "<< elapsed_seconds.count() <<"\n";
    }
    std::cout << std::endl;

    /*

    for (int i = 0; i < 10; ++i) {
        //         static void iterate([[maybe_unused]] OffloadExec pol, F fun, Tind max, [[maybe_unused]] Args &... args) {
        auto start = std::chrono::steady_clock::now();
        Iterator::iterate(OffloadExec(), [=] DEVLAMBDA(int pInd, HiDEMParticleData &pData) {

            pData.xCord[pInd] = pData.xCordPrev[pInd] + 10.0 * pData.xForce[pInd];
        }, count, pData);

        Iterator::syncAll();

        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;

        std::cout << "achieved BW " << (double) count * 3 * 8 / 1000.0 / 1000.0 / 1000.0 / elapsed_seconds.count() *6 << "GB/s "<< elapsed_seconds.count() <<"\n";
    }
    std::cout << std::endl;

    for (int i = 0; i < 10; ++i) {
        //         static void iterate([[maybe_unused]] OffloadExec pol, F fun, Tind max, [[maybe_unused]] Args &... args) {
        auto start = std::chrono::steady_clock::now();
        uint32_t blockSizeX = 128;
        uint32_t grd = 1+(count/128);
        dim3 threads{blockSizeX, 1, 1};

        copyKern6<<<grd,threads>>>(
                pData.xCord.ptr,
                pData.yCord.ptr,
                pData.zCord.ptr,
                pData.xTheta.ptr,
                pData.yTheta.ptr,
                pData.zTheta.ptr,
                pData.xCordPrev.ptr,
                pData.yCordPrev.ptr,
                pData.zCordPrev.ptr,
                pData.xThetaPrev.ptr,
                pData.yThetaPrev.ptr,
                pData.zThetaPrev.ptr,
                pData.xForce.ptr,
                pData.yForce.ptr,
                pData.zForce.ptr,
                pData.uForce.ptr,
                pData.vForce.ptr,
                pData.wForce.ptr,
                count);

        Iterator::syncAll();

        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;

        std::cout << "achieved BW " << (double) count * 3 * 8 / 1000.0 / 1000.0 / 1000.0 / elapsed_seconds.count() *6 << "GB/s "<< elapsed_seconds.count() <<"\n";
    }
    std::cout << std::endl;
*/

    for (int i = 0; i < 10; ++i) {
        //         static void iterate([[maybe_unused]] OffloadExec pol, F fun, Tind max, [[maybe_unused]] Args &... args) {
        auto start = std::chrono::steady_clock::now();
        Iterator::iterate(ParallelExec(), [=] DEVLAMBDA(int pInd, HiDEMParticleData &pData) {

            pData.xCord[pInd] = pData.xCordPrev[pInd] + 10.0 * pData.xForce[pInd];
            pData.yCord[pInd] = pData.yCordPrev[pInd] + 10.0 * pData.yForce[pInd];
            pData.zCord[pInd] = pData.zCordPrev[pInd] + 10.0 * pData.zForce[pInd];
            pData.xTheta[pInd] = pData.xThetaPrev[pInd] + 10.0 * pData.uForce[pInd];
            pData.yTheta[pInd] = pData.yThetaPrev[pInd] + 10.0 * pData.vForce[pInd];
            pData.zTheta[pInd] = pData.zThetaPrev[pInd] + 10.0 * pData.wForce[pInd];

            /*
            xCordPtr[pInd] = xCordPrevPtr[pInd] + 10.0 * xForcePtr[pInd];
            yCordPtr[pInd] = yCordPrevPtr[pInd] + 10.0 * yForcePtr[pInd];
            zCordPtr[pInd] = zCordPrevPtr[pInd] + 10.0 * zForcePtr[pInd];
            uCordPtr[pInd] = uCordPrevPtr[pInd] + 10.0 * uForcePtr[pInd];
            vCordPtr[pInd] = vCordPrevPtr[pInd] + 10.0 * vForcePtr[pInd];
            wCordPtr[pInd] = wCordPrevPtr[pInd] + 10.0 * wForcePtr[pInd];
             */
        }, count, pData);

        Iterator::syncAll();

        auto end = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed_seconds = end - start;

        std::cout << "achieved BW " << (double) count * 3 * 8 / 1000.0 / 1000.0 / 1000.0 / elapsed_seconds.count() *6 << "GB/s "<< elapsed_seconds.count() <<"\n";
    }
    std::cout << std::endl;



}