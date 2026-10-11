//
// Created by Fredrik Robertsen on 07/11/2020.
//

#ifndef PHELL_MANVEC_H
#define PHELL_MANVEC_H
#include <signal.h>
#include "Allocator.h"
// #include <stacktrace>

// default size in bytes
#define DEFAULTSIZE 4096

#include <cstring>


#include <vector>


#include "DevPortabilityLayer.h"
#include "DevCall.h"
#include "Allocator.h"


#ifdef USE_VEC
#ifdef GPU

template<class T>
class ManVec : public std::vector<T, ManagedAllocator<T>> {
};

#else

template<class T>
class ManVec : public std::vector<T> {
};

#endif
#else

template<class T, class Allocator = ManagedAllocator<T>>
class ManVec : public ManagedParent {
    T *ptr;
    size_t count;
    size_t cap;
    float overAllocFactor = 2;
    bool allocated = false;
    Allocator allocator;

    inline void realloc(size_t newCap, bool copyOld = true) {
        T *ptrTemp = allocator.allocate(newCap);

        if (ptrTemp == nullptr) {
            std::cout << "Failed to allocate memory for ManVec, requested cap: " << newCap << std::endl;
            throw std::bad_alloc();
        }

        size_t toCopyCount = count;

        if (newCap < cap)
            toCopyCount = newCap;

        if (copyOld && allocated) {
#ifdef GPU
            getErrorDevApi((devMemcpy(ptrTemp, ptr, sizeof(T)*toCopyCount, devMemcpyDefault)));
#else
                std::memcpy(ptrTemp, ptr, sizeof(T) * toCopyCount);
#endif
        }

        cap = newCap;
        if (allocated) {
            allocator.deallocate(ptr, cap);
        }
        allocated = true;

        ptr = ptrTemp;
    }

public:
    //
    ManVec(): allocated(false) {
        count = 0;
        cap = 0;
    }

    ManVec(size_t initCap): allocated(true), cap(initCap) {

        ptr = allocator.allocate(cap);

        count = initCap;
    }

    ~ManVec() {
        if (allocated) {
            allocator.deallocate(ptr, cap);
        }
    }


    // copy constructor
    ManVec(const ManVec &old_obj) = delete;

    // move constructor
    ManVec(ManVec &&other) : allocated(false) {
        count = 0;
        cap = 0;
        std::swap(count, other.count);
        std::swap(cap, other.cap);
        std::swap(ptr, other.ptr);
        std::swap(allocated, other.allocated);
    }

    ManVec<T> &operator=(ManVec<T> &&other) noexcept {
        swap(*this, other);
        return *this;
    }

    inline void push_back(T val) {
        //
        if (count + 1 <= cap) {
            ptr[count] = val;
            count++;
            return;
        } else {
            realloc((cap + 1) * overAllocFactor);
            push_back(val);
        }
    }

    inline void reserve(size_t newCap) {
        if (newCap > cap)
            realloc(newCap);
    }

    inline void resize(size_t newCap, bool copyOld = true) {
        // todo implement copyOld
        if (newCap <= cap) {
            count = newCap;
            // todo go clear unused elements
            // todo and maybe actually resize it if its really a lot smaller than the allocated size
        } else {
            realloc((newCap + 1) * overAllocFactor, copyOld);
        }
        count = newCap;
    }

    inline void shrink_to_fit() {
        // does f all since it does not have to...
    }

    DEVCALLABLE
    inline T &operator[](const size_t &ind) {

        return ptr[ind];
    }


    DEVCALLABLE
    inline const T &operator[](const size_t &ind) const {
        return ptr[ind];
    }

    DEVCALLABLE
    inline size_t size() {
        return count;
    }

    DEVCALLABLE
    inline size_t capacity() {
        return cap;
    }

    inline void clear() {
        count = 0;
    }

    DEVCALLABLE
    inline T *&data() {
        return ptr;
    }

    // used to fix the size
    inline void set_size(size_t newSize) {
        count = newSize;
    }

    DEVCALLABLE
    inline bool empty() {
        return count == 0;
    }

    DEVCALLABLE
    inline T *begin() {
        return ptr;
    }

    DEVCALLABLE
    inline T *end() {
        return ptr + count;
    }

    static void swap(ManVec<T> &first, ManVec<T> &second) {
        std::swap(first.ptr, second.ptr);
        std::swap(first.count, second.count);
        std::swap(first.cap, second.cap);
        std::swap(first.overAllocFactor, second.overAllocFactor);
        std::swap(first.allocated, second.allocated);
    }
};


#endif

#endif
