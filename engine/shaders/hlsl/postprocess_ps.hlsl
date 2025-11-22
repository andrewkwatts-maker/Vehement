/**
 * @file postprocess_ps.hlsl
 * @brief Post-processing pixel shader
 *
 * Effects included:
 * - Vignette
 * - FXAA placeholder
 * - Color grading hooks
 */

#include "common.hlsli"

Texture2D g_colorTexture : register(t0);

cbuffer PostProcessUniforms : register(b0)
{
    float g_vignetteStrength;
    float g_vignetteSoftness;
    float g_exposure;
    float g_gamma;
    float g_contrast;
    float g_saturation;
    float2 _postProcessPadding;
};

struct PostProcessPixelInput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
};

// Simple vignette effect
float3 ApplyVignette(float3 color, float2 uv, float strength, float softness)
{
    float2 centered = uv - 0.5f;
    float dist = length(centered);
    float vignette = smoothstep(0.5f, 0.5f - softness, dist * strength);
    return color * vignette;
}

// FXAA (simplified)
float3 ApplyFXAA(float2 uv)
{
    float2 texelSize = 1.0f / g_screenSize;

    // Sample center and neighbors
    float3 center = g_colorTexture.Sample(g_linearSampler, uv).rgb;
    float3 top = g_colorTexture.Sample(g_linearSampler, uv + float2(0.0f, -texelSize.y)).rgb;
    float3 bottom = g_colorTexture.Sample(g_linearSampler, uv + float2(0.0f, texelSize.y)).rgb;
    float3 left = g_colorTexture.Sample(g_linearSampler, uv + float2(-texelSize.x, 0.0f)).rgb;
    float3 right = g_colorTexture.Sample(g_linearSampler, uv + float2(texelSize.x, 0.0f)).rgb;

    // Luminance
    float lumCenter = dot(center, float3(0.299f, 0.587f, 0.114f));
    float lumTop = dot(top, float3(0.299f, 0.587f, 0.114f));
    float lumBottom = dot(bottom, float3(0.299f, 0.587f, 0.114f));
    float lumLeft = dot(left, float3(0.299f, 0.587f, 0.114f));
    float lumRight = dot(right, float3(0.299f, 0.587f, 0.114f));

    float lumMin = min(lumCenter, min(min(lumTop, lumBottom), min(lumLeft, lumRight)));
    float lumMax = max(lumCenter, max(max(lumTop, lumBottom), max(lumLeft, lumRight)));
    float lumRange = lumMax - lumMin;

    // Skip anti-aliasing if contrast is low
    if (lumRange < max(0.0833f, lumMax * 0.125f))
    {
        return center;
    }

    // Edge direction
    float edgeHorizontal = abs(lumTop + lumBottom - 2.0f * lumCenter);
    float edgeVertical = abs(lumLeft + lumRight - 2.0f * lumCenter);

    // Blend in edge direction
    if (edgeHorizontal >= edgeVertical)
    {
        return (top + bottom + center * 2.0f) * 0.25f;
    }
    else
    {
        return (left + right + center * 2.0f) * 0.25f;
    }
}

// Color grading
float3 ApplyColorGrading(float3 color, float exposure, float contrast, float saturation)
{
    // Exposure
    color *= exposure;

    // Contrast
    color = (color - 0.5f) * contrast + 0.5f;

    // Saturation
    float luma = dot(color, float3(0.299f, 0.587f, 0.114f));
    color = lerp(float3(luma, luma, luma), color, saturation);

    return saturate(color);
}

float4 main(PostProcessPixelInput input) : SV_TARGET
{
    // Apply FXAA
    float3 color = ApplyFXAA(input.texCoord);

    // Apply color grading
    color = ApplyColorGrading(color, g_exposure, g_contrast, g_saturation);

    // Apply vignette
    color = ApplyVignette(color, input.texCoord, g_vignetteStrength, g_vignetteSoftness);

    // Gamma correction (if not already applied)
    color = pow(color, float3(1.0f / g_gamma, 1.0f / g_gamma, 1.0f / g_gamma));

    return float4(color, 1.0f);
}
