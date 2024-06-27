#include <PathfinderPCH.h>
#include "Renderer2D.h"

#include <Core/Application.h>
#include <Core/Window.h>

#include "Pipeline.h"
#include "Shader.h"
#include "Buffer.h"
#include "CommandBuffer.h"
#include "Texture.h"

#include "RenderGraph/RenderGraph.h"

namespace Pathfinder
{

void Renderer2D::Init()
{
    m_RendererData2D = MakeUnique<RendererData2D>();
    memset(&m_Renderer2DStats, 0, sizeof(m_Renderer2DStats));
    m_RendererData2D->Sprites.resize(s_MAX_QUADS);

    ShaderLibrary::Load(std::vector<ShaderSpecification>{{"Quad2D"}});
    ShaderLibrary::WaitUntilShadersLoaded();

    // NOTE: Formats should be the same as ForwardPlus pipeline.
    const GraphicsPipelineOptions quadGPO = {
        .Formats           = {EImageFormat::FORMAT_RGBA16F, EImageFormat::FORMAT_RGBA16F, EImageFormat::FORMAT_D32F},
        .FrontFace         = EFrontFace::FRONT_FACE_COUNTER_CLOCKWISE,
        .PrimitiveTopology = EPrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .bBlendEnable      = true,
        .BlendMode         = EBlendMode::BLEND_MODE_ALPHA,
        .PolygonMode       = EPolygonMode::POLYGON_MODE_FILL,
        .bDepthTest        = true,
        .bDepthWrite       = true,  // NOTE: For now quad 2d, as well as debug pass are the latest, so I write depth.
        .DepthCompareOp    = ECompareOp::COMPARE_OP_GREATER_OR_EQUAL};

    PipelineSpecification quadPipelineSpec = {.DebugName       = "Quad2D",
                                              .PipelineOptions = MakeOptional<GraphicsPipelineOptions>(quadGPO),
                                              .Shader          = ShaderLibrary::Get("Quad2D"),
                                              .PipelineType    = EPipelineType::PIPELINE_TYPE_GRAPHICS};
    m_RendererData2D->QuadPipelineHash     = PipelineLibrary::Push(quadPipelineSpec);

    const auto& windowSpec       = Application::Get().GetWindow()->GetSpecification();
    m_RendererData2D->Quad2DPass = Quad2DPass(windowSpec.Width, windowSpec.Height, m_RendererData2D->QuadPipelineHash);

    Application::Get().GetWindow()->AddResizeCallback([&](const WindowResizeData& resizeData)
                                                      { m_RendererData2D->Quad2DPass.OnResize(resizeData.Width, resizeData.Height); });

    PipelineLibrary::Compile();
    LOG_TRACE("{}", __FUNCTION__);
}

void Renderer2D::Shutdown()
{
    m_RendererData2D.reset();
    LOG_TRACE("{}", __FUNCTION__);
}

void Renderer2D::Begin(const uint8_t frameIndex)
{
    m_Renderer2DStats = {};

    m_RendererData2D->FrameIndex         = frameIndex;
    m_RendererData2D->CurrentSpriteCount = 0;
}

void Renderer2D::Flush(Unique<RenderGraph>& renderGraph)
{
    m_RendererData2D->Quad2DPass.AddPass(renderGraph, m_RendererData2D->Sprites, m_RendererData2D->CurrentSpriteCount);
}

void Renderer2D::DrawQuad(const glm::vec3& translation, const glm::vec3& scale, const glm::vec4& orientation, const glm::vec4& color,
                          const Shared<Texture>& texture, const uint32_t layer)
{
    if (m_Renderer2DStats.QuadCount >= s_MAX_QUADS)
    {
        LOG_WARN("m_Renderer2DStats.QuadCount >= m_RendererData2D->s_MAX_QUADS");
        return;
    }

    const uint32_t defaultBindlessTextureIndex                      = TextureManager::GetWhiteTexture()->GetBindlessIndex();
    m_RendererData2D->Sprites[m_RendererData2D->CurrentSpriteCount] = Sprite(
        translation, scale, orientation, glm::packUnorm4x8(color), texture ? texture->GetBindlessIndex() : defaultBindlessTextureIndex);

    ++m_Renderer2DStats.QuadCount;
    m_Renderer2DStats.TriangleCount += 2;
    ++m_RendererData2D->CurrentSpriteCount;
}

}  // namespace Pathfinder