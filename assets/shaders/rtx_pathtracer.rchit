#version 460
#extension GL_NV_ray_tracing : require
#extension GL_EXT_nonuniform_qualifier : require

/**
 * @file rtx_pathtracer.rchit
 * @brief Closest Hit Shader for RTX Path Tracing
 *
 * Handles ray-geometry intersections and computes shading.
 * Supports PBR materials, shadows, and ambient occlusion.
 */

// Ray payload
layout(location = 0) rayPayloadInNV vec3 hitValue;
layout(location = 1) rayPayloadNV bool shadowed;

// Hit attributes (barycentric coordinates)
hitAttributeNV vec3 attribs;

// Bindings
layout(binding = 0, set = 0) uniform accelerationStructureNV topLevelAS;

// Vertex data buffers (one per instance)
layout(binding = 5, set = 0) buffer VertexPositions { vec4 data[]; } vertexPositions[];
layout(binding = 6, set = 0) buffer VertexNormals { vec4 data[]; } vertexNormals[];
layout(binding = 7, set = 0) buffer VertexTexCoords { vec2 data[]; } vertexTexCoords[];
layout(binding = 8, set = 0) buffer Indices { uint data[]; } indices[];

// Material data
struct Material {
    vec4 albedo;
    vec4 emission;
    float metallic;
    float roughness;
    float ior;
    int albedoTexture;  // Texture index (-1 if none)
    int normalTexture;
    int metallicRoughnessTexture;
    int emissionTexture;
    int _pad0;
};

layout(binding = 9, set = 0) buffer Materials {
    Material materials[];
};

// Textures
layout(binding = 10, set = 0) uniform sampler2D textures[];

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

// Random number generation
uint rngState = 0;

uint wang_hash(uint seed) {
    seed = (seed ^ 61u) ^ (seed >> 16u);
    seed *= 9u;
    seed = seed ^ (seed >> 4u);
    seed *= 0x27d4eb2du;
    seed = seed ^ (seed >> 15u);
    return seed;
}

float random() {
    rngState = wang_hash(rngState);
    return float(rngState) / 4294967296.0;
}

vec3 randomCosineDirection(vec3 normal) {
    // Generate random direction in hemisphere oriented by normal
    float r1 = random();
    float r2 = random();

    float phi = 2.0 * 3.14159265 * r1;
    float cosTheta = sqrt(1.0 - r2);
    float sinTheta = sqrt(r2);

    vec3 w = normal;
    vec3 u = normalize(cross((abs(w.x) > 0.1 ? vec3(0, 1, 0) : vec3(1, 0, 0)), w));
    vec3 v = cross(w, u);

    return normalize(u * cos(phi) * sinTheta + v * sin(phi) * sinTheta + w * cosTheta);
}

// Trace shadow ray
bool traceShadow(vec3 origin, vec3 direction, float maxDist) {
    shadowed = true;  // Assume occluded

    uint rayFlags = gl_RayFlagsTerminateOnFirstHitNV |
                    gl_RayFlagsOpaqueNV |
                    gl_RayFlagsSkipClosestHitShaderNV;

    traceNV(
        topLevelAS,
        rayFlags,
        0xFF,
        1,              // shadow hit group
        0,
        1,              // shadow miss shader
        origin,
        0.001,
        direction,
        maxDist,
        1               // payload 1 (shadowed)
    );

    return shadowed;
}

// Compute ambient occlusion
float computeAO(vec3 position, vec3 normal, int samples) {
    float occlusion = 0.0;

    for (int i = 0; i < samples; i++) {
        vec3 dir = randomCosineDirection(normal);
        bool hit = traceShadow(position, dir, settings.aoRadius);
        if (hit) {
            occlusion += 1.0;
        }
    }

    return 1.0 - (occlusion / float(samples));
}

// Simple PBR shading (diffuse + specular)
vec3 evaluateBRDF(vec3 albedo, float metallic, float roughness,
                  vec3 N, vec3 V, vec3 L) {
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float NdotV = max(dot(N, V), 0.0);

    // Diffuse
    vec3 diffuse = albedo / 3.14159265;

    // Specular (simplified GGX)
    float alpha = roughness * roughness;
    float alpha2 = alpha * alpha;
    float denom = (NdotH * NdotH * (alpha2 - 1.0) + 1.0);
    float D = alpha2 / (3.14159265 * denom * denom);

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - max(dot(H, V), 0.0), 5.0);

    float k = alpha / 2.0;
    float G = NdotV / (NdotV * (1.0 - k) + k) *
              NdotL / (NdotL * (1.0 - k) + k);

    vec3 specular = (D * F * G) / (4.0 * NdotV * NdotL + 0.001);

    return mix(diffuse, specular, metallic) * NdotL;
}

void main() {
    // Get instance and primitive IDs
    int instanceID = gl_InstanceCustomIndexNV;
    int primitiveID = gl_PrimitiveID;

    // Fetch triangle indices
    uint i0 = indices[instanceID].data[primitiveID * 3 + 0];
    uint i1 = indices[instanceID].data[primitiveID * 3 + 1];
    uint i2 = indices[instanceID].data[primitiveID * 3 + 2];

    // Fetch vertex positions
    vec3 v0 = vertexPositions[instanceID].data[i0].xyz;
    vec3 v1 = vertexPositions[instanceID].data[i1].xyz;
    vec3 v2 = vertexPositions[instanceID].data[i2].xyz;

    // Barycentric interpolation
    vec3 barycentrics = vec3(1.0 - attribs.x - attribs.y, attribs.x, attribs.y);

    // Interpolate position
    vec3 hitPos = v0 * barycentrics.x + v1 * barycentrics.y + v2 * barycentrics.z;
    hitPos = vec3(gl_ObjectToWorldNV * vec4(hitPos, 1.0));

    // Fetch and interpolate normal
    vec3 n0 = vertexNormals[instanceID].data[i0].xyz;
    vec3 n1 = vertexNormals[instanceID].data[i1].xyz;
    vec3 n2 = vertexNormals[instanceID].data[i2].xyz;
    vec3 normal = normalize(n0 * barycentrics.x + n1 * barycentrics.y + n2 * barycentrics.z);
    normal = normalize(vec3(gl_ObjectToWorldNV * vec4(normal, 0.0)));

    // Fetch material
    Material mat = materials[instanceID];
    vec3 albedo = mat.albedo.rgb;
    vec3 emission = mat.emission.rgb;
    float metallic = mat.metallic;
    float roughness = mat.roughness;

    // Sample textures if available
    if (mat.albedoTexture >= 0) {
        vec2 t0 = vertexTexCoords[instanceID].data[i0];
        vec2 t1 = vertexTexCoords[instanceID].data[i1];
        vec2 t2 = vertexTexCoords[instanceID].data[i2];
        vec2 texCoord = t0 * barycentrics.x + t1 * barycentrics.y + t2 * barycentrics.z;
        albedo *= texture(textures[mat.albedoTexture], texCoord).rgb;
    }

    // View direction
    vec3 V = normalize(-gl_WorldRayDirectionNV);

    // Direct lighting
    vec3 L = normalize(-settings.lightDirection);
    vec3 directLight = vec3(0.0);

    if (settings.enableShadows != 0) {
        // Check shadow
        bool inShadow = traceShadow(hitPos + normal * 0.001, L, settings.maxDistance);
        if (!inShadow) {
            directLight = evaluateBRDF(albedo, metallic, roughness, normal, V, L) *
                         settings.lightColor * settings.lightIntensity;
        }
    } else {
        directLight = evaluateBRDF(albedo, metallic, roughness, normal, V, L) *
                     settings.lightColor * settings.lightIntensity;
    }

    // Ambient occlusion
    float ao = 1.0;
    if (settings.enableAO != 0) {
        // Initialize RNG for AO
        uint pixelIndex = gl_LaunchIDNV.y * gl_LaunchSizeNV.x + gl_LaunchIDNV.x;
        rngState = wang_hash(pixelIndex + primitiveID * 719393u);

        ao = computeAO(hitPos + normal * 0.001, normal, 4);
    }

    // Ambient term
    vec3 ambient = albedo * 0.03 * ao;

    // Final color
    vec3 color = emission + ambient + directLight;

    hitValue = color;
}
