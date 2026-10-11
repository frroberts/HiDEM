# Link regeneration (Viscoelastic ice simulation)

# This is a prototype for now and may be updated in the future

This sample shows how to run a simulation that regenerates links as it runs, so links that can break and then get regenerated.

The regeneration works by every time it regenerates the neighbour list it also generates broken links to all neighbours within reach. The `linkUpdateRegen` function will then iterate over all links even the broken ones and you can then choose to reenable them or not.

The sample also uses `BulkRandom<lpReal_t` to generate random probabilities to choose if links should be regenerated or broken. The `BulkRandom` object is is sized based on the number of links in lData, the numLocal value includes both broken and active links so it will be sized such that every link iterated over will have one random value generated for it. For the random numbers it is important to regenerate them every iteration, that is calling `reGenerate` on the random data.

## Code highlights

The link update function that does the regeneration

```c++
    lpReal_t linkLenLimit = (radius+radius)*hiDem.geometry.linkGenScale;
    // prob is a random number "uniformly distributed on the interval [a,b)" with a = 0 b = 1 (1 is excluded)
    auto breakLinks = [=]
    DEVCALLABLE REALT_TEMPL (
        const hpReal_t &xCord1, const hpReal_t &yCord1, const hpReal_t &zCord1,
        const lpReal_t &xTheta1, const lpReal_t &yTheta1, const lpReal_t &zTheta1,
        const hpReal_t &xCord2, const hpReal_t &yCord2, const hpReal_t &zCord2,
        const lpReal_t &xTheta2, const lpReal_t &yTheta2, const lpReal_t &zTheta2,
        uint8_t &state, lpReal_t &linkLen, lpReal_t &prob
    ) {
        // only write to lData.XXX[lInd]
        lpReal_t xDist = xCord1 - xCord2;
        lpReal_t yDist = yCord1 - yCord2;
        lpReal_t zDist = zCord1 - zCord2;
        lpReal_t currentLength = sqrtf((xDist * xDist) + (yDist * yDist) + (zDist * zDist));

        if (state == 0) {
            if ((currentLength < linkLenLimit) && (prob > 0.995f)) {
                state = 1;
            }
        } else {
            lpReal_t maxBend = std::max(abs(xTheta1 - xTheta2),
                                        std::max(abs(yTheta1 - yTheta2), abs(zTheta1 - zTheta2)));
            lpReal_t load = (currentLength - linkLen) / linkLen + bendBreak * maxBend;

            if (load >= maxLoad) {
                state = 0;
            }

            if (prob < 1.0f - exp(-50.0f * load)) {
                state = 0;
            }
        }
    };
```

The simplified time step loop

```c++
    BulkRandom<lpReal_t> breakProbs;

    while (hiDem.run()) {
        ...
        // we need to regenerate these every iteration
        breakProbs.setSize(lData.numLocal);
        breakProbs.reGenerate();

        auto forceLoop = xvtxRangeStart("forceLoop");

        hiDem.computePartForce(neighbourForceFunc, linkForceFunc,
                               packVecPtrs(pData.xDiff, pData.yDiff, pData.zDiff,
                                           pData.uDiff, pData.vDiff, pData.wDiff, pData.radius),
                               packVecPtrs(),
                               packVecPtrs(lData.CT0_flat, lData.CT1_flat, lData.CT2_flat,
                                           lData.CT3_flat, lData.CT4_flat, lData.CT5_flat),
                               packVecPtrs(pData.xForce, pData.yForce, pData.zForce,
                                           pData.uForce, pData.vForce, pData.wForce));

        hiDem.applyForce(applyForce);

        if (hiDem.getCurrentTimestep() * dt > breakTime) {
            hiDem.linkUpdateRegen(breakLinks,
                                  packVecPtrs(pData.xCord, pData.yCord, pData.zCord,
                                              pData.xTheta, pData.yTheta, pData.zTheta),
                                  packVecPtrs(),
                                  packVecPtrs(lData.state, lData.initialLen, breakProbs.values));
        }

        hiDem.finalizeTs();
        xvtxRangeEnd(forceLoop);

    }
```

## Config file

For regeneration to work properly the config file needs to have link regeneration enabled

```toml
[linkRegen]
# enable regeneration of links
enabled = true
```

