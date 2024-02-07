#include "MeshSimplifier.h"

#include "mesh_simplify/meshoptimizer_mod.h"
#include "mesh_simplify/simplify_mod_tex.h"

/* Threads number */
const unsigned short threadNum = 8;
pthread_mutex_t block_index_mutex;
unsigned idx_to_do;

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

size_t CollapseSimplifyMesh(uint32_t *out_indices, uint32_t *simplification_remap, const uint32_t *indices, size_t index_count,
                            const void *vertices, size_t vertex_count, int vertex_stride, size_t target_index_count, float targetError, float block_extend, float block_bottom[3])
{
    return meshopt_simplify_mod((unsigned int *)out_indices, (unsigned int *)simplification_remap,
                                (const unsigned int *)indices, index_count, (const float *)vertices, vertex_count, vertex_stride, target_index_count,
                                targetError, NULL, block_extend, block_bottom);
}

size_t CollapseSimplifyMeshTex(uint32_t *out_indices, uint32_t *out_indicesUV, uint32_t *simplification_remap, uint32_t *simplification_remap_uv,
                               const uint32_t *indices, size_t index_count, const uint32_t *indicesUV, size_t index_uv_count,
                               const void *vertices, size_t vertex_count, const void *uvs, size_t uv_count, unsigned vertex_stride, unsigned uv_stride,
                               size_t target_index_count, float targetError, float block_extend, float block_bottom[3])
{
    return meshopt_simplify_mod_texDebug((unsigned int *)out_indices, (unsigned int *)out_indicesUV, (unsigned int *)simplification_remap,
                                         (unsigned int *)simplification_remap_uv, (const unsigned int *)indices, index_count,
                                         (const unsigned int *)indicesUV, index_uv_count, (const float *)vertices, vertex_count, (const float *)uvs,
                                         uv_count, vertex_stride, uv_stride, target_index_count, targetError, NULL, block_extend, block_bottom);
}

unsigned LoadBlockData(HLOD *hlod, Boxcoord& bottom, int curLevel, int width, unordered_map<uint64_t, pair<size_t, size_t>>& cubeTable, Mesh *destination, bool isGetData){
    Boxcoord temp;
    size_t indexOffset = 0;
    size_t vertexOffset = 0;

    unsigned cubeCount = 0;
    short nx = bottom.x;
    short ny = bottom.y;
    short nz = bottom.z;
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
                if(!ConvertBlockCoordinates(temp, result, SC_BLOCK_SIZE, hlod->lods[curLevel]->gridSize))
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

unsigned LoadChildData(HLOD *hlod, Mesh *blkData, uint64_t *cubeList, Boxcoord& bottom, int curLevel, unordered_map<uint64_t, pair<size_t, size_t>>& cubeTable, Mesh *destination){
    Boxcoord temp;
    size_t indexOffset = 0;
    size_t vertexOffset = 0;
    size_t uvIndexOffset = 0;
    size_t uvOffset = 0;

    unsigned cubeCount = 0;
    short nx = bottom.x;
    short ny = bottom.y;
    short nz = bottom.z;

    for (int dx = 0; dx < 2; dx++){
        temp.x = nx + dx;
        for (int dy = 0; dy < 2; dy++){
            temp.y = ny + dy;
            for (int dz = 0; dz < 2; dz++){
                temp.z = nz + dz;
                
                Boxcoord result;
                if(!ConvertBlockCoordinates(temp, result, SC_BLOCK_SIZE, hlod->lods[curLevel]->gridSize))
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

    for (int nx = 0; nx < hlod->lods[curLevel]->gridSize + blk->width; nx = nx + blk->width)
    {
        for (int ny = 0; ny < hlod->lods[curLevel]->gridSize + blk->width; ny = ny + blk->width)
        {
            for (int nz = 0; nz < hlod->lods[curLevel]->gridSize + blk->width; nz = nz + blk->width)
            {
                Boxcoord bottom;
                bottom.x = nx, bottom.y = ny, bottom.z = nz;
                size_t index_count = 0;
                size_t vertex_count = 0;

                size_t index_uv_count = 0;
                size_t uv_count = 0;

                /* Get the data of block */
                Mesh dest;
                unsigned box_count = LoadBlockData(hlod, bottom, curLevel, blk->width, cubeTable, &dest, false);

                index_count = dest.idxCount;
                vertex_count = dest.posCount;

                if (modelAttriSatus.hasSingleTexture || modelAttriSatus.hasMultiTexture)
                {
                    index_uv_count = dest.idxUVCount;
                    uv_count = dest.uvCount;
                }

                if (box_count == 0)
                    continue;
                
                blk->list[block_counter] = bottom;
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
                         int vertex_stride, uint32_t *grid_remap, uint32_t *simplification_remap)
{
    for (size_t i = 0; i < vertex_count; ++i)
    {
        uint32_t j = grid_remap[i];
        uint32_t k = simplification_remap[j];
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
                        int color_stride, uint32_t *grid_remap, uint32_t *simplification_remap)
{
    for (size_t i = 0; i < color_count; ++i)
    {
        uint32_t j = grid_remap[i];
        uint32_t k = simplification_remap[j];
        memcpy((unsigned char *)color_parents + i * color_stride, (unsigned char *)unique_color_parents + k * color_stride, color_stride);
    }
}

void InitParentMeshGrid(LOD *pmg, LOD *mg)
{
    pmg->level = mg->level - 1;
    pmg->gridSize = mg->gridSize / 2 ;

    pmg->step = mg->step * 0.5f;
    pmg->cubeLength = mg->cubeLength * 2.0f;
}

void *BlockSimplification(Parameter &arg, unsigned int block_idx)
{
    Boxcoord bottom = arg.simplifyBlks->list[block_idx];
    
    unordered_map<uint64_t, pair<size_t, size_t>> cubeTable;
    /* Mesh data buffer */
    Mesh blk_data;
    blk_data.indices = (uint32_t*)malloc(arg.simplifyBlks->maxIdxCount * sizeof(uint32_t));
    blk_data.positions = (float*)malloc(arg.simplifyBlks->maxVertexCount * VERTEX_STRIDE);
    blk_data.normals = (float*)malloc(arg.simplifyBlks->maxVertexCount * VERTEX_STRIDE);
    blk_data.idxCount = 0;
    blk_data.posCount = 0;

    float    *unique_positions = (float*)malloc(arg.simplifyBlks->maxVertexCount * VERTEX_STRIDE);
    uint32_t *grid_remap = (uint32_t*)malloc(arg.simplifyBlks->maxVertexCount * sizeof(uint32_t));
    uint32_t *simplification_remap = (uint32_t*)malloc(arg.simplifyBlks->maxVertexCount * sizeof(uint32_t));
    //cout<<"blk max idx: "<<arg.simplifyBlks->maxIdxCount<<" max vert: "<<arg.simplifyBlks->maxVertexCount<<endl;
    unsigned box_count = LoadBlockData(arg.hlod, bottom, arg.curLevel, SC_BLOCK_SIZE, cubeTable, &blk_data, true);

    if (!box_count)
        return NULL;
    //cout<<"block cube "<<box_count<<endl;

    float extension = arg.hlod->lods[arg.curLevel]->cubeLength;
    float simplification_error = arg.targetError * extension;
    float block_extension = 4 * extension;

    Boxcoord coord_real;
    coord_real.x = (bottom.x - arg.simplifyBlks->width / 2);
    coord_real.y = (bottom.y - arg.simplifyBlks->width / 2);
    coord_real.z = (bottom.z - arg.simplifyBlks->width / 2);
    
    float block_bottom[3];
    block_bottom[0] = arg.hlod->min[0] + coord_real.x * extension;
    block_bottom[1] = arg.hlod->min[1] + coord_real.y * extension;
    block_bottom[2] = arg.hlod->min[2] + coord_real.z * extension;
    
    /* Mesh simplification */
    size_t unique_vertex_count = meshopt_generateVertexRemap(grid_remap, NULL, blk_data.posCount, blk_data.positions, blk_data.posCount, VERTEX_STRIDE);
    meshopt_remapVertexBuffer(unique_positions, blk_data.positions, blk_data.posCount, VERTEX_STRIDE, grid_remap);
    blk_data.idxCount = RemapIndexBufferSkipDegenerate(blk_data.indices, blk_data.idxCount, grid_remap);
    size_t out_index_count = CollapseSimplifyMesh(blk_data.indices, simplification_remap, blk_data.indices, blk_data.idxCount, unique_positions,
                                            unique_vertex_count, VERTEX_STRIDE, blk_data.idxCount / 4, simplification_error, block_extension, block_bottom);
    
    /* Update and wirte back parent information */
    UpdateVertexParents(blk_data.positions, unique_positions, blk_data.posCount, unique_vertex_count, VERTEX_STRIDE, grid_remap, simplification_remap);

    /* Parent cube construction */
    uint64_t *boxes_2 = (uint64_t *)malloc(arg.parentBlks->maxBoxCount * sizeof(uint64_t));
    float *unique_positions_2 = (float*)malloc(arg.parentBlks->maxVertexCount * VERTEX_STRIDE);
    float *unique_normals_2 = (float*)malloc(arg.parentBlks->maxVertexCount * VERTEX_STRIDE);
    uint32_t *remap_2 = (uint32_t *)malloc(arg.parentBlks->maxVertexCount * sizeof(uint32_t));

    Mesh blk_data_2;
    blk_data_2.indices = (uint32_t *)malloc(arg.parentBlks->maxIdxCount * sizeof(uint32_t));
    blk_data_2.positions = (float *)malloc(arg.parentBlks->maxVertexCount * VERTEX_STRIDE);
    blk_data_2.normals = (float *)malloc(arg.parentBlks->maxVertexCount * VERTEX_STRIDE);
    
    for (unsigned short ix = bottom.x; ix < bottom.x + SC_BLOCK_SIZE; ix = ix + 2)
    {
        for (unsigned short iy = bottom.y; iy < bottom.y + SC_BLOCK_SIZE; iy = iy + 2)
        {
            for (unsigned short iz = bottom.z; iz < bottom.z + SC_BLOCK_SIZE; iz = iz + 2)
            {

                Boxcoord bottom_2;
                bottom_2.x = ix, bottom_2.y = iy, bottom_2.z = iz;

                blk_data_2.idxCount = 0;
                blk_data_2.posCount = 0;

                unsigned childCubeCount = LoadChildData(arg.hlod, &blk_data, boxes_2, bottom_2, arg.curLevel, cubeTable, &blk_data_2);
                
                if (!childCubeCount)
                    continue;
                //cout<<"parent cube: "<<childCubeCount<<endl;
                
                int unique_vertex_count_2 = meshopt_generateVertexRemap(remap_2, blk_data_2.indices, blk_data_2.idxCount, blk_data_2.positions, blk_data_2.posCount, VERTEX_STRIDE);
                meshopt_remapVertexBuffer(unique_positions_2, blk_data_2.positions, blk_data_2.posCount, VERTEX_STRIDE, remap_2);
                meshopt_remapVertexBuffer(unique_normals_2, blk_data_2.normals, blk_data_2.posCount, VERTEX_STRIDE, remap_2);
                blk_data_2.idxCount = RemapIndexBufferSkipDegenerate(blk_data_2.indices, blk_data_2.idxCount, remap_2);
                
                //TODO: NO DIVISION
                bottom_2.x = short((bottom_2.x - SC_BLOCK_SIZE / 2) / 2);
                bottom_2.y = short((bottom_2.y - SC_BLOCK_SIZE / 2) / 2);
                bottom_2.z = short((bottom_2.z - SC_BLOCK_SIZE / 2) / 2);

                uint64_t ijk_p = (uint64_t)(bottom_2.x) | ((uint64_t)(bottom_2.y) << 16) | ((uint64_t)(bottom_2.z) << 32);
                
                Cube cell_p;

                cell_p.ijk[0] = bottom_2.x;
                cell_p.ijk[1] = bottom_2.y;
                cell_p.ijk[2] = bottom_2.z;

                cell_p.ijk_64 = ijk_p;

                pthread_mutex_lock(&block_index_mutex);
                {
                    /* Update cube offset */
                    cell_p.triangleCount = blk_data_2.idxCount / 3;
                    cell_p.vertCount = unique_vertex_count_2;

                    cell_p.vertexOffset = arg.hlod->curVertOffset;
                    cell_p.idxOffset = arg.hlod->curIdxOffset;

                    arg.hlod->curIdxOffset += blk_data_2.idxCount;
                    arg.hlod->curVertOffset += unique_vertex_count_2;
                }
                pthread_mutex_unlock(&block_index_mutex);

                arg.hlod->lods[arg.curLevel + 1]->cubeTable.insert(make_pair(ijk_p, cell_p));

                memcpy(&arg.hlod->data.indices[cell_p.idxOffset], blk_data_2.indices, blk_data_2.idxCount * sizeof(uint32_t));
                memcpy(&arg.hlod->data.positions[3 * cell_p.vertexOffset], unique_positions_2, unique_vertex_count_2 * VERTEX_STRIDE);
                memcpy(&arg.hlod->data.normals[3 * cell_p.vertexOffset], unique_normals_2, unique_vertex_count_2 * VERTEX_STRIDE);

                /* Upadate the vertex parent remap */
                size_t vertex_offset = 0;
                for (int i = 0; i < childCubeCount; ++i)
                {
                    uint64_t ijk_child = boxes_2[i];
                    for (int j = 0; j < arg.hlod->lods[arg.curLevel]->cubeTable[ijk_child].vertCount; ++j)
                    {
                        uint32_t l = j + vertex_offset;
                        size_t remapOffset = arg.hlod->lods[arg.curLevel]->cubeTable[ijk_child].vertexOffset;
                        arg.hlod->data.remap[remapOffset + j] = remap_2[l];
                    }
                    vertex_offset += arg.hlod->lods[arg.curLevel]->cubeTable[ijk_child].vertCount;
                }
            }
        }
        
    }

    /* Free cube table */
    cubeTable.clear();

    /* Free memory space */
    MemoryFree(unique_positions);
    MemoryFree(grid_remap);
    MemoryFree(simplification_remap);

    MemoryFree(blk_data.normals);
    MemoryFree(blk_data.positions);
    MemoryFree(blk_data.indices);
        
    /* parent reconstruction block data */
    MemoryFree(unique_positions_2);
    MemoryFree(unique_normals_2);
    MemoryFree(remap_2);
    MemoryFree(boxes_2);

    MemoryFree(blk_data_2.normals);
    MemoryFree(blk_data_2.positions);
    MemoryFree(blk_data_2.indices);
    
    return NULL;
}

bool BuileNextBlock(Parameter& param)
{
    pthread_mutex_lock(&block_index_mutex);
    unsigned curIdx = idx_to_do;
    if (curIdx >= param.simplifyBlks->validBoxCount)
    {
        pthread_mutex_unlock(&block_index_mutex);
        return false;
    }
    idx_to_do ++;
    pthread_mutex_unlock(&block_index_mutex);
    
    BlockSimplification(param, curIdx);

    return true;
}

void *BuildParallel(void *arg)
{
    Parameter tmp = *(Parameter *)arg;
    
    while (BuileNextBlock(tmp))
    {
    }
    return NULL;
}

void LODConstructor(HLOD *hlod, int curLevel, int width, float targetError){
    /* Init parent mesh level */
    InitParentMeshGrid(hlod->lods[curLevel+1], hlod->lods[curLevel]);
    
    int grid_size = hlod->lods[curLevel]->gridSize;

    /* Simplification block information */
    unsigned short value = int((grid_size + width) / width);
    unsigned short block_max_num = 0;
    block_max_num = value * value * value;

    Block blks_simplify;
    blks_simplify.width = width;
    blks_simplify.list = (Boxcoord *)calloc(block_max_num, block_max_num * 3 * sizeof(unsigned short));
    blks_simplify.maxBoxCount = ComputeMaxCounts(hlod, curLevel, &blks_simplify);
    
    /* Parent cube construction block information */
    Block blks_parent;
    blks_parent.width = 2;
    blks_parent.list = (Boxcoord *)calloc(block_max_num, (block_max_num) * 3 * sizeof(unsigned short));
    blks_parent.maxBoxCount = ComputeMaxCounts(hlod, curLevel, &blks_parent);

    /* Thread parameter */
    Parameter attr;
    attr.hlod = hlod;
    attr.curLevel = curLevel;
    attr.simplifyBlks = &blks_simplify;
    attr.parentBlks = &blks_parent;
    attr.targetError = targetError;
    idx_to_do = 0;
    
    /* Allocate memory for next LOD level */
    size_t vertexBufferSize = hlod->curVertOffset + hlod->lods[curLevel]->totalVertCount;
    size_t idxBufferSize = hlod->curIdxOffset + hlod->lods[curLevel]->totalTriCount * 3;

    // cout<<"current size : "<<hlod->curVertOffset<<" "<<vertexBufferSize<<endl;
    // cout<<"current size : "<<hlod->curIdxOffset<<" "<<idxBufferSize<<endl;

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
    
    /* Compute the bottom vertex of cube for next LOD */
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

    /* free data */
    // MemoryFree(blks_simplify.list);
    // MemoryFree(blks_parent.list);
}

void HLODConsructor(HLOD *hlod, int maxLevel, float targetError){
    /* Multi-resolution model construction */
    for (int i = 0; i < maxLevel; i++)                                          
    {                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                   
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