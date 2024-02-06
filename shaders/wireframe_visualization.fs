#version 430 core 
layout (location = 0) in vec3 Normal;
layout (location = 1) in vec2 TexCoord;
layout (location = 2) in vec3 LightPos;
layout (location = 3) in vec3 ViewDir;
layout (location = 4) in vec3 Color;
layout (location = 5) in float lambda;

uniform bool texture_exist;
uniform bool colorExist;
uniform int level;

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
uniform int maxLevel;


/*Line structure*/
uniform struct LinInfo{
    float Width;
    vec4  Color;
} Line;

noperspective in vec3 GEdgDisctance;

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

    vec4 full = vec4(0.);
    
    if(texture_exist){
        if(lod_colorized)
            full = texture(texture1, TexCoord) * vec4((ambient + diffuse + specular) * Color_lod, 1.0f);
        else
            full = texture(texture1, TexCoord) * vec4((ambient + diffuse + specular) , 1.0f);
        
    }
    else if(colorExist){
        if(lod_colorized)
            full = vec4((ambient + diffuse + specular) * Color_lod , 1.0f);
        else{
            //vec3 crl = vec3(Color.x/255.0, Color.y/255.0, Color.z/255.0);
            // vec3 crl = vec3(Color.x, Color.y, Color.z);
            full = vec4((ambient + diffuse + specular) * Color, 1.0f);
        }
    }
    else{
        if(lod_colorized){
            float l = maxLevel - level + (1 - lambda);
            vec3 c = vec3(0.0f);
            if (l < 1) 
            {
                c.r = 1 - l;
                c.g = l;
            }
            else if (l < 2)
            {
                c.g = 2 - l;
                c.b = l - 1;
            }
            else
            {
                c.r = smoothstep(2.0, 6.0, l);
                c.b = 1 - c.r;
            }
            full = vec4((ambient + diffuse + specular) * c , 1.0f);
        }
        else if(cube_colorized){
            //if(gl_FragCoord.x < 960){
                int idx = 31 * level + 7 * cube_i + 13 * cube_j + 17 * cube_k;
                idx = idx & 7;
                vec3 c = vec3(cube_colors[idx]) / 255.f;
                full = vec4((ambient + diffuse + specular) * c , 1.0f);
            // }
            // else 
            //     full = vec4((ambient + diffuse + specular) * Color_default , 1.0f);
            
        }
        else{
            full = vec4((ambient + diffuse + specular) * Color_default , 1.0f);
        }
    }

    if (GEdgDisctance.x >= 0) {
	    float d = min( GEdgDisctance.x, min(GEdgDisctance.y, GEdgDisctance.z ));

        float mixVal = 0.0;
        float lineWidth = 1.0f;
        // if (d < lineWidth - 1) {
        //     mixVal = 1.0;
        // } else if (d > lineWidth + 1) {
        //     mixVal = 0.0;
        // } else {
        //     float x = d - (lineWidth - 1);
        //     mixVal = exp2(-2.0 * x * x);
        // }        
        mixVal = smoothstep(lineWidth - 1, lineWidth + 1, d);

        vec4 lineColor = vec4(0.0f, 0.0f, 0.0f, 1.0f);
        FragColor = mix(lineColor, full, mixVal);
        //FragColor = full;
    }
}