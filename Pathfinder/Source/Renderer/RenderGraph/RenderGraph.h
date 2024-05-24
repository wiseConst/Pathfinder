#pragma once

#include <Core/Core.h>
#include <Renderer/Buffer.h>
#include <Renderer/Image.h>
#include <Renderer/Texture2D.h>

namespace Pathfinder
{

enum class EPassType : uint8_t
{
    PASS_TYPE_GRAPHICS = 0,
    PASS_TYPE_COMPUTE,
    PASS_TYPE_RAY_TRACING,
};

using PassCallbackFn = std::function<void()>;
class Pass final : private Uncopyable, private Unmovable
{
  public:
    Pass(const std::string& debugName, const EPassType passType, const PassCallbackFn& executeCallback) noexcept
        : m_DebugName(debugName), m_PassType(passType), m_ExecuteCallback(executeCallback)
    {
        LOG_DEBUG("Pass {} created!", debugName);
    }
    ~Pass() = default;

  private:
    std::string m_DebugName          = s_DEFAULT_STRING;
    EPassType m_PassType             = EPassType::PASS_TYPE_GRAPHICS;
    PassCallbackFn m_ExecuteCallback = {};

    Pass() = delete;
};

class RenderGraph final : private Uncopyable, private Unmovable
{
  public:
    RenderGraph(const std::string_view& debugName) : m_DebugName(debugName) { LOG_INFO("RenderGraph {} created!", m_DebugName); }
    ~RenderGraph() = default;

    NODISCARD FORCEINLINE Unique<Pass>& AddPass(const std::string& name, const EPassType passType,
                                                const PassCallbackFn& executeCallback) noexcept
    {
        PFR_ASSERT(!name.empty(), "Pass name is empty!");
        PFR_ASSERT(!m_PassNameToIndex.contains(name), "Pass already present!");

        m_PassNameToIndex[name] = m_Passes.size();
        return m_Passes.emplace_back(MakeUnique<Pass>(name, passType, executeCallback));
    }

  private:
    std::string m_DebugName = s_DEFAULT_STRING;
    std::vector<Unique<Pass>> m_Passes;
    std::unordered_map<std::string, std::uint32_t> m_PassNameToIndex;

    RenderGraph() = delete;
};
}  // namespace Pathfinder