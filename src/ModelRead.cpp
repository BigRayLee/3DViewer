#include "miniply/miniply.h"
#include "ModelRead.h"
#include "mesh_simplify/meshoptimizer_mod.h"

#define FAST_OBJ_IMPLEMENTATION 1
#include "fast_obj/fast_obj.h"
#undef FAST_OBJ_IMPLEMENTATION

ModelReader::ModelReader() {
    meshData = new Mesh;
}

ModelReader::~ModelReader(){
    if(meshData->positions) MemoryFree(meshData->positions);
    if(meshData->indices) MemoryFree(meshData->indices);
    if(modelAttriSatus.hasNormal && meshData->normals) MemoryFree(meshData->normals);
    if(modelAttriSatus.hasColor && meshData->colors) MemoryFree(meshData->colors);

    if ((modelAttriSatus.hasSingleTexture || modelAttriSatus.hasMultiTexture) && meshData->uvs && meshData->indicesUV){
        MemoryFree(meshData->uvs);
        MemoryFree(meshData->indicesUV);
    }
}

int ModelReader::InputModel(string fileName){
    if (fileName.substr(fileName.length() - 3, fileName.length()) == "ply"){
        cout << "Reading Ply file...";
        if (PlyParser(fileName.c_str()))
        {
            printf("Error reading PLY file.\n");
        }
    }
    else if (fileName.substr(fileName.length() - 3, fileName.length()) == "obj"){
        cout << "Reading Obj file...";
        ObjParser(fileName.c_str());
    }
    else if (fileName.substr(fileName.length() - 3, fileName.length()) == "txt"){
        cout << "Reading BBX file..."
             << " ";
        BbxParser(fileName.c_str());
    }
    else{
        cout << "Unsupported (yet) file type extension" << endl;
        return -1;
    }

    /* Compute normal */
    if (!modelAttriSatus.hasNormal){
        cout<<"the normal does not exist, compute normal..."<<endl;
        meshData->normals = (float *)malloc(3 * vertCount * sizeof(float));
        meshData->normals = ComputeNormal(meshData->positions, meshData->indices, vertCount, triCount * 3);
        modelAttriSatus.hasNormal = true;
    }

    return 0;
}

void ModelReader::GetMaxMin(float x, float y, float z)
{
    if (x < min[0]) min[0] = x;
    if (x > max[0]) max[0] = x;

    if (y < min[1]) min[1] = y;
    if (y > max[1]) max[1] = y;

    if (z < min[2]) min[2] = z;
    if (z > max[2]) max[2] = z;
}

int ModelReader::PlyParser(const char *fileName)
{
    using namespace miniply;
    PLYReader reader(fileName);
    size_t index_count = 0;

    cout << "file name : " << fileName << endl;
    if (!reader.valid()){
        return -1;
    }

    /* TODO get the texture file path */
    /* Vertex element */
    if (!reader.element_is(kPLYVertexElement)){
        cout << "missing vertex elements" << endl;
        return -1;
    }
    else{
        uint32_t pos_idx[3];
        reader.load_element();
        reader.find_pos(pos_idx);
        size_t vertex_count = reader.num_rows();
        vertCount = vertex_count;
        cout << "vertex: " << vertex_count << " ";

        /* Extract the positions */
        meshData->positions = (float *)malloc(vertex_count * 3 * sizeof(float));
        reader.extract_properties(pos_idx, 3, PLYPropertyType::Float, meshData->positions);

        /* Extract the normals */
        uint32_t nml_idx[3];
        if (reader.find_normal(nml_idx)){
            meshData->normals = (float *)malloc(vertex_count * 3 * sizeof(float));
            reader.extract_properties(nml_idx, 3, PLYPropertyType::Float, meshData->normals);
        }
        else{
            modelAttriSatus.hasNormal = false;
        }

        // /*colors*/
        // uint32_t color_idx[3];
        // if(reader.find_color(color_idx)){
        //     cout<<"read color"<<endl;
        //     meshData->colors = (u_char*)malloc(vertex_count * 3 * sizeof(u_char));
        //     reader.extract_properties(color_idx, 3, PLYPropertyType::UChar, meshData->colors);
        //     colorExist = true;
        // }
    }

    /*face element*/
    reader.next_element();
    if (!reader.element_is(kPLYFaceElement)){
        cout << "missing face elements" << endl;
        return -1;
    }
    else{
        /* Indices */
        if (!reader.load_element()){
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
        cout << "index: " << index_count << " ";
        meshData->indices = (uint32_t *)malloc(index_count * sizeof(uint32_t));
        reader.extract_list_property(idx[0], PLYPropertyType::Int, meshData->indices);

        /* Texture coordinates */
        uint32_t uv_idx[1];
        if (reader.find_property("texcoord") == 1){
            modelAttriSatus.hasSingleTexture = true;
            uv_idx[0] = reader.find_property("texcoord");
            meshData->uvs = (float *)malloc(index_count * 2 * sizeof(float));
            reader.extract_list_property(uv_idx[0], PLYPropertyType::Float, meshData->uvs);
        }
        else{
            cout << "missing texcoords in face elements" << endl;
        }
    }

    /* Generate the new index buffer for the texture coordinates */
    if (modelAttriSatus.hasSingleTexture){
        uint32_t *remap_tex = (uint32_t *)malloc(index_count * sizeof(uint32_t));
        meshData->indicesUV = (uint32_t *)malloc(index_count * sizeof(uint32_t));

        size_t texture_count = meshopt_generateVertexRemap(remap_tex, NULL, index_count, meshData->uvs, index_count, sizeof(float) * 2);
        meshopt_remapIndexBuffer(meshData->indicesUV, NULL, index_count, remap_tex);
        meshopt_remapVertexBuffer(meshData->uvs, meshData->uvs, index_count, sizeof(float) * 2, remap_tex);

        /* Realloc the size */
        meshData->uvs = (float *)realloc(meshData->uvs, 2 * texture_count * sizeof(float));

        MemoryFree(remap_tex);
    }
    return 0;
}

int ModelReader::ObjParser(const char *fileName)
{
    fastObjMesh *obj = fast_obj_read(fileName);

    if (obj == nullptr){
        cout << "Error reading obj file." << endl;
        return -1;
    }

    if (obj->group_count == 1 && obj->materials->map_Kd.name){
        modelAttriSatus.hasSingleTexture = true;
        texturesPath.push_back(obj->materials->map_Kd.path);
    }
    else if (obj->group_count > 1){
        modelAttriSatus.hasMultiTexture = true;
        for (int k = 1; k < obj->material_count; ++k){
            texturesPath.push_back(obj->materials[k].map_Kd.path);
        }
    }
    else{
        modelAttriSatus.hasSingleTexture = false;
        modelAttriSatus.hasMultiTexture = false;
    }

    size_t vertex_count = 0;
    size_t index_count = 0;
    size_t texture_count = 0;

    for (int i = 0; i < obj->face_count; ++i){
        index_count += 3 * (obj->face_vertices[i] - 2);
    }

    meshData->positions = (float *)malloc(3 * index_count * sizeof(float));
    meshData->normals = (float *)malloc(3 * index_count * sizeof(float));
    meshData->uvs = (float *)malloc(2 * index_count * sizeof(float));

    /* Operate all the thing based on the group */
    for (int k = 0; k < obj->group_count; ++k){
        size_t index_offset = obj->groups[k].face_offset * 3;
        size_t vertex_offset = 0;

        float max_uv_x = 0.0, max_uv_y = 0.0;
        for (size_t i = 0; i < obj->groups[k].face_count; ++i){
            for (size_t j = 0; j < obj->face_vertices[i]; ++j){
                fastObjIndex gi = obj->indices[index_offset + j];

                /* if the j > 3, triangulate polygon on the fly; offset-3 is always the first polygon vertex */
                // if (j >= 3)
                // {
                //     // memcpy(&positions[3 * vertex_offset], &positions[3* (vertex_offset - 3)], 3 * sizeof(float));
                //     // memcpy(&positions[3 * (vertex_offset + 1)], &positions[3* (vertex_offset - 1)], 3 * sizeof(float));
                //     // vertex_offset += 2;
                // }

                /* Positions */
                memcpy(&meshData->positions[3 * (index_offset + j)], &obj->positions[gi.p * 3], 3 * sizeof(float));

                /* Normals */
                if (!gi.n){
                    modelAttriSatus.hasNormal = false;
                }
                else{
                    memcpy(&meshData->normals[3 * (index_offset + j)], &obj->normals[gi.n * 3], 3 * sizeof(float));
                }

                /* Textcoord */
                if (gi.t){
                    memcpy(&meshData->uvs[2 * (index_offset + j)], &obj->texcoords[gi.t * 2], 2 * sizeof(float));
                    if (max_uv_x < meshData->uvs[2 * (index_offset + j)]){
                        max_uv_x = meshData->uvs[2 * (index_offset + j)];
                    }
                        
                    if (max_uv_y < meshData->uvs[2 * (index_offset + j) + 1]){
                        max_uv_y = meshData->uvs[2 * (index_offset + j) + 1];
                    }
                        
                    meshData->uvs[2 * (index_offset + j) + 1] += k;
                }

                /* If the j > 3, triangulate polygon on the fly; offset-3 is always the first polygon vertex */
                if (j >= 3){
                    meshData->positions[vertex_offset + 0] = meshData->positions[vertex_offset - 3];
                    meshData->positions[vertex_offset + 1] = meshData->positions[vertex_offset - 1];
                    vertex_offset += 2;
                }

                vertex_offset++;
            }
            index_offset += obj->face_vertices[i];
        }
    }

    fast_obj_destroy(obj);

    uint32_t *remap = (uint32_t *)malloc(index_count * sizeof(uint32_t));
    meshData->indices = (u_int32_t *)malloc(index_count * sizeof(uint32_t));

    vertex_count = meshopt_generateVertexRemap(remap, NULL, index_count, meshData->positions, index_count, sizeof(float) * 3);
    meshopt_remapIndexBuffer(meshData->indices, NULL, index_count, remap);
    meshopt_remapVertexBuffer(meshData->positions, meshData->positions, index_count, sizeof(float) * 3, remap);

    /* Realloc the buffer */
    meshData->positions = (float *)realloc(meshData->positions, 3 * vertex_count * sizeof(float));

    if (modelAttriSatus.hasNormal){
        meshopt_remapVertexBuffer(meshData->normals, meshData->normals, index_count, sizeof(float) * 3, remap);
        meshData->normals = (float *)realloc(meshData->normals, 3 * vertex_count * sizeof(float));
    }

    if (modelAttriSatus.hasSingleTexture){
        uint32_t *remap_tex = (uint32_t *)malloc(index_count * sizeof(uint32_t));
        meshData->indicesUV = (uint32_t *)malloc(index_count * sizeof(uint32_t));

        texture_count = meshopt_generateVertexRemap(remap_tex, NULL, index_count, meshData->uvs, index_count, sizeof(float) * 2);
        meshopt_remapIndexBuffer(meshData->indicesUV, NULL, index_count, remap_tex);
        meshopt_remapVertexBuffer(meshData->uvs, meshData->uvs, index_count, sizeof(float) * 2, remap_tex);

        /*realloc the size*/
        meshData->uvs = (float *)realloc(meshData->uvs, 2 * texture_count * sizeof(float));

        MemoryFree(remap_tex);
    }
    else{
        uint32_t *remap_tex = (uint32_t *)malloc(index_count * sizeof(uint32_t));
        meshData->indicesUV = (uint32_t *)malloc(index_count * sizeof(uint32_t));

        texture_count = meshopt_generateVertexRemap(remap_tex, NULL, index_count, meshData->uvs, index_count, sizeof(float) * 2);
        meshopt_remapIndexBuffer(meshData->indicesUV, NULL, index_count, remap_tex);
        meshopt_remapVertexBuffer(meshData->uvs, meshData->uvs, index_count, sizeof(float) * 2, remap_tex);

        /*realloc the size*/
        meshData->uvs = (float *)realloc(meshData->uvs, 2 * texture_count * sizeof(float));

        MemoryFree(remap_tex);
    }

    cout << "vertex: " << vertex_count << " ";
    cout << "index: " << index_count << " ";

    triCount = obj->face_count;
    vertCount = vertex_count;

    MemoryFree(remap);

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

    while (file.peek() != EOF){
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
        
        if(file.peek() != EOF){
            meshData->normals = (float *)malloc(sizeof(float) * 3 * vertCount);
            file.read((char *)(meshData->normals), vertCount * 3 * sizeof(float));
            modelAttriSatus.hasNormal = true;
        }
        if(file.peek() != EOF){
            meshData->remap = (uint32_t*)malloc(sizeof(uint32_t) * vertCount);
            file.read((char*)(meshData->remap), vertCount * sizeof(uint32_t));
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

