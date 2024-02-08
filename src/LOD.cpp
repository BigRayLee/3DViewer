#include "LOD.h"

LOD::LOD(int l){
    level = l;
    lodSize = 1 << l;
}

size_t LOD::GetCubeCounts(){
    return cubeTable.size();
}

void LOD::SetLOD(float max[3], float min[3]){
    cubeLength = std::max((max[0] - min[0]), std::max((max[1] - min[1]), (max[2] - min[2]) ));
    cubeLength /= lodSize;
    step = 1.0f / cubeLength;
}

size_t LOD::CalculateTriangleCounts(){
    for (auto &cb : cubeTable){
        totalTriCount += cb.second.triangleCount;
    }
    return totalTriCount;
}

size_t LOD::CalculateVertexCounts(){
    for (auto &cb : cubeTable){
        totalVertCount += cb.second.vertCount;
    }
    return totalVertCount;
}

void Dispatch(int xyz[3], unordered_map<uint64_t,Cube >&table){
    uint64_t ijk = (uint64_t)(xyz[0]) | ((uint64_t)(xyz[1]) << 16) | ((uint64_t)(xyz[2]) << 32);
    if UNLIKELY(table.size() == 0){
        Cube cube;
        cube.ijk[0] = xyz[0];
        cube.ijk[1] = xyz[1];
        cube.ijk[2] = xyz[2];

        cube.ijk_64 = ijk;

        cube.triangleCount += 1;

        pair<uint64_t, Cube> p(ijk, cube);
        table.insert(p);
    }
    else{
        unordered_map<uint64_t, Cube>::const_iterator got = table.find(ijk);
        if (table.count(ijk) == 0){
            Cube cube;
            cube.ijk[0] = xyz[0];
            cube.ijk[1] = xyz[1];
            cube.ijk[2] = xyz[2];

            cube.triangleCount += 1;

            cube.ijk_64 = ijk;

            pair<uint64_t, Cube> p(ijk, cube);
            table.insert(p);
        }
        else{
            table[ijk].triangleCount += 1;
        }
    }
}

void HLOD::BuildLODFromInput(Mesh* rawMesh, size_t vertCount, size_t triCount){
    /* Get the max and min position value of the model */
    clock_t start, end;
    
    TimerStart();
    for (int i = 0; i < vertCount * 3; i = i + 3){
        GetMaxMin(rawMesh->positions[i], rawMesh->positions[i + 1], rawMesh->positions[i + 2], min, max);
    }
    TimerStop("Bounding box computing time: ");
    
    std::cout<< "Xmin: "<<min[0]<<" "<<"Ymin: "<<min[1]<<" "<<"Zmin: "<<min[2]<<endl;
    std::cout<< "Xmax: "<<max[0]<<" "<<"Ymax: "<<max[1]<<" "<<"Zmax: "<<max[2]<<endl;
    
    /* Set LOD information */
    lods[0]->SetLOD(max, min);
    
    start = clock();
    /* Dispatch the traingle */
    uint64_t *triangleToCube = (uint64_t *)malloc(sizeof(uint64_t) * triCount);
    for (size_t i = 0; i < triCount * 3; i += 3){
        /* Calculate the barycenter */
        float ct[3];
        Vec3 v1, v2, v3;
        v1.x = rawMesh->positions[3 * rawMesh->indices[i]],     v1.y = rawMesh->positions[3 * rawMesh->indices[i] + 1],     v1.z = rawMesh->positions[3 * rawMesh->indices[i] + 2];
        v2.x = rawMesh->positions[3 * rawMesh->indices[i + 1]], v2.y = rawMesh->positions[3 * rawMesh->indices[i + 1] + 1], v2.z = rawMesh->positions[3 * rawMesh->indices[i + 1] + 2];
        v3.x = rawMesh->positions[3 * rawMesh->indices[i + 2]], v3.y = rawMesh->positions[3 * rawMesh->indices[i + 2] + 1], v3.z = rawMesh->positions[3 * rawMesh->indices[i + 2] + 2];
        CalculateBarycenter(v1, v2, v3, ct);
        
        /* Assign the traingle to the ijk cube */
        int ijk[3];
        Float2Int(ct, ijk, min, lods[0]->step);

        /* Dispatch the triangle */
        Dispatch(ijk, lods[0]->cubeTable);

        uint64_t ijk_64 = (uint64_t)(ijk[0]) | ((uint64_t)(ijk[1]) << 16) | ((uint64_t)(ijk[2]) << 32);
        triangleToCube[i / 3] = ijk_64;
    }
    end = clock();
    std::cout << "dispatch time : " << double(end - start) / CLOCKS_PER_SEC << endl;

    /* Allocate vertex attributes memory space for each cube */
    size_t totalIndexCount = 0;
    for (auto &cube : lods[0]->cubeTable){
        cube.second.idxOffset = totalIndexCount;
        totalIndexCount += cube.second.triangleCount * 3; 
    }

    /* Allocate memory for indices */
    data.indices = (uint32_t*)malloc(totalIndexCount * sizeof(uint32_t));

    /* Reset triangle count to zero */
    for(auto &cube : lods[0]->cubeTable){
        cube.second.triangleCount = 0;
    }

    /* Fill the indices for each cube */
    for(size_t i = 0; i < triCount; ++i){
        Cube &cube = lods[0]->cubeTable[triangleToCube[i]];
        size_t idxOffset = cube.idxOffset + 3 * cube.triangleCount;
        uint32_t *dst = data.indices + idxOffset;
        memcpy(dst, &rawMesh->indices[3 * i], 3 * sizeof(uint32_t));
        cube.triangleCount ++;
    }

    /* Release memory */
    MemoryFree(triangleToCube);

    /* Allocate memory for vertices */
    size_t estimatedVertexCount = vertCount + vertCount / 4;
    data.positions = (float*)malloc(estimatedVertexCount * VERTEX_STRIDE);
    data.normals = (float*)malloc(estimatedVertexCount * VERTEX_STRIDE);
    data.remap = (uint32_t*)malloc(estimatedVertexCount * sizeof(uint32_t));
    
    /* Generate the unique vertices buffer and the compact index buffer for each cube */
    start = clock();
    size_t maxIdxCount = 0;
    for(auto &cb : lods[0]->cubeTable){
        maxIdxCount = maxIdxCount > 3 * cb.second.triangleCount ? maxIdxCount : 3 * cb.second.triangleCount;
    }
    
    /* Allocate the data based on the max index count and vertex count */
    uint32_t *remap = (uint32_t *)malloc(maxIdxCount * sizeof(uint32_t));
    float *uniqueVerts = (float*)malloc(maxIdxCount * VERTEX_STRIDE);
    float *verts = (float*)malloc(maxIdxCount * VERTEX_STRIDE);
    float *normals = (float *)malloc(maxIdxCount * VERTEX_STRIDE);
    float *uniqueNormals = (float *)malloc(maxIdxCount * VERTEX_STRIDE);

    /* Reindex the data of each cube */
    size_t totalVertCount = 0;
    for (auto &cube : lods[0]->cubeTable){
        size_t cubeIdxOffset = cube.second.idxOffset;

        memset(remap, 0, maxIdxCount * sizeof(uint32_t));
        /* Get the vertex data */
        /* Position */
        for(int i = 0; i < cube.second.triangleCount * 3; ++i){
            memcpy(verts + 3 * i, &rawMesh->positions[3 * data.indices[cubeIdxOffset + i]], VERTEX_STRIDE);
        }

        /* Normal */
        for(int i = 0; i < cube.second.triangleCount * 3; ++i){
            memcpy(normals + 3 * i, &rawMesh->normals[3 * data.indices[cubeIdxOffset + i]], VERTEX_STRIDE);
        }

        /* Assigne index value */
        for(int i = 0; i < cube.second.triangleCount * 3; ++i){
            data.indices[cubeIdxOffset + i] = i;
        }

        /* Re-organize vertex */
        int vertCount = cube.second.triangleCount * 3;
        int uniqueVertCount = meshopt_generateVertexRemap(remap, &data.indices[cubeIdxOffset], vertCount, verts, vertCount, VERTEX_STRIDE);
        meshopt_remapVertexBuffer(uniqueVerts, verts, vertCount, VERTEX_STRIDE, remap);
        meshopt_remapVertexBuffer(uniqueNormals, normals, vertCount, VERTEX_STRIDE, remap);
        //meshopt_remapIndexBuffer(&data.indices[cubeIdxOffset], NULL, cube.second.triangleCount * 3, remap);

        size_t new_index_count = 0;
        for (size_t i = 0; i < cube.second.triangleCount * 3; i += 3)
        {
            uint32_t i0 = remap[data.indices[cubeIdxOffset + i]];
            uint32_t i1 = remap[data.indices[cubeIdxOffset + i + 1]];
            uint32_t i2 = remap[data.indices[cubeIdxOffset + i + 2]];

            if (i0 != i1 && i0 != i2 && i1 != i2)
            {
                data.indices[cubeIdxOffset + new_index_count + 0] = i0;
                data.indices[cubeIdxOffset + new_index_count + 1] = i1;
                data.indices[cubeIdxOffset + new_index_count + 2] = i2;
                new_index_count += 3;
            }
        }

        /* Copy data */
        size_t vertexOffset = totalVertCount;
        size_t vertexBufferSize = totalVertCount + uniqueVertCount;

        /* Reallocate data */
        data.normals = (float*)realloc(data.normals, vertexBufferSize * VERTEX_STRIDE);
        data.positions = (float*)realloc(data.positions, vertexBufferSize * VERTEX_STRIDE);

        memcpy(&data.positions[3 * vertexOffset], uniqueVerts, VERTEX_STRIDE * uniqueVertCount);
        memcpy(&data.normals[3 * vertexOffset], uniqueNormals, VERTEX_STRIDE * uniqueVertCount);

        /* Update the vertex count */
        cube.second.vertCount = uniqueVertCount;
        cube.second.vertexOffset = vertexOffset;
        
        totalVertCount += uniqueVertCount;
        cube.second.triangleCount = new_index_count / 3;

        /* Compute the Cube bottom point assign the ijk to the bounding box and the cube */
        cube.second.ComputeBottomVertex(cube.second.bottom, cube.second.ijk, lods[0]->cubeLength, min);
        cube.second.ijk_64 = cube.first;
    }
    end = clock();

    /* If level = 0 remap to itself child-parent map */
    if (lods[0]->level == 0)
    {
        for (int i = 0; i < lods[0]->cubeTable[0].vertCount; ++i)
            data.remap[lods[0]->cubeTable[0].vertexOffset + i] = i;
    }

    data.posCount = totalVertCount;
    data.idxCount = totalIndexCount;
    
    /* Update the hlod data offset */
    curIdxOffset = totalIndexCount;
    curVertOffset = totalVertCount;

    data.normals = (float*)realloc(data.normals, data.posCount * VERTEX_STRIDE);
    data.positions = (float*)realloc(data.positions, data.posCount * VERTEX_STRIDE);
    data.remap = (uint32_t*)realloc(data.remap, data.posCount * sizeof(uint32_t));
    data.indices = (uint32_t*)realloc(data.indices, data.idxCount * sizeof(uint32_t));

    MemoryFree(remap);
    MemoryFree(verts);
    MemoryFree(normals);
    MemoryFree(uniqueVerts);
    MemoryFree(uniqueNormals);
}
