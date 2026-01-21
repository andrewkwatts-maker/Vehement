# SDF Pipeline Improvement Research

Based on analysis of Mike Turitzin's dynamic SDF engine video and Inigo Quilez's techniques.

## Sources
- Mike Turitzin: https://www.youtube.com/watch?v=il-TXbn5iMA
- Inigo Quilez: https://iquilezles.org/articles/
- DaydreamSoft SDF Animation: https://daydreamsoft.com/blog/sdf-based-character-animation-and-deformation-systems

---

## Priority 1: Immediate Improvements (High Impact, Low Effort)

### 1.1 Better Smooth Blending Parameters

**Current Issue**: Using fixed smoothness values for all connections.

**Improvement**: Context-aware smoothness based on body region:
```cpp
// In shader or primitive setup
float getSmoothness(int bodyRegion) {
    switch(bodyRegion) {
        case REGION_FINGERS: return 0.02;  // Tight blends
        case REGION_JOINTS:  return 0.05;  // Medium blends
        case REGION_TORSO:   return 0.08;  // Larger organic blends
        case REGION_CLOTHING: return 0.12; // Soft flowing blends
    }
}
```

### 1.2 Onion Layers for Clothing

**Current Issue**: Clothing primitives are separate shapes that can clip through body.

**Improvement**: Use onion operation to create cloth as offset surface:
```glsl
// Onion layer - creates hollow shell
float sdOnion(float sdf, float thickness) {
    return abs(sdf) - thickness;
}

// Usage: robe as 0.02 thickness shell around body SDF
float body = sdBody(p);
float robe = sdOnion(body, 0.02);
```

### 1.3 Elongation for Limbs

**Current Issue**: Limbs use multiple capsules chained together.

**Improvement**: Single elongated primitive for cleaner silhouette:
```glsl
// Elongate a sphere into a limb shape
vec3 opElongate(vec3 p, vec3 h) {
    vec3 q = p - clamp(p, -h, h);
    return q;
}

// Usage: elongate sphere to create arm
float arm = sdSphere(opElongate(p, vec3(0, 0.3, 0)), 0.06);
```

---

## Priority 2: Medium-Term Improvements (Medium Impact, Medium Effort)

### 2.1 Procedural Surface Detail (FBM)

Add subtle surface noise for organic textures without breaking SDF properties:

```glsl
// Safe FBM addition using smooth operations
float addDetail(float baseSDF, vec3 p, float amplitude) {
    float noise = fbm(p * 10.0) * amplitude;
    // Clamp noise contribution to surface vicinity
    float inflation = amplitude * 2.0;
    return smax(baseSDF - inflation, baseSDF + noise, 0.1);
}
```

**Application**:
- Cloth wrinkles: amplitude = 0.005
- Skin texture: amplitude = 0.002
- Armor detail: amplitude = 0.01

### 2.2 Hierarchical Bounding Volumes

Group primitives and pre-compute group bounds for early-out:

```cpp
struct PrimitiveGroup {
    glm::vec4 boundingSphere;  // xyz = center, w = radius
    int firstPrimitive;
    int primitiveCount;
};

// In shader: skip entire groups
if (distance(rayPos, group.boundingSphere.xyz) > group.boundingSphere.w + currentDist) {
    continue; // Skip all primitives in this group
}
```

### 2.3 Material-Based Brightness Validation

Ensure generated characters are visible:

```python
def validate_material_brightness(material):
    """Ensure materials aren't too dark to see."""
    color = material.get('baseColor', [0.5, 0.5, 0.5, 1.0])
    luminance = 0.299 * color[0] + 0.587 * color[1] + 0.114 * color[2]

    MIN_LUMINANCE = 0.1  # Minimum 10% brightness

    if luminance < MIN_LUMINANCE:
        # Boost brightness while preserving hue
        boost = MIN_LUMINANCE / max(luminance, 0.01)
        color[0] = min(color[0] * boost, 1.0)
        color[1] = min(color[1] * boost, 1.0)
        color[2] = min(color[2] * boost, 1.0)

    return color
```

---

## Priority 3: Long-Term Improvements (High Impact, High Effort)

### 3.1 Brick-Map Caching (from Mike Turitzin)

For complex characters (100+ primitives), cache SDF values in a 3D texture:

```cpp
// Build phase: compute SDF at grid points
void buildSDFCache(SDFModel& model, GLuint& texture3D, int resolution = 64) {
    std::vector<float> sdfData(resolution * resolution * resolution);

    for (int z = 0; z < resolution; z++) {
        for (int y = 0; y < resolution; y++) {
            for (int x = 0; x < resolution; x++) {
                vec3 worldPos = gridToWorld(x, y, z, model.bounds);
                sdfData[index(x,y,z)] = model.Evaluate(worldPos);
            }
        }
    }

    // Upload to 3D texture for trilinear interpolation
    glTexImage3D(GL_TEXTURE_3D, 0, GL_R32F, res, res, res, 0,
                 GL_RED, GL_FLOAT, sdfData.data());
}

// Render phase: sample cached values
float sampleCachedSDF(vec3 p) {
    vec3 uv = worldToUV(p, bounds);
    return texture(sdfCache, uv).r;
}
```

### 3.2 Geometry Clipmaps for LOD

Multi-resolution SDF for different viewing distances:

```cpp
struct SDFClipmap {
    GLuint textures[4];  // 4 LOD levels
    float cellSizes[4];  // 0.01, 0.02, 0.04, 0.08 meters

    float sample(vec3 p, float distance) {
        int lod = clamp(int(log2(distance)), 0, 3);
        return texture(textures[lod], worldToUV(p, lod)).r;
    }
};
```

### 3.3 Animation Deformation via SDF Morphing

Instead of re-evaluating all primitives each frame, morph cached SDFs:

```glsl
// Blend between two cached pose SDFs
float animatedSDF(vec3 p, float blendFactor) {
    float pose1 = texture(sdfPose1, worldToUV(p)).r;
    float pose2 = texture(sdfPose2, worldToUV(p)).r;
    return mix(pose1, pose2, blendFactor);
}
```

---

## Prompt Engineering Improvements

### Better Body Part Descriptions

Add anatomical guidance to Gemini prompts:

```
ANATOMICAL GUIDELINES:
- Head-to-body ratio: 1:7 for realistic, 1:5 for stylized
- Shoulder width: ~2.5 head widths
- Hip width: ~1.5 head widths for female, ~1.2 for male
- Arm length: fingertips at mid-thigh when relaxed
- Use ELONGATION for limbs (single stretched primitive > chain of capsules)
- Use ONION LAYERS for clothing (offset surfaces > separate primitives)
```

### Smooth Blending Instructions

```
BLENDING REQUIREMENTS:
- Joint connections: smoothness = 0.03-0.05
- Torso-limb connections: smoothness = 0.06-0.10
- Clothing-body: smoothness = 0.10-0.15
- ALWAYS use SmoothUnion for organic parts
- NEVER use hard Union for body parts
```

---

## Implementation Priority

| Improvement | Impact | Effort | Priority |
|-------------|--------|--------|----------|
| Onion layers for clothing | High | Low | 1 |
| Context-aware smoothness | Medium | Low | 2 |
| Material brightness validation | Medium | Low | 3 |
| Elongation operations | High | Medium | 4 |
| FBM surface detail | Medium | Medium | 5 |
| Hierarchical BVH | High | Medium | 6 |
| Brick-map caching | High | High | 7 |
| Clipmap LOD | Medium | High | 8 |

---

## Quick Wins to Implement Now

1. **Add brightness validation** in `aaa_sdf_generator.py` validate_primitive()
2. **Add onion operation** to shader for clothing primitives
3. **Update prompts** with elongation and onion layer guidance
4. **Add body region** field to primitives for context-aware blending
