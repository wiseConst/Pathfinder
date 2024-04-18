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
    virtual void Set(const std::string_view name, const Shared<Buffer> buffer)       = 0;
    virtual void Set(const std::string_view name, const Shared<Texture2D> texture)   = 0;
    virtual void Set(const std::string_view name, const Shared<Image> attachment)    = 0;
    virtual void Set(const std::string_view name, const AccelerationStructure& tlas) = 0;

    // NOTE: Updates all frames
    virtual void Set(const std::string_view name, const BufferPerFrame& buffers)                 = 0;
    virtual void Set(const std::string_view name, const ImagePerFrame& attachments)              = 0;
    virtual void Set(const std::string_view name, const std::vector<Shared<Image>>& attachments) = 0;

    NODISCARD static Shared<Shader> Create(const std::string_view shaderName);
    virtual void DestroyGarbageIfNeeded() = 0;

  protected:
    explicit Shader();

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
