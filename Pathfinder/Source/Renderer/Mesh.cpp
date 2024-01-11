#include "PathfinderPCH.h"
#include "Mesh.h"

#include <fastgltf/parser.hpp>
#include <fastgltf/types.hpp>

#include <meshoptimizer.h>

namespace Pathfinder
{

Mesh::Mesh(const std::string& meshPath)
{
    // Creates a Parser instance. Optimally, you should reuse this across loads, but don't use it
    // across threads. To enable extensions, you have to pass them into the parser's constructor.
    thread_local fastgltf::Parser parser;

    // The GltfDataBuffer class is designed for re-usability of the same JSON string. It contains
    // utility functions to load data from a std::filesystem::path, copy from an existing buffer,
    // or re-use an already existing allocation. Note that it has to outlive the process of every
    // parsing function you call.
    fastgltf::GltfDataBuffer data;
    data.loadFromFile(meshPath);

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
                                 fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers |
                                 fastgltf::Options::LoadExternalImages | fastgltf::Options::GenerateMeshIndices;

    // TODO: Add GLB case
    const auto gltfType = fastgltf::determineGltfFileType(&data);
    if (gltfType == fastgltf::GltfType::Invalid) PFR_ASSERT(false, "Failed to parse gltf!");

    auto asset = parser.loadGltf(&data, meshPath, fastgltf::Options::None);
    if (const auto error = asset.error(); error != fastgltf::Error::None)
    {
        LOG_TAG_ERROR(FASTGLTF, "Error occured while loading mesh \"%s\"! Error name: %s / Message: %s", meshPath.data(),
                      fastgltf::getErrorName(error).data(), fastgltf::getErrorMessage(error).data());
        PFR_ASSERT(false, "Failed to load mesh!");
    }
    LOG_INFO("%s has (%zu) buffers, (%zu) textures, (%zu) materials", meshPath.data(), asset->buffers.size(), asset->textures.size(),
             asset->materials.size());

    // The glTF 2.0 asset is now ready to be used. Simply call asset.get(), asset.get_if() or
    // asset-> to get a direct reference to the Asset class. You can then access the glTF data
    // structures, like, for example, with buffers:
    // for (auto& buffer : asset->buffers)
    // {
    //     LOG_INFO("%zu bytes", buffer.byteLength);
    //     // Process the buffers.
    // }

    LOG_INFO("%zu meshes", asset->meshes.size());
    for (auto& submesh : asset->meshes)
    {
        LoadSubmeshes(asset, submesh);
    }

    // Optionally, you can now also call the fastgltf::validate method. This will more strictly
    // enforce the glTF spec and is not needed most of the time, though I would certainly
    // recommend it in a development environment or when debugging to avoid mishaps.
    // fastgltf::validate(asset.get());
}

Mesh::~Mesh() {}

Shared<Mesh> Mesh::Create(const std::string& meshPath)
{
    return MakeShared<Mesh>(meshPath);
}

void Mesh::Destroy() {}

void Mesh::LoadSubmeshes(const auto& asset, const auto& submesh)
{

    //    Mesh outMesh = {};
    //  outMesh.primitives.resize(mesh.primitives.size());
/*
    for (auto it = submesh.primitives.begin(); it != submesh.primitives.end(); ++it)
    {
        auto* positionIt = it->findAttribute("POSITION");
        assert(positionIt != it->attributes.end());

        PFR_ASSERT(it->indicesAccessor.has_value(), "We only support indexed geometry.");

        // Generate the VAO
        // GLuint vao = GL_NONE;
        // glCreateVertexArrays(1, &vao);

        // Get the output primitive
        auto index              = std::distance(submesh.primitives.begin(), it);
        auto& primitive         = submesh.primitives[index];
        primitive.primitiveType = fastgltf::to_underlying(it->type);
        // primitive.vertexArray   = vao;
        if (it->materialIndex.has_value())
        {
            //    primitive.materialUniformsIndex = it->materialIndex.value() + 1;  // Adjust for default material
            auto& material = viewer->asset.materials[it->materialIndex.value()];
            if (material.pbrData.baseColorTexture.has_value())
            {
                auto& texture = viewer->asset.textures[material.pbrData.baseColorTexture->textureIndex];
                if (!texture.imageIndex.has_value()) return false;
                primitive.albedoTexture = viewer->textures[texture.imageIndex.value()].texture;
            }
        }
        else
        {
            primitive.materialUniformsIndex = 0;
        }

        {
            // Position
            auto& positionAccessor = asset.accessors[positionIt->second];
            if (!positionAccessor.bufferViewIndex.has_value()) continue;

      //      glEnableVertexArrayAttrib(vao, 0);
    //        glVertexArrayAttribFormat(vao, 0, static_cast<GLint>(fastgltf::getNumComponents(positionAccessor.type)), fastgltf::getGLComponentType(positionAccessor.componentType), GL_FALSE, 0);
           // glVertexArrayAttribBinding(vao, 0, 0);

            auto& positionView = asset.bufferViews[positionAccessor.bufferViewIndex.value()];
            auto offset        = positionView.byteOffset + positionAccessor.byteOffset;
            if (positionView.byteStride.has_value())
            {
                glVertexArrayVertexBuffer(vao, 0, viewer->buffers[positionView.bufferIndex], static_cast<GLintptr>(offset),
                                          static_cast<GLsizei>(positionView.byteStride.value()));
            }
            else
            {
                glVertexArrayVertexBuffer(
                    vao, 0, viewer->buffers[positionView.bufferIndex], static_cast<GLintptr>(offset),
                    static_cast<GLsizei>(fastgltf::getElementByteSize(positionAccessor.type, positionAccessor.componentType)));
            }
        }

        if (const auto* texcoord0 = it->findAttribute("TEXCOORD_0"); texcoord0 != it->attributes.end())
        {
            // Tex coord
            auto& texCoordAccessor = asset.accessors[texcoord0->second];
            if (!texCoordAccessor.bufferViewIndex.has_value()) continue;

            glEnableVertexArrayAttrib(vao, 1);
            glVertexArrayAttribFormat(vao, 1, static_cast<GLint>(fastgltf::getNumComponents(texCoordAccessor.type)),
                                      fastgltf::getGLComponentType(texCoordAccessor.componentType), GL_FALSE, 0);
            glVertexArrayAttribBinding(vao, 1, 1);

            auto& texCoordView = asset.bufferViews[texCoordAccessor.bufferViewIndex.value()];
            auto offset        = texCoordView.byteOffset + texCoordAccessor.byteOffset;
            if (texCoordView.byteStride.has_value())
            {
                glVertexArrayVertexBuffer(vao, 1, viewer->buffers[texCoordView.bufferIndex], static_cast<GLintptr>(offset),
                                          static_cast<GLsizei>(texCoordView.byteStride.value()));
            }
            else
            {
                glVertexArrayVertexBuffer(
                    vao, 1, viewer->buffers[texCoordView.bufferIndex], static_cast<GLintptr>(offset),
                    static_cast<GLsizei>(fastgltf::getElementByteSize(texCoordAccessor.type, texCoordAccessor.componentType)));
            }
        }

        // Generate the indirect draw command
        auto& draw         = primitive.draw;
        draw.instanceCount = 1;
        draw.baseInstance  = 0;
        draw.baseVertex    = 0;

        auto& indices = asset.accessors[it->indicesAccessor.value()];
        if (!indices.bufferViewIndex.has_value()) return false;
        draw.count = static_cast<uint32_t>(indices.count);

        auto& indicesView = asset.bufferViews[indices.bufferViewIndex.value()];
        draw.firstIndex   = static_cast<uint32_t>(indices.byteOffset + indicesView.byteOffset) /
                          fastgltf::getElementByteSize(indices.type, indices.componentType);
        primitive.indexType = getGLComponentType(indices.componentType);
        glVertexArrayElementBuffer(vao, viewer->buffers[indicesView.bufferIndex]);
    }

    // Create the buffer holding all of our primitive structs.
    glCreateBuffers(1, &outMesh.drawsBuffer);
    glNamedBufferData(outMesh.drawsBuffer, static_cast<GLsizeiptr>(outMesh.primitives.size() * sizeof(Primitive)),
                      outMesh.primitives.data(), GL_STATIC_DRAW);

    viewer->meshes.emplace_back(outMesh);
    */
}

}  // namespace Pathfinder