#version 460
#extension GL_NV_ray_tracing : require

/**
 * @file rtx_pathtracer.rmiss
 * @brief Miss Shader for RTX Path Tracing
 *
 * Executed when a ray doesn't hit any geometry.
 * Returns sky/environment color.
 */

// Ray payload
layout(location = 0) rayPayloadInNV vec3 hitValue;

// Render settings
layout(binding = 4, set = 0) uniform RenderSettings {
    vec3 lightDirection;
    float lightIntensity;
    vec3 lightColor;
    float maxDistance;
    vec3 backgroundColor;
    int maxBounces;
    int enableShadows;
    int enableGI;
    int enableAO;
    float aoRadius;
} settings;

// Optional: Environment map
layout(binding = 11, set = 0) uniform samplerCube environmentMap;
layout(binding = 12, set = 0) uniform EnvironmentSettings {
    int useEnvironmentMap;
    float environmentIntensity;
} envSettings;

// Simple procedural sky
vec3 proceduralSky(vec3 direction) {
    // Gradient from horizon to zenith
    float t = 0.5 * (direction.y + 1.0);
    vec3 skyColor = mix(vec3(1.0, 1.0, 1.0), vec3(0.5, 0.7, 1.0), t);

    // Optional: Add sun
    vec3 sunDir = normalize(-settings.lightDirection);
    float sunDot = max(dot(direction, sunDir), 0.0);
    vec3 sunColor = settings.lightColor * settings.lightIntensity * pow(sunDot, 256.0);

    return skyColor + sunColor;
}

void main() {
    vec3 rayDir = gl_WorldRayDirectionNV;

    if (envSettings.useEnvironmentMap != 0) {
        // Sample environment map
        vec3 envColor = texture(environmentMap, rayDir).rgb;
        hitValue = envColor * envSettings.environmentIntensity;
    } else {
        // Use procedural sky or background color
        hitValue = proceduralSky(rayDir);
        // Or just use solid background:
        // hitValue = settings.backgroundColor;
    }
}
