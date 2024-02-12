#version 430 core
/* In variables */
layout (location = 0) in vec3 pos;
layout (location = 1) in vec3 normal;
layout (location = 2) in int idx;

layout(std430, binding = 0) restrict readonly buffer positions {float parentPos[];};
layout(std430, binding = 1) restrict readonly buffer normals {float parentNormal[];};

layout (std140, binding = 0) uniform Matrices
{
    mat4 projection;
    mat4 view;
    mat4 model;
} matrices;

struct AdaptiveParameters{
    float kappa;
    float sigma;
    vec3 vp;
    vec3 freezeVp;
    bool isFreezeFrame;
    bool isAdaptive;
};

uniform AdaptiveParameters params;

/* Base index for the ssbo */
uniform int level;
uniform int parentBase;

/* Out to fragment shader*/
layout (location = 0) out vec3 nml;
layout (location = 1) out vec2 texCoord;
layout (location = 2) out vec3 lightPos;
layout (location = 3) out vec3 viewDir;
layout (location = 5) out float lambda;

float ComputeDistance(vec3 vp, vec3 vert){
    vec4 posWorld =  matrices.model * vec4(vert.xyz, 1.0);
    vec3 dis = abs(vp.xyz - posWorld.xyz);
    float maxDis = max(max(dis.x, dis.y), dis.z);

    return maxDis;
}

float ComputeLambda(int level, float kappa, float dis){
    float minDis = (1 + kappa + params.sigma) / (1 << level); 
    float maxDis = (kappa - params.sigma) / (1 << (level - 1));
    
    return clamp((maxDis - dis) / (maxDis - minDis), 0.0f, 1.0f);
}

void main(){    
    float dis;
    if(params.isFreezeFrame){
        dis = ComputeDistance(params.freezeVp, pos);
    }
    else{
        dis = ComputeDistance(params.vp, pos);
    }

    lambda = 1.0f;
    vec4 gl = vec4(pos, 1.0f);
    nml = normal;
    if(params.isAdaptive){
        uint p = 3 * (idx + parentBase);
        lambda = ComputeLambda(level, params.kappa, dis);

        gl = lambda * vec4(pos, 1.0) + (1 - lambda) * vec4(parentPos[p], parentPos[p + 1], parentPos[p + 2], 1.0);
        nml = lambda * normal + (1 - lambda) * vec3(parentNormal[p], parentNormal[p + 1], parentNormal[p + 2]);
    }   
   
    gl_Position = matrices.projection * matrices.view * matrices.model * gl;
    
    viewDir = params.vp - (matrices.model * gl).xyz;
    lightPos = viewDir;
}
