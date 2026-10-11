//
// Created by Fredrik Robertsen on 12/28/23.


#ifndef PHELL_TRIANGLE_H
#define PHELL_TRIANGLE_H

#include "../Util/Real3.h"


class Triangle{
public:
    std::array<Real3,3 > points;
    Real3 normal;
    Triangle(Real3 p0, Real3 p1, Real3 p2){
        points[0] = p0;
        points[1] = p1;
        points[2] = p2;
    }

		float intersectDist(
			const Real3 &orig, const Real3 &dir,
			const Real3 &v1, const Real3 &v2, const Real3 &v3)
	{

		float r = 0;
		Real3 e1 = v2 - v1;
		Real3 e2 = v3 - v1;
		Real3 pvec = Real3::cross(dir, e2);
		float det = Real3::dot(e1, pvec);
		if ((det < 1e-8f && det > -1e-8f))
			return std::numeric_limits<float>::max();


		float inv_det = 1.f / det;
		Real3 tvec = orig - v1;
		float u = Real3::dot(tvec, pvec) * inv_det;

		if ((u < 0.f || u > 1.f))
			return std::numeric_limits<float>::max();

		Real3 qvec = Real3::cross(tvec, e1);
		float v = Real3::dot(dir, qvec) * inv_det;
		if (((v < 0.f || u + v > 1.f)))
			return std::numeric_limits<float>::max();

		r = Real3::dot(e2, qvec) * inv_det;
		return r;

	}

    void autoNormal(){
        //
        normal = Real3::normalize(Real3::cross(points[2]-points[1], points[1]-points[0] ));

    };

};


#endif //PHELL_TRIANGLE_H
