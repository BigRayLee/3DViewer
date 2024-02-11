#include "Display.h"
#include "./math/vec4.h"

glm::vec3 freezeVp;
Viewer *viewer = new Viewer;                              /* Initialize the viewer */
std::stack<std::pair<int, uint64_t>> freezeRenderStack;   /*freeze stack*/
stack<pair<int, uint64_t>> renderStack;                   /* Render stack */
size_t renderedTriSum = 0;
GLuint pos, nml, clr, remap, uv, idx;       

void SaveScreenshotToFile(std::string filename, int windowWidth, int windowHeight)
{
    const int numberOfPixels = windowWidth * windowHeight * 3;
    unsigned char pixels[numberOfPixels];

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_FRONT);
    glReadPixels(0, 0, windowWidth, windowHeight, GL_RGB, GL_UNSIGNED_BYTE, pixels);

    FILE *outputFile = fopen(filename.c_str(), "w");
    short header[] = {0, 2, 0, 0, 0, 0, (short)windowWidth, (short)windowHeight, 24};

    fwrite(&header, sizeof(header), 1, outputFile);
    fwrite(pixels, numberOfPixels, 1, outputFile);
    fclose(outputFile);

    printf("Finish writing to file.\n");
}


float CalculateDistanceToCube(float cubeBottom[3], glm::vec3 viewpoint, glm::mat4 model, glm::mat4 view, float cubeLength)
{
    float maxDis = 0.0f;
    glm::vec4 bottom = model * glm::vec4(cubeBottom[0], cubeBottom[1], cubeBottom[2], 1.0f);
    glm::vec4 length = model * glm::vec4(cubeLength, cubeLength, cubeLength, 1.0f);

    float disX = 0.0f;
    if (viewpoint.x < bottom.x) disX = abs(bottom.x - viewpoint.x);
    else if (viewpoint.x > bottom.x + length[0]) disX = abs(viewpoint.x - bottom.x - length[0]);
    else disX = 0.0f;

    // y
    float disY = 0.0f;
    if (viewpoint.y < bottom.y) disY = abs(bottom.y - viewpoint.y);
    else if (viewpoint.y > bottom.y + length[1]) disY = abs(viewpoint.y - bottom.y - length[1]);
    else disY = 0.0f;

    // z
    float disZ = 0.0f;
    if (viewpoint.z < bottom.z) disZ = abs(bottom.z - viewpoint.z);
    else if (viewpoint.z > bottom.z + length[2]) disZ = abs(viewpoint.z - bottom.z - length[2]);
    else disZ = 0.0f;

    maxDis = max(max(disX, disY), disZ);
    return maxDis;
}

int LoadChildCube(int parentCoord[3], LOD *meshbook[],
                  glm::mat4 projection, glm::mat4 view, glm::mat4 model, Camera* camera, int maxLevel, int curLevel){
    if (curLevel < 0) return -1;

    LOD *mg = meshbook[curLevel];
    Mat4 pvm = viewer->camera->GlmToMAT4(projection * view * model);

    int xyz[3];
    for (int ix = 0; ix < 2; ix++){
        xyz[0] = parentCoord[0] * 2 + ix;
        for (int iy = 0; iy < 2; iy++){
            xyz[1] = parentCoord[1] * 2 + iy;
            for (int iz = 0; iz < 2; iz++){
                xyz[2] = parentCoord[2] * 2 + iz;
                uint64_t coord = (uint64_t)(xyz[0]) | ((uint64_t)(xyz[1]) << 16) | ((uint64_t)(xyz[2]) << 32);
                
                if (mg->cubeTable.count(coord) == 0){
                    continue;
                }
                    
                glm::vec3 cameraPos = glm::vec3(camera->position.x, camera->position.y, camera->position.z);
                float dis = CalculateDistanceToCube(mg->cubeTable[coord].bottom, cameraPos, model, view, mg->cubeLength); 

                if (dis >= (viewer->kappa * pow(2, -(mg->level))) || mg->level == maxLevel){
                    if (AfterFrustumCulling(mg->cubeTable[coord], pvm)){
                        renderStack.push(make_pair(mg->level, coord));
                    }
                }
                else{
                    LoadChildCube(xyz, meshbook, projection, view, model, camera, maxLevel, curLevel - 1);
                }

            }
        }
    }
    return 0;
}

bool AfterFrustumCulling(Cube &cube, Mat4 pvm)
{
    Aabb bbox;
    bbox.min.x = cube.bottom[0];
    bbox.min.y = cube.bottom[1];
    bbox.min.z = cube.bottom[2];
    bbox.max.x = cube.top[0];
    bbox.max.y = cube.top[1];
    bbox.max.z = cube.top[2];

    int visible = is_visible(bbox, &pvm(0, 0));
    if (visible != 0) return true;
    else return false;
}


void SelectCubeVisbility(LOD *meshbook[], int maxLevel, glm::mat4 model, glm::mat4 view, glm::mat4 projection)
{
    Mat4 pvm = viewer->camera->GlmToMAT4(projection * view * model);
    for (auto &c : meshbook[maxLevel]->cubeTable){

        glm::vec3 cameraPos = glm::vec3(viewer->camera->position.x, viewer->camera->position.y, viewer->camera->position.z);
        float dis = CalculateDistanceToCube(c.second.bottom, cameraPos, model, view, meshbook[maxLevel]->cubeLength); // CalculateDistanceToCube(bottom_cam, camera.cameraPos, 1.0/meshbook[level-1]->x0[3]); meshbook[level-1]->length c.second.length

        if (dis >= (viewer->kappa * pow(2, -meshbook[maxLevel]->level)) || meshbook[maxLevel]->level == maxLevel){
            if (AfterFrustumCulling(c.second, pvm)){
                renderStack.push(make_pair(meshbook[maxLevel]->level, c.first));
            }
        }
        else{
            LoadChildCube(c.second.coord, meshbook, projection, view, model, viewer->camera, maxLevel, maxLevel - 1);
        }

    }
}

void ObjectBufferInit(Mesh& data){
    /* Position */
    glGenBuffers(1, &pos);
    glBindBuffer(GL_ARRAY_BUFFER, pos);
    glBufferData(GL_ARRAY_BUFFER, data.posCount * VERTEX_STRIDE, &(data.positions[0]), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Normal */
    glGenBuffers(1, &nml);
    glBindBuffer(GL_ARRAY_BUFFER, nml);
    glBufferData(GL_ARRAY_BUFFER, data.posCount * VERTEX_STRIDE, &(data.normals[0]), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Parent-children dependency */
    glGenBuffers(1, &remap);
    glBindBuffer(GL_ARRAY_BUFFER, remap);
    glBufferData(GL_ARRAY_BUFFER, data.posCount * sizeof(uint32_t), &(data.remap[0]), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Index */
    glGenBuffers(1, &idx);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idx);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.idxCount * sizeof(uint32_t), &(data.indices[0]), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glGenBuffers(1, &uv);
    glGenBuffers(1, &clr);
}

/* Bind the vbo buffer with vao (original data) and vao (isQuantized data) */
void BindVAOBuffer(GLuint &vao)
{
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, pos);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GL_FLOAT),
                            (void *)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, nml);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GL_FLOAT),
                            (void *)0);
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, remap);
    glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 1 * sizeof(GL_UNSIGNED_INT),
                            (void *)0);
    glEnableVertexAttribArray(2);

    glBindBuffer(GL_ARRAY_BUFFER, uv);
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GL_FLOAT),
                            (void *)0);
    glEnableVertexAttribArray(3);

    glBindBuffer(GL_ARRAY_BUFFER, clr);
    glVertexAttribIPointer(4, 3, GL_UNSIGNED_BYTE, 3 * sizeof(unsigned char),
                            (void *)0);
    glEnableVertexAttribArray(4);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idx);
    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
}

int Display(HLOD &multiResModel, int maxLevel)
{
    glm::mat4 modelMat(1.0f);
    Shader *shader = new Shader();
    Shader *bbxShader = new Shader();
    BoundingBoxDraw* bbxDrawer = new BoundingBoxDraw();
    string vs_shader = "./shaders/shader_default.vs";
    string fs_shader = "./shaders/shader_light.fs";
    float maxModelSize = multiResModel.lods[maxLevel]->cubeLength;
    GLuint vao;                   /* defualt VAO */
    GLuint uboMatices;            /* Uniform matrices */
    int frameCount = 0;           /* Screen shoot counter */
    int renderedCubeCount = 0; 

    bool isNormalVis = false;
    bool isBbxDisplay = false;
    bool isEdgeDisplay = false;
    bool isLodColorized = false;
    bool isCubeColorized = false;
    bool isAdaptive = true;

    /* Init the glfw init */
    viewer->InitGlfwFunctions();

    /* Imgui initial*/
    viewer->imgui->ImguiInial(viewer->window);
    viewer->imgui->inputVertexCount = multiResModel.lods[0]->totalVertCount;
    viewer->imgui->inputTriCount = multiResModel.lods[0]->totalTriCount;
    viewer->imgui->hlodTriCount = multiResModel.data.idxCount / 3;
    viewer->imgui->hlodVertexCount = multiResModel.data.posCount;
    viewer->imgui->gpuVendorStr = const_cast<unsigned char*>(glGetString(GL_VENDOR));
    viewer->imgui->gpuModelStr = const_cast<unsigned char*>(glGetString(GL_RENDERER));

    /* GL function controlling */
    glEnable(GL_DEBUG_OUTPUT);
    glDisable(GL_CULL_FACE);
    glCullFace(GL_FRONT);
    glFrontFace(GL_CCW);

    glEnable(GL_DEPTH_TEST | GL_DEPTH_BUFFER_BIT);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Bind VAO VBO*/
    TimerStart();
    ObjectBufferInit(multiResModel.data);
    TimerStop("Loading data to GPU: ");
    BindVAOBuffer(vao);

    /* Build shader */
    shader->Build(vs_shader.c_str(), fs_shader.c_str());
    unsigned int uniformBlockIndexVertex = glGetUniformBlockIndex(shader->ID, "Matrices");
    glUniformBlockBinding(shader->ID, uniformBlockIndexVertex, 0);

    bbxShader->Build("./shaders/shader_bbx.vs", "./shaders/shader_bbx.fs");
    unsigned int uniformBlockIndexBBX = glGetUniformBlockIndex(bbxShader->ID, "Matrices");
    glUniformBlockBinding(bbxShader->ID, uniformBlockIndexBBX, 0);
    
    /* Uniform matrices generation */
    glGenBuffers(1, &uboMatices);
    glBindBuffer(GL_UNIFORM_BUFFER, uboMatices);
    glBufferData(GL_UNIFORM_BUFFER, 3 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboMatices, 0, 3 * sizeof(glm::mat4));

    /* Calculate model center */
    viewer->scale = 1.0f / maxModelSize;
    modelMat = glm::scale(modelMat, glm::vec3(viewer->scale));

    glm::vec3 center = glm::vec3((multiResModel.max[0] + multiResModel.min[0]) * 0.5f,
                                 (multiResModel.max[1] + multiResModel.min[1]) * 0.5f,
                                 (multiResModel.max[2] + multiResModel.min[2]) * 0.5f);

    glm::vec4 cameraTarget = modelMat * glm::vec4(center, 1.0);
    
    viewer->camera->set_position(Vec3(cameraTarget.x, cameraTarget.y, cameraTarget.z) + Vec3(0.0f, 0.0f, 3.0f * viewer->scale * maxModelSize));
    viewer->camera->set_near(0.0001 * viewer->scale * maxModelSize);
    viewer->camera->set_far(100.0f * viewer->scale * maxModelSize);
    viewer->target = Vec3(cameraTarget.x, cameraTarget.y, cameraTarget.z);

    /* Progressive buffer */
    viewer->kappa = viewer->imgui->kappa;
    viewer->sigma = viewer->imgui->sigma;

    /* BbxDrawer initialization */
    bbxDrawer->InitBuffer(multiResModel.lods[maxLevel]->cubeTable[0], multiResModel.lods[maxLevel]->cubeLength);

    /* Render loop */
    while (!glfwWindowShouldClose(viewer->window))
    {
        renderedTriSum = 0; 
        renderedCubeCount = 0;
        frameCount++;

        glClearColor(viewer->imgui->color.x, viewer->imgui->color.y, viewer->imgui->color.z, viewer->imgui->color.w);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glfwSwapInterval(viewer->imgui->VSync);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glfwWindowHint(GLFW_SAMPLES, 4);

        /* Wireframe mode */
        if(isEdgeDisplay){
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }
        else{
            glPolygonMode(GL_FRONT_AND_BACK, GL_POLYGON);
        }

        /* Get camera position in every frame */
        glm::vec3 cameraPos = glm::vec3(viewer->camera->position.x, viewer->camera->position.y, viewer->camera->position.z);

        /* Time recording for the mouse operation */
        float currentFrame = glfwGetTime();
        viewer->deltaTime = currentFrame - viewer->lastFrame;
        viewer->lastFrame = currentFrame;

        viewer->ProcessInput(viewer->window);

        /* Transformation matrix */
        Mat4 proj = viewer->camera->view_to_clip();
        Mat4 vm = viewer->camera->world_to_view();
        glm::mat4 view = viewer->camera->Mat4ToGLM(vm);

        glm::mat4 projection = viewer->camera->Mat4ToGLM(proj);
        Mat4 pvm = viewer->camera->GlmToMAT4(projection * view * modelMat);

        /* Matrices uniform buffer */
        glBindBuffer(GL_UNIFORM_BUFFER, uboMatices);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projection));
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));
        glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(modelMat));
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        /* Setting shaders */
        shader->Use();
        shader->SetFloat("params.kappa", viewer->kappa);
        shader->SetFloat("params.sigma", viewer->sigma);
        shader->SetVec3("params.vp", cameraPos);
        shader->SetVec3("params.freezeVp", freezeVp);
        shader->SetBool("params.isFreezeFrame", viewer->isFreezeFrame);
        shader->SetBool("params.isAdaptive", isAdaptive);
        shader->SetInt("maxLevel", maxLevel);

        /* Screen shot */
        if (viewer->imgui->isSavePic){
            string screen_shot_name = "./pic/" + to_string(frameCount) + ".tga";
            SaveScreenshotToFile(screen_shot_name, viewer->width, viewer->height);
            viewer->imgui->isSavePic = false;
        }

        /* Select visible cubes */
        if (viewer->isFreezeFrame){
            renderStack = freezeRenderStack;
        }
        else{
            SelectCubeVisbility(multiResModel.lods, maxLevel, modelMat, view, projection);
        }

        /* Store the stack for the freeze frame */
        if (!viewer->isFreezeFrame){
            freezeRenderStack = renderStack;
            freezeVp = glm::vec3(viewer->camera->position.x, viewer->camera->position.y, viewer->camera->position.z);
        }

        /* Render the current scene */
        while (!renderStack.empty()){
            int curLevel = renderStack.top().first;
            uint64_t curCubeIdx = renderStack.top().second;
            size_t indexOffset = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].idxOffset * sizeof(uint32_t);
            size_t vertexOffset = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].vertexOffset * VERTEX_STRIDE;
            size_t uvOffset = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].uvOffset * UV_STRIDE;

            /* Get the parent offset */
            size_t parentOffset = 0;
            
            if (curLevel != 0){
                uint parentCoord[3];
                parentCoord[0] = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].coord[0] / 2;
                parentCoord[1] = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].coord[1] / 2;
                parentCoord[2] = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].coord[2] / 2;
                uint64_t ijk_p = (uint64_t)(parentCoord[0]) | ((uint64_t)(parentCoord[1]) << 16) | ((uint64_t)(parentCoord[2]) << 32);
                parentOffset = multiResModel.lods[maxLevel - curLevel + 1]->cubeTable[ijk_p].vertexOffset * VERTEX_STRIDE;
            }
            else{
                parentOffset = vertexOffset;
            }

            shader->Use();

            glBindVertexArray(vao);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pos);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, nml);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, uv);
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, clr);

            shader->SetInt("parentBase", parentOffset / (3 * sizeof(float)));
            shader->SetInt("level", multiResModel.lods[maxLevel - curLevel]->level);
            shader->SetInt("cubeI", multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].coord[0]);
            shader->SetInt("cubeJ", multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].coord[1]);
            shader->SetInt("cubek", multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].coord[2]);
            
            glDrawElementsBaseVertex(GL_TRIANGLES,
                                     multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].triangleCount * 3,
                                     GL_UNSIGNED_INT,
                                     (void *)(indexOffset),
                                     vertexOffset / (3 * sizeof(float)));

            glBindVertexArray(0);

            /* Calculate the number of traingles */
            renderedTriSum += multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].triangleCount * 3;

            /* BBX render*/
            if (isBbxDisplay){
                bbxShader->Use();
                bbxDrawer->Render(multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx], bbxShader, multiResModel.lods[maxLevel - curLevel]->cubeLength, multiResModel.lods[maxLevel - curLevel]->level);
            }

            renderedCubeCount++;
            renderStack.pop();
        }

        glfwSwapInterval(viewer->imgui->VSync);

        viewer->imgui->renderCubeCount = renderedCubeCount;
        size_t renderedTriSum_tri = renderedTriSum / 3;

        viewer->imgui->ImguiDraw(renderedTriSum_tri);
        isBbxDisplay = viewer->imgui->isBBXVis;
        isEdgeDisplay = viewer->imgui->isWireframe;
        isLodColorized = viewer->imgui->isLODColor;
        isCubeColorized = viewer->imgui->isCubeColor;
        isAdaptive = viewer->imgui->isAdaptiveLOD;
        viewer->kappa = viewer->imgui->kappa;
        viewer->sigma = viewer->imgui->sigma;

        shader->SetBool("isSmoothShading", viewer->imgui->isSoomthShading);
        shader->SetBool("isLodColorized", isLodColorized);
        shader->SetBool("isCubeColorized", isCubeColorized);

        /* Imgui rendering */
        viewer->imgui->ImguiRender();

        glfwSwapBuffers(viewer->window);
        glfwPollEvents();
    }

    viewer->imgui->ImguiClean();

    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &pos);
    glDeleteBuffers(1, &idx);
    glDeleteBuffers(1, &remap);
    glDeleteBuffers(1, &nml);
    glDeleteBuffers(1, &uv);
    glDeleteBuffers(1, &clr);

    glfwTerminate();

    return 0;
}
