#pragma once

#include <Core/Core.h>
#include <map>
#include <string>
#include <future>
#include "RendererCoreDefines.h"

#include <shaderc/shaderc.hpp>

namespace Pathfinder
{

// TODO: Shader Hash instead of std::string?

static constexpr uint8_t s_SHADER_EXTENSIONS_SIZE                                                 = 13;
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
class Pipeline;
class Texture2D;
class TextureCube;

struct ShaderSpecification
{
    std::string Name = s_DEFAULT_STRING;
    std::unordered_map<std::string, std::string> MacroDefinitions;
};

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

    const auto& GetSpecification() const { return m_Specification; }

    virtual bool DestroyGarbageIfNeeded() = 0;  // Returns true if any garbage is destroyed.
    virtual void Invalidate()             = 0;

    virtual ShaderBindingTable CreateSBT(const Shared<Pipeline>& rtPipeline) const = 0;

  protected:
    ShaderSpecification m_Specification = {};

    friend class ShaderLibrary;

    explicit Shader(const ShaderSpecification& shaderSpec);
    virtual void Destroy() = 0;
    NODISCARD static Shared<Shader> Create(const ShaderSpecification& shaderSpec);

    static EShaderStage ShadercShaderStageToPathfinder(const shaderc_shader_kind& shaderKind);
    static void DetectShaderKind(shaderc_shader_kind& shaderKind, const std::string_view currentShaderExt);
    std::vector<uint32_t> CompileOrRetrieveCached(const std::string& shaderName, const std::string& localShaderPath,
                                                  shaderc_shader_kind shaderKind, const bool bHotReload);
};

class ShaderLibrary final : private Uncopyable, private Unmovable
{
  public:
    static void Init();
    static void Shutdown();

    static void Load(const std::vector<ShaderSpecification>& shaderSpecs);
    NODISCARD static const Shared<Shader>& Get(const std::string& shaderName);
    NODISCARD static const Shared<Shader>& Get(const ShaderSpecification& shaderSpec);

    FORCEINLINE static void WaitUntilShadersLoaded()
    {
#if PFR_DEBUG
        Timer t = {};
#endif

        for (auto& future : s_ShaderFutures)
            future.get();

#if PFR_DEBUG
        LOG_INFO("Time took to create ({}) shaders: {:.2f}ms", s_ShaderFutures.size(), t.GetElapsedMilliseconds());
#endif

        s_ShaderFutures.clear();
    }

    FORCEINLINE static void DestroyGarbageIfNeeded()
    {
        size_t shaderGargbageCount = 0;
        std::ranges::for_each(s_Shaders,
                              [&](const std::pair<std::string, Shared<Shader>>& shaderData)
                              {
                                  if (shaderData.second && shaderData.second->DestroyGarbageIfNeeded()) ++shaderGargbageCount;
                              });
        if (shaderGargbageCount != 0) LOG_TRACE("Destroyed ({}) shader garbages!", shaderGargbageCount);
    }

  private:
    static inline std::vector<std::shared_future<void>> s_ShaderFutures;
    static inline std::multimap<std::string, Shared<Shader>> s_Shaders;
    static inline std::mutex s_ShaderLibMutex;

    static void Load(const ShaderSpecification& shaderSpec);
    ShaderLibrary()  = delete;
    ~ShaderLibrary() = default;
};

}  // namespace Pathfinder
