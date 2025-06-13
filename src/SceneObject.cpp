#include "SceneObject.h"
#include <iostream>


SceneObject::SceneObject(ObjectType type, vec3 initial_position, float mass)
    : velocity(0.0f),
      Orientation(0.0f, 0.0f, 0.0f, 1.0f),
      AngularVelocity(vec3(0.0f, 0.15f, 0.0f)) {

    SetupAs(type);
    
    if (mass > 0.0f) {
        this->Mass = mass;
    }

    SetPosition(initial_position);
}

SceneObject::~SceneObject() {}

void SceneObject::Update(float dt) {
    CheckForTypeTransition();

    vec3 displacement = velocity * dt;
    SetPosition(GetPosition() + displacement);

    float angle = length(AngularVelocity) * dt;
    if (angle > 1e-6f) { 
        vec3 axis = normalize(AngularVelocity);
    
        vec4 deltaRotation = quat_from_axis_angle(axis, angle);
        Orientation = quat_mult(Orientation, deltaRotation);
        
        for (auto& gpu_obj : gpuObjects) {
            gpu_obj.rot_quat = Orientation;
        }
    }
}

vec3 SceneObject::GetPosition() const {
    if (gpuObjects.empty()) {
        return vec3(0.0f);
    }
    return gpuObjects[0].center;
}

void SceneObject::SetPosition(const vec3& new_position) {
    for (auto& gpu_obj : gpuObjects) {
        gpu_obj.center = new_position;
    }
}

void SceneObject::ApplyForce(const vec3& force, float dt) {
    if (Mass > 0.0f) {
        vec3 acceleration = force / Mass;
        velocity += acceleration * dt;
    }
}

void SceneObject::ResetRotation() {
    Orientation = vec4(0.0f, 0.0f, 0.0f, 1.0f);
}

void SceneObject::SetupAs(ObjectType newType) {
    this->Type = newType;
    vec3 currentPos = GetPosition();
    vec4 currentRot = Orientation;

    gpuObjects.clear(); 

    switch (newType) {
        case ObjectType::Star:
            Name = "Star";
            hasRings = false;
            gpuObjects.resize(1);
            GetGpuObject(0).r1 = 8.0f; // Stars are large.
            GetGpuObject(0).m.albedo = vec3(1.0, 0.8, 0.5);
            GetGpuObject(0).m.emission = 1000.0f; // Sustained fusion is very bright.
            Mass = 800.0f;
            break;

        case ObjectType::BrownDwarf:
            Name = "Brown Dwarf";
            hasRings = false;
            gpuObjects.resize(1);
            GetGpuObject(0).r1 = 4.0f; 
            GetGpuObject(0).m.albedo = vec3(0.4, 0.15, 0.1); // Dim, deep red glow.
            GetGpuObject(0).m.emission = 7.0f; // Glows faintly.
            Mass = 250.0f;
            break;

        case ObjectType::GasGiant:
            Name = "Gas Giant";
            gpuObjects.resize(hasRings ? 2 : 1);
            GetGpuObject(0).r1 = 1.5f;
            GetGpuObject(0).m.albedo = vec3(0.8, 0.7, 0.6);
            GetGpuObject(0).m.emission = 0.0f; 
            Mass = 80.0f;
            if (hasRings) {
                GetGpuObject(1).type = 1; 
                GetGpuObject(1).r1 = GetGpuObject(0).r1 * 2.0f;
                GetGpuObject(1).r2 = GetGpuObject(0).r1 * 1.2f;
                GetGpuObject(1).m.albedo = vec3(0.6f);
            }
            break;
            
        case ObjectType::RockyPlanet:
            Name = "Rocky Planet";
            gpuObjects.resize(hasRings ? 2 : 1);
            GetGpuObject(0).r1 = 0.5f;
            GetGpuObject(0).m.albedo = vec3(0.5, 0.6, 0.8);
            GetGpuObject(0).m.emission = 0.0f;
            Mass = 1.0f;
            if (hasRings) {
                 GetGpuObject(1).type = 1;
                 GetGpuObject(1).r1 = GetGpuObject(0).r1 * 2.5f;
                 GetGpuObject(1).r2 = GetGpuObject(0).r1 * 1.5f;
                 GetGpuObject(1).m.albedo = vec3(0.7f);
            }
            break;

        case ObjectType::BlackHole:
            Name = "Black Hole";
            hasRings = true; 
            gpuObjects.resize(2);
            GetGpuObject(0).r1 = 0.5f; 
            GetGpuObject(0).m.albedo = vec3(0.0f);
            GetGpuObject(0).m.emission = 0.0f;
            GPUobject& disk = GetGpuObject(1);
            disk.type = 1;
            disk.r1 = GetGpuObject(0).r1 * 10.0f;
            disk.r2 = GetGpuObject(0).r1 * 1.5f;
            disk.m.albedo = vec3(1.0, 0.8, 0.3);
            disk.m.emission = 500.0f;
            break;
    }

    SetPosition(currentPos);
    Orientation = currentRot;
    for(auto& gpu_obj : gpuObjects) {
        gpu_obj.rot_quat = Orientation;
    }
}

void SceneObject::CheckForTypeTransition() {

    const float MASS_LIMIT_ROCKY_TO_GIANT = 50.0f;
    const float MASS_LIMIT_GIANT_TO_DWARF = 200.0f; // Approx. 13 Jupiter masses
    const float MASS_LIMIT_DWARF_TO_STAR  = 600.0f; // Approx. 80 Jupiter masses
    const float SCHWARZSCHILD_FACTOR      = 0.005f; // Artistic value for collapse

    ObjectType currentType = this->Type;
    float currentRadius = GetGpuObject(0).r1;
    float schwarzschildRadius = this->Mass * SCHWARZSCHILD_FACTOR;

    if (currentType != ObjectType::BlackHole && currentRadius < schwarzschildRadius) {
        std::cout << "Object '" << Name << "' collapsed into a Black Hole!" << std::endl;
        float collapsingMass = this->Mass;
        SetupAs(ObjectType::BlackHole);
        this->Mass = collapsingMass;
        GPUobject& sphere = GetGpuObject(0);
        sphere.r1 = schwarzschildRadius;
        sphere.m.roughness = 0.0f;
        GPUobject& disk = GetGpuObject(1);
        disk.r1 = schwarzschildRadius * 4.0f; 
        disk.r2 = schwarzschildRadius * 1.5f;
        return; 
    }

    switch (currentType) {
        case ObjectType::Star:
            if (this->Mass < MASS_LIMIT_DWARF_TO_STAR) {
                std::cout << "Star '" << Name << "' lost mass and became a Brown Dwarf." << std::endl;
                SetupAs(ObjectType::BrownDwarf);
            }
            break;

        case ObjectType::BrownDwarf:
            if (this->Mass > MASS_LIMIT_DWARF_TO_STAR) {
                std::cout << "Brown Dwarf '" << Name << "' gained enough mass to ignite as a Star!" << std::endl;
                SetupAs(ObjectType::Star);
            }
            else if (this->Mass < MASS_LIMIT_GIANT_TO_DWARF) {
                std::cout << "Brown Dwarf '" << Name << "' cooled into a Gas Giant." << std::endl;
                SetupAs(ObjectType::GasGiant);
            }
            break;

        case ObjectType::GasGiant:
            if (this->Mass > MASS_LIMIT_GIANT_TO_DWARF) {
                std::cout << "Gas Giant '" << Name << "' became a Brown Dwarf." << std::endl;
                SetupAs(ObjectType::BrownDwarf);
            }
            else if (this->Mass < MASS_LIMIT_ROCKY_TO_GIANT) {
                std::cout << "Gas Giant '" << Name << "' lost its atmosphere, revealing a Rocky Planet." << std::endl;
                SetupAs(ObjectType::RockyPlanet);
            }
            break;

        case ObjectType::RockyPlanet:
            if (this->Mass > MASS_LIMIT_ROCKY_TO_GIANT) {
                std::cout << "Rocky Planet '" << Name << "' accreted an atmosphere and became a Gas Giant." << std::endl;
                SetupAs(ObjectType::GasGiant);
            }
            break;
            
        case ObjectType::BlackHole:
            break;
    }
}

size_t SceneObject::GetGpuObjectCount() const {
    return gpuObjects.size();
}

const GPUobject& SceneObject::GetGpuObject(size_t index) const {
    return gpuObjects.at(index);
}

GPUobject& SceneObject::GetGpuObject(size_t index) {
    return gpuObjects.at(index);
}

