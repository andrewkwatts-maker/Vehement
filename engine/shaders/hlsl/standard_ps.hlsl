/**
 * @file standard_ps.hlsl
 * @brief Standard PBR pixel shader
 *
 * Physically-based rendering with:
 * - Metallic-roughness workflow
 * - IBL (Image-Based Lighting)
 * - Normal mapping
 * - Shadow mapping
 */

#include "common.hlsli"

float4 main(VertexOutput input) : SV_TARGET
{
    // Sample textures
    float4 albedo = g_albedoColor;
    if (g_useAlbedoMap)
    {
        albedo *= g_albedoMap.Sample(g_linearSampler, input.texCoord);
    }
    albedo *= input.color;

    float metallic = g_metallic;
    float roughness = g_roughness;
    if (g_useMetallicRoughnessMap)
    {
        float4 mr = g_metallicRoughnessMap.Sample(g_linearSampler, input.texCoord);
        metallic *= mr.b;
        roughness *= mr.g;
    }

    float ao = g_ao;
    if (g_useAOMap)
    {
        ao *= g_aoMap.Sample(g_linearSampler, input.texCoord).r;
    }

    // Normal mapping
    float3 N = normalize(input.normal);
    if (g_useNormalMap)
    {
        float3 tangentNormal = g_normalMap.Sample(g_linearSampler, input.texCoord).xyz * 2.0f - 1.0f;
        tangentNormal.xy *= g_normalScale;
        float3x3 TBN = float3x3(
            normalize(input.tangent),
            normalize(input.bitangent),
            N
        );
        N = normalize(mul(tangentNormal, TBN));
    }

    float3 V = normalize(g_cameraPosition - input.worldPos);

    // Base reflectivity
    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, albedo.rgb, metallic);

    // Direct lighting
    float3 Lo = float3(0.0f, 0.0f, 0.0f);
    for (int i = 0; i < g_numLights && i < MAX_LIGHTS; ++i)
    {
        Lo += CalculateLight(g_lights[i], N, V, input.worldPos, albedo.rgb, metallic, roughness, F0);
    }

    // Shadow
    float shadow = CalculateShadow(input.shadowCoord);
    Lo *= shadow;

    // Ambient lighting (IBL)
    float3 F = FresnelSchlickRoughness(max(dot(N, V), 0.0f), F0, roughness);
    float3 kS = F;
    float3 kD = 1.0f - kS;
    kD *= 1.0f - metallic;

    float3 irradiance = g_irradianceMap.Sample(g_linearSampler, N).rgb;
    float3 diffuse = irradiance * albedo.rgb;

    float3 R = reflect(-V, N);
    float3 prefilteredColor = g_prefilteredMap.SampleLevel(g_linearSampler, R, roughness * MAX_REFLECTION_LOD).rgb;
    float2 brdf = g_brdfLUT.Sample(g_clampSampler, float2(max(dot(N, V), 0.0f), roughness)).rg;
    float3 specular = prefilteredColor * (F * brdf.x + brdf.y);

    float3 ambient = (kD * diffuse + specular) * ao * g_ambientColor * g_ambientIntensity;

    float3 color = ambient + Lo;

    // Emissive
    if (g_useEmissiveMap)
    {
        color += g_emissiveMap.Sample(g_linearSampler, input.texCoord).rgb * g_emissiveColor * g_emissiveStrength;
    }
    else
    {
        color += g_emissiveColor * g_emissiveStrength;
    }

    // HDR tonemapping (Reinhard)
    color = TonemapReinhard(color);

    // Gamma correction
    color = LinearToSRGB(color);

    return float4(color, albedo.a);
}
