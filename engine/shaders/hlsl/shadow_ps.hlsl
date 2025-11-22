/**
 * @file shadow_ps.hlsl
 * @brief Shadow map generation pixel shader
 */

struct ShadowPixelInput
{
    float4 position : SV_POSITION;
    float depth : TEXCOORD0;
};

float4 main(ShadowPixelInput input) : SV_TARGET
{
    return float4(input.depth, 0.0f, 0.0f, 1.0f);
}
