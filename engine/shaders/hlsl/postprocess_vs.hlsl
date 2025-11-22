/**
 * @file postprocess_vs.hlsl
 * @brief Post-processing fullscreen vertex shader
 */

struct PostProcessVertexOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

PostProcessVertexOutput main(uint vertexID : SV_VertexID)
{
    PostProcessVertexOutput output;

    // Fullscreen triangle
    float2 pos = float2((vertexID << 1) & 2, vertexID & 2);
    output.position = float4(pos * 2.0f - 1.0f, 0.0f, 1.0f);
    output.texCoord = pos;
    output.texCoord.y = 1.0f - output.texCoord.y;

    return output;
}
