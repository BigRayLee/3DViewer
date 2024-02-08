#include "MeshSimplifier.h"

#include "mesh_simplify/meshoptimizer_mod.h"
#include "mesh_simplify/simplify_mod_tex.h"

/* Threads number */
const unsigned short threadNum = 8;
pthread_mutex_t block_index_mutex;
unsigned nextIdx;

size_t RemapIndexBufferSkipDegenerate(uint32_t *indices, size_t index_count, const uint32_t *remap)
{
    size_t new_index_count = 0;

    for (size_t i = 0; i < index_count; i += 3)
    {
        uint32_t i0 = remap[indices[i + 0]];
        uint32_t i1 = remap[indices[i + 1]];
        uint32_t i2 = remap[indices[i + 2]];

        if (i0 != i1 && i0 != i2 && i1 != i2)
        {
            indices[new_index_count + 0] = i0;
            indices[new_index_count + 1] = i1;
            indices[new_index_count + 2] = i2;
            new_index_count += 3;
        }
    }

    return new_index_count;
}

unsigned LoadBlockData(HLOD *hlod, Boxcoord& blkCoord, int curLevel, int width, unordered_map<uint64_t, pair<size_t, size_t>>& cubeTable, Mesh *destination, bool isGetData){
    Boxcoord temp;
    size_t indexOffset = 0;
    size_t vertexOffset = 0;

    unsigned cubeCount = 0;
    short nx = blkCoord.x;
    short ny = blkCoord.y;
    short nz = blkCoord.z;
    for (int dx = 0; dx < width; dx++)
    {
        temp.x = nx + dx;
        for (int dy = 0; dy < width; dy++)
        {
            temp.y = ny + dy;
            for (int dz = 0; dz < width; dz++)
            {
                temp.z = nz + dz;

                Boxcoord result;
                if(!ConvertBlockCoordinates(temp, result, SC_BLOCK_SIZE, hlod->lods[curLevel]->lodSize))
                    continue;
                
                uint64_t ijk = (uint64_t)(result.x) | ((uint64_t)(result.y) << 16) | ((uint64_t)(result.z) << 32);
                
                if (hlod->lods[curLevel]->cubeTable.count(ijk) == 0)
                    continue;

                int indexCount = hlod->lods[curLevel]->cubeTable[ijk].triangleCount * 3;
                int vertexCount = hlod->lods[curLevel]->cubeTable[ijk].vertCount;

                if (isGetData){
                    // Offset of hlod data buffer
                    size_t cubeVertexOffset = hlod->lods[curLevel]->cubeTable[ijk].vertexOffset;
                    size_t cubeIdxOffset = hlod->lods[curLevel]->cubeTable[ijk].idxOffset;
                    //cout<<"load block "<<indexOffset<<" "<<vertexOffset<<" index count: "<<indexCount<<endl;
                    uint32_t *targetIndices = destination->indices + indexOffset;
                    memcpy(targetIndices, &hlod->data.indices[cubeIdxOffset], indexCount * sizeof(uint32_t));
                    
                    for (size_t i = 0; i < indexCount; ++i){
                        targetIndices[i] = targetIndices[i] + vertexOffset;
                    }
                                        
                    /* position */
                    float *targetPosition = destination->positions + vertexOffset * 3;
                    memcpy(targetPosition, &hlod->data.positions[3 * cubeVertexOffset], vertexCount * VERTEX_STRIDE);

                    // for (size_t i = 0; i < vertexCount * 3; i = i + 3){
                    //     cout<<targetPosition[i]<<" "<<targetPosition[i + 1]<<" "<<targetPosition[i+2]<<",";
                    // }

                    /* normal */
                    float *targetNormal = destination->normals + vertexOffset * 3;
                    memcpy(targetNormal, &hlod->data.normals[3 * cubeVertexOffset], vertexCount * VERTEX_STRIDE);
                    
                    cubeTable.insert(make_pair(ijk, make_pair(indexOffset, vertexOffset)));
                }

                cubeCount++;
                
                indexOffset += indexCount;
                vertexOffset += vertexCount;
            }
        }
    }

    destination->idxCount = indexOffset;
    destination->posCount = vertexOffset;

    return cubeCount;
}

unsigned LoadChildData(HLOD *hlod, Mesh *blkData, uint64_t *cubeList, Boxcoord& blkCoord, int curLevel, unordered_map<uint64_t, pair<size_t, size_t>>& cubeTable, Mesh *destination){
    Boxcoord temp;
    size_t indexOffset = 0;
    size_t vertexOffset = 0;
    size_t uvIndexOffset = 0;
    size_t uvOffset = 0;

    unsigned cubeCount = 0;
    short nx = blkCoord.x;
    short ny = blkCoord.y;
    short nz = blkCoord.z;

    for (int dx = 0; dx < 2; dx++){
        temp.x = nx + dx;
        for (int dy = 0; dy < 2; dy++){
            temp.y = ny + dy;
            for (int dz = 0; dz < 2; dz++){
                temp.z = nz + dz;
                
                Boxcoord result;
                if(!ConvertBlockCoordinates(temp, result, SC_BLOCK_SIZE, hlod->lods[curLevel]->lodSize))
                    continue;

                uint64_t ijk = (uint64_t)(result.x) | ((uint64_t)(result.y) << 16) | ((uint64_t)(result.z) << 32);
                
                if (hlod->lods[curLevel]->cubeTable.count(ijk) == 0)
                    continue;

                
                cubeList[cubeCount] = ijk;

                int indexCount = hlod->lods[curLevel]->cubeTable[ijk].triangleCount * 3;
                int vertexCount = hlod->lods[curLevel]->cubeTable[ijk].vertCount;

                // Vetrex offset based on simplied vertices
                size_t cubeVertexOffset = cubeTable[ijk].second;
                // Index offset based on original data
                size_t cubeIdxOffset = hlod->lods[curLevel]->cubeTable[ijk].idxOffset;

                uint32_t *targetIndices = destination->indices + indexOffset;
                memcpy(targetIndices, &hlod->data.indices[cubeIdxOffset], indexCount * sizeof(uint32_t));

                for (size_t i = 0; i < indexCount; ++i){
                    targetIndices[i] = targetIndices[i] + vertexOffset;
                }
                
                /* position */
                float *targetPosition = destination->positions + vertexOffset * 3;
                memcpy(targetPosition, &blkData->positions[3 * cubeVertexOffset], vertexCount * VERTEX_STRIDE);
                
                /* normal */
                float *targetNormal = destination->normals + vertexOffset * 3;
                memcpy(targetNormal, &blkData->normals[3 * cubeVertexOffset], vertexCount * VERTEX_STRIDE);
                
                indexOffset += indexCount;
                vertexOffset += vertexCount;

                cubeCount ++;
            }
        }
    }

    destination->idxCount = indexOffset;
    destination->posCount = vertexOffset;

    return cubeCount;
}

unsigned ComputeMaxCounts(HLOD *hlod, int curLevel, Block* blk){
    size_t maxIdxCount = 0;
    size_t maxVertexCount = 0;

    size_t maxIdxUVCount = 0;
    size_t maxUVCount = 0;

    unsigned maxBoxCount = 0;

    unsigned short block_counter = 0;

    unordered_map<uint64_t, pair<size_t, size_t>> cubeTable;

    for (int nx = 0; nx < hlod->lods[curLevel]->lodSize + blk->width; nx = nx + blk->width)
    {
        for (int ny = 0; ny < hlod->lods[curLevel]->lodSize + blk->width; ny = ny + blk->width)
        {
            for (int nz = 0; nz < hlod->lods[curLevel]->lodSize + blk->width; nz = nz + blk->width)
            {
                Boxcoord blkCoord;
                blkCoord.x = nx, blkCoord.y = ny, blkCoord.z = nz;
                size_t index_count = 0;
                size_t vertex_count = 0;

                size_t index_uv_count = 0;
                size_t uv_count = 0;

                /* Get the data of block */
                Mesh dest;
                unsigned box_count = LoadBlockData(hlod, blkCoord, curLevel, blk->width, cubeTable, &dest, false);

                index_count = dest.idxCount;
                vertex_count = dest.posCount;

                if (modelAttriSatus.hasSingleTexture || modelAttriSatus.hasMultiTexture)
                {
                    index_uv_count = dest.idxUVCount;
                    uv_count = dest.uvCount;
                }

                if (box_count == 0)
                    continue;
                
                blk->list[block_counter] = blkCoord;
                block_counter++;

                maxBoxCount = box_count > maxBoxCount ? box_count : maxBoxCount;
                maxIdxCount = index_count > maxIdxCount ? index_count : maxIdxCount;
                maxVertexCount = vertex_count > maxVertexCount ? vertex_count : maxVertexCount;

                if (modelAttriSatus.hasSingleTexture || modelAttriSatus.hasMultiTexture)
                {
                    maxIdxUVCount = index_uv_count > maxIdxUVCount ? index_uv_count : maxIdxUVCount;
                    maxUVCount = uv_count > maxUVCount ? uv_count : maxUVCount;
                }
            }
        }
    }
    blk->maxIdxCount = maxIdxCount;
    blk->maxVertexCount = maxVertexCount;
    blk->validBoxCount = block_counter;

    if (modelAttriSatus.hasSingleTexture || modelAttriSatus.hasMultiTexture)
    {
        blk->maxIdxUVCount = maxIdxUVCount;
        blk->maxUVCount = maxUVCount;
    }

    return maxBoxCount;
}

/* Update position */
void UpdateVertexParents(void *parents, void *unique_parents, size_t vertex_count, size_t unique_vertex_count,
                         int vertex_stride, uint32_t *remap, uint32_t *simplificationRemap)
{
    for (size_t i = 0; i < vertex_count; ++i)
    {
        uint32_t j = remap[i];
        uint32_t k = simplificationRemap[j];
        memcpy((unsigned char *)parents + i * vertex_stride, (unsigned char *)unique_parents + k * vertex_stride, vertex_stride);
    }
}

/* Update the texture coordinates */
void UpdateUVParents(void *uv_parents, void *unique_uv_parents, size_t uv_count, size_t unique_uv_count,
                     int uv_stride, uint32_t *grid_remap_uv, uint32_t *simlification_remap_uv)
{
    for (size_t i = 0; i < uv_count; ++i)
    {
        uint32_t j = grid_remap_uv[i];
        uint32_t k = simlification_remap_uv[j];
        memcpy((unsigned char *)uv_parents + i * uv_stride, (unsigned char *)unique_uv_parents + k * uv_stride, uv_stride);
    }
}

/* Update color */
void UpdateColorParents(void *color_parents, void *unique_color_parents, size_t color_count, size_t unique_color_count,
                        int color_stride, uint32_t *remap, uint32_t *simplificationRemap)
{
    for (size_t i = 0; i < color_count; ++i)
    {
        uint32_t j = remap[i];
        uint32_t k = simplificationRemap[j];
        memcpy((unsigned char *)color_parents + i * color_stride, (unsigned char *)unique_color_parents + k * color_stride, color_stride);
    }
}

void InitParentMeshGrid(LOD *pmg, LOD *mg)
{
    pmg->level = mg->level - 1;
    pmg->lodSize = mg->lodSize / 2 ;

    pmg->step = mg->step * 0.5f;
    pmg->cubeLength = mg->cubeLength * 2.0f;
}

void *BlockSimplification(Parameter &arg, unsigned int block_idx)
{
    Boxcoord blkCoord = arg.simplifyBlks->list[block_idx];
    
    unordered_map<uint64_t, pair<size_t, size_t>> cubeTable;
    /* Mesh data buffer */
    Mesh simplifyBlk;
    simplifyBlk.indices = (uint32_t*)malloc(arg.simplifyBlks->maxIdxCount * sizeof(uint32_t));
    simplifyBlk.positions = (float*)malloc(arg.simplifyBlks->maxVertexCount * VERTEX_STRIDE);
    simplifyBlk.normals = (float*)malloc(arg.simplifyBlks->maxVertexCount * VERTEX_STRIDE);
    simplifyBlk.idxCount = 0;
    simplifyBlk.posCount = 0;

    unsigned box_count = LoadBlockData(arg.hlod, blkCoord, arg.curLevel, SC_BLOCK_SIZE, cubeTable, &simplifyBlk, true);

    if (!box_count)
        return NULL;

    float extension = arg.hlod->lods[arg.curLevel]->cubeLength;
    float simplification_error = arg.targetError * extension;
    float block_extension = 4 * extension;

    Boxcoord realCoord;
    realCoord.x = (blkCoord.x - arg.simplifyBlks->width / 2);
    realCoord.y = (blkCoord.y - arg.simplifyBlks->width / 2);
    realCoord.z = (blkCoord.z - arg.simplifyBlks->width / 2);
    
    float blkBottom[3];
    blkBottom[0] = arg.hlod->min[0] + realCoord.x * extension;
    blkBottom[1] = arg.hlod->min[1] + realCoord.y * extension;
    blkBottom[2] = arg.hlod->min[2] + realCoord.z * extension;

    /* Allocate memory for simplified mesh */
    float    *uniquePositions = (float*)malloc(arg.simplifyBlks->maxVertexCount * VERTEX_STRIDE);
    uint32_t *remap = (uint32_t*)malloc(arg.simplifyBlks->maxVertexCount * sizeof(uint32_t));
    uint32_t *simplificationRemap = (uint32_t*)malloc(arg.simplifyBlks->maxVertexCount * sizeof(uint32_t));
    
    /* Mesh simplification */
    size_t unique_vertex_count = meshopt_generateVertexRemap(remap, NULL, simplifyBlk.posCount, simplifyBlk.positions, simplifyBlk.posCount, VERTEX_STRIDE);
    meshopt_remapVertexBuffer(uniquePositions, simplifyBlk.positions, simplifyBlk.posCount, VERTEX_STRIDE, remap);
    simplifyBlk.idxCount = RemapIndexBufferSkipDegenerate(simplifyBlk.indices, simplifyBlk.idxCount, remap);
    size_t out_index_count = meshopt_simplify_mod(simplifyBlk.indices, simplificationRemap, simplifyBlk.indices, simplifyBlk.idxCount, uniquePositions,
                                            unique_vertex_count, VERTEX_STRIDE, simplifyBlk.idxCount / 4, simplification_error, NULL, block_extension, blkBottom);
    
    /* Update and wirte back parent information */
    UpdateVertexParents(simplifyBlk.positions, uniquePositions, simplifyBlk.posCount, unique_vertex_count, VERTEX_STRIDE, remap, simplificationRemap);

    /* Parent cube construction */
    uint64_t *cubeList = (uint64_t *)malloc(arg.parentBlks->maxBoxCount * sizeof(uint64_t));
    float *unqiueParentPosition = (float*)malloc(arg.parentBlks->maxVertexCount * VERTEX_STRIDE);
    float *unqiueParentNormal = (float*)malloc(arg.parentBlks->maxVertexCount * VERTEX_STRIDE);
    uint32_t *parentRemap = (uint32_t *)malloc(arg.parentBlks->maxVertexCount * sizeof(uint32_t));

    Mesh parentBlk;
    parentBlk.indices = (uint32_t *)malloc(arg.parentBlks->maxIdxCount * sizeof(uint32_t));
    parentBlk.positions = (float *)malloc(arg.parentBlks->maxVertexCount * VERTEX_STRIDE);
    parentBlk.normals = (float *)malloc(arg.parentBlks->maxVertexCount * VERTEX_STRIDE);
    
    for (unsigned short ix = blkCoord.x; ix < blkCoord.x + SC_BLOCK_SIZE; ix = ix + 2)
    {
        for (unsigned short iy = blkCoord.y; iy < blkCoord.y + SC_BLOCK_SIZE; iy = iy + 2)
        {
            for (unsigned short iz = blkCoord.z; iz < blkCoord.z + SC_BLOCK_SIZE; iz = iz + 2)
            {

                Boxcoord parentCoord;
                parentCoord.x = ix, parentCoord.y = iy, parentCoord.z = iz;

                parentBlk.idxCount = 0;
                parentBlk.posCount = 0;

                unsigned childCubeCount = LoadChildData(arg.hlod, &simplifyBlk, cubeList, parentCoord, arg.curLevel, cubeTable, &parentBlk);
                
                if (!childCubeCount)
                    continue;
                //cout<<"parent cube: "<<childCubeCount<<endl;
                
                int unique_vertex_count_2 = meshopt_generateVertexRemap(parentRemap, parentBlk.indices, parentBlk.idxCount, parentBlk.positions, parentBlk.posCount, VERTEX_STRIDE);
                meshopt_remapVertexBuffer(unqiueParentPosition, parentBlk.positions, parentBlk.posCount, VERTEX_STRIDE, parentRemap);
                meshopt_remapVertexBuffer(unqiueParentNormal, parentBlk.normals, parentBlk.posCount, VERTEX_STRIDE, parentRemap);
                parentBlk.idxCount = RemapIndexBufferSkipDegenerate(parentBlk.indices, parentBlk.idxCount, parentRemap);
                
                //TODO: NO DIVISION
                parentCoord.x = short((parentCoord.x - SC_BLOCK_SIZE / 2) / 2);
                parentCoord.y = short((parentCoord.y - SC_BLOCK_SIZE / 2) / 2);
                parentCoord.z = short((parentCoord.z - SC_BLOCK_SIZE / 2) / 2);

                uint64_t ijk_p = (uint64_t)(parentCoord.x) | ((uint64_t)(parentCoord.y) << 16) | ((uint64_t)(parentCoord.z) << 32);
                
                Cube parentCube;

                parentCube.ijk[0] = parentCoord.x;
                parentCube.ijk[1] = parentCoord.y;
                parentCube.ijk[2] = parentCoord.z;

                parentCube.ijk_64 = ijk_p;

                pthread_mutex_lock(&block_index_mutex);
                {
                    /* Update cube offset */
                    parentCube.triangleCount = parentBlk.idxCount / 3;
                    parentCube.vertCount = unique_vertex_count_2;

                    parentCube.vertexOffset = arg.hlod->curVertOffset;
                    parentCube.idxOffset = arg.hlod->curIdxOffset;

                    arg.hlod->curIdxOffset += parentBlk.idxCount;
                    arg.hlod->curVertOffset += unique_vertex_count_2;
                }
                pthread_mutex_unlock(&block_index_mutex);

                arg.hlod->lods[arg.curLevel + 1]->cubeTable.insert(make_pair(ijk_p, parentCube));

                memcpy(&arg.hlod->data.indices[parentCube.idxOffset], parentBlk.indices, parentBlk.idxCount * sizeof(uint32_t));
                memcpy(&arg.hlod->data.positions[3 * parentCube.vertexOffset], unqiueParentPosition, unique_vertex_count_2 * VERTEX_STRIDE);
                memcpy(&arg.hlod->data.normals[3 * parentCube.vertexOffset], unqiueParentNormal, unique_vertex_count_2 * VERTEX_STRIDE);

                /* Upadate the vertex parent remap */
                size_t vertex_offset = 0;
                for (int i = 0; i < childCubeCount; ++i){
                    uint64_t ijk_child = cubeList[i];
                    for (int j = 0; j < arg.hlod->lods[arg.curLevel]->cubeTable[ijk_child].vertCount; ++j){
                        uint32_t l = j + vertex_offset;
                        size_t remapOffset = arg.hlod->lods[arg.curLevel]->cubeTable[ijk_child].vertexOffset;
                        arg.hlod->data.remap[remapOffset + j] = parentRemap[l];
                    }
                    vertex_offset += arg.hlod->lods[arg.curLevel]->cubeTable[ijk_child].vertCount;
                }
            }
        }
        
    }

    /* Free cube table */
    cubeTable.clear();

    /* Free memory space */
    MemoryFree(uniquePositions);
    MemoryFree(remap);
    MemoryFree(simplificationRemap);

    MemoryFree(simplifyBlk.normals);
    MemoryFree(simplifyBlk.positions);
    MemoryFree(simplifyBlk.indices);
        
    /* parent reconstruction block data */
    MemoryFree(unqiueParentPosition);
    MemoryFree(unqiueParentNormal);
    MemoryFree(parentRemap);
    MemoryFree(cubeList);

    MemoryFree(parentBlk.normals);
    MemoryFree(parentBlk.positions);
    MemoryFree(parentBlk.indices);
    
    return NULL;
}

bool BuileNextBlock(Parameter& param)
{
    pthread_mutex_lock(&block_index_mutex);
    unsigned curIdx = nextIdx;
    
    if (curIdx >= param.simplifyBlks->validBoxCount){
        pthread_mutex_unlock(&block_index_mutex);
        return false;
    }

    nextIdx ++;
    pthread_mutex_unlock(&block_index_mutex);
    
    BlockSimplification(param, curIdx);

    return true;
}

void *BuildParallel(void *arg)
{
    Parameter tmp = *(Parameter *)arg;

    while (BuileNextBlock(tmp)){}
    return NULL;
}

void LODConstructor(HLOD *hlod, int curLevel, int width, float targetError){
    /* Init parent mesh level */
    InitParentMeshGrid(hlod->lods[curLevel+1], hlod->lods[curLevel]);
    
    int lodSize = hlod->lods[curLevel]->lodSize;

    /* Simplification block information */
    unsigned short value = int((lodSize + width) / width);
    unsigned short maxBlkCount = value * value * value;

    Block simplifyBlks;
    simplifyBlks.width = width;
    simplifyBlks.list = (Boxcoord *)calloc(maxBlkCount, maxBlkCount * 3 * sizeof(unsigned short));
    simplifyBlks.maxBoxCount = ComputeMaxCounts(hlod, curLevel, &simplifyBlks);
    
    /* Parent cube construction block information */
    Block parentBlks;
    parentBlks.width = 2;
    parentBlks.list = (Boxcoord *)calloc(maxBlkCount, (maxBlkCount) * 3 * sizeof(unsigned short));
    parentBlks.maxBoxCount = ComputeMaxCounts(hlod, curLevel, &parentBlks);

    /* Thread parameter */
    Parameter attr;
    attr.hlod = hlod;
    attr.curLevel = curLevel;
    attr.simplifyBlks = &simplifyBlks;
    attr.parentBlks = &parentBlks;
    attr.targetError = targetError;
    nextIdx = 0;
    
    /* Allocate memory for next LOD level */
    size_t vertexBufferSize = hlod->curVertOffset + hlod->lods[curLevel]->totalVertCount;
    size_t idxBufferSize = hlod->curIdxOffset + hlod->lods[curLevel]->totalTriCount * 3;

    hlod->data.normals = (float*)realloc(hlod->data.normals, vertexBufferSize * VERTEX_STRIDE);
    hlod->data.positions = (float*)realloc(hlod->data.positions, vertexBufferSize * VERTEX_STRIDE);
    hlod->data.remap = (uint32_t*)realloc(hlod->data.remap, vertexBufferSize * sizeof(uint32_t));
    hlod->data.indices = (uint32_t*)realloc(hlod->data.indices, idxBufferSize * sizeof(uint32_t));
    
    /* Create threads */
    pthread_t threads[threadNum];
    pthread_mutex_init(&block_index_mutex, NULL);

    for (int i = 0; i < threadNum; ++i){
        pthread_create(&threads[i], NULL, BuildParallel, (void *)&attr);
    }

    for (int i = 0; i < threadNum; ++i){

        pthread_join(threads[i], NULL);
    }

    pthread_mutex_destroy(&block_index_mutex);
    
    /* Compute the blkCoord vertex of cube for next LOD */
    for (auto &cb : hlod->lods[curLevel + 1]->cubeTable){
        cb.second.ComputeBottomVertex(cb.second.bottom, cb.second.ijk, hlod->lods[curLevel + 1]->cubeLength, hlod->min);
    }

    /* For the coarst level, remap is itself */
    if (hlod->lods[curLevel + 1]->level == 0){
        for (int i = 0; i < hlod->lods[curLevel + 1]->cubeTable[0].vertCount; ++i){
            hlod->data.remap[hlod->lods[curLevel + 1]->cubeTable[0].vertexOffset + i] = i;
        }
    }

    hlod->data.posCount = hlod->curVertOffset;
    hlod->data.idxCount = hlod->curIdxOffset;
}

void HLODConsructor(HLOD *hlod, int maxLevel, float targetError){
    for (int i = 0; i < maxLevel; i++){                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   
        hlod->lods[i+1] = new LOD(maxLevel - 1 - i);
        cout << "LOD: " << maxLevel - 1 - i << " ";

        TimerStart();
        LODConstructor(hlod, i, SC_BLOCK_SIZE, targetError);
        TimerStop("build time: ");

        cout << "Cell: " << hlod->lods[i+1]->cubeTable.size() 
        << " faces: " << hlod->lods[i+1]->CalculateTriangleCounts() 
        << " positions:  "<< hlod->lods[i+1]->CalculateVertexCounts() 
        << " simplify ratio: " << float(hlod->lods[i+1]->totalTriCount) / float(hlod->lods[i]->totalTriCount) << endl;
    }

    hlod->data.normals = (float*)realloc(hlod->data.normals, hlod->data.posCount * VERTEX_STRIDE);
    hlod->data.positions = (float*)realloc(hlod->data.positions, hlod->data.posCount * VERTEX_STRIDE);
    hlod->data.remap = (uint32_t*)realloc(hlod->data.remap, hlod->data.posCount * sizeof(uint32_t));
    hlod->data.indices = (uint32_t*)realloc(hlod->data.indices, hlod->data.idxCount * sizeof(uint32_t));
}