#version 430 core

in vec2 TexCoord0;
layout(binding = 0, r32ui) uniform uimage2D output_image;

out vec4 FragColor;

void main() {
	float ratio = clamp(imageLoad(output_image, ivec2(gl_FragCoord.xy)).x/32.0, 0, 1.0);
	if (ratio < .5f) {
		FragColor = vec4(0.f, 1 - 2.f * ratio, 2.f * ratio, 1.f);
	} else {
		ratio = 1.f - ratio;
		FragColor = vec4(1.f - 2.f * ratio, 0, 2.f * ratio, 1.f);
	}
}