//
// Created by Fredrik Robertsen on 12/23/25.
//

#ifndef PHELL_MAT4_H
#define PHELL_MAT4_H
#include "Real3.h"

class Mat4
{
public:
    std::array<std::array<double, 4>, 4> mat;

    void fromEulerAnglesXYZ(double x, double y, double z)
    {
        double cosX = std::cos(x);
        double sinX = std::sin(x);
        double cosY = std::cos(y);
        double sinY = std::sin(y);
        double cosZ = std::cos(z);
        double sinZ = std::sin(z);

        mat[0][0] = cosX * cosZ + sinX * sinY * sinZ;
        mat[0][1] = sinZ * cosY;
        mat[0][2] = -sinX * cosZ + cosX * sinY * sinZ;
        mat[0][3] = 0.0;
        mat[1][0] = -cosX * sinZ + sinX * sinY * cosZ;
        mat[1][1] = cosZ * cosY;
        mat[1][2] = sinZ * sinX + cosX * sinY * cosZ;
        mat[1][3] = 0.0;
        mat[2][0] = sinX * cosY;
        mat[2][1] = -sinY;
        mat[2][2] = cosX * cosY;
        mat[2][3] = 0.0;
        mat[3][0] = 0.0;
        mat[3][1] = 0.0;
        mat[3][2] = 0.0;
        mat[3][3] = 1.0;
    }

    Mat4 toInvert()
    {
        Mat4 res;
        double s0 = mat[0][0] * mat[1][1] - mat[0][1] * mat[1][0];
        double s1 = mat[0][0] * mat[1][2] - mat[0][2] * mat[1][0];
        double s2 = mat[0][0] * mat[1][3] - mat[0][3] * mat[1][0];
        double s3 = mat[0][1] * mat[1][2] - mat[0][2] * mat[1][1];
        double s4 = mat[0][1] * mat[1][3] - mat[0][3] * mat[1][1];
        double s5 = mat[0][2] * mat[1][3] - mat[0][3] * mat[1][2];

        double c5 = mat[2][2] * mat[3][3] - mat[2][3] * mat[3][2];
        double c4 = mat[2][1] * mat[3][3] - mat[2][3] * mat[3][1];
        double c3 = mat[2][1] * mat[3][2] - mat[2][2] * mat[3][1];
        double c2 = mat[2][0] * mat[3][3] - mat[2][3] * mat[3][0];
        double c1 = mat[2][0] * mat[3][2] - mat[2][2] * mat[3][0];
        double c0 = mat[2][0] * mat[3][1] - mat[2][1] * mat[3][0];

        double det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;

        if (std::abs(det) <= 1e-9)
        {
            return res; // Return identity or handle singular matrix as needed
        }

        double invDet = 1.0 / det;

        res.mat[0][0] = (mat[1][1] * c5 - mat[1][2] * c4 + mat[1][3] * c3) * invDet;
        res.mat[0][1] = (-mat[0][1] * c5 + mat[0][2] * c4 - mat[0][3] * c3) * invDet;
        res.mat[0][2] = (mat[3][1] * s5 - mat[3][2] * s4 + mat[3][3] * s3) * invDet;
        res.mat[0][3] = (-mat[2][1] * s5 + mat[2][2] * s4 - mat[2][3] * s3) * invDet;

        res.mat[1][0] = (-mat[1][0] * c5 + mat[1][2] * c2 - mat[1][3] * c1) * invDet;
        res.mat[1][1] = (mat[0][0] * c5 - mat[0][2] * c2 + mat[0][3] * c1) * invDet;
        res.mat[1][2] = (-mat[3][0] * s5 + mat[3][2] * s2 - mat[3][3] * s1) * invDet;
        res.mat[1][3] = (mat[2][0] * s5 - mat[2][2] * s2 + mat[2][3] * s1) * invDet;

        res.mat[2][0] = (mat[1][0] * c4 - mat[1][1] * c2 + mat[1][3] * c0) * invDet;
        res.mat[2][1] = (-mat[0][0] * c4 + mat[0][1] * c2 - mat[0][3] * c0) * invDet;
        res.mat[2][2] = (mat[3][0] * s4 - mat[3][1] * s2 + mat[3][3] * s0) * invDet;
        res.mat[2][3] = (-mat[2][0] * s4 + mat[2][1] * s2 - mat[2][3] * s0) * invDet;

        res.mat[3][0] = (-mat[1][0] * c3 + mat[1][1] * c1 - mat[1][2] * c0) * invDet;
        res.mat[3][1] = (mat[0][0] * c3 - mat[0][1] * c1 + mat[0][2] * c0) * invDet;
        res.mat[3][2] = (-mat[3][0] * s3 + mat[3][1] * s1 - mat[3][2] * s0) * invDet;
        res.mat[3][3] = (mat[2][0] * s3 - mat[2][1] * s1 + mat[2][2] * s0) * invDet;

        return res;
    }

    Real3 mulReal3(const Real3& c)
    {
        return {
            mat[0][0] * c.x + mat[1][0] * c.y + mat[2][0] * c.z + mat[3][0],
            mat[0][1] * c.x + mat[1][1] * c.y + mat[2][1] * c.z + mat[3][1],
            mat[0][2] * c.x + mat[1][2] * c.y + mat[2][2] * c.z + mat[3][2]
        };
    }
};

#endif //PHELL_MAT4_H
