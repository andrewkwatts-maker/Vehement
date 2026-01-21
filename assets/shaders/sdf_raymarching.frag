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

// BVH acceleration
uniform int u_bvhNodeCount;
uniform bool u_useBVH;

// SDF 3D texture cache (brick-map)
uniform sampler3D u_sdfCache;
uniform vec3 u_cacheBoundsMin;
uniform vec3 u_cacheBoundsMax;
uniform bool u_useCachedSDF;
uniform int u_cacheResolution;

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
    vec4 parameters4;     // onionThickness, shellMinY, shellMaxY, flags
    vec4 material;        // metallic, roughness, emissive, unused
    vec4 baseColor;
    vec4 emissiveColor;
    vec4 boundingSphere;  // xyz = world center, w = bounding radius (for early-out)
    int type;
    int csgOperation;
    int visible;
    int parentIndex;      // -1 for root, >= 0 for child
};

// Primitive flags (must match C++ enum SDFPrimitiveFlags)
#define SDF_FLAG_ONION          1
#define SDF_FLAG_SHELL_BOUNDED  2
#define SDF_FLAG_HOLLOW         4
#define SDF_FLAG_FBM_DETAIL     8

// SSBO for primitives
layout(std430, binding = 0) buffer PrimitivesBuffer {
    SDFPrimitive primitives[];
};

// BVH node structure for GPU traversal
struct BVHNode {
    vec4 boundsMin;       // xyz = AABB min, w = unused
    vec4 boundsMax;       // xyz = AABB max, w = unused
    int leftChild;        // Left child index (internal) or first primitive (leaf)
    int rightChild;       // Right child index
    int primitiveCount;   // 0 = internal node, >0 = leaf
    int padding;
};

// SSBO for BVH nodes
layout(std430, binding = 1) buffer BVHNodesBuffer {
    BVHNode bvhNodes[];
};

// SSBO for BVH primitive indices (reordered by BVH)
layout(std430, binding = 2) buffer BVHPrimitiveIndicesBuffer {
    int bvhPrimitiveIndices[];
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
// Onion Shell Operations (for clothing layers)
// ===========================================================================

// Basic onion shell - creates thin shell around surface
float opOnion(float sdf, float thickness) {
    return abs(sdf) - thickness;
}

// Bounded onion shell - only applies within Y range (for open-bottom robes)
float opOnionBounded(float sdf, float thickness, float minY, float maxY, vec3 localPos) {
    // Only create shell within Y bounds
    if (localPos.y < minY || localPos.y > maxY) {
        return sdf; // Return original SDF outside bounds
    }

    // Smooth transition at boundaries to avoid hard edges
    float edgeSoftness = thickness * 2.0;
    float fadeBottom = smoothstep(minY, minY + edgeSoftness, localPos.y);
    float fadeTop = smoothstep(maxY, maxY - edgeSoftness, localPos.y);
    float fade = fadeBottom * fadeTop;

    float shell = abs(sdf) - thickness;
    return mix(sdf, shell, fade);
}

// Apply onion modifier based on primitive flags
float applyOnionModifier(float sdf, SDFPrimitive prim, vec3 localPos) {
    int flags = int(prim.parameters4.w);

    if ((flags & SDF_FLAG_ONION) == 0) {
        return sdf; // Onion not enabled
    }

    float thickness = prim.parameters4.x;
    if (thickness <= 0.0) {
        return sdf; // No thickness specified
    }

    if ((flags & SDF_FLAG_SHELL_BOUNDED) != 0) {
        // Bounded shell with Y cutoffs
        float minY = prim.parameters4.y;
        float maxY = prim.parameters4.z;
        return opOnionBounded(sdf, thickness, minY, maxY, localPos);
    } else {
        // Full onion shell
        return opOnion(sdf, thickness);
    }
}

// ===========================================================================
// FBM Surface Detail (Procedural Cloth/Skin Texture)
// ===========================================================================

// Hash function for noise
float hash(float n) {
    return fract(sin(n) * 43758.5453123);
}

// 3D value noise
float noise3D(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f); // Smoothstep interpolation

    float n = i.x + i.y * 157.0 + 113.0 * i.z;

    return mix(
        mix(mix(hash(n + 0.0), hash(n + 1.0), f.x),
            mix(hash(n + 157.0), hash(n + 158.0), f.x), f.y),
        mix(mix(hash(n + 113.0), hash(n + 114.0), f.x),
            mix(hash(n + 270.0), hash(n + 271.0), f.x), f.y),
        f.z);
}

// Fractal Brownian Motion with 4 octaves
float fbm(vec3 p, int octaves) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int i = 0; i < octaves; i++) {
        value += amplitude * noise3D(p * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
    }

    return value;
}

// Smooth maximum for safe SDF blending
float smax(float a, float b, float k) {
    float h = clamp(0.5 + 0.5 * (b - a) / k, 0.0, 1.0);
    return mix(a, b, h) + k * h * (1.0 - h);
}

// Add procedural surface detail without breaking SDF properties
// amplitude: height of detail bumps
// frequency: scale of noise pattern
float addSurfaceDetail(float baseSDF, vec3 p, float amplitude, float frequency) {
    if (amplitude <= 0.0) {
        return baseSDF;
    }

    // Only add detail near the surface (conservative approach)
    // This preserves SDF validity by avoiding interior changes
    float surfaceProximity = 1.0 - smoothstep(0.0, amplitude * 4.0, abs(baseSDF));

    // Generate FBM noise centered around 0.5, then shift to [-0.5, 0.5]
    float detail = (fbm(p * frequency, 4) - 0.5) * amplitude * surfaceProximity;

    // Use smooth max to ensure the result is still a valid SDF
    // This prevents the detail from making the SDF non-Euclidean
    return smax(baseSDF - amplitude, baseSDF + detail, amplitude * 0.5);
}

// Apply FBM detail modifier based on primitive flags
float applyFBMDetail(float sdf, SDFPrimitive prim, vec3 localPos) {
    int flags = int(prim.parameters4.w);

    if ((flags & SDF_FLAG_FBM_DETAIL) == 0) {
        return sdf; // FBM detail not enabled
    }

    // Use material roughness to control detail amplitude (rougher = more detail)
    // Frequency is fixed at a reasonable value for character-scale objects
    float roughness = prim.material.y;
    float amplitude = roughness * 0.01;  // Max 1% of unit size
    float frequency = 20.0;  // Detail frequency

    return addSurfaceDetail(sdf, localPos, amplitude, frequency);
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

    // Apply onion shell modifier if enabled (for clothing layers)
    dist = applyOnionModifier(dist, prim, localPos);

    // Apply FBM surface detail if enabled (for cloth/skin texture)
    dist = applyFBMDetail(dist, prim, localPos);

    return dist;
}

struct SDFResult {
    float distance;
    int primitiveIndex;
    vec4 baseColor;
    vec4 material;
    vec4 emissiveColor;
};

// Bounding sphere distance (for early-out culling)
// Returns distance to sphere surface, negative if inside
float sdBoundingSphere(vec3 p, vec4 sphere) {
    return length(p - sphere.xyz) - sphere.w;
}

// AABB distance for BVH node culling
// Returns distance to AABB surface, negative if inside
float sdAABB(vec3 p, vec3 boundsMin, vec3 boundsMax) {
    vec3 center = (boundsMin + boundsMax) * 0.5;
    vec3 halfSize = (boundsMax - boundsMin) * 0.5;
    vec3 d = abs(p - center) - halfSize;
    return length(max(d, 0.0)) + min(max(d.x, max(d.y, d.z)), 0.0);
}

// Apply CSG operation between child and parent distances
float applyCSG(int op, float parentDist, float childDist, float smoothness) {
    if (op == CSG_UNION) {
        return opUnion(parentDist, childDist);
    }
    else if (op == CSG_SUBTRACTION) {
        return opSubtraction(childDist, parentDist);
    }
    else if (op == CSG_INTERSECTION) {
        return opIntersection(parentDist, childDist);
    }
    else if (op == CSG_SMOOTH_UNION) {
        return opSmoothUnion(parentDist, childDist, smoothness);
    }
    else if (op == CSG_SMOOTH_SUBTRACTION) {
        return opSmoothSubtraction(childDist, parentDist, smoothness);
    }
    else if (op == CSG_SMOOTH_INTERSECTION) {
        return opSmoothIntersection(parentDist, childDist, smoothness);
    }
    return parentDist;
}

// ===========================================================================
// Cached SDF Sampling (3D Texture Brick-Map)
// ===========================================================================

// Sample cached SDF distance from 3D texture
float sampleCachedSDF(vec3 worldPos) {
    // Convert world position to texture UVW coordinates
    vec3 uvw = (worldPos - u_cacheBoundsMin) / (u_cacheBoundsMax - u_cacheBoundsMin);

    // Check if outside cache bounds
    if (any(lessThan(uvw, vec3(0.0))) || any(greaterThan(uvw, vec3(1.0)))) {
        return 1e10; // Outside cache bounds, return large distance
    }

    // Sample with trilinear interpolation (built into texture hardware)
    return texture(u_sdfCache, uvw).r;
}

// Evaluate scene using cached SDF texture (fast path for complex models)
SDFResult evaluateSceneCached(vec3 p) {
    SDFResult result;
    result.primitiveIndex = 0;  // Cache doesn't track individual primitives
    result.baseColor = vec4(0.8, 0.8, 0.8, 1.0);  // Default color
    result.material = vec4(0.0, 0.5, 0.0, 0.0);   // Default material
    result.emissiveColor = vec4(0.0);

    result.distance = sampleCachedSDF(p);

    // Get material from nearest primitive (if we have any)
    // For cached SDF, we use the first visible primitive's material as default
    if (u_primitiveCount > 0) {
        for (int i = 0; i < u_primitiveCount; i++) {
            if (primitives[i].visible != 0) {
                result.baseColor = primitives[i].baseColor;
                result.material = primitives[i].material;
                result.emissiveColor = primitives[i].emissiveColor;
                break;
            }
        }
    }

    return result;
}

// ===========================================================================
// BVH-Accelerated Scene Evaluation
// ===========================================================================

// Evaluate scene using BVH acceleration structure
// Uses stack-based traversal with AABB distance culling
SDFResult evaluateSceneBVH(vec3 p) {
    SDFResult result;
    result.distance = 1e10;
    result.primitiveIndex = -1;
    result.baseColor = vec4(1.0);
    result.material = vec4(0.0, 0.5, 0.0, 0.0);
    result.emissiveColor = vec4(0.0);

    if (u_bvhNodeCount == 0 || u_primitiveCount == 0) {
        return result;
    }

    float closestDist = 1e10;
    int closestIdx = -1;

    // Stack-based BVH traversal (max depth 24 to avoid stack overflow)
    int stack[24];
    int stackPtr = 0;
    stack[stackPtr++] = 0;  // Push root node

    while (stackPtr > 0 && stackPtr < 24) {
        int nodeIdx = stack[--stackPtr];
        BVHNode node = bvhNodes[nodeIdx];

        // AABB distance check for early culling
        float boxDist = sdAABB(p, node.boundsMin.xyz, node.boundsMax.xyz);

        // Skip this subtree if AABB is farther than current best distance
        if (boxDist > result.distance + 0.01) {
            continue;
        }

        if (node.primitiveCount > 0) {
            // Leaf node - evaluate primitives
            int firstPrim = node.leftChild;
            for (int i = 0; i < node.primitiveCount; i++) {
                int primIdx = bvhPrimitiveIndices[firstPrim + i];
                if (primIdx < 0 || primIdx >= u_primitiveCount) continue;
                if (primitives[primIdx].visible == 0) continue;

                float dist = evaluatePrimitive(primitives[primIdx], p);
                float smoothness = primitives[primIdx].parameters3.z;

                // Apply CSG operation
                int parentIdx = primitives[primIdx].parentIndex;
                int op = primitives[primIdx].csgOperation;

                if (parentIdx == -1) {
                    // Root primitive - union
                    result.distance = opUnion(result.distance, dist);
                } else {
                    // Child primitive - apply CSG
                    result.distance = applyCSG(op, result.distance, dist, smoothness);
                }

                // Track closest for material
                if (dist < closestDist) {
                    closestDist = dist;
                    closestIdx = primIdx;
                }
            }
        } else {
            // Internal node - push children (near child last so it's popped first)
            // Calculate distances to both children for front-to-back ordering
            float leftDist = sdAABB(p, bvhNodes[node.leftChild].boundsMin.xyz,
                                        bvhNodes[node.leftChild].boundsMax.xyz);
            float rightDist = sdAABB(p, bvhNodes[node.rightChild].boundsMin.xyz,
                                         bvhNodes[node.rightChild].boundsMax.xyz);

            // Push far child first (will be processed later)
            if (leftDist < rightDist) {
                if (rightDist <= result.distance + 0.01 && stackPtr < 23) {
                    stack[stackPtr++] = node.rightChild;
                }
                if (leftDist <= result.distance + 0.01 && stackPtr < 23) {
                    stack[stackPtr++] = node.leftChild;
                }
            } else {
                if (leftDist <= result.distance + 0.01 && stackPtr < 23) {
                    stack[stackPtr++] = node.leftChild;
                }
                if (rightDist <= result.distance + 0.01 && stackPtr < 23) {
                    stack[stackPtr++] = node.rightChild;
                }
            }
        }
    }

    // Set material from closest primitive
    if (closestIdx >= 0) {
        result.primitiveIndex = closestIdx;
        result.baseColor = primitives[closestIdx].baseColor;
        result.material = primitives[closestIdx].material;
        result.emissiveColor = primitives[closestIdx].emissiveColor;
    }

    return result;
}

// ===========================================================================
// Standard Scene Evaluation (fallback when BVH disabled)
// ===========================================================================

SDFResult evaluateScene(vec3 p) {
    SDFResult result;
    result.distance = 1e10;
    result.primitiveIndex = -1;
    result.baseColor = vec4(1.0);
    result.material = vec4(0.0, 0.5, 0.0, 0.0);
    result.emissiveColor = vec4(0.0);

    // Use cached SDF if enabled (fastest path for complex models)
    if (u_useCachedSDF && u_cacheResolution > 0) {
        return evaluateSceneCached(p);
    }

    if (u_primitiveCount == 0) {
        return result;
    }

    // Use BVH acceleration if enabled and available
    if (u_useBVH && u_bvhNodeCount > 0) {
        return evaluateSceneBVH(p);
    }

    // Streaming evaluation with bounding sphere early-out optimization
    // This approach evaluates primitives on-the-fly and accumulates the result
    float closestDist = 1e10;
    int closestIdx = -1;

    for (int i = 0; i < u_primitiveCount; i++) {
        if (primitives[i].visible == 0) continue;

        // Early-out using bounding sphere
        // Skip expensive SDF evaluation if we're far outside the bounding sphere
        float boundDist = sdBoundingSphere(p, primitives[i].boundingSphere);

        // For root primitives (union), skip if bounding sphere is farther than current best
        // For child primitives (CSG ops), we still need to evaluate if close to surface
        bool isRoot = (primitives[i].parentIndex == -1);
        int op = primitives[i].csgOperation;

        // Skip this primitive if:
        // - It's a root primitive AND bounding sphere is farther than current result
        // - OR it's a subtraction/intersection AND we're far outside the bounding sphere
        //   AND far outside the current surface (subtraction can only affect near surface)
        if (isRoot && boundDist > result.distance + 0.01) {
            continue;  // Can't contribute to union - too far away
        }
        if (!isRoot && (op == CSG_SUBTRACTION || op == CSG_SMOOTH_SUBTRACTION ||
                        op == CSG_INTERSECTION || op == CSG_SMOOTH_INTERSECTION)) {
            // For subtraction: only affects region where parent exists
            // For intersection: only affects region where both exist
            // If we're far outside the bounding sphere, skip
            if (boundDist > result.distance + 1.0) {
                continue;
            }
        }

        // Full SDF evaluation
        float dist = evaluatePrimitive(primitives[i], p);
        float smoothness = primitives[i].parameters3.z;

        // Apply CSG operation based on parentIndex
        // For root primitives (parentIndex == -1), use simple union
        // For child primitives, apply the specified operation
        if (isRoot) {
            // Root primitive - union with accumulated result
            result.distance = opUnion(result.distance, dist);
        } else {
            // Child primitive - apply CSG operation to accumulated result
            result.distance = applyCSG(op, result.distance, dist, smoothness);
        }

        // Track closest surface for material
        if (dist < closestDist) {
            closestDist = dist;
            closestIdx = i;
        }
    }

    // Set material from closest primitive
    if (closestIdx >= 0) {
        result.primitiveIndex = closestIdx;
        result.baseColor = primitives[closestIdx].baseColor;
        result.material = primitives[closestIdx].material;
        result.emissiveColor = primitives[closestIdx].emissiveColor;
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
    float alpha = 1.0;

    if (result.hit) {
        vec3 viewDir = normalize(rayOrigin - result.position);

        // Calculate lighting
        vec3 lighting = calculateLighting(viewDir, result.normal, result.baseColor, result.material, result.position);

        // Add emissive
        float emissiveStrength = result.material.z;
        lighting += result.emissiveColor.rgb * emissiveStrength;

        finalColor = lighting;
        alpha = 1.0;
    } else {
        // Background - transparent for icon rendering
        if (u_useEnvironmentMap) {
            finalColor = texture(u_environmentMap, rayDir).rgb;
            alpha = 1.0;
        } else {
            finalColor = u_backgroundColor;
            alpha = 0.0;  // Transparent background
        }
    }

    // Tone mapping (simple Reinhard)
    finalColor = finalColor / (finalColor + vec3(1.0));

    // Gamma correction
    finalColor = pow(finalColor, vec3(1.0 / 2.2));

    // Output alpha: 1.0 for hit, 0.0 for miss (allows transparent background)
    FragColor = vec4(finalColor, alpha);
}
