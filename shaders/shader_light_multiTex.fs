#version 460 core
in vec3 Normal;
in vec3 LightPos;
in vec3 ViewDir;

in vec2 TexCoord;
flat in int level_out;

uniform bool texture_exist;
uniform vec3 Color_default;
uniform bool smooth_shadering;

/*colorize LOD*/
uniform vec3 Color_lod;
uniform bool lod_colorized;

/*bind with texture file*/
uniform sampler2D texture1;
uniform sampler2D texture2;
uniform sampler2D texture3;
uniform sampler2D texture4;
uniform sampler2D texture5;
uniform sampler2D texture6;
uniform sampler2D texture7;
uniform sampler2D texture8;
uniform sampler2D texture9;
uniform sampler2D texture10;

out vec4 FragColor;

#define AMBIENT_COLOR vec3(1.f, 1.0f, 1.0f)
#define Ka 0.1f
#define DIFFUSE_COLOR vec3(1.0, 1.0, 1.0)
#define Kd 0.9f
#define SPECULAR_COLOR vec3(1.0f, 1.0f, 1.0f)
#define Ks 0.1f
#define shininess 8

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

    float diff = max(dot(NML, LIGHT), 0.0);
    vec3 diffuse = Kd * DIFFUSE_COLOR * diff;

    float spec = 1.0;
    if(diff > 0 ){
        vec3 reflectDir = reflect(-LIGHT, NML);
        spec = pow(max(dot(reflectDir, VIEW), 0.0), shininess);
    }
    
    vec3 specular = Ks * SPECULAR_COLOR * spec;
    
    if(texture_exist){
        vec4 res;
        
        res = vec4(1.0);
        if(TexCoord.y < 5.0){
            if(floor(TexCoord.y) == 0)
                res = texture(texture1, vec2(TexCoord.x, TexCoord.y - floor(TexCoord.y)));
            else if(floor(TexCoord.y) == 1)
                res = texture(texture2, vec2(TexCoord.x, TexCoord.y - floor(TexCoord.y)));
            else if(floor(TexCoord.y) == 2)
                res = texture(texture3, vec2(TexCoord.x, TexCoord.y - floor(TexCoord.y)));
            else
                res = texture(texture4, vec2(TexCoord.x, TexCoord.y - floor(TexCoord.y)));   
        }
        else if(TexCoord.y > 5.0 && TexCoord.y < 10.0){
            if(floor(TexCoord.y) - 1 == 4)
                res = texture(texture5, vec2(TexCoord.x, TexCoord.y - floor(TexCoord.y) - 1.0));
            else if(floor(TexCoord.y) - 1 == 5)
                res = texture(texture6, vec2(TexCoord.x, TexCoord.y - floor(TexCoord.y) - 1.0));
            else if(floor(TexCoord.y) - 1 == 6)
                res = texture(texture7, vec2(TexCoord.x, TexCoord.y - floor(TexCoord.y) - 1.0));
            else
                res = texture(texture8, vec2(TexCoord.x, TexCoord.y - floor(TexCoord.y) - 1.0));  
        }
        else if(TexCoord.y > 10.0 && TexCoord.y < 13.0){
            if(floor(TexCoord.y) - 2 == 8)
                res = texture(texture9, vec2(TexCoord.x, TexCoord.y - floor(TexCoord.y) - 2.0));
            else
                res = texture(texture10, vec2(TexCoord.x, TexCoord.y - floor(TexCoord.y) - 2.0));
        }
        else
            res = vec4(1.0);

        if(lod_colorized)
            FragColor = res * vec4((ambient + diffuse + specular) * Color_lod, 1.0f);
        else
            FragColor = res * vec4((ambient + diffuse + specular) , 1.0f);
    }
        
    else{
        if(lod_colorized){
            if(level_out == 0)
                FragColor = vec4((ambient + diffuse + specular) * vec3(0.0, 1.0, 1.0) , 1.0f);
            else
                FragColor = vec4((ambient + diffuse + specular) * Color_lod , 1.0f);
        }
        
        else
            FragColor = vec4((ambient + diffuse + specular) * Color_default , 1.0f);
    }
        
}