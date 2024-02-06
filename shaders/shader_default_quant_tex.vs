#version 430 core
layout (location = 1) in int in_Normal;
layout (location = 2) in uint idx_p;
layout (location = 3) in uvec2 tex_in;
layout (location = 4) in uvec3 color_in;

layout(std430, binding = 0) restrict readonly buffer positions {uint aPos_ssbo[];};
layout(std430, binding = 1) restrict readonly buffer normals {int aNorm_in_p[];};
layout(std430, binding = 3) restrict readonly buffer tex {uint tex_in_p[];};

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
out vec3 Normal;
out vec2 TexCoord;
flat out int level_out;
out vec3 LightPos;
out vec3 ViewDir;
out vec3 Color;
out float lambda;

/*quantization parameters*/
uniform vec3 decode_pos;
uniform vec3 decode_pos_min;

uniform vec3 decode_pos_p;
uniform vec3 decode_pos_min_p;

uniform vec2 decode_uv;
uniform vec2 decode_uv_min;

uniform vec2 decode_uv_p;
uniform vec2 decode_uv_min_p;

/*compute the distance from vp and position*/
float computDis(vec3 vp, vec3 vert){
    vec4 aPos_t =  matrices.model * vec4(vert.xyz, 1.0);
    vec3 v = abs(vp.xyz - aPos_t.xyz);
    float maxDis = max(max(v.x, v.y), v.z);

    return maxDis;
}

/*compute the blend value*/
float computeBlend(int level, float kappa, float dis){
    float min_d = (1 + kappa + params.sigma) / (1 << level); 
    float max_d = (kappa - params.sigma) / (1 << (level - 1));
    
    return clamp((max_d - dis) / (max_d - min_d), 0.0, 1.0);
}

void main(){
    /*get the offset value for the parent position*/
    uint i = 3 * (gl_VertexID);
    uint p = 3 * (idx_p + base_p);
    uint t = 2 * (idx_p + base_p);

    /*compute the value*/
    uvec3 parent, aPos;
    uint array_pos = (2 * i)/3;
    uint array_pos_p = (2 * p)/3;
    
    parent.x = (aPos_ssbo[array_pos_p] ) & 0xffff;
    parent.y = (aPos_ssbo[array_pos_p] >> 16) & 0xffff;
    parent.z = (aPos_ssbo[array_pos_p + 1] ) & 0xffff;

    aPos.x = (aPos_ssbo[array_pos]) & 0xffff;
    aPos.y = (aPos_ssbo[array_pos]  >> 16) & 0xffff;
    aPos.z = (aPos_ssbo[array_pos + 1] ) & 0xffff;


    /*de-quantization posintion*/
    vec4 aPos_decode = vec4(aPos.x * decode_pos.x + decode_pos_min.x, 
                                         aPos.y * decode_pos.y + decode_pos_min.y,
                                         aPos.z * decode_pos.z + decode_pos_min.z, 1);
    vec4 aPosp_decode = vec4(parent.x * decode_pos_p.x + decode_pos_min_p.x,
                                           parent.y * decode_pos_p.y + decode_pos_min_p.y,
                                           parent.z * decode_pos_p.z + decode_pos_min_p.z, 1);

    /*interpolation coefficient computation*/
    float dis;
    if(params.freeze_cur_frame)
       dis = computDis(params.vp_p, aPos_decode.xyz);
    else
       dis = computDis(params.vp, aPos_decode.xyz);

    float blend = computeBlend(level, params.kappa, dis);

    vec4 gl;
    if(params.isAdaptive)
        //gl = vec4(aPos, 1.0);
        gl = (blend) * vec4(aPos_decode.xyz, 1.0) + (1 - blend) * vec4(aPosp_decode.xyz, 1.0);
    else
        gl = vec4(aPos_decode.xyz, 1.0);
        //gl = vec4(aPosp_decode.xyz, 1.0);

    gl_Position = matrices.projection * matrices.view * matrices.model * gl;

    /*out variables*/
    /*dequantize the normals*/
    int nx = (in_Normal) & 0xff;
    int ny = (in_Normal >> 8) & 0xff;
    int nz = (in_Normal >> 16) & 0xff;
    float nx_f = nx > 0 ? (nx/127.5) : ((nx + 1)/127.5);
    float ny_f = ny > 0 ? (ny/127.5) : ((ny + 1)/127.5);
    float nz_f = nz > 0 ? (nz/127.5) : ((nz + 1)/127.5);
    //Normal = blend * vec3(aNorm_in_p[i], aNorm_in_p[i + 1], aNorm_in_p[i + 2]) + (1 - blend) * vec3(aNorm_in_p[p], aNorm_in_p[p + 1], aNorm_in_p[p + 2]);
    Normal = vec3(nx_f, ny_f, nz_f); 
    //Normal = mat3(transpose(inverse(matrices.model))) * aNorm_in; 
    ViewDir = params.vp - (matrices.model * gl).xyz;
    LightPos = ViewDir;
    //LightPos = vec3(0, 1, 1);
    level_out = level;

    /*pass texture coordinates to fragment shader*/
    float u = (tex_in.x * decode_uv.x) + decode_uv_min.x;
    float v = (tex_in.y * decode_uv.y) + decode_uv_min.y;
    //TexCoord = vec2(u, v);
    
    uint array_t = t/2;
    float u_p = (tex_in_p[array_t] & 0xffff) * decode_uv_p.x + decode_uv_min_p.x;
    float v_p = ((tex_in_p[array_t] >> 16) & 0xffff) * decode_uv_p.y + decode_uv_min_p.y;
    TexCoord = blend * vec2(u, v) + (1 - blend) * vec2(u_p, v_p);

    Color = vec3(color_in.x/255.0, color_in.y/255.0, color_in.z/255.0);

    lambda = blend;
}
