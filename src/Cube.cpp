#include "Cube.h"

Cube::Cube(){}

/*get the bottom point information of boundingbox based on the logical cell size*/
void Cube::ComputeBottomVertex(float bottom[3], int ijk[3], float length, float min[3]){
    for(int i = 0; i < 3; ++i){
        bottom[i] = ijk[i] * length + min[i];
        top[i] = bottom[i] + length;
    }
}

uint64_t Cube::GetBoxCoord() const{
    return ijk_64;
}

int Cube::GetTriangleCount() const{
    return triangleCount;
}

int Cube::GetVertexCount() const {
    return vertCount;
}

int Cube::GetIndexCount() const{
    return triangleCount * 3;
}

int Cube::GetTextureCount() const {
    return uvCount;
}

int Cube::GetTextureIndexCount() const{
    return idxUVCount;
}

/*
    Compute the Bounding Box vertex
         F----G
        /|    /|
        E|---H |
        | A--|-B
        |/   |/
        D----C
            
*/
void Cube::CalculateBBXVertex(float length){
    float vertices[24] = {
                            bottom[0], bottom[1], bottom[2],                           //A
                            bottom[0], bottom[1] + length, bottom[2],                  //B
                            bottom[0] + length, bottom[1] + length, bottom[2],         //C
                            bottom[0] + length, bottom[1], bottom[2],                  //D
                            bottom[0] + length, bottom[1], bottom[2] + length,         //E
                            bottom[0], bottom[1], bottom[2] + length,                  //F
                            bottom[0], bottom[1] + length, bottom[2] + length,         //G
                            bottom[0] + length, bottom[1] + length, bottom[2] + length //H
                        };
    memcpy(bbxVertices, vertices, 24 * sizeof(float));
}