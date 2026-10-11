//
// Created by Fredrik Robertsen on 07/11/2020.
//

#ifndef PHELL_MANVEC_H
#define PHELL_MANVEC_H
#include <signal.h>

// default size in bytes
#define DEFAULTSIZE 4096

#include <cstring>
#include <iostream>
#include <vector>

#ifdef GPU
#include "DevPortabilityLayer.h"
#endif
#include "DevCall.h"
#include "Allocator.h"

    template <class T>
    class ManVec : public ManagedParent{
    public:
        T *ptr;
        size_t count;
        size_t cap;
        float overAllocFactor = 1.2;
        bool allocated;

        inline void realloc(size_t newCap, bool copyOld = true)
        {
            //
            //std::cout << "reallocing to " << newCap << std::endl;
            T *ptrTemp;// = new T[newCap];
            #ifdef GPU
            getErrorDev((devMallocManaged((void**)&ptrTemp, newCap * sizeof(T))));
            #else
            ptrTemp = new T[newCap];
            #endif

            size_t toCopyCount = count;

            if(newCap < cap)
                toCopyCount = newCap;

            if (copyOld) {
#ifdef GPU
                getErrorDev((devMemcpy(ptrTemp, ptr, sizeof(T)*toCopyCount, devMemcpyDefault)));
#else
                std::memcpy(ptrTemp, ptr, sizeof(T) * toCopyCount);
#endif
            }

            cap = newCap;

            #ifdef GPU
            devFree(ptr);
            #else
            delete[] ptr;
            #endif


            ptr = ptrTemp;
            //std::cout << "reallocing out " << std::endl;

        }

        public:
        //
        ManVec(): allocated(true){
            //std::cout << "DevVec create " << std::endl;

            cap = DEFAULTSIZE / sizeof(T);
            #ifdef GPU
            getErrorDev((devMallocManaged((void**)&ptr, cap * sizeof(T))));
            #else
            ptr = new T[cap];
            #endif

            count = 0;
            //std::cout << "DevVec create out" << std::endl;
        }

        ManVec(size_t initCap): allocated(true), cap(initCap){
            //std::cout << "DevVec create " << std::endl;

            //cap = DEFAULTSIZE / sizeof(T);
            #ifdef GPU
            getErrorDev((devMallocManaged((void**)&ptr, cap * sizeof(T))));
            #else
            ptr = new T[cap];
            #endif

            count = initCap;
            //std::cout << "DevVec create out" << std::endl;
        }

        ~ManVec(){
            //std::cout << "DevVec deconstruct " << std::endl;

            if(allocated)
            {
                #ifdef GPU
                devFree(ptr);
                #else
                delete[] ptr;
                #endif
            }
            //std::cout << "DevVec deconstruct out" << std::endl;
        }


        // copy constructor
        ManVec (const ManVec &old_obj) = delete;
        /*
        {
            //
            //std::cout << "DevVec copy ctr "<< old_obj.count << " " << old_obj.cap << std::endl;

            cap = old_obj.cap;
            count = old_obj.count;
            allocated = old_obj.allocated;

            #ifdef GPU
            getErrorDev((devMallocManaged((void**)&ptr, cap * sizeof(T))));
            devMemcpy(ptr, old_obj.ptr, sizeof(T)*count, devMemcpyDefault);
            #else
            ptr = new T[cap];
            std::memcpy(ptr, old_obj.ptr, sizeof(T)*count);
            #endif

        }*/

        // move constructor
        ManVec (ManVec && other) : allocated(false)
        {
            //std::cout << "DevVec move ctr "<< other.count << " " << other.cap << std::endl;
            count = 0;
            cap = 0;
            std::swap(count, other.count);
            std::swap(cap, other.cap);
            std::swap(ptr, other.ptr);
            std::swap(allocated, other.allocated);
        }

        ManVec<T>& operator=(ManVec<T> other)
        {
            swap(*this, other);
            return *this;
        }

        /*
        DevVec<T> operator=(const DevVec<T> &other)
        {
            return DevVec(other);
        };
*/

        inline void push_back(T val)
        {
            //
            if(count+1 <= cap)
            {
                ptr[count] = val;
                count++;
                return;
            }
            else{
                realloc((cap+1)*overAllocFactor);
                push_back(val);
            }
        }

        inline void reserve(size_t newCap)
        {
            //std::cout << "reserve" << std::endl;
            if(newCap > cap)
                realloc(newCap);
        }

        inline void resize(size_t newCap, bool copyOld = true)
        {
            // todo implement copyOld
            if(newCap <= cap)
            {
                count = newCap;
                // todo go clear unused elements
                // todo and maybe actually resize it if its really a lot smaller than the allocated size
            }
            else
            {
                realloc((newCap+1)*overAllocFactor, copyOld);
            }
            count = newCap;
        }

        inline void shrink_to_fit()
        {
            // does f all since it does not have to...
        }

        DEVCALLABLE
        inline T & operator[](const size_t &ind)
        {
            //
/*
            if(ind >= cap) {
                printf("out of bounds %llu %llu \n", ind, cap);
                raise(SIGINT);
            }
*/
            return ptr[ind];
        }


        DEVCALLABLE
        inline const T &operator[](const size_t &ind) const
        {
/*
            if(ind >= cap) {
                printf("out of bounds %llu %llu \n", ind, cap);
                raise(SIGINT);
            }
*/
            return ptr[ind];
        }

        DEVCALLABLE
        inline size_t size()
        {
            return count;
        }

        DEVCALLABLE
        inline size_t capacity()
        {
            return cap;
        }

        inline void clear()
        {
            count = 0;
        }

        DEVCALLABLE
        inline T*& data()
        {
            return ptr;
        }

        // used to fix the size
        inline void set_size(size_t newSize)
        {
            count = newSize;
        }

        DEVCALLABLE
        inline bool empty()
        {
            return count == 0;
        }

        DEVCALLABLE
        inline T* begin()
        {
            return ptr;
        }

        DEVCALLABLE
        inline T* end()
        {
            return ptr+count;
        }


        inline std::vector<T> toVector() const
        {
            //
            //std::cout << "getting vector" << std::endl;
            std::vector<T> ret;
            for (size_t i = 0; i < count; i++)
            {
                ret.push_back(ptr[i]);
            }
            return ret;
        }

        static void swap(ManVec<T> &first, ManVec<T> &second)
        {
            std::swap(first.ptr, second.ptr);
            std::swap(first.count, second.count);
            std::swap(first.cap, second.cap);
            std::swap(first.overAllocFactor, second.overAllocFactor);
            std::swap(first.allocated, second.allocated);
        }
    };

#endif