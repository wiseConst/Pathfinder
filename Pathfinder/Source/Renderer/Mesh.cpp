#include "PathfinderPCH.h"
#include "Mesh.h"

#include "Buffer.h"
#include "RendererCoreDefines.h"

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
    m_VertexPositionBuffers.clear();
    m_VertexPositionBuffers.clear();
    m_IndexBuffers.clear();
}

// TODO: Sort out why would I need byteOffset/byteStride in accessor
void Mesh::LoadSubmeshes(const fastgltf::Asset& asset, const fastgltf::Mesh& submesh)
{
    for (const auto& p : submesh.primitives)
    {
        const auto positionIt = p.findAttribute("POSITION");
        PFR_ASSERT(positionIt != p.attributes.end(), "Mesh doesn't contain positions?!");
        PFR_ASSERT(p.indicesAccessor.has_value(), "Non-indexed geometry is not supported!");

        // INDICES
        {
            const auto& indicesAccessor = asset.accessors[p.indicesAccessor.value()];
            std::vector<uint32_t> indices(indicesAccessor.count);
            fastgltf::iterateAccessorWithIndex<std::uint32_t>(asset, indicesAccessor,
                                                              [&](uint32_t index, std::size_t idx) { indices[idx] = index; });

            BufferSpecification ibSpec = {};
            ibSpec.BufferUsage         = EBufferUsage::BUFFER_TYPE_INDEX;
            ibSpec.Data                = indices.data();
            ibSpec.DataSize            = indices.size() * sizeof(indices[0]);

            m_IndexBuffers.emplace_back(Buffer::Create(ibSpec));
        }

        // POSITION
        {
            const auto& positionAccessor = asset.accessors[positionIt->second];
            PFR_ASSERT(positionAccessor.type == fastgltf::AccessorType::Vec3, "Positions can only contain vec3!");

            const auto& bufferView = asset.bufferViews[positionAccessor.bufferViewIndex.value()];
            const auto& bufferData = asset.buffers[bufferView.bufferIndex].data;
            PFR_ASSERT(std::holds_alternative<fastgltf::sources::Vector>(bufferData), "Only vector parsing support for now!");

            std::vector<MeshVertexPosition> vertices(positionAccessor.count);
            fastgltf::iterateAccessorWithIndex<glm::vec3>(
                asset, positionAccessor, [&](const glm::vec3& position, std::size_t idx) { vertices[idx].Position = position; });

            BufferSpecification vbSpec = {};
            vbSpec.BufferUsage         = EBufferUsage::BUFFER_TYPE_VERTEX;
            vbSpec.Data                = vertices.data();
            vbSpec.DataSize            = vertices.size() * sizeof(vertices[0]);
            m_VertexPositionBuffers.emplace_back(Buffer::Create(vbSpec));
        }

        std::vector<MeshVertex> vertexAttributes(asset.accessors[positionIt->second].count);
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

            m_bIsOpaque.emplace_back(materialAccessor.alphaMode == fastgltf::AlphaMode::Opaque);
        }

        BufferSpecification vbSpec = {};
        vbSpec.BufferUsage         = EBufferUsage::BUFFER_TYPE_VERTEX;
        vbSpec.Data                = vertexAttributes.data();
        vbSpec.DataSize            = vertexAttributes.size() * sizeof(vertexAttributes[0]);

        m_VertexAttributeBuffers.emplace_back(Buffer::Create(vbSpec));
    }
}
}  // namespace Pathfinder