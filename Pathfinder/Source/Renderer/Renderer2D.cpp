#include <PathfinderPCH.h>
#include "Renderer2D.h"

#include "Renderer.h"
#include "Pipeline.h"
#include "Shader.h"
#include "Buffer.h"
#include "CommandBuffer.h"
#include "Texture.h"

namespace Pathfinder
{

void Renderer2D::Init()
{
    s_RendererData2D = MakeUnique<RendererData2D>();
    memset(&s_Renderer2DStats, 0, sizeof(s_Renderer2DStats));

    ShaderLibrary::Load(std::vector<ShaderSpecification>{{"Quad2D"}});

    std::ranges::for_each(s_RendererData2D->SpriteSSBO,
                          [](auto& spriteSSBO)
                          {
                              spriteSSBO = Buffer::Create(
                                  {.ExtraFlags = EBufferFlag::BUFFER_FLAG_MAPPED | EBufferFlag::BUFFER_FLAG_DEVICE_LOCAL,
                                   .UsageFlags = EBufferUsage::BUFFER_USAGE_STORAGE | EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE});
                          });

    ShaderLibrary::WaitUntilShadersLoaded();

    const auto* gpo = std::get_if<GraphicsPipelineOptions>(
        &PipelineLibrary::Get(Renderer::GetRendererData()->ForwardPlusOpaquePipelineHash)->GetSpecification().PipelineOptions.value());
    PFR_ASSERT(gpo, "GPO is not valid!");

    const GraphicsPipelineOptions quadGPO = {.Formats           = gpo->Formats,
                                             .CullMode          = ECullMode::CULL_MODE_BACK,
                                             .FrontFace         = EFrontFace::FRONT_FACE_COUNTER_CLOCKWISE,
                                             .PrimitiveTopology = EPrimitiveTopology::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
                                             .bBlendEnable      = true,
                                             .BlendMode         = EBlendMode::BLEND_MODE_ALPHA,
                                             .PolygonMode       = EPolygonMode::POLYGON_MODE_FILL,
                                             .bDepthTest        = true,
                                             .bDepthWrite       = false,
                                             .DepthCompareOp    = ECompareOp::COMPARE_OP_GREATER_OR_EQUAL};

    PipelineSpecification quadPipelineSpec = {.DebugName       = "Quad2D",
                                              .PipelineOptions = MakeOptional<GraphicsPipelineOptions>(quadGPO),
                                              .Shader          = ShaderLibrary::Get("Quad2D"),
                                              .PipelineType    = EPipelineType::PIPELINE_TYPE_GRAPHICS};
    s_RendererData2D->QuadPipelineHash     = PipelineLibrary::Push(quadPipelineSpec);

    PipelineLibrary::Compile();
    LOG_TRACE("{}", __FUNCTION__);
}

void Renderer2D::Shutdown()
{
    s_RendererData2D.reset();
    LOG_TRACE("{}", __FUNCTION__);
}

void Renderer2D::Begin()
{
    s_Renderer2DStats = {};

    auto& rd                             = Renderer::GetRendererData();
    s_RendererData2D->FrameIndex         = rd->FrameIndex;
    s_RendererData2D->CurrentSpriteCount = 0;
}

void Renderer2D::Flush(Shared<CommandBuffer>& renderCommandBuffer)
{
    PFR_ASSERT(false, "NOT IMPLEMENTED!");

#if 0
    renderCommandBuffer->BeginDebugLabel("2D-QuadPass", glm::vec3(0.1f, 0.1f, 0.1f));
    renderCommandBuffer->BeginTimestampQuery();

    if (const uint32_t dataSize = s_RendererData2D->Sprites.size() * sizeof(s_RendererData2D->Sprites[0]); dataSize != 0)
    {
        auto& rd = Renderer::GetRendererData();
        s_RendererData2D->SpriteSSBO[rd->FrameIndex]->SetData(s_RendererData2D->Sprites.data(), dataSize);

        //  rd->GBuffer->BeginPass(renderCommandBuffer);

        PushConstantBlock pc = {};
        pc.CameraDataBuffer  = rd->CameraSSBO[rd->FrameIndex]->GetBDA();
        pc.addr0             = s_RendererData2D->SpriteSSBO[rd->FrameIndex]->GetBDA();

        Renderer::BindPipeline(renderCommandBuffer, PipelineLibrary::Get(s_RendererData2D->QuadPipelineHash));
        renderCommandBuffer->BindPushConstants(PipelineLibrary::Get(s_RendererData2D->QuadPipelineHash), 0, 0, sizeof(pc), &pc);
        renderCommandBuffer->Draw(6, s_Renderer2DStats.QuadCount);

        //   rd->GBuffer->EndPass(renderCommandBuffer);
    }

    renderCommandBuffer->EndTimestampQuery();
    renderCommandBuffer->EndDebugLabel();
#endif
}

void Renderer2D::DrawQuad(const glm::vec3& translation, const glm::vec3& scale, const glm::vec4& orientation, const glm::vec4& color,
                          const Shared<Texture>& texture, const uint32_t layer)
{
    if (s_Renderer2DStats.QuadCount >= s_RendererData2D->s_MAX_QUADS)
    {
        LOG_WARN("s_Renderer2DStats.QuadCount >= s_RendererData2D->s_MAX_QUADS");
        return;
    }

    const uint32_t defaultBindlessTextureIndex                      = TextureManager::GetWhiteTexture()->GetBindlessIndex();
    s_RendererData2D->Sprites[s_RendererData2D->CurrentSpriteCount] = Sprite(
        translation, scale, orientation, glm::packUnorm4x8(color), texture ? texture->GetBindlessIndex() : defaultBindlessTextureIndex);

    ++s_Renderer2DStats.QuadCount;
    s_Renderer2DStats.TriangleCount += 2;
    ++s_RendererData2D->CurrentSpriteCount;
}

}  // namespace Pathfinder