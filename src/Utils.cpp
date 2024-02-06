#include "Utils.h"

ModelAttributesStatus modelAttriSatus = {false, false, false, false};

/* Compute the normal for the model */
float* ComputeNormal(float* vertices, uint32_t* indices, size_t vertex_count, size_t index_count){
    float* normals = (float*)malloc(vertex_count * 3 * sizeof(float));

    /* position map */
    uint32_t *remap = (uint32_t*)malloc(vertex_count * sizeof(uint32_t));
    float* unique_vertices = (float*)malloc(vertex_count * sizeof(float) * 3);
    size_t unique_vertex_count = meshopt_generateVertexRemap(remap, NULL, vertex_count, vertices, vertex_count, 3 * sizeof(float));

    /* initialize the normals */
    for(size_t i = 0; i < 3 * vertex_count; ++i){
        normals[i] = 0.0;
    }

    for(size_t i = 0; i < index_count; i = i + 3){
        Vec3 v1, v2, v3, n;
        v1.x = vertices[3 * indices[i]],     v1.y = vertices[3 * indices[i] + 1],     v1.z = vertices[3 * indices[i] + 2];
        v2.x = vertices[3 * indices[i + 1]], v2.y = vertices[3 * indices[i + 1] + 1], v2.z = vertices[3 * indices[i + 1] + 2];
        v3.x = vertices[3 * indices[i + 2]], v3.y = vertices[3 * indices[i + 2] + 1], v3.z = vertices[3 * indices[i + 2] + 2];
        
        n = normalized(cross(v2 - v1, v3 - v1));

        //accumulate
        normals[3 * remap[indices[i]]] +=  n.x,     normals[3 * remap[indices[i]] + 1] += n.y,     normals[3 * remap[indices[i]] + 2] += n.z;
        normals[3 * remap[indices[i + 1]]] +=  n.x, normals[3 * remap[indices[i + 1]] + 1] += n.y, normals[3 * remap[indices[i + 1]] + 2] += n.z;
        normals[3 * remap[indices[i + 2]]] +=  n.x, normals[3 * remap[indices[i + 2]] + 1] += n.y, normals[3 * remap[indices[i + 2]] + 2] += n.z;
    }

    for (size_t i = 0; i < vertex_count; ++i)
	{
        Vec3 n;
        if(remap[i] == i)
		{
            n.x = normals[3 * i], n.y = normals[3 * i + 1], n.z = normals[3 * i + 2];
        }
        else{
            n.x = normals[3 * remap[i]], n.y = normals[3 * remap[i] + 1], n.z = normals[3 * remap[i] + 2];
        }

        n = normalized(n);
        normals[3 * i] = n.x, normals[3 * i + 1] = n.y, normals[3 * i + 2] = n.z;
	}

    MemoryFree(remap);
    MemoryFree(unique_vertices);

    return normals;
}

void GetMaxMin(Vec3 v, float min[3], float max[3])
{
    if (v.x < min[0])
        min[0] = v.x;
    if (v.x > max[0])
        max[0] = v.x;

    if (v.y < min[1])
        min[1] = v.y;
    if (v.y > max[1])
        max[1] = v.y;

    if (v.z < min[2])
        min[2] = v.z;
    if (v.z > max[2])
        max[2] = v.z;
}

void GetMaxMin(float x, float y, float z, float min[3], float max[3])
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