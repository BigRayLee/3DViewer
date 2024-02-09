#version 430 core
layout (location = 0) in vec3 Normal;
layout (location = 1) in vec2 TexCoord;
layout (location = 2) in vec3 LightPos;
layout (location = 3) in vec3 ViewDir;
layout (location = 4) in vec3 Color;
layout (location = 5) in float lambda;

uniform int level;
uniform int maxLevel;
uniform int cubeI;
uniform int cubeJ;
uniform int cubeK;
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

    /*ambient light*/
    vec3 ambient = Ka * AMBIENT_COLOR;

    /*diffuse light*/
    vec3 VIEW = normalize(ViewDir);
    vec3 LIGHT = normalize(LightPos);
    vec3 NML;
    
    /*smooth shading*/
    if(isSmoothShading){
        NML = normalize(Normal);
        if(dot(VIEW, NML) < 0.0f)
            NML = -NML;
    }
    else{/*flat shading*/
        NML = normalize(cross(dFdx(ViewDir), dFdy(ViewDir)));
    }

    float Id = max(dot(NML, LIGHT), 0.0f);
    vec3 diffuse = Kd * DIFFUSE_COLOR * Id;

    float Is = 1.0f;
    if(Id > 0 ){
        vec3 reflectDir = reflect(-LIGHT, NML);
        Is = pow(max(dot(reflectDir, VIEW), 0.0f), shininess) * shininess * 0.25f;
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
        int idx = 31 * level + 7 * cubeI + 13 * cubeJ + 17 * cubeK;
        idx = idx & 7;
        vec3 c = vec3(cubeColors[idx]) / 255.f;
        outColor = vec4((ambient + diffuse + specular) * c , 1.0f);  
    }
    else{
        outColor = vec4((ambient + diffuse + specular) * defaultColor , 1.0f);
    }
    
}