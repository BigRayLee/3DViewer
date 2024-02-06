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
    float min[3]{FLT_MAX, FLT_MAX, FLT_MAX};      /* min value of model*/
    float max[3]{FLT_MIN, FLT_MIN, FLT_MIN};      /* max value of model*/
    unordered_map<uint64_t,Cube> cubeTable;        /* hashmap between ijk and cell*/
    size_t totalTriCount = 0;
    size_t totalVertCount = 0;
    size_t vertexOffset = 0;
    size_t indexOffset = 0;
    int level;                                    /* level*/
    int gridSize;                                 /* grid size 1<<L */ 
    float step;
    float cubeLength;                          

    LOD(){}                                    /*construction function*/
    LOD(int l);                                /*use level to intialize the LOD*/
    void SetMeshGrid();                             /*set the length, scale value*/
    size_t CalculateTriangleCounts();                     /*get the total number of triangle of this level*/
    size_t CalculateVertexCounts();                       /*get the total number of vertex of this level*/
    size_t GetCubeCounts();                         /*get bounding box number*/
    void MeshQuantization(Cube &bbx);               /*mesh quantization for each cell*/
    void CleanBBXData(Cube &bbx);                   /*clean the bounding box*/
    void Clean();                                   /*clean the LOD structure*/
};

struct HLOD{
    LOD *lods[SC_MAX_LOD_LEVEL];
    Mesh data;
    size_t curIdxOffset = 0;
    size_t curVertOffset = 0;

    /* Build the highest resolution data based on input data */
    void BuildLODFromInput(Mesh* rawMesh, size_t vertCount, size_t triCount);
};

/* Dispath the triangle based on the triangle, push the ijk to the cubeTable, push the ijk and cells to the cubeTable*/
void Dispatch(int xyz[3], unordered_map<uint64_t,Cube > &cubeTable);

/* Calculate the multi-resolution model information */
void ComputeMeshGridsInfo(LOD *meshbook[], int level, mesh_offset_cell &sum_offset, bool mesh_quatization);

/* Simplification error(extent error) based on the M0 and ML */
float LODErrorCalculation(LOD *meshbook[], int level);

