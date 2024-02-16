#include "Display.h"
#include "./math/vec4.h"
#include "./math/transform.h"

Vec3 freezeVp;
Viewer *viewer = new Viewer;                              /* Initialize the viewer */
std::stack<std::pair<int, uint64_t>> freezeRenderStack;   /*freeze stack*/
stack<pair<int, uint64_t>> renderStack;                   /* Render stack */
size_t renderedTriSum = 0;
GLuint pos, nml, clr, remap, uv, idx;       

void SaveScreenshotToFile(std::string filename, int windowWidth, int windowHeight){
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

float CalculateDistanceToCube(float cubeBottom[3], Vec3 viewpoint, Mat4& model, float cubeLength){
    float maxDis = 0.0f;
    Vec3 bottom = transform(model, Vec3{cubeBottom[0], cubeBottom[1], cubeBottom[2]});
    Vec3 length = transform(model, Vec3{cubeLength, cubeLength, cubeLength});

    float disX = 0.0f;
    if (viewpoint.x < bottom.x) disX = abs(bottom.x - viewpoint.x);
    else if (viewpoint.x > bottom.x + length[0]) disX = abs(viewpoint.x - bottom.x - length[0]);
    else disX = 0.0f;

    float disY = 0.0f;
    if (viewpoint.y < bottom.y) disY = abs(bottom.y - viewpoint.y);
    else if (viewpoint.y > bottom.y + length[1]) disY = abs(viewpoint.y - bottom.y - length[1]);
    else disY = 0.0f;

    float disZ = 0.0f;
    if (viewpoint.z < bottom.z) disZ = abs(bottom.z - viewpoint.z);
    else if (viewpoint.z > bottom.z + length[2]) disZ = abs(viewpoint.z - bottom.z - length[2]);
    else disZ = 0.0f;

    maxDis = max(max(disX, disY), disZ);
    return maxDis;
}

int LoadChildCube(int parentCoord[3], LOD *meshbook[], Mat4& pvmMat, Mat4& model, Camera* camera, int maxLevel, int curLevel){
    if (curLevel < 0) return -1;
    LOD *mg = meshbook[curLevel];
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
                    
                float dis = CalculateDistanceToCube(mg->cubeTable[coord].bottom, camera->position, model, mg->cubeLength); 

                if (dis >= (viewer->imgui->kappa * pow(2, -(mg->level))) || mg->level == maxLevel){
                    if (AfterFrustumCulling(mg->cubeTable[coord], pvmMat)){
                        renderStack.push(make_pair(mg->level, coord));
                    }
                }
                else{
                    LoadChildCube(xyz, meshbook, pvmMat, model, camera, maxLevel, curLevel - 1);
                }

            }
        }
    }
    return 0;
}

bool AfterFrustumCulling(Cube &cube, Mat4 pvm){
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

void SelectCubeVisbility(LOD *meshbook[], int maxLevel, Mat4& pvmMat, Mat4& model){
    for (auto &c : meshbook[maxLevel]->cubeTable){
        float dis = CalculateDistanceToCube(c.second.bottom, viewer->camera->position, model, meshbook[maxLevel]->cubeLength); 
        if (dis >= (viewer->imgui->kappa * pow(2, -meshbook[maxLevel]->level)) || meshbook[maxLevel]->level == maxLevel){
            if (AfterFrustumCulling(c.second, pvmMat)){
                renderStack.push(make_pair(meshbook[maxLevel]->level, c.first));
            }
        }
        else{
            LoadChildCube(c.second.coord, meshbook, pvmMat, model, viewer->camera, maxLevel, maxLevel - 1);
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

void BindVAOBuffer(GLuint &vao){
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

int Display(HLOD &multiResModel, int maxLevel){
    Shader *shader = new Shader();
    Shader *bbxShader = new Shader();
    BoundingBoxDraw* bbxDrawer = new BoundingBoxDraw();
    string vs_shader = "./shaders/shader_default.vs";
    string fs_shader = "./shaders/shader_light.fs";
    float maxModelSize = multiResModel.lods[maxLevel]->cubeLength;
    GLuint vao;                   
    GLuint uboMatices;            
    int frameCount = 0;           
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
    glBufferData(GL_UNIFORM_BUFFER, 3 * sizeof(Mat4), NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboMatices, 0, 3 * sizeof(Mat4));

    /* Calculate model center */
    viewer->scale = 1.0f / maxModelSize;
    Mat4 model = Mat4::Indentity;
    model = viewer->scale * model;

    Vec3 center = Vec3{(multiResModel.max[0] + multiResModel.min[0]) * 0.5f,
                                 (multiResModel.max[1] + multiResModel.min[1]) * 0.5f,
                                 (multiResModel.max[2] + multiResModel.min[2]) * 0.5f};
    
    Vec3 cameraTarget = transform(model, center);
    
    viewer->camera->set_position(cameraTarget + Vec3(0.0f, 0.0f, 3.0f * viewer->scale * maxModelSize));
    viewer->camera->set_near(0.0001f * viewer->scale * maxModelSize);
    viewer->camera->set_far(100.0f * viewer->scale * maxModelSize);
    viewer->target = cameraTarget;

    /* BBXDrawer initialization */
    bbxDrawer->InitBuffer(multiResModel.lods[maxLevel]->cubeTable[0], multiResModel.lods[maxLevel]->cubeLength);

    /* Render loop */
    while (!glfwWindowShouldClose(viewer->window)){
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

        /* Time recording for the mouse operation */
        float currentFrame = glfwGetTime();
        viewer->deltaTime = currentFrame - viewer->lastFrame;
        viewer->lastFrame = currentFrame;

        viewer->ProcessInput(viewer->window);

        /* Transformation matrix */
        Mat4 projection = viewer->camera->view_to_clip();
        Mat4 view = viewer->camera->world_to_view();
        Mat4 pvm = projection * view * model;
        /* Matrices uniform buffer */
        glBindBuffer(GL_UNIFORM_BUFFER, uboMatices);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(Mat4), &(projection.cols[0]));
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(Mat4), sizeof(Mat4), &(view.cols[0]));
        glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(Mat4), sizeof(Mat4), &(model.cols[0]));
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        /* Setting shaders */
        shader->Use();
        shader->SetFloat("params.kappa", viewer->imgui->kappa);
        shader->SetFloat("params.sigma", viewer->imgui->sigma);
        shader->SetVec3("params.vp", viewer->camera->position);
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
            SelectCubeVisbility(multiResModel.lods, maxLevel, pvm, model);
        }

        /* Store the stack for the freeze frame */
        if (!viewer->isFreezeFrame){
            freezeRenderStack = renderStack;
            freezeVp = viewer->camera->position;
        }

        /* Render the current scene */
        while (!renderStack.empty()){
            int curLevel = renderStack.top().first;
            uint64_t curCubeIdx = renderStack.top().second;
            size_t indexOffset = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].idxOffset * sizeof(uint32_t);
            size_t vertexOffset = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].vertexOffset * VERTEX_STRIDE;

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
            shader->SetInt("coordX", multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].coord[0]);
            shader->SetInt("coordY", multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].coord[1]);
            shader->SetInt("coordZ", multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].coord[2]);
            
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
