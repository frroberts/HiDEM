//
// Created by Fredrik Robertsen on 9/5/25.
//

#ifndef FILEPARTICLEGENERATOR_H
#define FILEPARTICLEGENERATOR_H

class FileParticleGenerator : public ParticleGenerator {
    std::string filename;
    std::string xCordName;
    std::string yCordName;
    std::string zCordName;
    std::string radiusName;

public:
    FileParticleGenerator(const std::string &fileName, const std::string &xCordName, const std::string &yCordName, const std::string &zCordName,
                          const std::string &radiusName) : filename(fileName), xCordName(xCordName), yCordName(yCordName), zCordName(zCordName),
                                                           radiusName(radiusName) {
    }

    virtual void
    generate(Domain &dom, std::vector<std::shared_ptr<InputGeometry> > geometries, const std::vector<Map<double> *> &generateProbabilities, Real3 offset,
             ManVec<ElementaryParticle> &particles) {
        HDF5IO reader;
        reader.openFile(filename);
        size_t globalDSetSize  = reader.getDataSetSize(xCordName);
        int numRanks;
        MPI_Comm_size(MPI_COMM_WORLD, &numRanks);

        size_t dSetSize = 1+(globalDSetSize / numRanks);
        int self = getLocalRank();
        if ((self+1)*dSetSize > globalDSetSize) {
            dSetSize = globalDSetSize - self*dSetSize;
        }

        HDF5Span span;
        span.localCount = dSetSize;
        span.localOffset = self*dSetSize;
        span.globalCount = globalDSetSize;

        std::random_device r;
        std::mt19937 randomEngine(r());
        std::uniform_real_distribution<double> uniformDist(0, 1);

        auto xCords = reader.readVectorSpann<float>(xCordName, span);
        auto yCords = reader.readVectorSpann<float>(yCordName, span);
        auto zCords = reader.readVectorSpann<float>(zCordName, span);
        auto radius = reader.readVectorSpann<float>(radiusName, span);

        // reader.closeFile();
        std::vector<float> xRecv;
        std::vector<float> yRecv;
        std::vector<float> zRecv;
        std::vector<float> radiusRecv;

        size_t dRecvMax = 1+(globalDSetSize / numRanks);
        xRecv.resize(dRecvMax);
        yRecv.resize(dRecvMax);
        zRecv.resize(dRecvMax);
        radiusRecv.resize(dRecvMax);

        std::vector<MPI_Request> reqsFull;
        reqsFull.resize(4*numRanks);

        // could be rewritten as one-sided if someone feels adventurous

        for (int rank = 0; rank < numRanks; rank++) {
            size_t sendCount = xCords.size();

            MPI_Isend(&sendCount, 1, MPI_UNSIGNED_LONG, (self + rank) % numRanks, 0, MPI_COMM_WORLD, &reqsFull[0 + (4 * rank)]);

            MPI_Isend(xCords.data(), sendCount, MPI_FLOAT, (self + rank) % numRanks, 0, MPI_COMM_WORLD, &reqsFull[1 + (4 * rank)]);
            MPI_Isend(yCords.data(), sendCount, MPI_FLOAT, (self + rank) % numRanks, 0, MPI_COMM_WORLD, &reqsFull[2 + (4 * rank)]);
            MPI_Isend(zCords.data(), sendCount, MPI_FLOAT, (self + rank) % numRanks, 0, MPI_COMM_WORLD, &reqsFull[3 + (4 * rank)]);
            MPI_Isend(radius.data(), sendCount, MPI_FLOAT, (self + rank) % numRanks, 0, MPI_COMM_WORLD, &reqsFull[3 + (4 * rank)]);
        }

        for (int rank = 0; rank < numRanks; rank++) {
            size_t recvCount = 0;

            MPI_Recv(&recvCount, 1, MPI_UNSIGNED_LONG, (self + rank) % numRanks, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            MPI_Recv(xRecv.data(), recvCount, MPI_FLOAT, (self + rank) % numRanks, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(yRecv.data(), recvCount, MPI_FLOAT, (self + rank) % numRanks, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(zRecv.data(), recvCount, MPI_FLOAT, (self + rank) % numRanks, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(radiusRecv.data(), recvCount, MPI_FLOAT, (self + rank) % numRanks, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            for (size_t i = 0; i < recvCount; i++) {
                this->createParticle({xRecv[i], yRecv[i], zRecv[i]}, self, particles, dom, geometries,
                                     generateProbabilities, offset, radiusRecv[i], uniformDist(randomEngine));
            }
        }

        MPI_Waitall(reqsFull.size(), reqsFull.data(), MPI_STATUSES_IGNORE);

        this->setParticleKeys(particles);
    }
};

#endif //FILEPARTICLEGENERATOR_H

