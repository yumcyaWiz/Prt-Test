#include <iostream>
#include <random>
#include "vec3.h"
#include "math.h"


std::random_device rnd_dev;
std::mt19937 mt(rnd_dev());
std::uniform_real_distribution<> dist(0, 1);
inline float rnd() {
    return dist(mt);
}


struct Spherical {
    float theta;
    float phi;

    Spherical() {};
    Spherical(float theta, float phi) : theta(theta), phi(phi) {};
};


struct Sample {
    Spherical spherical_coord;
    Vec3 cartesian_coord;
    float* sh_functions;
};
struct Sampler {
    Sample* samples;
    int n;
};


void GenSamples(Sampler* sampler, int n) {
    Sample* samples = new Sample[n*n];
    sampler->samples = samples;
    sampler->n = n;

    for(int i = 0; i < n; i++) {
        for(int j = 0; j < n; j++) {
            float u = rnd();
            float v = rnd();

            float theta = 2*std::acos(std::sqrt(1 - u));
            float phi = 2*M_PI*v;
            
            float x = std::cos(phi)*std::sin(theta);
            float y = std::cos(theta);
            float z = std::sin(phi)*std::sin(theta);

            int k = n*i + j;
            sampler->samples[k].spherical_coord = Spherical(theta, phi);
            sampler->samples[k].cartesian_coord = Vec3(x, y, z);
            sampler->samples[k].sh_functions = nullptr;
        }
    }
}


void PrecomputeSH(Sampler* sampler, int bands) {
    for(int i = 0; i < sampler->n; i++) {
        float* sh_functions = new float[bands*bands];
        sampler->samples[i].sh_functions = sh_functions;

        float theta = sampler->samples[i].spherical_coord.theta;
        float phi = sampler->samples[i].spherical_coord.phi;
        int index = 0;
        for(int l = 0; l < bands; l++) {
            for(int m = -l; m <= l; m++) {
                sh_functions[index] = sph(theta, phi, l, m);
                index++;
            }
        }
    }
}


int main() {
    Sampler sampler;
    GenSamples(&sampler, 100);
    PrecomputeSH(&sampler, 10);


    for(int i = 0; i < sampler.n; i++) {
        for(int j = 0; j < 10*10; j++) {
            std::cout << sampler.samples[i].sh_functions[j] << " ";
        }
        std::cout << std::endl;
    }

    return 0;
}
