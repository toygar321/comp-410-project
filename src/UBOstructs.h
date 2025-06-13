#pragma once

#include "Angel.h"

struct alignas(16) GPUmaterial {
    vec3 albedo = vec3(1.0);
    float emission = 0.0f;
    float metallic = 0.0f;
    float roughness = 1.0f;
    int textureID = -1;
};

struct alignas(16) GPUobject {
    GPUmaterial m;
    vec4 rot_quat = vec4(0.0f, 0.0f, 0.0f, 1.0f);
    vec3 center;
    float r1;
    float r2 = 0;
    int type = 0;    
};

const int MAX_OBJECTS_CPP = 16;

struct ObjectUBOData {
    GPUobject objects[MAX_OBJECTS_CPP];
    int num_objects_active;
};