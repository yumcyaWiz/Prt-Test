#include <iostream>
#include <fstream>
#include <random>
#include "vec3.h"
#include "math.h"
#include "sky.h"
#include "image.h"


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
    Sample* samples = new Sample[n];
    sampler->samples = samples;
    sampler->n = n;

    for(int i = 0; i < n; i++) {
        float u = rnd();
        float v = rnd();

        float theta = 2*std::acos(std::sqrt(1 - u));
        float phi = 2*M_PI*v;
        
        float x = std::cos(phi)*std::sin(theta);
        float y = std::cos(theta);
        float z = std::sin(phi)*std::sin(theta);

        sampler->samples[i].spherical_coord = Spherical(theta, phi);
        sampler->samples[i].cartesian_coord = Vec3(x, y, z);
        sampler->samples[i].sh_functions = nullptr;
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


void ProjectLightFunction(Vec3* coeffs, Sampler* sampler, Sky* sky, int bands) {
    for(int i = 0; i < bands*bands; i++) {
        coeffs[i].x = 0.0f;
        coeffs[i].y = 0.0f;
        coeffs[i].z = 0.0f;
    }

    for(int i = 0; i < sampler->n; i++) {
        Vec3 dir = sampler->samples[i].cartesian_coord;
        Vec3 skyColor = sky->getSky(dir);
        for(int j = 0; j < bands*bands; j++) {
            float sh_function = sampler->samples[i].sh_functions[j];
            coeffs[j] = coeffs[j] + skyColor * sh_function;
        }
    }

    float weight = 4.0f*M_PI / sampler->n;
    for(int i = 0; i < sampler->n; i++) {
        coeffs[i] = coeffs[i] * weight;
    }
}


int main() {
    int samples = 100;
    int bands = 10;

    Sampler sampler;
    GenSamples(&sampler, samples);
    PrecomputeSH(&sampler, bands);

    Sky* sky = new IBL("PaperMill_E_3k.hdr", 0, 0);
    Vec3* skyCoeffs = new Vec3[bands*bands];
    ProjectLightFunction(skyCoeffs, &sampler, sky, bands);

    std::ofstream file("skyCoeffs.csv");
    for(int i = 0; i < bands; i++) {
        for(int j = 0; j < bands; j++) {
            Vec3 v = skyCoeffs[j + bands*i];
            float vf = (v.x + v.y + v.z)/3;
            if(j != bands - 1) {
                file << vf << ", ";
            }
            else {
                file << vf;
            }
        }
        file << std::endl;
    }
    file.close();

    delete sky;
    delete[] skyCoeffs;
    return 0;
}
