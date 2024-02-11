#pragma once
#include "math/vec3.h"
#include "mesh_simplify/meshoptimizer_mod.h"
#include <unordered_map>
#include <iostream>

using namespace std;

/* Mesh attributes stride*/
constexpr int VERTEX_STRIDE = 3 * sizeof(float);        /* vertex stride */
constexpr int UV_STRIDE = 2 * sizeof(float);            /* uv stride */
constexpr int COLOR_STRIDE = 3 * sizeof(unsigned char); /* color stride*/

/* Mesh simplification block size */
constexpr int SC_BLOCK_SIZE = 4; 
constexpr int SC_COORD_CONVERT = 2;                     /* half of SC_BLOCK_SIZE*/

/* Input mesh model attribute */
struct ModelAttributesStatus{
    bool hasNormal = false;
    bool hasSingleTexture = false;
    bool hasMultiTexture = false;
    bool hasColor = false;
};

extern ModelAttributesStatus modelAttriSatus;

/* Multiresolution construction related structure */
struct Boxcoord{
    short x, y, z;
};

struct Mesh{
    float* positions = nullptr;
    float* normals = nullptr;
    float* uvs = nullptr;
    uint32_t* remap = nullptr;
    u_char* colors = nullptr;

    uint32_t* indices = nullptr;
    uint32_t* indicesUV = nullptr;

    size_t posCount = 0;
    size_t uvCount = 0;
    size_t idxCount = 0;
    size_t idxUVCount = 0;
};

struct quantizedMesh{
    unsigned short *positions = nullptr;   /*x: 16 bits y: 16 bits z: 16 bits placeholder: 16 bits*/
    int32_t *normals = nullptr;            /*8_8_8_8: nx, ny, nz, placeholder*/
    unsigned short *uvs = nullptr;         /*u: 16 bits v: 16 bits*/

    size_t vertCount = 0;
    size_t idxCount = 0;
};

/* Mesh data offset, based on cell unit */
struct mesh_offset_cell{
  size_t vertex_offset;
  size_t parent_offset;
  size_t nml_offset;
  size_t uv_offset;
  size_t color_offset;

  size_t vertex_offset_pack;
  size_t nml_offset_pack;
  size_t uv_offset_pack;
  size_t parent_offset_q;
  size_t remap_offset;

  size_t index_offset;
  size_t index_parent_offset;
  size_t index_uv_offset;

  size_t sum;
  size_t sum_q;
};

/*record the vertex offset for LOAD_ALL cache method*/
struct meshoffset{
    size_t index_offset;
    size_t vertex_offset;
    size_t map_offset;
    size_t texture_offset;
};

/* Calculate triangle Center */
inline void CalculateBarycenter(Vec3 v1, Vec3 v2,  Vec3 v3, float ct[3]){
        ct[0] = (v1.x + v2.x + v3.x) / 3.0;
        ct[1] = (v1.y + v2.y + v3.y) / 3.0;
        ct[2] = (v1.z + v2.z + v3.z) / 3.0;
    }

/* Convert the point to coord coordinate */
inline void Float2Int(float p[3], int coord[3], float bottom[3], float step){
    for(int i = 0; i < 3; i++){
        coord[i] = floor((p[i] - bottom[i]) * step);
    }
}

/* Compute the normal for the model */
float* ComputeNormal(float* vertices, uint32_t* indices, size_t vertCount, size_t indexCount);

/* Convert the coord coordinate based on the block size */
inline bool ConvertBlockCoordinates(Boxcoord& temp, Boxcoord& result, int block_width, int lodSize){
    bool exist = true;
    if (((temp.x - SC_COORD_CONVERT ) >= 0) && ((temp.y - SC_COORD_CONVERT) >= 0) && ((temp.z - SC_COORD_CONVERT) >= 0) &&
        ((temp.x - SC_COORD_CONVERT ) < lodSize) && ((temp.y - SC_COORD_CONVERT) < lodSize) && ((temp.z - SC_COORD_CONVERT) < lodSize)){
        result.x = temp.x - SC_COORD_CONVERT;
        result.y = temp.y - SC_COORD_CONVERT;
        result.z = temp.z - SC_COORD_CONVERT;
    }
    else{
        exist = false;
    }
        
    return exist;
}

/* Compute the max min value */
void GetMaxMin(Vec3 v, float min[3], float max[3]);
void GetMaxMin(float x, float y, float z, float min[3], float max[3]);

/* Memory management functions */
inline void MemoryFree(void* ptr){
    if(ptr){
        free(ptr);
        ptr = nullptr;
    }
    
}
