#ifndef SHADER_H
#define SHADER_H

#include "Core/Core.h"
#include <map>
#include <string>

#include <shaderc/shaderc.hpp>

namespace Pathfinder
{

static constexpr uint16_t s_SHADER_EXTENSIONS_SIZE                                                = 13;
static constexpr std::array<const std::string_view, s_SHADER_EXTENSIONS_SIZE> s_SHADER_EXTENSIONS = {
    ".vert", ".tesc", ".tese", ".geom", ".frag", ".comp", ".rmiss", ".rgen", ".rchit", ".rahit", ".rcall", ".mesh", ".task"};

enum class EShaderStage : uint8_t
{
    SHADER_STAGE_VERTEX = 0,
    SHADER_STAGE_TESSELLATION_CONTROL,
    SHADER_STAGE_TESSELLATION_EVALUATION,
    SHADER_STAGE_GEOMETRY,
    SHADER_STAGE_FRAGMENT,
    SHADER_STAGE_COMPUTE,
    SHADER_STAGE_ALL_GRAPHICS,
    SHADER_STAGE_ALL,
    SHADER_STAGE_RAYGEN,
    SHADER_STAGE_ANY_HIT,
    SHADER_STAGE_CLOSEST_HIT,
    SHADER_STAGE_MISS,
    SHADER_STAGE_INTERSECTION,
    SHADER_STAGE_CALLABLE,
    SHADER_STAGE_TASK,
    SHADER_STAGE_MESH,
};

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

class Shader : private Uncopyable, private Unmovable
{
  public:
    virtual ~Shader() = default;

    NODISCARD static Shared<Shader> Create(const std::string_view shaderName);

    virtual void DestroyReflectionGarbage() = 0;

  protected:
    Shader()               = default;
    virtual void Destroy() = 0;
};

class ShaderLibrary final : private Uncopyable, private Unmovable
{
  public:
    ShaderLibrary()           = default;
    ~ShaderLibrary() override = default;

    static void Init();
    static void Shutdown();
    static void DestroyReflectionGarbage();

    static void Load(const std::string& shaderName);
    NODISCARD static const Shared<Shader>& Get(const std::string& shaderName);

  private:
    static inline std::map<std::string, Shared<Shader>> s_Shaders;
};

}  // namespace Pathfinder

#endif  // SHADER_H
