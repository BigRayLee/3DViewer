#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <vector>
#include <stack>
#include <algorithm>
#include <utility>
#include <unordered_map>
#include <queue>
#include "Shader.h"
#include "Camera.h"
#include "HLOD.h"
#include "Viewer.h"
#include "Frustum.h"
#include "BoundingBoxDraw.h"
#include "Chrono.h"

using namespace std;

void SelectCubeVisbility(LOD *meshbook[], int maxLevel, Mat4 &pvmMat, Mat4 &model);

int LoadChildCube(int parentCoord[3], LOD *meshbook[], Mat4 &pvmMat, Mat4 &model, Camera *camera, int maxLevel, int curLevel);

float CalculateDistanceToCube(float cubeBottom[3], Vec3 viewpoint, Mat4 &model, float cubeLength);

bool AfterFrustumCulling(Cube &cube, Mat4 pvm);

int Display(HLOD &multiResoModel, int maxLevel);
