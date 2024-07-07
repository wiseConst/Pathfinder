#pragma once

#include <Core/Core.h>
#include "DescriptorManager.h"
#include "RendererCoreDefines.h"
#include "Renderer2D.h"
#include "Layers/UILayer.h"

#include "CPUProfiler.h"
#include "GPUProfiler.h"

#include <Renderer/RenderGraph/RenderGraphPass.h>
#include <Renderer/RenderGraph/RenderGraphResourcePool.h>

#include <Renderer/Passes/GBufferPass.h>
#include <Renderer/Passes/DepthPrePass.h>
#include <Renderer/Passes/AOBlur.h>
#include <Renderer/Passes/FinalComposite.h>
#include <Renderer/Passes/FramePreparePass.h>
#include <Renderer/Passes/SSAO.h>
#include <Renderer/Passes/ScreenSpaceShadowsPass.h>
#include <Renderer/Passes/BloomPass.h>
#include <Renderer/Passes/LightCulling.h>
#include <Renderer/Passes/ObjectCulling.h>
#include <Renderer/Passes/CascadedShadowMapPass.h>

namespace Pathfinder
{

// TODO: Create samplers array for bindless usage, replace every cache thing with *ObjectKey

class Buffer;
class Texture;
class Pipeline;
class Camera;
class Image;
class Submesh;
class Mesh;

class Renderer final
{
  public:
    static void Init();
    static void Shutdown();

    static void Begin();
    static void Flush(const Unique<UILayer>& uiLayer = nullptr);

    static void BeginScene(const Camera& camera);
    static void EndScene();

    static void DrawQuad(const glm::vec3& translation, const glm::vec3& scale, const glm::vec4& orientation,
                         const glm::vec4& color = glm::vec4(1.0f), const Shared<Texture>& texture = nullptr, const uint32_t layer = 0);

    static void SubmitMesh(const Shared<Mesh>& mesh, const glm::vec3& translation = glm::vec3(0.0f),
                           const glm::vec3& scale = glm::vec3(1.0f), const glm::vec4& orientation = glm::vec4(0.f, 0.f, 0.f, 1.f));
    static void AddDirectionalLight(const DirectionalLight& dl);
    static void AddPointLight(const PointLight& pl);
    static void AddSpotLight(const SpotLight& sl);

    static void BindPipeline(const Shared<CommandBuffer>& commandBuffer, Shared<Pipeline> pipeline);

    static const std::map<std::string, Shared<Image>> GetRenderTargetList();

    static const auto& GetCPUProfilerResults()
    {
        PFR_ASSERT(s_RendererData, "RendererData is not valid!");
        return s_RendererData->CachedCPUTimers;
    }

    static const auto& GetGPUProfilerResults()
    {
        PFR_ASSERT(s_RendererData, "RendererData is not valid!");
        return s_RendererData->CachedGPUTimers;
    }

    static const std::vector<std::pair<std::string, uint64_t>>& GetPipelineStatistics()
    {
        PFR_ASSERT(s_RendererData, "RendererData is not valid!");
        return s_RendererData->CachedPipelineStats;
    }

    NODISCARD FORCEINLINE static const auto& GetRendererData()
    {
        PFR_ASSERT(s_RendererData, "RendererData is not valid!");
        return s_RendererData;
    }

    NODISCARD FORCEINLINE static const auto& GetDescriptorManager()
    {
        PFR_ASSERT(s_DescriptorManager, "DescriptorManager is not valid!");
        return s_DescriptorManager;
    }

    NODISCARD FORCEINLINE static auto& GetRendererSettings() { return s_RendererSettings; }
    NODISCARD FORCEINLINE static auto& GetStats() { return s_RendererStats; }

    NODISCARD FORCEINLINE static bool IsWorldEmpty()
    {
        return s_RendererData && s_RendererData->TransparentObjects.empty() && s_RendererData->OpaqueObjects.empty();
    }

  private:
    Renderer()  = delete;
    ~Renderer() = default;

    struct RenderObject
    {
        Shared<Submesh> submesh = nullptr;
        glm::vec3 Translation   = glm::vec3(0.f);
        glm::vec3 Scale         = glm::vec3(1.f);
        glm::vec4 Orientation   = glm::vec4(0.f, 0.f, 0.f, 1.f);
    };

    struct RendererData
    {
        Unique<Renderer2D> R2D = nullptr;

        // MISC
        Unique<LightData> LightStruct = nullptr;
        CameraData CameraStruct;

        // NOTE: PerFramed should be objects that are used by host and device.
        BufferPerFrame UploadHeap;

        std::vector<ProfilerTask> CachedCPUTimers;
        Pathfinder::CPUProfiler CPUProfiler;

        std::vector<std::pair<std::string, uint64_t>> CachedPipelineStats;
        std::vector<ProfilerTask> CachedGPUTimers;
        Pathfinder::GPUProfiler GPUProfiler;

        static constexpr size_t s_MAX_UPLOAD_HEAP_CAPACITY = 32 * 1024 * 1024;  // 32 MB
        uint8_t FrameIndex                                 = 0;

        RGResourcePool ResourcePool;
        Weak<Pipeline> LastBoundPipeline;

        // Rendering
        CommandBufferPerFrame RenderCommandBuffer;
        Pathfinder::FramePreparePass FramePreparePass;

        // Final
        Pathfinder::FinalCompositePass FinalCompositePass;
        uint64_t CompositePipelineHash = 0;

        // Forward+
        Pathfinder::GBufferPass GBufferPass;
        uint64_t ForwardPlusOpaquePipelineHash      = 0;
        uint64_t ForwardPlusTransparentPipelineHash = 0;

        // DepthPrePass
        Pathfinder::DepthPrePass DepthPrePass;
        uint64_t DepthPrePassPipelineHash = 0;

        // BLOOM Ping-pong
        Pathfinder::BloomPass BloomPass;
        std::array<uint64_t, 2> BloomPipelineHash;

        // Cascaded Shadows
        std::array<CSMData, MAX_DIR_LIGHTS> ShadowMapData = {0};
        uint64_t CSMPipelineHash                          = 0;
        Pathfinder::CascadedShadowMapPass CascadedShadowMapPass;

        /*             SCREEN-SPACE SHADOWS                */
        bool bAnybodyCastsShadows      = false;
        uint64_t SSShadowsPipelineHash = 0;
        Pathfinder::ScreenSpaceShadowsPass SSSPass;
        /*             SCREEN-SPACE SHADOWS                */

        std::vector<RenderObject> OpaqueObjects;
        std::vector<RenderObject> TransparentObjects;

        // Light-Culling
        uint64_t ComputeFrustumsPipelineHash = 0;
        uint64_t LightCullingPipelineHash    = 0;
        Pathfinder::LightCullingPass LightCullingPass;

        // AO
        Shared<Texture> AONoiseTexture = nullptr;

        Pathfinder::SSAOPass SSAOPass;
        uint64_t SSAOPipelineHash = 0;

        Pathfinder::AOBlurPass AOBlurPass;
        uint64_t BoxBlurAOPipelineHash = 0;

        // Indirect Rendering
        struct ObjectCullStatistics
        {
            uint32_t DrawCountOpaque;
            uint32_t DrawCountTransparent;
        } ObjectCullStats;
        uint64_t ObjectCullingPipelineHash = 0;
        Pathfinder::ObjectCullingPass ObjectCullingPass;
        bool bIsFrameBegin = false;
    };

    static inline Unique<RendererData> s_RendererData           = nullptr;
    static inline Shared<DescriptorManager> s_DescriptorManager = nullptr;

    struct RendererSettings
    {
        bool bVSync;
        bool bDrawColliders;
        bool bCollectGPUStats;
    };

    static inline RendererSettings s_RendererSettings;

    struct RendererStats
    {
        std::atomic<uint64_t> TriangleCount;
        std::atomic<uint64_t> ObjectsDrawn;
        std::atomic<uint64_t> BarrierCount;
        std::atomic<uint64_t> BarrierBatchCount;
        std::atomic<uint64_t> QuadCount;
        float GPUTime;
        float SwapchainPresentTime;
        uint32_t ImageViewCount;
        std::vector<MemoryBudget> MemoryBudgets;

        void Reset()
        {
            TriangleCount.store(0);
            ObjectsDrawn.store(0);
            BarrierCount.store(0);
            BarrierBatchCount.store(0);
            QuadCount.store(0);
            GPUTime              = 0.f;
            SwapchainPresentTime = 0.f;
            ImageViewCount       = 0;
            MemoryBudgets.clear();
        }
    };

    static inline RendererStats s_RendererStats = {};

    static void CreatePipelines();
};

}  // namespace Pathfinder