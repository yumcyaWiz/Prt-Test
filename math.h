#ifndef MATH_H
#define MATH_H
#include <cmath>


inline long factorial(long x) {
    if(x <= 1) return 1;
    else return x * factorial(x - 1);
}


inline long dfactorial(long x) {
    if(x <= 1) return 1;
    else return x*dfactorial(x - 2);
}


inline float legendre(float x, long l, long m) {
    if(l == m + 1) {
        return x*(2*m + 1)*legendre(x, m, m);
    }
    else if(l == m) {
        return std::pow(-1, m)*dfactorial(2*m - 1)*std::pow(1 - x*x, m/2);
    }
    else {
        return (x*(2*l - 1)*legendre(x, l - 1, m) - (l + m - 1)*legendre(x, l - 2, m))/(l - m);
    }
}


inline float sph_k(long l, long m) {
    return std::sqrt((2*l + 1)*factorial(l - std::abs(m))/(4*M_PI*factorial(l + std::abs(m))));
}


inline float sph(float theta, float phi, long l, long m) {
    if(m > 0) {
        return std::sqrt(2)*sph_k(l, m)*std::cos(m*phi)*legendre(std::cos(theta), l, m);
    }
    else if(m < 0) {
        return std::sqrt(2)*sph_k(l, m)*std::sin(-m*phi)*legendre(std::cos(theta), l, -m);
    }
    else {
        return sph_k(l, 0)*legendre(std::cos(theta), l, 0);
    }
}
#endif
