#include "PathfinderPCH.h"
#include "Mesh.h"

#include "Buffer.h"
#include "Material.h"
#include "Image.h"
#include "Texture2D.h"
#include "RendererCoreDefines.h"
#include "Renderer.h"
#include "BindlessRenderer.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

#include <meshoptimizer.h>

namespace Pathfinder
{

namespace FasgtlfUtils
{

static ESamplerFilter FastgltfSamplerFilterToPathfinder(const fastgltf::Filter filter)
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

static ESamplerWrap FastgltfSamplerWrapToPathfinder(const fastgltf::Wrap wrap)
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

static Shared<Texture2D> FastgltfLoadTexture(const fastgltf::Asset& asset, const fastgltf::Material& materialAccessor,
                                             TextureSpecification& textureSpec)
{
    const auto textureIndex     = materialAccessor.pbrData.baseColorTexture->textureIndex;
    const auto& fastgltfTexture = asset.textures[textureIndex];
    const auto imageIndex       = fastgltfTexture.imageIndex;
    PFR_ASSERT(imageIndex.has_value(), "Invalid image index!");

    if (fastgltfTexture.samplerIndex.has_value())
    {
        const auto& fastgltfTextureSampler = asset.samplers[fastgltfTexture.samplerIndex.value()];

        if (fastgltfTextureSampler.magFilter.has_value())
            textureSpec.Filter = FastgltfSamplerFilterToPathfinder(fastgltfTextureSampler.magFilter.value());

        textureSpec.Wrap = FastgltfSamplerWrapToPathfinder(fastgltfTextureSampler.wrapS);
    }

    Shared<Texture2D> texture = nullptr;
    const auto& imageData     = asset.images[imageIndex.value()].data;
    std::visit(fastgltf::visitor{[](auto& arg) {},
                                 [&](const fastgltf::sources::URI& uri)
                                 {
                                     int32_t x = 1, y = 1, channels = 4;
                                     void* data           = ImageUtils::LoadRawImage(uri.uri.path().data(), &x, &y, &channels);
                                     textureSpec.Data     = data;
                                     textureSpec.DataSize = static_cast<size_t>(x) * static_cast<size_t>(y) * channels;
                                     textureSpec.Width    = x;
                                     textureSpec.Height   = y;

                                     texture = Texture2D::Create(textureSpec);
                                     ImageUtils::UnloadRawImage(data);
                                 },
                                 [&](const fastgltf::sources::Vector& vector)
                                 {
                                     int32_t x = 1, y = 1, channels = 4;
                                     void* data = ImageUtils::LoadRawImageFromMemory(
                                         vector.bytes.data(), vector.bytes.size() * sizeof(vector.bytes[0]), &x, &y, &channels);

                                     textureSpec.Data     = data;
                                     textureSpec.DataSize = static_cast<size_t>(x) * static_cast<size_t>(y) * channels;
                                     textureSpec.Width    = x;
                                     textureSpec.Height   = y;

                                     texture = Texture2D::Create(textureSpec);
                                     ImageUtils::UnloadRawImage(data);
                                 }},
               imageData);

    return texture;
}

}  // namespace FasgtlfUtils

namespace MeshPreprocessorUtils
{

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

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | /*fastgltf::Options::AllowDouble |*/
                                 fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers |
                                 fastgltf::Options::LoadExternalImages | fastgltf::Options::GenerateMeshIndices;

    std::filesystem::path fsMeshPath(meshPath);
    fastgltf::Expected<fastgltf::Asset> asset(fastgltf::Error::None);
    if (gltfType == fastgltf::GltfType::glTF)
    {
        asset = parser.loadGltf(&data, fsMeshPath.parent_path(), gltfOptions);
    }
    else if (gltfType == fastgltf::GltfType::GLB)
    {
        asset = parser.loadGltfBinary(&data, fsMeshPath.parent_path(), gltfOptions);
    }
    else
        PFR_ASSERT(false, "[FASTGLTF]: Failed to determine glTF container");

    if (const auto error = asset.error(); error != fastgltf::Error::None)
    {
        LOG_TAG_ERROR(FASTGLTF, "Error occured while loading mesh \"%s\"! Error name: %s / Message: %s", meshPath.data(),
                      fastgltf::getErrorName(error).data(), fastgltf::getErrorMessage(error).data());
        PFR_ASSERT(false, "Failed to load mesh!");
    }
    LOG_TAG_INFO(FASTGLTF, "\"%s\" has (%zu) buffers, (%zu) textures, (%zu) animations, (%zu) materials, (%zu) meshes.", meshPath.data(),
                 asset->buffers.size(), asset->animations.size(), asset->textures.size(), asset->materials.size(), asset->meshes.size());

    for (auto& node : asset->nodes)
    {
        TraverseNodes(asset.get(), node);
    }

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
    m_Submeshes.clear();
}

void Mesh::TraverseNodes(const fastgltf::Asset& asset, const fastgltf::Node& node)
{

    PFR_ASSERT(node.meshIndex.has_value(), "Node hasn't mesh index!");
    for (const auto& p : asset.meshes[node.meshIndex.value()].primitives)
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

        fastgltf::iterateAccessorWithIndex<glm::vec3>(
            asset, positionAccessor,
            [&](const glm::vec3& position, std::size_t idx)
            {
                meshoptimizeVertices[idx].Position = position;

                {
                    if (const fastgltf::Node::TRS* pTransform = std::get_if<fastgltf::Node::TRS>(&node.transform))
                        meshoptimizeVertices[idx].Position =
                            vec4(meshoptimizeVertices[idx].Position, 1.0) *
                            glm::translate(glm::mat4(1.0f), glm::make_vec3(pTransform->translation.data())) *
                            glm::toMat4(glm::make_quat(pTransform->rotation.data())) *
                            glm::scale(glm::mat4(1.0f), glm::make_vec3(pTransform->scale.data()));
                }
            });

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

            submesh->SetMaterial(MakeShared<Material>());
            auto& material = submesh->GetMaterial();

            PBRData pbrData   = {};
            pbrData.BaseColor = glm::make_vec4(materialAccessor.pbrData.baseColorFactor.data());
            pbrData.Metallic  = materialAccessor.pbrData.metallicFactor;
            pbrData.Roughness = materialAccessor.pbrData.roughnessFactor;

            material->SetPBRData(pbrData);
            material->SetIsOpaque(materialAccessor.alphaMode == fastgltf::AlphaMode::Opaque);

            if (materialAccessor.pbrData.baseColorTexture.has_value())
            {
                TextureSpecification albedoTextureSpec = {};

                Shared<Texture2D> albedo = FasgtlfUtils::FastgltfLoadTexture(asset, materialAccessor, albedoTextureSpec);

                Renderer::GetBindlessRenderer()->LoadTexture(albedo);
                material->SetAlbedo(albedo);
            }

            if (materialAccessor.normalTexture.has_value())
            {
                TextureSpecification normalMapTextureSpec = {};

                Shared<Texture2D> normalMap = FasgtlfUtils::FastgltfLoadTexture(asset, materialAccessor, normalMapTextureSpec);

                Renderer::GetBindlessRenderer()->LoadTexture(normalMap);
                material->SetNormalMap(normalMap);
            }

            if (materialAccessor.pbrData.metallicRoughnessTexture.has_value())
            {
                TextureSpecification metallicRoughnessTextureSpec = {};

                Shared<Texture2D> metallicRoughness =
                    FasgtlfUtils::FastgltfLoadTexture(asset, materialAccessor, metallicRoughnessTextureSpec);

                Renderer::GetBindlessRenderer()->LoadTexture(metallicRoughness);
                material->SetMetallicRoughness(metallicRoughness);
            }
        }

        std::vector<uint32_t> finalIndices;
        std::vector<MeshPreprocessorUtils::MeshoptimizeVertex> finalVertices;
        MeshPreprocessorUtils::OptimizeMesh(indices, meshoptimizeVertices, finalIndices, finalVertices);

        // NOTE: Currently index buffer is not used in shaders, so they don't need STORAGE usage flag
        BufferSpecification ibSpec = {EBufferUsage::BUFFER_TYPE_INDEX /* | EBufferUsage::BUFFER_TYPE_STORAGE */};
        ibSpec.Data                = finalIndices.data();
        ibSpec.DataSize            = finalIndices.size() * sizeof(finalIndices[0]);
        if (Renderer::GetRendererSettings().bRTXSupport)
        {
            ibSpec.BufferUsage |=
                EBufferUsage::BUFFER_TYPE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY | EBufferUsage::BUFFER_TYPE_SHADER_DEVICE_ADDRESS;
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

        // NOTE: Do I free some RAM here?
        indices.clear();
        meshoptimizeVertices.clear();

        BufferSpecification vbPosSpec = {EBufferUsage::BUFFER_TYPE_VERTEX | EBufferUsage::BUFFER_TYPE_STORAGE};
        vbPosSpec.Data                = vertexPositions.data();
        vbPosSpec.DataSize            = vertexPositions.size() * sizeof(vertexPositions[0]);
        if (Renderer::GetRendererSettings().bRTXSupport)
        {
            vbPosSpec.BufferUsage |=
                EBufferUsage::BUFFER_TYPE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY | EBufferUsage::BUFFER_TYPE_SHADER_DEVICE_ADDRESS;
        }
        submesh->m_VertexPositionBuffer = Buffer::Create(vbPosSpec);

        BufferSpecification vbAttribSpec = {EBufferUsage::BUFFER_TYPE_VERTEX | EBufferUsage::BUFFER_TYPE_STORAGE};
        vbAttribSpec.Data                = vertexAttributes.data();
        vbAttribSpec.DataSize            = vertexAttributes.size() * sizeof(vertexAttributes[0]);
        if (Renderer::GetRendererSettings().bRTXSupport)
        {
            vbAttribSpec.BufferUsage |=
                EBufferUsage::BUFFER_TYPE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY | EBufferUsage::BUFFER_TYPE_SHADER_DEVICE_ADDRESS;
        }
        submesh->m_VertexAttributeBuffer = Buffer::Create(vbAttribSpec);

        if (Renderer::GetRendererSettings().bRTXSupport)
        {
            // TODO:
        }

        if (Renderer::GetRendererSettings().bMeshShadingSupport)
        {
            std::vector<uint32_t> meshletVertices;
            std::vector<uint8_t> meshletTriangles;
            std::vector<Meshlet> meshlets;
            MeshPreprocessorUtils::BuildMeshlets(finalIndices, vertexPositions, meshlets, meshletVertices, meshletTriangles);

            BufferSpecification mbSpec = {EBufferUsage::BUFFER_TYPE_STORAGE};
            mbSpec.Data                = meshlets.data();
            mbSpec.DataSize            = meshlets.size() * sizeof(meshlets[0]);
            submesh->m_MeshletBuffer   = Buffer::Create(mbSpec);

            BufferSpecification mbVertSpec   = {EBufferUsage::BUFFER_TYPE_STORAGE};
            mbVertSpec.Data                  = meshletVertices.data();
            mbVertSpec.DataSize              = meshletVertices.size() * sizeof(meshletVertices[0]);
            submesh->m_MeshletVerticesBuffer = Buffer::Create(mbVertSpec);

            BufferSpecification mbTriSpec     = {EBufferUsage::BUFFER_TYPE_STORAGE};
            mbTriSpec.Data                    = meshletTriangles.data();
            mbTriSpec.DataSize                = meshletTriangles.size() * sizeof(meshletTriangles[0]);
            submesh->m_MeshletTrianglesBuffer = Buffer::Create(mbTriSpec);
        }
    }

    // Load buffers into bindless system
    const auto& br = Renderer::GetBindlessRenderer();
    for (auto& submesh : m_Submeshes)
    {
        if (submesh->m_VertexPositionBuffer) br->LoadVertexPosBuffer(submesh->m_VertexPositionBuffer);

        if (submesh->m_VertexAttributeBuffer) br->LoadVertexAttribBuffer(submesh->m_VertexAttributeBuffer);

        if (Renderer::GetRendererSettings().bMeshShadingSupport)
        {
            if (submesh->m_MeshletBuffer) br->LoadMeshletBuffer(submesh->m_MeshletBuffer);

            if (submesh->m_MeshletVerticesBuffer) br->LoadMeshletVerticesBuffer(submesh->m_MeshletVerticesBuffer);

            if (submesh->m_MeshletTrianglesBuffer) br->LoadMeshletTrianglesBuffer(submesh->m_MeshletTrianglesBuffer);
        }
    }
}

void Submesh::Destroy()
{
    m_VertexPositionBuffer.reset();
    m_VertexPositionBuffer.reset();
    m_IndexBuffer.reset();

    if (Renderer::GetRendererSettings().bMeshShadingSupport)
    {
        m_MeshletBuffer.reset();
        m_MeshletVerticesBuffer.reset();
        m_MeshletTrianglesBuffer.reset();
    }
}

}  // namespace Pathfinder