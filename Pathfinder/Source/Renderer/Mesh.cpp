#include "PathfinderPCH.h"
#include "Mesh.h"

#include "Buffer.h"
#include "RendererCoreDefines.h"
#include "Renderer.h"
#include "BindlessRenderer.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/parser.hpp>
#include <fastgltf/tools.hpp>

#include <meshoptimizer.h>

namespace Pathfinder
{

Mesh::Mesh(const std::string& meshPath)
{
    // Optimally, you should reuse Parser instance across loads, but don't use it across threads.
    // To enable extensions, you have to pass them into the parser's constructor.
    thread_local fastgltf::Parser parser;

    Timer t = {};

    // The GltfDataBuffer class is designed for re-usability of the same JSON string. It contains
    // utility functions to load data from a std::filesystem::path, copy from an existing buffer,
    // or re-use an already existing allocation. Note that it has to outlive the process of every
    // parsing function you call.
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
    {
        PFR_ASSERT(false, "[FASTGLTF]: Failed to determine glTF container");
    }

    if (const auto error = asset.error(); error != fastgltf::Error::None)
    {
        LOG_TAG_ERROR(FASTGLTF, "Error occured while loading mesh \"%s\"! Error name: %s / Message: %s", meshPath.data(),
                      fastgltf::getErrorName(error).data(), fastgltf::getErrorMessage(error).data());
        PFR_ASSERT(false, "Failed to load mesh!");
    }
    LOG_TAG_INFO(FASTGLTF, "\"%s\" has (%zu) buffers, (%zu) textures, (%zu) animations, (%zu) materials, (%zu) meshes.", meshPath.data(),
                 asset->buffers.size(), asset->animations.size(), asset->textures.size(), asset->materials.size(), asset->meshes.size());

    for (auto& submesh : asset->meshes)
    {
        LoadSubmeshes(asset.get(), submesh);
    }

#if PFR_DEBUG
    // Optionally, you can now also call the fastgltf::validate method. This will more strictly
    // enforce the glTF spec and is not needed most of the time, though I would certainly
    // recommend it in a development environment or when debugging to avoid mishaps.
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

// TODO: Sort out why would I need byteOffset/byteStride in accessor
void Mesh::LoadSubmeshes(const fastgltf::Asset& asset, const fastgltf::Mesh& GLTFsubmesh)
{
    for (const auto& p : GLTFsubmesh.primitives)
    {
        const auto positionIt = p.findAttribute("POSITION");
        PFR_ASSERT(positionIt != p.attributes.end(), "Mesh doesn't contain positions?!");
        PFR_ASSERT(p.indicesAccessor.has_value(), "Non-indexed geometry is not supported!");

        auto& submesh = m_Submeshes.emplace_back(MakeShared<Submesh>());

        // INDICES
        const auto& indicesAccessor = asset.accessors[p.indicesAccessor.value()];  // out of scope cuz needed for meshlets
        std::vector<uint32_t> indices(indicesAccessor.count);
        {
            fastgltf::iterateAccessorWithIndex<std::uint32_t>(asset, indicesAccessor,
                                                              [&](uint32_t index, std::size_t idx) { indices[idx] = index; });

            BufferSpecification ibSpec = {};
            ibSpec.BufferUsage         = EBufferUsage::BUFFER_TYPE_INDEX | EBufferUsage::BUFFER_TYPE_STORAGE;
            ibSpec.Data                = indices.data();
            ibSpec.DataSize            = indices.size() * sizeof(indices[0]);
            if (Renderer::GetRendererSettings().bRTXSupport)
            {
                ibSpec.BufferUsage |= EBufferUsage::BUFFER_TYPE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY |
                                      EBufferUsage::BUFFER_TYPE_SHADER_DEVICE_ADDRESS;
            }

            submesh->m_IndexBuffer = Buffer::Create(ibSpec);
        }

        // POSITION
        const auto& positionAccessor = asset.accessors[positionIt->second];  // out of scope cuz needed for meshlets
        std::vector<MeshPositionVertex> vertices(positionAccessor.count);
        {
            PFR_ASSERT(positionAccessor.type == fastgltf::AccessorType::Vec3, "Positions can only contain vec3!");

            const auto& bufferView = asset.bufferViews[positionAccessor.bufferViewIndex.value()];
            const auto& bufferData = asset.buffers[bufferView.bufferIndex].data;
            PFR_ASSERT(std::holds_alternative<fastgltf::sources::Vector>(bufferData), "Only vector parsing support for now!");

            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                asset, positionAccessor, [&](const glm::vec3& position, std::size_t idx) { vertices[idx].Position = position; });

            BufferSpecification vbPosSpec = {};
            vbPosSpec.BufferUsage         = EBufferUsage::BUFFER_TYPE_VERTEX | EBufferUsage::BUFFER_TYPE_STORAGE;
            vbPosSpec.Data                = vertices.data();
            vbPosSpec.DataSize            = vertices.size() * sizeof(vertices[0]);
            if (Renderer::GetRendererSettings().bRTXSupport)
            {
                vbPosSpec.BufferUsage |= EBufferUsage::BUFFER_TYPE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY |
                                         EBufferUsage::BUFFER_TYPE_SHADER_DEVICE_ADDRESS;
            }

            submesh->m_VertexPositionBuffer = Buffer::Create(vbPosSpec);
        }

        std::vector<MeshAttributeVertex> vertexAttributes(asset.accessors[positionIt->second].count);
        // NORMAL
        {
            const auto& normalIt = p.findAttribute("NORMAL");
            if (normalIt != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[normalIt->second],
                                                              [&](const glm::vec3& normal, std::size_t idx)
                                                              { vertexAttributes[idx].Normal = normal; });
            }
            // PFR_ASSERT(normalIt != p.attributes.end(), "Mesh doesn't contain normals?!");
        }

        // COLOR_0
        {
            const auto& color_0_It = p.findAttribute("COLOR_0");
            if (color_0_It != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[color_0_It->second],
                                                              [&](const glm::vec4& color, std::size_t idx)
                                                              { vertexAttributes[idx].Color = color; });
            }
            // PFR_ASSERT(color_0_It != p.attributes.end(), "Mesh doesn't contain colors?!");
        }

        // TANGENT
        {
            const auto& tangentIt = p.findAttribute("TANGENT");
            if (tangentIt != p.attributes.end())
            {
                fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[tangentIt->second],
                                                              [&](const glm::vec4& tangent, std::size_t idx)
                                                              { vertexAttributes[idx].Tangent = tangent; });
            }
            //  PFR_ASSERT(tangentIt != p.attributes.end(), "Mesh doesn't contain tangents?!");
        }

        // UV
        {
            const auto& uvIT = p.findAttribute("TEXCOORD_0");
            if (uvIT != p.attributes.end())
            {
            }
        }

        // PBR stuff
        if (p.materialIndex.has_value())
        {
            LOG_TAG_INFO(FASTGLTF, "Loading PBR data!");
            const auto& materialAccessor = asset.materials[p.materialIndex.value()];
            for (auto& vertexAttribute : vertexAttributes)
            {
                vertexAttribute.Color = glm::vec4(materialAccessor.pbrData.baseColorFactor[0], materialAccessor.pbrData.baseColorFactor[1],
                                                  materialAccessor.pbrData.baseColorFactor[2], materialAccessor.pbrData.baseColorFactor[3]);
            }

            submesh->m_bIsOpaque = materialAccessor.alphaMode == fastgltf::AlphaMode::Opaque;
        }

        BufferSpecification vbAttribSpec = {};
        vbAttribSpec.BufferUsage         = EBufferUsage::BUFFER_TYPE_VERTEX | EBufferUsage::BUFFER_TYPE_STORAGE;
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

        if (!Renderer::GetRendererSettings().bMeshShadingSupport) continue;

        // Simple meshlet building from zeux stream
        std::vector<Meshlet> meshlets;
        Meshlet meshlet = {};
        std::vector<uint8_t> meshletVertices(vertices.size(), UINT8_MAX);
        for (size_t i = 0; i < indices.size(); i += 3)
        {
            uint32_t a = indices[i + 0];
            uint32_t b = indices[i + 1];
            uint32_t c = indices[i + 2];

            uint8_t& av = meshletVertices[a];
            uint8_t& bv = meshletVertices[b];
            uint8_t& cv = meshletVertices[c];

            // 1 meshlet can store max 64 vertices, so if we exceeding the limit we do flush
            if (meshlet.vertexCount + (av == UINT8_MAX) + (bv == UINT8_MAX) + (cv == UINT8_MAX) > 64 || meshlet.indexCount + 3 > 126)
            {
                meshlets.emplace_back(meshlet);
                meshlet = {};
                memset(meshletVertices.data(), UINT8_MAX, meshletVertices.size() * sizeof(meshletVertices[0]));
            }

            if (av == UINT8_MAX)
            {
                av                                    = meshlet.vertexCount;
                meshlet.vertices[meshlet.vertexCount] = a;
                ++meshlet.vertexCount;
            }

            if (bv == UINT8_MAX)
            {
                bv                                    = meshlet.vertexCount;
                meshlet.vertices[meshlet.vertexCount] = b;
                ++meshlet.vertexCount;
            }

            if (cv == UINT8_MAX)
            {
                cv                                    = meshlet.vertexCount;
                meshlet.vertices[meshlet.vertexCount] = c;
                ++meshlet.vertexCount;
            }

            meshlet.indices[meshlet.indexCount++] = av;
            meshlet.indices[meshlet.indexCount++] = bv;
            meshlet.indices[meshlet.indexCount++] = cv;
        }

        if (meshlet.indexCount > 0)
        {
            meshlets.emplace_back(meshlet);
            meshlet = {};
            memset(meshletVertices.data(), UINT8_MAX, meshletVertices.size() * sizeof(meshletVertices[0]));
        }

        BufferSpecification mbSpec = {};
        mbSpec.BufferUsage         = EBufferUsage::BUFFER_TYPE_STORAGE;
        mbSpec.Data                = meshlets.data();
        mbSpec.DataSize            = meshlets.size() * sizeof(meshlets[0]);

        submesh->m_MeshletBuffer = Buffer::Create(mbSpec);
    }

    // Load buffers into bindless system
    const auto& br = Renderer::GetBindlessRenderer();
    for (auto& submesh : m_Submeshes)
    {
        if (submesh->m_VertexPositionBuffer) br->LoadVertexPosBuffer(submesh->m_VertexPositionBuffer);

        if (submesh->m_VertexAttributeBuffer) br->LoadVertexAttribBuffer(submesh->m_VertexAttributeBuffer);

        if (Renderer::GetRendererSettings().bMeshShadingSupport && submesh->m_MeshletBuffer)
            br->LoadMeshletBuffer(submesh->m_MeshletBuffer);
    }
}

void Submesh::Destroy()
{
    // NOTE: Remove it, d-tor handles it
    m_VertexPositionBuffer.reset();
    m_VertexPositionBuffer.reset();
    m_IndexBuffer.reset();

    if (Renderer::GetRendererSettings().bMeshShadingSupport)
    {
        m_MeshletBuffer.reset();
    }
}

}  // namespace Pathfinder