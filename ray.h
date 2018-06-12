#ifndef RAY_H
#define RAY_H
#include "vec3.h"
class Ray {
    public:
        Vec3 origin;
        Vec3 direction;
        const float tmin = 0.0f;
        const float tmax = 10000.0f;

        Ray(const Vec3& origin, const Vec3& direction) : origin(origin), direction(direction) {};
};
#endif
