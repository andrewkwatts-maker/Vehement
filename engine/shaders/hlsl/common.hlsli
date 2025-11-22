/**
 * @file common.hlsli
 * @brief Common HLSL definitions and structures
 *
 * Shared types and constants for DirectX shaders
 */

#ifndef COMMON_HLSLI
#define COMMON_HLSLI

// =============================================================================
// Constants
// =============================================================================

static const float PI = 3.14159265359f;
static const float INV_PI = 0.31830988618f;
static const int MAX_LIGHTS = 16;
static const float MAX_REFLECTION_LOD = 4.0f;

// =============================================================================
// Vertex Input Structures
// =============================================================================

struct VertexInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 texCoord : TEXCOORD0;
    float4 tangent  : TANGENT;
    float4 color    : COLOR;
};

struct VertexOutput
{
    float4 position    : SV_POSITION;
    float3 worldPos    : TEXCOORD0;
    float3 normal      : TEXCOORD1;
    float2 texCoord    : TEXCOORD2;
    float3 tangent     : TEXCOORD3;
    float3 bitangent   : TEXCOORD4;
    float4 color       : COLOR;
    float4 shadowCoord : TEXCOORD5;
};

// =============================================================================
// Constant Buffers
// =============================================================================

cbuffer FrameUniforms : register(b0)
{
    float4x4 g_viewMatrix;
    float4x4 g_projectionMatrix;
    float4x4 g_viewProjectionMatrix;
    float4x4 g_inverseViewMatrix;
    float4x4 g_inverseProjectionMatrix;
    float3 g_cameraPosition;
    float g_time;
    float2 g_screenSize;
    float g_nearPlane;
    float g_farPlane;
};

cbuffer ObjectUniforms : register(b1)
{
    float4x4 g_modelMatrix;
    float4x4 g_normalMatrix;
    float4x4 g_modelViewProjectionMatrix;
    float4x4 g_lightSpaceMatrix;
};

cbuffer MaterialUniforms : register(b2)
{
    float4 g_albedoColor;
    float g_metallic;
    float g_roughness;
    float g_ao;
    float g_emissiveStrength;
    float3 g_emissiveColor;
    float g_normalScale;
    int g_useAlbedoMap;
    int g_useNormalMap;
    int g_useMetallicRoughnessMap;
    int g_useEmissiveMap;
    int g_useAOMap;
    float3 _materialPadding;
};

struct Light
{
    float3 position;
    float range;
    float3 direction;
    float spotAngle;
    float3 color;
    float intensity;
    int type;  // 0=directional, 1=point, 2=spot
    float3 _padding;
};

cbuffer LightingUniforms : register(b3)
{
    Light g_lights[MAX_LIGHTS];
    int g_numLights;
    float3 g_ambientColor;
    float g_ambientIntensity;
    float3 _lightingPadding;
};

// =============================================================================
// Textures and Samplers
// =============================================================================

Texture2D g_albedoMap : register(t0);
Texture2D g_normalMap : register(t1);
Texture2D g_metallicRoughnessMap : register(t2);
Texture2D g_emissiveMap : register(t3);
Texture2D g_aoMap : register(t4);
Texture2D g_shadowMap : register(t5);
TextureCube g_irradianceMap : register(t6);
TextureCube g_prefilteredMap : register(t7);
Texture2D g_brdfLUT : register(t8);

SamplerState g_linearSampler : register(s0);
SamplerComparisonState g_shadowSampler : register(s1);
SamplerState g_clampSampler : register(s2);

// =============================================================================
// PBR Functions
// =============================================================================

// Normal Distribution Function (GGX/Trowbridge-Reitz)
float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;

    return a2 / denom;
}

// Geometry Function (Smith's Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f;
    return NdotV / (NdotV * (1.0f - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// Fresnel (Schlick)
float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
    float3 maxSpec = max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0);
    return F0 + (maxSpec - F0) * pow(saturate(1.0f - cosTheta), 5.0f);
}

// Calculate lighting for a single light
float3 CalculateLight(
    Light light,
    float3 N,
    float3 V,
    float3 worldPos,
    float3 albedo,
    float metallic,
    float roughness,
    float3 F0)
{
    float3 L;
    float attenuation = 1.0f;

    if (light.type == 0)
    {
        // Directional light
        L = normalize(-light.direction);
    }
    else
    {
        // Point/Spot light
        float3 lightDir = light.position - worldPos;
        float distance = length(lightDir);
        L = normalize(lightDir);

        // Distance attenuation
        attenuation = 1.0f / (distance * distance);
        attenuation *= saturate(1.0f - pow(distance / light.range, 4.0f));

        // Spot light cone
        if (light.type == 2)
        {
            float theta = dot(L, normalize(-light.direction));
            float epsilon = light.spotAngle - light.spotAngle * 0.9f;
            attenuation *= saturate((theta - light.spotAngle * 0.9f) / epsilon);
        }
    }

    float3 H = normalize(V + L);

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);

    float3 numerator = NDF * G * F;
    float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f;
    float3 specular = numerator / denominator;

    float3 kS = F;
    float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
    kD *= 1.0f - metallic;

    float NdotL = max(dot(N, L), 0.0f);

    return (kD * albedo * INV_PI + specular) * light.color * light.intensity * NdotL * attenuation;
}

// =============================================================================
// Utility Functions
// =============================================================================

// Linear to sRGB conversion
float3 LinearToSRGB(float3 color)
{
    return pow(color, float3(1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f));
}

// sRGB to linear conversion
float3 SRGBToLinear(float3 color)
{
    return pow(color, float3(2.2f, 2.2f, 2.2f));
}

// Reinhard tonemapping
float3 TonemapReinhard(float3 color)
{
    return color / (color + float3(1.0f, 1.0f, 1.0f));
}

// ACES tonemapping
float3 TonemapACES(float3 color)
{
    const float a = 2.51f;
    const float b = 0.03f;
    const float c = 2.43f;
    const float d = 0.59f;
    const float e = 0.14f;
    return saturate((color * (a * color + b)) / (color * (c * color + d) + e));
}

// Calculate shadow factor
float CalculateShadow(float4 shadowCoord)
{
    // Perspective divide
    float3 projCoords = shadowCoord.xyz / shadowCoord.w;

    // Transform to [0,1] range
    projCoords.xy = projCoords.xy * 0.5f + 0.5f;
    projCoords.y = 1.0f - projCoords.y;  // Flip Y for D3D

    // Check if outside shadow map
    if (projCoords.x < 0.0f || projCoords.x > 1.0f ||
        projCoords.y < 0.0f || projCoords.y > 1.0f ||
        projCoords.z < 0.0f || projCoords.z > 1.0f)
    {
        return 1.0f;
    }

    // PCF filtering
    float shadow = 0.0f;
    float2 texelSize = float2(1.0f / 1024.0f, 1.0f / 1024.0f);  // Assume 1024x1024 shadow map

    [unroll]
    for (int x = -1; x <= 1; ++x)
    {
        [unroll]
        for (int y = -1; y <= 1; ++y)
        {
            float2 offset = float2(x, y) * texelSize;
            shadow += g_shadowMap.SampleCmpLevelZero(g_shadowSampler, projCoords.xy + offset, projCoords.z - 0.001f);
        }
    }

    return shadow / 9.0f;
}

#endif // COMMON_HLSLI
