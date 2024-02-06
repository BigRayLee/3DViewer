#include "Display.h"
#include "./math/vec4.h"


clock_t start_t, end_t;       /* Time record */
Viewer *viewer = new Viewer;                /* Initialize the viewer */

glm::vec3 freezeVp;
std::stack<std::pair<int, uint64_t>> render_stack_freeze;   /*freeze stack*/

stack<pair<int, uint64_t>> render_stack;                    /* Render stack */

size_t renderedTriSum = 0;
float gpu = 0;
float gpu_level_0 = 0;

int fps = 0;                       /* number of triangles display in every frame*/
float gpu_use = 0;

GLuint pos, nml, clr, remap, uv;   /* Vertex Buffer declaration */
GLuint idx;                        /* Element Buffer declaration */

unsigned int texture1, texture2, texture3, texture4, texture5, texture6, texture7, texture8, texture9, texture10; /* Multiple texture buffer declaration */

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


float ComputeMaxDistance(float cubeBottom[3], glm::vec3 viewpoint, glm::mat4 model, glm::mat4 view, float cubeLength)
{
    float maxDis = 0.0f;
    glm::vec4 bottom = model * glm::vec4(cubeBottom[0], cubeBottom[1], cubeBottom[2], 1.0f);
    glm::vec4 length = model * glm::vec4(cubeLength, cubeLength, cubeLength, 1.0f);

    // x
    float DisX = 0.0;
    if (viewpoint.x < bottom.x)
        DisX = abs(bottom.x - viewpoint.x);
    else if (viewpoint.x > bottom.x + length[0])
        DisX = abs(viewpoint.x - bottom.x - length[0]);
    else
        DisX = 0.0;

    // y
    float DisY = 0.0;
    if (viewpoint.y < bottom.y)
        DisY = abs(bottom.y - viewpoint.y);
    else if (viewpoint.y > bottom.y + length[1])
        DisY = abs(viewpoint.y - bottom.y - length[1]);
    else
        DisY = 0.0;

    // z
    float DisZ = 0.0;
    if (viewpoint.z < bottom.z)
        DisZ = abs(bottom.z - viewpoint.z);
    else if (viewpoint.z > bottom.z + length[2])
        DisZ = abs(viewpoint.z - bottom.z - length[2]);
    else
        DisZ = 0.0;

    maxDis = max(max(DisX, DisY), DisZ);
    return maxDis;
}

int LoadChildCell(int ijk_p[3], LOD *meshbook[],
                  glm::mat4 projection, glm::mat4 view, glm::mat4 model, Camera* camera, int level_max, int level_current)
{

    if (level_current < 0)
        return -1;
    LOD *mg = meshbook[level_current];
    Mat4 pvm = viewer->camera->GlmMat4_to(projection * view * model);

    /*load the child of cell*/
    int xyz[3];
    for (int ix = 0; ix < 2; ix++)
    {
        xyz[0] = ijk_p[0] * 2 + ix;
        for (int iy = 0; iy < 2; iy++)
        {
            xyz[1] = ijk_p[1] * 2 + iy;
            for (int iz = 0; iz < 2; iz++)
            {
                xyz[2] = ijk_p[2] * 2 + iz;
                uint64_t ijk = (uint64_t)(xyz[0]) | ((uint64_t)(xyz[1]) << 16) | ((uint64_t)(xyz[2]) << 32);
                if (mg->cubeTable.count(ijk) == 0)
                    continue;

                glm::vec3 camera_pos = glm::vec3(camera->position.x, camera->position.y, camera->position.z);
                float dis = ComputeMaxDistance(mg->cubeTable[ijk].bottom, camera_pos, model, view, mg->cubeLength); 

                if (dis >= (viewer->kappa * pow(2, -(mg->level))) || mg->level == level_max)
                {
                    if (AfterFrustumCulling(mg->cubeTable[ijk], pvm))
                    {
                        render_stack.push(make_pair(mg->level, ijk));
                    }
                }
                else
                {
                    LoadChildCell(xyz, meshbook, projection, view, model, camera, level_max, level_current - 1);
                }
            }
        }
    }
    return 0;
}

bool AfterFrustumCulling(Cube &cell, Mat4 pvm)
{
    Aabb bbox;
    bbox.min.x = cell.bottom[0];
    bbox.min.y = cell.bottom[1];
    bbox.min.z = cell.bottom[2];
    bbox.max.x = cell.top[0];
    bbox.max.y = cell.top[1];
    bbox.max.z = cell.top[2];

    int visible = is_visible(bbox, &pvm(0, 0));
    if (visible != 0)
        return true;
    else
        return false;
}


void SelectCellVisbility(LOD *meshbook[], int maxLevel, glm::mat4 model, glm::mat4 view, glm::mat4 projection)
{
    Mat4 pvm = viewer->camera->GlmMat4_to(projection * view * model);
    for (auto &c : meshbook[maxLevel]->cubeTable)
    {

        glm::vec3 camera_pos = glm::vec3(viewer->camera->position.x, viewer->camera->position.y, viewer->camera->position.z);
        float dis = ComputeMaxDistance(c.second.bottom, camera_pos, model, view, meshbook[maxLevel]->cubeLength); // ComputeMaxDistance(bottom_cam, camera.cameraPos, 1.0/meshbook[level-1]->x0[3]); meshbook[level-1]->length c.second.length

        if (dis >= (viewer->kappa * pow(2, -meshbook[maxLevel]->level)) || meshbook[maxLevel]->level == maxLevel)
        {
            if (AfterFrustumCulling(c.second, pvm))
            {
                render_stack.push(make_pair(meshbook[maxLevel]->level, c.first));
            }
        }
        else
        {
            LoadChildCell(c.second.ijk, meshbook, projection, view, model, viewer->camera, maxLevel, maxLevel - 1);
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

int Display(HLOD &multiResModel, int maxLevel, vector<string> textures_path, bool quant)
{
    /* isQuantized mesh mode */
    bool isQuantized = quant;
    
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

    bool isNormalVis = false;
    bool isBbxDisplay = false;
    bool isEdgeDisplay = false;
    bool isLodColorized = false;
    bool isCubeColorized = false;
    bool isAdaptive = true;

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

    TimerStart();
    ObjectBufferInit(multiResModel.data);
    TimerStop("Loading data to GPU: ");

    /*set shader for different functions*/
    string vs_shader, fs_shader;
    Shader *shader = new Shader();
    vs_shader = "./shaders/shader_default.vs";
    fs_shader = "./shaders/shader_light.fs";

    shader->Build(vs_shader.c_str(), fs_shader.c_str());

    Shader bbxShader;
    bbxShader.Build("./shaders/shader_bbx.vs", "./shaders/shader_bbx.fs");

    unsigned int uniformBlockIndexVertex = glGetUniformBlockIndex(shader->ID, "Matrices");
    unsigned int uniformBlockIndexBBX = glGetUniformBlockIndex(bbxShader.ID, "Matrices");

    /* Uniform */
    glUniformBlockBinding(shader->ID, uniformBlockIndexVertex, 0);
    glUniformBlockBinding(bbxShader.ID, uniformBlockIndexBBX, 0);

    /* Uniform matrices generation */
    GLuint uboMatices;
    glGenBuffers(1, &uboMatices);
    glBindBuffer(GL_UNIFORM_BUFFER, uboMatices);
    glBufferData(GL_UNIFORM_BUFFER, 3 * sizeof(glm::mat4), NULL, GL_STATIC_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferRange(GL_UNIFORM_BUFFER, 0, uboMatices, 0, 3 * sizeof(glm::mat4));

    /* VAO bind */
    GLuint vaoQuant;                    /* isQuantized vao */
    GLuint vao;                          /* default vao */
    BindVAOBuffer(vao);

    /* Camera setting */
    float max_size = multiResModel.lods[maxLevel]->cubeLength;
    
    /* Calculate model center */
    glm::mat4 model(1.0f);
    viewer->scale = 1.0f / max_size;
    model = glm::scale(model, glm::vec3(viewer->scale));

    glm::vec3 model_center = glm::vec3((multiResModel.lods[maxLevel]->max[0] + multiResModel.lods[maxLevel]->min[0]) / 2.0,
                                       (multiResModel.lods[maxLevel]->max[1] + multiResModel.lods[maxLevel]->min[1]) / 2.0,
                                       (multiResModel.lods[maxLevel]->max[2] + multiResModel.lods[maxLevel]->min[2]) / 2.0);

    glm::vec4 model_center_c = model * glm::vec4(model_center, 1.0);
    
    viewer->camera->set_position(Vec3(model_center_c.x, model_center_c.y, model_center_c.z) + Vec3(0.0, 0.0, 3 * viewer->scale * max_size));
    viewer->camera->set_near(0.0001 * viewer->scale * max_size);
    viewer->camera->set_far(100 * viewer->scale * max_size);
    viewer->target = Vec3(model_center_c.x, model_center_c.y, model_center_c.z);

    /* Progressive buffer */
    viewer->kappa = viewer->imgui->kappa;
    viewer->sigma = viewer->imgui->sigma;

    /* BBX draw */
    BoundingBoxDraw bbxDrawer;
    bbxDrawer.InitBuffer(multiResModel.lods[maxLevel]->cubeTable[0], multiResModel.lods[maxLevel]->cubeLength);

    /* Screen shoot counter */
    int frameCount = 0;
    
    /* Render loop */
    while (!glfwWindowShouldClose(viewer->window))
    {
        frameCount++;
        int renderedCubeCount = 0;

        glClearColor(viewer->imgui->color.x, viewer->imgui->color.y, viewer->imgui->color.z, viewer->imgui->color.w);
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        glfwSwapInterval(viewer->imgui->VSync);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glfwWindowHint(GLFW_SAMPLES, 4);

        /* Get camera_pos in every frame */
        glm::vec3 camera_pos = glm::vec3(viewer->camera->position.x, viewer->camera->position.y, viewer->camera->position.z);

        /* Time recording for the mouse operation */
        float currentFrame = glfwGetTime();
        viewer->deltaTime = currentFrame - viewer->lastFrame;
        viewer->lastFrame = currentFrame;

        viewer->ProcessInput(viewer->window);

        /* Transformation matrix */
        Mat4 proj = viewer->camera->view_to_clip();
        Mat4 vm = viewer->camera->world_to_view();
        glm::mat4 view = viewer->camera->Mat4_to(vm);

        glm::mat4 projection = viewer->camera->Mat4_to(proj);
        Mat4 pvm = viewer->camera->GlmMat4_to(projection * view * model);

        /* Matrices uniform buffer */
        glBindBuffer(GL_UNIFORM_BUFFER, uboMatices);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::mat4), glm::value_ptr(projection));
        glBufferSubData(GL_UNIFORM_BUFFER, sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(view));
        glBufferSubData(GL_UNIFORM_BUFFER, 2 * sizeof(glm::mat4), sizeof(glm::mat4), glm::value_ptr(model));
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        /* Setting shaders */
        shader->Use();
        shader->SetFloat("params.kappa", viewer->kappa);
        shader->SetFloat("params.sigma", viewer->sigma);
        shader->SetVec3("params.vp", camera_pos);
        shader->SetVec3("params.freezeVp", freezeVp);
        shader->SetBool("params.isFreezeFrame", viewer->isFreezeFrame);
        shader->SetBool("params.isAdaptive", isAdaptive);
        shader->SetInt("maxLevel", maxLevel);

        if (viewer->imgui->isSavePic)
        {
            string screen_shot_name = "./pic/" + to_string(frameCount) + ".tga";
            SaveScreenshotToFile(screen_shot_name, viewer->width, viewer->height);
            viewer->imgui->isSavePic = false;
        }

        /* Select visible cubes */
        if (viewer->isFreezeFrame){
            render_stack = render_stack_freeze;
        }
        else{
            SelectCellVisbility(multiResModel.lods, maxLevel, model, view, projection);
        }

        /* Store the stack for the freeze frame */
        if (!viewer->isFreezeFrame){
            render_stack_freeze = render_stack;
            freezeVp = glm::vec3(viewer->camera->position.x, viewer->camera->position.y, viewer->camera->position.z);
        }

        /* Render the current scene */
        while (!render_stack.empty()){
            int curLevel = render_stack.top().first;
            uint64_t curCubeIdx = render_stack.top().second;
            size_t indexOffset = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].idxOffset * sizeof(uint32_t);
            size_t vertexOffset = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].vertexOffset * VERTEX_STRIDE;
            size_t uvOffset = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].uvOffset * UV_STRIDE;

            /* Get the parent offset */
            size_t parentOffset = 0;
            
            if (curLevel != 0){
                uint ijk_parent[3];
                ijk_parent[0] = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].ijk[0] / 2;
                ijk_parent[1] = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].ijk[1] / 2;
                ijk_parent[2] = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].ijk[2] / 2;
                uint64_t ijk_p = (uint64_t)(ijk_parent[0]) | ((uint64_t)(ijk_parent[1]) << 16) | ((uint64_t)(ijk_parent[2]) << 32);
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
            shader->SetInt("cubeI", multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].ijk[0]);
            shader->SetInt("cubeJ", multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].ijk[1]);
            shader->SetInt("cubek", multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].ijk[2]);
            
            glDrawElementsBaseVertex(GL_TRIANGLES,
                                        multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].GetIndexCount(),
                                        GL_UNSIGNED_INT,
                                        (void *)(indexOffset),
                                        vertexOffset / (3 * sizeof(float)));

            glBindVertexArray(0);

            /*calculate the number of traingles*/
            renderedTriSum += multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].GetIndexCount();

            /*bbx render*/
            if (isBbxDisplay)
            {
                bbxShader.Use();
                bbxDrawer.Render(multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx], bbxShader, multiResModel.lods[maxLevel - curLevel]->cubeLength, multiResModel.lods[maxLevel - curLevel]->level);
            }


            renderedCubeCount++;
            render_stack.pop();
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

        /*imgui rendering*/
        viewer->imgui->ImguiRender();

        renderedTriSum = 0;

        /*glfw swap buffers and IO events*/
        glfwSwapBuffers(viewer->window);
        glfwPollEvents();
    }

    viewer->imgui->ImguiClean();

    // multiple buffer delete
    glDeleteVertexArrays(1, &vao);
    glDeleteVertexArrays(1, &vaoQuant);
    glDeleteBuffers(1, &pos);
    glDeleteBuffers(1, &idx);
    glDeleteBuffers(1, &remap);
    glDeleteBuffers(1, &nml);
    glDeleteBuffers(1, &uv);
    glDeleteBuffers(1, &clr);

    // glfw: terminate
    glfwTerminate();

    return 0;
}
