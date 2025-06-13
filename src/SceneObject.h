#pragma once

#include "UBOstructs.h"
#include <vector>
#include <string>
#include <deque>

enum class ObjectType {
    Star,
    BrownDwarf,
    GasGiant,
    RockyPlanet,
    BlackHole
};

vec4 quat_mult(vec4 q1, vec4 q2);
vec4 quat_from_axis_angle(vec3 axis, float angle_rad);

class SceneObject {
public:

    SceneObject(ObjectType type, vec3 initial_position = vec3(0.0f), float mass = 1.0f);
    ~SceneObject();

 
    void Update(float dt); 
    void SetPosition(const vec3& new_position);
    vec3 GetPosition() const;
    void ApplyForce(const vec3& force, float dt);
    void ResetRotation();
    void SetupAs(ObjectType newType); 
    void CheckForTypeTransition();


    size_t GetGpuObjectCount() const;
    const GPUobject& GetGpuObject(size_t index) const;
    GPUobject& GetGpuObject(size_t index); // Non-const version for ImGui to modify

    std::string Name;
    ObjectType Type;
    float Mass;
    vec3 velocity;
    vec4 Orientation;
    vec3 AngularVelocity;
    bool hasRings = false;

    int MaxTrailPoints = 500;

    std::deque<vec3> TrailPoints;
    std::vector<GPUobject> gpuObjects;

};
