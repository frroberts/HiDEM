//
// Created by Fredrik Robertsen on 08/10/2020.
//

#ifndef PHELL_REAL3_H
#define PHELL_REAL3_H

#include <cmath>
#include <string>
#include "DevCall.h"

class Real3
{
private:
public:

    /* data */
    double x;
    double y;
    double z;
    DEVCALLABLE
    Real3(double x, double y, double z): x(x), y{y}, z{z}
    {
        //
    }
    DEVCALLABLE
    Real3(double a): x(a), y{a}, z{a}
    {

    }
    Real3() = default;
    ~Real3() = default;

    DEVCALLABLE
    inline bool operator<(const Real3& p) const
    {
        //
        if(this->z < p.z )
            return true;
        if(this->z == p.z ){
            if(this->y < p.y )
                return true;
            if(this->y == p.y ){
                if(this->x < p.x )
                    return true;
            }
        }
        return false;
    }
    DEVCALLABLE
	inline bool operator>(const Real3& p) const
	{
		//
		if(this->z > p.z )
			return true;
		if(this->z == p.z ){
			if(this->y > p.y )
				return true;
			if(this->y == p.y ){
				if(this->x > p.x )
					return true;
			}
		}
		return false;
	}
    DEVCALLABLE
	inline Real3 operator-(const Real3& p) const
    {
        Real3 ret;
        ret.x = x - p.x;
        ret.y = y - p.y;
        ret.z = z - p.z;
        return ret;
    }
    DEVCALLABLE
    inline Real3 operator+(const Real3& p) const
    {
        Real3 ret;
        ret.x = x + p.x;
        ret.y = y + p.y;
        ret.z = z + p.z;
        return ret;
    }
    DEVCALLABLE
    inline Real3 operator/(const Real3& p) const
    {
        Real3 ret;
        ret.x = x / p.x;
        ret.y = y / p.y;
        ret.z = z / p.z;
        return ret;
    }

    DEVCALLABLE
    inline Real3 operator*(const Real3& p) const
    {
        Real3 ret;
        ret.x = x * p.x;
        ret.y = y * p.y;
        ret.z = z * p.z;
        return ret;
    }

    DEVCALLABLE
    inline bool operator==(const Real3& b) const
    {
        return x == b.x && y == b.y && z == b.z;
    }

    DEVCALLABLE
    static double dot(Real3 a, Real3 b){
        return (a.x*b.x) + (a.y*b.y) + (a.z*b.z);
    }

    DEVCALLABLE
    static Real3 cross(Real3 a, Real3 b){
        Real3 ret;
        ret.x = a.y*b.z - a.z*b.y;
        ret.y = a.z*b.x - a.x*b.z;
        ret.z = a.x*b.y - a.y*b.x;
        return ret;
    }

    DEVCALLABLE
    static Real3 normalize(Real3 val){
        double len = sqrt(val.x*val.x + val.y*val.y + val.z*val.z);
        val = val/len;
        return val;
    }

    DEVCALLABLE
    static double lenght(Real3 val){
        double len = sqrt(val.x*val.x + val.y*val.y + val.z*val.z);
        return len;
    }

    std::string str() {
    return std::to_string(this->x) + " " + std::to_string(this->y) + " " + std::to_string(this->z) ;
}
};

class Real3f
{
private:
public:

    /* data */
    float x;
    float y;
    float z;
    DEVCALLABLE
    Real3f(float x, float y, float z): x(x), y{y}, z{z}
    {
        //
    }
    DEVCALLABLE
    Real3f(float a): x(a), y{a}, z{a}
    {

    }
    Real3f() = default;
    ~Real3f() = default;

    DEVCALLABLE
    inline bool operator<(const Real3f& p) const
    {
        //
        if(this->z < p.z )
            return true;
        if(this->z == p.z ){
            if(this->y < p.y )
                return true;
            if(this->y == p.y ){
                if(this->x < p.x )
                    return true;
            }
        }
        return false;
    }
    DEVCALLABLE
	inline bool operator>(const Real3f& p) const
	{
		//
		if(this->z > p.z )
			return true;
		if(this->z == p.z ){
			if(this->y > p.y )
				return true;
			if(this->y == p.y ){
				if(this->x > p.x )
					return true;
			}
		}
		return false;
	}
    DEVCALLABLE
	inline Real3f operator-(const Real3f& p) const
    {
        Real3f ret;
        ret.x = x - p.x;
        ret.y = y - p.y;
        ret.z = z - p.z;
        return ret;
    }
    DEVCALLABLE
    inline Real3f operator+(const Real3f& p) const
    {
        Real3f ret;
        ret.x = x + p.x;
        ret.y = y + p.y;
        ret.z = z + p.z;
        return ret;
    }
    DEVCALLABLE
    inline Real3f operator/(const Real3f& p) const
    {
        Real3f ret;
        ret.x = x / p.x;
        ret.y = y / p.y;
        ret.z = z / p.z;
        return ret;
    }

    DEVCALLABLE
    inline Real3f operator*(const Real3f& p) const
    {
        Real3f ret;
        ret.x = x * p.x;
        ret.y = y * p.y;
        ret.z = z * p.z;
        return ret;
    }

    DEVCALLABLE
    inline bool operator==(const Real3f& b) const
    {
        return x == b.x && y == b.y && z == b.z;
    }

    DEVCALLABLE
    static float dot(Real3f a, Real3f b){
        return (a.x*b.x) + (a.y*b.y) + (a.z*b.z);
    }

    DEVCALLABLE
    static Real3f cross(Real3f a, Real3f b){
        Real3f ret;
        ret.x = a.y*b.z - a.z*b.y;
        ret.y = a.z*b.x - a.x*b.z;
        ret.z = a.x*b.y - a.y*b.x;
        return ret;
    }

    DEVCALLABLE
    static Real3f normalize(Real3f val){
        float len = sqrt(val.x*val.x + val.y*val.y + val.z*val.z);
        val = val/len;
        return val;
    }

    DEVCALLABLE
    static float lenght(Real3f val){
        double len = sqrt(val.x*val.x + val.y*val.y + val.z*val.z);
        return len;
    }

    std::string str() {
    return std::to_string(this->x) + " " + std::to_string(this->y) + " " + std::to_string(this->z) ;
}
};



#endif