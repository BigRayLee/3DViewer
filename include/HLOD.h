#pragma once 
#include "LOD.h"

struct HLOD
{
    float min[3]{FLT_MAX, FLT_MAX, FLT_MAX}; /* min value of model*/
    float max[3]{FLT_MIN, FLT_MIN, FLT_MIN}; /* max value of model*/
    LOD *lods[SC_MAX_LOD_LEVEL];
    Mesh data;
    size_t curIdxOffset = 0;
    size_t curVertOffset = 0;

    HLOD();
    /* Build the highest resolution data based on input data */
    void BuildLODFromInput(Mesh *rawMesh, size_t vertCount, size_t triCount);
};