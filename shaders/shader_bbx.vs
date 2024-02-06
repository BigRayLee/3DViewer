#version 460 core 
layout (location = 0) in vec3 aPos; 


layout (std140, binding = 0) uniform Matrices
{
    mat4 projection;
    mat4 view;
    mat4 model;
} matrices;

void main(){
    gl_Position = matrices.projection * matrices.view * matrices.model * vec4(aPos, 1.0);
}
