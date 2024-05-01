#include "PathfinderPCH.h"
#include "MeshManager.h"

#include "Core/Intrinsics.h"
#include "Renderer/Renderer.h"

namespace Pathfinder
{

AABB MeshManager::GenerateAABB(const std::vector<MeshPositionVertex>& points)
{
#if INVESTIGATE
    if (AVX2Supported())
    {
        size_t i      = 0;
        __m256 minVec = _mm256_set1_ps(FLT_MAX);
        __m256 maxVec = _mm256_set1_ps(FLT_MIN);

        const size_t alignedDataSize = (points.size() & ~7);
        for (i = 0; i < alignedDataSize; i += 8)
        {
            const auto* pointPtr = &points[i];

            const __m256 pointX =
                _mm256_set_ps(pointPtr[7].Position.x, pointPtr[6].Position.x, pointPtr[5].Position.x, pointPtr[4].Position.x,
                              pointPtr[3].Position.x, pointPtr[2].Position.x, pointPtr[1].Position.x, pointPtr[0].Position.x);
            const __m256 pointY =
                _mm256_set_ps(pointPtr[7].Position.y, pointPtr[6].Position.y, pointPtr[5].Position.y, pointPtr[4].Position.y,
                              pointPtr[3].Position.y, pointPtr[2].Position.y, pointPtr[1].Position.y, pointPtr[0].Position.y);
            const __m256 pointZ =
                _mm256_set_ps(pointPtr[7].Position.z, pointPtr[6].Position.z, pointPtr[5].Position.z, pointPtr[4].Position.z,
                              pointPtr[3].Position.z, pointPtr[2].Position.z, pointPtr[1].Position.z, pointPtr[0].Position.z);

            minVec = _mm256_min_ps(minVec, _mm256_min_ps(_mm256_min_ps(pointX, pointY), pointZ));
            maxVec = _mm256_max_ps(maxVec, _mm256_max_ps(_mm256_max_ps(pointX, pointY), pointZ));
        }

        alignas(32) float minResults[8] = {0};
        alignas(32) float maxResults[8] = {0};

        _mm256_storeu_ps(minResults, minVec);
        _mm256_storeu_ps(maxResults, maxVec);

        glm::vec3 min(minResults[0], minResults[1], minResults[2]);
        glm::vec3 max(maxResults[0], maxResults[1], maxResults[2]);

        // Take into account the remainder.
        for (i = alignedDataSize; i < points.size(); ++i)
        {
            min = glm::min(points[i].Position, min);
            max = glm::max(points[i].Position, max);
        }

        const glm::vec3 center = (max + min) * 0.5f;
        return {center, max - center};
    }
#endif

    glm::vec3 min = glm::vec3(FLT_MAX);
    glm::vec3 max = glm::vec3(FLT_MIN);
    for (const auto& point : points)
    {
        min = glm::min(point.Position, min);
        max = glm::max(point.Position, max);
    }

    const glm::vec3 center = (max + min) * .5f;
    return {center, max - center};
}

Sphere MeshManager::GenerateBoundingSphere(const std::vector<MeshPositionVertex>& points)
{
    glm::vec3 farthestVtx[2] = {points[0].Position, points[0].Position};
    glm::vec3 averagedVertexPos(0.0f);

    // First pass - find averaged vertex pos.
    for (const auto& point : points)
        averagedVertexPos += point.Position;

    averagedVertexPos /= points.size();
    const auto aabb = GenerateAABB(points);

    // Second pass - find farthest vertices for both averaged vertex position and AABB centroid.
    for (const auto& point : points)
    {
        if (glm::distance2(averagedVertexPos, point.Position) > glm::distance2(averagedVertexPos, farthestVtx[0]))
            farthestVtx[0] = point.Position;
        if (glm::distance2(aabb.Center, point.Position) > glm::distance2(aabb.Center, farthestVtx[1])) farthestVtx[1] = point.Position;
    }

    const float averagedVtxToFarthestDistance  = glm::distance(farthestVtx[0], averagedVertexPos);
    const float aabbCentroidToFarthestDistance = glm::distance(farthestVtx[1], aabb.Center);

    Sphere sphere = {};
    sphere.Center = averagedVtxToFarthestDistance < aabbCentroidToFarthestDistance ? averagedVertexPos : aabb.Center;
    sphere.Radius = glm::min(averagedVtxToFarthestDistance, aabbCentroidToFarthestDistance);

    return sphere;
}

void MeshManager::OptimizeMesh(const std::vector<uint32_t>& srcIndices, const std::vector<MeshoptimizeVertex>& srcVertices,
                               std::vector<uint32_t>& outIndices, std::vector<MeshoptimizeVertex>& outVertices)
{
    // #1 INDEXING
    std::vector<uint32_t> remappedIndices(srcIndices.size());
    const size_t remappedVertexCount = meshopt_generateVertexRemap(remappedIndices.data(), srcIndices.data(), srcIndices.size(),
                                                                   srcVertices.data(), srcVertices.size(), sizeof(srcVertices[0]));

    outIndices.resize(remappedIndices.size());
    meshopt_remapIndexBuffer(outIndices.data(), srcIndices.data(), remappedIndices.size(), remappedIndices.data());

    outVertices.resize(remappedVertexCount);
    meshopt_remapVertexBuffer(outVertices.data(), srcVertices.data(), srcVertices.size(), sizeof(srcVertices[0]), remappedIndices.data());

    // #2 VERTEX CACHE
    meshopt_optimizeVertexCache(outIndices.data(), outIndices.data(), outIndices.size(), outVertices.size());

    // #3 OVERDRAW
    meshopt_optimizeOverdraw(outIndices.data(), outIndices.data(), outIndices.size(), &outVertices[0].Position.x, outVertices.size(),
                             sizeof(outVertices[0]),
                             1.05f);  // max 5% vertex cache hit ratio can be worse then before

    // NOTE: I don't think I rly need to call these funcs twice since I'm using 2 vertex streams, cuz I've already merged them in
    // MeshoptimizeVertex and then I unmerge them xd
    // #4 VERTEX FETCH
    meshopt_optimizeVertexFetch(outVertices.data(), outIndices.data(), outIndices.size(), outVertices.data(), outVertices.size(),
                                sizeof(outVertices[0]));
}

void MeshManager::BuildMeshlets(const std::vector<uint32_t>& indices, const std::vector<MeshPositionVertex>& vertexPositions,
                                std::vector<Meshlet>& outMeshlets, std::vector<uint32_t>& outMeshletVertices,
                                std::vector<uint8_t>& outMeshletTriangles)
{
    const size_t maxMeshletCount = meshopt_buildMeshletsBound(indices.size(), MAX_MESHLET_VERTEX_COUNT, MAX_MESHLET_TRIANGLE_COUNT);
    std::vector<meshopt_Meshlet> meshopt_meshlets(maxMeshletCount);
    outMeshletVertices.resize(maxMeshletCount * MAX_MESHLET_VERTEX_COUNT);
    outMeshletTriangles.resize(maxMeshletCount * MAX_MESHLET_TRIANGLE_COUNT * 3);

    const size_t actualMeshletCount =
        meshopt_buildMeshlets(meshopt_meshlets.data(), outMeshletVertices.data(), outMeshletTriangles.data(), indices.data(),
                              indices.size(), &vertexPositions[0].Position.x, vertexPositions.size(), sizeof(vertexPositions[0]),
                              MAX_MESHLET_VERTEX_COUNT, MAX_MESHLET_TRIANGLE_COUNT, MESHLET_CONE_WEIGHT);

    {
        // Trimming
        const meshopt_Meshlet& last = meshopt_meshlets[actualMeshletCount - 1];
        outMeshletVertices.resize(last.vertex_offset + last.vertex_count);
        outMeshletVertices.shrink_to_fit();

        outMeshletTriangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
        outMeshletTriangles.shrink_to_fit();

        meshopt_meshlets.resize(actualMeshletCount);
        meshopt_meshlets.shrink_to_fit();
    }

    // For optimal performance, it is recommended to further optimize each meshlet in isolation for better triangle and vertex locality
    for (size_t i{}; i < meshopt_meshlets.size(); ++i)
    {
        meshopt_optimizeMeshlet(&outMeshletVertices[meshopt_meshlets[i].vertex_offset],
                                &outMeshletTriangles[meshopt_meshlets[i].triangle_offset], meshopt_meshlets[i].triangle_count,
                                meshopt_meshlets[i].vertex_count);
    }

    outMeshlets.resize(meshopt_meshlets.size());
    for (size_t i = 0; i < actualMeshletCount; ++i)
    {
        const auto& meshopt_m       = meshopt_meshlets[i];
        const meshopt_Bounds bounds = meshopt_computeMeshletBounds(
            &outMeshletVertices[meshopt_m.vertex_offset], &outMeshletTriangles[meshopt_m.triangle_offset], meshopt_m.triangle_count,
            &vertexPositions[0].Position.x, vertexPositions.size(), sizeof(vertexPositions[0]));

        auto& m          = outMeshlets[i];
        m.vertexOffset   = meshopt_m.vertex_offset;
        m.vertexCount    = meshopt_m.vertex_count;
        m.triangleOffset = meshopt_m.triangle_offset;
        m.triangleCount  = meshopt_m.triangle_count;

        m.center = glm::vec3(bounds.center[0], bounds.center[1], bounds.center[2]);
        m.radius = bounds.radius;

        m.coneApex   = glm::vec3(bounds.cone_apex[0], bounds.cone_apex[1], bounds.cone_apex[2]);
        m.coneAxis   = glm::vec3(bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2]);
        m.coneCutoff = bounds.cone_cutoff;
    }
}

}  // namespace Pathfinder