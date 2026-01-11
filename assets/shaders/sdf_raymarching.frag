#version 430 core

out vec4 FragColor;

in vec2 v_texCoord;

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

// Primitive data
uniform int u_primitiveCount;

// Primitive types
const int PRIM_SPHERE = 0;
const int PRIM_BOX = 1;
const int PRIM_CYLINDER = 2;
const int PRIM_CAPSULE = 3;
const int PRIM_CONE = 4;
const int PRIM_TORUS = 5;
const int PRIM_PLANE = 6;
const int PRIM_ROUNDED_BOX = 7;
const int PRIM_ELLIPSOID = 8;
const int PRIM_PYRAMID = 9;
const int PRIM_PRISM = 10;

// CSG operations
const int CSG_UNION = 0;
const int CSG_SUBTRACTION = 1;
const int CSG_INTERSECTION = 2;
const int CSG_SMOOTH_UNION = 3;
const int CSG_SMOOTH_SUBTRACTION = 4;
const int CSG_SMOOTH_INTERSECTION = 5;

// Primitive data structure
struct SDFPrimitive {
    mat4 transform;
    mat4 inverseTransform;
    vec4 parameters;      // radius, dimensions.xyz
    vec4 parameters2;     // height, topRadius, bottomRadius, cornerRadius
    vec4 parameters3;     // majorRadius, minorRadius, smoothness, sides
    vec4 material;        // metallic, roughness, emissive, unused
    vec4 baseColor;
    vec4 emissiveColor;
    int type;
    int csgOperation;
    int visible;
    int padding;
};

// SSBO for primitives
layout(std430, binding = 0) buffer PrimitivesBuffer {
    SDFPrimitive primitives[];
};

// ===========================================================================
// SDF Primitive Functions
// ===========================================================================

float sdSphere(vec3 p, float r) {
    return length(p) - r;
}

float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

float sdRoundedBox(vec3 p, vec3 b, float r) {
    vec3 q = abs(p) - b + vec3(r);
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}

float sdCylinder(vec3 p, float h, float r) {
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(r, h * 0.5);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdCapsule(vec3 p, float h, float r) {
    float halfHeight = h * 0.5 - r;
    p.y -= clamp(p.y, -halfHeight, halfHeight);
    return length(p) - r;
}

float sdCone(vec3 p, float h, float r) {
    vec2 c = normalize(vec2(r, h));
    float q = length(p.xz);
    return dot(c, vec2(q, p.y + h * 0.5));
}

float sdTorus(vec3 p, float R, float r) {
    vec2 q = vec2(length(p.xz) - R, p.y);
    return length(q) - r;
}

float sdPlane(vec3 p, vec3 n, float offset) {
    return dot(p, n) + offset;
}

float sdEllipsoid(vec3 p, vec3 r) {
    float k0 = length(p / r);
    float k1 = length(p / (r * r));
    return k0 * (k0 - 1.0) / k1;
}

float sdPyramid(vec3 p, float h, float baseSize) {
    p.y += h * 0.5;
    float m2 = h * h + 0.25;

    vec2 pxz = abs(p.xz);
    if (pxz.y > pxz.x) pxz = pxz.yx;
    pxz -= baseSize * 0.5;

    vec3 q = vec3(pxz.y, h * pxz.x - baseSize * 0.5 * p.y, h * p.y + baseSize * 0.5 * pxz.x);

    float s = max(-q.x, 0.0);
    float t = clamp((q.y - 0.5 * baseSize * q.z) / (m2 + 0.25 * baseSize * baseSize), 0.0, 1.0);

    float a = m2 * (q.x + s) * (q.x + s) + q.y * q.y;
    float b = m2 * (q.x + 0.5 * t) * (q.x + 0.5 * t) + (q.y - m2 * t) * (q.y - m2 * t);

    float d2 = max(-q.y, q.x * m2 + q.y * 0.5) < 0.0 ? 0.0 : min(a, b);

    return sqrt((d2 + q.z * q.z) / m2) * (max(q.z, -p.y) < 0.0 ? -1.0 : 1.0);
}

float sdPrism(vec3 p, int sides, float radius, float height) {
    const float PI = 3.14159265359;
    float angle = PI / float(sides);
    vec2 cs = vec2(cos(angle), sin(angle));

    vec2 pxz = p.xz;
    float phi = atan(pxz.y, pxz.x);
    float sector = floor(phi / (2.0 * angle) + 0.5);
    float sectorAngle = sector * 2.0 * angle;

    vec2 rotated;
    rotated.x = pxz.x * cos(-sectorAngle) - pxz.y * sin(-sectorAngle);
    rotated.y = pxz.x * sin(-sectorAngle) + pxz.y * cos(-sectorAngle);

    vec2 d = abs(vec2(rotated.x, p.y)) - vec2(radius * cs.x, height * 0.5);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

// ===========================================================================
// CSG Operations
// ===========================================================================

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

float opSmoothSubtraction(float d1, float d2, float k) {
    float h = clamp(0.5 - 0.5 * (d2 + d1) / k, 0.0, 1.0);
    return mix(d2, -d1, h) + k * h * (1.0 - h);
}

float opSmoothIntersection(float d1, float d2, float k) {
    float h = clamp(0.5 - 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) + k * h * (1.0 - h);
}

// ===========================================================================
// SDF Evaluation
// ===========================================================================

float evaluatePrimitive(SDFPrimitive prim, vec3 worldPos) {
    // Transform to local space
    vec3 localPos = (prim.inverseTransform * vec4(worldPos, 1.0)).xyz;

    float dist = 1e10;

    if (prim.type == PRIM_SPHERE) {
        dist = sdSphere(localPos, prim.parameters.x);
    }
    else if (prim.type == PRIM_BOX) {
        dist = sdBox(localPos, prim.parameters.yzw * 0.5);
    }
    else if (prim.type == PRIM_ROUNDED_BOX) {
        dist = sdRoundedBox(localPos, prim.parameters.yzw * 0.5, prim.parameters2.w);
    }
    else if (prim.type == PRIM_CYLINDER) {
        dist = sdCylinder(localPos, prim.parameters2.x, prim.parameters.x);
    }
    else if (prim.type == PRIM_CAPSULE) {
        dist = sdCapsule(localPos, prim.parameters2.x, prim.parameters.x);
    }
    else if (prim.type == PRIM_CONE) {
        dist = sdCone(localPos, prim.parameters2.x, prim.parameters2.z);
    }
    else if (prim.type == PRIM_TORUS) {
        dist = sdTorus(localPos, prim.parameters3.x, prim.parameters3.y);
    }
    else if (prim.type == PRIM_PLANE) {
        dist = sdPlane(localPos, vec3(0, 1, 0), 0.0);
    }
    else if (prim.type == PRIM_ELLIPSOID) {
        // Radii stored in parameters3.rgb (though we use parameters for consistency)
        vec3 radii = vec3(prim.parameters.x, prim.parameters.y, prim.parameters.z);
        dist = sdEllipsoid(localPos, radii);
    }
    else if (prim.type == PRIM_PYRAMID) {
        dist = sdPyramid(localPos, prim.parameters2.x, prim.parameters2.z);
    }
    else if (prim.type == PRIM_PRISM) {
        int sides = int(prim.parameters3.w);
        dist = sdPrism(localPos, sides, prim.parameters2.z, prim.parameters2.x);
    }

    return dist;
}

struct SDFResult {
    float distance;
    int primitiveIndex;
    vec4 baseColor;
    vec4 material;
    vec4 emissiveColor;
};

SDFResult evaluateScene(vec3 p) {
    SDFResult result;
    result.distance = 1e10;
    result.primitiveIndex = -1;
    result.baseColor = vec4(1.0);
    result.material = vec4(0.0, 0.5, 0.0, 0.0);
    result.emissiveColor = vec4(0.0);

    if (u_primitiveCount == 0) {
        return result;
    }

    // Evaluate root primitive (assuming index 0 is root)
    float dist = evaluatePrimitive(primitives[0], p);
    result.distance = dist;
    result.primitiveIndex = 0;
    result.baseColor = primitives[0].baseColor;
    result.material = primitives[0].material;
    result.emissiveColor = primitives[0].emissiveColor;

    // Apply CSG operations for children
    for (int i = 1; i < u_primitiveCount; i++) {
        if (primitives[i].visible == 0) continue;

        float childDist = evaluatePrimitive(primitives[i], p);
        float smoothness = primitives[i].parameters3.z;
        int op = primitives[i].csgOperation;

        float oldDist = result.distance;

        if (op == CSG_UNION) {
            result.distance = opUnion(result.distance, childDist);
        }
        else if (op == CSG_SUBTRACTION) {
            result.distance = opSubtraction(childDist, result.distance);
        }
        else if (op == CSG_INTERSECTION) {
            result.distance = opIntersection(result.distance, childDist);
        }
        else if (op == CSG_SMOOTH_UNION) {
            result.distance = opSmoothUnion(result.distance, childDist, smoothness);
        }
        else if (op == CSG_SMOOTH_SUBTRACTION) {
            result.distance = opSmoothSubtraction(childDist, result.distance, smoothness);
        }
        else if (op == CSG_SMOOTH_INTERSECTION) {
            result.distance = opSmoothIntersection(result.distance, childDist, smoothness);
        }

        // Update material if this primitive is closer
        if (childDist < oldDist && result.distance < oldDist) {
            result.primitiveIndex = i;
            result.baseColor = primitives[i].baseColor;
            result.material = primitives[i].material;
            result.emissiveColor = primitives[i].emissiveColor;
        }
    }

    return result;
}

// ===========================================================================
// Raymarching
// ===========================================================================

vec3 calculateNormal(vec3 p) {
    const float eps = 0.001;
    vec3 n;
    n.x = evaluateScene(p + vec3(eps, 0, 0)).distance - evaluateScene(p - vec3(eps, 0, 0)).distance;
    n.y = evaluateScene(p + vec3(0, eps, 0)).distance - evaluateScene(p - vec3(0, eps, 0)).distance;
    n.z = evaluateScene(p + vec3(0, 0, eps)).distance - evaluateScene(p - vec3(0, 0, eps)).distance;
    return normalize(n);
}

float calculateShadow(vec3 origin, vec3 direction, float minT, float maxT) {
    float res = 1.0;
    float t = minT;

    for (int i = 0; i < u_shadowSteps; i++) {
        float h = evaluateScene(origin + direction * t).distance;
        if (h < u_hitThreshold) {
            return 0.0;
        }
        res = min(res, u_shadowSoftness * h / t);
        t += h;
        if (t >= maxT) break;
    }

    return clamp(res, 0.0, 1.0);
}

float calculateAO(vec3 p, vec3 n) {
    float occ = 0.0;
    float sca = 1.0;

    for (int i = 0; i < u_aoSteps; i++) {
        float h = 0.01 + u_aoDistance * float(i) / float(u_aoSteps - 1);
        float d = evaluateScene(p + h * n).distance;
        occ += (h - d) * sca;
        sca *= 0.95;
    }

    return clamp(1.0 - u_aoIntensity * occ, 0.0, 1.0);
}

struct RaymarchResult {
    bool hit;
    vec3 position;
    vec3 normal;
    vec4 baseColor;
    vec4 material;  // metallic, roughness, emissive, unused
    vec4 emissiveColor;
    float distance;
};

RaymarchResult raymarch(vec3 origin, vec3 direction) {
    RaymarchResult result;
    result.hit = false;
    result.distance = 0.0;

    float t = 0.0;

    for (int i = 0; i < u_maxSteps; i++) {
        vec3 p = origin + direction * t;
        SDFResult sdf = evaluateScene(p);

        if (sdf.distance < u_hitThreshold) {
            result.hit = true;
            result.position = p;
            result.normal = calculateNormal(p);
            result.baseColor = sdf.baseColor;
            result.material = sdf.material;
            result.emissiveColor = sdf.emissiveColor;
            result.distance = t;
            break;
        }

        t += sdf.distance;

        if (t >= u_maxDistance) {
            break;
        }
    }

    return result;
}

// ===========================================================================
// Lighting
// ===========================================================================

vec3 calculateLighting(vec3 viewDir, vec3 normal, vec4 baseColor, vec4 material, vec3 worldPos) {
    vec3 lightDir = normalize(-u_lightDirection);

    // Metallic and roughness
    float metallic = material.x;
    float roughness = material.y;

    // Diffuse
    float diff = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = diff * baseColor.rgb * u_lightColor * u_lightIntensity;

    // Specular (simplified Blinn-Phong)
    vec3 halfwayDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(normal, halfwayDir), 0.0), mix(256.0, 16.0, roughness));
    vec3 specular = spec * mix(vec3(0.04), baseColor.rgb, metallic) * u_lightColor * u_lightIntensity;

    // Shadows
    float shadow = 1.0;
    if (u_enableShadows) {
        shadow = calculateShadow(worldPos + normal * 0.01, lightDir, 0.01, u_maxDistance);
    }

    // Ambient occlusion
    float ao = 1.0;
    if (u_enableAO) {
        ao = calculateAO(worldPos, normal);
    }

    // Ambient
    vec3 ambient = baseColor.rgb * 0.15;

    return (ambient + (diffuse + specular) * shadow) * ao;
}

// ===========================================================================
// Main
// ===========================================================================

void main() {
    // Get ray from camera
    vec2 ndc = v_texCoord * 2.0 - 1.0;

    vec4 clipPos = vec4(ndc, -1.0, 1.0);
    vec4 viewPos = u_invProjection * clipPos;
    viewPos /= viewPos.w;

    vec3 rayDir = normalize((u_invView * vec4(viewPos.xyz, 0.0)).xyz);
    vec3 rayOrigin = u_cameraPos;

    // Raymarch
    RaymarchResult result = raymarch(rayOrigin, rayDir);

    vec3 finalColor;

    if (result.hit) {
        vec3 viewDir = normalize(rayOrigin - result.position);

        // Calculate lighting
        vec3 lighting = calculateLighting(viewDir, result.normal, result.baseColor, result.material, result.position);

        // Add emissive
        float emissiveStrength = result.material.z;
        lighting += result.emissiveColor.rgb * emissiveStrength;

        finalColor = lighting;
    } else {
        // Background
        if (u_useEnvironmentMap) {
            finalColor = texture(u_environmentMap, rayDir).rgb;
        } else {
            finalColor = u_backgroundColor;
        }
    }

    // Tone mapping (simple Reinhard)
    finalColor = finalColor / (finalColor + vec3(1.0));

    // Gamma correction
    finalColor = pow(finalColor, vec3(1.0 / 2.2));

    FragColor = vec4(finalColor, 1.0);
}
