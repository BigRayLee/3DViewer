#pragma once 

#include <cstdint>
#include <limits>

#include "math/vec3.h"

class Cube;

struct HitRecord
{
    bool hit = false;
    float distance = std::numeric_limits<float>::infinity();

    Vec3 position = Vec3::Zero;
    Vec3 normal = Vec3::Zero;

    const Cube *cube = nullptr;
    uint32_t triangleID = 0;
    int      lodIndex = -1;
};
