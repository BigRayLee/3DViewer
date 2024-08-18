#pragma once
#include <vector>
#include <utility>
#include <limits.h>
#include <cfloat>
#include <cstring>
#include <stdint.h>
#include <malloc.h>
#include <string>
#include "Utils.h"

using namespace std;

struct Cube
{
    float bbxVertices[24];
    float bottom[3]{FLT_MAX, FLT_MAX, FLT_MAX};
    float top[3]{FLT_MIN, FLT_MIN, FLT_MIN};
    int coord[3];
    uint64_t coord64;
    size_t vertexOffset = 0;
    size_t idxOffset = 0;
    int vertCount = 0;
    int triangleCount = 0;

    Cube();
    Cube(float min[3], float max[3]) {}
    void ComputeBottomVertex(float bottom[3], int coord[3], float length, float min[3]);
    void CalculateBBXVertex(float length);
};
