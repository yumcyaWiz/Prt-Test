#ifndef SKY_H
#define SKY_H
#include <string>

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#endif

#include "vec3.h"


float clamp(float x, float xmin, float xmax) {
    if(x < xmin) return xmin;
    else if(x > xmax) return xmax;
    else return x;
}


class Sky {
    public:
        Sky() {};
        virtual Vec3 getSky(const Vec3& dir) const = 0;
};


class TestSky : public Sky {
    public:
        TestSky() {};

        Vec3 getSky(const Vec3& dir) const {
            return (dir + 1.0f)/2.0f;
        };
};


class UniformSky : public Sky {
    public:
        const Vec3 color;

        UniformSky(const Vec3& _color) : color(_color) {};

        Vec3 getSky(const Vec3& dir) const {
            return color;
        };
};


class SimpleSky : public Sky {
    public:
        SimpleSky() {};

        Vec3 getSky(const Vec3& dir) const {
            float t = (dir.y + 1.0f)*0.5f;
            return (1.0f - t)*Vec3(1.0f) + t*Vec3(0.5f, 0.7f, 1.0f);
        };
};


class IBL : public Sky {
    public:
        int width;
        int height;
        float *HDRI;
        float offsetX;
        float offsetY;

        IBL(const std::string& filename, float _offsetX, float _offsetY) : offsetX(_offsetX), offsetY(_offsetY) {
            int n;
            HDRI = stbi_loadf(filename.c_str(), &width, &height, &n, 0);
        };
        ~IBL() {
            stbi_image_free(HDRI);
        };

        Vec3 getSky(const Vec3& dir) const {
            float phi = std::atan2(dir.z, dir.x);
            if(phi < 0) phi += 2*M_PI;
            phi += offsetX;
            if(phi > 2*M_PI) phi -= 2*M_PI;
            float theta = std::acos(clamp(dir.y, -1.0f, 1.0f));
            theta += offsetY;
            if(theta > M_PI) theta -= M_PI;

            float u = phi/(2.0*M_PI);
            float v = theta/M_PI;

            int w = (int)(u * width);
            int h = (int)(v * height);
            int adr = 3*w + 3*width*h;
            return Vec3(HDRI[adr], HDRI[adr+1], HDRI[adr+2]);
        };
};
#endif
