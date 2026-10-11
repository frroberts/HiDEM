//
// Created by Fredrik Robertsen on 12/03/2021.
//

#ifndef PHELL_SCRATCHMEM_H
#define PHELL_SCRATCHMEM_H


#include <cstring>
#include <iostream>
#include <vector>

#ifdef GPU
#include <DevPortabilityLayer.h>
#endif

#include "DevCall.h"

template<class T>
class ScratchMem {

    T *ptr;
    size_t count = 0;
    size_t cap = 0;
    bool allocated = false;

    inline void realloc(size_t newCap) {

        if (allocated == true) {
#ifdef GPU
            devFree(ptr);
#else
            delete[] ptr;
#endif
        }

        //
        //std::cout << "reallocing to " << newCap << std::endl;
#ifdef GPU
        getErrorDev((devMalloc((void**)&ptr, newCap * sizeof(T))));
#else
        ptr = new T[newCap];
#endif
        allocated = true;

    }

public:
    //
    ScratchMem() : allocated(false) {

    }

    ~ScratchMem() {
        if (allocated) {
#ifdef GPU
            devFree(ptr);
#else
            delete[] ptr;
#endif
        }
    }


    // copy constructor
    ScratchMem(const ScratchMem &old_obj) {

        cap = old_obj.cap;
        count = old_obj.count;
        allocated = old_obj.allocated;

#ifdef GPU
        getErrorDev((devMalloc((void**)&ptr, cap * sizeof(T))));
        //cudaMemcpy(ptr, old_obj.ptr, sizeof(T)*count, cudaMemcpyDefault);
#else
        ptr = new T[cap];
        std::memcpy(ptr, old_obj.ptr, sizeof(T) * count);
#endif

    }

    // move constructor
    ScratchMem(ScratchMem &&other) : allocated(false) {
        count = 0;
        cap = 0;
        std::swap(count, other.count);
        std::swap(cap, other.cap);
        std::swap(ptr, other.ptr);
        std::swap(allocated, other.allocated);
    }

    ScratchMem<T> &operator=(ScratchMem<T> other) {
        swap(*this, other);
        return *this;
    }

    inline void resize(size_t newCap) {
        if (newCap <= cap) {
            count = newCap;
            // todo go clear unused elements
        } else {
            //std::cout << "reallocing " << newCap << " old cap " << cap << std::endl;
            realloc((newCap));
        }
        count = newCap;
    }

    DEVCALLABLE
    inline T &operator[](const size_t &ind) {
        //

        //if(ind >= cap)
        //    std::cout << "out of bound error " << ind << " " << cap << std::endl;

        return ptr[ind];
    }

    DEVCALLABLE
    inline const T &operator[](const size_t &ind) const {
        //

        //if(ind >= cap)
        //    std::cout << "out of bound error " << ind << " " << cap << std::endl;

        return ptr[ind];
    }

    DEVCALLABLE
    inline size_t size() {
        return count;
    }

    DEVCALLABLE
    inline T *&data() {
        return ptr;
    }


    static void swap(ScratchMem<T> &first, ScratchMem<T> &second) {
        std::swap(first.ptr, second.ptr);
        std::swap(first.count, second.count);
        std::swap(first.cap, second.cap);
        std::swap(first.allocated, second.allocated);
    }
};

template<class T>
class PinnedMem {

    T *ptr;
    size_t count = 0;
    size_t cap = 0;
    bool allocated = false;

    inline void realloc(size_t newCap) {

        if (allocated == true) {
#ifdef GPU
            devFree(ptr);
#else
            delete[] ptr;
#endif
        }

        //
        //std::cout << "reallocing to " << newCap << std::endl;
#ifdef GPU
        getErrorDev((devMallocHost((void**)&ptr, newCap * sizeof(T))));
#else
        ptr = new T[newCap];
#endif
        allocated = true;

    }

public:
    //
    PinnedMem() : allocated(false) {

    }

    ~PinnedMem() {
        if (allocated) {
#ifdef GPU
            devFreeHost(ptr);
#else
            delete[] ptr;
#endif
        }
    }


    // copy constructor
    PinnedMem(const PinnedMem &old_obj) {

        cap = old_obj.cap;
        count = old_obj.count;
        allocated = old_obj.allocated;

#ifdef GPU
        getErrorDev((devMalloc((void**)&ptr, cap * sizeof(T))));
        //cudaMemcpy(ptr, old_obj.ptr, sizeof(T)*count, cudaMemcpyDefault);
#else
        ptr = new T[cap];
        std::memcpy(ptr, old_obj.ptr, sizeof(T) * count);
#endif

    }

    // move constructor
    PinnedMem(PinnedMem &&other) : allocated(false) {
        count = 0;
        cap = 0;
        std::swap(count, other.count);
        std::swap(cap, other.cap);
        std::swap(ptr, other.ptr);
        std::swap(allocated, other.allocated);
    }

    ScratchMem<T> &operator=(ScratchMem<T> other) {
        swap(*this, other);
        return *this;
    }

    inline void resize(size_t newCap) {
        if (newCap <= cap) {
            count = newCap;
            // todo go clear unused elements
        } else {
            //std::cout << "reallocing " << newCap << " old cap " << cap << std::endl;
            realloc((newCap));
        }
        count = newCap;
    }

    DEVCALLABLE
    inline T &operator[](const size_t &ind) {
        //

        //if(ind >= cap)
        //    std::cout << "out of bound error " << ind << " " << cap << std::endl;

        return ptr[ind];
    }

    DEVCALLABLE
    inline const T &operator[](const size_t &ind) const {
        //

        //if(ind >= cap)
        //    std::cout << "out of bound error " << ind << " " << cap << std::endl;

        return ptr[ind];
    }

    DEVCALLABLE
    inline size_t size() {
        return count;
    }

    DEVCALLABLE
    inline T *&data() {
        return ptr;
    }


    static void swap(ScratchMem<T> &first, ScratchMem<T> &second) {
        std::swap(first.ptr, second.ptr);
        std::swap(first.count, second.count);
        std::swap(first.cap, second.cap);
        std::swap(first.allocated, second.allocated);
    }
};



#endif //PHELL_SCRATCHMEM_H
