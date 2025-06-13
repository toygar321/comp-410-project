#include "Camera.h"
#include <iostream>

vec4 quat_conj(vec4 q) {
    return vec4(-q.x, -q.y, -q.z, q.w);
}

vec4 quat_mult(vec4 q1, vec4 q2) {
    vec4 qr;
    qr.x = (q1.w * q2.x) + (q1.x * q2.w) + (q1.y * q2.z) - (q1.z * q2.y);
    qr.y = (q1.w * q2.y) - (q1.x * q2.z) + (q1.y * q2.w) + (q1.z * q2.x);
    qr.z = (q1.w * q2.z) + (q1.x * q2.y) - (q1.y * q2.x) + (q1.z * q2.w);
    qr.w = (q1.w * q2.w) - (q1.x * q2.x) - (q1.y * q2.y) - (q1.z * q2.z);
    return qr;
}

vec4 quat_from_axis_angle(vec3 axis, float angle_radians) {
    float half_angle = angle_radians * 0.5f;
    float s = sin(half_angle);
    float c = cos(half_angle);
    return (vec4(axis.x * s, axis.y * s, axis.z * s, c));
}

vec3 rotate(vec4 qr, vec3 v) {
    vec4 p = vec4(v.x, v.y, v.z, 0.0f);
    vec4 qr_conj = quat_conj(qr);
    vec4 res = quat_mult(quat_mult(qr, p), qr_conj);
    return vec3(res.x, res.y, res.z);
}

Camera::Camera(vec3 target, float distance)
    : Target(target),
      Distance(distance),
      Yaw(0.0f),
      Pitch(-30.0f),
      Fov(45.0f) {
    UpdateCameraVectors();
}

void Camera::ProcessOrbitDrag(float xoffset, float yoffset) {
    const float sensitivity = 0.25f;
    Yaw -= xoffset * sensitivity;
    Pitch += yoffset * sensitivity;

    if (Pitch > 89.0f) Pitch = 89.0f;
    if (Pitch < -89.0f) Pitch = -89.0f;

    UpdateCameraVectors();
}


void Camera::ProcessOrbitZoom(float yoffset) {
    const float zoomSensitivity = 0.1f;
    float zoomAmount = yoffset * Distance * zoomSensitivity;

    Distance -= zoomAmount;

    if (Distance < 1.0f) Distance = 1.0f;
    if (Distance > 20000.0f) Distance = 20000.0f;

    UpdateCameraVectors();
}


void Camera::UpdateCameraVectors() {
    vec4 yawQuat = quat_from_axis_angle(vec3(0.0f, 1.0f, 0.0f), DegreesToRadians * Yaw);
    vec4 pitchQuat = quat_from_axis_angle(vec3(1.0f, 0.0f, 0.0f), DegreesToRadians * Pitch);
    OrientationQuat = quat_mult(yawQuat, pitchQuat);

    vec3 forwardVector = rotate(OrientationQuat, vec3(0.0f, 0.0f, -1.0f));
    Position = vec4(Target - (forwardVector * Distance), 1.0f);
}