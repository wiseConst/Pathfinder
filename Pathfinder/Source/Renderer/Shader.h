#ifndef SHADER_H
#define SHADER_H

#include "Core/Core.h"
#include <map>
#include <string>
#include "RendererCoreDefines.h"

#include <shaderc/shaderc.hpp>

namespace Pathfinder
{

static constexpr uint16_t s_SHADER_EXTENSIONS_SIZE                                                = 13;
static constexpr std::array<const std::string_view, s_SHADER_EXTENSIONS_SIZE> s_SHADER_EXTENSIONS = {
    ".vert", ".tesc", ".tese", ".geom", ".frag", ".mesh", ".task", ".comp", ".rmiss", ".rgen", ".rchit", ".rahit", ".rcall"};

enum EShaderStage : uint32_t
{
    SHADER_STAGE_VERTEX                  = BIT(0),
    SHADER_STAGE_TESSELLATION_CONTROL    = BIT(1),
    SHADER_STAGE_TESSELLATION_EVALUATION = BIT(2),
    SHADER_STAGE_GEOMETRY                = BIT(3),
    SHADER_STAGE_FRAGMENT                = BIT(4),
    SHADER_STAGE_COMPUTE                 = BIT(5),
    SHADER_STAGE_ALL_GRAPHICS            = BIT(6),
    SHADER_STAGE_ALL                     = BIT(7),
    SHADER_STAGE_RAYGEN                  = BIT(8),
    SHADER_STAGE_ANY_HIT                 = BIT(9),
    SHADER_STAGE_CLOSEST_HIT             = BIT(10),
    SHADER_STAGE_MISS                    = BIT(11),
    SHADER_STAGE_INTERSECTION            = BIT(12),
    SHADER_STAGE_CALLABLE                = BIT(13),
    SHADER_STAGE_TASK                    = BIT(14),
    SHADER_STAGE_MESH                    = BIT(15),
};
typedef uint32_t ShaderStageFlags;

class GLSLShaderIncluder final : public shaderc::CompileOptions::IncluderInterface
{
  public:
    // Handles shaderc_include_resolver_fn callbacks.
    shaderc_include_result* GetInclude(const char* requested_source, shaderc_include_type type, const char* requesting_source,
                                       size_t include_depth) final override;

    // Handles shaderc_include_result_release_fn callbacks.
    void ReleaseInclude(shaderc_include_result* data) final override;

    ~GLSLShaderIncluder() override = default;
};

class Buffer;
class Image;
class Texture2D;
class TextureCube;

class Shader : private Uncopyable, private Unmovable
{
  public:
    virtual ~Shader() = default;



    // NOTE: Updates all frames
    virtual void Set(const std::string_view name, const Shared<Buffer> buffer)    = 0;
    virtual void Set(const std::string_view name, const Shared<Texture2D> texture)    = 0;
    virtual void Set(const std::string_view name, const Shared<Image> attachment) = 0;

    // NOTE: Updates all frames
    virtual void Set(const std::string_view name, const BufferPerFrame& buffers)    = 0;
    virtual void Set(const std::string_view name, const ImagePerFrame& attachments) = 0;

    NODISCARD static Shared<Shader> Create(const std::string_view shaderName);
    virtual void DestroyGarbageIfNeeded() = 0;

  protected:
    Shader() = default;

    virtual void Destroy() = 0;
};

class ShaderLibrary final : private Uncopyable, private Unmovable
{
  public:
    ShaderLibrary()           = default;
    ~ShaderLibrary() override = default;

    static void Init();
    static void Shutdown();

    static void Load(const std::string& shaderName);
    static void Load(const std::vector<std::string>& shaderNames);
    NODISCARD static const Shared<Shader>& Get(const std::string& shaderName);

    FORCEINLINE static void DestroyGarbageIfNeeded()
    {
        std::ranges::for_each(s_Shaders,
                              [](const std::pair<std::string, Shared<Shader>>& shaderData)
                              {
                                  if (shaderData.second) shaderData.second->DestroyGarbageIfNeeded();
                              });
        LOG_TAG_TRACE(SHADER_LIBRARY, "Destroyed (%zu) shader garbages!", s_Shaders.size());
    }

  private:
    static inline std::map<std::string, Shared<Shader>> s_Shaders;
};

}  // namespace Pathfinder

#endif  // SHADER_H
