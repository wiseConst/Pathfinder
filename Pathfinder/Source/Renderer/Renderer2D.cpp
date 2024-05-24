#include <PathfinderPCH.h>
#include "Renderer2D.h"

#include "Renderer.h"
#include "Pipeline.h"
#include "Shader.h"
#include "Buffer.h"
#include "CommandBuffer.h"
#include "Framebuffer.h"
#include "Texture2D.h"

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
                              BufferSpecification ssboSpec = {EBufferUsage::BUFFER_USAGE_STORAGE |
                                                              EBufferUsage::BUFFER_USAGE_TRANSFER_SOURCE |
                                                              EBufferUsage::BUFFER_USAGE_SHADER_DEVICE_ADDRESS};
                              ssboSpec.Capacity            = sizeof(Sprite);
                              ssboSpec.bMapPersistent      = true;

                              spriteSSBO = Buffer::Create(ssboSpec);
                          });

    ShaderLibrary::WaitUntilShadersLoaded();

    PipelineSpecification quadPipelineSpec = {"Quad2D"};
    quadPipelineSpec.PipelineType          = EPipelineType::PIPELINE_TYPE_GRAPHICS;
    quadPipelineSpec.Shader                = ShaderLibrary::Get("Quad2D");
    quadPipelineSpec.bBindlessCompatible   = true;

    GraphicsPipelineOptions quadGPO    = {};
    quadGPO.FrontFace                  = EFrontFace::FRONT_FACE_COUNTER_CLOCKWISE;
    quadGPO.TargetFramebuffer          = Renderer::GetRendererData()->GBuffer;
    quadGPO.BlendMode                  = EBlendMode::BLEND_MODE_ALPHA;
    quadGPO.bBlendEnable               = true;
    quadGPO.bDepthTest                 = true;
    quadGPO.bDepthWrite                = false;  // Do I rly need this? since im using depth pre pass of my static meshes
    quadGPO.DepthCompareOp             = ECompareOp::COMPARE_OP_GREATER_OR_EQUAL;
    quadPipelineSpec.PipelineOptions   = MakeOptional<GraphicsPipelineOptions>(quadGPO);
    s_RendererData2D->QuadPipelineHash = PipelineLibrary::Push(quadPipelineSpec);

    PipelineLibrary::Compile();
    LOG_TRACE("RENDERER_2D: Renderer2D created!");
}

void Renderer2D::Shutdown()
{
    s_RendererData2D.reset();
    LOG_TRACE("RENDERER_2D: Renderer2D destroyed!");
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
    renderCommandBuffer->BeginDebugLabel("2D-QuadPass", glm::vec3(0.1f, 0.1f, 0.1f));
    renderCommandBuffer->BeginTimestampQuery();

    if (const uint32_t dataSize = s_RendererData2D->Sprites.size() * sizeof(s_RendererData2D->Sprites[0]); dataSize != 0)
    {
        auto& rd = Renderer::GetRendererData();
        s_RendererData2D->SpriteSSBO[rd->FrameIndex]->SetData(s_RendererData2D->Sprites.data(), dataSize);

        rd->GBuffer->BeginPass(renderCommandBuffer);

        PushConstantBlock pc = {};
        pc.CameraDataBuffer  = rd->CameraSSBO[rd->FrameIndex]->GetBDA();
        pc.addr0             = s_RendererData2D->SpriteSSBO[rd->FrameIndex]->GetBDA();

        Renderer::BindPipeline(renderCommandBuffer, PipelineLibrary::Get(s_RendererData2D->QuadPipelineHash));
        renderCommandBuffer->BindPushConstants(PipelineLibrary::Get(s_RendererData2D->QuadPipelineHash), 0, 0, sizeof(pc), &pc);
        renderCommandBuffer->Draw(6, s_Renderer2DStats.QuadCount);

        rd->GBuffer->EndPass(renderCommandBuffer);
    }

    renderCommandBuffer->EndTimestampQuery();
    renderCommandBuffer->EndDebugLabel();
}

void Renderer2D::DrawQuad(const glm::vec3& translation, const glm::vec3& scale, const glm::vec4& orientation, const glm::vec4& color,
                          const Shared<Texture2D>& texture, const uint32_t layer)
{
    if (s_Renderer2DStats.QuadCount >= s_RendererData2D->s_MAX_QUADS)
    {
        LOG_WARN("s_Renderer2DStats.QuadCount >= s_RendererData2D->s_MAX_QUADS");
        return;
    }

    const uint32_t defaultBindlessTextureIndex                      = Renderer::GetRendererData()->WhiteTexture->GetBindlessIndex();
    s_RendererData2D->Sprites[s_RendererData2D->CurrentSpriteCount] = Sprite(
        translation, scale, orientation, PackVec4ToU8Vec4(color), texture ? texture->GetBindlessIndex() : defaultBindlessTextureIndex);

    ++s_Renderer2DStats.QuadCount;
    s_Renderer2DStats.TriangleCount += 2;
    ++s_RendererData2D->CurrentSpriteCount;
}

}  // namespace Pathfinder