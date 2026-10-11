//
// Created by Fredrik Robertsen on 4/7/25.
//

#ifndef DYNAMICVARIABLES_H
#define DYNAMICVARIABLES_H

#include <Allocator.h>

template <typename T>
class DynamicVariable : public ManagedParent {
    T variable;
public:
    DynamicVariable() : ManagedParent() {
        variable = 0;
        // add cuda memadvice to force this to the device to not ping pong it
    }
    DEVCALLABLE
    T get() {
        return variable;
    }
    DEVCALLABLE
    void set(T value) {
        variable = value;
    }
    DEVCALLABLE
    void increment() {
        variable++;
    }
    DEVCALLABLE
    void decrement() {
        variable--;
    }
    DEVCALLABLE
    T operator++() {
        increment();
    }
    DEVCALLABLE
    T operator--() {
        decrement();
    }
};



#endif //DYNAMICVARIABLES_H
