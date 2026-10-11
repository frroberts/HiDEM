//
// Created by Fredrik Robertsen on 2/3/24.
//

#include <catch2/catch.hpp>
#include <ParticleData.h>


#include <SIMDHelper.h>

TEST_CASE( "Simd playground", "[SimdTests]" ) {

    double val[] = {0,1,2,3,4,5,6,7,8};

    auto intr = _mm512_loadu_pd(val);

    double outVal[8];

    __mmask8 mask = 0b01010101;


    _mm512_mask_storeu_pd(outVal, mask, intr);

    double sum = _mm512_mask_reduce_add_pd(mask, intr);

    //std::cout << sum << std::endl;

    for(int i = 0; i < 8; i++){
        //std::cout << outVal[i] << " " << std::endl;
    }
}


/*
TEST_CASE( "Test mask split", "[SimdTests]" ) {
    uint16_t mask = 0b000000000000000;
    uint8_t low;
    uint8_t high;

    splitMask(mask, low, high);
    REQUIRE(low == 0b00000000);
    REQUIRE(high == 0b00000000);

    mask = 0b0101010101010101;
    splitMask(mask, low, high);
    REQUIRE(low == 0b01010101);
    REQUIRE(high == 0b01010101);

    mask = 0b1111111100000000;
    splitMask(mask, low, high);
    REQUIRE(low == 0b00000000);
    REQUIRE(high == 0b11111111);

    mask = 0b0000000011111111;
    splitMask(mask, low, high);
    REQUIRE(low == 0b11111111);
    REQUIRE(high == 0b00000000);

}
 */
/*
TEST_CASE( "Test fp64 to fp32 and pack", "[SimdTests]" ) {
    __m512d a = {1,1,1,1,1,1,1,1};
    __m512d b = {2,2,2,2,2,2,2,2};

    __m512 c = pd_to_ps_pack(a,b);

    REQUIRE(c[0] == 1);
    REQUIRE(c[1] == 1);
    REQUIRE(c[2] == 1);
    REQUIRE(c[3] == 1);
    REQUIRE(c[4] == 1);
    REQUIRE(c[5] == 1);
    REQUIRE(c[6] == 1);
    REQUIRE(c[7] == 1);
    REQUIRE(c[8] == 2);
    REQUIRE(c[9] == 2);
    REQUIRE(c[10] == 2);
    REQUIRE(c[11] == 2);
    REQUIRE(c[12] == 2);
    REQUIRE(c[13] == 2);
    REQUIRE(c[14] == 2);
    REQUIRE(c[15] == 2);
}
 */

TEST_CASE( "Particle loop load", "[SimdTests]" )
{

    double cords[] = {1, 1,1,1,1,1,1,0,0,0};

    int cordIds[] = {7,8,9};
    float output[] = {0,0,0};

    int neighInds[20] = {1,1,1,1,1,1,2,2,2,
                       2,2,2,3,3,3,3,3,3,3,3};

    int starts[] = {0,5,12};
    int sizes[] = {6,7,8};

    uint16_t mask = 0;

    int numMasks = 0;
    uint16_t selfMasks[16];
    int outIndex[16];

    // used to build input vector
    int selfInd[16];
    int neighInd[16];

    for (int i = 0; i < 16; ++i) {
        selfMasks[i] = 0;
    }

    int filledSlots = 0;
    for (int i = 0; i < 3;i++) {
        for (int j = 0; j < sizes[i]; ++j) {
            // fill a slot
            mask = mask | (0b1 << filledSlots);
            selfInd[filledSlots] = cordIds[i];
            neighInd[filledSlots] = neighInds[j+starts[i]];
            filledSlots++;
            if(filledSlots == 16){
                outIndex[numMasks] = i;
                selfMasks[numMasks] = mask;
                numMasks++;

                __m512 diff = computePsDiff_pd(cords, numMasks, selfMasks, selfInd, neighInd);

                Vec16f vf(diff);
                reduceSumByMasks(output, numMasks, selfMasks, outIndex, diff);

                filledSlots = 0;
                mask = 0;
                numMasks = 0;
                for (int i = 0; i < 16; ++i) {
                    selfMasks[i] = 0;
                }

            }
        }
        // push out current mask
        selfMasks[numMasks] = mask;
        outIndex[numMasks] = i;
        numMasks++;
    }
    __m512 diff = computePsDiff_pd(cords, numMasks, selfMasks, selfInd, neighInd);


    reduceSumByMasks(output, numMasks, selfMasks, outIndex, diff);
    for (int i = 0; i < 3; i++) {
        std::cout << output[i] << " " << std::endl;
    }


}


TEST_CASE( "Bcast from masks", "[SimdTests]" ) {

    int numMasks = 2;
    uint16_t selfMasks[] = {0b1111111111000000, 0b111111};
    int outIndex[] = {0,2};

    float data[] = {1,2,3};

    auto res = bCastFromMasks_ps(numMasks, selfMasks, outIndex, data);

    REQUIRE(res[0] == 3);
    REQUIRE(res[1] == 3);
    REQUIRE(res[2] == 3);
    REQUIRE(res[3] == 3);
    REQUIRE(res[11] == 1);
    REQUIRE(res[12] == 1);
    REQUIRE(res[13] == 1);
    REQUIRE(res[14] == 1);
    REQUIRE(res[15] == 1);
}


TEST_CASE( "computePsDiff_pd", "[SimdTests]" ) {

    int numMasks = 2;
    uint16_t selfMasks[] = {0b111111, 0b1111111111000000};
    int outIndex[] = {0,6};

    int indexes[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23};

    double dataA[16];

    for (int i = 0; i < 16; ++i) {
        dataA[i] = i;
    }

    Vec16f val = computePsDiff_pd(dataA, numMasks, selfMasks, outIndex, indexes);


    REQUIRE(val[0] == 0);
    REQUIRE(val[1] == -1);
    REQUIRE(val[2] == -2);
    REQUIRE(val[3] == -3);
    REQUIRE(val[4] == -4);
    REQUIRE(val[5] == -5);
    REQUIRE(val[6] == 0);
    REQUIRE(val[7] == -1);

    /*
    for (int i = 0; i < 16; ++i) {
        std::cout << val[i] << " ";

    }
    std::cout << std::endl;
     */
}


TEST_CASE( "Block scatter2", "[SimdTests]" ) {
    std::cout << std::endl;


    int indexes[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20};
    //int indexes[] = {0,1,2,3,4,5,6,7,8,10,10,11,12,13,14,15,16,17,18,19,20};


    Vec16f val =  {1};

    Vec16i ind;
    ind.load(indexes);


    float *data = new float[128];
    float *data2 = new float[128];
    for (int i = 0; i < 128; ++i) {
        data[i] = 0;
        data2[i] = 0;
    }

//    _mm512_i32scatter_ps(data, ind, val, 4);
    _mm512_i32scatter_ps(data, ind, val, 4);
    for (int i = 0; i < 25; ++i) {
        std::cout << data[i] << std::endl;
    }

    blockScatter_ps(indexes, 16, val, data2);
    std::cout << std::endl;
    for (int i = 0; i < 25; ++i) {
        std::cout << data2[i] << std::endl;
    }

}

// make a lambda in zero
template<int W, class T>
T __attribute__ ((vector_size (W))) getZeroSimd(T *arg) {
    T __attribute__ ((vector_size (W))) ret{0};
    // zero the vec
    return ret;
}
template<int W, class ...Args>
auto zeroTupleSimd(std::tuple<Args *...> args) {
    return std::apply([=](auto ...x) {
        return std::make_tuple(getZeroSimd<W>(x)...);
    }, args);
}


template<class T>
DEVCALLABLE
T &bCastTupleFromMasks_ps_impl(int index, T *vec) {
    // bCastFromMasks_ps
    return vec[index];
}

template<class ...Args>
DEVCALLABLE
auto bCastTupleFromMasks_ps(int ind, std::tuple<Args *...> ptrs) {
    return std::apply([&](auto ...x) {
        return std::make_tuple(bCastTupleFromMasks_ps_impl(ind, x)...);
    }, ptrs);
}


TEST_CASE( "ZeroSimdTuple", "[SimdTests]" ) {

    float *a;
    float *b;
    float *c;
    int *d;

    auto tups = std::make_tuple(a,b,c,d);

    auto z = zeroTupleSimd<64>(tups);
}