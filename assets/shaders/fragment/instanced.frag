#version 330 core

// Input from vertex shader
in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    vec3 Tangent;
    vec3 Bitangent;
    vec4 InstanceColor;
    flat uint InstanceID;
    vec3 ViewDir;
} fs_in;

// Output
layout (location = 0) out vec4 FragColor;
layout (location = 1) out uint ObjectID;  // For picking

// Material uniforms
uniform sampler2D u_AlbedoMap;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MetallicMap;
uniform sampler2D u_RoughnessMap;
uniform sampler2D u_AOMap;

uniform vec3 u_Albedo;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AO;

uniform bool u_HasAlbedoMap;
uniform bool u_HasNormalMap;
uniform bool u_HasMetallicMap;
uniform bool u_HasRoughnessMap;
uniform bool u_HasAOMap;

// Lighting uniforms
uniform vec3 u_LightDirection;
uniform vec3 u_LightColor;
uniform float u_LightIntensity;
uniform vec3 u_AmbientColor;

// Constants
const float PI = 3.14159265359;

// PBR functions
vec3 fresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float distributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float geometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float geometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometrySchlickGGX(NdotV, roughness);
    float ggx1 = geometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 getNormalFromMap() {
    if (!u_HasNormalMap) {
        return normalize(fs_in.Normal);
    }

    vec3 tangentNormal = texture(u_NormalMap, fs_in.TexCoord).xyz * 2.0 - 1.0;

    mat3 TBN = mat3(
        normalize(fs_in.Tangent),
        normalize(fs_in.Bitangent),
        normalize(fs_in.Normal)
    );

    return normalize(TBN * tangentNormal);
}

void main() {
    // Sample textures or use defaults
    vec3 albedo = u_HasAlbedoMap ? texture(u_AlbedoMap, fs_in.TexCoord).rgb : u_Albedo;
    albedo *= fs_in.InstanceColor.rgb;  // Apply instance color tint

    float metallic = u_HasMetallicMap ? texture(u_MetallicMap, fs_in.TexCoord).r : u_Metallic;
    float roughness = u_HasRoughnessMap ? texture(u_RoughnessMap, fs_in.TexCoord).r : u_Roughness;
    float ao = u_HasAOMap ? texture(u_AOMap, fs_in.TexCoord).r : u_AO;

    // Get normal
    vec3 N = getNormalFromMap();
    vec3 V = normalize(fs_in.ViewDir);
    vec3 L = normalize(-u_LightDirection);
    vec3 H = normalize(V + L);

    // Calculate reflectance
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Cook-Torrance BRDF
    float NDF = distributionGGX(N, H, roughness);
    float G = geometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001;
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);
    vec3 Lo = (kD * albedo / PI + specular) * u_LightColor * u_LightIntensity * NdotL;

    // Ambient
    vec3 ambient = u_AmbientColor * albedo * ao;

    vec3 color = ambient + Lo;

    // HDR tonemapping
    color = color / (color + vec3(1.0));

    // Gamma correction
    color = pow(color, vec3(1.0 / 2.2));

    FragColor = vec4(color, fs_in.InstanceColor.a);
    ObjectID = fs_in.InstanceID;
}
