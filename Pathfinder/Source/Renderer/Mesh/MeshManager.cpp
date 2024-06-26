#include <PathfinderPCH.h>
#include "MeshManager.h"

#include "Submesh.h"
#include "Globals.h"

#include <Core/Application.h>
#include <Core/Intrinsics.h>

#include <Renderer/Buffer.h>
#include <Renderer/Material.h>
#include <Renderer/Image.h>
#include <Renderer/Texture.h>
#include <Renderer/Renderer.h>

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

#include <meshoptimizer.h>

namespace Pathfinder
{

namespace MeshOptimizerUtils
{
template <typename T>
FORCEINLINE constexpr static void RemapVertexStream(const size_t remappedVertexCount, std::vector<T>& vertexStream,
                                                    const std::vector<uint32_t>& remappedIndices)
{
    std::vector<T> remmapedVertexStream(remappedVertexCount);
    meshopt_remapVertexBuffer(remmapedVertexStream.data(), vertexStream.data(), vertexStream.size(), sizeof(vertexStream[0]),
                              remappedIndices.data());
    vertexStream = std::move(remmapedVertexStream);
}
}  // namespace MeshOptimizerUtils

namespace FastGLTFUtils
{

NODISCARD FORCEINLINE static ESamplerFilter SamplerFilterToPathfinder(const fastgltf::Filter filter)
{
    switch (filter)
    {
        case fastgltf::Filter::Nearest: return ESamplerFilter::SAMPLER_FILTER_NEAREST;
        case fastgltf::Filter::Linear: return ESamplerFilter::SAMPLER_FILTER_LINEAR;
        default: LOG_WARN("FASTGLTF: Other sampler filters aren't supported!"); return ESamplerFilter::SAMPLER_FILTER_NEAREST;
    }

    PFR_ASSERT(false, "Unknown fastgltf::filter!");
    return ESamplerFilter::SAMPLER_FILTER_LINEAR;
}

NODISCARD FORCEINLINE static ESamplerWrap SamplerWrapToPathfinder(const fastgltf::Wrap wrap)
{
    switch (wrap)
    {
        case fastgltf::Wrap::ClampToEdge: return ESamplerWrap::SAMPLER_WRAP_CLAMP_TO_EDGE;
        case fastgltf::Wrap::MirroredRepeat: return ESamplerWrap::SAMPLER_WRAP_MIRRORED_REPEAT;
        case fastgltf::Wrap::Repeat: return ESamplerWrap::SAMPLER_WRAP_REPEAT;
        default: LOG_WARN("FASTGLTF: Other sampler wraps aren't supported!"); return ESamplerWrap::SAMPLER_WRAP_REPEAT;
    }

    PFR_ASSERT(false, "Unknown fastgltf::wrap!");
    return ESamplerWrap::SAMPLER_WRAP_REPEAT;
}

NODISCARD static Shared<Texture> LoadTexture(UnorderedMap<std::string, Shared<Texture>>& loadedTextures, const std::string& meshAssetsDir,
                                             const size_t textureIndex, const fastgltf::Asset& asset,
                                             const EImageFormat requestedImageFormat = EImageFormat::FORMAT_RGBA8_UNORM,
                                             const bool bMetallicRoughness = false, const bool bFlipOnLoad = false)
{
    const auto& fastgltfTexture = asset.textures.at(textureIndex);
    const auto imageIndex       = fastgltfTexture.imageIndex;
    PFR_ASSERT(imageIndex.has_value(), "Invalid image index!");

    const auto& fastgltfImage = asset.images.at(imageIndex.value());

    TextureSpecification textureSpec = {.DebugName = std::string(fastgltfImage.name), .Format = requestedImageFormat};

    PFR_ASSERT(std::holds_alternative<fastgltf::sources::URI>(fastgltfImage.data), "Texture hasn't path!");
    const auto& fastgltfURI = std::get<fastgltf::sources::URI>(fastgltfImage.data);

    const std::string textureName = fastgltfURI.uri.fspath().stem().string();
    PFR_ASSERT(textureName.data(), "Texture has no name!");
    if (loadedTextures.contains(textureName)) return loadedTextures.at(textureName);

    if (fastgltfTexture.samplerIndex.has_value())
    {
        const auto& fastgltfTextureSampler = asset.samplers.at(fastgltfTexture.samplerIndex.value());
        textureSpec.Wrap                   = FastGLTFUtils::SamplerWrapToPathfinder(fastgltfTextureSampler.wrapS);
        if (fastgltfTextureSampler.magFilter.has_value())
            textureSpec.Filter = FastGLTFUtils::SamplerFilterToPathfinder(fastgltfTextureSampler.magFilter.value());
    }

    const auto& appSpec = Application::Get().GetSpecification();
    const auto ind      = meshAssetsDir.find(appSpec.MeshDir);
    PFR_ASSERT(ind != std::string::npos, "Failed to find meshes substr index!");
    const std::string meshDir = meshAssetsDir.substr(ind);  // contains something like: "Meshes/sponza/"
    const auto currentMeshTextureCacheDir =
        std::filesystem::path(appSpec.WorkingDir) / appSpec.AssetsDir / appSpec.CacheDir / meshDir / "textures/";
    if (!std::filesystem::exists(currentMeshTextureCacheDir)) std::filesystem::create_directories(currentMeshTextureCacheDir);

    Shared<Texture> texture = nullptr;
    std::visit(
        fastgltf::visitor{
            [](auto& arg) {},
            [&](const fastgltf::sources::URI& uri)
            {
                std::filesystem::path textureURIPath = uri.uri.string();
                if (const auto lastSlashIndex = textureURIPath.string().find_last_of("/"); lastSlashIndex != std::string::npos)
                {
                    // Strip extra info, all we want is texture name(everything after last slash).
                    textureURIPath = uri.uri.string().substr(lastSlashIndex + 1);
                }

                std::string bcExtension = ".bc";
                switch (textureSpec.Format)
                {
                    case EImageFormat::FORMAT_BC1_RGB_UNORM: bcExtension += "1_rgb_unorm"; break;
                    case EImageFormat::FORMAT_BC1_RGB_SRGB: bcExtension += "1_rgb_srgb"; break;
                    case EImageFormat::FORMAT_BC1_RGBA_UNORM: bcExtension += "1_rgba_unorm"; break;
                    case EImageFormat::FORMAT_BC1_RGBA_SRGB: bcExtension += "1_rgba_srgb"; break;
                    case EImageFormat::FORMAT_BC2_UNORM: bcExtension += "2_unorm"; break;
                    case EImageFormat::FORMAT_BC2_SRGB: bcExtension += "2_srgb"; break;
                    case EImageFormat::FORMAT_BC3_UNORM: bcExtension += "3_unorm"; break;
                    case EImageFormat::FORMAT_BC3_SRGB: bcExtension += "3_srgb"; break;
                    case EImageFormat::FORMAT_BC4_UNORM: bcExtension += "4_unorm"; break;
                    case EImageFormat::FORMAT_BC4_SNORM: bcExtension += "4_snorm"; break;
                    case EImageFormat::FORMAT_BC5_UNORM: bcExtension += "5_unorm"; break;
                    case EImageFormat::FORMAT_BC5_SNORM: bcExtension += "5_snorm"; break;
                    case EImageFormat::FORMAT_BC6H_UFLOAT: bcExtension += "6H_ufloat"; break;
                    case EImageFormat::FORMAT_BC6H_SFLOAT: bcExtension += "6H_sfloat"; break;
                    case EImageFormat::FORMAT_BC7_UNORM: bcExtension += "7_unorm"; break;
                    case EImageFormat::FORMAT_BC7_SRGB: bcExtension += "7_srgb"; break;
                    default: LOG_WARN("Image format is not a BC!"); break;
                }

                std::filesystem::path textureCacheFilePath = currentMeshTextureCacheDir / textureURIPath;
                textureCacheFilePath.replace_extension(bcExtension);

                // Firstly try to load compressed, otherwise compress and save.
                if (std::filesystem::exists(textureCacheFilePath))
                {
                    std::vector<uint8_t> compressedImage;
                    TextureCompressor::LoadCompressed(textureCacheFilePath, textureSpec, compressedImage);
                    texture = Texture::Create(textureSpec, compressedImage.data(), compressedImage.size());
                }
                else
                {
                    int32_t x = 1, y = 1, channels = 4;
                    const std::string texturePath   = meshAssetsDir + std::string(uri.uri.string());
                    void* uncompressedData          = ImageUtils::LoadRawImage(texturePath, bFlipOnLoad, &x, &y, &channels);
                    const auto uncompressedDataSize = static_cast<size_t>(x) * static_cast<size_t>(y) * channels;
                    textureSpec.Width               = x;
                    textureSpec.Height              = y;

                    void* rgbToRgbaBuffer = nullptr;
                    if (channels == 3) rgbToRgbaBuffer = ImageUtils::ConvertRgbToRgba((uint8_t*)uncompressedData, x, y);

                    // Default format set in texture specification in case no BCn specified.
                    EImageFormat srcImageFormat = EImageFormat::FORMAT_RGBA8_UNORM;
                    switch (channels)
                    {
                        case 1:
                        {
                            if (textureSpec.Format == EImageFormat::FORMAT_RGBA8_UNORM) textureSpec.Format = EImageFormat::FORMAT_R8_UNORM;
                            srcImageFormat = EImageFormat::FORMAT_R8_UNORM;  // Grayscale
                            break;
                        }
                        case 2:
                        {
                            if (textureSpec.Format == EImageFormat::FORMAT_RGBA8_UNORM) textureSpec.Format = EImageFormat::FORMAT_RG8_UNORM;
                            srcImageFormat = EImageFormat::FORMAT_RG8_UNORM;  // Grayscale with alpha
                            break;
                        }
                        case 3:  // 24bpp(RGB) formats are hard to optimize in graphics hardware, so they're mostly
                        // not supported(also in Vulkan). (but we handle this by converting rgb to rgba)
                        case 4:
                        {
                            if (textureSpec.Format == EImageFormat::FORMAT_RGBA8_UNORM)
                                textureSpec.Format = EImageFormat::FORMAT_RGBA8_UNORM;
                            srcImageFormat = EImageFormat::FORMAT_RGBA8_UNORM;  // RGBA
                            break;
                        }
                        default: PFR_ASSERT(false, "Unsupported number of image channels!");
                    }

                    uint8_t* whatToCompress   = nullptr;
                    size_t whatToCompressSize = 0;

                    // From: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html
                    // The textures for METALNESS and ROUGHNESS properties are packed together in a single texture called
                    // metallicRoughnessTexture. Its GREEN channel contains ROUGHNESS values and its BLUE channel contains METALNESS values.
                    // This texture MUST be encoded with linear transfer function and MAY use more than 8 bits per channel.
                    // Convert to RG format by shifting GB to the left.
                    if (bMetallicRoughness)
                    {
                        const size_t dataSize = static_cast<size_t>(x) * static_cast<size_t>(y);
                        whatToCompressSize    = rgbToRgbaBuffer ? dataSize * 4 : uncompressedDataSize;
                        whatToCompress        = static_cast<uint8_t*>(rgbToRgbaBuffer ? rgbToRgbaBuffer : uncompressedData);
                        for (size_t i{}; i < dataSize; ++i)
                        {
                            whatToCompress[i * 4 + 0] = whatToCompress[i * 4 + 1];
                            whatToCompress[i * 4 + 1] = whatToCompress[i * 4 + 2];
                        }
                    }
                    else
                    {
                        whatToCompress     = reinterpret_cast<uint8_t*>(rgbToRgbaBuffer ? rgbToRgbaBuffer : uncompressedData);
                        whatToCompressSize = rgbToRgbaBuffer ? static_cast<size_t>(x) * static_cast<size_t>(y) * 4 : uncompressedDataSize;
                    }

                    if (textureSpec.Format == EImageFormat::FORMAT_RGBA8_UNORM)
                    {
                        texture = Texture::Create(textureSpec, whatToCompress, whatToCompressSize);
                    }
                    else
                    {
                        void* compressedData      = nullptr;
                        size_t compressedDataSize = 0;

                        TextureCompressor::Compress(textureSpec, srcImageFormat, whatToCompress, whatToCompressSize, &compressedData,
                                                    compressedDataSize);

                        texture = Texture::Create(textureSpec, compressedData, compressedDataSize);
                        TextureCompressor::SaveCompressed(textureCacheFilePath, textureSpec, compressedData, compressedDataSize);
                        free(compressedData);
                    }

                    ImageUtils::UnloadRawImage(uncompressedData);
                    if (channels == 3) delete[] rgbToRgbaBuffer;
                }
            }},
        fastgltfImage.data);

    loadedTextures[textureName] = texture;
    return texture;
}

}  // namespace FastGLTFUtils

void MeshManager::LoadMesh(std::vector<Shared<Submesh>>& submeshes, const std::filesystem::path& meshFilePath)
{
    // Optimally, you should reuse Parser instance across loads, but don't use it across threads.
    thread_local fastgltf::Parser parser;

    Timer t = {};
    fastgltf::GltfDataBuffer data;
    PFR_ASSERT(data.loadFromFile(meshFilePath), "Failed to load  fastgltf::GltfDataBuffer!");

    const auto gltfType = fastgltf::determineGltfFileType(&data);
    if (gltfType == fastgltf::GltfType::Invalid) PFR_ASSERT(false, "Failed to parse gltf!");

    constexpr auto gltfOptions = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
                                 fastgltf::Options::LoadGLBBuffers | fastgltf::Options::LoadExternalBuffers |
                                 fastgltf::Options::GenerateMeshIndices;

    auto asset = parser.loadGltf(&data, meshFilePath.parent_path(), gltfOptions);
    if (const auto error = asset.error(); error != fastgltf::Error::None)
    {
        LOG_ERROR("FASTGLTF: Error occured while loading mesh \"{}\"! Error name: {} / Message: {}", meshFilePath.string(),
                  fastgltf::getErrorName(error), fastgltf::getErrorMessage(error));
        PFR_ASSERT(false, "Failed to load mesh!");
    }
    LOG_INFO("FASTGLTF: \"{}\" has ({}) buffers, ({}) textures, ({}) animations, ({}) materials, ({}) meshes.", meshFilePath.string(),
             asset->buffers.size(), asset->animations.size(), asset->textures.size(), asset->materials.size(), asset->meshes.size());

    std::string currentMeshDir = meshFilePath.parent_path().string() + "/";
    PFR_ASSERT(!currentMeshDir.empty(), "Current mesh directory path invalid!");

    UnorderedMap<std::string, Shared<Texture>> loadedTextures;
    for (size_t meshIndex{}; meshIndex < asset->meshes.size(); ++meshIndex)
    {
        LoadSubmeshes(loadedTextures, submeshes, currentMeshDir, asset.get(), meshIndex);
    }

    submeshes.shrink_to_fit();
    LOG_INFO("FASTGLTF: Time taken to load and create mesh - \"{}\": ({:.5f}) seconds.", meshFilePath.string(), t.GetElapsedSeconds());
}

SurfaceMesh MeshManager::GenerateUVSphere(const uint32_t sectorCount, const uint32_t stackCount)
{
    std::vector<glm::vec3> vertices = {};
    const float invStackCount       = 1.f / stackCount;
    const float sectorAngleCoeff    = 2 * glm::pi<float>() / sectorCount;

    for (uint32_t i{}; i <= stackCount; ++i)
    {
        const float stackAngle = glm::pi<float>() / 2 - glm::pi<float>() * i * invStackCount;
        const float xy         = cosf(stackAngle);
        const float z          = sinf(stackAngle);

        for (uint32_t j{}; j <= sectorCount; ++j)
        {
            const float sectorAngle = sectorAngleCoeff * j;
#if 0
            glm::vec3 vertexPos    = glm::vec3(xy * cosf(sectorAngle), xy * sinf(sectorAngle), z);
            glm::vec3 vertexNormal = glm::normalize(vertexPos);
            glm::vec2 texCoord     = glm::vec2((float)j / sectorCount, (float)i / stackCount);

            Vertex vertex = {vertexPos, vertexNormal, texCoord};
#endif

            vertices.emplace_back(xy * glm::cos(sectorAngle), xy * glm::sin(sectorAngle), z);
        }
    }
    vertices.shrink_to_fit();

    std::vector<uint32_t> indices = {};
    for (uint32_t i{}; i < stackCount; ++i)
    {
        uint32_t k1 = i * (sectorCount + 1);
        uint32_t k2 = k1 + sectorCount + 1;

        for (uint32_t j = 0; j < sectorCount; ++j, ++k1, ++k2)
        {
            if (i != 0)
            {
                indices.emplace_back(k1);
                indices.emplace_back(k2);
                indices.emplace_back(k1 + 1);
            }

            if (i != (stackCount - 1))
            {
                indices.emplace_back(k1 + 1);
                indices.emplace_back(k2);
                indices.emplace_back(k2 + 1);
            }
        }
    }
    indices.shrink_to_fit();

    SurfaceMesh mesh = {};
    {
        const BufferSpecification vbSpec = {.DebugName  = "UVSphere_VertexBuffer",
                                            .ExtraFlags = EBufferFlag::BUFFER_FLAG_MAPPED | EBufferFlag::BUFFER_FLAG_DEVICE_LOCAL,
                                            .UsageFlags = EBufferUsage::BUFFER_USAGE_VERTEX};
        mesh.VertexBuffer                = Buffer::Create(vbSpec, vertices.data(), vertices.size() * sizeof(vertices[0]));
    }

    {
        const BufferSpecification ibSpec = {.DebugName  = "UVSphere_IndexBuffer",
                                            .ExtraFlags = EBufferFlag::BUFFER_FLAG_MAPPED | EBufferFlag::BUFFER_FLAG_DEVICE_LOCAL,
                                            .UsageFlags = EBufferUsage::BUFFER_USAGE_INDEX};
        mesh.IndexBuffer                 = Buffer::Create(ibSpec, indices.data(), indices.size() * sizeof(indices[0]));
    }

    return mesh;
}

void MeshManager::LoadSubmeshes(UnorderedMap<std::string, Shared<Texture>>& loadedTextures, std::vector<Shared<Submesh>>& submeshes,
                                const std::string& meshDir, const fastgltf::Asset& asset, const size_t meshIndex)
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
        // INDICES
        PFR_ASSERT(p.indicesAccessor.has_value(), "Non-indexed geometry is not supported!");
        const auto& indicesAccessor = asset.accessors[p.indicesAccessor.value()];
        std::vector<uint32_t> indices(indicesAccessor.count);
        fastgltf::iterateAccessorWithIndex<std::uint32_t>(asset, indicesAccessor,
                                                          [&](uint32_t index, std::size_t idx) { indices[idx] = index; });

        glm::mat4 localTransform = glm::mat4(1.f);
        std::visit(fastgltf::visitor{[&](const fastgltf::Node::TransformMatrix& matrix)
                                     { memcpy(&localTransform, matrix.data(), sizeof(matrix)); },
                                     [&](const fastgltf::TRS& transform)
                                     {
                                         const glm::vec3 tl(transform.translation[0], transform.translation[1], transform.translation[2]);
                                         const glm::quat rot(transform.rotation[0], transform.rotation[1], transform.rotation[2],
                                                             transform.rotation[3]);
                                         const glm::vec3 sc(transform.scale[0], transform.scale[1], transform.scale[2]);

                                         const glm::mat4 tm = glm::translate(glm::mat4(1.f), tl);
                                         const glm::mat4 rm = glm::toMat4(rot);
                                         const glm::mat4 sm = glm::scale(glm::mat4(1.f), sc);

                                         localTransform = tm * rm * sm;
                                     }},
                   fastGLTFnode.transform);

        // POSITION
        const auto positionIt = p.findAttribute("POSITION");
        PFR_ASSERT(positionIt != p.attributes.end(), "Mesh doesn't contain positions?!");

        const auto& positionAccessor = asset.accessors[positionIt->second];
        PFR_ASSERT(positionAccessor.type == fastgltf::AccessorType::Vec3, "Positions can only contain vec3!");

        std::vector<MeshAttributeVertex> attributeVertices(positionAccessor.count);
        std::vector<MeshPositionVertex> rawVertices(positionAccessor.count);
        fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, positionAccessor,
                                                      [&](const glm::vec3& position, std::size_t idx)
                                                      { rawVertices[idx].Position = localTransform * vec4(position, 1.0); });

        constexpr auto packUnorm3x8 = [](const glm::vec3& value) { return glm::u8vec3(value * 127.f + 127.5f); };

        // NORMAL
        if (const auto& normalIt = p.findAttribute("NORMAL"); normalIt != p.attributes.end())
        {
            fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[normalIt->second],
                                                          [&](const glm::vec3& normal, std::size_t idx)
                                                          {
                                                              // NOTE: Decode by using (int32_t(x)/127.0 - 1.0)
                                                              attributeVertices[idx].Normal = packUnorm3x8(normal);
                                                          });
        }

        // TANGENT
        if (const auto& tangentIt = p.findAttribute("TANGENT"); tangentIt != p.attributes.end())
        {
            fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[tangentIt->second],
                                                          [&](const glm::vec4& tangent, std::size_t idx)
                                                          { attributeVertices[idx].Tangent = packUnorm3x8(tangent); });
        }

        // COLOR_0
        if (const auto& color_0_It = p.findAttribute("COLOR_0"); color_0_It != p.attributes.end())
        {
            fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[color_0_It->second],
                                                          [&](const glm::vec4& color, std::size_t idx)
                                                          { attributeVertices[idx].Color = glm::packUnorm4x8(color); });
        }
        else
        {
            for (auto& attributeVertex : attributeVertices)
                attributeVertex.Color = 0xFFFFFFFF;
        }

        // UV
        if (const auto& uvIT = p.findAttribute("TEXCOORD_0"); uvIT != p.attributes.end())
        {
            fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, asset.accessors[uvIT->second],
                                                          [&](const glm::vec2& uv, std::size_t idx) {
                                                              attributeVertices[idx].UV =
                                                                  glm::u16vec2(meshopt_quantizeHalf(uv.x), meshopt_quantizeHalf(uv.y));
                                                          });
        }

        auto& submesh = submeshes.emplace_back(MakeShared<Submesh>());

        // PBR shading model materials
        Shared<Material> material = nullptr;
        if (p.materialIndex.has_value())
        {
            const auto& materialAccessor = asset.materials[p.materialIndex.value()];

            // Assemble gathered textures.
            PBRData pbrData = {
                .BaseColor = glm::make_vec4(materialAccessor.pbrData.baseColorFactor.data()),
                .Roughness = materialAccessor.pbrData.roughnessFactor,
                .Metallic  = materialAccessor.pbrData.metallicFactor,
                .bIsOpaque = materialAccessor.alphaMode == fastgltf::AlphaMode::Opaque,
            };

            Shared<Texture> albedo = nullptr;
            if (materialAccessor.pbrData.baseColorTexture.has_value())
            {
                TextureSpecification albedoTextureSpec = {};

                const size_t textureIndex = materialAccessor.pbrData.baseColorTexture.value().textureIndex;
                albedo = FastGLTFUtils::LoadTexture(loadedTextures, meshDir, textureIndex, asset, EImageFormat::FORMAT_BC7_UNORM);
                pbrData.AlbedoTextureIndex = albedo->GetBindlessIndex();
            }

            Shared<Texture> normalMap = nullptr;
            if (materialAccessor.normalTexture.has_value())
            {
                const size_t textureIndex = materialAccessor.normalTexture.value().textureIndex;
                normalMap = FastGLTFUtils::LoadTexture(loadedTextures, meshDir, textureIndex, asset, EImageFormat::FORMAT_BC5_UNORM);
                pbrData.NormalTextureIndex = normalMap->GetBindlessIndex();
            }

            Shared<Texture> metallicRoughness = nullptr;
            if (materialAccessor.pbrData.metallicRoughnessTexture.has_value())
            {
                const size_t textureIndex = materialAccessor.pbrData.metallicRoughnessTexture.value().textureIndex;
                metallicRoughness =
                    FastGLTFUtils::LoadTexture(loadedTextures, meshDir, textureIndex, asset, EImageFormat::FORMAT_BC5_UNORM, true);
                pbrData.MetallicRoughnessTextureIndex = metallicRoughness->GetBindlessIndex();
            }

            Shared<Texture> emissiveMap = nullptr;
            if (materialAccessor.emissiveTexture.has_value())
            {
                const size_t textureIndex = materialAccessor.emissiveTexture.value().textureIndex;
                emissiveMap = FastGLTFUtils::LoadTexture(loadedTextures, meshDir, textureIndex, asset, EImageFormat::FORMAT_BC7_UNORM);

                pbrData.EmissiveTextureIndex = emissiveMap->GetBindlessIndex();
            }

            Shared<Texture> occlusionMap = nullptr;
            if (materialAccessor.occlusionTexture.has_value())
            {
                const size_t textureIndex = materialAccessor.occlusionTexture.value().textureIndex;
                occlusionMap = FastGLTFUtils::LoadTexture(loadedTextures, meshDir, textureIndex, asset, EImageFormat::FORMAT_BC4_UNORM);

                pbrData.OcclusionTextureIndex = occlusionMap->GetBindlessIndex();
            }

            material = MakeShared<Material>(pbrData);
            submesh->SetMaterial(material);

            material->SetAlbedo(albedo);
            material->SetNormalMap(normalMap);
            material->SetEmissiveMap(emissiveMap);
            material->SetMetallicRoughnessMap(metallicRoughness);
            material->SetOcclusionMap(occlusionMap);
        }

        // In case mesh didn't have any material we force white material.
        if (!material)
        {
            const PBRData pbrData = {.BaseColor = glm::vec4(1.f), .Roughness = 1.f, .Metallic = 1.f, .bIsOpaque = true};
            material              = MakeShared<Material>(pbrData);
            submesh->SetMaterial(material);
        }

        MeshManager::OptimizeMesh(indices, rawVertices, attributeVertices);
        const BufferSpecification bufferSpec = {.ExtraFlags = EBufferFlag::BUFFER_FLAG_DEVICE_LOCAL,
                                                .UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE |
                                                              EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY};
        submesh->m_IndexBuffer               = Buffer::Create(bufferSpec, indices.data(), indices.size() * sizeof(indices[0]));

        submesh->m_VertexPositionBuffer = Buffer::Create(bufferSpec, rawVertices.data(), rawVertices.size() * sizeof(rawVertices[0]));
        submesh->m_VertexAttributeBuffer =
            Buffer::Create(bufferSpec, attributeVertices.data(), attributeVertices.size() * sizeof(attributeVertices[0]));
        submesh->m_BoundingSphere = MeshManager::GenerateBoundingSphere(rawVertices);

        std::vector<uint32_t> meshletVertices;
        std::vector<uint8_t> meshletTriangles;
        std::vector<Meshlet> meshlets;
        MeshManager::BuildMeshlets(indices, rawVertices, meshlets, meshletVertices, meshletTriangles);

        const BufferSpecification meshletBufferSpec = {.ExtraFlags = EBufferFlag::BUFFER_FLAG_DEVICE_LOCAL,
                                                       .UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE};
        submesh->m_MeshletBuffer = Buffer::Create(meshletBufferSpec, meshlets.data(), meshlets.size() * sizeof(meshlets[0]));
        submesh->m_MeshletVerticesBuffer =
            Buffer::Create(meshletBufferSpec, meshletVertices.data(), meshletVertices.size() * sizeof(meshletVertices[0]));
        submesh->m_MeshletTrianglesBuffer =
            Buffer::Create(meshletBufferSpec, meshletTriangles.data(), meshletTriangles.size() * sizeof(meshletTriangles[0]));
    }
}

AABB MeshManager::GenerateAABB(const std::vector<MeshPositionVertex>& points)
{
#if _MSC_VER
    if (AVX2Supported())
    {
        size_t i       = 0;
        __m256 minVecX = _mm256_set1_ps(FLT_MAX), minVecY = _mm256_set1_ps(FLT_MAX), minVecZ = _mm256_set1_ps(FLT_MAX);
        __m256 maxVecX = _mm256_set1_ps(FLT_MIN), maxVecY = _mm256_set1_ps(FLT_MIN), maxVecZ = _mm256_set1_ps(FLT_MIN);

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

            minVecX = _mm256_min_ps(minVecX, pointX);
            minVecY = _mm256_min_ps(minVecY, pointY);
            minVecZ = _mm256_min_ps(minVecZ, pointZ);

            maxVecX = _mm256_max_ps(maxVecX, pointX);
            maxVecY = _mm256_max_ps(maxVecY, pointY);
            maxVecZ = _mm256_max_ps(maxVecZ, pointZ);
        }

        // Gather results and prepare them.
        float minX[8] = {0.0f}, minY[8] = {0.0f}, minZ[8] = {0.0f};
        _mm256_storeu_ps(minX, minVecX);
        _mm256_storeu_ps(minY, minVecY);
        _mm256_storeu_ps(minZ, minVecZ);

        float maxX[8] = {0.0f}, maxY[8] = {0.0f}, maxZ[8] = {0.0f};
        _mm256_storeu_ps(maxX, maxVecX);
        _mm256_storeu_ps(maxY, maxVecY);
        _mm256_storeu_ps(maxZ, maxVecZ);

        glm::vec3 min(minX[0], minY[0], minZ[0]);
        glm::vec3 max(maxX[0], maxY[0], maxZ[0]);
        for (i = 1; i < 8; ++i)
        {
            min = glm::min(min, glm::vec3{minX[i], minY[i], minZ[i]});
            max = glm::max(max, glm::vec3{maxX[i], maxY[i], maxZ[i]});
        }

        // Take into account the remainder.
        for (i = alignedDataSize; i < points.size(); ++i)
        {
            min = glm::min(points[i].Position, min);
            max = glm::max(points[i].Position, max);
        }

        const glm::vec3 center = (max + min) * 0.5f;
        return {.Center = center, .Extents = max - center};
    }
#endif

    glm::vec3 min = glm::vec3(FLT_MAX);
    glm::vec3 max = glm::vec3(FLT_MIN);
    for (const auto& point : points)
    {
        min = glm::min(point.Position, min);
        max = glm::max(point.Position, max);
    }

    const glm::vec3 center = (max + min) * 0.5f;
    return {center, max - center};
}

Sphere MeshManager::GenerateBoundingSphere(const std::vector<MeshPositionVertex>& points)
{
    PFR_ASSERT(!points.empty(), "Empty vertices, can't generate bounding sphere!");

    // First pass - find averaged vertex pos.
    glm::vec3 averagedVertexPos(0.0f);
    for (const auto& point : points)
        averagedVertexPos += point.Position;

    averagedVertexPos /= points.size();
    const auto aabb = GenerateAABB(points);

    // Second pass - find farthest vertices for both averaged vertex position and AABB centroid.
    glm::vec3 farthestVtx[2] = {points[0].Position, points[0].Position};
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

void MeshManager::OptimizeMesh(std::vector<uint32_t>& indices, std::vector<MeshPositionVertex>& rawVertices,
                               std::vector<MeshAttributeVertex>& attributeVertices)
{
    PFR_ASSERT(!rawVertices.empty() && rawVertices.size() == attributeVertices.size(),
               "Vertex streams should have exact same not zero size!");

    const meshopt_Stream vertexStreams[] = {{rawVertices.data(), sizeof(MeshPositionVertex), sizeof(MeshPositionVertex)},
                                            {attributeVertices.data(), sizeof(MeshAttributeVertex), sizeof(MeshAttributeVertex)}};

    // #1 REINDEXING BUFFERS TO GET RID OF REDUNDANT VERTICES.
    std::vector<uint32_t> remappedIndices(indices.size());
    const size_t uniqueVertexCount =
        meshopt_generateVertexRemapMulti(remappedIndices.data(), indices.data(), indices.size(), rawVertices.size(), vertexStreams,
                                         sizeof(vertexStreams) / sizeof(vertexStreams[0]));

    meshopt_remapIndexBuffer(indices.data(), indices.data(), remappedIndices.size(), remappedIndices.data());

    MeshOptimizerUtils::RemapVertexStream<MeshPositionVertex>(uniqueVertexCount, rawVertices, remappedIndices);
    MeshOptimizerUtils::RemapVertexStream<MeshAttributeVertex>(uniqueVertexCount, attributeVertices, remappedIndices);

    // #2 VERTEX CACHE OPTIMIZATION (REORDER TRIANGLES TO MAXIMIZE THE LOCALITY OF REUSED VERTEX REFERENCES IN VERTEX SHADERS)
    meshopt_optimizeVertexCache(indices.data(), indices.data(), indices.size(), rawVertices.size());

    // #3 OVERDRAW OPTIMIZATION (REORDER TRIANGLES TO MINIMIZE OVERDRAW FROM ALL DIRECTIONS)
    constexpr float vertexCacheHitRatio = 1.05f;  // max 5% vertex cache hit ratio can be worse then before
    meshopt_optimizeOverdraw(indices.data(), indices.data(), indices.size(), &rawVertices[0].Position.x, rawVertices.size(),
                             sizeof(rawVertices[0]), vertexCacheHitRatio);
}

void MeshManager::BuildMeshlets(const std::vector<uint32_t>& indices, const std::vector<MeshPositionVertex>& vertexPositions,
                                std::vector<Meshlet>& outMeshlets, std::vector<uint32_t>& outMeshletVertices,
                                std::vector<uint8_t>& outMeshletTriangles)
{
    const size_t maxMeshletCount = meshopt_buildMeshletsBound(indices.size(), MAX_MESHLET_VERTEX_COUNT, MAX_MESHLET_TRIANGLE_COUNT);
    std::vector<meshopt_Meshlet> meshoptMeshlets(maxMeshletCount);
    outMeshletVertices.resize(maxMeshletCount * MAX_MESHLET_VERTEX_COUNT);
    outMeshletTriangles.resize(maxMeshletCount * MAX_MESHLET_TRIANGLE_COUNT * 3);

    const size_t actualMeshletCount =
        meshopt_buildMeshlets(meshoptMeshlets.data(), outMeshletVertices.data(), outMeshletTriangles.data(), indices.data(), indices.size(),
                              &vertexPositions[0].Position.x, vertexPositions.size(), sizeof(vertexPositions[0]), MAX_MESHLET_VERTEX_COUNT,
                              MAX_MESHLET_TRIANGLE_COUNT, MESHLET_CONE_WEIGHT);

    {
        // Trimming
        const meshopt_Meshlet& last = meshoptMeshlets.at(actualMeshletCount - 1);
        outMeshletVertices.resize(last.vertex_offset + last.vertex_count);
        outMeshletVertices.shrink_to_fit();

        outMeshletTriangles.resize(last.triangle_offset + ((last.triangle_count * 3 + 3) & ~3));
        outMeshletTriangles.shrink_to_fit();

        meshoptMeshlets.resize(actualMeshletCount);
        meshoptMeshlets.shrink_to_fit();
    }

    // For optimal performance, it is recommended to further optimize each meshlet in isolation for better triangle and vertex locality
    for (size_t i{}; i < meshoptMeshlets.size(); ++i)
    {
        meshopt_optimizeMeshlet(&outMeshletVertices[meshoptMeshlets[i].vertex_offset],
                                &outMeshletTriangles[meshoptMeshlets[i].triangle_offset], meshoptMeshlets[i].triangle_count,
                                meshoptMeshlets[i].vertex_count);
    }

    outMeshlets.resize(meshoptMeshlets.size());
    for (size_t i{}; i < actualMeshletCount; ++i)
    {
        const auto& meshopt_m = meshoptMeshlets[i];
        const auto bounds     = meshopt_computeMeshletBounds(
            &outMeshletVertices[meshopt_m.vertex_offset], &outMeshletTriangles[meshopt_m.triangle_offset], meshopt_m.triangle_count,
            &vertexPositions[0].Position.x, vertexPositions.size(), sizeof(MeshPositionVertex));

        auto& outMeshlet = outMeshlets.at(i);

        outMeshlet.vertexOffset   = meshopt_m.vertex_offset;
        outMeshlet.vertexCount    = meshopt_m.vertex_count;
        outMeshlet.triangleOffset = meshopt_m.triangle_offset;
        outMeshlet.triangleCount  = meshopt_m.triangle_count;

        outMeshlet.center = glm::vec3(bounds.center[0], bounds.center[1], bounds.center[2]);
        outMeshlet.radius = bounds.radius;

        outMeshlet.coneCutoff = bounds.cone_cutoff_s8;
        for (uint32_t k{}; k < 3; ++k)
            outMeshlet.coneAxis[k] = bounds.cone_axis_s8[k];
    }
}

}  // namespace Pathfinder