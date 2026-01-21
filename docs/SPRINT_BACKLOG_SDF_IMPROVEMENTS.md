# Sprint Backlog: SDF Rendering Improvements

**Sprint Goal:** Implement advanced SDF features for AAA-quality character rendering

**Sprint Duration:** 2 weeks
**Team Velocity:** 40 story points

---

## Epic 1: Onion Layers for Clothing Shells

**Story Points:** 13
**Priority:** P0 (Critical)
**Dependencies:** None

### User Story
> As an artist, I want clothing to render as thin shells around body parts, so that robes and armor have proper thickness without fully encapsulating the character.

### Acceptance Criteria
- [ ] Onion operation available in shader with configurable thickness
- [ ] Shell primitives support hole/cutout regions (not fully enclosed)
- [ ] Clothing layers properly blend with body underneath
- [ ] Visible layering effect in renders

### Work Packages

#### WP-1.1: Shader Onion Operation (5 SP)
**File:** `assets/shaders/sdf_raymarching.frag`

```glsl
// Add onion shell operation
float opOnion(float sdf, float thickness) {
    return abs(sdf) - thickness;
}

// Add bounded onion (with cutoff)
float opOnionBounded(float sdf, float thickness, float minY, float maxY, vec3 p) {
    float shell = abs(sdf) - thickness;
    // Cut off shell outside Y bounds (e.g., open bottom for robes)
    if (p.y < minY || p.y > maxY) {
        return sdf; // Return original, not shell
    }
    return shell;
}
```

**Tasks:**
1. Add `opOnion()` function to shader
2. Add `opOnionBounded()` for partial shells
3. Add `ONION_THICKNESS` parameter to SDFPrimitive struct
4. Update `evaluatePrimitive()` to apply onion when flag set
5. Add shell cutoff bounds (minY, maxY) to parameters

#### WP-1.2: Primitive Flag System (3 SP)
**File:** `engine/sdf/SDFPrimitive.hpp`

```cpp
// Add to SDFParameters
struct SDFParameters {
    // ... existing fields ...
    float onionThickness = 0.0f;  // 0 = disabled, >0 = shell thickness
    float shellMinY = -FLT_MAX;   // Lower Y cutoff
    float shellMaxY = FLT_MAX;    // Upper Y cutoff
    uint32_t flags = 0;           // Bit flags for options
};

enum SDFPrimitiveFlags {
    SDF_FLAG_ONION = 1 << 0,      // Enable onion shell
    SDF_FLAG_SHELL_BOUNDED = 1 << 1, // Enable Y-axis bounds
    SDF_FLAG_HOLLOW = 1 << 2,     // Hollow interior
};
```

**Tasks:**
1. Add `onionThickness` to SDFParameters
2. Add shell bounds (minY, maxY)
3. Add flags bitfield
4. Update GPU data upload in SDFRenderer

#### WP-1.3: JSON Support & Python Integration (3 SP)
**Files:** `tools/asset_media_renderer.cpp`, `tools/aaa_sdf_generator.py`

```json
{
    "id": "robe_outer",
    "type": "Ellipsoid",
    "params": {
        "radii": [0.2, 0.3, 0.15],
        "onionThickness": 0.02,
        "shellMinY": 0.5,
        "shellMaxY": 999
    }
}
```

**Tasks:**
1. Parse `onionThickness` in asset_media_renderer.cpp
2. Parse shell bounds
3. Add onion layer guidance to Gemini prompts
4. Add validation for onion parameters

#### WP-1.4: Testing & Validation (2 SP)
**Tasks:**
1. Create test model with onion clothing
2. Render and verify shell appearance
3. Test bounded shells (open bottom robes)
4. Performance benchmark

---

## Epic 2: Hierarchical BVH for GPU

**Story Points:** 13
**Priority:** P1 (High)
**Dependencies:** None

### User Story
> As a developer, I want the BVH acceleration structure to be used during GPU raymarching, so that 40+ primitive characters render efficiently.

### Acceptance Criteria
- [ ] BVH nodes uploaded to GPU SSBO
- [ ] Shader traverses BVH during scene evaluation
- [ ] 2x+ performance improvement for 40+ primitives
- [ ] Correct visual output maintained

### Work Packages

#### WP-2.1: GPU BVH Data Upload (5 SP)
**File:** `engine/graphics/SDFRenderer.cpp`

```cpp
void SDFRenderer::UploadBVH() {
    // Build BVH from current primitives
    m_bvh.Build(m_primitives);

    // Convert to GPU format
    std::vector<SDFBVHNodeGPU> gpuNodes;
    for (const auto& node : m_bvh.GetNodes()) {
        SDFBVHNodeGPU gpu;
        gpu.boundsMin = vec4(node.bounds.GetMin(), 0);
        gpu.boundsMax = vec4(node.bounds.GetMax(), 0);
        gpu.leftFirst = node.leftFirst;
        gpu.primitiveCount = node.primitiveCount;
        gpuNodes.push_back(gpu);
    }

    // Upload to SSBO
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_bvhSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 gpuNodes.size() * sizeof(SDFBVHNodeGPU),
                 gpuNodes.data(), GL_DYNAMIC_DRAW);
}
```

**Tasks:**
1. Create `SDFBVHNodeGPU` struct with std430 layout
2. Implement `UploadBVH()` method
3. Create BVH SSBO buffer
4. Bind BVH SSBO to shader
5. Upload primitive indices reordered by BVH

#### WP-2.2: Shader BVH Traversal (5 SP)
**File:** `assets/shaders/sdf_raymarching.frag`

```glsl
layout(std430, binding = 1) buffer BVHNodes {
    SDFBVHNodeGPU nodes[];
};

layout(std430, binding = 2) buffer BVHIndices {
    uint primitiveIndices[];
};

float evaluateSceneBVH(vec3 p) {
    float minDist = MAX_DIST;

    // Stack-based traversal
    int stack[32];
    int stackPtr = 0;
    stack[stackPtr++] = 0; // Root node

    while (stackPtr > 0) {
        int nodeIdx = stack[--stackPtr];
        SDFBVHNodeGPU node = nodes[nodeIdx];

        // AABB distance check
        float boxDist = sdBox(p - (node.boundsMin.xyz + node.boundsMax.xyz) * 0.5,
                              (node.boundsMax.xyz - node.boundsMin.xyz) * 0.5);

        if (boxDist > minDist) continue; // Cull

        if (node.primitiveCount > 0) {
            // Leaf node - evaluate primitives
            for (uint i = 0; i < node.primitiveCount; i++) {
                uint primIdx = primitiveIndices[node.leftFirst + i];
                float d = evaluatePrimitive(primitives[primIdx], p);
                minDist = min(minDist, d);
            }
        } else {
            // Internal node - push children
            stack[stackPtr++] = int(node.leftFirst);
            stack[stackPtr++] = int(node.leftFirst) + 1;
        }
    }

    return minDist;
}
```

**Tasks:**
1. Add BVH node SSBO binding
2. Add primitive indices SSBO binding
3. Implement stack-based traversal
4. Add AABB distance culling
5. Replace `evaluateScene()` with BVH version when enabled

#### WP-2.3: Performance Benchmarking (3 SP)
**Tasks:**
1. Create benchmark with 50, 100, 200 primitives
2. Measure frame time before/after BVH
3. Profile GPU occupancy
4. Document performance gains

---

## Epic 3: Brick-Map Caching

**Story Points:** 8
**Priority:** P2 (Medium)
**Dependencies:** Epic 2 (BVH)

### User Story
> As a developer, I want complex characters (100+ primitives) to use cached SDF values, so that rendering remains interactive.

### Acceptance Criteria
- [ ] 3D texture stores cached SDF distances
- [ ] Cache built on model load/change
- [ ] Trilinear interpolation in shader
- [ ] Fallback to direct evaluation for animated regions

### Work Packages

#### WP-3.1: 3D Texture Cache Generation (5 SP)
**File:** `engine/graphics/SDFCache.hpp` (new)

```cpp
class SDFCache {
public:
    void Build(const SDFModel& model, int resolution = 64);
    GLuint GetTexture() const { return m_texture; }
    glm::vec3 GetBoundsMin() const { return m_boundsMin; }
    glm::vec3 GetBoundsMax() const { return m_boundsMax; }

private:
    GLuint m_texture = 0;
    int m_resolution = 64;
    glm::vec3 m_boundsMin, m_boundsMax;
};

void SDFCache::Build(const SDFModel& model, int resolution) {
    m_resolution = resolution;
    m_boundsMin = model.GetBounds().GetMin() - glm::vec3(0.1f);
    m_boundsMax = model.GetBounds().GetMax() + glm::vec3(0.1f);

    std::vector<float> sdfData(resolution * resolution * resolution);

    // Compute SDF at each voxel
    for (int z = 0; z < resolution; z++) {
        for (int y = 0; y < resolution; y++) {
            for (int x = 0; x < resolution; x++) {
                glm::vec3 worldPos = voxelToWorld(x, y, z);
                sdfData[index(x, y, z)] = model.Evaluate(worldPos);
            }
        }
    }

    // Upload to 3D texture
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_3D, m_texture);
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F,
                 resolution, resolution, resolution,
                 0, GL_RED, GL_FLOAT, sdfData.data());
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}
```

**Tasks:**
1. Create SDFCache class
2. Implement voxel grid evaluation
3. Upload to 3D texture
4. Add bounds padding for interpolation
5. Support incremental updates

#### WP-3.2: Shader Cache Sampling (3 SP)
**File:** `assets/shaders/sdf_raymarching.frag`

```glsl
uniform sampler3D u_sdfCache;
uniform vec3 u_cacheBoundsMin;
uniform vec3 u_cacheBoundsMax;
uniform bool u_useCachedSDF;

float sampleCachedSDF(vec3 p) {
    vec3 uv = (p - u_cacheBoundsMin) / (u_cacheBoundsMax - u_cacheBoundsMin);
    if (any(lessThan(uv, vec3(0))) || any(greaterThan(uv, vec3(1)))) {
        return MAX_DIST; // Outside cache bounds
    }
    return texture(u_sdfCache, uv).r;
}

float evaluateSceneWithCache(vec3 p) {
    if (u_useCachedSDF) {
        return sampleCachedSDF(p);
    }
    return evaluateSceneBVH(p);
}
```

**Tasks:**
1. Add 3D texture sampler uniform
2. Add bounds uniforms
3. Implement UV calculation
4. Add cache enable toggle
5. Blend cached + direct for animation

---

## Epic 4: FBM Surface Detail

**Story Points:** 5
**Priority:** P3 (Low)
**Dependencies:** None

### User Story
> As an artist, I want procedural surface detail on characters, so that cloth and skin have realistic texture without additional primitives.

### Acceptance Criteria
- [ ] FBM noise applied to SDF surface
- [ ] Configurable amplitude and frequency
- [ ] No raymarching artifacts
- [ ] Per-material detail settings

### Work Packages

#### WP-4.1: FBM Noise Functions (2 SP)
**File:** `assets/shaders/sdf_raymarching.frag`

```glsl
// Value noise for FBM
float noise3D(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f); // Smoothstep

    float n = i.x + i.y * 157.0 + 113.0 * i.z;
    return mix(
        mix(mix(hash(n), hash(n + 1.0), f.x),
            mix(hash(n + 157.0), hash(n + 158.0), f.x), f.y),
        mix(mix(hash(n + 113.0), hash(n + 114.0), f.x),
            mix(hash(n + 270.0), hash(n + 271.0), f.x), f.y), f.z);
}

// FBM with 4 octaves
float fbm(vec3 p) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int i = 0; i < 4; i++) {
        value += amplitude * noise3D(p * frequency);
        amplitude *= 0.5;
        frequency *= 2.0;
    }
    return value;
}
```

**Tasks:**
1. Add 3D value noise function
2. Add FBM function with configurable octaves
3. Add hash function for randomness

#### WP-4.2: Safe SDF Detail Addition (3 SP)
**File:** `assets/shaders/sdf_raymarching.frag`

```glsl
// Add surface detail without breaking SDF properties
float addSurfaceDetail(float baseSDF, vec3 p, float amplitude, float frequency) {
    if (amplitude <= 0.0) return baseSDF;

    // Only add detail near surface (conservative approach)
    float surfaceProximity = 1.0 - smoothstep(0.0, amplitude * 4.0, abs(baseSDF));

    float detail = (fbm(p * frequency) - 0.5) * amplitude * surfaceProximity;

    // Use smooth max to ensure valid SDF
    return smax(baseSDF - amplitude, baseSDF + detail, amplitude * 0.5);
}
```

**Tasks:**
1. Implement `addSurfaceDetail()` function
2. Add per-material amplitude/frequency uniforms
3. Apply only near surface (surfaceProximity)
4. Use smooth operations to maintain SDF validity
5. Add material flag for detail enable

---

## Sprint Capacity Planning

| Epic | Story Points | Priority | Risk |
|------|-------------|----------|------|
| Onion Layers | 13 | P0 | Low |
| Hierarchical BVH | 13 | P1 | Medium |
| Brick-Map Cache | 8 | P2 | Medium |
| FBM Detail | 5 | P3 | Low |
| **Total** | **39** | - | - |

## Definition of Done
- [ ] Code compiles without warnings
- [ ] Unit tests pass
- [ ] Visual output verified with test renders
- [ ] Performance benchmark meets targets
- [ ] Documentation updated
- [ ] Code reviewed and approved

## Risks & Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| BVH traversal divergence on GPU | High | Use sorted traversal, limit stack depth |
| Cache memory usage | Medium | Use sparse brick-map, configurable resolution |
| FBM breaking raymarching | Medium | Conservative amplitude, surface proximity check |
| Onion shells fully enclosing | Low | Add bounded onion with Y cutoffs |

---

## Gemini Technical Review Feedback (Incorporated)

### Issues Identified:
1. **Onion normal smoothing** - Sharp edges may appear; add normal averaging
2. **64^3 cache too low** - Use 128^3 or higher; trilinear causes blur

### Missing Items Added:
1. **Performance metrics** - Add baseline + improvement tracking
2. **Testing strategy** - Unit tests for each feature
3. **BVH sync strategy** - Handle CPU→GPU update cost
4. **Cache update strategy** - Define when/how to rebuild

### Risk Mitigations Updated:
1. **Stack overflow** - Use dynamic stack or limit to 24 levels
2. **Cache blur** - Use 128^3 minimum, add mip-mapping
3. **BVH upload cost** - Only rebuild on model change, not per-frame

### Priority Adjustments:
1. **Define metrics FIRST** - Before implementing
2. **Onion Layers** - Highest priority (low risk, high value)
3. **BVH** - Medium priority (only if not updating frequently)
4. **Cache** - Lower priority (memory concerns)
