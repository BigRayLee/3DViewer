#pragma once 

#include <cstdint>
#include <string>

#include "HLOD.h"
#include "Camera.h"
#include "HitRecord.h"

struct RenderCell
{
    int      lodIndex = -1;
    uint64_t coord = 0;
};

class OfflineRender
{
public:
    enum class DebugMode
    {
        CubeID,
        TriangleID,
        Normal,
        Depth,
        LodLevel
    };

    explicit OfflineRender(const HLOD &hlod);
    ~OfflineRender() = default;

    // Render cubes from one fixed LOD 
    bool RenderLOD(int lodIndex, const Camera& camera, int width, int height, const std::string& filename, DebugMode mode = DebugMode::CubeID) const;

    // Render the cubes selected by adaptive HLOD cut
    bool RenderCut(const std::vector<RenderCell>& cut, const Camera& camera, int width, int height, const std::string& filename, DebugMode mode = DebugMode::LodLevel) const;

private:
    bool TraceCut(const std::vector<RenderCell>& cut, const Ray& ray, HitRecord& hit) const;

    bool TraceRay(int lodIndex, const Ray& ray, HitRecord& hit) const;

    bool TraceCube(int lodIndex, const Cube& cube, const Ray& ray, HitRecord& hit) const;

    bool TraceTriangle(int lodIndex, const Cube &cube, uint32_t localTriangleId, const Ray& ray, HitRecord& hit) const;

    bool IntersectAABB(const Cube& cube, const Ray& ray, float& tNear, float& tFar) const;

    Vec3 MakePrimaryRayDirection(const Camera &camera, float u, float v) const;

    void ShadeDebug(const HitRecord& hit, DebugMode mode, unsigned char rgb[3]) const;

    bool RenderImpl(const Camera& camera, int width, int height, const std::string& filename, DebugMode mode, const std::vector<RenderCell>* cut, int fixedLodIndex) const;

    const HLOD& model;
};
