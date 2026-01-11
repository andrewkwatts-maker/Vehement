#version 450 core

// SDF raymarching fragment shader - writes to G-buffer
// Pass 1 of deferred rendering: Raymarch SDFs and output material properties

// ============================================================================
// Inputs
// ============================================================================

in vec2 v_uv;
in vec3 v_rayOrigin;
in vec3 v_rayDirection;

// ============================================================================
// Outputs (G-Buffer)
// ============================================================================

layout(location = 0) out vec4 o_albedo;       // RGB = albedo, A = metallic
layout(location = 1) out vec4 o_normal;       // RGB = normal (view space), A = roughness
layout(location = 2) out vec4 o_material;     // R = IOR, G = scattering, B = emission, A = materialID

// ============================================================================
// Uniforms
// ============================================================================

uniform mat4 u_view;
uniform mat4 u_proj;
uniform mat4 u_invView;
uniform mat4 u_invProj;
uniform vec3 u_cameraPos;

// Raymarch settings
uniform int u_maxSteps;
uniform float u_maxDistance;
uniform float u_hitThreshold;
uniform float u_normalEpsilon;

// Material properties
uniform vec3 u_albedo;
uniform float u_metallic;
uniform float u_roughness;
uniform float u_ior;
uniform float u_scattering;
uniform vec3 u_emission;
uniform uint u_materialID;

// ============================================================================
// SDF Functions
// ============================================================================

// Basic primitives
float sdSphere(vec3 p, float radius) {
    return length(p) - radius;
}

float sdBox(vec3 p, vec3 size) {
    vec3 q = abs(p) - size;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdRoundBox(vec3 p, vec3 size, float radius) {
    vec3 q = abs(p) - size;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - radius;
}

float sdCylinder(vec3 p, float radius, float height) {
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(radius, height);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

float sdCapsule(vec3 p, vec3 a, vec3 b, float r) {
    vec3 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - r;
}

// Operations
float opUnion(float d1, float d2) {
    return min(d1, d2);
}

float opSubtraction(float d1, float d2) {
    return max(-d1, d2);
}

float opIntersection(float d1, float d2) {
    return max(d1, d2);
}

float opSmoothUnion(float d1, float d2, float k) {
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) - k * h * (1.0 - h);
}

// ============================================================================
// Scene SDF
// ============================================================================

struct SDFResult {
    float distance;
    uint materialID;
};

SDFResult sceneSDF(vec3 p) {
    SDFResult result;

    // Example scene: Building made of boxes
    float building = sdBox(p - vec3(0, 5, 0), vec3(4, 5, 4));

    // Add windows (subtraction)
    for (float y = 1.0; y < 9.0; y += 2.5) {
        for (float x = -3.0; x <= 3.0; x += 2.0) {
            float window = sdBox(p - vec3(x, y, 4.1), vec3(0.6, 0.8, 0.2));
            building = opSubtraction(window, building);
        }
    }

    // Ground plane
    float ground = p.y + 0.5;

    // Combine
    result.distance = opUnion(building, ground);
    result.materialID = (building < ground) ? 1 : 0;

    return result;
}

// ============================================================================
// Normal Calculation
// ============================================================================

vec3 calculateNormal(vec3 p) {
    vec2 e = vec2(u_normalEpsilon, 0.0);

    return normalize(vec3(
        sceneSDF(p + e.xyy).distance - sceneSDF(p - e.xyy).distance,
        sceneSDF(p + e.yxy).distance - sceneSDF(p - e.yxy).distance,
        sceneSDF(p + e.yyx).distance - sceneSDF(p - e.yyx).distance
    ));
}

// ============================================================================
// Material Properties
// ============================================================================

struct Material {
    vec3 albedo;
    float metallic;
    float roughness;
    float ior;
    float scattering;
    vec3 emission;
    uint id;
};

Material getMaterial(vec3 p, uint materialID) {
    Material mat;

    if (materialID == 0) {
        // Ground
        mat.albedo = vec3(0.3, 0.3, 0.3);
        mat.metallic = 0.0;
        mat.roughness = 0.8;
        mat.ior = 1.45;
        mat.scattering = 0.0;
        mat.emission = vec3(0.0);
        mat.id = 0;
    }
    else if (materialID == 1) {
        // Building
        mat.albedo = vec3(0.8, 0.7, 0.6);
        mat.metallic = 0.1;
        mat.roughness = 0.5;
        mat.ior = 1.5;
        mat.scattering = 0.0;
        mat.emission = vec3(0.0);
        mat.id = 1;
    }
    else {
        // Default
        mat.albedo = u_albedo;
        mat.metallic = u_metallic;
        mat.roughness = u_roughness;
        mat.ior = u_ior;
        mat.scattering = u_scattering;
        mat.emission = u_emission;
        mat.id = u_materialID;
    }

    return mat;
}

// ============================================================================
// Raymarching
// ============================================================================

struct RaymarchResult {
    bool hit;
    vec3 position;
    float depth;
    uint materialID;
    int steps;
};

RaymarchResult raymarch(vec3 origin, vec3 direction) {
    RaymarchResult result;
    result.hit = false;
    result.steps = 0;

    float t = 0.0;

    for (int i = 0; i < u_maxSteps; i++) {
        result.steps = i;

        vec3 p = origin + direction * t;
        SDFResult sdf = sceneSDF(p);

        if (sdf.distance < u_hitThreshold) {
            // Hit!
            result.hit = true;
            result.position = p;
            result.materialID = sdf.materialID;

            // Calculate depth in clip space
            vec4 clipPos = u_proj * u_view * vec4(p, 1.0);
            result.depth = clipPos.z / clipPos.w;

            break;
        }

        t += sdf.distance;

        if (t > u_maxDistance) {
            break;  // Ray escaped
        }
    }

    return result;
}

// ============================================================================
// Utility Functions
// ============================================================================

vec3 reconstructRayOrigin(vec2 uv) {
    // Reconstruct ray origin from UV
    return u_cameraPos;
}

vec3 reconstructRayDir(vec2 uv) {
    // Reconstruct ray direction from UV
    vec4 clipPos = vec4(uv * 2.0 - 1.0, -1.0, 1.0);
    vec4 viewPos = u_invProj * clipPos;
    viewPos = vec4(viewPos.xyz / viewPos.w, 0.0);
    vec3 worldDir = (u_invView * viewPos).xyz;
    return normalize(worldDir);
}

// ============================================================================
// Main
// ============================================================================

void main() {
    // Reconstruct ray
    vec3 rayOrigin = reconstructRayOrigin(v_uv);
    vec3 rayDir = reconstructRayDir(v_uv);

    // Raymarch scene
    RaymarchResult result = raymarch(rayOrigin, rayDir);

    if (!result.hit) {
        discard;  // No hit - don't write to G-buffer
    }

    // Calculate surface normal
    vec3 worldNormal = calculateNormal(result.position);

    // Transform normal to view space
    vec3 viewNormal = mat3(u_view) * worldNormal;

    // Get material properties
    Material mat = getMaterial(result.position, result.materialID);

    // Write to G-buffer
    o_albedo = vec4(mat.albedo, mat.metallic);
    o_normal = vec4(viewNormal * 0.5 + 0.5, mat.roughness);  // Encode normal [0,1]
    o_material = vec4(mat.ior, mat.scattering, length(mat.emission), float(mat.id) / 255.0);

    // Write depth
    gl_FragDepth = result.depth * 0.5 + 0.5;  // Map to [0,1]
}

// ============================================================================
// Performance Notes:
// ============================================================================
//
// - Raymarch steps: 64-128 typical for buildings
// - Hit threshold: 0.001 for sharp edges
// - G-buffer write: 4 render targets (albedo, normal, material, depth)
//
// Expected performance at 1080p:
// - Simple scene: 2-3ms
// - Complex scene (1000+ SDFs): 5-8ms
//
