#include <iostream>
#include <fstream>
#include <random>
#include <vector>
#include <cstdlib>
#include "vec3.h"
#include "math.h"
#include "sky.h"
#include "image.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"


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


struct Triangle {
    int v1;
    int v2;
    int v3;
};
struct Scene {
    Vec3* vertices;
    Vec3* normals;
    Triangle* triangles;
    int vertices_n;
};


void loadObj(const std::string& filename, Vec3* vertices, Vec3* normals, Triangle* triangles) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string err;
    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &err, filename.c_str());
    if(!err.empty()) {
        std::cerr << err << std::endl;
    }
    if(!ret) {
        std::exit(1);
    }

    for(size_t s = 0; s < shapes.size(); s++) {
        size_t index_offset = 0;
        for(size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            int fv = shapes[s].mesh.num_face_vertices[f];
            for(size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];
                tinyobj::real_t vx = attrib.vertices[3*idx.vertex_index+0];
                tinyobj::real_t vy = attrib.vertices[3*idx.vertex_index+1];
                tinyobj::real_t vz = attrib.vertices[3*idx.vertex_index+2];
                
                tinyobj::real_t nx = attrib.normals[3*idx.normal_index+0];
                tinyobj::real_t ny = attrib.normals[3*idx.normal_index+1];
                tinyobj::real_t nz = attrib.normals[3*idx.normal_index+2];
            }
            index_offset += fv;
        }
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
