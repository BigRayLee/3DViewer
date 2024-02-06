#pragma once 
#include "Shader.h"

struct OverdrawAnalyzer{
    GLuint overdrawImage;
    GLuint clearOverdrawBuffer;
    Shader overDrawBlit;
    std::string vertexShader = "./shaders/overdrawvisblit.vs"; 
    std::string fragmentShader = "./shaders/overdrawvisblit.fs";
    void OverdrawBufferInitial(GLuint &overdrawImage, GLuint &clearOverdrawBuffer);
    void QuadBufferInit(GLuint &IBO, GLuint &quad_render_vbo, GLuint &quad_render_vao, GLuint &quad_tex_buffer);
};