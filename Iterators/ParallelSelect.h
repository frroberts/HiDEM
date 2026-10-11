//
// Created by Fredrik Robertsen on 28/11/22.
//

#ifndef PHELL_PARALLELSELECT_H
#define PHELL_PARALLELSELECT_H

#include "ManVec.h"
#include <cassert>

#include "Iterator.h"


#ifdef GPU
#include "DevPortabilityLayer.h"
#if (ENABLE_CUDA==1)
#include <cub/cub.cuh>
namespace cubX = cub;
#elif (ENABLE_HIP==1)
//#include <cub/cub.cuh>

#include <hipcub/hipcub.hpp>
namespace cubX = hipcub;
#endif
#endif

class ParallelSelect{
    void *temp = nullptr;
    size_t tempBytes = 0;
    size_t *cubNumSelected; // dummy this needs to be in managed mem somehow

public:
    ParallelSelect() {
        //
#ifdef GPU
    getErrorDevApi(devMallocManaged((void**)&cubNumSelected, sizeof(size_t)));

#endif
    }

    ~ParallelSelect() {
#ifdef GPU
        if(temp != nullptr)
        {
            getErrorDevApi(devFree(temp));
        }
        getErrorDevApi(devFree(cubNumSelected));

#endif
    }

    template<class T>
    void unique(ManVec<T> &input, ManVec<T> &output) {
        //
#ifdef GPU

        std::size_t newNeededBytes = 0;

        cubX::DeviceSelect::Unique(nullptr, newNeededBytes, input.ptr, output.ptr, cubNumSelected, input.size());
        if (newNeededBytes > tempBytes) {
            if (temp != nullptr) {
                getErrorDevApi(devFree(temp));
            }
            getErrorDevApi(devMalloc(&temp, newNeededBytes));
            Iterator::syncAll();
            tempBytes = newNeededBytes;
        }

        cubX::DeviceSelect::Unique(temp, tempBytes, input.ptr, output.ptr, cubNumSelected, input.size());
        Iterator::syncAll();
        output.resize(*cubNumSelected);
#else
        // copy to output first to make it work the same way as GPU, to then swap it afterwards ?
        output.resize(input.size());
        std::memcpy(output.ptr, input.ptr, sizeof(T)*input.size());

        auto endPos = std::unique(STDEXECPOLICY output.begin(), output.end());
        output.resize(std::distance(output.begin(), endPos));
#endif
    }

    template<class T, class TFlag>
    void flagged(ManVec<T> &locations, ManVec<TFlag> &flags, ManVec<T> &selected){
        #ifdef GPU

        std::size_t newNeededBytes = 0;

        Iterator::syncAll();
        cubX::DeviceSelect::Flagged(nullptr, newNeededBytes, locations.ptr, flags.ptr,
                                      selected.ptr, cubNumSelected, flags.size());
        if (newNeededBytes > tempBytes) {
            if (temp != nullptr) {
                getErrorDevApi(devFree(temp));
            }
            getErrorDevApi(devMalloc(&temp, newNeededBytes));
            Iterator::syncAll();
            tempBytes = newNeededBytes;
        }

        cubX::DeviceSelect::Flagged(temp, newNeededBytes, locations.ptr, flags.ptr,
                                      selected.ptr, cubNumSelected, flags.size());
        Iterator::syncAll();
        #else
        int out = 0;
        for (int i = 0; i < flags.size(); ++i) {
            if(flags[i]) {
                selected[out] = locations[i];
                out++;
            }
        }
        // todo make the CPU version
        // copy if flagged ?
        /*
             std::copy_if(from_vector.begin(), from_vector.end(),
                 std::back_inserter(to_vector),
                 [](int x) { return x % 3 == 0; });
         */
        #endif
    }
};

#endif //PHELL_PARALLELSELECT_H
