#ifndef RENDERERCOREDEFINES_H
#define RENDERERCOREDEFINES_H

#include "Core/Math.h"
#include <array>

namespace Pathfinder
{

#define LOG_SHADER_INFO 1

static constexpr uint32_t s_FRAMES_IN_FLIGHT = 2;

class CommandBuffer;
using CommandBufferPerFrame = std::array<Shared<CommandBuffer>, s_FRAMES_IN_FLIGHT>;

class Framebuffer;
using FramebufferPerFrame = std::array<Shared<Framebuffer>, s_FRAMES_IN_FLIGHT>;


// Mostly stolen from vulkan_core.h
enum class EPipelineStage : uint32_t
{
    PIPELINE_STAGE_TOP_OF_PIPE_BIT                      = 0x00000001,
    PIPELINE_STAGE_DRAW_INDIRECT_BIT                    = 0x00000002,
    PIPELINE_STAGE_VERTEX_INPUT_BIT                     = 0x00000004,
    PIPELINE_STAGE_VERTEX_SHADER_BIT                    = 0x00000008,
    PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT      = 0x00000010,
    PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT   = 0x00000020,
    PIPELINE_STAGE_GEOMETRY_SHADER_BIT                  = 0x00000040,
    PIPELINE_STAGE_FRAGMENT_SHADER_BIT                  = 0x00000080,
    PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT             = 0x00000100,
    PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT              = 0x00000200,
    PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT          = 0x00000400,
    PIPELINE_STAGE_COMPUTE_SHADER_BIT                   = 0x00000800,
    PIPELINE_STAGE_TRANSFER_BIT                         = 0x00001000,
    PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT                   = 0x00002000,
    PIPELINE_STAGE_HOST_BIT                             = 0x00004000,
    PIPELINE_STAGE_ALL_GRAPHICS_BIT                     = 0x00008000,
    PIPELINE_STAGE_ALL_COMMANDS_BIT                     = 0x00010000,
    PIPELINE_STAGE_NONE                                 = 0,
    PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT            = 0x00040000,
    PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT     = 0x02000000,
    PIPELINE_STAGE_RAY_TRACING_SHADER_BIT               = 0x00200000,
    PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT         = 0x00800000,
    PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT = 0x00400000,
    PIPELINE_STAGE_TASK_SHADER_BIT                      = 0x00080000,
    PIPELINE_STAGE_MESH_SHADER_BIT                      = 0x00100000,
};

enum class ECompareOp : uint8_t
{
    COMPARE_OP_NEVER = 0,
    COMPARE_OP_LESS,
    COMPARE_OP_EQUAL,
    COMPARE_OP_LESS_OR_EQUAL,
    COMPARE_OP_GREATER,
    COMPARE_OP_NOT_EQUAL,
    COMPARE_OP_GREATER_OR_EQUAL,
    COMPARE_OP_ALWAYS,
};

struct QuadVertex
{
    glm::vec3 Position  = glm::vec3(0.0f);
    glm::vec4 Color     = glm::vec4(1.0f);
    glm::uvec2 TexCoord = glm::uvec2(0);
};

struct MeshVertexPosition
{
    glm::vec3 Position = glm::vec3(0.0f);
};

struct MeshVertex
{
    glm::vec3 Normal  = glm::vec3(0.0f);
    glm::vec3 Tangent = glm::vec3(0.0f);
};

/*
struct Meshlet
{

};
*/

}  // namespace Pathfinder

#endif  // RENDERERCOREDEFINES_H
