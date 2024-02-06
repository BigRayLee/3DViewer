#version 430 core
/* In variables */
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNorm_in;
layout (location = 2) in int idx_p;
layout (location = 3) in vec2 tex_in;
layout (location = 4) in uvec3 color_in;

layout(std430, binding = 0) restrict readonly buffer positions {float aPos_ssbo[];};
layout(std430, binding = 1) restrict readonly buffer normals {float aNorm_in_p[];};
layout(std430, binding = 3) restrict readonly buffer tex {float tex_in_p[];};
layout(std430, binding = 4) restrict readonly buffer clr {uint color_in_p[];};

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

/*out for fragment shader*/
layout (location = 0) out vec3 Normal;
layout (location = 1) out vec2 TexCoord;
layout (location = 2) out vec3 LightPos;
layout (location = 3) out vec3 ViewDir;
layout (location = 4) out vec3 Color;
layout (location = 5) out float lambda;

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
    /*for the single draw array call*/
    uint p = 3 * (idx_p + base_p);
    uint t = 2 * (idx_p + base_p);
    uint color_idx_p = idx_p + base_p;
    
    float dis;
    if(params.freeze_cur_frame)
       dis = computDis(params.vp_p, aPos);
    else
       dis = computDis(params.vp, aPos);

    lambda = computeBlend(level, params.kappa, dis);

    vec4 gl;
    if(params.isAdaptive)
        //gl = vec4(aPos, 1.0);
        gl = (lambda) * vec4(aPos, 1.0) + (1 - lambda) * vec4(aPos_ssbo[p], aPos_ssbo[p + 1], aPos_ssbo[p + 2], 1.0);
    else
        gl = vec4(aPos, 1.0);
        //gl = vec4(aPos_ssbo[p], aPos_ssbo[p + 1], aPos_ssbo[p + 2], 1.0);

    gl_Position = matrices.projection * matrices.view * matrices.model * gl;

    /*out variables*/
    Normal = lambda * aNorm_in + (1 - lambda) * vec3(aNorm_in_p[p], aNorm_in_p[p + 1], aNorm_in_p[p + 2]);
    ViewDir = params.vp - (matrices.model * gl).xyz;
    LightPos = ViewDir;
    
    /*pass texture coordinates to fragment shader*/
    TexCoord = lambda * tex_in + (1 - lambda) * vec2(tex_in_p[t], tex_in_p[t + 1]);
    //Color = lambda * vec3(color_in.x/255.0, color_in.y/255.0, color_in.z/255.0) + 
    //        (1 - lambda) * vec3(color_in_p[color_idx_p].x/255.0, color_in_p[color_idx_p].y/255.0, color_in_p[color_idx_p].z/255.0);
    Color = vec3(color_in.x/255.0, color_in.y/255.0, color_in.z/255.0);

}
