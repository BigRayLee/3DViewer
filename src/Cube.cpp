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

int Cube::GetTextureParentCount() const{
    return parentMesh.uvCount;
}

int Cube::GetVertexParentCount() const {
    return parentMesh.posCount;
}

int Cube::GetIndexTextureParentCount() const {
    return parentMesh.idxUVCount;
}

int Cube::GetIndexParentCount() const {
    return parentMesh.idxCount;
}

/* get the max position value and min position value for the quantization of position */
void Cube::GetAttributeMaxMin(){
    for(size_t i = 0; i < mesh.posCount; ++i){
        /*max value*/
        if(posMax[0] < mesh.positions[ 3 * i])
            posMax[0] = mesh.positions[ 3 * i];
        if(posMax[1] < mesh.positions[ 3 * i + 1])
            posMax[1] = mesh.positions[ 3 * i + 1];
        if(posMax[2] < mesh.positions[ 3 * i + 2])
            posMax[2] = mesh.positions[ 3 * i + 2];
        
        /*min value*/
        if(posMin[0] > mesh.positions[ 3 * i])
            posMin[0] = mesh.positions[ 3 * i];
        if(posMin[1] > mesh.positions[ 3 * i + 1])
            posMin[1] = mesh.positions[ 3 * i + 1];
        if(posMin[2] > mesh.positions[ 3 * i + 2])
            posMin[2] = mesh.positions[ 3 * i + 2];
        
        if(mesh.uvs){
            /*max value*/
            if(uvMax[0] < mesh.uvs[2 * i])
                uvMax[0] = mesh.uvs[2 * i];
            if(uvMax[1] < mesh.uvs[2 * i + 1])
                uvMax[1] = mesh.uvs[2 * i + 1];
            
            /*min value*/
            if(uvMin[0] > mesh.uvs[2 * i])
                uvMin[0] = mesh.uvs[2 * i];
            if(uvMin[1] > mesh.uvs[2 * i + 1])
                uvMin[1] = mesh.uvs[2 * i + 1];
        }
    }
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