#include "PathfinderPCH.h"
#include "Mesh.h"

#include "Buffer.h"
#include "Material.h"
#include "Image.h"
#include "Texture2D.h"
#include "Renderer.h"
#include "BindlessRenderer.h"
#include "RayTracingBuilder.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <meshoptimizer.h>

namespace Pathfinder
{

namespace FastGLTFUtils
{

static ESamplerFilter SamplerFilterToPathfinder(const fastgltf::Filter filter)
{
    switch (filter)
    {
        case fastgltf::Filter::Nearest: return ESamplerFilter::SAMPLER_FILTER_NEAREST;
        case fastgltf::Filter::Linear: return ESamplerFilter::SAMPLER_FILTER_LINEAR;
        default: LOG_TAG_WARN(FASTGLTF, "Other sampler filters aren't supported!"); return ESamplerFilter::SAMPLER_FILTER_NEAREST;
    }

    PFR_ASSERT(false, "Unknown fastgltf::filter!");
    return ESamplerFilter::SAMPLER_FILTER_LINEAR;
}

static ESamplerWrap SamplerWrapToPathfinder(const fastgltf::Wrap wrap)
{
    switch (wrap)
    {
        case fastgltf::Wrap::ClampToEdge: return ESamplerWrap::SAMPLER_WRAP_CLAMP_TO_EDGE;
        case fastgltf::Wrap::MirroredRepeat: return ESamplerWrap::SAMPLER_WRAP_MIRRORED_REPEAT;
        case fastgltf::Wrap::Repeat: return ESamplerWrap::SAMPLER_WRAP_REPEAT;
        default: LOG_TAG_WARN(FASTGLTF, "Other sampler wraps aren't supported!"); return ESamplerWrap::SAMPLER_WRAP_REPEAT;
    }

    PFR_ASSERT(false, "Unknown fastgltf::wrap!");
    return ESamplerWrap::SAMPLER_WRAP_REPEAT;
}

static Shared<Texture2D> LoadTexture(std::unordered_map<std::string, Shared<Texture2D>>& loadedTextures, const std::string& meshDir,
                                     const fastgltf::Asset& asset, const fastgltf::Material& materialAccessor, const size_t textureIndex,
                                     TextureSpecification& textureSpec)
{
    const auto& fastgltfTexture = asset.textures[textureIndex];
    const auto imageIndex       = fastgltfTexture.imageIndex;
    PFR_ASSERT(imageIndex.has_value(), "Invalid image index!");

    if (fastgltfTexture.samplerIndex.has_value())
    {
        const auto& fastgltfTextureSampler = asset.samplers[fastgltfTexture.samplerIndex.value()];

        if (fastgltfTextureSampler.magFilter.has_value())
            textureSpec.Filter = SamplerFilterToPathfinder(fastgltfTextureSampler.magFilter.value());

        textureSpec.Wrap = SamplerWrapToPathfinder(fastgltfTextureSampler.wrapS);
    }

    const auto& imageData = asset.images[imageIndex.value()].data;

    PFR_ASSERT(std::holds_alternative<fastgltf::sources::URI>(imageData), "Texture hasn't path!");
    const auto& fastgltfURI = std::get<fastgltf::sources::URI>(imageData);

    const std::string textureName = fastgltfURI.uri.fspath().stem().string();
    PFR_ASSERT(textureName.data(), "Texture has no name!");

    if (loadedTextures.contains(textureName)) return loadedTextures[textureName];

    Shared<Texture2D> texture = nullptr;
    std::visit(fastgltf::visitor{[](auto& arg) {},
                                 [&](const fastgltf::sources::URI& uri)
                                 {
                                     int32_t x = 1, y = 1, channels = 4;
                                     const std::string texturePath = meshDir + std::string(uri.uri.string());
                                     void* data       = ImageUtils::LoadRawImage(texturePath, textureSpec.bFlipOnLoad, &x, &y, &channels);
                                     textureSpec.Data = data;
                                     textureSpec.DataSize = static_cast<size_t>(x) * static_cast<size_t>(y) * channels;
                                     textureSpec.Width    = x;
                                     textureSpec.Height   = y;

                                     texture = Texture2D::Create(textureSpec);
                                     ImageUtils::UnloadRawImage(data);
                                 }},
               imageData);

    loadedTextures[textureName] = texture;
    return texture;
}

}  // namespace FastGLTFUtils

namespace MeshPreprocessorUtils
{

// TODO: SIMDify it?
static AABB GenerateAABB(const std::vector<MeshPositionVertex>& points)
{
    glm::vec3 min = glm::vec3(.0f);
    glm::vec3 max = glm::vec3(.0f);
    for (const auto& point : points)
    {
        min = glm::min(point.Position, min);
        max = glm::max(point.Position, max);
    }

    const glm::vec3 center = (max + min) * .5f;
    return {center, max - center};
}

static Sphere GenerateBoundingSphere(const std::vector<MeshPositionVertex>& points)
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

struct MeshoptimizeVertex
{
    glm::vec3 Position = glm::vec3(0.0f);
    glm::vec4 Color    = glm::vec4(1.0f);
    glm::vec3 Normal   = glm::vec3(0.0f);
    glm::vec3 Tangent  = glm::vec3(0.0f);
    glm::vec2 UV       = glm::vec2(0.0f);
};

static void OptimizeMesh(const std::vector<uint32_t>& srcIndices, const std::vector<MeshoptimizeVertex>& srcVertices,
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

static void BuildMeshlets(const std::vector<uint32_t>& indices, const std::vector<MeshPositionVertex>& vertexPositions,
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

        m.center[0] = bounds.center[0];
        m.center[1] = bounds.center[1];
        m.center[2] = bounds.center[2];

        m.coneAxis[0] = bounds.cone_axis[0];
        m.coneAxis[1] = bounds.cone_axis[1];
        m.coneAxis[2] = bounds.cone_axis[2];

        m.coneCutoff = bounds.cone_cutoff;
    }
}
}  // namespace MeshPreprocessorUtils

Mesh::Mesh(const std::string& meshPath)
{
    // Optimally, you should reuse Parser instance across loads, but don't use it across threads.
    thread_local fastgltf::Parser parser;

    Timer t = {};
    fastgltf::GltfDataBuffer data;
    PFR_ASSERT(data.loadFromFile(meshPath), "Failed to load  fastgltf::GltfDataBuffer!");

    const auto gltfType = fastgltf::determineGltfFileType(&data);
    if (gltfType == fastgltf::GltfType::Invalid) PFR_ASSERT(false, "Failed to parse gltf!");

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | /* fastgltf::Options::AllowDouble |
                                                                                   */
                                 fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers |
                                 fastgltf::Options::GenerateMeshIndices;

    std::filesystem::path fsMeshPath(meshPath);
    fastgltf::Expected<fastgltf::Asset> asset = parser.loadGltf(&data, fsMeshPath.parent_path(), gltfOptions);
    if (const auto error = asset.error(); error != fastgltf::Error::None)
    {
        LOG_TAG_ERROR(FASTGLTF, "Error occured while loading mesh \"%s\"! Error name: %s / Message: %s", meshPath.data(),
                      fastgltf::getErrorName(error).data(), fastgltf::getErrorMessage(error).data());
        PFR_ASSERT(false, "Failed to load mesh!");
    }
    LOG_TAG_INFO(FASTGLTF, "\"%s\" has (%zu) buffers, (%zu) textures, (%zu) animations, (%zu) materials, (%zu) meshes.", meshPath.data(),
                 asset->buffers.size(), asset->animations.size(), asset->textures.size(), asset->materials.size(), asset->meshes.size());

    std::string meshDir = fsMeshPath.parent_path().string() + "/";
    std::unordered_map<std::string, Shared<Texture2D>> loadedTextures;
    PFR_ASSERT(meshDir.length() > 1, "Mesh directory path invalid!");
    for (size_t meshIndex = 0; meshIndex < asset->meshes.size(); ++meshIndex)
    {
        LoadSubmeshes(meshDir, loadedTextures, asset.get(), meshIndex);
    }

#if TODO
    if (Renderer::GetRendererSettings().bRTXSupport)
    {
        m_BLASes = RayTracingBuilder::BuildBLASes(m_Submeshes);
        m_TLAS   = RayTracingBuilder::BuildTLAS(m_BLASes);
    }
#endif

#if PFR_DEBUG
    PFR_ASSERT(fastgltf::validate(asset.get()) == fastgltf::Error::None, "Asset is not valid after processing?");
#endif

    LOG_TAG_INFO(FASTGLTF, "Time taken to load and create mesh - \"%s\": (%0.5f) seconds.", meshPath.data(), t.GetElapsedSeconds());
}

Shared<Mesh> Mesh::Create(const std::string& meshPath)
{
    return MakeShared<Mesh>(meshPath);
}

void Mesh::Destroy()
{
#if TODO
    if (Renderer::GetRendererSettings().bRTXSupport)
    {
        RayTracingBuilder::DestroyAccelerationStructure(m_TLAS);
        for (auto& blas : m_BLASes)
            RayTracingBuilder::DestroyAccelerationStructure(blas);
    }
#endif

    m_Submeshes.clear();
}

void Mesh::LoadSubmeshes(const std::string& meshDir, std::unordered_map<std::string, Shared<Texture2D>>& loadedTextures,
                         const fastgltf::Asset& asset, const size_t meshIndex)
{
    fastgltf::Node fastGLTFnode = {};
    for (auto& node : asset.nodes)
    {
        if (node.meshIndex.has_value() && meshIndex == node.meshIndex.value())
        {
            fastGLTFnode = node;
            break;
        }
    }

    for (const auto& p : asset.meshes[meshIndex].primitives)
    {
        const auto positionIt = p.findAttribute("POSITION");
        PFR_ASSERT(positionIt != p.attributes.end(), "Mesh doesn't contain positions?!");
        PFR_ASSERT(p.indicesAccessor.has_value(), "Non-indexed geometry is not supported!");

        // INDICES
        const auto& indicesAccessor = asset.accessors[p.indicesAccessor.value()];
        std::vector<uint32_t> indices(indicesAccessor.count);
        fastgltf::iterateAccessorWithIndex<std::uint32_t>(asset, indicesAccessor,
                                                          [&](uint32_t index, std::size_t idx) { indices[idx] = index; });

        // POSITION
        const auto& positionAccessor = asset.accessors[positionIt->second];
        PFR_ASSERT(positionAccessor.type == fastgltf::AccessorType::Vec3, "Positions can only contain vec3!");

        std::vector<MeshPreprocessorUtils::MeshoptimizeVertex> meshoptimizeVertices(positionAccessor.count);
        {
            // Safety checks
            const auto& bufferView = asset.bufferViews[positionAccessor.bufferViewIndex.value()];
            const auto& bufferData = asset.buffers[bufferView.bufferIndex].data;
            PFR_ASSERT(std::holds_alternative<fastgltf::sources::Vector>(bufferData), "Only vector parsing support for now!");
        }

        glm::mat4 localTransform = glm::mat4(1.f);
        std::visit(fastgltf::visitor{[&](const fastgltf::Node::TransformMatrix& matrix)
                                     { memcpy(&localTransform, matrix.data(), sizeof(matrix)); },
                                     [&](const fastgltf::Node::TRS& transform)
                                     {
                                         const glm::vec3 tl(transform.translation[0], transform.translation[1], transform.translation[2]);
                                         const glm::quat rot(transform.rotation[3], transform.rotation[0], transform.rotation[1],
                                                             transform.rotation[2]);
                                         const glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                                         const glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                                         const glm::mat4 rm = glm::toMat4(rot);
                                         const glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                                         localTransform = tm * rm * sm;
                                     }},
                   fastGLTFnode.transform);

        fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, positionAccessor,
                                                      [&](const glm::vec3& position, std::size_t idx)
                                                      { meshoptimizeVertices[idx].Position = localTransform * vec4(position, 1.0); });

        // NORMAL
        const auto& normalIt = p.findAttribute("NORMAL");
        if (normalIt != p.attributes.end())
        {
            fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[normalIt->second],
                                                          [&](const glm::vec3& normal, std::size_t idx)
                                                          { meshoptimizeVertices[idx].Normal = normal; });
        }

        // TANGENT
        const auto& tangentIt = p.findAttribute("TANGENT");
        if (tangentIt != p.attributes.end())
        {
            fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[tangentIt->second],
                                                          [&](const glm::vec4& tangent, std::size_t idx)
                                                          { meshoptimizeVertices[idx].Tangent = tangent; });
        }

        // COLOR_0
        const auto& color_0_It = p.findAttribute("COLOR_0");
        if (color_0_It != p.attributes.end())
        {
            fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[color_0_It->second],
                                                          [&](const glm::vec4& color, std::size_t idx)
                                                          { meshoptimizeVertices[idx].Color = color; });
        }

        // UV
        const auto& uvIT = p.findAttribute("TEXCOORD_0");
        if (uvIT != p.attributes.end())
        {
            fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, asset.accessors[uvIT->second],
                                                          [&](const glm::vec2& uv, std::size_t idx) { meshoptimizeVertices[idx].UV = uv; });
        }

        auto& submesh = m_Submeshes.emplace_back(MakeShared<Submesh>());

        // PBR shading model materials
        if (p.materialIndex.has_value())
        {
            const auto& materialAccessor = asset.materials[p.materialIndex.value()];

            // Assemble gathered textures.
            PBRData pbrData   = {};
            pbrData.BaseColor = glm::make_vec4(materialAccessor.pbrData.baseColorFactor.data());
            pbrData.Metallic  = materialAccessor.pbrData.metallicFactor;
            pbrData.Roughness = materialAccessor.pbrData.roughnessFactor;
            pbrData.bIsOpaque = materialAccessor.alphaMode == fastgltf::AlphaMode::Opaque;

            Shared<Texture2D> albedo = nullptr;
            if (materialAccessor.pbrData.baseColorTexture.has_value())
            {
                TextureSpecification albedoTextureSpec = {};
                albedoTextureSpec.bBindlessUsage       = true;

                const auto& textureInfo = materialAccessor.pbrData.baseColorTexture.value();
                albedo = FastGLTFUtils::LoadTexture(loadedTextures, meshDir, asset, materialAccessor, textureInfo.textureIndex,
                                                    albedoTextureSpec);
                pbrData.AlbedoTextureIndex = albedo->GetBindlessIndex();
            }

            Shared<Texture2D> normalMap = nullptr;
            if (materialAccessor.normalTexture.has_value())
            {
                TextureSpecification normalMapTextureSpec = {};
                normalMapTextureSpec.bBindlessUsage       = true;

                const auto& textureInfo = materialAccessor.normalTexture.value();
                normalMap = FastGLTFUtils::LoadTexture(loadedTextures, meshDir, asset, materialAccessor, textureInfo.textureIndex,
                                                       normalMapTextureSpec);
                pbrData.NormalTextureIndex = normalMap->GetBindlessIndex();
            }

            Shared<Texture2D> metallicRoughness = nullptr;
            if (materialAccessor.pbrData.metallicRoughnessTexture.has_value())
            {
                TextureSpecification metallicRoughnessTextureSpec = {};
                metallicRoughnessTextureSpec.bBindlessUsage       = true;

                const auto& textureInfo = materialAccessor.pbrData.metallicRoughnessTexture.value();
                metallicRoughness = FastGLTFUtils::LoadTexture(loadedTextures, meshDir, asset, materialAccessor, textureInfo.textureIndex,
                                                               metallicRoughnessTextureSpec);
                pbrData.MetallicRoughnessTextureIndex = metallicRoughness->GetBindlessIndex();
            }

            Shared<Texture2D> emissiveMap = nullptr;
            if (materialAccessor.emissiveTexture.has_value())
            {
                TextureSpecification emissiveTextureSpec = {};
                emissiveTextureSpec.bBindlessUsage       = true;

                const auto& textureInfo = materialAccessor.emissiveTexture.value();
                emissiveMap = FastGLTFUtils::LoadTexture(loadedTextures, meshDir, asset, materialAccessor, textureInfo.textureIndex,
                                                         emissiveTextureSpec);
                pbrData.EmissiveTextureIndex = emissiveMap->GetBindlessIndex();
            }

            Shared<Texture2D> occlusionMap = nullptr;
            if (materialAccessor.occlusionTexture.has_value())
            {
                TextureSpecification occlusionTextureSpec = {};
                occlusionTextureSpec.bBindlessUsage       = true;

                const auto& textureInfo = materialAccessor.occlusionTexture.value();
                occlusionMap = FastGLTFUtils::LoadTexture(loadedTextures, meshDir, asset, materialAccessor, textureInfo.textureIndex,
                                                          occlusionTextureSpec);
                pbrData.OcclusionTextureIndex = occlusionMap->GetBindlessIndex();
            }

            submesh->SetMaterial(MakeShared<Material>(pbrData));

            const auto& mat = submesh->GetMaterial();
            mat->SetAlbedo(albedo);
            mat->SetNormalMap(normalMap);
            mat->SetEmissiveMap(emissiveMap);
            mat->SetMetallicRoughnessMap(metallicRoughness);
            mat->SetOcclusionMap(occlusionMap);
        }

        std::vector<uint32_t> finalIndices;
        std::vector<MeshPreprocessorUtils::MeshoptimizeVertex> finalVertices;
        MeshPreprocessorUtils::OptimizeMesh(indices, meshoptimizeVertices, finalIndices, finalVertices);

        BufferSpecification ibSpec = {EBufferUsage::BUFFER_USAGE_INDEX};
        ibSpec.Data                = finalIndices.data();
        ibSpec.DataSize            = finalIndices.size() * sizeof(finalIndices[0]);
        if (Renderer::GetRendererSettings().bRTXSupport)
        {
            ibSpec.BufferBinding  = STORAGE_BUFFER_INDEX_BUFFER_BINDING;
            ibSpec.bBindlessUsage = true;
            ibSpec.BufferUsage |= EBufferUsage::BUFFER_USAGE_STORAGE |
                                  EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY |
                                  EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS;
        }
        submesh->m_IndexBuffer = Buffer::Create(ibSpec);

        std::vector<MeshPositionVertex> vertexPositions(finalVertices.size());
        std::vector<MeshAttributeVertex> vertexAttributes(finalVertices.size());
        for (size_t i = 0; i < finalVertices.size(); ++i)
        {
            const auto& finalVertex = finalVertices[i];

            vertexPositions[i].Position = finalVertex.Position;

            vertexAttributes[i].Color   = finalVertex.Color;
            vertexAttributes[i].Normal  = finalVertex.Normal;
            vertexAttributes[i].Tangent = finalVertex.Tangent;
            vertexAttributes[i].UV      = finalVertex.UV;
        }
        indices.clear();
        meshoptimizeVertices.clear();

        BufferSpecification vbPosSpec = {EBufferUsage::BUFFER_USAGE_VERTEX | EBufferUsage::BUFFER_USAGE_STORAGE,
                                         STORAGE_BUFFER_VERTEX_POS_BINDING, true};
        vbPosSpec.Data                = vertexPositions.data();
        vbPosSpec.DataSize            = vertexPositions.size() * sizeof(vertexPositions[0]);
        if (Renderer::GetRendererSettings().bRTXSupport)
        {
            vbPosSpec.BufferUsage |=
                EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS;
        }
        submesh->m_VertexPositionBuffer = Buffer::Create(vbPosSpec);

        BufferSpecification vbAttribSpec = {EBufferUsage::BUFFER_USAGE_VERTEX | EBufferUsage::BUFFER_USAGE_STORAGE,
                                            STORAGE_BUFFER_VERTEX_ATTRIB_BINDING, true};
        vbAttribSpec.Data                = vertexAttributes.data();
        vbAttribSpec.DataSize            = vertexAttributes.size() * sizeof(vertexAttributes[0]);
        if (Renderer::GetRendererSettings().bRTXSupport)
        {
            vbAttribSpec.BufferUsage |=
                EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS;
        }
        submesh->m_VertexAttributeBuffer = Buffer::Create(vbAttribSpec);

        submesh->m_BoundingSphere = MeshPreprocessorUtils::GenerateBoundingSphere(vertexPositions);

        if (Renderer::GetRendererSettings().bMeshShadingSupport)
        {
            std::vector<uint32_t> meshletVertices;
            std::vector<uint8_t> meshletTriangles;
            std::vector<Meshlet> meshlets;
            MeshPreprocessorUtils::BuildMeshlets(finalIndices, vertexPositions, meshlets, meshletVertices, meshletTriangles);

            BufferSpecification mbSpec = {EBufferUsage::BUFFER_USAGE_STORAGE, STORAGE_BUFFER_MESHLET_BINDING, true};
            mbSpec.Data                = meshlets.data();
            mbSpec.DataSize            = meshlets.size() * sizeof(meshlets[0]);
            submesh->m_MeshletBuffer   = Buffer::Create(mbSpec);

            BufferSpecification mbVertSpec   = {EBufferUsage::BUFFER_USAGE_STORAGE, STORAGE_BUFFER_MESHLET_VERTEX_BINDING, true};
            mbVertSpec.Data                  = meshletVertices.data();
            mbVertSpec.DataSize              = meshletVertices.size() * sizeof(meshletVertices[0]);
            submesh->m_MeshletVerticesBuffer = Buffer::Create(mbVertSpec);

            BufferSpecification mbTriSpec     = {EBufferUsage::BUFFER_USAGE_STORAGE, STORAGE_BUFFER_MESHLET_TRIANGLE_BINDING, true};
            mbTriSpec.Data                    = meshletTriangles.data();
            mbTriSpec.DataSize                = meshletTriangles.size() * sizeof(meshletTriangles[0]);
            submesh->m_MeshletTrianglesBuffer = Buffer::Create(mbTriSpec);
        }
    }
}

}  // namespace Pathfinder