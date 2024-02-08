#pragma once
#include <string>
#include <map>
#include <unordered_map>
#include <utility>
#include <string.h>
#include "Cube.h"
#include "Utils.h"
#include "mesh_simplify/meshoptimizer_mod.h"
#include "math/vec3.h"
#include "Chrono.h"
#include "System.h"

using namespace std;

struct Cube;
static constexpr short SC_MAX_LOD_LEVEL = 10;

struct LOD{
    unordered_map<uint64_t,Cube> cubeTable;        /* hashmap between ijk and cell*/
    size_t totalTriCount = 0;
    size_t totalVertCount = 0;
    int level;                                     /* level*/
    int lodSize;                                   /* grid size 1<<L */ 
    float step;
    float cubeLength;                          

    LOD(){}                                        
    LOD(int l);                                 
    void SetLOD(float max[3], float min[3]);        
    size_t CalculateTriangleCounts();               
    size_t CalculateVertexCounts();                
    size_t GetCubeCounts();                         
};

struct HLOD{
    float min[3]{FLT_MAX, FLT_MAX, FLT_MAX};      /* min value of model*/
    float max[3]{FLT_MIN, FLT_MIN, FLT_MIN};      /* max value of model*/
    LOD *lods[SC_MAX_LOD_LEVEL];
    Mesh data;
    size_t curIdxOffset = 0;
    size_t curVertOffset = 0;

    /* Build the highest resolution data based on input data */
    void BuildLODFromInput(Mesh* rawMesh, size_t vertCount, size_t triCount);
};

/* Dispath the triangle based on the triangle, push the ijk to the cubeTable, push the ijk and cells to the cubeTable*/
void Dispatch(int xyz[3], unordered_map<uint64_t,Cube > &cubeTable);

