#include "LOD.h"

LOD::LOD(int l) :  level(l), totalTriCount(0), totalVertCount(0), step(0.0f), cubeLength(0.0f){
    lodSize = 1 << l;
    cubeTable.clear() ;             
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
    uint64_t coord = (uint64_t)(xyz[0]) | ((uint64_t)(xyz[1]) << 16) | ((uint64_t)(xyz[2]) << 32);
    if UNLIKELY(table.size() == 0){
        Cube cube;
        cube.coord[0] = xyz[0];
        cube.coord[1] = xyz[1];
        cube.coord[2] = xyz[2];

        cube.coord64 = coord;

        cube.triangleCount += 1;

        pair<uint64_t, Cube> p(coord, cube);
        table.insert(p);
    }
    else{
        unordered_map<uint64_t, Cube>::const_iterator got = table.find(coord);
        if (table.count(coord) == 0){
            Cube cube;
            cube.coord[0] = xyz[0];
            cube.coord[1] = xyz[1];
            cube.coord[2] = xyz[2];

            cube.triangleCount += 1;

            cube.coord64 = coord;

            pair<uint64_t, Cube> p(coord, cube);
            table.insert(p);
        }
        else{
            table[coord].triangleCount += 1;
        }
    }
}

