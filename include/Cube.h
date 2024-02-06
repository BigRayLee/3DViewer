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
    
    float posMin[3]{FLT_MAX, FLT_MAX, FLT_MAX}; /* real max and min position value*/
    float posMax[3]{FLT_MIN, FLT_MIN, FLT_MIN};
    
    float multiplier_p[3];                      /*position quantization decode matrix*/
    float multiplier_t[2];                      /*uv quantization decode matrix*/
    
    float uvMin[2]{FLT_MAX, FLT_MAX};           /*max uv value and min uv value*/
    float uvMax[2]{FLT_MIN, FLT_MIN};
    
    
    size_t count = 0;                  /* counter of traingel during the LOD building */

    Mesh mesh;                         /* mesh data */
    Mesh parentMesh;                   /* parent mesh data */
    quantizedMesh pack;                /* quantized data */

    /* Offsets in vbo */
    size_t vertexOffset = 0;           /* position */
    size_t normalOffset = 0;           /* normal */
    size_t uvOffset = 0;               /* uv */
    size_t idxOffset = 0;              /* index */
    size_t remapOffset = 0;            /* parent child correspondance */
    size_t colorOffset = 0;            /* color */

    /* New data offset */
    int vertCount = 0;
    int uvCount = 0;
    int idxUVCount = 0;
    int triangleCount = 0;          /* triangle numbers */
    
    uint64_t ijk_64;                   /*ijk coordinates in uint_64*/
    int ijk[3];                        /*ijk coordinates*/

    /* Operator */
    bool operator == (Cube bbx){
        return (this->ijk[0] == bbx.ijk[0] && this->ijk[1] == bbx.ijk[1] && this->ijk[2] == bbx.ijk[2]);
    }

    /* Constructor */
    Cube();
    Cube(float min[3], float max[3]){}

    void ComputeBottomVertex(float bottom[3], int ijk[3], float length, float min[3]);
    uint64_t GetBoxCoord() const;
    int GetTriangleCount() const;
    int GetVertexParentCount() const;
    int GetVertexCount() const;
    int GetTextureCount() const;
    int GetTextureParentCount() const;
    int GetIndexCount() const;
    int GetTextureIndexCount() const;
    int GetIndexTextureParentCount() const;
    int GetIndexParentCount() const;
    
    void CalculateBBXVertex(float length);
    void GetAttributeMaxMin();
};
