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

struct LOD
{
    unordered_map<uint64_t, Cube> cubeTable;       /* hashmap between coord and cell*/
    size_t totalTriCount = 0;
    size_t totalVertCount = 0;
    int level;                                     /* level*/
    int lodSize;                                   /* grid size 1 << L */ 
    float step;
    float cubeLength;

    LOD() {}
    LOD(int l);
    void SetLOD(float max[3], float min[3]);
    size_t CalculateTriangleCounts();
    size_t CalculateVertexCounts();
    size_t GetCubeCounts();
};

/* Dispath the triangle based on the triangle, push the coord to the cubeTable, push the coord and cells to the cubeTable*/
void Dispatch(int xyz[3], unordered_map<uint64_t, Cube> &cubeTable);
