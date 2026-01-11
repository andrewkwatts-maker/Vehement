#version 460 core

out vec4 FragColor;

in vec2 TexCoords;

// Camera uniforms
uniform mat4 u_view;
uniform mat4 u_projection;
uniform mat4 u_invView;
uniform mat4 u_invProjection;
uniform vec3 u_cameraPos;
uniform vec3 u_cameraDir;

// Raymarching settings
uniform int u_maxSteps;
uniform float u_maxDistance;
uniform float u_hitThreshold;

// Quality settings
uniform bool u_enableShadows;
uniform bool u_enableAO;
uniform bool u_enableReflections;

// Shadow settings
uniform float u_shadowSoftness;
uniform int u_shadowSteps;

// AO settings
uniform int u_aoSteps;
uniform float u_aoDistance;
uniform float u_aoIntensity;

// Lighting
uniform vec3 u_lightDirection;
uniform vec3 u_lightColor;
uniform float u_lightIntensity;

// Background
uniform vec3 u_backgroundColor;
uniform bool u_useEnvironmentMap;
uniform samplerCube u_environmentMap;

// Primitive count
uniform int u_primitiveCount;

// Global Illumination
uniform bool u_enableGI;
uniform int u_numCascades;
uniform vec3 u_cascadeOrigin;
uniform float u_cascadeSpacing;
uniform sampler3D u_cascadeTexture0;
uniform sampler3D u_cascadeTexture1;
uniform sampler3D u_cascadeTexture2;
uniform sampler3D u_cascadeTexture3;

// Spectral rendering
uniform int u_spectralMode;  // 0=RGB, 1=Spectral, 2=HeroWavelength
uniform bool u_enableDispersion;
uniform bool u_enableDiffraction;
uniform float u_wavelengthMin;
uniform float u_wavelengthMax;

// Primitive data structure
struct SDFPrimitiveData {
    mat4 transform;
    mat4 inverseTransform;
    vec4 parameters;
    vec4 parameters2;
    vec4 parameters3;
    vec4 material;      // metallic, roughness, emissive, padding
    vec4 baseColor;
    vec4 emissiveColor;
    int type;
    int csgOperation;
    int visible;
    int padding;
};

// SSBO for primitives
layout(std430, binding = 0) buffer PrimitivesBuffer {
    SDFPrimitiveData primitives[];
};

// Constants
const float PI = 3.14159265359;
const float EPSILON = 0.001;

// ============================================================================
// Black Body Radiation
// ============================================================================

vec3 BlackbodyColor(float temperature) {
    temperature = clamp(temperature, 1000.0, 40000.0);
    float t = temperature / 100.0;
    vec3 color;

    // Red
    if (t <= 66.0) {
        color.r = 1.0;
    } else {
        color.r = clamp((329.698727446 * pow(t - 60.0, -0.1332047592)) / 255.0, 0.0, 1.0);
    }

    // Green
    if (t <= 66.0) {
        color.g = clamp((99.4708025861 * log(t) - 161.1195681661) / 255.0, 0.0, 1.0);
    } else {
        color.g = clamp((288.1221695283 * pow(t - 60.0, -0.0755148492)) / 255.0, 0.0, 1.0);
    }

    // Blue
    if (t >= 66.0) {
        color.b = 1.0;
    } else if (t <= 19.0) {
        color.b = 0.0;
    } else {
        color.b = clamp((138.5177312231 * log(t - 10.0) - 305.0447927307) / 255.0, 0.0, 1.0);
    }

    return color;
}

float BlackbodyIntensity(float temperature) {
    const float sigma = 5.670374419e-8;
    float T4 = pow(temperature, 4.0);
    const float sunTemp = 5778.0;
    float sunIntensity = sigma * pow(sunTemp, 4.0);
    return (sigma * T4) / sunIntensity;
}

// ============================================================================
// Spectral Rendering - CIE 1931 Color Matching
// ============================================================================

vec3 WavelengthToRGB(float wavelength) {
    // CIE 1931 standard observer approximation
    float x, y, z;

    if (wavelength >= 380.0 && wavelength < 440.0) {
        x = -(wavelength - 440.0) / (440.0 - 380.0);
        y = 0.0;
        z = 1.0;
    } else if (wavelength >= 440.0 && wavelength < 490.0) {
        x = 0.0;
        y = (wavelength - 440.0) / (490.0 - 440.0);
        z = 1.0;
    } else if (wavelength >= 490.0 && wavelength < 510.0) {
        x = 0.0;
        y = 1.0;
        z = -(wavelength - 510.0) / (510.0 - 490.0);
    } else if (wavelength >= 510.0 && wavelength < 580.0) {
        x = (wavelength - 510.0) / (580.0 - 510.0);
        y = 1.0;
        z = 0.0;
    } else if (wavelength >= 580.0 && wavelength < 645.0) {
        x = 1.0;
        y = -(wavelength - 645.0) / (645.0 - 580.0);
        z = 0.0;
    } else if (wavelength >= 645.0 && wavelength <= 780.0) {
        x = 1.0;
        y = 0.0;
        z = 0.0;
    } else {
        x = 0.0;
        y = 0.0;
        z = 0.0;
    }

    // Intensity falloff at edges
    float intensity;
    if (wavelength >= 380.0 && wavelength < 420.0) {
        intensity = 0.3 + 0.7 * (wavelength - 380.0) / (420.0 - 380.0);
    } else if (wavelength >= 420.0 && wavelength < 700.0) {
        intensity = 1.0;
    } else if (wavelength >= 700.0 && wavelength <= 780.0) {
        intensity = 0.3 + 0.7 * (780.0 - wavelength) / (780.0 - 700.0);
    } else {
        intensity = 0.0;
    }

    return vec3(x, y, z) * intensity;
}

// ============================================================================
// Sellmeier Dispersion Equation (for BK7 glass)
// ============================================================================

float GetDispersedIOR(float baseIOR, float wavelength_nm) {
    float lambda = wavelength_nm / 1000.0;  // Convert to micrometers
    float lambda2 = lambda * lambda;

    // Sellmeier coefficients for BK7 glass
    const float B1 = 1.03961212;
    const float B2 = 0.231792344;
    const float B3 = 1.01046945;
    const float C1 = 0.00600069867;
    const float C2 = 0.0200179144;
    const float C3 = 103.560653;

    float n2 = 1.0 + (B1 * lambda2) / (lambda2 - C1) +
                     (B2 * lambda2) / (lambda2 - C2) +
                     (B3 * lambda2) / (lambda2 - C3);

    return sqrt(n2);
}

// ============================================================================
// Radiance Cascade Global Illumination
// ============================================================================

vec3 SampleRadianceCascade(vec3 worldPos, vec3 normal) {
    if (!u_enableGI) return vec3(0.0);

    vec3 radiance = vec3(0.0);
    vec3 localPos = worldPos - u_cascadeOrigin;

    for (int i = 0; i < u_numCascades; ++i) {
        float spacing = u_cascadeSpacing * pow(2.0, float(i));
        vec3 gridPos = localPos / spacing;
        vec3 uvw = gridPos * 0.5 + 0.5;

        if (all(greaterThan(uvw, vec3(0.0))) && all(lessThan(uvw, vec3(1.0)))) {
            vec4 cascadeSample;
            if (i == 0) cascadeSample = texture(u_cascadeTexture0, uvw);
            else if (i == 1) cascadeSample = texture(u_cascadeTexture1, uvw);
            else if (i == 2) cascadeSample = texture(u_cascadeTexture2, uvw);
            else cascadeSample = texture(u_cascadeTexture3, uvw);

            float weight = cascadeSample.a;
            radiance += cascadeSample.rgb * weight;
        }
    }

    // Weight by normal (hemisphere weighting)
    return radiance * max(0.0, dot(normal, vec3(0.0, 1.0, 0.0)));
}

// ============================================================================
// SDF Primitive Evaluation Functions
// ============================================================================

float sdSphere(vec3 p, float r) {
    return length(p) - r;
}

float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdRoundedBox(vec3 p, vec3 b, float r) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}

float sdCylinder(vec3 p, float h, float r) {
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdCapsule(vec3 p, float h, float r) {
    p.y -= clamp(p.y, 0.0, h);
    return length(p) - r;
}

float sdCone(vec3 p, float h, float r) {
    vec2 c = vec2(sin(atan(r / h)), cos(atan(r / h)));
    vec2 q = vec2(length(p.xz), p.y);
    float d1 = -q.y - h;
    float d2 = max(dot(q, c), q.y);
    float d = length(max(vec2(d1, d2), 0.0)) + min(max(d1, d2), 0.0);
    return d;
}

float sdTorus(vec3 p, float majorR, float minorR) {
    vec2 q = vec2(length(p.xz) - majorR, p.y);
    return length(q) - minorR;
}

float sdEllipsoid(vec3 p, vec3 r) {
    float k0 = length(p / r);
    float k1 = length(p / (r * r));
    return k0 * (k0 - 1.0) / k1;
}

// CSG Operations
float opUnion(float d1, float d2) {
    return min(d1, d2);
}

float opSubtraction(float d1, float d2) {
    return max(d1, -d2);
}

float opIntersection(float d1, float d2) {
    return max(d1, d2);
}

float opSmoothUnion(float d1, float d2, float k) {
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) - k * h * (1.0 - h);
}

float opSmoothSubtraction(float d1, float d2, float k) {
    float h = clamp(0.5 - 0.5 * (d2 + d1) / k, 0.0, 1.0);
    return mix(d2, -d1, h) + k * h * (1.0 - h);
}

float opSmoothIntersection(float d1, float d2, float k) {
    float h = clamp(0.5 - 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) + k * h * (1.0 - h);
}

// ============================================================================
// Scene SDF Evaluation
// ============================================================================

struct SDFResult {
    float distance;
    int primitiveIndex;
    vec4 baseColor;
    float metallic;
    float roughness;
    float emissive;
    vec3 emissiveColor;
};

SDFResult EvaluateSDF(vec3 p) {
    SDFResult result;
    result.distance = u_maxDistance;
    result.primitiveIndex = -1;
    result.baseColor = vec4(0.8);
    result.metallic = 0.0;
    result.roughness = 0.5;
    result.emissive = 0.0;
    result.emissiveColor = vec3(0.0);

    for (int i = 0; i < u_primitiveCount; ++i) {
        SDFPrimitiveData prim = primitives[i];
        if (prim.visible == 0) continue;

        // Transform point to primitive's local space
        vec3 localP = (prim.inverseTransform * vec4(p, 1.0)).xyz;

        float dist = u_maxDistance;

        // Evaluate primitive based on type
        if (prim.type == 0) {  // Sphere
            dist = sdSphere(localP, prim.parameters.x);
        } else if (prim.type == 1) {  // Box
            if (prim.parameters2.w > 0.0) {  // Rounded box
                dist = sdRoundedBox(localP, prim.parameters.yzw, prim.parameters2.w);
            } else {
                dist = sdBox(localP, prim.parameters.yzw);
            }
        } else if (prim.type == 2) {  // Cylinder
            dist = sdCylinder(localP, prim.parameters2.x, prim.parameters.x);
        } else if (prim.type == 3) {  // Capsule
            dist = sdCapsule(localP, prim.parameters2.x, prim.parameters.x);
        } else if (prim.type == 4) {  // Cone
            dist = sdCone(localP, prim.parameters2.x, prim.parameters.x);
        } else if (prim.type == 5) {  // Torus
            dist = sdTorus(localP, prim.parameters3.x, prim.parameters3.y);
        } else if (prim.type == 7) {  // RoundedBox
            dist = sdRoundedBox(localP, prim.parameters.yzw, prim.parameters2.w);
        } else if (prim.type == 8) {  // Ellipsoid
            dist = sdEllipsoid(localP, prim.parameters3.xyz);
        }

        // Apply CSG operation
        float smoothness = prim.parameters3.z;
        if (prim.csgOperation == 0) {  // Union
            if (smoothness > 0.0) {
                result.distance = opSmoothUnion(result.distance, dist, smoothness);
            } else {
                result.distance = opUnion(result.distance, dist);
            }
        } else if (prim.csgOperation == 1) {  // Subtraction
            if (smoothness > 0.0) {
                result.distance = opSmoothSubtraction(result.distance, dist, smoothness);
            } else {
                result.distance = opSubtraction(result.distance, dist);
            }
        } else if (prim.csgOperation == 2) {  // Intersection
            if (smoothness > 0.0) {
                result.distance = opSmoothIntersection(result.distance, dist, smoothness);
            } else {
                result.distance = opIntersection(result.distance, dist);
            }
        }

        // Update material if this is the closest primitive
        if (dist < abs(result.distance) + EPSILON) {
            result.primitiveIndex = i;
            result.baseColor = prim.baseColor;
            result.metallic = prim.material.x;
            result.roughness = prim.material.y;
            result.emissive = prim.material.z;
            result.emissiveColor = prim.emissiveColor.rgb;
        }
    }

    return result;
}

// ============================================================================
// Normal Calculation
// ============================================================================

vec3 CalculateNormal(vec3 p) {
    const vec2 e = vec2(EPSILON, 0.0);
    return normalize(vec3(
        EvaluateSDF(p + e.xyy).distance - EvaluateSDF(p - e.xyy).distance,
        EvaluateSDF(p + e.yxy).distance - EvaluateSDF(p - e.yxy).distance,
        EvaluateSDF(p + e.yyx).distance - EvaluateSDF(p - e.yyx).distance
    ));
}

// ============================================================================
// Soft Shadows
// ============================================================================

float SoftShadow(vec3 ro, vec3 rd, float mint, float maxt, float k) {
    float res = 1.0;
    float t = mint;

    for (int i = 0; i < u_shadowSteps; ++i) {
        float h = EvaluateSDF(ro + rd * t).distance;
        if (h < u_hitThreshold) {
            return 0.0;
        }
        res = min(res, k * h / t);
        t += h;
        if (t >= maxt) break;
    }

    return res;
}

// ============================================================================
// Ambient Occlusion
// ============================================================================

float AmbientOcclusion(vec3 p, vec3 n) {
    float occ = 0.0;
    float sca = 1.0;

    for (int i = 0; i < u_aoSteps; ++i) {
        float h = 0.01 + u_aoDistance * float(i) / float(u_aoSteps);
        float d = EvaluateSDF(p + h * n).distance;
        occ += (h - d) * sca;
        sca *= 0.95;
    }

    return clamp(1.0 - u_aoIntensity * occ, 0.0, 1.0);
}

// ============================================================================
// PBR Lighting
// ============================================================================

vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 CalculateLighting(vec3 worldPos, vec3 normal, vec3 viewDir,
                       vec3 albedo, float metallic, float roughness, vec3 emissive) {
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    vec3 L = normalize(-u_lightDirection);
    vec3 H = normalize(viewDir + L);

    // Direct lighting
    float NdotL = max(dot(normal, L), 0.0);

    // Shadow
    float shadow = 1.0;
    if (u_enableShadows && NdotL > 0.0) {
        shadow = SoftShadow(worldPos + normal * 0.01, L, 0.01, 100.0, u_shadowSoftness);
    }

    // Cook-Torrance BRDF
    float NDF = DistributionGGX(normal, H, roughness);
    float G = GeometrySmith(normal, viewDir, L, roughness);
    vec3 F = FresnelSchlick(max(dot(H, viewDir), 0.0), F0);

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(normal, viewDir), 0.0) * NdotL + 0.001;
    vec3 specular = numerator / denominator;

    vec3 directLight = (kD * albedo / PI + specular) * u_lightColor * u_lightIntensity * NdotL * shadow;

    // Ambient / Global Illumination
    vec3 ambient = vec3(0.03) * albedo;

    // Add radiance cascade GI
    vec3 giContribution = SampleRadianceCascade(worldPos, normal);
    ambient += giContribution * albedo;

    // Ambient Occlusion
    float ao = 1.0;
    if (u_enableAO) {
        ao = AmbientOcclusion(worldPos, normal);
    }

    ambient *= ao;

    // Emissive
    vec3 emission = emissive;

    return directLight + ambient + emission;
}

// ============================================================================
// Raymarching
// ============================================================================

struct RayMarchResult {
    bool hit;
    vec3 position;
    float distance;
    SDFResult sdfResult;
};

RayMarchResult Raymarch(vec3 ro, vec3 rd) {
    RayMarchResult result;
    result.hit = false;
    result.distance = 0.0;

    float t = 0.0;
    for (int i = 0; i < u_maxSteps; ++i) {
        vec3 p = ro + rd * t;
        SDFResult sdf = EvaluateSDF(p);

        if (abs(sdf.distance) < u_hitThreshold) {
            result.hit = true;
            result.position = p;
            result.distance = t;
            result.sdfResult = sdf;
            break;
        }

        t += abs(sdf.distance);

        if (t >= u_maxDistance) {
            break;
        }
    }

    return result;
}

// ============================================================================
// Main
// ============================================================================

void main() {
    // Reconstruct world-space ray
    vec4 clipSpace = vec4(TexCoords * 2.0 - 1.0, 1.0, 1.0);
    vec4 viewSpace = u_invProjection * clipSpace;
    viewSpace /= viewSpace.w;
    vec3 rayDir = normalize((u_invView * vec4(viewSpace.xyz, 0.0)).xyz);

    // Raymarch
    RayMarchResult result = Raymarch(u_cameraPos, rayDir);

    if (result.hit) {
        vec3 normal = CalculateNormal(result.position);
        vec3 viewDir = normalize(u_cameraPos - result.position);

        // Calculate lighting
        vec3 albedo = result.sdfResult.baseColor.rgb;
        float metallic = result.sdfResult.metallic;
        float roughness = result.sdfResult.roughness;
        vec3 emissive = result.sdfResult.emissiveColor * result.sdfResult.emissive;

        vec3 color = CalculateLighting(result.position, normal, viewDir,
                                       albedo, metallic, roughness, emissive);

        FragColor = vec4(color, 1.0);
    } else {
        // Background
        if (u_useEnvironmentMap) {
            FragColor = texture(u_environmentMap, rayDir);
        } else {
            FragColor = vec4(u_backgroundColor, 1.0);
        }
    }
}
