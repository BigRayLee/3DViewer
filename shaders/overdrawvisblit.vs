#version 430 core
layout (location = 0) in vec3 vposition;
layout (location = 1) in vec2 TexCoord;

out vec2 TexCoord0;

void main() {
	TexCoord0   = TexCoord;
	gl_Position = vec4(vposition, 1.0);
}