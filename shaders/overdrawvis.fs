#version 430 core

layout (binding = 0, r32ui) uniform uimage2D overdraw_count;

uniform uint cube_count;

out vec4 color;

void main(void)
{
    imageAtomicAdd(overdraw_count, ivec2(gl_FragCoord.xy), 1);
}
