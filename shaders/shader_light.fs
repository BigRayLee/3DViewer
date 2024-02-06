#version 430 core
layout (location = 0) in vec3 Normal;
layout (location = 1) in vec2 TexCoord;
layout (location = 2) in vec3 LightPos;
layout (location = 3) in vec3 ViewDir;
layout (location = 4) in vec3 Color;
layout (location = 5) in float lambda;

uniform int level;
uniform int maxLevel;
uniform bool texture_exist;
uniform bool colorExist;

/*colorize LOD*/
uniform vec3 Color_default;
uniform vec3 Color_lod;
uniform bool lod_colorized;

/*colorized cube*/
uniform bool cube_colorized;
uniform int  cube_i;
uniform int  cube_j;
uniform int  cube_k;


/*bind with texture array*/
uniform sampler2DArray textureArray;

/*bind with texture file*/
uniform sampler2D texture1;

uniform bool smooth_shadering;

out vec4 FragColor;

#define AMBIENT_COLOR vec3(1.f, 1.0f, 1.0f)
#define Ka 0.1f
#define DIFFUSE_COLOR vec3(1.0, 1.0, 1.0)
#define Kd 0.8f
#define SPECULAR_COLOR vec3(1.0f, 1.0f, 1.0f)
#define Ks 0.1f
#define shininess 8

// #define AMBIENT_COLOR vec3(1.f, 0.8f, 1.f)
// #define Ka 0.1f
// #define DIFFUSE_COLOR vec3(.88f, .75f, 0.43f)
// #define Kd .8f
// #define SPECULAR_COLOR vec3(1.f, 1.f, 1.f)
// #define Ks 0.1f
// #define shininess 8

const ivec3 cube_colors[8] = {
	{120,28,129},
	{64,67,153},
	{72,139,194},
	{107,178,140},
	{159,190,87},
	{210,179,63},
	{231,126,49},
	{217,33,32}
};


void main(){

    /*ambient light*/
    vec3 ambient = Ka * AMBIENT_COLOR;

    /*diffuse light*/
    vec3 VIEW = normalize(ViewDir);
    vec3 LIGHT = normalize(LightPos);
    vec3 NML;
    /*smooth shading*/
    if(smooth_shadering){
        NML = normalize(Normal);
        if(dot(VIEW, NML) < 0)
            NML = -NML;
    }
    else{/*flat shading*/
        NML = normalize(cross(dFdx(ViewDir), dFdy(ViewDir)));
    }

    float Id = max(dot(NML, LIGHT), 0.0);
    vec3 diffuse = Kd * DIFFUSE_COLOR * Id;

    float Is = 1.0;
    if(Id > 0 ){
        vec3 reflectDir = reflect(-LIGHT, NML);
        Is = pow(max(dot(reflectDir, VIEW), 0.0), shininess) * shininess / 4;
    }
    
    vec3 specular = Ks * SPECULAR_COLOR * Is;

    vec3 full = vec3(0.);
    
    if(texture_exist){
        if(lod_colorized)
            FragColor = texture(texture1, TexCoord) * vec4((ambient + diffuse + specular) * Color_lod, 1.0f);
        else
            FragColor = texture(texture1, TexCoord) * vec4((ambient + diffuse + specular) , 1.0f);
        
    }
    else if(colorExist){
        if(lod_colorized)
            FragColor = vec4((ambient + diffuse + specular) * Color_lod , 1.0f);
        else{
            //vec3 crl = vec3(Color.x/255.0, Color.y/255.0, Color.z/255.0);
            // vec3 crl = vec3(Color.x, Color.y, Color.z);
            FragColor = vec4((ambient + diffuse + specular) * Color, 1.0f);
        }
    }
    else{
        if(lod_colorized){
            //normal one 
            // if(level == 0)
            //     FragColor = vec4((ambient + diffuse + specular) * vec3(0.0, 1.0, 1.0) , 1.0f);
            // else
            //     FragColor = vec4((ambient + diffuse + specular) * Color_lod , 1.0f);

            // r g b colors shading 
            float l = maxLevel - level + (1 - lambda);
            vec3 c = vec3(0.0f);
            if (l < 1) 
            {
                c.r = 1.0f - l;
                c.g = l;
            }
            else if (l < 2)
            {
                c.g = 2.0f - l;
                c.b = l - 1.0f;
            }
            else
            {
                c.r = smoothstep(2.0f, 6.0f, l);
                c.b = 1.0f - c.r;
            }
            FragColor = vec4((ambient + diffuse + specular) * c , 1.0f);
        }
        else if(cube_colorized){
            //if(gl_FragCoord.x < 960){
                int idx = 31 * level + 7 * cube_i + 13 * cube_j + 17 * cube_k;
                idx = idx & 7;
                vec3 c = vec3(cube_colors[idx]) / 255.f;
                FragColor = vec4((ambient + diffuse + specular) * c , 1.0f);
            // }
            // else 
            //     FragColor = vec4((ambient + diffuse + specular) * Color_default , 1.0f);
            
        }
        else{

            // float r = clamp((0.55 * (Normal.z > 0.6 ? 1.0 : 0.0) + 0.45 * min(0.7 +  max(Normal.z, 0) + 0.1 * (cos(31.0 * Normal.x) + cos(27.0 * Normal.y) + cos(4431.0 * Normal.z)), 1.0)), 0.0, 1.0);

            // float g = clamp((0.6 * (Normal.z > 0.6 ? 1.0 : 0.0) + 0.4 * min(0.7 +  max(Normal.z, 0) + 0.1 * (cos(31.0 * Normal.x) + cos(27.0 * Normal.y) + cos(4431.0 * Normal.z)), 1.0)), 0.0, 1.0);

            // float b = clamp((0.65 * (Normal.z > 0.6 ? 1.0 : 0.0) + 0.35 * min(0.7 +  max(Normal.z, 0) + 0.1 * (cos(31.0 * Normal.x) + cos(27.0 * Normal.y) + cos(4431.0 * Normal.z)), 1.0)), 0.0, 1.0); 
	
            // FragColor = vec4((ambient + diffuse + specular) * vec3(r,g,b) , 1.0f);

            FragColor = vec4((ambient + diffuse + specular) * Color_default , 1.0f);

            //full = (ambient + diffuse + specular) * Color_default;
        }
        //FragColor = vec4(full , 1.0f);    
    }
        
}