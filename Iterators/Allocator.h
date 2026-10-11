//
// Created by Fredrik Robertsen on 09/11/2020.
//

#ifndef PHELL_ALLOCATOR_H
#define PHELL_ALLOCATOR_H

#include <stdio.h>
#include <stdlib.h>
#include <memory>
#include "../PLiB/Global.h"

#ifdef GPU
#include <DevCall.h>
#include "DevPortabilityLayer.h"
#endif

class ManagedParent {
    public:
    void *operator new(size_t len) {
#ifdef GPU
        void *ptr;
        getErrorDevApi((devMallocManaged(&ptr, len)));
        return ptr;
#else
        return malloc(len);
#endif
    }


    void operator delete(void *ptr) {
#ifdef GPU
        getErrorDevApi((devFree(ptr)));
#else
        free(ptr);
#endif
    }
};


#ifdef GPU

template<typename T, typename A = std::allocator<T>>
class ManagedAllocator  : public A {
public:
    using value_type = T;

    ManagedAllocator() = default;

    template<typename U>
    constexpr ManagedAllocator(const ManagedAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        if (n > std::allocator_traits<ManagedAllocator>::max_size(*this)) {
            throw std::bad_alloc();
        }
        T* ptr;
        getErrorDevApi((devMallocManaged(reinterpret_cast<void **>(&ptr), n * sizeof(T))));

        if (ptr == nullptr) {
            throw std::bad_alloc();
        }

        return ptr;
    }

    void deallocate(T* p, std::size_t n) noexcept {
        getErrorDevApi((devFree(p)));
    }

    friend bool operator==(const ManagedAllocator&, const ManagedAllocator&) { return true; }
    friend bool operator!=(const ManagedAllocator&, const ManagedAllocator&) { return false; }
};

template<typename T, typename A = std::allocator<T>>
class ScratchAllocator  : public A {
public:
    using value_type = T;

    ScratchAllocator() = default;

    template<typename U>
    constexpr ScratchAllocator(const ScratchAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        if (n > std::allocator_traits<ScratchAllocator>::max_size(*this)) {
            throw std::bad_alloc();
        }
        T* ptr;
        getErrorDevApi((devMalloc(reinterpret_cast<void **>(&ptr), n * sizeof(T))));

        if (ptr == nullptr) {
            throw std::bad_alloc();
        }

        return ptr;
    }

    void deallocate(T* p, std::size_t n) noexcept {
        getErrorDevApi((devFree(p)));
    }

    friend bool operator==(const ScratchAllocator&, const ScratchAllocator&) { return true; }
    friend bool operator!=(const ScratchAllocator&, const ScratchAllocator&) { return false; }
};

template<typename T, typename A = std::allocator<T>>
class PinnedAllocator  : public A {
public:
    using value_type = T;

    PinnedAllocator() = default;

    template<typename U>
    constexpr PinnedAllocator(const PinnedAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) {
        if (n > std::allocator_traits<PinnedAllocator>::max_size(*this)) {
            throw std::bad_alloc();
        }
        T* ptr;
        getErrorDevApi((devMallocHost(reinterpret_cast<void **>(&ptr), n * sizeof(T))));

        if (ptr == nullptr) {
            throw std::bad_alloc();
        }

        return ptr;
    }

    void deallocate(T* p, std::size_t n) noexcept {
        getErrorDevApi((devFreeHost(p)));
    }

    friend bool operator==(const PinnedAllocator&, const PinnedAllocator&) { return true; }
    friend bool operator!=(const PinnedAllocator&, const PinnedAllocator&) { return false; }
};

#else
// cpu version of allocators, i.e. just std allocator ?
template<typename T, typename A = std::allocator<T>>
class ManagedAllocator  : public A {
public:

};

template<typename T, typename A = std::allocator<T>>
class ScratchAllocator  : public A {
public:
};

template<typename T, typename A = std::allocator<T>>
class PinnedAllocator  : public A {
public:
};
#endif




#endif //PHELL_ALLOCATOR_H
