/**
 * @file skybox_vs.hlsl
 * @brief Skybox vertex shader using fullscreen triangle
 */

#include "common.hlsli"

struct SkyboxVertexOutput
{
    float4 position : SV_POSITION;
    float3 texCoord : TEXCOORD0;
};

SkyboxVertexOutput main(uint vertexID : SV_VertexID)
{
    SkyboxVertexOutput output;

    // Fullscreen triangle
    float2 pos = float2((vertexID << 1) & 2, vertexID & 2);
    pos = pos * 2.0f - 1.0f;

    output.position = float4(pos, 0.9999f, 1.0f);

    // Calculate view direction
    float4 clipPos = float4(pos, 1.0f, 1.0f);
    float4 viewDir = mul(g_inverseProjectionMatrix, clipPos);
    viewDir = viewDir / viewDir.w;
    output.texCoord = mul((float3x3)g_inverseViewMatrix, viewDir.xyz);

    return output;
}
