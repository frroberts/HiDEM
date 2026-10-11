//
// Created by Fredrik Robertsen on 27/01/2022.
//
#ifndef PHELL_SIMDHELPER_H
#define PHELL_SIMDHELPER_H

#include "DevCall.h"

#if (ENABLE_SIMD512 == 1) || (ENABLE_SIMD256 == 1)
#include <immintrin.h>
#include <vectorclass.h>
#include <tuple>
#endif

DEVCALLABLE
double select(bool pred, double a, double b) {
    if (pred) {
        return a;
    }
    return b;
}

DEVCALLABLE
float select(bool pred, float a, float b) {
    if (pred) {
        return a;
    }
    return b;
}

#if (ENABLE_SIMD512 == 1)

auto getVecType(float *){
    return Vec16f(0);
}

auto getVecType(double *){
    return Vec8d(0);
}

template<class ...PtrArgs, std::size_t... ints>
auto zeroTupFromPtrImpl(std::tuple<PtrArgs *...> ptrs, std::integer_sequence<std::size_t, ints...> integerSequence){
    return std::make_tuple(getVecType(std::get<ints>(ptrs))... );
}

template<class ...PtrArgs>
auto zeroTupFromPtr(std::tuple<PtrArgs *...> ptrs){
    return zeroTupFromPtrImpl(ptrs, std::make_index_sequence<sizeof...(PtrArgs)>());
}


template<class ...PtrArgs, class ...Args, class Mask_t, std::size_t... ints>
void reduceSumByMasksTupleImpl(std::tuple<PtrArgs *...> ptrs, int numMasks, const Mask_t *selfMasks, const int *outIndex, std::tuple<Args...> vals,
                   std::integer_sequence<std::size_t, ints...>) {
    for (int m = 0; m < numMasks; ++m) {
        ((std::get<ints>(ptrs)[outIndex[m]] += _mm512_mask_reduce_add_ps(selfMasks[m] ,std::get<ints>(vals))), ...);
    }
}

template<class ...PtrArgs, class ...Args, std::size_t... ints>
void blockScatterTupleImpl(const int *indPtr, int numInd, std::tuple<PtrArgs *...> ptrs, std::tuple<Args...> val,
                   std::integer_sequence<std::size_t, ints...>) {
    Vec16i one = {1};

    Vec16i indVec;
    if(numInd != 16)
        indVec.load_partial(numInd, indPtr);
    else
        indVec.load(indPtr);

    Vec16i indShift;
    indShift.load_partial(numInd - 1, &indPtr[1]);

    Vec16i diff = indShift - indVec;

    auto mask = _mm512_cmp_epi32_mask(diff, one, _CMP_NEQ_UQ);
    int count = _mm_popcnt_u32(mask & (1 << numInd ) - 1);

    if (count == 1) {
        if (numInd != 16) {
            // store normally with a mask for the last elements
            (Vec16f(std::get<ints>(val)).store_partial(numInd, &std::get<ints>(ptrs)[indPtr[0]]), ...);
        } else {
            // just store normally
            (Vec16f(std::get<ints>(val)).store(&std::get<ints>(ptrs)[indPtr[0]]), ...);
        }
    } else {
                // todo optimize this at some point ?
        int16_t mask = 1;
        int baseInd = indPtr[0];
        for (int i = 1; i < numInd; ++i) {
            //
            if(indPtr[i] != indPtr[i-1]+1){
                (_mm512_mask_storeu_ps(&std::get<ints>(ptrs)[baseInd], mask, std::get<ints>(val)), ...);
                mask = 1 << i;
                baseInd = indPtr[i]-i;
            } else {
                mask = mask | (1<<i);
            }
        }
        // dont do this if we just did it, i.e. the last element was a single one
        (_mm512_mask_storeu_ps(&std::get<ints>(ptrs)[baseInd], mask, std::get<ints>(val)), ...);
    }
}

template<class ...PtrArgs, std::size_t... ints>
auto blockGatherTupleImpl(const int *indPtr, int numInd, std::tuple<PtrArgs *...> ptrs,
                   std::integer_sequence<std::size_t, ints...> integerSequence) {

    Vec16i one = {1};

    Vec16i indVec;
    if(numInd != 16)
        indVec.load_partial(numInd, indPtr);
    else
        indVec.load(indPtr);

    Vec16i indShift;
    indShift.load_partial(numInd - 1, &indPtr[1]);

    Vec16i diff = indShift - indVec;

    auto mask = _mm512_cmp_epi32_mask(diff, one, _CMP_NEQ_UQ);
    int count = _mm_popcnt_u32(mask & (1 << numInd ) - 1);

    if (count == 1) {
        //
        if (numInd != 16) {
            // store normally with a mask for the last elements
            return std::apply([&](auto ...x) {
                    return std::make_tuple(Vec16f().load_partial(numInd, &x[indPtr[0]])...);
                }, ptrs);
        } else {
            // just store normally
            return std::apply([&](auto ...x) {
                    return std::make_tuple(Vec16f().load(&x[indPtr[0]])...);
                }, ptrs);
        }
    }
    auto val = zeroTupFromPtr(ptrs);

    int16_t mask16 = 1;
    int baseInd = indPtr[0];
    for (int i = 1; i < numInd; ++i) {
        //
        if (indPtr[i] != indPtr[i - 1] + 1) {

            ((std::get<ints>(val) = _mm512_mask_loadu_ps(std::get<ints>(val), mask16, &std::get<ints>(ptrs)[baseInd])), ...);
            mask16 = 1 << i;
            baseInd = indPtr[i] - i;
        } else {
            mask16 = mask16 | (1 << i);
        }
    }
    // dont do this if we just did it, i.e. the last element was a single one
    ((std::get<ints>(val) = _mm512_mask_loadu_ps(std::get<ints>(val), mask16, &std::get<ints>(ptrs)[baseInd])), ...);

    return val;
}

static Vec16f bCastFromMasks_ps(int numMasks, const __mmask16 *selfMasks, const int *outIndex, float *data){
    __m512 out = _mm512_setzero_ps();
    for (int m = 0; m < numMasks; ++m) {
        out = _mm512_mask_blend_ps(selfMasks[m],out, _mm512_set1_ps(data[outIndex[m]]));
    }
    return out;
}


__m512 pd_to_ps_pack(__m512d &a, __m512d &b){
    return _mm512_insertf32x8(_mm512_castps256_ps512(_mm512_cvtpd_ps(a)), _mm512_cvtpd_ps(b), 1);
}


template<class ...PtrArgs, std::size_t... ints>
auto computePsDiffPdTupImpl(std::tuple<PtrArgs *...> cords, int numMasks, const __mmask16 *selfMasks, const int *selfIndex, const int *neighInd,
                            std::integer_sequence<std::size_t, ints...> integerSequence){

    __mmask16 fullMask = selfMasks[0];
    for (int m = 1; m < numMasks; ++m) {
        fullMask = _kor_mask16(fullMask,selfMasks[m]);
    }

    __m512d zero = _mm512_setzero_pd();

    auto low = std::make_tuple(zero, zero, zero);
    auto high = std::make_tuple(zero, zero, zero);

    for (int m = 0; m < numMasks; ++m) {
        __mmask8 selfMaskLo = selfMasks[m];
        __mmask8 selfMaskHi = selfMasks[m]>>8;
        auto selfInd = selfIndex[m];

        auto selfCord = std::make_tuple((_mm512_set1_pd(std::get<ints>(cords)[selfInd]))...);

        ((std::get<ints>(low) = _mm512_mask_blend_pd(selfMaskLo,std::get<ints>(low), std::get<ints>(selfCord))), ...);
        ((std::get<ints>(high) = _mm512_mask_blend_pd(selfMaskHi, std::get<ints>(high), std::get<ints>(selfCord))), ...);
    }

    __m256i lowInd = _mm256_loadu_epi32(neighInd);
    __m256i highInd = _mm256_loadu_epi32(&neighInd[8]);;

    auto fullMaskLo = fullMask;
    auto fullMaskHi = fullMask >> 8;

    auto lowNeigh = std::make_tuple(_mm512_mask_i32gather_pd(zero, fullMaskLo, lowInd, std::get<ints>(cords), 8) ...);
    auto highNeigh = std::make_tuple(_mm512_mask_i32gather_pd(zero, fullMaskHi, highInd, std::get<ints>(cords), 8) ...);

    auto lowDiff = std::make_tuple(std::get<ints>(low) - std::get<ints>(lowNeigh) ...);
    auto highDiff = std::make_tuple(std::get<ints>(high) - std::get<ints>(highNeigh) ... );

    std::tuple<Vec16f, Vec16f, Vec16f> diff = std::make_tuple(pd_to_ps_pack(std::get<ints>(lowDiff), std::get<ints>(highDiff))...);
    return diff;
}

Vec16f gather_ps(__m512i ind, float *dataBase) {
    return _mm512_i32gather_ps(ind, dataBase, 4);
}

__m512i getNeighInd(const int *indPtr) {
    //
    return _mm512_load_epi32(indPtr);
}

__m512 b2fMask(__mmask16 mask){
    return _mm512_mask_blend_ps(mask, _mm512_setzero_ps(), _mm512_set1_ps(1));
}


template<class Mask_t>
void clearMask(Mask_t &m){
    m = 0;
}

template<class Mask_t>
void setMask(Mask_t &m, int place) {
    constexpr Mask_t maskLookup[16] = {
            __mmask16(0b1),
            __mmask16(0b10),
            __mmask16(0b100),
            __mmask16(0b1000),
            __mmask16(0b10000),
            __mmask16(0b100000),
            __mmask16(0b1000000),
            __mmask16(0b10000000),
            __mmask16(0b100000000),
            __mmask16(0b1000000000),
            __mmask16(0b10000000000),
            __mmask16(0b100000000000),
            __mmask16(0b1000000000000),
            __mmask16(0b10000000000000),
            __mmask16(0b100000000000000),
            __mmask16(0b1000000000000000)
    };

    m = _kor_mask16(m, maskLookup[place]);
}


template<typename Mask_t>
Mask_t getFullMask(int numMasks, const Mask_t *selfMasks) {
    Mask_t fullMask = selfMasks[0];
    for (int j = 1; j < numMasks; ++j) {
        fullMask = _kor_mask16(fullMask, selfMasks[j]);
    }
    return fullMask;
}



#endif

#if (ENABLE_SIMD256 == 1)

auto getVecType(float *){
    return Vec8f(0);
}

auto getVecType(double *){
    return Vec4d(0);
}

template<class ...PtrArgs, std::size_t... ints>
auto zeroTupFromPtrImpl(std::tuple<PtrArgs *...> ptrs, std::integer_sequence<std::size_t, ints...> integerSequence){
    return std::make_tuple(getVecType(std::get<ints>(ptrs))... );
}

template<class ...PtrArgs>
auto zeroTupFromPtr(std::tuple<PtrArgs *...> ptrs){
    return zeroTupFromPtrImpl(ptrs, std::make_index_sequence<sizeof...(PtrArgs)>());
}

__m256 pd_to_ps_pack(__m256d &a, __m256d &b){
    return _mm256_insertf128_ps(_mm256_castps128_ps256(_mm256_cvtpd_ps(a)), _mm256_cvtpd_ps(b), 1);
}

const Vec8i maskLookup[8] = {
        /*
        Vec8i(0, 0, 0, 0, 0, 0, 0, 0xffffffff),
        Vec8i(0, 0, 0, 0, 0, 0, 0xffffffff, 0),
        Vec8i(0, 0, 0, 0, 0, 0xffffffff, 0, 0),
        Vec8i(0, 0, 0, 0, 0xffffffff, 0, 0, 0),
        Vec8i(0, 0, 0, 0xffffffff, 0, 0, 0, 0),
        Vec8i(0, 0, 0xffffffff, 0, 0, 0, 0, 0),
        Vec8i(0, 0xffffffff, 0, 0, 0, 0, 0, 0),
        Vec8i(0xffffffff, 0, 0, 0, 0, 0, 0, 0)
         */

        Vec8i(0xffffffff, 0, 0, 0, 0, 0, 0, 0),
        Vec8i(0, 0xffffffff, 0, 0, 0, 0, 0, 0),
        Vec8i(0, 0, 0xffffffff, 0, 0, 0, 0, 0),
        Vec8i(0, 0, 0, 0xffffffff, 0, 0, 0, 0),
        Vec8i(0, 0, 0, 0, 0xffffffff, 0, 0, 0),
        Vec8i(0, 0, 0, 0, 0, 0xffffffff, 0, 0),
        Vec8i(0, 0, 0, 0, 0, 0, 0xffffffff, 0),
        Vec8i(0, 0, 0, 0, 0, 0, 0, 0xffffffff),
};


// AVX2 versions
template<class Mask_t>
void clearMask(Mask_t &m) {
    m = Vec8i(0);
}

// AVX2 versions
template<class Mask_t>
void maskFirstSet(Mask_t &m) {
    m = maskLookup[0];
}


template<class Mask_t>
void setMask(Mask_t &m, int place) {


    m = _mm256_or_si256(m, maskLookup[place]);
}


static void reduceSumByMasks(float *output, int numMasks, const __m256i *selfMasks, const int *outIndex, __m256 diff) {
    for (int m = 0; m < numMasks; ++m) {
        // todo cant be auto changed to avx2 needs more work
        // auto val = _mm256_mask_reduce_add_ps(selfMasks[m] ,diff);
        // output[outIndex[m]] += val;
        // std::cout << " " << val << std::endl;

        float val = 0;

        Vec8f maskedDiff = _mm256_blendv_ps(_mm256_setzero_ps(), diff, (__m256)selfMasks[m]);

        output[outIndex[m]] += horizontal_add(maskedDiff);
    }
}

template<class ...PtrArgs, class ...Args, std::size_t... ints>
void reduceSumByMasksTupleImpl(std::tuple<PtrArgs *...> ptrs, int numMasks, const __m256i *selfMasks, const int *outIndex, std::tuple<Args...> vals,
                   std::integer_sequence<std::size_t, ints...>) {


    //reduceSumByMasksTupleRecurse<sizeof...(PtrArgs)>(ptrs, numMasks, selfMasks, outIndex, vals);

    (reduceSumByMasks(std::get<ints>(ptrs), numMasks, selfMasks, outIndex, std::get<ints>(vals)),...);


    /*
    for (int m = 0; m < numMasks; ++m) {
        ((std::get<ints>(ptrs)[outIndex[m]] += _mm256_mask_reduce_add_ps(selfMasks[m] ,std::get<ints>(vals))), ...);
    }
     */
}


static Vec8f bCastFromMasks_ps(int numMasks, const __m256i *selfMasks, const int *outIndex, float *data){
    __m256 out = _mm256_setzero_ps();
    for (int m = 0; m < numMasks; ++m) {
        __m256 one = _mm256_set1_ps(data[outIndex[m]]);
        out = _mm256_blendv_ps(out, one, (__m256)selfMasks[m]);
    }
    return out;
}

/*
static void bCastFromMasks_pd2(int numMasks, const __mmask8 *selfMasks, const int *outIndex, const double *data, __m256d &A, __m256d &B){
    A = _mm256_setzero_pd();
    B = _mm256_setzero_pd();
    for (int m = 0; m < numMasks; ++m) {
        auto aBase = _mm256_set1_pd(data[outIndex[m]]);
        __mmask8 maskLow = selfMasks[m];
        A = _mm256_blendv_pd(A, aBase, maksToVec_pd(maskLow));
        B = _mm256_blendv_pd(B, _mm256_set1_pd(data[outIndex[m]]), maksToVec_pd(selfMasks[m]>>4));
    }
}
*/

template<class ...PtrArgs, std::size_t... ints>
auto computePsDiffPdTupImpl(std::tuple<PtrArgs *...> cords, int numMasks, const __m256i *selfMasks, const int *selfIndex, const int *neighInd,
                            std::integer_sequence<std::size_t, ints...> integerSequence){

    Vec8i fullMask = selfMasks[0];
    for (int m = 1; m < numMasks; ++m) {
        fullMask = _mm256_or_si256(fullMask,selfMasks[m]);
    }

    __m256d zero = _mm256_setzero_pd();

    auto low = std::make_tuple(zero, zero, zero);
    auto high = std::make_tuple(zero, zero, zero);

    for (int m = 0; m < numMasks; ++m) {
        __m256i selfMaskLo = permute8<0,0,1,1,2,2,3,3>(Vec8i(selfMasks[m]));
        __m256i selfMaskHi = permute8<4,4,5,5,6,6,7,7>(Vec8i(selfMasks[m]));
        auto selfInd = selfIndex[m];

        auto selfCord = std::make_tuple((_mm256_set1_pd(std::get<ints>(cords)[selfInd]))...);

        ((std::get<ints>(low) = _mm256_blendv_pd(std::get<ints>(low), std::get<ints>(selfCord), (__m256d)selfMaskLo)), ...);
        ((std::get<ints>(high) = _mm256_blendv_pd( std::get<ints>(high), std::get<ints>(selfCord), (__m256d)selfMaskHi)), ...);
    }

    __m128i lowInd = _mm_load_si128((__m128i*)neighInd);
    __m128i highInd = _mm_load_si128((__m128i*)&neighInd[4]);;

    __m256i fullMaskLo = permute8<0,0,1,1,2,2,3,3>(fullMask);
    __m256i fullMaskHi = permute8<4,4,5,5,6,6,7,7>(fullMask);

    auto lowNeigh = std::make_tuple(_mm256_blendv_pd(zero, _mm256_i32gather_pd(std::get<ints>(cords), lowInd, 8), (__m256d)fullMaskLo) ...);
    auto highNeigh = std::make_tuple(_mm256_blendv_pd(zero, _mm256_i32gather_pd(std::get<ints>(cords), highInd, 8), (__m256d)fullMaskHi) ...);

    auto lowDiff = std::make_tuple(std::get<ints>(low) - std::get<ints>(lowNeigh) ...);
    auto highDiff = std::make_tuple(std::get<ints>(high) - std::get<ints>(highNeigh) ... );

    std::tuple<Vec8f, Vec8f, Vec8f> diff = std::make_tuple(pd_to_ps_pack(std::get<ints>(lowDiff), std::get<ints>(highDiff))...);
    return diff;
}

template<class ...PtrArgs, class ...Args, std::size_t... ints>
void blockScatterTupleImpl(const int *indPtr, int numInd, std::tuple<PtrArgs *...> ptrs, std::tuple<Args...> val,
                   std::integer_sequence<std::size_t, ints...>) {
    // todo reenable this idea
    /*
    Vec8i one = {1};

    Vec8i indVec;
    if(numInd != 8)
        indVec.load_partial(numInd, indPtr);
    else
        indVec.load(indPtr);

    Vec8i indShift;
    indShift.load_partial(numInd - 1, &indPtr[1]);

    Vec8i diff = indShift - indVec;

    auto mask = _mm256_cmp_epi32_mask(diff, one, _CMP_NEQ_UQ); // not AVX2
    int count = _mm_popcnt_u32(mask & (1 << numInd ) - 1);


    if (count == 1) {
        if (numInd != 8) {
            // store normally with a mask for the last elements
            (Vec8f(std::get<ints>(val)).store_partial(numInd, &std::get<ints>(ptrs)[indPtr[0]]), ...);
        } else {
            // just store normally
            (Vec8f(std::get<ints>(val)).store(&std::get<ints>(ptrs)[indPtr[0]]), ...);
        }
    }
*/
    // todo optimize this at some point ?
    __m256i mask = maskLookup[0];

    int baseInd = indPtr[0];
    for (int i = 1; i < numInd; ++i) {
        //
        if (indPtr[i] != indPtr[i - 1] + 1) {
            (_mm256_maskstore_ps(&std::get<ints>(ptrs)[baseInd], mask, std::get<ints>(val) ), ...); // not avx2
            mask = maskLookup[i];
            baseInd = indPtr[i] - i;
        } else {
            setMask(mask, i);
            // mask = mask | (1 << i);
        }
    }
    // dont do this if we just did it, i.e. the last element was a single one
    (_mm256_maskstore_ps(&std::get<ints>(ptrs)[baseInd], mask, std::get<ints>(val) ), ...);  // not avx2
}

template<class ...PtrArgs, std::size_t... ints>
auto blockGatherTupleImpl(const int *indPtr, int numInd, std::tuple<PtrArgs *...> ptrs,
                   std::integer_sequence<std::size_t, ints...> integerSequence) {
    // todo reenable this idea
/*
    Vec8i one = {1};

    Vec8i indVec;
    if(numInd != 8)
        indVec.load_partial(numInd, indPtr);
    else
        indVec.load(indPtr);

    Vec8i indShift;
    indShift.load_partial(numInd - 1, &indPtr[1]);

    Vec8i diff = indShift - indVec;

    auto mask = _mm256_cmp_epi32_mask(diff, one, _CMP_NEQ_UQ); // not AVX2
    int count = _mm_popcnt_u32(mask & (1 << numInd ) - 1);

    if (count == 1) {
        //
        if (numInd != 8) {
            // store normally with a mask for the last elements
            return std::apply([&](auto ...x) {
                    return std::make_tuple(Vec8f().load_partial(numInd, &x[indPtr[0]])...);
                }, ptrs);
        } else {
            // just store normally
            return std::apply([&](auto ...x) {
                    return std::make_tuple(Vec8f().load(&x[indPtr[0]])...);
                }, ptrs);
        }
    }
    */
    auto val = zeroTupFromPtr(ptrs);

    __m256i mask = maskLookup[0];

    int baseInd = indPtr[0];
    for (int i = 1; i < numInd; ++i) {
        //
        if (indPtr[i] != indPtr[i - 1] + 1) {

            ((std::get<ints>(val) = _mm256_blendv_ps(std::get<ints>(val), _mm256_maskload_ps(&std::get<ints>(ptrs)[baseInd], mask), (__m256)mask)), ...);
            mask = maskLookup[i];
            baseInd = indPtr[i] - i;
        } else {
            setMask(mask, i);
            //mask = mask | (1 << i);
        }
    }
    // dont do this if we just did it, i.e. the last element was a single one
    ((std::get<ints>(val) = _mm256_blendv_ps(std::get<ints>(val), _mm256_maskload_ps(&std::get<ints>(ptrs)[baseInd], mask), (__m256)mask)), ...);

    return val;
}

Vec8f gather_ps(__m256i ind, float *dataBase) {
    return _mm256_i32gather_ps(dataBase, ind, 4);
}

__m256i getNeighInd(const int *indPtr) {
    //
    return _mm256_load_si256((__m256i*)indPtr);
}

__m256 b2fMask(__m256i mask){
    // yeah the cast is not nice but the types used for masks in avx2 is all over the place
    return _mm256_blendv_ps(_mm256_setzero_ps(), _mm256_set1_ps(1), (__m256) mask);
}




template<typename Mask_t>
Mask_t getFullMask(int numMasks, const Mask_t *selfMasks) {
    Mask_t fullMask = selfMasks[0];
    for (int j = 1; j < numMasks; ++j) {
        fullMask = _mm256_or_si256(fullMask, selfMasks[j]);
    }
    return fullMask;
}



#endif

// hide behind generic simd defines

#if (ENABLE_SIMD512 == 1) || (ENABLE_SIMD256 == 1)

// reduceSumByMasks
template<class ...PtrArgs, class ...Args, class Mask_t>
void reduceSumByMasksTuple(std::tuple<PtrArgs *...> ptrs, int numMasks, const Mask_t *selfMasks,
    const int *outIndex, std::tuple<Args...> vals) {
    reduceSumByMasksTupleImpl(ptrs, numMasks, selfMasks, outIndex, vals, std::make_index_sequence<sizeof...(Args)>());
}

template<class ...PtrArgs, class ...Args>
void blockScatterTuple(const int *indPtr, int numInd, std::tuple<PtrArgs *...> ptrs, std::tuple<Args...> val) {
    blockScatterTupleImpl(indPtr, numInd, ptrs, val, std::make_index_sequence<sizeof...(Args)>());
}

template<class ...PtrArgs>
auto blockGatherTuple(const int *indPtr, int numInd, std::tuple<PtrArgs *...> ptrs) {
    return blockGatherTupleImpl(indPtr, numInd, ptrs, std::make_index_sequence<sizeof...(PtrArgs)>());
}

template<class ...PtrArgs, class Mask_t>
auto bCastFromMasksTuple(const int *indPtr, int numInd, const Mask_t *selfMasks, std::tuple<PtrArgs *...> ptrs) {

    return std::apply([&](auto ...x) {
        return std::make_tuple(bCastFromMasks_ps(numInd, selfMasks, indPtr, x)...);
    }, ptrs);
}

template<class ...PtrArgs, class Mask_t>
auto computePsDiffPdTup(std::tuple<PtrArgs *...> cords, int numMasks, const Mask_t *selfMasks, const int *selfIndex, const int *neighInd){
    return computePsDiffPdTupImpl(cords, numMasks, selfMasks, selfIndex, neighInd, std::make_index_sequence<sizeof...(PtrArgs)>());
}

#endif




#endif //PHELL_SIMDHELPER_H
