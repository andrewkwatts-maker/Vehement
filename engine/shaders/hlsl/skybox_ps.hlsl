/**
 * @file skybox_ps.hlsl
 * @brief Skybox pixel shader
 */

#include "common.hlsli"

TextureCube g_skyboxMap : register(t0);

struct SkyboxPixelInput
{
    float4 position : SV_POSITION;
    float3 texCoord : TEXCOORD0;
};

float4 main(SkyboxPixelInput input) : SV_TARGET
{
    float3 color = g_skyboxMap.Sample(g_linearSampler, input.texCoord).rgb;

    // Tonemapping
    color = TonemapReinhard(color);
    color = LinearToSRGB(color);

    return float4(color, 1.0f);
}
