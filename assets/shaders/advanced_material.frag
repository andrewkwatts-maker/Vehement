#version 450 core

// Advanced Material Shader
// Supports: PBR, Dispersion, Blackbody, Subsurface Scattering, Emission

// Inputs
in vec2 v_TexCoord;
in vec3 v_WorldPos;
in vec3 v_Normal;
in vec3 v_Tangent;
in vec3 v_Bitangent;
in vec4 v_VertexColor;

// Outputs
out vec4 FragColor;

// Material properties
uniform vec3 u_Albedo;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_Specular;
uniform float u_IOR;
uniform float u_Transmission;

// Optical properties
uniform bool u_EnableDispersion;
uniform float u_AbbeNumber;

// Emission
uniform bool u_EnableEmission;
uniform bool u_UseBlackbody;
uniform float u_Temperature;
uniform float u_Luminosity;
uniform vec3 u_EmissionColor;
uniform float u_EmissionStrength;

// Subsurface scattering
uniform bool u_EnableSSS;
uniform float u_SSSRadius;
uniform vec3 u_SSSColor;
uniform float u_SSSCoefficient;

// Textures
uniform sampler2D u_AlbedoMap;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MetallicMap;
uniform sampler2D u_RoughnessMap;
uniform sampler2D u_EmissionMap;
uniform sampler2D u_AOMap;

uniform bool u_UseAlbedoMap;
uniform bool u_UseNormalMap;
uniform bool u_UseMetallicMap;
uniform bool u_UseRoughnessMap;
uniform bool u_UseEmissionMap;

// Lighting
uniform vec3 u_LightPosition;
uniform vec3 u_LightColor;
uniform float u_LightIntensity;

// Camera
uniform vec3 u_CameraPos;
uniform float u_Time;

// Constants
const float PI = 3.14159265359;

// ============================================================================
// Utility Functions
// ============================================================================

vec3 GetNormal() {
    if (u_UseNormalMap) {
        vec3 tangentNormal = texture(u_NormalMap, v_TexCoord).xyz * 2.0 - 1.0;
        mat3 TBN = mat3(v_Tangent, v_Bitangent, v_Normal);
        return normalize(TBN * tangentNormal);
    }
    return normalize(v_Normal);
}

vec3 GetAlbedo() {
    if (u_UseAlbedoMap) {
        return pow(texture(u_AlbedoMap, v_TexCoord).rgb, vec3(2.2)) * u_Albedo;
    }
    return u_Albedo;
}

float GetMetallic() {
    if (u_UseMetallicMap) {
        return texture(u_MetallicMap, v_TexCoord).r * u_Metallic;
    }
    return u_Metallic;
}

float GetRoughness() {
    if (u_UseRoughnessMap) {
        return texture(u_RoughnessMap, v_TexCoord).r * u_Roughness;
    }
    return clamp(u_Roughness, 0.01, 1.0);
}

// ============================================================================
// Blackbody Radiation (Mitchell's approximation)
// ============================================================================

vec3 TemperatureToRGB(float temp) {
    temp = temp / 100.0;
    float r, g, b;

    // Red
    if (temp <= 66.0) {
        r = 1.0;
    } else {
        r = temp - 60.0;
        r = 1.29293618606 * pow(r, -0.1332047592);
        r = clamp(r, 0.0, 1.0);
    }

    // Green
    if (temp <= 66.0) {
        g = temp;
        g = 0.39008157876 * log(g) - 0.63184144378;
    } else {
        g = temp - 60.0;
        g = 1.12989086089 * pow(g, -0.0755148492);
    }
    g = clamp(g, 0.0, 1.0);

    // Blue
    if (temp >= 66.0) {
        b = 1.0;
    } else if (temp <= 19.0) {
        b = 0.0;
    } else {
        b = temp - 10.0;
        b = 0.54320678911 * log(b) - 1.19625408134;
        b = clamp(b, 0.0, 1.0);
    }

    return vec3(r, g, b);
}

// ============================================================================
// PBR Functions
// ============================================================================

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return nom / max(denom, 0.0001);
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / max(denom, 0.0001);
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(1.0 - cosTheta, 5.0);
}

// ============================================================================
// Dispersion (wavelength-dependent IOR)
// ============================================================================

float GetDispersedIOR(float baseIOR, float abbeNumber, float wavelength) {
    const float lambda_d = 587.6;  // D-line (nm)
    const float lambda_F = 486.1;  // F-line (nm)
    const float lambda_C = 656.3;  // C-line (nm)

    float V_d = abbeNumber;
    float n_d = baseIOR;
    float delta_n = (n_d - 1.0) / V_d;

    float wavelength_factor = (wavelength - lambda_d) / (lambda_F - lambda_C);
    return n_d + delta_n * wavelength_factor;
}

vec3 ApplyChromat icDispersion(vec3 viewDir, vec3 normal) {
    if (!u_EnableDispersion) {
        return vec3(1.0);
    }

    // RGB wavelengths
    const float redWavelength = 630.0;
    const float greenWavelength = 530.0;
    const float blueWavelength = 470.0;

    // Calculate dispersed IORs
    float iorRed = GetDispersedIOR(u_IOR, u_AbbeNumber, redWavelength);
    float iorGreen = GetDispersedIOR(u_IOR, u_AbbeNumber, greenWavelength);
    float iorBlue = GetDispersedIOR(u_IOR, u_AbbeNumber, blueWavelength);

    // Refraction for each wavelength
    vec3 refractedRed = refract(viewDir, normal, 1.0 / iorRed);
    vec3 refractedGreen = refract(viewDir, normal, 1.0 / iorGreen);
    vec3 refractedBlue = refract(viewDir, normal, 1.0 / iorBlue);

    // Sample environment/background with offset directions
    // For now, just return chromatic aberration effect
    float separation = length(refractedRed - refractedBlue) * 0.5;
    return vec3(1.0 - separation, 1.0, 1.0 - separation);
}

// ============================================================================
// Subsurface Scattering (simplified)
// ============================================================================

vec3 SubsurfaceScattering(vec3 N, vec3 L, vec3 albedo) {
    if (!u_EnableSSS) {
        return vec3(0.0);
    }

    float distortion = 0.2;
    float scale = u_SSSRadius;
    float power = 2.0;

    vec3 H = normalize(L + N * distortion);
    float NdotH = max(0.0, dot(N, -H));
    float scatter = pow(NdotH, power) * scale;

    return u_SSSColor * albedo * scatter;
}

// ============================================================================
// Emission
// ============================================================================

vec3 GetEmission() {
    if (!u_EnableEmission) {
        return vec3(0.0);
    }

    vec3 emission;

    if (u_UseBlackbody) {
        emission = TemperatureToRGB(u_Temperature);
        emission *= u_Luminosity;
    } else {
        emission = u_EmissionColor * u_EmissionStrength;
    }

    if (u_UseEmissionMap) {
        emission *= texture(u_EmissionMap, v_TexCoord).rgb;
    }

    return emission;
}

// ============================================================================
// Main PBR Lighting
// ============================================================================

vec3 CalculatePBR(vec3 albedo, vec3 N, vec3 V, vec3 L, float metallic, float roughness) {
    vec3 H = normalize(V + L);

    // Calculate F0 (base reflectivity)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    vec3 specular = numerator / max(denominator, 0.001);

    float NdotL = max(dot(N, L), 0.0);
    vec3 radiance = u_LightColor * u_LightIntensity;

    return (kD * albedo / PI + specular) * radiance * NdotL;
}

// ============================================================================
// Main Function
// ============================================================================

void main() {
    // Get material properties
    vec3 albedo = GetAlbedo();
    vec3 N = GetNormal();
    vec3 V = normalize(u_CameraPos - v_WorldPos);
    vec3 L = normalize(u_LightPosition - v_WorldPos);
    float metallic = GetMetallic();
    float roughness = GetRoughness();

    // Calculate lighting
    vec3 color = vec3(0.0);

    // PBR direct lighting
    color += CalculatePBR(albedo, N, V, L, metallic, roughness);

    // Subsurface scattering
    if (u_EnableSSS) {
        color += SubsurfaceScattering(N, L, albedo);
    }

    // Chromatic dispersion effect
    if (u_EnableDispersion && u_Transmission > 0.0) {
        vec3 dispersionTint = ApplyChromaticDispersion(V, N);
        color *= dispersionTint;
    }

    // Emission
    vec3 emission = GetEmission();
    color += emission;

    // Ambient occlusion
    if (u_UseAlbedoMap) {
        float ao = texture(u_AOMap, v_TexCoord).r;
        color *= ao;
    }

    // Tone mapping and gamma correction
    color = color / (color + vec3(1.0));  // Reinhard tone mapping
    color = pow(color, vec3(1.0/2.2));    // Gamma correction

    FragColor = vec4(color, 1.0);
}
