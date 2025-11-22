/**
 * @file standard_vs.hlsl
 * @brief Standard PBR vertex shader
 */

#include "common.hlsli"

VertexOutput main(VertexInput input)
{
    VertexOutput output;

    // Transform to world space
    float4 worldPos = mul(g_modelMatrix, float4(input.position, 1.0f));
    output.worldPos = worldPos.xyz;
    output.position = mul(g_viewProjectionMatrix, worldPos);

    // Transform normal to world space
    output.normal = normalize(mul((float3x3)g_normalMatrix, input.normal));

    // Transform tangent to world space
    output.tangent = normalize(mul((float3x3)g_normalMatrix, input.tangent.xyz));
    output.bitangent = cross(output.normal, output.tangent) * input.tangent.w;

    output.texCoord = input.texCoord;
    output.color = input.color;

    // Shadow coordinates
    output.shadowCoord = mul(g_lightSpaceMatrix, worldPos);

    return output;
}
