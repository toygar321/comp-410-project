#pragma once

#include "Angel.h"

vec4 quat_conj(vec4 q);
vec4 quat_mult(vec4 q1, vec4 q2);
vec4 quat_from_axis_angle(vec3 axis, float angle_rad);
vec3 rotate(vec4 qr, vec3 v);

class Camera {
public:
    vec4 Position;
    vec4 OrientationQuat;
    float Fov;

    vec3 Target;        
    float Distance;      
    float Yaw;          
    float Pitch; 

    Camera(vec3 target = vec3(0.0f), float distance = 100.0f);

    void ProcessOrbitDrag(float xoffset, float yoffset);
    void ProcessOrbitZoom(float yoffset);
    void UpdateCameraVectors();
};