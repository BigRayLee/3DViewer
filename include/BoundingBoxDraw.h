#pragma once

#include "Shader.h"
#include "Cube.h"
#include <unordered_map>

static constexpr uint32_t SC_INDICES_BBX[24] = {4, 5, 5, 6, 6, 7, 7, 4,
                                                0, 1, 1, 2, 2, 3, 3, 0,
                                                0, 5, 3, 4, 2, 7, 1, 6};
struct BoundingBoxDraw
{
    unordered_map<uint64_t, size_t> vboOffset;
    unordered_map<uint64_t, size_t> eboOffset;
    GLuint bbxVAO;
    GLuint bbxVBO;
    GLuint bbxEBO;
    uint32_t vertexOffset = 0;
    uint32_t idxOffset = 0;

    BoundingBoxDraw();
    ~BoundingBoxDraw();

    void InitBuffer(Cube &cube, float length);
    void Render(Cube &cube, Shader *bbxShader, float length, int level);
    void FlushBuffer(Cube &cube);
};
