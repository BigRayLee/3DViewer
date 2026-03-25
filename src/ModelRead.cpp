#include "miniply/miniply.h"
#include "ModelRead.h"
#include "mesh_simplify/meshoptimizer_mod.h"

#define FAST_OBJ_IMPLEMENTATION 1
#include "fast_obj/fast_obj.h"
#undef FAST_OBJ_IMPLEMENTATION

ModelReader::ModelReader()
{
    meshData = new Mesh;
}

ModelReader::~ModelReader()
{
    if (meshData->positions)
        MemoryFree(meshData->positions);
    if (meshData->indices)
        MemoryFree(meshData->indices);
    if (modelAttriSatus.hasNormal && meshData->normals)
        MemoryFree(meshData->normals);
    if (modelAttriSatus.hasColor && meshData->colors)
        MemoryFree(meshData->colors);

    if ((modelAttriSatus.hasSingleTexture || modelAttriSatus.hasMultiTexture) && meshData->uvs && meshData->indicesUV)
    {
        MemoryFree(meshData->uvs);
        MemoryFree(meshData->indicesUV);
    }
}

int ModelReader::InputModel(string fileName)
{
    if (fileName.substr(fileName.length() - 3, fileName.length()) == "ply")
    {
        cout << "Reading Ply file...";
        if (PlyParser(fileName.c_str()))
        {
            printf("Error reading PLY file.\n");
        }
    }
    else if (fileName.substr(fileName.length() - 3, fileName.length()) == "obj")
    {
        cout << "Reading Obj file...";
        ObjParser(fileName.c_str());
    }
    else if (fileName.substr(fileName.length() - 3, fileName.length()) == "txt")
    {
        cout << "Reading BBX file..."
             << " ";
        BbxParser(fileName.c_str());
    }
    else
    {
        cout << "Unsupported (yet) file type extension" << endl;
        return -1;
    }

    return 0;
}

void ModelReader::GetMaxMin(float x, float y, float z)
{
    if (x < min[0])
        min[0] = x;
    if (x > max[0])
        max[0] = x;

    if (y < min[1])
        min[1] = y;
    if (y > max[1])
        max[1] = y;

    if (z < min[2])
        min[2] = z;
    if (z > max[2])
        max[2] = z;
}

int ModelReader::PlyParser(const char *fileName)
{
    using namespace miniply;
    PLYReader reader(fileName);
    size_t index_count = 0;

    cout << "file name : " << fileName << endl;
    if (!reader.valid())
    {
        return -1;
    }

    /* TODO get the texture file path */
    /* Vertex element */
    if (!reader.element_is(kPLYVertexElement))
    {
        cout << "missing vertex elements" << endl;
        return -1;
    }
    else
    {
        uint32_t pos_idx[3];
        reader.load_element();
        reader.find_pos(pos_idx);
        size_t vertex_count = reader.num_rows();
        vertCount = vertex_count;
        cout << "vertex: " << vertCount << " ";

        /* Extract the positions */
        meshData->positions = (float *)malloc(vertex_count * 3 * sizeof(float));
        reader.extract_properties(pos_idx, 3, PLYPropertyType::Float, meshData->positions);

        /* Extract the normals */
        uint32_t nml_idx[3];
        if (reader.find_normal(nml_idx))
        {
            meshData->normals = (float *)malloc(vertex_count * 3 * sizeof(float));
            reader.extract_properties(nml_idx, 3, PLYPropertyType::Float, meshData->normals);
        }
        else
        {
            modelAttriSatus.hasNormal = false;
        }
    }

    /*face element*/
    reader.next_element();
    if (!reader.element_is(kPLYFaceElement))
    {
        cout << "missing face elements" << endl;
        return -1;
    }
    else
    {
        /* Indices */
        if (!reader.load_element())
        {
            cout << "can not read faces." << endl;
            return -1;
        }

        uint32_t list_num[2];
        reader.get_list_counts(list_num[0]);
        uint32_t idx[1];
        reader.load_element();
        reader.find_indices(idx);
        index_count = reader.num_rows() * 3;
        triCount = index_count / 3;
        cout << "index: " << triCount * 3 << " ";
        meshData->indices = (uint32_t *)malloc(index_count * sizeof(uint32_t));
        reader.extract_list_property(idx[0], PLYPropertyType::Int, meshData->indices);
    }

    return 0;
}

void ModelReader::CalculateNormals()
{
    cout << "Normal does not exist, compute normal..." << endl;
    meshData->normals = (float *)malloc(vertCount * VERTEX_STRIDE);
    meshData->normals = ComputeNormal(meshData->positions, meshData->indices, vertCount, triCount * 3);
    modelAttriSatus.hasNormal = true;
}

int ModelReader::ObjParser(const char *fileName)
{
    fastObjMesh *obj = fast_obj_read(fileName);

    if (obj == nullptr)
    {
        cout << "Error reading obj file." << endl;
        return -1;
    }
    
   for (int k = 0; k < obj->material_count; ++k)
    {
        const char* path = obj->materials[k].map_Kd.path;
        if (path && path[0] != '\0')
            texturesPath.push_back(path);
    }

    if (texturesPath.size() == 1){
        modelAttriSatus.hasSingleTexture = true;
    }
    else if (texturesPath.size() > 1){
        modelAttriSatus.hasMultiTexture = true;
    }
       
    size_t vertex_count = 0;
    size_t index_count = 0;
    size_t texture_count = 0;

    for (int i = 0; i < obj->face_count; ++i)
    {
        index_count += 3 * (obj->face_vertices[i] - 2);
    }
        
    meshData->positions = (float *)malloc(3 * index_count * sizeof(float));
    meshData->normals   = (float *)malloc(3 * index_count * sizeof(float));
    meshData->uvs       = (float *)malloc(2 * index_count * sizeof(float));

    size_t write_offset = 0;
    for (int k = 0; k < obj->group_count; ++k)
    {
        size_t index_offset = obj->groups[k].index_offset; 
        size_t vertex_offset = 0;

        for (size_t i = 0; i < obj->groups[k].face_count; ++i)
        {
            unsigned int fv = obj->face_vertices[obj->groups[k].face_offset + i]; 

           for (size_t j = 1; j + 1 < fv; ++j)
           {
                fastObjIndex gi0 = obj->indices[index_offset];         
                fastObjIndex gi1 = obj->indices[index_offset + j];     
                fastObjIndex gi2 = obj->indices[index_offset + j + 1]; 

                auto writeVertex = [&](fastObjIndex gi, int k) {
                    memcpy(&meshData->positions[3 * write_offset],
                        &obj->positions[gi.p * 3], 3 * sizeof(float));

                    if (gi.n)
                        memcpy(&meshData->normals[3 * write_offset],
                            &obj->normals[gi.n * 3], 3 * sizeof(float));
                    else
                        modelAttriSatus.hasNormal = false;

                    if (gi.t) {
                        memcpy(&meshData->uvs[2 * write_offset],
                            &obj->texcoords[gi.t * 2], 2 * sizeof(float));
                        meshData->uvs[2 * write_offset + 1] += k;
                    }

                    write_offset++;
                };

                writeVertex(gi0, k);
                writeVertex(gi1, k);
                writeVertex(gi2, k);
            }
            index_offset += fv;
        }
    }

    uint32_t *remap = (uint32_t *)malloc(index_count * sizeof(uint32_t));
    meshData->indices = (u_int32_t *)malloc(index_count * sizeof(uint32_t));

    vertex_count = meshopt_generateVertexRemap(remap, NULL, index_count, meshData->positions, index_count, sizeof(float) * 3);
    meshopt_remapIndexBuffer(meshData->indices, NULL, index_count, remap);
    meshopt_remapVertexBuffer(meshData->positions, meshData->positions, index_count, sizeof(float) * 3, remap);

    /* Realloc the buffer */
    meshData->positions = (float *)realloc(meshData->positions, 3 * vertex_count * sizeof(float));

    if (modelAttriSatus.hasNormal)
    {
        meshopt_remapVertexBuffer(meshData->normals, meshData->normals, index_count, sizeof(float) * 3, remap);
        meshData->normals = (float *)realloc(meshData->normals, 3 * vertex_count * sizeof(float));
    }

    if (modelAttriSatus.hasSingleTexture)
    {
        uint32_t *remap_tex = (uint32_t *)malloc(index_count * sizeof(uint32_t));
        meshData->indicesUV = (uint32_t *)malloc(index_count * sizeof(uint32_t));

        texture_count = meshopt_generateVertexRemap(remap_tex, NULL, index_count, meshData->uvs, index_count, sizeof(float) * 2);
        meshopt_remapIndexBuffer(meshData->indicesUV, NULL, index_count, remap_tex);
        meshopt_remapVertexBuffer(meshData->uvs, meshData->uvs, index_count, sizeof(float) * 2, remap_tex);

        /* Realloc the size*/
        meshData->uvs = (float *)realloc(meshData->uvs, 2 * texture_count * sizeof(float));

        MemoryFree(remap_tex);
    }
    else
    {
        uint32_t *remap_tex = (uint32_t *)malloc(index_count * sizeof(uint32_t));
        meshData->indicesUV = (uint32_t *)malloc(index_count * sizeof(uint32_t));

        texture_count = meshopt_generateVertexRemap(remap_tex, NULL, index_count, meshData->uvs, index_count, sizeof(float) * 2);
        meshopt_remapIndexBuffer(meshData->indicesUV, NULL, index_count, remap_tex);
        meshopt_remapVertexBuffer(meshData->uvs, meshData->uvs, index_count, sizeof(float) * 2, remap_tex);

        /* Realloc the size */
        meshData->uvs = (float *)realloc(meshData->uvs, 2 * texture_count * sizeof(float));

        MemoryFree(remap_tex);
    }

    cout << "vertex: " << vertex_count << " ";
    cout << "index: " << index_count << " ";

    triCount = obj->face_count;
    vertCount = vertex_count;

    MemoryFree(remap);

    fast_obj_destroy(obj);

    return 0;
}

int ModelReader::BbxParser(const char *fileName)
{
    cout << fileName << endl;
    ifstream file;
    file.open(fileName, ios::in | ios::binary);

    string cubeCoord;
    getline(file, cubeCoord, '\n');
    size_t vertCount = 0, idxCount = 0;

    while (file.peek() != EOF)
    {
        string vertCountStr;
        getline(file, vertCountStr, '\n');
        vertCount = std::stoi(vertCountStr);
        string triCountStr;
        getline(file, triCountStr, '\n');
        idxCount = std::stoi(triCountStr);

        meshData->positions = (float *)malloc(sizeof(float) * 3 * vertCount);
        meshData->indices = (uint32_t *)malloc(sizeof(uint32_t) * idxCount);

        cout << "reading test: " << cubeCoord << " " << vertCount << " " << idxCount << endl;
        file.read((char *)(meshData->positions), vertCount * 3 * sizeof(float));

        file.read((char *)(meshData->indices), idxCount * sizeof(uint32_t));

        if (file.peek() != EOF)
        {
            meshData->normals = (float *)malloc(sizeof(float) * 3 * vertCount);
            file.read((char *)(meshData->normals), vertCount * 3 * sizeof(float));
            modelAttriSatus.hasNormal = true;
        }
        if (file.peek() != EOF)
        {
            meshData->remap = (uint32_t *)malloc(sizeof(uint32_t) * vertCount);
            file.read((char *)(meshData->remap), vertCount * sizeof(uint32_t));
        }
    }
    file.close();

    triCount = idxCount / 3;
    vertCount = vertCount;

    cout << "vertex: " << vertCount << " ";
    cout << "index: " << idxCount << " ";

    modelAttriSatus.hasNormal = false;

    return 0;
}
