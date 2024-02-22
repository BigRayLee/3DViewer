#version 430 core
layout (location = 0) in vec3 nml;
layout (location = 1) in vec2 texCoord;
layout (location = 2) in vec3 lightPos;
layout (location = 3) in vec3 viewDir;
layout (location = 5) in float lambda;

uniform int level;
uniform int maxLevel;
uniform int coordX;
uniform int coordY;
uniform int coordZ;
uniform bool textureExist;
uniform bool colorExist;
uniform bool isCubeColorized;
uniform bool isSmoothShading;
uniform bool isLodColorized;

out vec4 outColor;

#define AMBIENT_COLOR vec3(1.f, 1.0f, 1.0f)
#define Ka 0.1f
#define DIFFUSE_COLOR vec3(1.0, 1.0, 1.0)
#define Kd 0.8f
#define SPECULAR_COLOR vec3(1.0f, 1.0f, 1.0f)
#define Ks 0.1f
#define shininess 8

const ivec3 cubeColors[8] = {
	{120,28,129},
	{64,67,153},
	{72,139,194},
	{107,178,140},
	{159,190,87},
	{210,179,63},
	{231,126,49},
	{217,33,32}
};

const vec3 defaultColor = vec3(0.99f, 0.76f, 0.0f);

void main(){
    /* Ambient light */
    vec3 ambient = Ka * AMBIENT_COLOR;

    /* Diffuse light */
    vec3 view = normalize(viewDir);
    vec3 light = normalize(lightPos);
    vec3 normal;
    
    /* Smooth shading */
    if(isSmoothShading){
        normal = normalize(nml);
        if(dot(view, normal) < 0.0f)
            normal = -normal;
    }
    else{ /* Flat shading */
        normal = normalize(cross(dFdx(viewDir), dFdy(viewDir)));
    }

    float Id = max(dot(normal, light), 0.0f);
    vec3 diffuse = Kd * DIFFUSE_COLOR * Id;

    float Is = 1.0f;
    if(Id > 0 ){
        vec3 reflectDir = reflect(-light, normal);
        Is = pow(max(dot(reflectDir, view), 0.0f), shininess) * shininess * 0.25f;
    }
    
    vec3 specular = Ks * SPECULAR_COLOR * Is;
    vec3 full = vec3(0.0f);
    
    if(isLodColorized){
        float l = maxLevel - level + (1.0f - lambda);
        vec3 c = vec3(0.0f);
        if (l < 1){
            c.r = 1.0f - l;
            c.g = l;
        }
        else if (l < 2){
            c.g = 2.0f - l;
            c.b = l - 1.0f;
        }
        else{
            c.r = smoothstep(2.0f, 6.0f, l);
            c.b = 1.0f - c.r;
        }

        outColor = vec4((ambient + diffuse + specular) * c , 1.0f);
    }
    else if(isCubeColorized){
        int idx = 31 * level + 7 * coordX + 13 * coordY + 17 * coordZ;
        idx = idx & 7;
        vec3 c = vec3(cubeColors[idx]) / 255.f;
        outColor = vec4((ambient + diffuse + specular) * c , 1.0f);  
    }
    else{
        outColor = vec4((ambient + diffuse + specular) * defaultColor , 1.0f);
    }
    
}