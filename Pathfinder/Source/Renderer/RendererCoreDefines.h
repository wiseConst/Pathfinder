#ifndef RENDERERCOREDEFINES_H
#define RENDERERCOREDEFINES_H

#include "Core/Math.h"
#include <array>

namespace Pathfinder
{

#define LOG_SHADER_INFO 0

static constexpr uint32_t s_FRAMES_IN_FLIGHT = 2;

class CommandBuffer;
using CommandBufferPerFrame = std::array<Shared<CommandBuffer>, s_FRAMES_IN_FLIGHT>;

class Framebuffer;
using FramebufferPerFrame = std::array<Shared<Framebuffer>, s_FRAMES_IN_FLIGHT>;

class Buffer;
using BufferPerFrame = std::array<Shared<Buffer>, s_FRAMES_IN_FLIGHT>;

class Image;
using ImagePerFrame = std::array<Shared<Image>, s_FRAMES_IN_FLIGHT>;

// Mostly stolen from vulkan_core.h
enum EPipelineStage : uint64_t
{
    PIPELINE_STAGE_TOP_OF_PIPE_BIT                      = BIT(0),
    PIPELINE_STAGE_DRAW_INDIRECT_BIT                    = BIT(1),
    PIPELINE_STAGE_VERTEX_INPUT_BIT                     = BIT(2),
    PIPELINE_STAGE_VERTEX_SHADER_BIT                    = BIT(3),
    PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT      = BIT(4),
    PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT   = BIT(5),
    PIPELINE_STAGE_GEOMETRY_SHADER_BIT                  = BIT(6),
    PIPELINE_STAGE_FRAGMENT_SHADER_BIT                  = BIT(7),
    PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT             = BIT(8),
    PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT              = BIT(9),
    PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT          = BIT(10),
    PIPELINE_STAGE_COMPUTE_SHADER_BIT                   = BIT(11),
    PIPELINE_STAGE_TRANSFER_BIT                         = BIT(12),
    PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT                   = BIT(13),
    PIPELINE_STAGE_HOST_BIT                             = BIT(14),
    PIPELINE_STAGE_ALL_GRAPHICS_BIT                     = BIT(15),
    PIPELINE_STAGE_ALL_COMMANDS_BIT                     = BIT(16),
    PIPELINE_STAGE_NONE                                 = 0,
    PIPELINE_STAGE_CONDITIONAL_RENDERING_BIT            = BIT(17),
    PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT     = BIT(18),
    PIPELINE_STAGE_RAY_TRACING_SHADER_BIT               = BIT(19),
    PIPELINE_STAGE_FRAGMENT_DENSITY_PROCESS_BIT         = BIT(20),
    PIPELINE_STAGE_FRAGMENT_SHADING_RATE_ATTACHMENT_BIT = BIT(21),
    PIPELINE_STAGE_TASK_SHADER_BIT                      = BIT(22),
    PIPELINE_STAGE_MESH_SHADER_BIT                      = BIT(23),
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
    glm::vec3 Position = glm::vec3(0.0f);
    glm::vec3 Normal   = glm::vec3(0.0f);
    glm::vec2 UV       = glm::vec2(0.0f);
    glm::vec4 Color    = glm::vec4(1.0f);
};

struct MeshPositionVertex
{
    glm::vec3 Position = glm::vec3(0.0f);
};

struct MeshAttributeVertex
{
    glm::vec4 Color   = glm::vec4(1.0f);
    glm::vec3 Normal  = glm::vec3(0.0f);
    glm::vec3 Tangent = glm::vec3(0.0f);
};

// TESTING, learning mesh shaders from zeux
struct Meshlet
{
    uint32_t vertices[64];
    uint8_t indices[126];  // up to 42 tris
    uint8_t indexCount;
    uint8_t vertexCount;
};

struct CameraData
{
    glm::mat4 Projection  = glm::mat4(1.0f);
    glm::mat4 InverseView = glm::mat4(1.0f);
    glm::vec3 Position    = glm::vec3(0.0f);
};

struct PCBlock
{
    glm::mat4 Transform              = glm::mat4(1.0);
    uint32_t StorageImageIndex       = 0;
    uint32_t AlbedoTextureIndex      = 0;
    uint32_t NormalTextureIndex      = 0;
    uint32_t VertexPosBufferIndex    = 0;
    uint32_t VertexAttribBufferIndex = 0;
    uint32_t MeshletBufferIndex      = 0;
};

struct PBRData
{
    glm::vec4 BaseColor = glm::vec4(1.0f);
    float Roughness     = 1.0f;
    float Metallic      = 1.0f;
};

}  // namespace Pathfinder

#endif  // RENDERERCOREDEFINES_H
