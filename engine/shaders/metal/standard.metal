/**
 * @file standard.metal
 * @brief Standard PBR shaders for Metal
 *
 * Physically-based rendering with:
 * - Metallic-roughness workflow
 * - IBL (Image-Based Lighting)
 * - Normal mapping
 * - Shadow mapping
 */

#include <metal_stdlib>
#include <simd/simd.h>

using namespace metal;

// =============================================================================
// Shared Types
// =============================================================================

struct VertexInput {
    float3 position [[attribute(0)]];
    float3 normal   [[attribute(1)]];
    float2 texCoord [[attribute(2)]];
    float4 tangent  [[attribute(3)]];
    float4 color    [[attribute(4)]];
};

struct VertexOutput {
    float4 position [[position]];
    float3 worldPos;
    float3 normal;
    float2 texCoord;
    float3 tangent;
    float3 bitangent;
    float4 color;
    float4 shadowCoord;
};

struct FrameUniforms {
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4x4 viewProjectionMatrix;
    float4x4 inverseViewMatrix;
    float4x4 inverseProjectionMatrix;
    float3 cameraPosition;
    float time;
    float2 screenSize;
    float nearPlane;
    float farPlane;
};

struct ObjectUniforms {
    float4x4 modelMatrix;
    float4x4 normalMatrix;
    float4x4 modelViewProjectionMatrix;
    float4x4 lightSpaceMatrix;
};

struct MaterialUniforms {
    float4 albedoColor;
    float metallic;
    float roughness;
    float ao;
    float emissiveStrength;
    float3 emissiveColor;
    float normalScale;
    int useAlbedoMap;
    int useNormalMap;
    int useMetallicRoughnessMap;
    int useEmissiveMap;
    int useAOMap;
};

struct Light {
    float3 position;
    float range;
    float3 direction;
    float spotAngle;
    float3 color;
    float intensity;
    int type;  // 0=directional, 1=point, 2=spot
    float3 _padding;
};

constant int MAX_LIGHTS = 16;

struct LightingUniforms {
    Light lights[MAX_LIGHTS];
    int numLights;
    float3 ambientColor;
    float ambientIntensity;
};

// =============================================================================
// Vertex Shader
// =============================================================================

vertex VertexOutput vertex_main(
    VertexInput in [[stage_in]],
    constant FrameUniforms& frame [[buffer(0)]],
    constant ObjectUniforms& object [[buffer(1)]]
) {
    VertexOutput out;

    float4 worldPos = object.modelMatrix * float4(in.position, 1.0);
    out.worldPos = worldPos.xyz;
    out.position = frame.viewProjectionMatrix * worldPos;

    // Transform normal to world space
    out.normal = normalize((object.normalMatrix * float4(in.normal, 0.0)).xyz);

    // Transform tangent to world space
    out.tangent = normalize((object.normalMatrix * float4(in.tangent.xyz, 0.0)).xyz);
    out.bitangent = cross(out.normal, out.tangent) * in.tangent.w;

    out.texCoord = in.texCoord;
    out.color = in.color;

    // Shadow coordinates
    out.shadowCoord = object.lightSpaceMatrix * worldPos;

    return out;
}

// =============================================================================
// Fragment Shader - PBR
// =============================================================================

constant float PI = 3.14159265359;

// Normal Distribution Function (GGX/Trowbridge-Reitz)
float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return a2 / denom;
}

// Geometry Function (Smith's Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
    return ggx1 * ggx2;
}

// Fresnel (Schlick)
float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

float3 FresnelSchlickRoughness(float cosTheta, float3 F0, float roughness) {
    return F0 + (max(float3(1.0 - roughness), F0) - F0) * pow(saturate(1.0 - cosTheta), 5.0);
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
    float3 F0
) {
    float3 L;
    float attenuation = 1.0;

    if (light.type == 0) {
        // Directional light
        L = normalize(-light.direction);
    } else {
        // Point/Spot light
        float3 lightDir = light.position - worldPos;
        float distance = length(lightDir);
        L = normalize(lightDir);

        // Distance attenuation
        attenuation = 1.0 / (distance * distance);
        attenuation *= saturate(1.0 - pow(distance / light.range, 4.0));

        // Spot light cone
        if (light.type == 2) {
            float theta = dot(L, normalize(-light.direction));
            float epsilon = light.spotAngle - light.spotAngle * 0.9;
            attenuation *= saturate((theta - light.spotAngle * 0.9) / epsilon);
        }
    }

    float3 H = normalize(V + L);

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    float3 specular = numerator / denominator;

    float3 kS = F;
    float3 kD = float3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);

    return (kD * albedo / PI + specular) * light.color * light.intensity * NdotL * attenuation;
}

fragment float4 fragment_main(
    VertexOutput in [[stage_in]],
    constant FrameUniforms& frame [[buffer(0)]],
    constant MaterialUniforms& material [[buffer(1)]],
    constant LightingUniforms& lighting [[buffer(2)]],
    texture2d<float> albedoMap [[texture(0)]],
    texture2d<float> normalMap [[texture(1)]],
    texture2d<float> metallicRoughnessMap [[texture(2)]],
    texture2d<float> emissiveMap [[texture(3)]],
    texture2d<float> aoMap [[texture(4)]],
    texture2d<float> shadowMap [[texture(5)]],
    texturecube<float> irradianceMap [[texture(6)]],
    texturecube<float> prefilteredMap [[texture(7)]],
    texture2d<float> brdfLUT [[texture(8)]],
    sampler textureSampler [[sampler(0)]],
    sampler shadowSampler [[sampler(1)]]
) {
    // Sample textures
    float4 albedo = material.albedoColor;
    if (material.useAlbedoMap) {
        albedo *= albedoMap.sample(textureSampler, in.texCoord);
    }
    albedo *= in.color;

    float metallic = material.metallic;
    float roughness = material.roughness;
    if (material.useMetallicRoughnessMap) {
        float4 mr = metallicRoughnessMap.sample(textureSampler, in.texCoord);
        metallic *= mr.b;
        roughness *= mr.g;
    }

    float ao = material.ao;
    if (material.useAOMap) {
        ao *= aoMap.sample(textureSampler, in.texCoord).r;
    }

    // Normal mapping
    float3 N = normalize(in.normal);
    if (material.useNormalMap) {
        float3 tangentNormal = normalMap.sample(textureSampler, in.texCoord).xyz * 2.0 - 1.0;
        tangentNormal.xy *= material.normalScale;
        float3x3 TBN = float3x3(normalize(in.tangent), normalize(in.bitangent), N);
        N = normalize(TBN * tangentNormal);
    }

    float3 V = normalize(frame.cameraPosition - in.worldPos);

    // Base reflectivity
    float3 F0 = float3(0.04);
    F0 = mix(F0, albedo.rgb, metallic);

    // Direct lighting
    float3 Lo = float3(0.0);
    for (int i = 0; i < lighting.numLights && i < MAX_LIGHTS; ++i) {
        Lo += CalculateLight(lighting.lights[i], N, V, in.worldPos, albedo.rgb, metallic, roughness, F0);
    }

    // Ambient lighting (IBL)
    float3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0), F0, roughness);
    float3 kS = F;
    float3 kD = 1.0 - kS;
    kD *= 1.0 - metallic;

    float3 irradiance = irradianceMap.sample(textureSampler, N).rgb;
    float3 diffuse = irradiance * albedo.rgb;

    float3 R = reflect(-V, N);
    const float MAX_REFLECTION_LOD = 4.0;
    float3 prefilteredColor = prefilteredMap.sample(textureSampler, R, level(roughness * MAX_REFLECTION_LOD)).rgb;
    float2 brdf = brdfLUT.sample(textureSampler, float2(max(dot(N, V), 0.0), roughness)).rg;
    float3 specular = prefilteredColor * (F * brdf.x + brdf.y);

    float3 ambient = (kD * diffuse + specular) * ao * lighting.ambientColor * lighting.ambientIntensity;

    float3 color = ambient + Lo;

    // Emissive
    if (material.useEmissiveMap) {
        color += emissiveMap.sample(textureSampler, in.texCoord).rgb * material.emissiveColor * material.emissiveStrength;
    } else {
        color += material.emissiveColor * material.emissiveStrength;
    }

    // HDR tonemapping (Reinhard)
    color = color / (color + float3(1.0));

    // Gamma correction
    color = pow(color, float3(1.0/2.2));

    return float4(color, albedo.a);
}

// =============================================================================
// Shadow Pass
// =============================================================================

struct ShadowVertexOutput {
    float4 position [[position]];
};

vertex ShadowVertexOutput shadow_vertex(
    VertexInput in [[stage_in]],
    constant ObjectUniforms& object [[buffer(1)]]
) {
    ShadowVertexOutput out;
    out.position = object.lightSpaceMatrix * object.modelMatrix * float4(in.position, 1.0);
    return out;
}

fragment float4 shadow_fragment(ShadowVertexOutput in [[stage_in]]) {
    return float4(in.position.z, 0.0, 0.0, 1.0);
}

// =============================================================================
// Skybox
// =============================================================================

struct SkyboxVertexOutput {
    float4 position [[position]];
    float3 texCoord;
};

vertex SkyboxVertexOutput skybox_vertex(
    uint vertexID [[vertex_id]],
    constant FrameUniforms& frame [[buffer(0)]]
) {
    // Fullscreen triangle
    float2 pos = float2((vertexID << 1) & 2, vertexID & 2);
    pos = pos * 2.0 - 1.0;

    SkyboxVertexOutput out;
    out.position = float4(pos, 0.9999, 1.0);

    // Calculate view direction
    float4 clipPos = float4(pos, 1.0, 1.0);
    float4 viewDir = frame.inverseProjectionMatrix * clipPos;
    viewDir = viewDir / viewDir.w;
    out.texCoord = (frame.inverseViewMatrix * float4(viewDir.xyz, 0.0)).xyz;

    return out;
}

fragment float4 skybox_fragment(
    SkyboxVertexOutput in [[stage_in]],
    texturecube<float> skybox [[texture(0)]],
    sampler textureSampler [[sampler(0)]]
) {
    float3 color = skybox.sample(textureSampler, in.texCoord).rgb;

    // Tonemapping
    color = color / (color + float3(1.0));
    color = pow(color, float3(1.0/2.2));

    return float4(color, 1.0);
}

// =============================================================================
// Post-Processing
// =============================================================================

struct PostProcessVertexOutput {
    float4 position [[position]];
    float2 texCoord;
};

vertex PostProcessVertexOutput postprocess_vertex(uint vertexID [[vertex_id]]) {
    PostProcessVertexOutput out;

    float2 pos = float2((vertexID << 1) & 2, vertexID & 2);
    out.position = float4(pos * 2.0 - 1.0, 0.0, 1.0);
    out.texCoord = pos;
    out.texCoord.y = 1.0 - out.texCoord.y;

    return out;
}

fragment float4 postprocess_fragment(
    PostProcessVertexOutput in [[stage_in]],
    texture2d<float> colorTexture [[texture(0)]],
    sampler textureSampler [[sampler(0)]]
) {
    float4 color = colorTexture.sample(textureSampler, in.texCoord);

    // FXAA could be added here
    // Vignette
    float2 uv = in.texCoord * (1.0 - in.texCoord);
    float vignette = uv.x * uv.y * 15.0;
    vignette = pow(vignette, 0.25);
    color.rgb *= vignette;

    return color;
}
