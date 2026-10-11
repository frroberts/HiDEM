//
// Created by Fredrik Robertsen on 19/10/2020.
//

#ifndef PHELL_MAP2D_H
#define PHELL_MAP2D_H

#include <cmath>
#include <algorithm>
#include <tuple>
#include <iostream>

#include <DevCall.h>
#include <ManVec.h>
#include <Allocator.h>
#include "Map.h"
#include "DevPortabilityLayer.h"

template<class T>
class Map2d : public Map<T> {
public:
    //T *data;
    bool allocated = false;
    T* data;
    float xMin;
    float yMin;
    float xMax;
    float yMax;
    float xRes;
    float yRes;
    float xOffset = 0;
    float yOffset = 0;
    float invXMaxMin = 0;
    float invYMaxMin = 0;
    int xPoints;
    int yPoints;

    Map2d(int xPoints, int yPoints, float xMin, float yMin, float xMax, float yMax) :
            xPoints(xPoints), yPoints(yPoints), xMin(xMin), yMin(yMin), xMax(xMax), yMax(yMax) {
#ifdef GPU
        getErrorDevApi(devMallocManaged((void**) &data, sizeof(T)*xPoints * yPoints));
#else
        data = new T[xPoints*yPoints];
#endif

        allocated = true;

        //data.resize(xPoints * yPoints);
        for (int i = 0; i < xPoints * yPoints; ++i) {
            data[i] = -1;
        }
        xRes = (xMax - xMin) / (float) (xPoints - 1);
        yRes = (yMax - yMin) / (float) (yPoints - 1);
        invXMaxMin = 1.0f / (xMax - xMin);
        invYMaxMin = 1.0f / (yMax - yMin);
    }

    Map2d() = default;

    ~Map2d() {
        //delete data;
        if (allocated) {
#ifdef GPU
            devFree((void**) &data);
#else
            delete[] data;
#endif
        }
    }

    void setOffset(float offsetX, float offsetY) {
        xOffset = offsetX;
        yOffset = offsetY;
    }

    void setPointCord(float x, float y, T val) {
        //
        x -= xMin;
        x /= xRes;
        y -= yMin;
        y /= yRes;
        //std::cout << (int)x << " " << (int)y << " " << val << std::endl
        data[(int) x + ((int) y * xPoints)] = val;
    }

    void setPointInd(int x, int y, T val) {
        //
        data[x + (y * xPoints)] = val;
    }

    DEVCALLABLE
    auto getIndexes(float x, float y) {
        //return std::make_tuple(0, 0, 1, 1);
        //std::cout << x << " " << y << " ";
        //std::cout << " | " << (x-xMin) << " " << (xMax-xMin) << " " << xPoints << " | ";

        int xPoint = std::floor(((x - xMin) * invXMaxMin) * (float) (xPoints - 1));
        int yPoint = std::floor(((y - yMin) * invYMaxMin) * (float) (yPoints - 1));
        //std::cout << xPoint << " " << yPoint << " ";
        // clamp
        int xLow = std::clamp(xPoint, 0, xPoints - 1);
        int yLow = std::clamp(yPoint, 0, yPoints - 1);
        int xHigh = std::clamp(xPoint + 1, 0, xPoints - 1);
        int yHigh = std::clamp(yPoint + 1, 0, yPoints - 1);
        return std::make_tuple(xLow, yLow, xHigh, yHigh);
    }

    DEVCALLABLE
    T interp(float x, float y) {
        x -= xOffset;
        y -= yOffset;

        auto [xLow, yLow, xHigh, yHigh] = getIndexes(x, y);

        if (xLow == xHigh && yLow == yHigh) {
            return data[xLow + (yLow * xPoints)];
        }

        float v00 = data[xLow + (yLow * xPoints)];
        float v01 = data[xLow + (yHigh * xPoints)];
        float v10 = data[xHigh + (yLow * xPoints)];
        float v11 = data[xHigh + (yHigh * xPoints)];

        float x0 = x - xMin;
        float y0 = y - yMin;

        float x1 = xLow * xRes;
        float x2 = xHigh * xRes;
        float y1 = yLow * yRes;
        float y2 = yHigh * yRes;
        float invX21 = 1.0f / (x2 - x1);
        float c1 = (x2 - x0) * invX21;
        float c2 = (x0 - x1) * invX21;

        float fxy1 = (c1 * v00) + (c2 * v10);
        float fxy2 = (c1 * v01) + (c2 * v11);

        if (xLow == xHigh) {
            // return y interploate
            fxy1 = v00;
            fxy2 = v01;
        } else if (yLow == yHigh) {
            // return x interploate
            return fxy1;
        }
        float invY21 = 1.0f / (y2 - y1);
        return (((y2 - y0) * invY21) * fxy1) + (((y0 - y1) * invY21) * fxy2);
    }

    void printValues() {
        //
        for (int x = 0; x < xPoints; ++x) {
            for (int y = 0; y < yPoints; ++y) {
                std::cout << x << " " << y << " " << data[x + (y * xPoints)] << std::endl;
            }
        }
    }

    T getMax() {
        T ret = std::numeric_limits<T>::min();
        for (int x = 0; x < xPoints; ++x) {
            for (int y = 0; y < yPoints; ++y) {
                ret = std::max(ret, data[x + (y * xPoints)]);
            }
        }
        return ret;
    }

    T getMin() {
        T ret = std::numeric_limits<T>::max();
        for (int x = 0; x < xPoints; ++x) {
            for (int y = 0; y < yPoints; ++y) {
                ret = std::min(ret, data[x + (y * xPoints)]);
            }
        }
        return ret;
    }

    virtual T getVal(float x, float y, float z) {
        return interp(x,y);
    }
};


#endif //PHELL_MAP2D_H
