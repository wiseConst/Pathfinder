#include "PathfinderPCH.h"
#include "MeshManager.h"

#include "Submesh.h"
#include "Globals.h"

#include "Core/Application.h"
#include "Core/Threading.h"
#include "Core/Intrinsics.h"

#include "Renderer/Buffer.h"
#include "Renderer/Material.h"
#include "Renderer/Image.h"
#include "Renderer/Texture2D.h"
#include "Renderer/Renderer.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>

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
}  // namespace FastGLTFUtils

Shared<Texture2D> MeshManager::LoadTexture(std::unordered_map<std::string, Shared<Texture2D>>& loadedTextures,
                                           const std::string& meshAssetsDir, const fastgltf::Asset& asset,
                                           const fastgltf::Material& materialAccessor, const size_t textureIndex,
                                           TextureSpecification& textureSpec, const bool bMetallicRoughness)
{
    const auto& fastgltfTexture = asset.textures[textureIndex];
    const auto imageIndex       = fastgltfTexture.imageIndex;
    PFR_ASSERT(imageIndex.has_value(), "Invalid image index!");

    if (fastgltfTexture.samplerIndex.has_value())
    {
        const auto& fastgltfTextureSampler = asset.samplers[fastgltfTexture.samplerIndex.value()];

        if (fastgltfTextureSampler.magFilter.has_value())
            textureSpec.Filter = FastGLTFUtils::SamplerFilterToPathfinder(fastgltfTextureSampler.magFilter.value());

        textureSpec.Wrap = FastGLTFUtils::SamplerWrapToPathfinder(fastgltfTextureSampler.wrapS);
    }

    const auto& imageData = asset.images[imageIndex.value()].data;
    PFR_ASSERT(std::holds_alternative<fastgltf::sources::URI>(imageData), "Texture hasn't path!");
    const auto& fastgltfURI = std::get<fastgltf::sources::URI>(imageData);

    const std::string textureName = fastgltfURI.uri.fspath().stem().string();
    PFR_ASSERT(textureName.data(), "Texture has no name!");
    if (loadedTextures.contains(textureName)) return loadedTextures[textureName];

    const auto& appSpec = Application::Get().GetSpecification();
    const auto ind      = meshAssetsDir.find(appSpec.MeshDir);
    PFR_ASSERT(ind != std::string::npos, "Failed to find meshes substr index!");
    const std::string meshDir = meshAssetsDir.substr(ind);  // contains something like: "Meshes/sponza/"
    const auto currentMeshTextureCacheDir =
        std::filesystem::path(appSpec.WorkingDir) / appSpec.AssetsDir / appSpec.CacheDir / meshDir / "textures/";
    if (!std::filesystem::exists(currentMeshTextureCacheDir)) std::filesystem::create_directories(currentMeshTextureCacheDir);

    Shared<Texture2D> texture = nullptr;
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
                    texture = Texture2D::Create(textureSpec, compressedImage.data(), compressedImage.size());
                }
                else
                {
                    int32_t x = 1, y = 1, channels = 4;
                    const std::string texturePath   = meshAssetsDir + std::string(uri.uri.string());
                    void* uncompressedData          = ImageUtils::LoadRawImage(texturePath, textureSpec.bFlipOnLoad, &x, &y, &channels);
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
                        texture = Texture2D::Create(textureSpec, whatToCompress, whatToCompressSize);
                    }
                    else
                    {
                        void* compressedData      = nullptr;
                        size_t compressedDataSize = 0;

                        TextureCompressor::Compress(textureSpec, srcImageFormat, whatToCompress, whatToCompressSize, &compressedData,
                                                    compressedDataSize);

                        texture = Texture2D::Create(textureSpec, compressedData, compressedDataSize);
                        TextureCompressor::SaveCompressed(textureCacheFilePath, textureSpec, compressedData, compressedDataSize);
                        free(compressedData);
                    }

                    ImageUtils::UnloadRawImage(uncompressedData);
                    if (channels == 3) delete[] rgbToRgbaBuffer;
                }
            }},
        imageData);

    loadedTextures[textureName] = texture;
    return texture;
}

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

    fastgltf::Expected<fastgltf::Asset> asset = parser.loadGltf(&data, meshFilePath.parent_path(), gltfOptions);
    if (const auto error = asset.error(); error != fastgltf::Error::None)
    {
        LOG_TAG_ERROR(FASTGLTF, "Error occured while loading mesh \"%s\"! Error name: %s / Message: %s", meshFilePath.string().data(),
                      fastgltf::getErrorName(error).data(), fastgltf::getErrorMessage(error).data());
        PFR_ASSERT(false, "Failed to load mesh!");
    }
    LOG_TAG_INFO(FASTGLTF, "\"%s\" has (%zu) buffers, (%zu) textures, (%zu) animations, (%zu) materials, (%zu) meshes.",
                 meshFilePath.string().data(), asset->buffers.size(), asset->animations.size(), asset->textures.size(),
                 asset->materials.size(), asset->meshes.size());

    std::string currentMeshDir = meshFilePath.parent_path().string() + "/";
    PFR_ASSERT(!currentMeshDir.empty(), "Current mesh directory path invalid!");
    std::unordered_map<std::string, Shared<Texture2D>> loadedTextures;
    for (size_t meshIndex = 0; meshIndex < asset->meshes.size(); ++meshIndex)
    {
        LoadSubmeshes(submeshes, currentMeshDir, loadedTextures, asset.get(), meshIndex);
    }

#if PFR_DEBUG
    PFR_ASSERT(fastgltf::validate(asset.get()) == fastgltf::Error::None, "Asset is not valid after processing?");
#endif

    submeshes.shrink_to_fit();

    LOG_TAG_INFO(FASTGLTF, "Time taken to load and create mesh - \"%s\": (%0.5f) seconds.", meshFilePath.string().data(),
                 t.GetElapsedSeconds());
}

SurfaceMesh MeshManager::GenerateUVSphere(const uint32_t sectorCount, const uint32_t stackCount)
{
    std::vector<glm::vec3> vertices = {};

    for (uint32_t i = 0; i <= stackCount; ++i)
    {
        const float stackAngle = glm::pi<float>() / 2 - glm::pi<float>() * i / stackCount;
        const float xy         = cosf(stackAngle);
        const float z          = sinf(stackAngle);

        for (int j = 0; j <= sectorCount; ++j)
        {
            const float sectorAngle = 2 * glm::pi<float>() * j / sectorCount;
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
    for (uint32_t i = 0; i < stackCount; ++i)
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
        BufferSpecification vbSpec = {EBufferUsage::BUFFER_USAGE_VERTEX};
        vbSpec.Data                = vertices.data();
        vbSpec.DataSize            = vertices.size() * sizeof(vertices[0]);
        mesh.VertexBuffer          = Buffer::Create(vbSpec);
    }

    {
        BufferSpecification ibSpec = {EBufferUsage::BUFFER_USAGE_INDEX};
        ibSpec.Data                = indices.data();
        ibSpec.DataSize            = indices.size() * sizeof(indices[0]);
        mesh.IndexBuffer           = Buffer::Create(ibSpec);
    }

    return mesh;
}

void MeshManager::LoadSubmeshes(std::vector<Shared<Submesh>>& submeshes, const std::string& meshDir,
                                std::unordered_map<std::string, Shared<Texture2D>>& loadedTextures, const fastgltf::Asset& asset,
                                const size_t meshIndex)
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

        std::vector<MeshManager::MeshoptimizeVertex> meshoptimizeVertices(positionAccessor.count);
        fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, positionAccessor,
                                                      [&](const glm::vec3& position, std::size_t idx)
                                                      { meshoptimizeVertices[idx].Position = localTransform * vec4(position, 1.0); });

        // NORMAL
        if (const auto& normalIt = p.findAttribute("NORMAL"); normalIt != p.attributes.end())
        {
            fastgltf::iterateAccessorWithIndex<glm::vec3>(asset, asset.accessors[normalIt->second],
                                                          [&](const glm::vec3& normal, std::size_t idx)
                                                          { meshoptimizeVertices[idx].Normal = normal; });
        }

        // TANGENT
        if (const auto& tangentIt = p.findAttribute("TANGENT"); tangentIt != p.attributes.end())
        {
            fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[tangentIt->second],
                                                          [&](const glm::vec4& tangent, std::size_t idx)
                                                          { meshoptimizeVertices[idx].Tangent = tangent; });
        }

        // COLOR_0
        if (const auto& color_0_It = p.findAttribute("COLOR_0"); color_0_It != p.attributes.end())
        {
            fastgltf::iterateAccessorWithIndex<glm::vec4>(asset, asset.accessors[color_0_It->second],
                                                          [&](const glm::vec4& color, std::size_t idx)
                                                          { meshoptimizeVertices[idx].Color = PackVec4ToU8Vec4(color); });
        }

        // UV
        if (const auto& uvIT = p.findAttribute("TEXCOORD_0"); uvIT != p.attributes.end())
        {
            fastgltf::iterateAccessorWithIndex<glm::vec2>(asset, asset.accessors[uvIT->second],
                                                          [&](const glm::vec2& uv, std::size_t idx) {
                                                              meshoptimizeVertices[idx].UV =
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
            PBRData pbrData   = {};
            pbrData.BaseColor = glm::make_vec4(materialAccessor.pbrData.baseColorFactor.data());
            pbrData.Metallic  = materialAccessor.pbrData.metallicFactor;
            pbrData.Roughness = materialAccessor.pbrData.roughnessFactor;
            pbrData.bIsOpaque = materialAccessor.alphaMode == fastgltf::AlphaMode::Opaque;

            Shared<Texture2D> albedo = nullptr;
            if (materialAccessor.pbrData.baseColorTexture.has_value())
            {
                TextureSpecification albedoTextureSpec = {};
                albedoTextureSpec.Format               = EImageFormat::FORMAT_BC7_UNORM;
                albedoTextureSpec.bBindlessUsage       = true;

                const auto& textureInfo = materialAccessor.pbrData.baseColorTexture.value();
                albedo = LoadTexture(loadedTextures, meshDir, asset, materialAccessor, textureInfo.textureIndex, albedoTextureSpec);
                pbrData.AlbedoTextureIndex = albedo->GetBindlessIndex();
            }

            Shared<Texture2D> normalMap = nullptr;
            if (materialAccessor.normalTexture.has_value())
            {
                TextureSpecification normalMapTextureSpec = {};
                normalMapTextureSpec.Format               = EImageFormat::FORMAT_BC5_UNORM;
                normalMapTextureSpec.bBindlessUsage       = true;

                const auto& textureInfo = materialAccessor.normalTexture.value();
                normalMap = LoadTexture(loadedTextures, meshDir, asset, materialAccessor, textureInfo.textureIndex, normalMapTextureSpec);
                pbrData.NormalTextureIndex = normalMap->GetBindlessIndex();
            }

            Shared<Texture2D> metallicRoughness = nullptr;
            if (materialAccessor.pbrData.metallicRoughnessTexture.has_value())
            {
                TextureSpecification metallicRoughnessTextureSpec = {};
                metallicRoughnessTextureSpec.Format               = EImageFormat::FORMAT_BC5_UNORM;
                metallicRoughnessTextureSpec.bBindlessUsage       = true;

                const auto& textureInfo = materialAccessor.pbrData.metallicRoughnessTexture.value();
                metallicRoughness       = LoadTexture(loadedTextures, meshDir, asset, materialAccessor, textureInfo.textureIndex,
                                                      metallicRoughnessTextureSpec, true);
                pbrData.MetallicRoughnessTextureIndex = metallicRoughness->GetBindlessIndex();
            }

            Shared<Texture2D> emissiveMap = nullptr;
            if (materialAccessor.emissiveTexture.has_value())
            {
                TextureSpecification emissiveTextureSpec = {};
                emissiveTextureSpec.Format               = EImageFormat::FORMAT_BC7_UNORM;
                emissiveTextureSpec.bBindlessUsage       = true;

                const auto& textureInfo = materialAccessor.emissiveTexture.value();
                emissiveMap = LoadTexture(loadedTextures, meshDir, asset, materialAccessor, textureInfo.textureIndex, emissiveTextureSpec);
                pbrData.EmissiveTextureIndex = emissiveMap->GetBindlessIndex();
            }

            Shared<Texture2D> occlusionMap = nullptr;
            if (materialAccessor.occlusionTexture.has_value())
            {
                TextureSpecification occlusionTextureSpec = {};
                occlusionTextureSpec.Format               = EImageFormat::FORMAT_BC4_UNORM;
                occlusionTextureSpec.bBindlessUsage       = true;

                const auto& textureInfo = materialAccessor.occlusionTexture.value();
                occlusionMap =
                    LoadTexture(loadedTextures, meshDir, asset, materialAccessor, textureInfo.textureIndex, occlusionTextureSpec);
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
            PBRData pbrData   = {};
            pbrData.BaseColor = glm::vec4(1.f);
            pbrData.Metallic  = 1.f;
            pbrData.Roughness = 1.f;
            pbrData.bIsOpaque = true;
            material          = MakeShared<Material>(pbrData);
            submesh->SetMaterial(material);
        }

        std::vector<uint32_t> finalIndices;
        std::vector<MeshManager::MeshoptimizeVertex> finalVertices;
        MeshManager::OptimizeMesh(indices, meshoptimizeVertices, finalIndices, finalVertices);

        BufferSpecification ibSpec = {EBufferUsage::BUFFER_USAGE_STORAGE, STORAGE_BUFFER_INDEX_BINDING, true};
        ibSpec.Data                = finalIndices.data();
        ibSpec.DataSize            = finalIndices.size() * sizeof(finalIndices[0]);
        ibSpec.BufferUsage |=
            EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS;
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

        BufferSpecification vbPosSpec = {EBufferUsage::BUFFER_USAGE_STORAGE, STORAGE_BUFFER_VERTEX_POS_BINDING, true};
        vbPosSpec.Data                = vertexPositions.data();
        vbPosSpec.DataSize            = vertexPositions.size() * sizeof(vertexPositions[0]);
        vbPosSpec.BufferUsage |=
            EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS;
        submesh->m_VertexPositionBuffer = Buffer::Create(vbPosSpec);

        BufferSpecification vbAttribSpec = {EBufferUsage::BUFFER_USAGE_STORAGE, STORAGE_BUFFER_VERTEX_ATTRIB_BINDING, true};
        vbAttribSpec.Data                = vertexAttributes.data();
        vbAttribSpec.DataSize            = vertexAttributes.size() * sizeof(vertexAttributes[0]);
        vbAttribSpec.BufferUsage |=
            EBufferUsage::BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY | EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS;
        submesh->m_VertexAttributeBuffer = Buffer::Create(vbAttribSpec);

        submesh->m_BoundingSphere = MeshManager::GenerateBoundingSphere(vertexPositions);

        std::vector<uint32_t> meshletVertices;
        std::vector<uint8_t> meshletTriangles;
        std::vector<Meshlet> meshlets;
        MeshManager::BuildMeshlets(finalIndices, vertexPositions, meshlets, meshletVertices, meshletTriangles);

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

AABB MeshManager::GenerateAABB(const std::vector<MeshPositionVertex>& points)
{
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
        float minX[8] = {0}, minY[8] = {0}, minZ[8] = {0};
        _mm256_storeu_ps(minX, minVecX);
        _mm256_storeu_ps(minY, minVecY);
        _mm256_storeu_ps(minZ, minVecZ);

        float maxX[8] = {0}, maxY[8] = {0}, maxZ[8] = {0};
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
        return {center, max - center};
    }

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
#if TEST_LATER
    if (AVX2Supported())
    {
        size_t i       = 0;
        __m256 sumVecX = _mm256_set1_ps(0.0f), sumVecY = _mm256_set1_ps(0.0f), sumVecZ = _mm256_set1_ps(0.0f);

        const size_t alignedDataSize = (points.size() & ~7);
        for (i = 0; i < alignedDataSize; i += 8)
        {
            const auto* pointPtr = &points[i];

            const __m256 pointX =
                _mm256_set_ps(pointPtr[7].Position.x, pointPtr[6].Position.x, pointPtr[5].Position.x, pointPtr[4].Position.x,
                              pointPtr[3].Position.x, pointPtr[2].Position.x, pointPtr[1].Position.x, pointPtr[0].Position.x);
            sumVecX = _mm256_add_ps(sumVecX, pointX);

            const __m256 pointY =
                _mm256_set_ps(pointPtr[7].Position.y, pointPtr[6].Position.y, pointPtr[5].Position.y, pointPtr[4].Position.y,
                              pointPtr[3].Position.y, pointPtr[2].Position.y, pointPtr[1].Position.y, pointPtr[0].Position.y);
            sumVecY = _mm256_add_ps(sumVecY, pointY);

            const __m256 pointZ =
                _mm256_set_ps(pointPtr[7].Position.z, pointPtr[6].Position.z, pointPtr[5].Position.z, pointPtr[4].Position.z,
                              pointPtr[3].Position.z, pointPtr[2].Position.z, pointPtr[1].Position.z, pointPtr[0].Position.z);
            sumVecZ = _mm256_add_ps(sumVecZ, pointZ);
        }

        // Gather results and prepare them.
        float sumX[8] = {0}, sumY[8] = {0}, sumZ[8] = {0};
        _mm256_storeu_ps(sumX, sumVecX);
        _mm256_storeu_ps(sumY, sumVecY);
        _mm256_storeu_ps(sumZ, sumVecZ);

        for (i = 0; i < 8; ++i)
            averagedVertexPos += glm::vec3{sumX[i], sumY[i], sumZ[i]};

        // Take into account the remainder.
        for (i = alignedDataSize; i < points.size(); ++i)
        {
            averagedVertexPos += points[i].Position;
        }
    }
    else
#endif
    {
        for (const auto& point : points)
            averagedVertexPos += point.Position;
    }

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

    // LOG_INFO("Time taken to create sphere from %u points: %0.3f ms", points.size(), t.GetElapsedMilliseconds());
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
    for (size_t i{}; i < actualMeshletCount; ++i)
    {
        const auto& meshopt_m = meshopt_meshlets[i];
        const auto bounds     = meshopt_computeMeshletBounds(
            &outMeshletVertices[meshopt_m.vertex_offset], &outMeshletTriangles[meshopt_m.triangle_offset], meshopt_m.triangle_count,
            &vertexPositions[0].Position.x, vertexPositions.size(), sizeof(vertexPositions[0]));

        outMeshlets[i].vertexOffset   = meshopt_m.vertex_offset;
        outMeshlets[i].vertexCount    = meshopt_m.vertex_count;
        outMeshlets[i].triangleOffset = meshopt_m.triangle_offset;
        outMeshlets[i].triangleCount  = meshopt_m.triangle_count;

        outMeshlets[i].center = glm::vec3(bounds.center[0], bounds.center[1], bounds.center[2]);
        outMeshlets[i].radius = bounds.radius;

        outMeshlets[i].coneApex   = glm::vec3(bounds.cone_apex[0], bounds.cone_apex[1], bounds.cone_apex[2]);
        outMeshlets[i].coneAxis   = glm::vec3(bounds.cone_axis[0], bounds.cone_axis[1], bounds.cone_axis[2]);
        outMeshlets[i].coneCutoff = bounds.cone_cutoff;
    }
}

}  // namespace Pathfinder