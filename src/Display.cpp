#include "Display.h"
#include "./math/vec4.h"


clock_t start_t, end_t;       /* Time record */
Viewer *viewer = new Viewer;                /* Initialize the viewer */

glm::vec3 freezeVp;
std::stack<std::pair<int, uint64_t>> render_stack_freeze;   /*freeze stack*/

stack<pair<int, uint64_t>> render_stack;                    /* Render stack */

size_t sum = 0;
float gpu = 0;
float gpu_level_0 = 0;

int fps = 0;                       /* number of triangles display in every frame*/
float gpu_use = 0;

GLuint pos, nml, clr, idx_p, uv;   /* Vertex Buffer declaration */
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

void SetShader(Shader shader, bool text_exist)
{
    /*default color*/
    //glm::vec3 color_default = glm::vec3(0.467, 0.533, 0.600);
    glm::vec3 color_default = glm::vec3(00.99, 0.76, 0.0);

    shader.Use();
    shader.SetVec3("Color_default", color_default);
    shader.SetBool("texture_exist", text_exist);
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

float CaculateGpuSize(Cube &cell, bool isTexture)
{
    float gpuUsage = 0.0f;
    size_t vertGpuUsage = sizeof(float) * 3 * cell.mesh.posCount;
    size_t remapGpuUsage = sizeof(uint32_t) * cell.mesh.posCount;
    size_t idxGpuUsage = sizeof(uint32_t) * cell.mesh.idxCount;
    size_t nrlGpuUsage = sizeof(float) * 3 * cell.mesh.posCount;

    gpuUsage = float(float(vertGpuUsage + remapGpuUsage + idxGpuUsage + nrlGpuUsage) / float(8.0f * 1024.0f * 1024.0f));

    if (isTexture)
    {
        size_t textureGpuUsage = sizeof(float) * 2.0f * cell.mesh.uvCount;
        gpuUsage += float(float(textureGpuUsage) / float(8.0f * 1024.0f * 1024.0f));
    }

    return gpuUsage;
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

/* Init overdraw counter buffer*/
void OverdrawBufferInitial(GLuint &overdrawImage, GLuint &clearOverdrawBuffer)
{
    glm::vec4 *data;

    glGenTextures(1, &overdrawImage);
    glBindTexture(GL_TEXTURE_2D, overdrawImage);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R32UI, SC_MAX_FRAMEBUFFER_WIDTH, SC_MAX_FRAMEBUFFER_HEIGHT, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);

    glGenBuffers(1, &clearOverdrawBuffer);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, clearOverdrawBuffer);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, SC_MAX_FRAMEBUFFER_WIDTH * SC_MAX_FRAMEBUFFER_HEIGHT * sizeof(GLuint), NULL, GL_STREAM_DRAW);

    data = (glm::vec4 *)glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
    memset(data, 0x00, SC_MAX_FRAMEBUFFER_WIDTH * SC_MAX_FRAMEBUFFER_HEIGHT * sizeof(GLuint));
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
}

/* Quad image buffer initialization */
void QuadBufferInit(GLuint &IBO, GLuint &quad_render_vbo, GLuint &quad_render_vao, GLuint &quad_tex_buffer)
{
    GLfloat quad_vertexData[] = {
        1.0f, -1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
        -1.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f};

    GLfloat quad_texData[] = {
        1.0f, 0.0f,
        1.0f, 1.0f,
        0.0f, 0.0f,
        0.0f, 1.0f};

    static const GLushort quad_vertex_indices[] = {0, 1, 2, 2, 3, 1};

    glGenVertexArrays(1, &quad_render_vao);
    glBindVertexArray(quad_render_vao);

    glGenBuffers(1, &IBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quad_vertex_indices), quad_vertex_indices, GL_STATIC_DRAW);

    glGenBuffers(1, &quad_render_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, quad_render_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertexData), quad_vertexData, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

    glGenBuffers(1, &quad_tex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, quad_tex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_texData), quad_texData, GL_STATIC_DRAW);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

/* Object buffers initialization */
void ObjectBufferInit(mesh_offset_cell &mof, bool isQuantized, LOD *meshbook[], int level, bool single_tex, bool multi_tex, bool colorExist)
{
    /* Each buffer offset */
    size_t vertex_offest_r = 0;
    size_t parent_offset_r = 0;
    size_t nml_offset_r = 0;
    size_t uv_offset_r = 0;
    size_t index_offset_r = 0;
    size_t remap_offset_r = 0;
    size_t color_offset_r = 0;
    
    /* Position */
    glGenBuffers(1, &pos);
    if (isQuantized)
    {
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, pos);
        glBufferData(GL_SHADER_STORAGE_BUFFER, mof.vertex_offset_pack + mof.vertex_offset_pack / (3 * sizeof(unsigned short)) * sizeof(uint32_t), NULL, GL_STATIC_DRAW);
        for (int i = 0; i <= level; ++i)
        {
            for (auto &c : meshbook[i]->cubeTable)
            {
                c.second.vertexOffset = vertex_offest_r;
                /* Merge the position data and place hold data */
                unsigned short *pos_q = (unsigned short *)malloc(sizeof(unsigned short) * 4 * c.second.mesh.posCount);
                for (int i = 0; i < c.second.mesh.posCount; ++i)
                {
                    memcpy(&pos_q[4 * i], &c.second.pack.positions[3 * i], 3 * sizeof(unsigned short));
                    pos_q[4 * i + 3] = 1;
                }
                glBufferSubData(GL_SHADER_STORAGE_BUFFER, vertex_offest_r, 4 * sizeof(unsigned short) * c.second.mesh.posCount, &(pos_q[0]));
                vertex_offest_r += 4 * sizeof(unsigned short) * c.second.mesh.posCount;
                free(pos_q);
            }
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    else
    {
        glBindBuffer(GL_ARRAY_BUFFER, pos);
        glBufferData(GL_ARRAY_BUFFER, mof.vertex_offset, NULL, GL_STATIC_DRAW);
        for (int i = 0; i <= level; ++i)
        {
            for (auto &c : meshbook[i]->cubeTable)
            {
                c.second.vertexOffset = vertex_offest_r;
                glBufferSubData(GL_ARRAY_BUFFER, vertex_offest_r, 3 * sizeof(float) * c.second.mesh.posCount, &(c.second.mesh.positions[0]));
                vertex_offest_r += 3 * sizeof(float) * c.second.mesh.posCount;
            }
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    /* Normal */
    glGenBuffers(1, &nml);
    if (isQuantized)
    {
        glBindBuffer(GL_ARRAY_BUFFER, nml);
        glBufferData(GL_ARRAY_BUFFER, mof.nml_offset_pack, NULL, GL_STATIC_DRAW);
        for (int i = 0; i <= level; ++i)
        {
            for (auto &c : meshbook[i]->cubeTable)
            {
                c.second.normalOffset = nml_offset_r;
                glBufferSubData(GL_ARRAY_BUFFER, c.second.normalOffset, sizeof(int32_t) * c.second.mesh.posCount, &(c.second.pack.normals[0]));
                nml_offset_r += sizeof(int32_t) * c.second.mesh.posCount;
            }
        }
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
    }
    else
    {
        glBindBuffer(GL_ARRAY_BUFFER, nml);
        glBufferData(GL_ARRAY_BUFFER, mof.nml_offset, NULL, GL_STATIC_DRAW);
        for (int i = 0; i <= level; ++i)
        {
            for (auto &c : meshbook[i]->cubeTable)
            {
                c.second.normalOffset = nml_offset_r;
                glBufferSubData(GL_ARRAY_BUFFER, c.second.normalOffset, 3 * sizeof(float) * c.second.mesh.posCount, &(c.second.mesh.normals[0]));
                nml_offset_r += 3 * sizeof(float) * c.second.mesh.posCount;
            }
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    /* Color */
    if (colorExist)
    {
        glGenBuffers(1, &clr);
        glBindBuffer(GL_ARRAY_BUFFER, clr);
        glBufferData(GL_ARRAY_BUFFER, mof.color_offset, NULL, GL_STATIC_DRAW);
        for (int i = 0; i <= level; ++i)
        {
            for (auto &c : meshbook[i]->cubeTable)
            {
                c.second.colorOffset = color_offset_r;
                glBufferSubData(GL_ARRAY_BUFFER, c.second.colorOffset, 3 * sizeof(u_char) * c.second.mesh.posCount, &(c.second.mesh.colors[0]));
                color_offset_r += 3 * sizeof(u_char) * c.second.mesh.posCount;
            }
        }
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }

    /* Index */
    glGenBuffers(1, &idx);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idx);
    size_t idx_offset_buffer = 0;
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mof.index_offset, NULL, GL_STATIC_DRAW);
    for (int i = 0; i <= level; ++i)
    {
        for (auto &c : meshbook[i]->cubeTable)
        {
            c.second.idxOffset = index_offset_r;
            glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, index_offset_r, sizeof(uint32_t) * c.second.mesh.idxCount, &(c.second.mesh.indices[0]));
            index_offset_r += sizeof(uint32_t) * c.second.mesh.idxCount;
        }
    }
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    /* Parent-children dependency */
    glGenBuffers(1, &idx_p);
    glBindBuffer(GL_ARRAY_BUFFER, idx_p);
    size_t idx_p_offset_buffer = 0;
    
    if (isQuantized)
        glBufferData(GL_ARRAY_BUFFER, mof.vertex_offset_pack / (3 * sizeof(unsigned short)) * sizeof(uint32_t), NULL, GL_STATIC_DRAW);
    else
        glBufferData(GL_ARRAY_BUFFER, mof.vertex_offset / (3 * sizeof(float)) * sizeof(uint32_t), NULL, GL_STATIC_DRAW);
    for (int i = 0; i <= level; ++i)
    {
        for (auto &c : meshbook[i]->cubeTable)
        {
            c.second.remapOffset = remap_offset_r;
            glBufferSubData(GL_ARRAY_BUFFER, c.second.remapOffset, sizeof(uint32_t) * c.second.mesh.posCount, &(c.second.mesh.remap[0]));
            remap_offset_r += c.second.mesh.posCount * sizeof(uint32_t);
        }
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &uv);
    if ((single_tex || multi_tex))
    {
        if (isQuantized)
        {
            glBindBuffer(GL_ARRAY_BUFFER, uv);
            glBufferData(GL_ARRAY_BUFFER, mof.uv_offset_pack, NULL, GL_STATIC_DRAW);
            for (int i = 0; i <= level; ++i)
            {
                for (auto &c : meshbook[i]->cubeTable)
                {
                    c.second.uvOffset = uv_offset_r;
                    glBufferSubData(GL_ARRAY_BUFFER, c.second.uvOffset, 2 * sizeof(unsigned short) * c.second.mesh.uvCount, &(c.second.pack.uvs[0]));
                    uv_offset_r += 2 * sizeof(unsigned short) * c.second.mesh.uvCount;
                }
            }
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }
        else
        {
            glBindBuffer(GL_ARRAY_BUFFER, uv);
            glBufferData(GL_ARRAY_BUFFER, mof.uv_offset, NULL, GL_STATIC_DRAW);
            for (int i = 0; i <= level; ++i)
            {
                for (auto &c : meshbook[i]->cubeTable)
                {
                    c.second.uvOffset = uv_offset_r;
                    glBufferSubData(GL_ARRAY_BUFFER, c.second.uvOffset, 2 * sizeof(float) * c.second.mesh.uvCount, &(c.second.mesh.uvs[0]));
                    uv_offset_r += 2 * sizeof(float) * c.second.mesh.uvCount;
                }
            }
            glBindBuffer(GL_ARRAY_BUFFER, 0);
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
    glGenBuffers(1, &idx_p);
    glBindBuffer(GL_ARRAY_BUFFER, idx_p);
    glBufferData(GL_ARRAY_BUFFER, data.posCount * sizeof(uint32_t), &(data.remap[0]), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    /* Index */
    glGenBuffers(1, &idx);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, idx);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, data.idxCount * sizeof(uint32_t), &(data.indices[0]), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    
    glGenBuffers(1, &uv);
    glGenBuffers(1, &clr);
    // cout<<"buffer init test: "<<data.posCount <<" "<< data.idxCount<<" "<<data.normals[0]<<" "<<data.indices[10]<<" "
    // <<data.remap[10]<<endl;
}

/* Bind the vbo buffer with vao (original data) and vao (isQuantized data) */
void VAObuffer(GLuint &vaoQuant, GLuint &vao, bool isQuantized)
{
    if (isQuantized)
    {
        glGenVertexArrays(1, &vaoQuant);
        glBindVertexArray(vaoQuant);

        glBindBuffer(GL_ARRAY_BUFFER, nml);
        glVertexAttribIPointer(1, 1, GL_INT, 1 * sizeof(GL_INT),
                               (void *)0);
        glEnableVertexAttribArray(1);

        glBindBuffer(GL_ARRAY_BUFFER, idx_p);
        glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, 1 * sizeof(GL_UNSIGNED_INT),
                               (void *)0);
        glEnableVertexAttribArray(2);

        glBindBuffer(GL_ARRAY_BUFFER, uv);
        glVertexAttribIPointer(3, 2, GL_UNSIGNED_SHORT, 2 * sizeof(unsigned short),
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
    else
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

        glBindBuffer(GL_ARRAY_BUFFER, idx_p);
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
}

void TextureFileLoad(TextureFile &texfile, bool single_tex, bool multi_tex, vector<string> textures_path)
{
    if (single_tex)
    {
        cout << "begin to load texture" << endl;
        if (textures_path[0].substr(textures_path[0].length() - 3, textures_path[0].length()) == "jpg" ||
            textures_path[0].substr(textures_path[0].length() - 3, textures_path[0].length()) == "png")
        {
            texfile.load(textures_path[0].c_str());
        }
        else
            texfile.loadDDS(textures_path[0].c_str(), texfile.texture);
        cout << "load single texture sucessfully" << endl;
    }
    if (modelAttriSatus.hasMultiTexture)
    {
        // texfile.loadMultipleTexture(textures_path.size(), textures_path);
        // texfile.load(textures_path[1].c_str());
        if (textures_path[0].substr(textures_path[0].length() - 3, textures_path[0].length()) == "jpg" ||
            textures_path[0].substr(textures_path[0].length() - 3, textures_path[0].length()) == "png")
        {
            texfile.load(textures_path[0].c_str(), texture1);
            texfile.load(textures_path[1].c_str(), texture2);
            texfile.load(textures_path[2].c_str(), texture3);
            texfile.load(textures_path[3].c_str(), texture4);
            texfile.load(textures_path[4].c_str(), texture5);
            texfile.load(textures_path[5].c_str(), texture6);
            texfile.load(textures_path[6].c_str(), texture7);
            texfile.load(textures_path[7].c_str(), texture8);
            texfile.load(textures_path[8].c_str(), texture9);
            texfile.load(textures_path[9].c_str(), texture10);
        }
        else
        {
            texfile.loadDDS(textures_path[0].c_str(), texture1);
            texfile.loadDDS(textures_path[1].c_str(), texture2);
            texfile.loadDDS(textures_path[2].c_str(), texture3);
            texfile.loadDDS(textures_path[3].c_str(), texture4);
            texfile.loadDDS(textures_path[4].c_str(), texture5);
            texfile.loadDDS(textures_path[5].c_str(), texture6);
            texfile.loadDDS(textures_path[6].c_str(), texture7);
            texfile.loadDDS(textures_path[7].c_str(), texture8);
            texfile.loadDDS(textures_path[8].c_str(), texture9);
            texfile.loadDDS(textures_path[9].c_str(), texture10);
        }

        cout << "load multiple texture sucessfully" << endl;
    }
}

void BindTextureBuffer(bool single_tex, bool multi_tex, TextureFile texfile, Shader shaderSSBO)
{
    if (single_tex)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texfile.texture);
        shaderSSBO.SetInt("texture1", 0);
    }
    if (multi_tex)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture1);
        shaderSSBO.SetInt("texture1", 0);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture2);
        shaderSSBO.SetInt("texture2", 1);

        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, texture3);
        shaderSSBO.SetInt("texture3", 2);

        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, texture4);
        shaderSSBO.SetInt("texture4", 3);

        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, texture5);
        shaderSSBO.SetInt("texture5", 4);

        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, texture6);
        shaderSSBO.SetInt("texture6", 5);

        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_2D, texture7);
        shaderSSBO.SetInt("texture7", 6);

        glActiveTexture(GL_TEXTURE7);
        glBindTexture(GL_TEXTURE_2D, texture8);
        shaderSSBO.SetInt("texture8", 7);

        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, texture9);
        shaderSSBO.SetInt("texture9", 8);

        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, texture10);
        shaderSSBO.SetInt("texture10", 9);
    }
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

    bool normal_vis = false;
    bool bbx_display = false;
    bool edge_display = false;
    bool lod_colorized = false;
    bool cube_colorized = false;
    bool isAdaptive = true;
    bool isOverdraw = false;
    
    /* NDC to screen space */
    glm::mat4 matNDCToScreen = {viewer->SC_SCR_WIDTH * 0.5, 0.0f, 0.0f, viewer->SC_SCR_WIDTH * 0.5,
                                0.0f, viewer->SC_SCR_WIDTH * 0.5, 0.0f, viewer->SC_SCR_HEIGHT,
                                0.0f, 0.0f, 1.0f, 0.0f,
                                0.0f, 0.0f, 0.0f, 1.0f};

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

    /* Overdraw buffer declaration */
    GLuint overdrawImage;
    GLuint clearOverdrawBuffer;
    
    /* Quad information */
    GLuint IBO;
    GLuint quad_render_vbo;
    GLuint quad_render_vao;
    GLuint quad_tex_buffer;
    
    TimerStart();
    //ObjectBufferInit(mof, isQuantized, multiResModel.lods, level, modelAttriSatus.hasSingleTexture, modelAttriSatus.hasMultiTexture, modelAttriSatus.hasColor);
    ObjectBufferInit(multiResModel.data);
    TimerStop("Loading data to GPU: ");

    /*set shader for different functions*/
    string vs_shader, fs_shader, overdrawfs_shader, gs_shader;
    Shader shaderSSBO;
    Shader overdrawShader;
    Shader NormalShader;
    overdrawfs_shader = "./shaders/overdrawvis.fs";
    //fs_shader = "./shaders/wireframe_visualization.fs";
    fs_shader = "./shaders/shader_light.fs";
    gs_shader = "./shaders/wireframe_visualization.gs";

    if (isQuantized)
    {
        if (modelAttriSatus.hasSingleTexture || modelAttriSatus.hasMultiTexture)
        {
            vs_shader = "./shaders/shader_default_quant_tex.vs";
            if (modelAttriSatus.hasMultiTexture)
                fs_shader = "./shaders/shader_light_multiTex.fs";
        }
        else if (modelAttriSatus.hasColor)
            vs_shader = "./shaders/shader_default_quant_tex.vs";
        else
            vs_shader = "./shaders/shader_default_quant.vs";

        /*normal shader*/
        //NormalShader.build("./shaders/normals_quant.vs", "./shaders/normals_terrain_color.fs");
    }
    else
    {
        if (modelAttriSatus.hasSingleTexture || modelAttriSatus.hasMultiTexture)
        {
            vs_shader = "./shaders/shader_default_tex.vs";
            if (modelAttriSatus.hasMultiTexture)
                fs_shader = "./shaders/shader_light_multiTex.fs";
        }
        else if (modelAttriSatus.hasColor)
            vs_shader = "./shaders/shader_default_clr.vs";
        else
            //vs_shader = "./shaders/wireframe_visualization.vs";
            vs_shader = "./shaders/shader_default.vs";
        /* Normal shader */
        //NormalShader.Build("./shaders/normals.vs", "./shaders/normals.fs");
    }

    /* Sahder Setting */
    //shaderSSBO.Build(vs_shader.c_str(), fs_shader.c_str(), gs_shader.c_str());
    shaderSSBO.Build(vs_shader.c_str(), fs_shader.c_str());
    overdrawShader.Build(vs_shader.c_str(), overdrawfs_shader.c_str());

    Shader bbxShader;
    bbxShader.Build("./shaders/shader_bbx.vs", "./shaders/shader_bbx.fs");

    Shader overDrawBlit;
    overDrawBlit.Build("./shaders/overdrawvisblit.vs", "./shaders/overdrawvisblit.fs");

    unsigned int uniformBlockIndexVertex = glGetUniformBlockIndex(shaderSSBO.ID, "Matrices");
    unsigned int uniformBlockIndexBBX = glGetUniformBlockIndex(bbxShader.ID, "Matrices");
    unsigned int uniformBlockIndexNormal = glGetUniformBlockIndex(NormalShader.ID, "Matrices");
    unsigned int uniformBlockOverDraw = glGetUniformBlockIndex(overdrawShader.ID, "Matrices");

    /* Uniform */
    glUniformBlockBinding(shaderSSBO.ID, uniformBlockIndexVertex, 0);
    glUniformBlockBinding(bbxShader.ID, uniformBlockIndexBBX, 0);
    glUniformBlockBinding(NormalShader.ID, uniformBlockIndexNormal, 0);
    glUniformBlockBinding(overdrawShader.ID, uniformBlockOverDraw, 0);

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
    VAObuffer(vaoQuant, vao, isQuantized);

    /* Load the texture file */
    TextureFile texfile;
    texfile.max_level = maxLevel;
    TextureFileLoad(texfile, modelAttriSatus.hasSingleTexture, modelAttriSatus.hasMultiTexture, textures_path);

    /* FPS */
    double lastTime = glfwGetTime();
    int nbFrames = 0;

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
    bool initial_overdraw = true;
    while (!glfwWindowShouldClose(viewer->window))
    {
        /* Gpu using rate per frame */
        gpu_use = gpu;
        gpu = 0;

        frameCount++;
        int renderedCubeCount = 0;

        /* Overdraw initialization */
        if (isOverdraw && initial_overdraw)
        {
            cout << "begin to load the overdraw info" << endl;
            OverdrawBufferInitial(overdrawImage, clearOverdrawBuffer);
            QuadBufferInit(IBO, quad_render_vbo, quad_render_vao, quad_tex_buffer);
            cout << "end loading the overdraw buffer info" << endl;
            initial_overdraw = false;
        }


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
        shaderSSBO.Use();
        shaderSSBO.SetFloat("params.kappa", viewer->kappa);
        shaderSSBO.SetFloat("params.sigma", viewer->sigma);
        shaderSSBO.SetBool("params.step", max_size / (1 << maxLevel));
        shaderSSBO.SetVec3("params.vp", camera_pos);
        shaderSSBO.SetVec3("params.freezeVp", freezeVp);
        shaderSSBO.SetBool("params.isFreezeFrame", viewer->isFreezeFrame);
        shaderSSBO.SetBool("params.isAdaptive", isAdaptive);
        shaderSSBO.SetMat4("ViewportMatrix", matNDCToScreen);
        shaderSSBO.SetInt("maxLevel", maxLevel);

        if (viewer->imgui->isSavePic)
        {
            string screen_shot_name = "./pic/" + to_string(frameCount) + ".tga";
            SaveScreenshotToFile(screen_shot_name, viewer->width, viewer->height);
            viewer->imgui->isSavePic = false;
        }

        /* Select visible cubes */
        if (viewer->isFreezeFrame)
        {
            render_stack = render_stack_freeze;
        }
        else
        {
            SelectCellVisbility(multiResModel.lods, maxLevel, model, view, projection);
        }

        /* Store the stack for the freeze frame */
        if (!viewer->isFreezeFrame)
        {
            render_stack_freeze = render_stack;
            freezeVp = glm::vec3(viewer->camera->position.x, viewer->camera->position.y, viewer->camera->position.z);
        }

        if (isOverdraw)
        {
            overdrawShader.Use();
            glBindBuffer(GL_PIXEL_UNPACK_BUFFER, clearOverdrawBuffer);
            glBindTexture(GL_TEXTURE_2D, overdrawImage);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, SC_MAX_FRAMEBUFFER_WIDTH, SC_MAX_FRAMEBUFFER_HEIGHT, GL_RED_INTEGER, GL_UNSIGNED_INT, NULL);
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        /* Render the current scene */
        while (!render_stack.empty())
        {
            int curLevel = render_stack.top().first;
            
            uint64_t curCubeIdx = render_stack.top().second;
            size_t indexOffset = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].idxOffset * sizeof(uint32_t);
            size_t vertexOffset = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].vertexOffset * VERTEX_STRIDE;
            size_t uvOffset = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].uvOffset * UV_STRIDE;

            /* Get the parent offset */
            size_t parentOffset = 0;

            /* Dequantization */
            glm::vec2 decode_uv, decode_uv_min, decode_uv_p, decode_uv_min_p;
            glm::vec3 decode_pos, decode_posMin, decode_pos_p, decode_posMin_p;
            decode_uv = glm::vec2(multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].multiplier_t[0],
                                    multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].multiplier_t[1]);
            decode_uv_min = glm::vec2(multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].uvMin[0],
                                        multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].uvMin[1]);
            decode_pos = glm::vec3(multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].multiplier_p[0],
                                    multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].multiplier_p[1],
                                    multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].multiplier_p[2]);
            decode_posMin = glm::vec3(multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].posMin[0],
                                        multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].posMin[1],
                                        multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].posMin[2]);
            
            if (curLevel != 0){
                uint ijk_parent[3];
                ijk_parent[0] = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].ijk[0] / 2;
                ijk_parent[1] = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].ijk[1] / 2;
                ijk_parent[2] = multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].ijk[2] / 2;
                uint64_t ijk_p = (uint64_t)(ijk_parent[0]) | ((uint64_t)(ijk_parent[1]) << 16) | ((uint64_t)(ijk_parent[2]) << 32);
                parentOffset = multiResModel.lods[maxLevel - curLevel + 1]->cubeTable[ijk_p].vertexOffset * VERTEX_STRIDE;

                decode_uv_p = glm::vec2(multiResModel.lods[maxLevel - curLevel + 1]->cubeTable[ijk_p].multiplier_t[0],
                                        multiResModel.lods[maxLevel - curLevel + 1]->cubeTable[ijk_p].multiplier_t[1]);
                decode_uv_min_p = glm::vec2(multiResModel.lods[maxLevel - curLevel + 1]->cubeTable[ijk_p].uvMin[0],
                                            multiResModel.lods[maxLevel - curLevel + 1]->cubeTable[ijk_p].uvMin[1]);

                decode_pos_p = glm::vec3(multiResModel.lods[maxLevel - curLevel + 1]->cubeTable[ijk_p].multiplier_p[0],
                                            multiResModel.lods[maxLevel - curLevel + 1]->cubeTable[ijk_p].multiplier_p[1],
                                            multiResModel.lods[maxLevel - curLevel + 1]->cubeTable[ijk_p].multiplier_p[2]);
                decode_posMin_p = glm::vec3(multiResModel.lods[maxLevel - curLevel + 1]->cubeTable[ijk_p].posMin[0],
                                                multiResModel.lods[maxLevel - curLevel + 1]->cubeTable[ijk_p].posMin[1],
                                                multiResModel.lods[maxLevel - curLevel + 1]->cubeTable[ijk_p].posMin[2]);
            }
            else
            {
                parentOffset = vertexOffset;
                
                /* De-quantization */
                decode_pos_p = decode_pos;
                decode_posMin_p = decode_posMin;
                decode_uv_p = decode_uv;
                decode_uv_min_p = decode_uv_min;
            }

            shaderSSBO.Use();

            if (isQuantized)
            {
                glBindVertexArray(vaoQuant);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pos);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, nml);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, uv);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, clr);

                shaderSSBO.SetInt("base_p", parentOffset / (4 * sizeof(unsigned short)));
                /*dequantization position*/
                shaderSSBO.SetVec3("decode_pos", decode_pos);
                shaderSSBO.SetVec3("decode_posMin", decode_posMin);
                shaderSSBO.SetVec3("decode_pos_p", decode_pos_p);
                shaderSSBO.SetVec3("decode_posMin_p", decode_posMin_p);
                /*dequantization texture coordinates*/
                shaderSSBO.SetVec2("decode_uv", decode_uv);
                shaderSSBO.SetVec2("decode_uv_min", decode_uv_min);
                shaderSSBO.SetVec2("decode_uv_p", decode_uv_p);
                shaderSSBO.SetVec2("decode_uv_min_p", decode_uv_min_p);
            }
            else
            {
                glBindVertexArray(vao);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pos);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, nml);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, uv);
                glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, clr);

                shaderSSBO.SetInt("base_p", parentOffset / (3 * sizeof(float)));
            }

            SetShader(shaderSSBO, (modelAttriSatus.hasSingleTexture || modelAttriSatus.hasMultiTexture));
            shaderSSBO.SetInt("level", multiResModel.lods[maxLevel - curLevel]->level);
            shaderSSBO.SetInt("cube_i", multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].ijk[0]);
            shaderSSBO.SetInt("cube_j", multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].ijk[1]);
            shaderSSBO.SetInt("cube_k", multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].ijk[2]);
    
            //cout<<"check render stack: "<<multiResModel.lods[level - curLevel]->level<<endl;
            /*bind texture image*/
            if (modelAttriSatus.hasSingleTexture || modelAttriSatus.hasMultiTexture)
                BindTextureBuffer(modelAttriSatus.hasSingleTexture, modelAttriSatus.hasMultiTexture, texfile, shaderSSBO);

            if (!isOverdraw)
            {
                if (edge_display){
                    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                    glEnable(GL_POLYGON_OFFSET_FILL);
                    glPolygonOffset(2.0f, 2.0f); 
                    glLineWidth(2.0f);

                    if (isQuantized)
                        glDrawElementsBaseVertex(GL_TRIANGLES,
                                                    multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].GetIndexCount(),
                                                    GL_UNSIGNED_INT,
                                                    (void *)(indexOffset),
                                                    vertexOffset / (4 * sizeof(unsigned short)));
                    else
                        glDrawElementsBaseVertex(GL_TRIANGLES,
                                                    multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].GetIndexCount(),
                                                    GL_UNSIGNED_INT,
                                                    (void *)(indexOffset),
                                                    vertexOffset / (3 * sizeof(float)));
                    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

                    
                }
                else
                {
                    if (isQuantized)
                        glDrawElementsBaseVertex(GL_TRIANGLES,
                                                    multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].GetIndexCount(),
                                                    GL_UNSIGNED_INT,
                                                    (void *)(indexOffset),
                                                    vertexOffset / (4 * sizeof(unsigned short)));
                    else
                        glDrawElementsBaseVertex(GL_TRIANGLES,
                                                    multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].GetIndexCount(),
                                                    GL_UNSIGNED_INT,
                                                    (void *)(indexOffset),
                                                    vertexOffset / (3 * sizeof(float)));
                }
            }

            if (normal_vis)
            {
                if (isQuantized)
                {
                    NormalShader.Use();
                    glBindVertexArray(vaoQuant);
                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pos);
                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, idx);
                    NormalShader.SetVec3("camera_pos", camera_pos);
                    NormalShader.SetInt("base_index", indexOffset / sizeof(uint32_t));
                    NormalShader.SetInt("base", vertexOffset / (4 * sizeof(unsigned short)));
                    NormalShader.SetMat4("projection", projection);
                    // glDrawArrays(GL_TRIANGLES, 0, multiResModel.lods[level - curLevel]->cubeTable[curCubeIdx].GetIndexCount()/3);
                    // glDrawArrays(GL_LINES, 0, 2 * multiResModel.lods[level - curLevel]->cubeTable[curCubeIdx].GetIndexCount()/3);
                    glDrawElementsBaseVertex(GL_TRIANGLES,
                                                multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].GetIndexCount(),
                                                GL_UNSIGNED_INT,
                                                (void *)(indexOffset),
                                                vertexOffset / (4 * sizeof(unsigned short)));
                }
                else
                {
                    NormalShader.Use();
                    glBindVertexArray(vao);
                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pos);
                    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, idx);
                    NormalShader.SetVec3("camera_pos", camera_pos);
                    NormalShader.SetInt("base_index", indexOffset / sizeof(uint32_t));
                    NormalShader.SetInt("base", vertexOffset / (3 * sizeof(float)));
                    NormalShader.SetMat4("projection", projection);
                    // glDrawArrays(GL_LINES, 0, 2 * multiResModel.lods[level - curLevel]->cubeTable[curCubeIdx].GetIndexCount()/3);
                    glDrawElementsBaseVertex(GL_TRIANGLES,
                                                multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].GetIndexCount(),
                                                GL_UNSIGNED_INT,
                                                (void *)(indexOffset),
                                                vertexOffset / (3 * sizeof(float)));
                }
            }

                glBindVertexArray(0);

                /*calculate the gpu memory*/
                gpu += CaculateGpuSize(multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx], (modelAttriSatus.hasSingleTexture || modelAttriSatus.hasMultiTexture));

                /*calculate the number of traingles*/
                sum += multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].GetIndexCount();

                /*bbx render*/
                if (bbx_display)
                {
                    // bbxDrawer.BBXSetShader(bbxShader, projection, view, model);
                    bbxShader.Use();
                    bbxDrawer.Render(multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx], bbxShader, multiResModel.lods[maxLevel - curLevel]->cubeLength, multiResModel.lods[maxLevel - curLevel]->level);
                }

                if (isOverdraw)
                {
                    overdrawShader.Use();
                    overdrawShader.SetFloat("params.kappa", viewer->kappa);
                    overdrawShader.SetFloat("params.sigma", viewer->sigma);
                    overdrawShader.SetVec3("params.vp", camera_pos);
                    overdrawShader.SetVec3("params.freezeVp", freezeVp);
                    overdrawShader.SetBool("params.isFreezeFrame", viewer->isFreezeFrame);
                    overdrawShader.SetBool("params.isAdaptive", isAdaptive);

                    SetShader(overdrawShader, (modelAttriSatus.hasSingleTexture || modelAttriSatus.hasMultiTexture));
                    overdrawShader.SetInt("level", multiResModel.lods[maxLevel - curLevel]->level);

                    if (isQuantized)
                    {
                        glBindVertexArray(vaoQuant);
                        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pos);
                        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, nml);
                        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, uv);
                        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, clr);

                        overdrawShader.SetInt("base_p", parentOffset / (4 * sizeof(unsigned short)));

                        /*dequantization position*/
                        overdrawShader.SetVec3("decode_pos", decode_pos);
                        overdrawShader.SetVec3("decode_posMin", decode_posMin);
                        overdrawShader.SetVec3("decode_pos_p", decode_pos_p);
                        overdrawShader.SetVec3("decode_posMin_p", decode_posMin_p);
                        /*dequantization texture coordinates*/
                        overdrawShader.SetVec2("decode_uv", decode_uv);
                        overdrawShader.SetVec2("decode_uv_min", decode_uv_min);
                        overdrawShader.SetVec2("decode_uv_p", decode_uv_p);
                        overdrawShader.SetVec2("decode_uv_min_p", decode_uv_min_p);
                    }
                    else
                    {
                        glBindVertexArray(vao);
                        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, pos);
                        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, nml);
                        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, uv);
                        glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 4, clr);

                        overdrawShader.SetInt("base_p", parentOffset / (3 * sizeof(float)));
                    }

                    /*overdraw count Texture*/
                    glBindImageTexture(0, overdrawImage, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);

                    if (isQuantized)
                        glDrawElementsBaseVertex(GL_TRIANGLES,
                                                multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].GetIndexCount(),
                                                GL_UNSIGNED_INT,
                                                (void *)(indexOffset),
                                                vertexOffset / (4 * sizeof(unsigned short)));
                    else
                        glDrawElementsBaseVertex(GL_TRIANGLES,
                                                multiResModel.lods[maxLevel - curLevel]->cubeTable[curCubeIdx].GetIndexCount(),
                                                GL_UNSIGNED_INT,
                                                (void *)(indexOffset),
                                                vertexOffset / (3 * sizeof(float)));

                    glBindVertexArray(0);
                }

            renderedCubeCount++;
            render_stack.pop();
        }

        if (isOverdraw)
        {
            /*draw overdraw count image*/
            overDrawBlit.Use();
            glBindVertexArray(quad_render_vao);
            glBindTexture(GL_TEXTURE_2D, overdrawImage);
            glBindImageTexture(0, overdrawImage, 0, GL_FALSE, 0, GL_READ_WRITE, GL_R32UI);
            glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
            glBindVertexArray(0);

            /*compute the overdraw ratio*/
            glBindTexture(GL_TEXTURE_2D, overdrawImage);
            uint32_t *pixel = (uint32_t *)malloc(SC_MAX_FRAMEBUFFER_WIDTH * SC_MAX_FRAMEBUFFER_HEIGHT * sizeof(uint32_t));
            glGetTextureImage(overdrawImage, 0, GL_RED_INTEGER, GL_UNSIGNED_INT, SC_MAX_FRAMEBUFFER_WIDTH * SC_MAX_FRAMEBUFFER_HEIGHT * sizeof(uint32_t), pixel);
            size_t draw_sum = 0;
            size_t overdraw_sum = 0;
            for (int i = 0; i < SC_MAX_FRAMEBUFFER_WIDTH * SC_MAX_FRAMEBUFFER_HEIGHT; ++i)
            {
                if (pixel[i] > 0)
                    draw_sum++;
                if (pixel[i] > 1)
                    overdraw_sum += pixel[i];
            }

            viewer->imgui->overdrawRatio = float(overdraw_sum) / float(draw_sum);
        }

        glfwSwapInterval(viewer->imgui->VSync);

        viewer->imgui->renderCubeCount = renderedCubeCount;
        size_t sum_tri = sum / 3;

        viewer->imgui->ImguiDraw(sum_tri);
        bbx_display = viewer->imgui->isNormalVis;
        edge_display = viewer->imgui->isWireframe;
        lod_colorized = viewer->imgui->isLODColor;
        cube_colorized = viewer->imgui->isCubeColor;
        isAdaptive = viewer->imgui->isAdaptiveLOD;
        viewer->kappa = viewer->imgui->kappa;
        viewer->sigma = viewer->imgui->sigma;
        normal_vis = viewer->imgui->isNormalVis;
        isOverdraw = viewer->imgui->isOverdrawVis;

        shaderSSBO.SetBool("smooth_shadering", viewer->imgui->isSoomthShading);
        shaderSSBO.SetBool("lod_colorized", lod_colorized);
        shaderSSBO.SetBool("cube_colorized", cube_colorized);
        shaderSSBO.SetBool("colorExist", modelAttriSatus.hasColor);

        /*imgui rendering*/
        viewer->imgui->ImguiRender();

        sum = 0;

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
    glDeleteBuffers(1, &idx_p);
    glDeleteBuffers(1, &nml);
    glDeleteBuffers(1, &uv);
    glDeleteBuffers(1, &clr);

    glDeleteBuffers(1, &overdrawImage);
    glDeleteBuffers(1, &clearOverdrawBuffer);

    // glfw: terminate
    glfwTerminate();

    return 0;
}
