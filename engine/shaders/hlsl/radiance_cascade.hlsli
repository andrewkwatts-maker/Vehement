/**
 * @file radiance_cascade.hlsli
 * @brief Radiance Cascade Global Illumination Functions
 *
 * Functions for sampling and using radiance cascades in shaders
 */

#ifndef RADIANCE_CASCADE_HLSLI
#define RADIANCE_CASCADE_HLSLI

// Radiance cascade configuration
static const int MAX_CASCADES = 4;

cbuffer RadianceCascadeUniforms : register(b4)
{
    float3 g_cascadeOrigin;
    float g_cascadeBaseSpacing;
    int g_numCascades;
    float g_cascadeScale;
    float2 _rcPadding;
};

// Radiance cascade 3D textures
Texture3D g_radianceCascade0 : register(t9);
Texture3D g_radianceCascade1 : register(t10);
Texture3D g_radianceCascade2 : register(t11);
Texture3D g_radianceCascade3 : register(t12);

SamplerState g_radianceSampler : register(s3);

/**
 * @brief Convert world position to cascade texture coordinates
 */
float3 WorldToCascadeUV(float3 worldPos, int cascadeLevel)
{
    float spacing = g_cascadeBaseSpacing * pow(g_cascadeScale, (float)cascadeLevel);
    float cascadeSize = spacing * 32.0f; // Assuming 32x32x32 resolution

    float3 localPos = (worldPos - g_cascadeOrigin) / cascadeSize;
    return localPos + 0.5f; // Center in texture space [0, 1]
}

/**
 * @brief Sample radiance from a specific cascade level
 */
float3 SampleCascadeLevel(float3 worldPos, float3 normal, int cascadeLevel)
{
    float3 uv = WorldToCascadeUV(worldPos, cascadeLevel);

    // Check if UV is within valid range
    if (any(uv < 0.0f) || any(uv > 1.0f))
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    // Sample appropriate cascade texture
    float4 radiance;
    if (cascadeLevel == 0)
    {
        radiance = g_radianceCascade0.Sample(g_radianceSampler, uv);
    }
    else if (cascadeLevel == 1)
    {
        radiance = g_radianceCascade1.Sample(g_radianceSampler, uv);
    }
    else if (cascadeLevel == 2)
    {
        radiance = g_radianceCascade2.Sample(g_radianceSampler, uv);
    }
    else if (cascadeLevel == 3)
    {
        radiance = g_radianceCascade3.Sample(g_radianceSampler, uv);
    }
    else
    {
        radiance = float4(0.0f, 0.0f, 0.0f, 0.0f);
    }

    // Alpha channel contains validity (0=invalid, 1=valid)
    return radiance.rgb * radiance.a;
}

/**
 * @brief Sample radiance cascade with automatic level selection
 */
float3 SampleRadianceCascade(float3 worldPos, float3 normal)
{
    // Try each cascade level from finest to coarsest
    for (int i = 0; i < g_numCascades; ++i)
    {
        float3 uv = WorldToCascadeUV(worldPos, i);

        // Use this cascade if position is within bounds
        if (all(uv >= 0.0f) && all(uv <= 1.0f))
        {
            float3 radiance = SampleCascadeLevel(worldPos, normal, i);

            // If we got valid radiance, return it
            if (any(radiance > 0.0f))
            {
                return radiance;
            }
        }
    }

    return float3(0.0f, 0.0f, 0.0f);
}

/**
 * @brief Sample indirect diffuse lighting using radiance cascade
 */
float3 GetIndirectDiffuse(float3 worldPos, float3 normal, float3 albedo)
{
    // Sample radiance cascade
    float3 irradiance = SampleRadianceCascade(worldPos + normal * 0.1f, normal);

    // Apply cosine-weighted diffuse
    return irradiance * albedo;
}

/**
 * @brief Sample indirect specular lighting using radiance cascade
 */
float3 GetIndirectSpecular(float3 worldPos, float3 normal, float3 viewDir, float roughness, float3 F0)
{
    // Calculate reflection direction
    float3 R = reflect(-viewDir, normal);

    // Sample along reflection ray with offset based on roughness
    float offset = 0.5f + roughness * 2.0f;
    float3 samplePos = worldPos + R * offset;

    float3 radiance = SampleRadianceCascade(samplePos, R);

    // Simple Fresnel approximation
    float NdotV = max(dot(normal, viewDir), 0.0f);
    float3 F = F0 + (1.0f - F0) * pow(1.0f - NdotV, 5.0f);

    return radiance * F * (1.0f - roughness * 0.5f);
}

/**
 * @brief Complete indirect lighting (diffuse + specular)
 */
float3 GetIndirectLighting(
    float3 worldPos,
    float3 normal,
    float3 viewDir,
    float3 albedo,
    float metallic,
    float roughness,
    float3 F0)
{
    // Diffuse indirect
    float3 kD = 1.0f - metallic;
    float3 indirectDiffuse = GetIndirectDiffuse(worldPos, normal, albedo) * kD;

    // Specular indirect
    float3 indirectSpecular = GetIndirectSpecular(worldPos, normal, viewDir, roughness, F0);

    return indirectDiffuse + indirectSpecular;
}

#endif // RADIANCE_CASCADE_HLSLI
