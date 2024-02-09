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

struct Cube{
    float bbxVertices[24];                      /* vertex of boundingbox */
    float bottom[3]{FLT_MAX, FLT_MAX, FLT_MAX}; /* left-bottom coordnates logical size*/
    float top[3]{FLT_MIN, FLT_MIN, FLT_MIN};

    int ijk[3];                        /*ijk coordinates*/
    uint64_t ijk_64;                   /*ijk coordinates in uint_64*/

    /* Offsets in vbo */
    size_t vertexOffset = 0;           /* position */
    size_t normalOffset = 0;           /* normal */
    size_t uvOffset = 0;               /* uv */
    size_t idxOffset = 0;              /* index */
    size_t remapOffset = 0;            /* parent child correspondance */
    size_t colorOffset = 0;            /* color */
    
    int vertCount = 0;
    int uvCount = 0;
    int idxUVCount = 0;
    int triangleCount = 0;             /* triangle numbers */

    /* Constructor */
    Cube();
    Cube(float min[3], float max[3]){}

    void ComputeBottomVertex(float bottom[3], int ijk[3], float length, float min[3]);
    void CalculateBBXVertex(float length);
};
