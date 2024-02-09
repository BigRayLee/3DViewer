#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <string>
#include <vector>
#include <stack>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <utility>
#include <unordered_map>
#include <queue>
#include "glm/glm.hpp"
#include "glm/gtc/matrix_transform.hpp"
#include "Shader.h"
#include "Camera.h"
#include "LOD.h"
#include "Viewer.h"
#include "Frustum.h"
#include "BoundingBoxDraw.h"
#include "Chrono.h"

using namespace std;

static constexpr int SC_MAX_FRAMEBUFFER_WIDTH = 1920;
static constexpr int SC_MAX_FRAMEBUFFER_HEIGHT = 1080;

float CalculateDistanceToCube(float cubeBottom[3], glm::vec3 viewpoint, glm::mat4 model, glm::mat4 view, float cubeLength);

int Display(HLOD &multiResoModel, int maxLevel);

int LoadChildCube(int parentCoord[3], LOD *meshbook[], 
                 glm::mat4 projection, glm::mat4 view, glm::mat4 model, Camera* camera, int maxLevel, int curLevel);

void SelectCubeVisbility(LOD *meshbook[], int maxLevel, glm::mat4 model, glm::mat4 view, glm::mat4 projection);

bool AfterFrustumCulling(Cube &cube, Mat4 pvm);

