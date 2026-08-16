#include "OfflineRender.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <limits>
#include <vector>

namespace
{
    constexpr float kRayEpsilon = 1.0e-6f;
    constexpr float kParrallelEpsilon = 1.0e-8f;

    inline unsigned char ToByte(float value)
    {
        value = std::max(0.0f, std::min(1.0f, value));
        return static_cast<unsigned int>(255.0f * value + 0.5f);
    }

    inline uint32_t Hash32(uint64_t value)
    {
        value ^= value >> 33;
        value *= 0xff51afd7ed558ccdULL;
        value ^= value >> 33;
        value *= 0xc4ceb9fe1a85ec53ULL;
        value ^= value >> 33;
        return static_cast<uint32_t>(value ^ (value >> 32));
    }

    inline Vec3 LoadPosition(const Mesh &mesh, const Cube &cube, uint32_t localIndex)
    {
        const size_t vertexIndex = cube.vertexOffset + static_cast<size_t>(localIndex);
        const size_t base = 3 * vertexIndex;
        return Vec3{mesh.positions[base], mesh.positions[base + 1], mesh.positions[base + 2]};
    }

    inline Vec3 NormalizeSafe(const Vec3 &v)
    {
        const float len = norm(v);
        if (len <= kParrallelEpsilon)
        {
            return Vec3::Zero;
        }

        return v * (1.0f / len);
    }
}

OfflineRender::OfflineRender(const HLOD &hlod) : model(hlod)
{
}

Vec3 OfflineRender::MakePrimaryRayDirection(const Camera &camera, float u, float v) const
{
    const Vec3 origin = camera.get_position();
    const Vec3 pointOnRay = camera.world_coord_at(u, v, 0.5f);
    return NormalizeSafe(pointOnRay - origin);
}

bool OfflineRender::IntersectAABB(const Cube &cube, const Ray &ray, float& tNear, float& tFar) const
{
    tNear = 0.0f;
    tFar = std::numeric_limits<float>::infinity();

    for (int axis = 0; axis < 3; ++axis)
    {
        const float origin = ray.start[axis];
        const float direction = ray.dir[axis];
        const float bmin = cube.bottom[axis];
        const float bmax = cube.top[axis];

        if (std::fabs(direction) < kParrallelEpsilon)
        {
            if (origin < bmin || origin > bmax)
            {
                return false;
            }
            continue;
        }

        const float invDir = 1.0 / direction;
        float t0 = (bmin - origin) * invDir;
        float t1 = (bmax - origin) * invDir;

        if (t0 > t1)
        {
            std::swap(t0, t1);
        }

        tNear = std::max(tNear, t0);
        tFar = std::min(tFar, t1);

        if (tNear > tFar)
        {
            return false;
        }
    }

    return tFar >= kRayEpsilon;
}

bool OfflineRender::TraceTriangle(int lodIndex, const Cube &cube, uint32_t localTriangleId, const Ray& ray, HitRecord &hit) const
{
    const Mesh& mesh = model.data;
    const size_t indexBase = cube.idxOffset + 3 * static_cast<size_t>(localTriangleId);

    const uint32_t i0 = mesh.indices[indexBase + 0];
    const uint32_t i1 = mesh.indices[indexBase + 1];
    const uint32_t i2 = mesh.indices[indexBase + 2];

    const Vec3 v0 = LoadPosition(mesh, cube, i0);
    const Vec3 v1 = LoadPosition(mesh, cube, i1);
    const Vec3 v2 = LoadPosition(mesh, cube, i2);

    // Moller-Trumbore ray/triangle intersection
    const Vec3 e1 = v1 - v0;
    const Vec3 e2 = v2 - v0;
    const Vec3 p = cross(ray.dir, e2);
    const float det = dot(e1, p);

    if (std::fabs(det) < kParrallelEpsilon)
    {
        return false;
    }

    const float invDet = 1.0f / det;
    const Vec3 s = ray.start - v0;
    const float u = dot(s,p) * invDet;

    if (u < 0.0f || u > 1.0f)
    {
        return false;
    }

    const Vec3 q = cross(s, e1);
    const float v = dot(ray.dir, q) * invDet;

    if (v < 0.0f || (u + v) > 1.0f)
    {
        return false;
    }

    const float t = dot(e2, q) * invDet;
    if (t <= kRayEpsilon || t >= hit.distance)
    {
        return false;
    }

    hit.hit = true;
    hit.distance = t;
    hit.position = ray.start + ray.dir * t;
    hit.normal = NormalizeSafe(cross(e1,e2));
    hit.cube = &cube;
    hit.triangleID = localTriangleId;
    hit.lodIndex = lodIndex;
    return true;
}

bool OfflineRender::TraceCut(const std::vector<RenderCell> &cut, const Ray &ray, HitRecord &hit) const
{
    bool anyHit = false;

    for (const RenderCell &cell : cut)
    {
        if (cell.lodIndex < 0 || model.lods[cell.lodIndex] == nullptr)
        {
            continue;
        }

        const LOD& lod = *model.lods[cell.lodIndex];
        const auto it = lod.cubeTable.find(cell.coord);
        if (it == lod.cubeTable.end())
        {
            continue;
        }

        anyHit |= TraceCube(cell.lodIndex, it->second, ray, hit);
    }

    return anyHit;
}

bool OfflineRender::TraceCube(int lodIndex, const Cube &cube, const Ray &ray, HitRecord &hit) const
{
    float tNear = 0.0f;
    float tFar = 0.0f;

    if (!IntersectAABB(cube, ray, tNear, tFar))
    {
        return false;
    }

    if (tNear >= hit.distance)
    {
        return false;
    }

    bool anyHit = false;
    for (uint32_t triangleId = 0; triangleId < static_cast<uint32_t>(cube.triangleCount); ++triangleId)
    {
        anyHit |= TraceTriangle(lodIndex, cube, triangleId, ray, hit);
    }

    return anyHit;
}

bool OfflineRender::TraceRay(int lodIndex, const Ray &ray, HitRecord &hit) const
{
    if (lodIndex < 0 || model.lods[lodIndex] == nullptr)
    {
        return false;
    }

    bool anyHit = false;
    const LOD &lod = *model.lods[lodIndex];

    for (const auto &entry : lod.cubeTable)
    {
        const Cube &cube = entry.second;
        anyHit |= TraceCube(lodIndex, cube, ray, hit);
    }

    return anyHit;
}

void OfflineRender::ShadeDebug(const HitRecord &hit, DebugMode mode, unsigned char rgb[3]) const
{
    if (!hit.hit || hit.cube == nullptr)
    {
        rgb[0] = 0;
        rgb[1] = 0;
        rgb[2] = 0;
        return;
    }

    if (mode == DebugMode::Normal)
    {
        rgb[0] = ToByte(0.5f * (hit.normal.x + 1.0f));
        rgb[1] = ToByte(0.5f * (hit.normal.y + 1.0f));
        rgb[2] = ToByte(0.5f * (hit.normal.z + 1.0f));
        return;
    }

    if (mode == DebugMode::Depth)
    {
        const float d = std::log2(1.0f + std::max(hit.distance, 0.0f));
        const float gray = 1.0f/(1.0f + d);
        rgb[0] = ToByte(gray);
        rgb[1] = ToByte(gray);
        rgb[2] = ToByte(gray);
        return;
    }

    if (mode == DebugMode::LodLevel)
    {
        // Different color for different LODs
        static const unsigned char palette[][3] = {
            {230, 80, 80},
            {80, 190, 110},
            {80, 130, 230},
            {230, 190, 70},
            {180, 90, 220},
            {70, 200, 210},
            {230, 120, 60},
            {150, 150, 150}};

        constexpr int paletteSize = sizeof(palette) / sizeof(palette[0]);
        const int index = hit.lodIndex >= 0 ? hit.lodIndex % paletteSize : 0;
        rgb[0] = palette[index][0];
        rgb[1] = palette[index][1];
        rgb[2] = palette[index][2];
        return;
    }

    uint64_t key = hit.cube->coord64;
    key ^= static_cast<uint64_t>(hit.lodIndex + 1) * 0x9e3779b97f4a7c15ULL;
    if(mode == DebugMode::TriangleID){
        key^= (static_cast<uint64_t>(hit.triangleID) + 0x9e3779b97f4a7c15ULL);
    }

    const uint32_t h = Hash32(key);
    rgb[0] = static_cast<unsigned char>(64 + (h & 0xBF));
    rgb[1] = static_cast<unsigned char>(64 + ((h >> 8) & 0xBF));
    rgb[2] = static_cast<unsigned char>(64 + ((h >> 16) & 0xBF));
}

bool OfflineRender::RenderImpl(const Camera& camera, int width, int height, const std::string& filename, DebugMode mode, const std::vector<RenderCell>* cut, int fixedLodIndex) const
{
    if (width <= 0 || height <= 0)
    {
        std::cerr<<"OfflineRender: invalid image size.\n";
        return false;
    }

    if (cut == nullptr && (fixedLodIndex < 0 || model.lods[fixedLodIndex] == nullptr))
    {
        std::cerr << "OfflineRender: invalid LOD index " << fixedLodIndex << ".\n";
        return false;
    }

    if (cut != nullptr && cut->empty())
    {
        std::cerr << "OfflineRender: adaptive cut is empty. \n";
        return false;
    }

    std::vector<unsigned char> pixels(static_cast<size_t>(width) * static_cast<size_t>(height) * 3, 0);

    const Vec3 origin = camera.get_position();

    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(width);
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(height);

            Ray ray;
            ray.start = origin;
            ray.dir = MakePrimaryRayDirection(camera, u, v);

            HitRecord hit;
            if (cut != nullptr)
            {
                TraceCut(*cut, ray, hit);
            }
            else
            {
                TraceRay(fixedLodIndex, ray, hit);
            }

            unsigned char rgb[3];
            ShadeDebug(hit, mode, rgb);

            // PPM is stored top-to-bottom here. If you want exact OpenGL
            // framebuffer orientation, swap y with height - 1 - y.
            const size_t pixelOffset =
                3 * (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x));

            pixels[pixelOffset + 0] = rgb[0];
            pixels[pixelOffset + 1] = rgb[1];
            pixels[pixelOffset + 2] = rgb[2];
        }

        if ((y % 32) == 0)
        {
            std::cout << "\rOffline ray tracing: "
                      << (100 * y / height) << "%" << std::flush;
        }
    }

    std::cout << "\rOffline ray tracing: 100%\n";

    std::ofstream out(filename, std::ios::binary);
    if (!out)
    {
        std::cerr << "OfflineRender: cannot open output file: " << filename << "\n";
        return false;
    }

    out << "P6\n"
        << width << " " << height << "\n255\n";
    out.write(
        reinterpret_cast<const char *>(pixels.data()),
        static_cast<std::streamsize>(pixels.size()));

    return static_cast<bool>(out);
}

bool OfflineRender::RenderLOD(int lodIndex, const Camera &camera, int width, int height, const std::string &filename, DebugMode mode) const
{
    return RenderImpl(camera, width, height, filename, mode, nullptr, lodIndex);
}

bool OfflineRender::RenderCut(const std::vector<RenderCell> &cut, const Camera &camera, int width, int height, const std::string &filename, DebugMode mode) const
{
    return RenderImpl(camera, width, height, filename, mode, &cut, -1);
}
