#ifndef PHELL_TUNE_H
#define PHELL_TUNE_H

#include <Global.h>
namespace CombinedSmemPairTune {
#if (ENABLE_CUDA==1)

#if defined(NV120)
#pragma message "Tuning for SM 120"
    constexpr bool FUSED_NEIGH = true;
    constexpr int THREADS_PER_BLOCK = 384;
    constexpr int X_DIMM_THREADS = 384;
    constexpr int BLOCKS_PER_SM = 2;
    constexpr int NUM_IN_CACHE = 960;
    constexpr int NUM_IN_CACHE_LARGE = NUM_IN_CACHE*1.5f;
    constexpr int SHARED_ENUM_SET_SIZE = 3072;
#elif defined(NV90)
#pragma message "Tuning for SM 90"
    constexpr bool FUSED_NEIGH = true;
    constexpr int THREADS_PER_BLOCK = 512;
    constexpr int X_DIMM_THREADS = 512;
    constexpr int BLOCKS_PER_SM = 2;
    constexpr int NUM_IN_CACHE = 1280;
    constexpr int NUM_IN_CACHE_LARGE = NUM_IN_CACHE**1.5f;
    constexpr int SHARED_ENUM_SET_SIZE = 3072;
#else
#pragma message "Tuning generic Nvidia"
    // cant have fused neighbours if we have non cachable blocks
    constexpr bool FUSED_NEIGH = true;
    constexpr int THREADS_PER_BLOCK = 384;
    constexpr int X_DIMM_THREADS = 128;
    constexpr int BLOCKS_PER_SM = 2;
    constexpr int NUM_IN_CACHE = 620;
    constexpr int NUM_IN_CACHE_LARGE = NUM_IN_CACHE*2;
    constexpr int SHARED_ENUM_SET_SIZE = 2048;
#endif


#elif (ENABLE_HIP==1)
#pragma message "Tuning generic AMD (MI250X)"
    constexpr bool FUSED_NEIGH = true;
    constexpr int THREADS_PER_BLOCK = 512;
    constexpr int X_DIMM_THREADS = 128;
    constexpr int BLOCKS_PER_SM = 2;
    constexpr int NUM_IN_CACHE = 620;
    constexpr int NUM_IN_CACHE_LARGE = NUM_IN_CACHE*2;
    constexpr int SHARED_ENUM_SET_SIZE = 2048;
#endif
}

#endif
