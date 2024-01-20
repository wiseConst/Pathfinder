#version 460

layout(location = 0) out vec4 outFragColor;

layout(location = 0) in VertexInput
{
    vec4 Color;
    vec3 Normal;
    vec3 Tangent;
} i_VertexInput;

void main()
{
    outFragColor = i_VertexInput.Color;
}