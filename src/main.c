// Copyright (C) THROUGHOTHEREYES
// Author: Jonathan Platzer <jonathan@throghothereyes.net>

#include <assert.h>
#include <stddef.h>
#include <math.h>

#include <stdio.h>
#include <string.h>

#define FB_WIDTH 512
#define FB_HEIGHT 512
#define FB_BYTES 3
#define FB_SIZE (FB_WIDTH * FB_HEIGHT * FB_BYTES)
#define FOCAL_LENGTH -0.024f
#define WIDTH 0.028f

#define min(x, y) (x < y ? x : y)
#define max(x, y) (x > y ? x : y)
#define clamp(a, b, x) (max(a, min(x, b)))

typedef float   Vec2[2];
typedef float   Vec3[3];
typedef float   Vec4[4];

typedef enum ObjectType {
    PLANE,
    SPHERE,
    BOX
} ObjectType;

typedef struct Object {
    ObjectType type;
    Vec3 pos;
} Object;

typedef struct Sphere {
    ObjectType type;
    Vec3 pos;
    float radius;
} Sphere;

typedef struct Plane {
    ObjectType type;
    Vec3 pos;
    Vec3 normal;
} Plane;

typedef struct Box {
    ObjectType type;
    Vec3 min;
    Vec3 max;
} Box;

#define REGION_SIZE 8

typedef struct RenderRegion {
    Vec3 pos;
    Vec3 cam;
    unsigned char* framebuffer[REGION_SIZE];
} RenderRegion;

static inline void vec3_sub(Vec3 a, Vec3 b, Vec3 dest) {
    dest[0] = a[0] - b[0];
    dest[1] = a[1] - b[1];
    dest[2] = a[2] - b[2];
}

static inline float vec3_dot(Vec3 a, Vec3 b) {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

static inline float vec3_norm2(Vec3 a) {
    return vec3_dot(a, a);
}

static inline float vec3_norm(Vec3 a) {
    return sqrtf(vec3_norm2(a));
}

static inline void vec3_scale(Vec3 src, float s, Vec3 dest) {
    dest[0] = src[0] * s;
    dest[1] = src[1] * s;
    dest[2] = src[2] * s;
}

static inline void vec3_normalize(Vec3 src, Vec3 dest) {
    float norm = vec3_norm(src);
    vec3_scale(src, 1.0f / norm, dest);
}

float intersect_plane(Plane* plane, Vec3 cam, Vec3 raydir) {
    float dn = vec3_dot(plane->normal, raydir);

    float pn = vec3_dot(plane->normal, plane->pos);
    return (pn - vec3_dot(cam, plane->normal)) / dn;
}

float intersect_sphere(Sphere* sphere, Vec3 cam, Vec3 raydir) {
    Vec3 q;
    vec3_sub(cam, sphere->pos, q);

    float raydir_norm = vec3_norm2(raydir);

    float delta = (vec3_dot(raydir, q) * vec3_dot(raydir, q)) - raydir_norm * (vec3_norm2(q) - sphere->radius * sphere->radius);
    float sqrt_delta = sqrt(delta);

    float t1 = (-vec3_dot(raydir, q) + sqrt_delta) / raydir_norm;
    float t2 = (-vec3_dot(raydir, q) - sqrt_delta) / raydir_norm;

    return min(t1, t2);
}

float intersect_box(Box* box, Vec3 cam, Vec3 raydir) {
    float tmin, tmax;

    float t1_x = (box->min[0] - cam[0]) / raydir[0];
    float t2_x = (box->max[0] - cam[0]) / raydir[0];

    float tmin_x = min(t1_x, t2_x);
    float tmax_x = max(t1_x, t2_x);

    float t1_y = (box->min[1] - cam[1]) / raydir[1];
    float t2_y = (box->max[1] - cam[1]) / raydir[1];

    float tmin_y = min(t1_y, t2_y);
    float tmax_y = max(t1_y, t2_y);

    if ((tmin_x > tmax_y) || (tmin_y > tmax_x))
        return -1.0f;

    tmin = max(tmin_x, tmin_y);
    tmax = min(tmax_x, tmax_y);

    float t1_z = (box->min[2] - cam[2]) / raydir[2];
    float t2_z = (box->max[2] - cam[2]) / raydir[2];

    float tmin_z = min(t1_z, t2_z);
    float tmax_z = max(t1_z, t2_z);

    if ((tmin > tmax_z) || (tmin_z > tmax))
        return -1.0f;

    tmin = max(tmin, tmin_z);
    tmax = min(tmax, tmax_z);

    return min(tmin, tmax);
}

float intersect(void* object, Vec3 cam, Vec3 raydir) {
    float return_value = -1.0f;
    Object* object_ptr = object;

    switch (object_ptr->type) {
        case PLANE:
            return_value = intersect_plane((Plane*) object, cam, raydir);
            break;
        case SPHERE:
            return_value = intersect_sphere((Sphere*) object, cam, raydir);
            break;
        case BOX:
            return_value = intersect_box((Box*) object, cam, raydir);
            break;
        default:
            assert(0 && "intersect function not implemented yet");
    }
    return return_value;
}

float map(Vec2 a, Vec2 b, float x) {
    return (x - a[0]) * ((b[1] - b[0]) / (a[1] - a[0])) + b[0];
}

Vec2 range_fb_w, range_fb_h, range_w, range_h;

int render_region(RenderRegion* rr, Object* object) {
    Vec3 dif, dir, p_proj;

    for (size_t y = 0; y < REGION_SIZE; y++) {
        unsigned char* fb = rr->framebuffer[y];

        for (size_t x = 0; x < REGION_SIZE; x++) {
            p_proj[0] = map(range_fb_w, range_w, x + rr->pos[0] + 0.5f);
            p_proj[1] = map(range_fb_h, range_h, y + rr->pos[1] + 0.5f);
            p_proj[2] = FOCAL_LENGTH;

            vec3_sub(p_proj, rr->cam, dif);
            vec3_normalize(dif, dir);

            int cur = x * FB_BYTES;

            float t = intersect(object, rr->cam, dir);
            if (t > 0.f) {
                float c = 1.0f / t;
                float c_gamma = powf(c, 1.0f / 2.2f);
                fb[cur+0] = (unsigned char) (clamp(0.0f, 1.0f, c_gamma) * 255.0f);
                fb[cur+1] = (unsigned char) (clamp(0.0f, 1.0f, c_gamma) * 255.0f);
                fb[cur+2] = (unsigned char) (clamp(0.0f, 1.0f, c_gamma) * 255.0f);
            } else {
                fb[cur+0] = 0;
                fb[cur+1] = 0;
                fb[cur+2] = 0;
            }
        }
    }
    return 0;
}


int main(int argc, char* argv[]) {
    Plane p = {
        .type = PLANE,
        .pos = { 1.0f, -2.0f, -3.0f },
        .normal = { -1.0f, 1.5f, 0.8f },
    };

    Sphere s = {
        .type = SPHERE,
        .pos = { 0.0f, 0.0f, -3.0f },
        .radius = 1
    };

    Box b = {
        .type = BOX,
        .min = { -0.5f, -0.5f, -3.f },
        .max = { 0.5f, 0.5f, -2.f }
    };


    float a = (float) FB_WIDTH / (float) FB_HEIGHT;
    float w = WIDTH;
    float h = w / a;

    range_fb_w[0] = 0.0f;
    range_fb_w[1] = FB_WIDTH;
    range_fb_h[0] = 0.0f;
    range_fb_h[1] = FB_HEIGHT;
    range_w[0] = -w/2.0f;
    range_w[1] = w/2.0f;
    range_h[0] = h/2.0f;
    range_h[1] = -h/2.0f;

    unsigned char framebuffer[FB_SIZE] = {0};

    RenderRegion rr;
    rr.cam[0] = 0.0f;
    rr.cam[1] = 0.0f;
    rr.cam[2] = 0.0f;

    for (size_t y = 0; y < FB_HEIGHT; y += REGION_SIZE) {
        for (size_t x = 0; x < FB_WIDTH; x += REGION_SIZE) {
            rr.pos[0] = x;
            rr.pos[1] = y;

            for (size_t i = 0; i < REGION_SIZE; i++) {
                int fb_region_idx = (x * FB_BYTES) + (y + i) * (FB_WIDTH * FB_BYTES);
                rr.framebuffer[i] = &framebuffer[fb_region_idx];
            }

            render_region(&rr, (Object*) &s);
        }
    }

    FILE *file;
    file = fopen("test.ppm", "wb");
    char header[24];
    snprintf(header, 24, "P6\n%d %d\n255\n", FB_WIDTH, FB_HEIGHT);
    fwrite(header, strlen(header), 1, file);
    fwrite(framebuffer, sizeof(framebuffer), 1, file);
    fclose(file);

    return 0;
}
