//
// Created by Fredrik Robertsen on 27/11/22.
//

#ifndef PHELL_LAYOUT_H
#define PHELL_LAYOUT_H

#include <Iterator.h>

class AoSLayout {
public:
    int elementSize;
    int numElements;

    void setSizes(int elemSize, int numElem) {
        elementSize = elemSize;
        numElements = numElem;
    }

    DEVCALLABLE
    size_t getSpace() {
        return elementSize * numElements;
    }

    DEVCALLABLE
    size_t getInd(int memberInd, int elemInd) {
        return (elemInd * elementSize) + memberInd;
    }
};


class SoALayout {
public:
    int elementSize;
    int numElements;

    void setSizes(int elemSize, int numElem) {
        elementSize = elemSize;
        numElements = numElem;
    }

    DEVCALLABLE
    size_t getSpace() {
        return elementSize * numElements;
    }

    DEVCALLABLE
    size_t getInd(int memberInd, int elemInd) {
        return (memberInd * numElements) + elemInd;
    }
};

template<int blockSize>
// require that blocksize is (blockSize & (blockSize-1)) == 0)
class AoSoALayout {
public:
    int elementSize;
    int numElements;

    void setSizes(int elemSize, int numElem) {
        elementSize = elemSize;
        numElements = numElem;
    }

    DEVCALLABLE
    size_t getSpace() {
        return (blockSize * elementSize) * (1 + (numElements / blockSize)); // 1+ a bit meh here
    }

    DEVCALLABLE
    size_t getInd(int memberInd, int elemInd) {
        return ((blockSize * elementSize) * (elemInd / blockSize)) + (((elemInd % blockSize)) + (memberInd * blockSize));
    }
};


#endif //PHELL_LAYOUT_H
