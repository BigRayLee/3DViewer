#pragma once
#include <fstream>
#include <pthread.h>
#include "HLOD.h"

/* Block structure for mesh simplification and parent cube construction */
struct Block{
    size_t maxIdxCount;
    size_t maxVertexCount;
    size_t maxIdxUVCount;
    size_t maxUVCount;
    Boxcoord *list;                        /* valid block list */
    unsigned int maxBoxCount;
    unsigned short validBoxCount;          /* valid box count */
    unsigned short width;                  /* block width */
};

/* Parallel thread parameters*/
struct Parameter{
    HLOD  *hlod;
    Block *simplifyBlks;
    Block *parentBlks;
    int curLevel;
    float targetError;
};

/* Compute the maximum vertex count and index count of cube at different lods */
unsigned ComputeMaxCounts(HLOD *hlod, int curLevel, Block* blk);

size_t RemapIndexBufferSkipDegenerate(uint32_t *indices, size_t index_count, const uint32_t *remap);

void UpdateVertexParents(void *parents, void *unique_parents, size_t vertex_count, size_t unique_vertex_count,
                        int vertex_stride, uint32_t * grid_remap, uint32_t *simplification_remap);

unsigned LoadBlockData(HLOD *hlod, Boxcoord& bottom, int width, int curLevel, unordered_map<uint64_t, pair<size_t, size_t>>& cubeTable, Mesh *destination, bool isGetData);

unsigned LoadChildData(HLOD *hlod, Mesh *blkData, uint64_t *cubeList, Boxcoord& bottom, int curLevel, unordered_map<uint64_t, pair<size_t, size_t>>& cubeTable, Mesh *destination);

void InitParentMeshGrid(LOD *pmg, LOD *mg);

void LODConstructor(HLOD *hlod, int curLevel, int width, float targetError);

void HLODConsructor(HLOD *hlod, int maxLevel, float targetError);