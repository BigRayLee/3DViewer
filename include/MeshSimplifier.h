#pragma once
#include <fstream>
#include <pthread.h>
#include "LOD.h"

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
    HLOD *hlod;
    Block *simplifyBlks;
    Block *parentBlks;
    int curLevel;
    float targetError;
};

/* Compute the maximum vertex count and index count of cube at different lods */
unsigned ComputeMaxCounts(HLOD *hlod, int curLevel, Block* blk);

size_t RemapIndexBufferSkipDegenerate(uint32_t *indices, size_t index_count, const uint32_t *remap);

/* Mesh simplification */
size_t CollapseSimplifyMesh(uint32_t *out_indices, uint32_t *simplification_remap, const uint32_t *indices, 
        size_t index_count, const void *vertices, size_t vertex_count, int vertex_stride,  size_t target_index_count, 
        float target_error, float block_extend, float block_bottom[3]);

/* Mesh simplification with texture coordinates */
size_t CollapseSimplifyMeshTex(uint32_t *out_indices, uint32_t *out_indices_uv, uint32_t *simplification_remap, uint32_t *simplification_remap_uv,
                               const uint32_t *indices, size_t index_count, const uint32_t *indices_uv, size_t index_uv_count,
                               const void *vertices, size_t vertex_count, const void *uvs, size_t uv_count, unsigned vertex_stride, unsigned uv_stride,
                               size_t target_index_count, float target_error, float block_extend, float block_bottom[3]);

/*UPDATE AND REWITE INFO AFTER SIMPLIFICATION*/ 
void UpdateVertexParents(void *parents, void *unique_parents, size_t vertex_count, size_t unique_vertex_count,
                        int vertex_stride, uint32_t * grid_remap, uint32_t *simplification_remap);

/*uv parent*/
void UpdateUVParents(void *uv_parents, void *unique_uv_parents, size_t uv_count, size_t unique_uv_count,
                    int uv_stride, uint32_t *grid_remap_uv, uint32_t * simlification_remap_uv);

/*color parent*/
void UpdateColorParents(void *color_parents, void *unique_color_parents, size_t color_count, size_t unique_color_count,
                    int color_stride, uint32_t *grid_remap, uint32_t * simlification_remap);

/*REWRITE*/
/*rewrite only geometry information*/
void WritebackVertexParents(LOD *mg, int box_count, uint64_t *boxes, Mesh & data);

unsigned LoadBlockData(HLOD *hlod, Boxcoord& bottom, int width, int curLevel, unordered_map<uint64_t, pair<size_t, size_t>>& cubeTable, Mesh *destination, bool isGetData);

unsigned LoadChildData(HLOD *hlod, Mesh *blkData, uint64_t *cubeList, Boxcoord& bottom, int curLevel, unordered_map<uint64_t, pair<size_t, size_t>>& cubeTable, Mesh *destination);


/*BUILD LOD*/
/*build parent mesh grid, build new LOD*/
void InitParentMeshGrid(LOD *pmg, LOD *mg);

/*block-based mesh simplification and parent mesh level construction (only geometry)*/
void LODConstructor(LOD *mg, LOD *pmg, int width, float target_error, int max_level);

void LODConstructor(LOD *mg, LOD *pmg, Mesh &data, int width, float targetError, int max_level);

void HLODConsructor(HLOD *hlod, int maxLevel, float targetError);