//
// Created by Fredrik Robertsen on 09/11/2020.
//

#ifndef PHELL_ALLOCATOR_H
#define PHELL_ALLOCATOR_H

#include <stdio.h>
#include <stdlib.h>

#ifdef GPU
#include "DevPortabilityLayer.h"
#endif

class ManagedParent {
    public:
    void *operator new(size_t len) {
#ifdef GPU
        void *ptr;
        devMallocManaged(&ptr, len);
        return ptr;
#else
        return malloc(len);
#endif
    }


    void operator delete(void *ptr) {
#ifdef GPU
        devFree(ptr);
#else
        free(ptr);
#endif
    }
};



#endif //PHELL_ALLOCATOR_H
