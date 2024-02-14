#include <fstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unistd.h>
#include <iostream>
#include "Cube.h"
#include "Utils.h"

using namespace std;

struct ModelReader{
    float min[3]{FLT_MAX, FLT_MAX, FLT_MAX};    /* Maximum position value */
    float max[3]{FLT_MIN, FLT_MIN, FLT_MIN};    /* Minimum position value */
    size_t triCount = 0;                        /* Number of triangles*/
    size_t vertCount = 0;                       /* Number of vertices*/
    Mesh *meshData = nullptr;          
    vector<string> texturesPath;                // TODO reserve 
    
    ModelReader();  
    ~ModelReader();
    void GetMaxMin(float x, float y, float z);  
    void CalculateNormals();
    int InputModel(string fileName);             /*  Read model */
    int PlyParser(const char *fileName);         /* .ply parser */
    int ObjParser(const char* fileName);         /* .obj parser */
    int BbxParser(const char* fileName);         /* .txt parser out of core data file parser */
};