#include <iostream>
#include <fstream>
#include <random>
#include <vector>
#include <cstdlib>
#include "vec3.h"
#include "ray.h"
#include "math.h"
#include "sky.h"
#include "image.h"
#include "timer.h"
#include "aabb.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include <GL/glut.h>

#include <omp.h>


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

    Sample() {};
    ~Sample() {};
};
struct Sampler {
    Sample* samples;
    int n;

    Sampler() {};
    ~Sampler() {};
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
    std::cout << n << " Sample Generated" << std::endl;
}


void PrecomputeSH(Sampler* sampler, int bands) {
#pragma omp parallel for
    for(int i = 0; i < sampler->n; i++) {
        float* sh_functions = new float[bands*bands];
        sampler->samples[i].sh_functions = sh_functions;

        float theta = sampler->samples[i].spherical_coord.theta;
        float phi = sampler->samples[i].spherical_coord.phi;
        for(int l = 0; l < bands; l++) {
            for(int m = -l; m <= l; m++) {
                int k = l*(l + 1) + m;
                sh_functions[k] = sph(theta, phi, l, m);
            }
        }
    }
    std::cout << "PrecomputeSH Finished" << std::endl;
}


Vec3 lightDir = Vec3(0, 1, 0);
void ProjectLightFunction(Vec3* coeffs, Sampler* sampler, int bands) {
    for(int i = 0; i < bands*bands; i++) {
        coeffs[i] = Vec3(0, 0, 0);
    }

#pragma omp parallel for
    for(int i = 0; i < sampler->n; i++) {
        Vec3 dir = sampler->samples[i].cartesian_coord;
        Vec3 skyColor = std::max(0.5f*dot(dir, lightDir), 0.0f);
        for(int j = 0; j < bands*bands; j++) {
            float sh_function = sampler->samples[i].sh_functions[j];
            coeffs[j] = coeffs[j] + skyColor * sh_function;
        }
    }

    float weight = 4.0f*M_PI / sampler->n;
    for(int i = 0; i < bands*bands; i++) {
        coeffs[i] = coeffs[i] * weight;
    }
}


struct Triangle {
    int v0;
    int v1;
    int v2;

    Triangle() {};
    Triangle(int v0, int v1, int v2) : v0(v0), v1(v1), v2(v2) {};
};
struct Scene {
    std::vector<Vec3> vertices;
    std::vector<Vec3> normals;
    std::vector<Triangle> triangles;
    int vertices_n;

    Scene() {};
};


void loadObj(const std::string& filename, std::vector<Vec3>& vertices, std::vector<Vec3>& normals, std::vector<Triangle>& triangles) {
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
                vertices.push_back(Vec3(vx, vy, vz));
            }

            Vec3 v1 = vertices[index_offset + 0];
            Vec3 v2 = vertices[index_offset + 1];
            Vec3 v3 = vertices[index_offset + 2];
            Vec3 n = normalize(cross(v2 - v1, v3 - v1));
            for(int i  = 0; i < 3; i++) {
                normals.push_back(n);
            }

            triangles.push_back(Triangle(index_offset+0, index_offset+1, index_offset+2));
            index_offset += fv;
        }
    }
    std::cout << "Vertex: " << vertices.size() << std::endl;
    std::cout << "Triangles: " << triangles.size() << std::endl;
}


void ProjectUnShadowed(Vec3** coeffs, Sampler* sampler, Scene* scene, int bands) {
    for(int i = 0; i < scene->vertices_n; i++) {
        for(int j = 0; j < bands*bands; j++) {
            coeffs[i][j] = Vec3(0, 0, 0);
        }
    }

    for(int i = 0; i < scene->vertices_n; i++) {
        Vec3 color = (scene->normals[i] + 1.0f)/2.0f;
#pragma omp parallel for
        for(int j = 0; j < sampler->n; j++) {
            Sample& sample = sampler->samples[j];
            float cos_term = std::max(dot(scene->normals[i], sample.cartesian_coord), 0.0f);
            for(int k = 0; k < bands*bands; k++) {
                float sh_function = sample.sh_functions[k];
                coeffs[i][k] = coeffs[i][k] + sh_function * cos_term * color;
            }
        }
    }

    float weight = 4.0f*M_PI / sampler->n;
    for(int i = 0; i < scene->vertices_n; i++) {
        for(int j = 0; j < bands*bands; j++) {
            coeffs[i][j] = coeffs[i][j] * weight;
        }
    }
}


bool RayTriangleIntersection(const Ray& ray, const Vec3& p1, const Vec3& p2, const Vec3& p3) {
    const float eps = 1e-6;
    const Vec3 edge1 = p2 - p1;
    const Vec3 edge2 = p3 - p1;
    const Vec3 h = cross(ray.direction, edge2);
    const float a = dot(edge1, h);
    if(a >= -eps && a <= eps) return false;

    const float f = 1.0f/a;
    const Vec3 s = ray.origin - p1;
    const float u = f*dot(s, h);
    if(u < 0.0f || u > 1.0f) return false;

    const Vec3 q = cross(s, edge1);
    const float v = f*dot(ray.direction, q);
    if(v < 0.0f || u + v > 1.0f) return false;

    float t = f*dot(edge2, q);
    if(t <= 0.0f) return false;

    return true;
}


bool Visibility(Scene* scene, int vertexID, const Vec3& direction) {
    Vec3 p = scene->vertices[vertexID];
    for(int i = 0; i < scene->triangles.size(); i++) {
        Triangle t = scene->triangles[i];
        if(vertexID != t.v0 && vertexID != t.v1 && vertexID != t.v2) {
            Vec3 v0 = scene->vertices[t.v0];
            Vec3 v1 = scene->vertices[t.v1];
            Vec3 v2 = scene->vertices[t.v2];
            Ray ray = Ray(p + 0.01f*scene->normals[vertexID], direction);
            if(RayTriangleIntersection(ray, v0, v1, v2)) {
                return false;
            }
        }
    }
    return true;
}


void ProjectShadowed(Vec3** coeffs, Sampler* sampler, Scene* scene, int bands) {
    for(int i = 0; i < scene->vertices_n; i++) {
        for(int j = 0; j < bands*bands; j++) {
            coeffs[i][j] = Vec3(0, 0, 0);
        }
    }

    for(int i = 0; i < scene->vertices_n; i++) {
        Vec3 color = (scene->normals[i] + 1.0f)/2.0f;
#pragma omp parallel for schedule(dynamic, 1)
        for(int j = 0; j < sampler->n; j++) {
            Sample& sample = sampler->samples[j];
            float cos_term = std::max(dot(scene->normals[i], sample.cartesian_coord), 0.0f);
            if(Visibility(scene, i, sample.cartesian_coord)) {
                for(int k = 0; k < bands*bands; k++) {
                    float sh_function = sample.sh_functions[k];
                    coeffs[i][k] = coeffs[i][k] + sh_function * cos_term * color;
                }
            }
        }
    }

    float weight = 4.0f*M_PI / sampler->n;
    for(int i = 0; i < scene->vertices_n; i++) {
        for(int j = 0; j < bands*bands; j++) {
            coeffs[i][j] = coeffs[i][j] * weight;
        }
    }
}


int samples = 100;
int bands = 5;
Sampler sampler;
std::vector<Vec3> vertices;
std::vector<Vec3> normals;
std::vector<Triangle> triangles;
Vec3* skyCoeffs;
Vec3** objCoeffs;

float cx = 0.0f;
float cy = 0.0f;
float cz = 0.0f;


int frame = 0;
float angle = 0.0f;
void render() {
    ProjectLightFunction(skyCoeffs, &sampler, bands);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glLoadIdentity();
    gluLookAt(cx, cy, cz, cx, cy, cz - 1.0f, 0.0f, 1.0f, 0.0f);

    glRotatef(angle, 0.0f, 1.0f, 0.0f);

    glBegin(GL_TRIANGLES);
    for(int i = 0; i < triangles.size(); i++) {
        Triangle& t = triangles[i];
        Vec3 v0 = vertices[t.v0];
        Vec3 v1 = vertices[t.v1];
        Vec3 v2 = vertices[t.v2];

        Vec3 c0, c1, c2;
        for(int k = 0; k < bands*bands; k++) {
            c0 = c0 + skyCoeffs[k]*objCoeffs[t.v0][k];
            c1 = c1 + skyCoeffs[k]*objCoeffs[t.v1][k];
            c2 = c2 + skyCoeffs[k]*objCoeffs[t.v2][k];
        }

        v0 = v0 * 5;
        v1 = v1 * 5;
        v2 = v2 * 5;

        glColor3f(c0.x, c0.y, c0.z);
        glVertex3f(v0.x, v0.y, v0.z);
        glColor3f(c1.x, c1.y, c1.z);
        glVertex3f(v1.x, v1.y, v1.z);
        glColor3f(c2.x, c2.y, c2.z);
        glVertex3f(v2.x, v2.y, v2.z);
    }
    glEnd();

    angle += 0.1f;
    frame++;
    lightDir = normalize(Vec3(std::sin(0.2f * angle), 0, std::cos(0.2f * angle)));

    glutSwapBuffers();
}


void normalKeys(unsigned char key, int x, int y) {
    switch(key) {
        case 'a':
            cx -= 0.01f;
            break;
        case 'd':
            cx += 0.01f;
            break;
        case 'w':
            cy += 0.01f;
            break;
        case 's':
            cy -= 0.01f;
            break;
        case 'q':
            cz -= 0.01f;
            break;
        case 'e':
            cz += 0.01f;
            break;
    }
}
void specialKeys(int key, int x, int y) {
}


int main(int argc, char** argv) {
    Timer timer;
    GenSamples(&sampler, samples);
    PrecomputeSH(&sampler, bands);

    skyCoeffs = new Vec3[bands*bands];
    /*
     Sky* sky = TestSky();
    Timer timer;
    timer.start();
    ProjectLightFunction(skyCoeffs, &sampler, sky, bands);
    timer.stop("ProjectLightFunction: ");


    std::ofstream file("skyCoeffs.csv");
    timer.start();
    for(int i = 0; i < bands*bands; i++) {
        Vec3 v = skyCoeffs[i];
        float vf = (v.x + v.y + v.z)/3;
        if(i != bands*bands - 1) {
            file << vf << ", ";
        }
        else {
            file << vf;
        }
        file << std::endl;
    }
    file.close();
    timer.stop("Output skyCoeffs.csv: ");
    */


    loadObj("bunny.obj", vertices, normals, triangles);
    Scene scene;
    scene.vertices = vertices;
    scene.normals = normals;
    scene.triangles = triangles;
    scene.vertices_n = vertices.size();


    objCoeffs = new Vec3*[scene.vertices_n];
    for(int i = 0; i < scene.vertices_n; i++) {
        objCoeffs[i] = new Vec3[bands*bands];
    }
    timer.start();
    ProjectShadowed(objCoeffs, &sampler, &scene, bands);
    timer.stop("ProjectTransferFunction: ");


    GenSamples(&sampler, samples);
    PrecomputeSH(&sampler, bands);


    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(512, 512);
    glutCreateWindow("Prt_Test");
    glutDisplayFunc(render);
    glutIdleFunc(render);
    glutKeyboardFunc(normalKeys);
    glutSpecialFunc(specialKeys);
    glEnable(GL_DEPTH_TEST);
    glutMainLoop();


    //delete sky;
    delete[] skyCoeffs;
    for(int i = 0; i < scene.vertices_n; i++) {
        delete[] objCoeffs[i];
    }
    delete[] objCoeffs;
    return 0;
}
