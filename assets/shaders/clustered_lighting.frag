#version 460 core

out vec4 FragColor;

in vec3 v_worldPos;
in vec3 v_normal;
in vec2 v_texCoord;

// Light structure (must match GPULight)
struct Light {
    vec4 position;        // xyz = position, w = range
    vec4 direction;       // xyz = direction, w = inner cone angle (cos)
    vec4 color;           // rgb = color, a = intensity
    vec4 params;          // x = outer cone angle (cos), y = type, z = enabled, w = padding
};

// Cluster and light data
layout(std430, binding = 1) readonly buffer ClusterBuffer {
    uvec2 clusters[];  // x = count, y = offset
};

layout(std430, binding = 2) readonly buffer LightBuffer {
    Light lights[];
};

layout(std430, binding = 3) readonly buffer LightIndexBuffer {
    uint lightIndices[];
};

// Uniforms
uniform vec3 u_cameraPos;
uniform ivec3 u_gridDim;
uniform vec2 u_screenDim;
uniform float u_nearPlane;
uniform float u_farPlane;
uniform int u_numLights;

// Material properties
uniform vec3 u_albedo;
uniform float u_metallic;
uniform float u_roughness;
uniform float u_ao;

// Textures
uniform sampler2D u_albedoMap;
uniform sampler2D u_normalMap;
uniform sampler2D u_metallicRoughnessMap;
uniform bool u_useTextures;

// Light types
const int LIGHT_TYPE_POINT = 0;
const int LIGHT_TYPE_SPOT = 1;
const int LIGHT_TYPE_DIRECTIONAL = 2;

const float PI = 3.14159265359;

// Get cluster index for current fragment
uint getClusterIndex(vec3 fragPos) {
    // Calculate depth in view space
    vec4 viewPos = vec4(fragPos - u_cameraPos, 1.0); // Simplified, should use view matrix
    float depth = length(viewPos.xyz);

    // Screen space coordinates
    vec2 screenPos = gl_FragCoord.xy / u_screenDim;

    // Calculate cluster indices
    ivec3 clusterID;
    clusterID.x = int(screenPos.x * float(u_gridDim.x));
    clusterID.y = int(screenPos.y * float(u_gridDim.y));

    // Calculate Z slice using exponential distribution
    float depthRatio = log2(depth / u_nearPlane) / log2(u_farPlane / u_nearPlane);
    clusterID.z = int(depthRatio * float(u_gridDim.z));

    // Clamp to grid bounds
    clusterID = clamp(clusterID, ivec3(0), u_gridDim - ivec3(1));

    // Calculate linear index
    uint clusterIndex = uint(clusterID.z * u_gridDim.x * u_gridDim.y +
                            clusterID.y * u_gridDim.x +
                            clusterID.x);

    return clusterIndex;
}

// PBR Functions
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / max(denom, 0.001);
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / max(denom, 0.001);
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Calculate lighting contribution from a single light
vec3 calculateLightContribution(Light light, vec3 fragPos, vec3 N, vec3 V,
                                vec3 F0, vec3 albedo, float roughness, float metallic) {
    int lightType = int(light.params.y);
    vec3 L;
    float attenuation = 1.0;
    vec3 radiance;

    if (lightType == LIGHT_TYPE_DIRECTIONAL) {
        L = normalize(-light.direction.xyz);
        radiance = light.color.rgb * light.color.a;
    }
    else if (lightType == LIGHT_TYPE_POINT) {
        vec3 lightPos = light.position.xyz;
        L = normalize(lightPos - fragPos);
        float distance = length(lightPos - fragPos);
        float range = light.position.w;

        // Inverse square falloff with range cutoff
        attenuation = 1.0 / (distance * distance);
        attenuation *= max(0.0, 1.0 - pow(distance / range, 4.0));

        radiance = light.color.rgb * light.color.a * attenuation;
    }
    else if (lightType == LIGHT_TYPE_SPOT) {
        vec3 lightPos = light.position.xyz;
        L = normalize(lightPos - fragPos);
        float distance = length(lightPos - fragPos);
        float range = light.position.w;

        // Distance attenuation
        attenuation = 1.0 / (distance * distance);
        attenuation *= max(0.0, 1.0 - pow(distance / range, 4.0));

        // Cone attenuation
        vec3 spotDir = normalize(light.direction.xyz);
        float theta = dot(L, -spotDir);
        float innerCutoff = light.direction.w;
        float outerCutoff = light.params.x;
        float epsilon = innerCutoff - outerCutoff;
        float intensity = clamp((theta - outerCutoff) / epsilon, 0.0, 1.0);

        radiance = light.color.rgb * light.color.a * attenuation * intensity;
    }
    else {
        return vec3(0.0);
    }

    // Cook-Torrance BRDF
    vec3 H = normalize(V + L);

    float NDF = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    vec3 specular = numerator / max(denominator, 0.001);

    float NdotL = max(dot(N, L), 0.0);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

void main() {
    // Sample material properties
    vec3 albedo = u_useTextures ? texture(u_albedoMap, v_texCoord).rgb : u_albedo;
    vec3 normal = normalize(v_normal);
    float metallic = u_metallic;
    float roughness = u_roughness;
    float ao = u_ao;

    if (u_useTextures) {
        vec4 mrSample = texture(u_metallicRoughnessMap, v_texCoord);
        metallic = mrSample.b;
        roughness = mrSample.g;
    }

    vec3 N = normal;
    vec3 V = normalize(u_cameraPos - v_worldPos);

    // Calculate F0 for dielectrics and metals
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Get cluster for this fragment
    uint clusterIndex = getClusterIndex(v_worldPos);
    uvec2 clusterData = clusters[clusterIndex];
    uint lightCount = clusterData.x;
    uint lightOffset = clusterData.y;

    // Accumulate lighting
    vec3 Lo = vec3(0.0);

    for (uint i = 0; i < lightCount; i++) {
        uint lightIndex = lightIndices[lightOffset + i];
        if (lightIndex >= uint(u_numLights)) continue;

        Light light = lights[lightIndex];

        // Skip disabled lights
        if (light.params.z < 0.5) continue;

        Lo += calculateLightContribution(light, v_worldPos, N, V, F0, albedo, roughness, metallic);
    }

    // Ambient lighting (simple approximation)
    vec3 ambient = vec3(0.03) * albedo * ao;
    vec3 color = ambient + Lo;

    // HDR tonemapping (Reinhard)
    color = color / (color + vec3(1.0));

    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, 1.0);
}
