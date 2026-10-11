//
// Created by Fredrik Robertsen on 09/10/2020.
//

#include "TestHelpers.h"
#include <algorithm>
#include <random>

#include "ParticleDataHandler.h"

void generateBox(ParticleDataHandler<HiDEMParticleData> *data, int size, int haloLayer, bool shuffle)
{
    //
    ManVec<ElementaryParticle> particles;
    int key = 0;
    for (int x = 0; x < size; ++x) {
        for (int y = 0; y < size; ++y) {
            for (int z = 0; z < size; ++z) {
                //
                ElementaryParticle p;
                if (x<haloLayer || x >= size-haloLayer ||
                    y<haloLayer || y >= size-haloLayer ||
                    z<haloLayer || z >= size-haloLayer)
                {
                    p.owner = 1;
                }
                else
                {
                    p.owner = 0;
                }
                p.cord = {(double)x,(double)y,(double)z};
                p.globalKey = key;
                particles.push_back(p);
                key++;
            }
        }
    }

    if(shuffle) {
        std::random_device rd;
        std::mt19937 g(1);
        std::shuffle(particles.begin(), particles.end(), g);
    }

    data->appendInitParticles(particles);
}

std::shared_ptr<Map2d<lpReal_t>> generateGeometryPlane(Real3 low, Real3 high, double level) {

    std::shared_ptr<Map2d<lpReal_t>> ret = std::make_shared<Map2d<lpReal_t>>(2,2,low.x, low.y, high.x, high.y);
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            ret->setPointInd(i,j,level);
        }
    }
    return ret;
}

ManVec<ElementaryParticle> make3Particles() {
    ManVec<ElementaryParticle> particles;

    ElementaryParticle p1;
    p1.globalKey = 0;
    p1.owner = 0;
    p1.cord = {0};
    particles.push_back(p1);

    ElementaryParticle p2;
    p2.globalKey = 1;
    p2.owner = 0;
    p2.cord = {1};
    particles.push_back(p2);

    ElementaryParticle p3;
    p3.globalKey = 2;
    p3.owner = 0;
    p3.cord = {1,0,0};
    particles.push_back(p3);

    return particles;
}

ManVec<ElementaryParticle> make4Particles() {
    ManVec<ElementaryParticle> particles;

    ElementaryParticle p1;
    p1.globalKey = 0;
    p1.owner = 0;
    p1.cord = {0};
    particles.push_back(p1);

    ElementaryParticle p2;
    p2.globalKey = 1;
    p2.owner = 0;
    p2.cord = {1};
    particles.push_back(p2);

    ElementaryParticle p3;
    p3.globalKey = 2;
    p3.owner = 0;
    p3.cord = {1,0,0};
    particles.push_back(p3);


    ElementaryParticle p4;
    p4.globalKey = 3;
    p4.owner = 0;
    p4.cord = {1,0,1};
    particles.push_back(p4);

    return particles;
}



