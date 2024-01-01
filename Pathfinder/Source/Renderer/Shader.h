#ifndef SHADER_H
#define SHADER_H

#include "Core/Core.h"
#include <map>
#include <string>

namespace Pathfinder
{

class Shader : private Uncopyable, private Unmovable
{
  public:
    Shader()          = default;
    virtual ~Shader() = default;

    NODISCARD static Shared<Shader> Create(const std::string_view shaderPath);
  protected:
    virtual void Destroy() = 0;

};

class ShaderLibrary final : private Uncopyable, private Unmovable
{
  public:
    ShaderLibrary()           = default;
    ~ShaderLibrary() override = default;

    static void Init();
    static void Shutdown();

    static void Load(const std::string& shaderPath);
    NODISCARD static Shared<Shader>& Get(const std::string& shaderPath);

  private:
    static inline std::map<std::string, Shared<Shader>> s_Shaders;
};

}  // namespace Pathfinder
#endif  // SHADER_H
