/**
 * @file shadow_vs.hlsl
 * @brief Shadow map generation vertex shader
 */

#include "common.hlsli"

struct ShadowVertexOutput
{
    float4 position : SV_POSITION;
    float depth : TEXCOORD0;
};

ShadowVertexOutput main(VertexInput input)
{
    ShadowVertexOutput output;

    float4 worldPos = mul(g_modelMatrix, float4(input.position, 1.0f));
    output.position = mul(g_lightSpaceMatrix, worldPos);
    output.depth = output.position.z / output.position.w;

    return output;
}
