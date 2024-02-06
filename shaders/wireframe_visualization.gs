#version 430 core
layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;

layout (location = 0) in vec3 Nomal0[];
layout (location = 1) in vec2 TexCoord0[];
layout (location = 2) in vec3 LightPos0[];
layout (location = 3) in vec3 ViewDir0[];
layout (location = 4) in vec3 Color0[];
layout (location = 5) in float lambda0[];

out vec3 GNormal;
out vec3 GPosition;
noperspective out vec3 GEdgDisctance;

uniform mat4 ViewportMatrix;

layout (location = 0) out vec3 Nomal;
layout (location = 1) out vec2 TexCoord;
layout (location = 2) out vec3 LightPos;
layout (location = 3) out vec3 ViewDir;
layout (location = 4) out vec3 Color;
layout (location = 5) out float lambda;

void main(){
    vec2 p0 = vec2(ViewportMatrix * (gl_in[0].gl_Position / gl_in[0].gl_Position.w));
    vec2 p1 = vec2(ViewportMatrix * (gl_in[1].gl_Position / gl_in[1].gl_Position.w));
    vec2 p2 = vec2(ViewportMatrix * (gl_in[2].gl_Position / gl_in[2].gl_Position.w));

    float a = length(p1 - p2);
    float b = length(p2 - p0);
    float c = length(p1 - p0);
    float alpha = acos((b*b + c*c - a*a) / (2.0 * b * c));
    float beta = acos((a*a + c*c - b*b) / (2.0 * a * c));

    float ha = abs(c * sin(beta));
    float hb = abs(c * sin(alpha));
    float hc = abs(b * sin(alpha));

    GEdgDisctance = vec3(ha, 0, 0);
    gl_Position = gl_in[0].gl_Position;
    gl_ClipDistance = gl_in[0].gl_ClipDistance;
    Nomal = Nomal0[0];
    TexCoord = TexCoord0[0]; 
    LightPos = LightPos0[0];
    ViewDir = ViewDir0[0];
    Color = Color0[0];
    lambda = lambda0[0]; 
    EmitVertex();

    GEdgDisctance = vec3(0, hb, 0);
    gl_Position = gl_in[1].gl_Position;
    gl_ClipDistance = gl_in[1].gl_ClipDistance;
    Nomal = Nomal0[1];
    TexCoord = TexCoord0[1]; 
    LightPos = LightPos0[1];
    ViewDir = ViewDir0[1];
    Color = Color0[1];
    lambda = lambda0[1]; 
    EmitVertex();

    GEdgDisctance = vec3(0, 0, hc);
    gl_Position = gl_in[2].gl_Position;
    gl_ClipDistance = gl_in[2].gl_ClipDistance;
    Nomal = Nomal0[2];
    TexCoord = TexCoord0[2]; 
    LightPos = LightPos0[2];
    ViewDir = ViewDir0[2];
    Color = Color0[2];
    lambda = lambda0[2]; 
    EmitVertex();

    EndPrimitive();
}


