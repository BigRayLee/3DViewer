#version 430 core
/* In variables */
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNorm_in;
layout (location = 2) in int idx_p;

layout(std430, binding = 0) restrict readonly buffer positions {float aPos_ssbo[];};
layout(std430, binding = 1) restrict readonly buffer normals {float aNorm_in_p[];};

layout (std140, binding = 0) uniform Matrices
{
    mat4 projection;
    mat4 view;
    mat4 model;
} matrices;

struct adaptiveParameters{
    float kappa;
    float sigma;
    vec3 vp;
    vec3 vp_p;
    bool freeze_cur_frame;
    bool isAdaptive;
};

uniform adaptiveParameters params;

/*base index for the ssbo*/
uniform int level;
uniform int base_p;

/*texture and color control*/
uniform bool texture_exist;
uniform bool colorExist;

/*smooth shading*/
uniform bool smooth_shadering;

/*out for fragment shader*/
layout (location = 0) out vec3 Nomal0;
layout (location = 1) out vec2 TexCoord0;
layout (location = 2) out vec3 LightPos0;
layout (location = 3) out vec3 ViewDir0;
layout (location = 4) out vec3 Color0;
layout (location = 5) out float lambda0;

float computDis(vec3 vp, vec3 vert){
    vec4 aPos_t =  matrices.model * vec4(vert.xyz, 1.0);
    vec3 v = abs(vp.xyz - aPos_t.xyz);
    float maxDis = max(max(v.x, v.y), v.z);

    return maxDis;
}

float computeBlend(int level, float kappa, float dis){
    float min_d = (1 + kappa + params.sigma) / (1 << level); 
    float max_d = (kappa - params.sigma) / (1 << (level - 1));
    
    return clamp((max_d - dis) / (max_d - min_d), 0.0, 1.0);
}

void main(){
    float dis;
    if(params.freeze_cur_frame){
        dis = computDis(params.vp_p, aPos);
    }
    else{
        dis = computDis(params.vp, aPos);
    }
    lambda0 = 1.0;
    vec4 gl = vec4(aPos, 1.0);
    Nomal0 = aNorm_in;
    if(params.isAdaptive){
        uint p = 3 * (idx_p + base_p);
        lambda0 = computeBlend(level, params.kappa, dis);
        gl = lambda0 * vec4(aPos, 1.0) + (1 - lambda0) * vec4(aPos_ssbo[p], aPos_ssbo[p + 1], aPos_ssbo[p + 2], 1.0);
        Nomal0 = lambda0 * aNorm_in + (1 - lambda0) * vec3(aNorm_in_p[p], aNorm_in_p[p + 1], aNorm_in_p[p + 2]);
    }   

    gl_Position = matrices.projection * matrices.view * matrices.model * gl;
    
    ViewDir0 = params.vp - (matrices.model * gl).xyz;
    LightPos0 = ViewDir0;
}
