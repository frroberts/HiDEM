//
// Created by Fredrik Robertsen on 09/10/2020.
//

#ifndef PHELL_TESTHELPERS_H
#define PHELL_TESTHELPERS_H

#include <memory>

#include <Geometry/Map2d.h>
#include <Util/Real3.h>

#include "ParticleDataHandler.h"
#include "../HiDEMLib/HiDEMParticleData.h"

void generateBox(ParticleDataHandler<HiDEMParticleData> *data, int size, int haloLayer, bool shuffle = false);

std::shared_ptr<Map2d<lpReal_t>>  generateGeometryPlane(Real3 low, Real3 high, double level);

ManVec<ElementaryParticle> make3Particles();
ManVec<ElementaryParticle> make4Particles();

#endif //PHELL_TESTHELPERS_H
