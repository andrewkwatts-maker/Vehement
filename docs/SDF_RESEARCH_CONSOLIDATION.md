# SDF Research Consolidation for Nova3D/Vehement Engine

**Version:** 1.0
**Date:** January 2026
**Engine:** Nova3D (Old3DEngine/Vehement)

---

## Table of Contents

1. [SDF Fundamentals](#1-sdf-fundamentals)
   - [Definition and Properties](#11-definition-and-properties)
   - [Basic Primitives](#12-basic-primitives)
   - [CSG Operations](#13-csg-operations)
2. [Raymarching Techniques](#2-raymarching-techniques)
   - [Sphere Tracing Algorithm](#21-sphere-tracing-algorithm)
   - [Enhanced Sphere Tracing](#22-enhanced-sphere-tracing-overrelaxation)
   - [Relaxed Cone Tracing](#23-relaxed-cone-tracing)
   - [Brick Maps and Distance Field Textures](#24-brick-maps-and-distance-field-textures)
3. [Global Illumination with SDFs](#3-global-illumination-with-sdfs)
   - [Radiance Cascades](#31-radiance-cascades-alexander-sannikov)
   - [Ambient Occlusion from SDFs](#32-ambient-occlusion-from-sdfs)
   - [Soft Shadows via Cone Tracing](#33-soft-shadows-via-cone-tracing)
   - [Light Propagation Volumes](#34-light-propagation-volumes)
4. [Performance Optimization](#4-performance-optimization)
   - [Temporal Reprojection](#41-temporal-reprojection)
   - [Checkerboard Rendering](#42-checkerboard-rendering)
   - [LOD for SDF Primitives](#43-lod-for-sdf-primitives)
   - [GPU Bytecode Evaluation](#44-gpu-bytecode-evaluation)
5. [Key Papers and Resources](#5-key-papers-and-resources)

---

## 1. SDF Fundamentals

### 1.1 Definition and Properties

A **Signed Distance Field (SDF)** is a scalar field where each point in space stores the shortest distance to the nearest surface. The sign indicates whether the point is inside (negative) or outside (positive) the surface.

**Key Properties:**

| Property | Description |
|----------|-------------|
| **Lipschitz Continuity** | The gradient magnitude is bounded by 1: `|grad(SDF)| <= 1` |
| **Surface Representation** | The zero-level set `{x : SDF(x) = 0}` defines the surface |
| **Normal Calculation** | Surface normal = `normalize(gradient(SDF))` |
| **Boolean Operations** | CSG operations are trivial min/max operations |
| **Distance Queries** | Direct distance-to-surface queries without iteration |

**Mathematical Definition:**

```
SDF(p) = sign(p) * min(||p - s||) for all surface points s
where sign(p) = -1 if p is inside, +1 if outside
```

**GLSL Implementation - Distance Field Evaluation:**

```glsl
// Evaluate scene SDF at point p
float sceneSDF(vec3 p) {
    float d = MAX_DISTANCE;

    // Combine all primitives using CSG operations
    for (int i = 0; i < primitiveCount; i++) {
        float primDist = evaluatePrimitive(primitives[i], p);
        d = combineSDF(d, primDist, primitives[i].csgOp);
    }

    return d;
}

// Calculate surface normal using central differences
vec3 calcNormal(vec3 p) {
    const float eps = 0.001;
    vec3 n;
    n.x = sceneSDF(p + vec3(eps, 0, 0)) - sceneSDF(p - vec3(eps, 0, 0));
    n.y = sceneSDF(p + vec3(0, eps, 0)) - sceneSDF(p - vec3(0, eps, 0));
    n.z = sceneSDF(p + vec3(0, 0, eps)) - sceneSDF(p - vec3(0, 0, eps));
    return normalize(n);
}
```

---

### 1.2 Basic Primitives

The following SDF primitives form the foundation of procedural modeling. All functions assume the primitive is centered at origin with default orientation.

#### Sphere

The simplest SDF - distance to surface equals distance to center minus radius.

```glsl
float sdSphere(vec3 p, float radius) {
    return length(p) - radius;
}
```

**Properties:**
- Exact distance function
- O(1) evaluation cost
- No numerical precision issues

---

#### Box (AABB)

An axis-aligned box uses component-wise distance calculation.

```glsl
float sdBox(vec3 p, vec3 halfExtents) {
    vec3 q = abs(p) - halfExtents;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}
```

**Explanation:**
- First term: distance outside the box (corner/edge distance)
- Second term: distance inside the box (max penetration depth)

---

#### Rounded Box

Box with rounded edges - subtracts corner radius from box, then re-expands.

```glsl
float sdRoundedBox(vec3 p, vec3 halfExtents, float cornerRadius) {
    vec3 q = abs(p) - halfExtents + vec3(cornerRadius);
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - cornerRadius;
}
```

---

#### Capsule

A cylinder with hemispherical caps - ideal for character limbs.

```glsl
float sdCapsule(vec3 p, float height, float radius) {
    // Clamp to cylinder axis
    float halfH = height * 0.5 - radius;
    p.y -= clamp(p.y, -halfH, halfH);
    return length(p) - radius;
}

// Alternative: capsule from point A to point B
float sdCapsule(vec3 p, vec3 a, vec3 b, float radius) {
    vec3 pa = p - a;
    vec3 ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - radius;
}
```

---

#### Torus

Ring shape defined by major (center to tube) and minor (tube) radii.

```glsl
float sdTorus(vec3 p, float majorRadius, float minorRadius) {
    vec2 q = vec2(length(p.xz) - majorRadius, p.y);
    return length(q) - minorRadius;
}
```

---

#### Cylinder

Finite cylinder aligned to Y-axis.

```glsl
float sdCylinder(vec3 p, float height, float radius) {
    vec2 d = abs(vec2(length(p.xz), p.y)) - vec2(radius, height * 0.5);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}
```

---

#### Cone

Finite cone with apex at origin pointing down.

```glsl
float sdCone(vec3 p, float height, float radius) {
    // Normalize the cone angle
    vec2 c = normalize(vec2(radius, height));
    float q = length(p.xz);
    return dot(c, vec2(q, p.y + height * 0.5));
}

// More robust bounded cone
float sdBoundedCone(vec3 p, float h, float r1, float r2) {
    vec2 q = vec2(length(p.xz), p.y);
    vec2 k1 = vec2(r2, h);
    vec2 k2 = vec2(r2 - r1, 2.0 * h);
    vec2 ca = vec2(q.x - min(q.x, (q.y < 0.0) ? r1 : r2), abs(q.y) - h);
    vec2 cb = q - k1 + k2 * clamp(dot(k1 - q, k2) / dot(k2, k2), 0.0, 1.0);
    float s = (cb.x < 0.0 && ca.y < 0.0) ? -1.0 : 1.0;
    return s * sqrt(min(dot(ca, ca), dot(cb, cb)));
}
```

---

#### Ellipsoid

Non-uniform scaling of a sphere - note this is an approximation.

```glsl
float sdEllipsoid(vec3 p, vec3 radii) {
    float k0 = length(p / radii);
    float k1 = length(p / (radii * radii));
    return k0 * (k0 - 1.0) / k1;
}
```

**Warning:** This is not an exact SDF - underestimates distance near poles.

---

#### Plane

Infinite plane defined by normal and offset.

```glsl
float sdPlane(vec3 p, vec3 normal, float offset) {
    return dot(p, normal) + offset;
}
```

---

### 1.3 CSG Operations

Constructive Solid Geometry (CSG) combines primitives using Boolean operations.

#### Standard Operations

```glsl
// Union: combines two shapes (OR)
float opUnion(float d1, float d2) {
    return min(d1, d2);
}

// Subtraction: carves d1 from d2 (d2 AND NOT d1)
float opSubtraction(float d1, float d2) {
    return max(-d1, d2);
}

// Intersection: keeps only overlapping region (AND)
float opIntersection(float d1, float d2) {
    return max(d1, d2);
}
```

---

#### Smooth Operations (Organic Blending)

Polynomial smooth blending creates organic transitions between shapes.

```glsl
// Smooth Union - polynomial blending
// k controls blend radius (higher = smoother)
float opSmoothUnion(float d1, float d2, float k) {
    float h = clamp(0.5 + 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) - k * h * (1.0 - h);
}

// Smooth Subtraction
float opSmoothSubtraction(float d1, float d2, float k) {
    float h = clamp(0.5 - 0.5 * (d2 + d1) / k, 0.0, 1.0);
    return mix(d2, -d1, h) + k * h * (1.0 - h);
}

// Smooth Intersection
float opSmoothIntersection(float d1, float d2, float k) {
    float h = clamp(0.5 - 0.5 * (d2 - d1) / k, 0.0, 1.0);
    return mix(d2, d1, h) + k * h * (1.0 - h);
}
```

---

#### Advanced Smooth Variants

```glsl
// Exponential smooth union - very organic, flowing blends
float opExpSmoothUnion(float d1, float d2, float k) {
    if (k == 0.0) return min(d1, d2);
    float res = exp2(-k * d1) + exp2(-k * d2);
    return -log2(res) / k;
}

// Cubic smooth union - C2 continuous, smoother than polynomial
float opCubicSmoothUnion(float d1, float d2, float k) {
    if (k == 0.0) return min(d1, d2);
    float h = max(k - abs(d1 - d2), 0.0) / k;
    float m = h * h * h * 0.5;
    float s = m * k * (1.0 / 3.0);
    return (d1 < d2) ? d1 - s : d2 - s;
}

// Distance-aware smooth union
// Prevents unwanted blending when parts are far apart (animation-safe)
float opDistanceAwareSmoothUnion(float d1, float d2, float k, float minDist) {
    float dist = abs(d1 - d2);
    if (dist > minDist) {
        return min(d1, d2);  // Hard union when far apart
    }
    float falloff = 1.0 - (dist / minDist);
    return opSmoothUnion(d1, d2, k * falloff);
}
```

---

## 2. Raymarching Techniques

### 2.1 Sphere Tracing Algorithm

Sphere tracing (Hart, 1996) is the standard algorithm for rendering SDFs. The key insight: at any point, the SDF value gives a safe step distance without overshooting.

**Algorithm:**
1. Start at ray origin
2. Evaluate SDF at current position
3. Advance along ray by SDF value
4. Repeat until SDF < threshold (hit) or distance > max (miss)

```glsl
struct RayMarchResult {
    bool hit;
    vec3 position;
    float distance;
    int steps;
};

RayMarchResult sphereTrace(vec3 rayOrigin, vec3 rayDir,
                           float maxDist, int maxSteps, float hitThreshold) {
    RayMarchResult result;
    result.hit = false;
    result.steps = 0;

    float t = 0.0;

    for (int i = 0; i < maxSteps; i++) {
        result.steps = i;
        vec3 p = rayOrigin + rayDir * t;

        float d = sceneSDF(p);

        if (d < hitThreshold) {
            result.hit = true;
            result.position = p;
            result.distance = t;
            break;
        }

        t += d;  // Safe step - guaranteed not to overshoot

        if (t > maxDist) {
            break;
        }
    }

    return result;
}
```

**Convergence Properties:**
- Linear convergence for most surfaces
- Superlinear convergence near high-curvature regions
- May slow down in thin features or grazing angles

---

### 2.2 Enhanced Sphere Tracing (Overrelaxation)

Overrelaxation accelerates convergence by taking larger steps, with backtracking when overshooting is detected.

**Reference:** "Enhanced Sphere Tracing" (Keinert et al., 2014)

```glsl
RayMarchResult enhancedSphereTrace(vec3 ro, vec3 rd, float maxDist, int maxSteps) {
    RayMarchResult result;
    result.hit = false;

    float t = 0.0;
    float prevRadius = 0.0;
    float stepLength = 0.0;
    float omega = 1.6;  // Relaxation factor (1.0 = standard, >1.0 = overrelaxed)

    for (int i = 0; i < maxSteps; i++) {
        vec3 p = ro + rd * t;
        float radius = sceneSDF(p);

        // Check for hit
        if (radius < HIT_THRESHOLD) {
            result.hit = true;
            result.position = p;
            result.distance = t;
            break;
        }

        // Overrelaxation with backtracking
        bool sorFail = omega > 1.0 &&
                       (radius + prevRadius) < stepLength;

        if (sorFail) {
            // Backtrack and reduce omega
            stepLength -= omega * stepLength;
            omega = 1.0;
        } else {
            stepLength = omega * radius;
        }

        prevRadius = radius;
        t += stepLength;

        if (t > maxDist) break;
    }

    return result;
}
```

**Performance Improvement:** 20-40% fewer iterations for typical scenes.

---

### 2.3 Relaxed Cone Tracing

Cone tracing uses a cone instead of a ray, enabling single-pass soft shadows and ambient occlusion.

```glsl
// Soft shadow via cone tracing
// k controls shadow softness (higher = softer)
float softShadow(vec3 ro, vec3 rd, float mint, float maxt, float k) {
    float res = 1.0;
    float t = mint;
    float ph = 1e20;  // Previous hit distance

    for (int i = 0; i < 64; i++) {
        float h = sceneSDF(ro + rd * t);

        if (h < HIT_THRESHOLD) {
            return 0.0;  // Full shadow
        }

        // Improved penumbra estimation
        float y = h * h / (2.0 * ph);
        float d = sqrt(h * h - y * y);
        res = min(res, k * d / max(0.0, t - y));
        ph = h;

        t += h;

        if (t > maxt) break;
    }

    return clamp(res, 0.0, 1.0);
}
```

---

### 2.4 Brick Maps and Distance Field Textures

For complex static geometry, precomputed 3D distance field textures provide O(1) evaluation.

**Brick Map Architecture:**

```
Brick Map Structure:
+----------------+
| Sparse Octree  |  <- Spatial index
+----------------+
       |
       v
+------+------+------+
|Brick |Brick |Brick |  <- 8x8x8 voxel blocks
|  0   |  1   |  2   |
+------+------+------+
```

**GLSL Brick Map Sampling:**

```glsl
uniform sampler3D u_distanceFieldTexture;
uniform vec3 u_volumeMin;
uniform vec3 u_volumeMax;
uniform vec3 u_volumeResolution;

float sampleBrickMap(vec3 worldPos) {
    // Transform to texture coordinates
    vec3 uvw = (worldPos - u_volumeMin) / (u_volumeMax - u_volumeMin);

    // Bounds check
    if (any(lessThan(uvw, vec3(0.0))) || any(greaterThan(uvw, vec3(1.0)))) {
        return MAX_DISTANCE;
    }

    // Trilinear sample
    float dist = texture(u_distanceFieldTexture, uvw).r;

    // Convert from normalized to world units
    vec3 voxelSize = (u_volumeMax - u_volumeMin) / u_volumeResolution;
    return dist * max(voxelSize.x, max(voxelSize.y, voxelSize.z));
}
```

**Compression Techniques:**
- BC4 compression: 4:1 ratio with minimal quality loss
- Sparse storage: only store occupied regions
- Brick deduplication: hash similar blocks

---

## 3. Global Illumination with SDFs

### 3.1 Radiance Cascades (Alexander Sannikov)

Radiance Cascades is a real-time GI technique using multi-scale radiance caching. Each cascade level stores radiance at different spatial resolutions.

**Reference:** Sannikov, A. "Radiance Cascades" (2024)

**Architecture:**

```
Cascade Structure:
Level 0: 32x32x32, spacing = 1m    (fine detail)
Level 1: 32x32x32, spacing = 2m
Level 2: 32x32x32, spacing = 4m
Level 3: 32x32x32, spacing = 8m    (coarse, far)
```

**GLSL Implementation:**

```glsl
// Radiance Cascade uniforms
uniform bool u_enableGI;
uniform int u_numCascades;
uniform vec3 u_cascadeOrigin;
uniform float u_cascadeBaseSpacing;
uniform sampler3D u_cascadeTexture[4];

// Sample radiance from cascades with blending
vec3 sampleRadianceCascade(vec3 worldPos, vec3 normal) {
    if (!u_enableGI) return vec3(0.0);

    vec3 totalRadiance = vec3(0.0);
    float totalWeight = 0.0;
    vec3 localPos = worldPos - u_cascadeOrigin;

    for (int level = 0; level < u_numCascades; level++) {
        float spacing = u_cascadeBaseSpacing * pow(2.0, float(level));
        vec3 gridPos = localPos / spacing;
        vec3 uvw = gridPos * 0.5 + 0.5;  // [-1,1] -> [0,1]

        // Check bounds
        if (all(greaterThan(uvw, vec3(0.0))) && all(lessThan(uvw, vec3(1.0)))) {
            vec4 sample = texture(u_cascadeTexture[level], uvw);

            // sample.rgb = radiance, sample.a = validity/confidence
            float weight = sample.a;
            totalRadiance += sample.rgb * weight;
            totalWeight += weight;
        }
    }

    if (totalWeight > 0.0) {
        totalRadiance /= totalWeight;
    }

    // Hemisphere weighting based on normal
    float hemisphereWeight = max(0.0, dot(normal, vec3(0.0, 1.0, 0.0)) * 0.5 + 0.5);

    return totalRadiance * hemisphereWeight;
}
```

**Cascade Update (Compute Shader):**

```glsl
#version 460
layout(local_size_x = 8, local_size_y = 8, local_size_z = 8) in;

layout(rgba16f, binding = 0) uniform image3D u_cascadeTexture;
uniform vec3 u_cascadeOrigin;
uniform float u_cascadeSpacing;
uniform int u_raysPerProbe;

void main() {
    ivec3 probeCoord = ivec3(gl_GlobalInvocationID);
    vec3 probeWorldPos = u_cascadeOrigin + vec3(probeCoord) * u_cascadeSpacing;

    vec3 radiance = vec3(0.0);
    int validSamples = 0;

    // Trace rays in hemisphere
    for (int i = 0; i < u_raysPerProbe; i++) {
        vec3 rayDir = fibonacciSpherePoint(i, u_raysPerProbe);

        // Trace ray through scene
        RayHit hit = traceRay(probeWorldPos, rayDir);

        if (hit.valid) {
            // Sample direct lighting at hit point
            vec3 directLight = sampleDirectLight(hit.position, hit.normal);

            // Sample lower cascade for indirect
            vec3 indirectLight = sampleRadianceCascade(hit.position, hit.normal);

            radiance += directLight + indirectLight * hit.albedo;
            validSamples++;
        }
    }

    if (validSamples > 0) {
        radiance /= float(validSamples);
    }

    // Temporal blending
    vec4 prevValue = imageLoad(u_cascadeTexture, probeCoord);
    float blend = 0.95;  // 95% history retention
    vec4 newValue = vec4(mix(radiance, prevValue.rgb, blend),
                         float(validSamples) / float(u_raysPerProbe));

    imageStore(u_cascadeTexture, probeCoord, newValue);
}
```

---

### 3.2 Ambient Occlusion from SDFs

SDF-based AO is elegant: march along the normal and compare actual distance to expected distance.

```glsl
// Multi-sample ambient occlusion
float calcAmbientOcclusion(vec3 pos, vec3 normal, int samples, float maxDist, float intensity) {
    float occlusion = 0.0;
    float weight = 1.0;

    for (int i = 0; i < samples; i++) {
        float sampleDist = maxDist * float(i + 1) / float(samples);
        vec3 samplePos = pos + normal * sampleDist;

        float actualDist = sceneSDF(samplePos);

        // Occlusion = how much closer surface is than expected
        occlusion += (sampleDist - actualDist) * weight;

        weight *= 0.7;  // Exponential falloff for distant samples
    }

    return clamp(1.0 - intensity * occlusion, 0.0, 1.0);
}

// Fast 5-tap AO (production quality)
float fastAO(vec3 pos, vec3 normal) {
    float occ = 0.0;
    float sca = 1.0;

    for (int i = 0; i < 5; i++) {
        float h = 0.01 + 0.12 * float(i) / 4.0;
        float d = sceneSDF(pos + h * normal);
        occ += (h - d) * sca;
        sca *= 0.95;
    }

    return clamp(1.0 - 3.0 * occ, 0.0, 1.0);
}
```

---

### 3.3 Soft Shadows via Cone Tracing

Beyond the basic soft shadow function, here's an improved version with physically-based penumbra:

```glsl
// Physically-based soft shadow with light source size
float softShadowPBR(vec3 ro, vec3 lightPos, float lightRadius) {
    vec3 rd = normalize(lightPos - ro);
    float lightDist = length(lightPos - ro);

    float res = 1.0;
    float t = 0.02;  // Start slightly above surface
    float ph = 1e10;

    for (int i = 0; i < 128 && t < lightDist; i++) {
        vec3 p = ro + rd * t;
        float h = sceneSDF(p);

        if (h < 0.0001) {
            return 0.0;
        }

        // Penumbra based on light source angular size
        float angularSize = lightRadius / t;
        float y = h * h / (2.0 * ph);
        float d = sqrt(h * h - y * y);
        res = min(res, d / (angularSize * max(0.0, t - y)));

        ph = h;
        t += h;
    }

    return clamp(res, 0.0, 1.0);
}
```

---

### 3.4 Light Propagation Volumes

LPVs are a classic technique adapted for SDFs. The SDF enables fast geometry injection.

```glsl
// LPV cell structure
struct LPVCell {
    vec4 redSH;    // Spherical harmonics - red
    vec4 greenSH;  // Spherical harmonics - green
    vec4 blueSH;   // Spherical harmonics - blue
};

// Inject direct lighting into LPV
void injectLight(vec3 worldPos, vec3 normal, vec3 flux) {
    ivec3 cell = worldToLPVCell(worldPos);

    // Encode direction using SH
    vec4 shCoeffs = evalSH(normal);

    // Inject (atomic add in real implementation)
    lpvGrid[cell].redSH += shCoeffs * flux.r;
    lpvGrid[cell].greenSH += shCoeffs * flux.g;
    lpvGrid[cell].blueSH += shCoeffs * flux.b;
}

// Propagate light (single iteration)
void propagateLight() {
    // For each cell, gather from 6 neighbors
    for (int face = 0; face < 6; face++) {
        ivec3 neighborOffset = faceOffsets[face];
        LPVCell neighbor = lpvGrid[cell + neighborOffset];

        // Propagate based on SDF occlusion
        float occlusion = sceneSDF(cellCenter + vec3(neighborOffset) * cellSize);
        float visibility = smoothstep(0.0, cellSize, occlusion);

        // Transfer SH coefficients
        currentCell.redSH += neighbor.redSH * visibility * propagationWeight;
        // ... green, blue
    }
}
```

---

## 4. Performance Optimization

### 4.1 Temporal Reprojection

Reuse previous frame's results to reduce per-frame computation.

```glsl
// TAA-style temporal reprojection for GI
uniform sampler2D u_historyBuffer;
uniform mat4 u_prevViewProjection;
uniform mat4 u_invViewProjection;

vec3 temporalReproject(vec3 currentRadiance, vec2 uv, vec3 worldPos) {
    // Reproject to previous frame UV
    vec4 prevClip = u_prevViewProjection * vec4(worldPos, 1.0);
    vec2 prevUV = (prevClip.xy / prevClip.w) * 0.5 + 0.5;

    // Check if reprojection is valid
    if (any(lessThan(prevUV, vec2(0.0))) || any(greaterThan(prevUV, vec2(1.0)))) {
        return currentRadiance;  // No history available
    }

    vec3 historyRadiance = texture(u_historyBuffer, prevUV).rgb;

    // Neighborhood clamping to reduce ghosting
    vec3 nearMin = currentRadiance;
    vec3 nearMax = currentRadiance;

    for (int y = -1; y <= 1; y++) {
        for (int x = -1; x <= 1; x++) {
            vec3 neighbor = textureOffset(u_currentBuffer, uv, ivec2(x, y)).rgb;
            nearMin = min(nearMin, neighbor);
            nearMax = max(nearMax, neighbor);
        }
    }

    historyRadiance = clamp(historyRadiance, nearMin, nearMax);

    // Blend (higher = more history = more stable but more ghosting)
    float blendFactor = 0.9;
    return mix(currentRadiance, historyRadiance, blendFactor);
}
```

---

### 4.2 Checkerboard Rendering

Render half the pixels per frame, reconstruct the rest.

```glsl
// Checkerboard pattern
bool isActivePixel(ivec2 coord, int frameIndex) {
    return ((coord.x + coord.y + frameIndex) & 1) == 0;
}

// Reconstruction from neighbors
vec4 reconstructCheckerboard(ivec2 coord, int frameIndex) {
    if (isActivePixel(coord, frameIndex)) {
        return texelFetch(u_currentFrame, coord, 0);
    }

    // Average 4 diagonal neighbors from current + previous frame
    vec4 sum = vec4(0.0);

    // Current frame neighbors
    sum += texelFetch(u_currentFrame, coord + ivec2(-1, -1), 0);
    sum += texelFetch(u_currentFrame, coord + ivec2( 1, -1), 0);
    sum += texelFetch(u_currentFrame, coord + ivec2(-1,  1), 0);
    sum += texelFetch(u_currentFrame, coord + ivec2( 1,  1), 0);

    return sum * 0.25;
}
```

---

### 4.3 LOD for SDF Primitives

Simplify distant primitives to reduce evaluation cost.

```glsl
// LOD-aware SDF evaluation
float evaluatePrimitiveLOD(Primitive prim, vec3 p, float distToCamera) {
    // LOD thresholds
    const float LOD1_DIST = 50.0;
    const float LOD2_DIST = 100.0;

    if (distToCamera > LOD2_DIST) {
        // Bounding sphere only
        return length(p - prim.center) - prim.boundingRadius;
    }
    else if (distToCamera > LOD1_DIST) {
        // Simplified primitive (sphere/box approximation)
        return sdSphere(p - prim.center, prim.boundingRadius * 0.9);
    }
    else {
        // Full detail
        return evaluatePrimitiveFull(prim, p);
    }
}

// Hierarchical culling with BVH
float evaluateSceneBVH(vec3 p) {
    float minDist = MAX_DISTANCE;

    // Start at BVH root
    int nodeStack[32];
    int stackPtr = 0;
    nodeStack[stackPtr++] = 0;  // Root node

    while (stackPtr > 0) {
        int nodeIdx = nodeStack[--stackPtr];
        BVHNode node = bvhNodes[nodeIdx];

        // Test AABB
        float boxDist = sdBox(p - node.center, node.halfExtents);

        if (boxDist < minDist) {
            if (node.isLeaf) {
                // Evaluate primitives in leaf
                for (int i = node.primStart; i < node.primEnd; i++) {
                    float d = evaluatePrimitive(primitives[i], p);
                    minDist = min(minDist, d);
                }
            } else {
                // Push children
                nodeStack[stackPtr++] = node.leftChild;
                nodeStack[stackPtr++] = node.rightChild;
            }
        }
    }

    return minDist;
}
```

---

### 4.4 GPU Bytecode Evaluation

Compile SDF trees to GPU-friendly bytecode for dynamic scenes.

```glsl
// Bytecode opcodes
const int OP_SPHERE = 0;
const int OP_BOX = 1;
const int OP_UNION = 10;
const int OP_SUBTRACT = 11;
const int OP_SMOOTH_UNION = 12;
const int OP_TRANSFORM = 20;
const int OP_END = 255;

// Bytecode interpreter
float evaluateBytecode(vec3 p, buffer BytecodeBuffer bytecode) {
    float stack[16];
    int stackPtr = 0;
    int pc = 0;

    mat4 currentTransform = mat4(1.0);

    while (true) {
        int op = bytecode.data[pc++];

        if (op == OP_END) break;

        switch (op) {
            case OP_SPHERE: {
                float radius = uintBitsToFloat(bytecode.data[pc++]);
                vec3 localP = (currentTransform * vec4(p, 1.0)).xyz;
                stack[stackPtr++] = sdSphere(localP, radius);
                break;
            }

            case OP_BOX: {
                vec3 dims;
                dims.x = uintBitsToFloat(bytecode.data[pc++]);
                dims.y = uintBitsToFloat(bytecode.data[pc++]);
                dims.z = uintBitsToFloat(bytecode.data[pc++]);
                vec3 localP = (currentTransform * vec4(p, 1.0)).xyz;
                stack[stackPtr++] = sdBox(localP, dims);
                break;
            }

            case OP_UNION: {
                float b = stack[--stackPtr];
                float a = stack[--stackPtr];
                stack[stackPtr++] = min(a, b);
                break;
            }

            case OP_SMOOTH_UNION: {
                float k = uintBitsToFloat(bytecode.data[pc++]);
                float b = stack[--stackPtr];
                float a = stack[--stackPtr];
                stack[stackPtr++] = opSmoothUnion(a, b, k);
                break;
            }

            case OP_TRANSFORM: {
                // Read 4x4 matrix
                for (int i = 0; i < 16; i++) {
                    currentTransform[i/4][i%4] = uintBitsToFloat(bytecode.data[pc++]);
                }
                break;
            }
        }
    }

    return stack[0];
}
```

---

## 5. Key Papers and Resources

### Primary References

| Resource | Author | Description |
|----------|--------|-------------|
| [Distance Functions](https://iquilezles.org/articles/distfunctions/) | Inigo Quilez | Comprehensive SDF primitive library |
| [Raymarching Primitives](https://iquilezles.org/articles/raymarchingdf/) | Inigo Quilez | Raymarching techniques and optimizations |
| [Smooth Minimum](https://iquilezles.org/articles/smin/) | Inigo Quilez | Smooth blending operations |
| [Radiance Cascades](https://drive.google.com/file/d/1L6v1_7HY2X-LV3Ofb6oyTIxgEaP4LOI6/view) | Alexander Sannikov | Real-time GI using cascaded radiance caching |
| [Sphere Tracing](https://graphics.stanford.edu/courses/cs348b-20-spring-content/uploads/hart.pdf) | John C. Hart | Original sphere tracing paper (1996) |

### Denoising and Filtering

| Paper | Authors | Year | Description |
|-------|---------|------|-------------|
| ReSTIR | Bitterli et al. | 2020 | Reservoir-based spatiotemporal importance resampling |
| SVGF | Schied et al. | 2017 | Spatiotemporal variance-guided filtering |
| A-SVGF | Schied et al. | 2018 | Adaptive SVGF with gradient estimation |

### Acceleration Structures

| Paper | Authors | Year | Description |
|-------|---------|------|-------------|
| Enhanced Sphere Tracing | Keinert et al. | 2014 | Overrelaxation for faster convergence |
| Efficient Sparse Voxel Octrees | Laine & Karras | 2010 | GPU-friendly octree traversal |
| SAH BVH Construction | Wald | 2007 | Surface area heuristic for optimal BVH |

### Additional Resources

- **Shadertoy**: Live GLSL examples - [shadertoy.com](https://www.shadertoy.com)
- **The Book of Shaders**: SDF chapter - [thebookofshaders.com](https://thebookofshaders.com)
- **GPU Gems 2, Chapter 8**: Per-Pixel Displacement Mapping with Distance Functions
- **Real-Time Rendering, 4th Edition**: Chapter 13 - Global Illumination

---

## Nova3D Implementation Status

The Nova3D engine implements the following SDF features:

| Feature | Status | Location |
|---------|--------|----------|
| Basic Primitives | Complete | `engine/sdf/SDFPrimitive.cpp` |
| CSG Operations | Complete | `engine/sdf/SDFPrimitive.cpp` |
| Smooth Blending | Complete | `SDFEval` namespace |
| Sphere Tracing | Complete | `assets/shaders/sdf_raymarching.frag` |
| Radiance Cascades | Complete | `engine/graphics/RadianceCascade.hpp` |
| Soft Shadows | Complete | `softShadow()` in shaders |
| Ambient Occlusion | Complete | `calcAO()` in shaders |
| BVH Acceleration | Complete | `engine/graphics/SDFAcceleration.hpp` |
| Brick Maps | Complete | `engine/graphics/SDFBrickMap.hpp` |
| Sparse Octree | Complete | `engine/graphics/SDFSparseOctree.hpp` |
| Temporal Reprojection | Complete | TAA resolve pass |
| Spectral Rendering | Complete | `sdf_raymarching_gi.frag` |

---

## Appendix: Quick Reference

### SDF Primitive Function Signatures

```glsl
float sdSphere(vec3 p, float r);
float sdBox(vec3 p, vec3 halfExtents);
float sdRoundedBox(vec3 p, vec3 halfExtents, float cornerRadius);
float sdCapsule(vec3 p, float height, float radius);
float sdTorus(vec3 p, float majorR, float minorR);
float sdCylinder(vec3 p, float height, float radius);
float sdCone(vec3 p, float height, float radius);
float sdEllipsoid(vec3 p, vec3 radii);
float sdPlane(vec3 p, vec3 normal, float offset);
```

### CSG Operation Signatures

```glsl
float opUnion(float d1, float d2);
float opSubtraction(float d1, float d2);
float opIntersection(float d1, float d2);
float opSmoothUnion(float d1, float d2, float k);
float opSmoothSubtraction(float d1, float d2, float k);
float opSmoothIntersection(float d1, float d2, float k);
```

### Performance Guidelines

| Scene Complexity | Max Steps | Max Distance | Hit Threshold |
|-----------------|-----------|--------------|---------------|
| Simple (< 10 prims) | 64 | 100.0 | 0.001 |
| Medium (10-50 prims) | 128 | 100.0 | 0.001 |
| Complex (50-200 prims) | 256 | 200.0 | 0.0005 |
| Very Complex (200+) | 512 | 500.0 | 0.0001 |

---

**Document Version:** 1.0
**Last Updated:** January 2026
**Maintainer:** Nova3D Engine Team
