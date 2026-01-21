# Painting with Maths: SDF Character Creation Guide

> "Like watching a Bob Ross video but drawing with SDF formulas as a palette" - Community reaction to Inigo Quilez's techniques

This document covers the mathematical techniques for creating characters and assets using Signed Distance Fields (SDFs), based on techniques pioneered by [Inigo Quilez](https://iquilezles.org/) and the shader art community.

---

## Table of Contents

1. [SDF Fundamentals](#sdf-fundamentals)
2. [Primitive Types](#primitive-types)
3. [Smooth Blending Operations](#smooth-blending-operations)
4. [Coloring Techniques](#coloring-techniques)
5. [Material Properties](#material-properties)
6. [Procedural Textures](#procedural-textures)
7. [Lighting & Shading](#lighting--shading)
8. [Character Construction Workflow](#character-construction-workflow)
9. [AI-Assisted Pipeline](#ai-assisted-pipeline)

---

## SDF Fundamentals

A Signed Distance Field (SDF) is a function that returns the shortest distance from any point in space to the surface of an object:
- **Negative values**: Inside the object
- **Zero**: On the surface
- **Positive values**: Outside the object

### Why SDFs for Characters?

| Benefit | Description |
|---------|-------------|
| **Infinite Resolution** | No polygon artifacts, smooth at any zoom |
| **Organic Blending** | Smooth unions create natural-looking joints |
| **Compact Storage** | A character can be represented with ~32 primitives |
| **Real-time Deformation** | Volumes deform without mesh artifacts |

---

## Primitive Types

### Basic Primitives

```glsl
// Sphere - The fundamental building block
float sdSphere(vec3 p, float r) {
    return length(p) - r;
}

// Box - Sharp-edged volumes
float sdBox(vec3 p, vec3 b) {
    vec3 q = abs(p) - b;
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0);
}

// Rounded Box - Soft-edged boxes (great for torsos, heads)
float sdRoundedBox(vec3 p, vec3 b, float r) {
    vec3 q = abs(p) - b + vec3(r);
    return length(max(q, 0.0)) + min(max(q.x, max(q.y, q.z)), 0.0) - r;
}

// Capsule - Perfect for limbs, fingers, tails
float sdCapsule(vec3 p, vec3 a, vec3 b, float r) {
    vec3 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - r;
}

// Ellipsoid - Organic shapes (heads, torsos, eyes)
float sdEllipsoid(vec3 p, vec3 r) {
    float k0 = length(p / r);
    float k1 = length(p / (r * r));
    return k0 * (k0 - 1.0) / k1;
}

// Torus - Rings, bracelets, halos
float sdTorus(vec3 p, vec2 t) {
    vec2 q = vec2(length(p.xz) - t.x, p.y);
    return length(q) - t.y;
}

// Cone - Hats, spikes, claws
float sdCone(vec3 p, vec2 c, float h) {
    vec2 q = h * vec2(c.x / c.y, -1.0);
    vec2 w = vec2(length(p.xz), p.y);
    vec2 a = w - q * clamp(dot(w, q) / dot(q, q), 0.0, 1.0);
    vec2 b = w - q * vec2(clamp(w.x / q.x, 0.0, 1.0), 1.0);
    float k = sign(q.y);
    float d = min(dot(a, a), dot(b, b));
    float s = max(k * (w.x * q.y - w.y * q.x), k * (w.y - q.y));
    return sqrt(d) * sign(s);
}
```

### Primitive Selection Guide

| Body Part | Recommended Primitive | Why |
|-----------|----------------------|-----|
| Head | Ellipsoid or Sphere | Natural rounded shape |
| Torso | Rounded Box or Ellipsoid | Broad, organic volume |
| Limbs | Capsule | Natural tapered cylinders |
| Fingers/Toes | Small Capsules | Thin cylindrical shapes |
| Eyes | Sphere | Perfect spherical shape |
| Joints | Sphere (blended) | Smooth transitions |
| Accessories | Torus, Box, Cone | Sharp detail elements |

---

## Smooth Blending Operations

The key to organic-looking characters is **smooth blending** (also called "smoothmin" or "soft blending").

### Standard Smooth Operations

```glsl
// Smooth Union - Merges shapes organically (IQ's polynomial version)
// Parameter k controls blend radius in world units
float opSmoothUnion(float d1, float d2, float k) {
    k *= 4.0;
    float h = max(k - abs(d1 - d2), 0.0);
    return min(d1, d2) - h * h * 0.25 / k;
}

// Smooth Subtraction - Carves shapes with soft edges
float opSmoothSubtraction(float d1, float d2, float k) {
    return -opSmoothUnion(d1, -d2, k);
}

// Smooth Intersection - Soft intersection of volumes
float opSmoothIntersection(float d1, float d2, float k) {
    return -opSmoothUnion(-d1, -d2, k);
}
```

### Advanced Blending Variants

```glsl
// Exponential Smooth Union - More organic, flowing blends
float opExpSmoothUnion(float d1, float d2, float k) {
    if (k == 0.0) return min(d1, d2);
    float res = exp2(-k * d1) + exp2(-k * d2);
    return -log2(res) / k;
}

// Cubic Smooth Union - Better C2 continuity for animation
float opCubicSmoothUnion(float d1, float d2, float k) {
    if (k == 0.0) return min(d1, d2);
    float h = max(k - abs(d1 - d2), 0.0) / k;
    float m = h * h * h * 0.5;
    float s = m * k * (1.0 / 3.0);
    return (d1 < d2) ? d1 - s : d2 - s;
}

// Distance-Aware Blend - Prevents unwanted merging of distant parts
float opDistanceAwareBlend(float d1, float d2, float k, float maxDist) {
    float dist = abs(d1 - d2);
    if (dist > maxDist) return min(d1, d2);  // Hard union when far apart
    float falloff = 1.0 - (dist / maxDist);
    return opSmoothUnion(d1, d2, k * falloff);
}
```

### Blend Radius Guidelines

| Connection Type | Suggested k Value | Description |
|-----------------|-------------------|-------------|
| Shoulder to arm | 0.1 - 0.15 | Medium blend for natural joint |
| Hip to leg | 0.1 - 0.15 | Similar to shoulder |
| Finger joints | 0.02 - 0.05 | Small, subtle blends |
| Head to neck | 0.08 - 0.12 | Moderate blend |
| Organic blobs | 0.2 - 0.5 | Large, goopy blends |
| Sharp details | 0.0 (hard union) | No blending for accessories |

---

## Coloring Techniques

### Position-Based Coloring

```glsl
// Color based on world position (great for gradients)
vec3 positionColor(vec3 p) {
    return vec3(
        0.5 + 0.5 * sin(p.x * 2.0),
        0.5 + 0.5 * sin(p.y * 2.0 + 1.0),
        0.5 + 0.5 * sin(p.z * 2.0 + 2.0)
    );
}

// Vertical gradient (skin tone variation)
vec3 verticalGradient(vec3 p, vec3 topColor, vec3 bottomColor, float height) {
    float t = clamp((p.y + height * 0.5) / height, 0.0, 1.0);
    return mix(bottomColor, topColor, t);
}
```

### Per-Primitive Coloring

```glsl
// Track closest primitive during evaluation
struct SDFResult {
    float distance;
    int primitiveId;
    vec3 color;
    float metallic;
    float roughness;
};

// Return material from closest surface
SDFResult evaluateScene(vec3 p) {
    SDFResult result;
    result.distance = MAX_DIST;

    // Head - skin tone
    float dHead = sdEllipsoid(p - headPos, headRadii);
    if (dHead < result.distance) {
        result.distance = dHead;
        result.color = vec3(0.9, 0.7, 0.6);  // Skin
        result.metallic = 0.0;
        result.roughness = 0.8;
    }

    // Eyes - shiny dark
    float dEye = sdSphere(p - eyePos, eyeRadius);
    if (dEye < result.distance) {
        result.distance = dEye;
        result.color = vec3(0.1, 0.1, 0.15);  // Dark iris
        result.metallic = 0.0;
        result.roughness = 0.1;  // Shiny!
    }

    return result;
}
```

### Surface-Detail Coloring

```glsl
// Triplanar projection for texture-like patterns
vec3 triplanarColor(vec3 p, vec3 normal, sampler2D tex) {
    vec3 blend = abs(normal);
    blend = normalize(max(blend, 0.00001));
    blend /= (blend.x + blend.y + blend.z);

    vec3 xColor = texture(tex, p.yz).rgb;
    vec3 yColor = texture(tex, p.xz).rgb;
    vec3 zColor = texture(tex, p.xy).rgb;

    return xColor * blend.x + yColor * blend.y + zColor * blend.z;
}

// Procedural spots/freckles
float spots(vec3 p, float density, float size) {
    vec3 cell = floor(p * density);
    vec3 local = fract(p * density) - 0.5;

    // Random offset per cell
    vec3 offset = hash33(cell) - 0.5;
    float d = length(local - offset * 0.5);

    return smoothstep(size, size * 0.5, d);
}
```

---

## Material Properties

### PBR Material Model

```glsl
struct Material {
    vec3 baseColor;      // Albedo color
    float metallic;      // 0 = dielectric, 1 = metal
    float roughness;     // 0 = mirror, 1 = diffuse
    float subsurface;    // Subsurface scattering amount
    vec3 emissive;       // Self-illumination color
    float emissiveStrength;
};

// Common material presets
Material skinMaterial() {
    return Material(
        vec3(0.9, 0.7, 0.6),  // Warm skin tone
        0.0,                   // Not metallic
        0.65,                  // Slightly rough
        0.3,                   // Some subsurface
        vec3(0.0),             // No emission
        0.0
    );
}

Material eyeMaterial() {
    return Material(
        vec3(0.1, 0.1, 0.15),  // Dark iris
        0.0,                    // Not metallic
        0.05,                   // Very smooth/wet
        0.0,                    // No subsurface
        vec3(0.0),
        0.0
    );
}

Material metalArmorMaterial() {
    return Material(
        vec3(0.8, 0.8, 0.85),  // Silver
        0.95,                   // Very metallic
        0.3,                    // Somewhat polished
        0.0,                    // No subsurface
        vec3(0.0),
        0.0
    );
}
```

### Iridescence (Color-Shifting Materials)

```glsl
// Iridescent effect based on view angle and surface normal
vec3 iridescence(vec3 baseColor, vec3 viewDir, vec3 normal, float strength) {
    float NdotV = max(dot(normal, viewDir), 0.0);

    // Shift hue based on viewing angle
    vec3 thinFilm = vec3(
        0.5 + 0.5 * cos(NdotV * 6.28 + 0.0),
        0.5 + 0.5 * cos(NdotV * 6.28 + 2.09),
        0.5 + 0.5 * cos(NdotV * 6.28 + 4.18)
    );

    return mix(baseColor, thinFilm, strength);
}
```

---

## Procedural Textures

### Noise Functions

```glsl
// Value noise (smooth random)
float valueNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);  // Smoothstep

    float n = i.x + i.y * 57.0 + i.z * 113.0;

    return mix(
        mix(mix(hash(n), hash(n + 1.0), f.x),
            mix(hash(n + 57.0), hash(n + 58.0), f.x), f.y),
        mix(mix(hash(n + 113.0), hash(n + 114.0), f.x),
            mix(hash(n + 170.0), hash(n + 171.0), f.x), f.y),
        f.z
    );
}

// FBM (Fractal Brownian Motion) - layered noise
float fbm(vec3 p, int octaves) {
    float value = 0.0;
    float amplitude = 0.5;
    float frequency = 1.0;

    for (int i = 0; i < octaves; i++) {
        value += amplitude * valueNoise(p * frequency);
        frequency *= 2.0;
        amplitude *= 0.5;
    }

    return value;
}
```

### Surface Detail Patterns

```glsl
// Scales pattern (for reptiles, fish, dragons)
float scales(vec3 p, vec3 normal, float size) {
    // Project onto dominant axis
    vec2 uv;
    if (abs(normal.y) > 0.5) {
        uv = p.xz;
    } else if (abs(normal.x) > abs(normal.z)) {
        uv = p.yz;
    } else {
        uv = p.xy;
    }

    uv *= size;
    vec2 cell = floor(uv);
    vec2 local = fract(uv) - 0.5;

    // Hexagonal offset for alternating rows
    if (mod(cell.y, 2.0) > 0.5) {
        local.x -= 0.5;
    }

    return 1.0 - smoothstep(0.3, 0.35, length(local));
}

// Fur/hair strands
float furPattern(vec3 p, vec3 normal, float density, float length) {
    vec3 tangent = normalize(cross(normal, vec3(0, 1, 0)));
    float n = fbm(p * density, 4);
    return smoothstep(0.3, 0.7, n) * length;
}
```

---

## Lighting & Shading

### Normal Calculation

```glsl
vec3 calcNormal(vec3 p) {
    const float eps = 0.001;
    return normalize(vec3(
        evaluateSDF(p + vec3(eps, 0, 0)) - evaluateSDF(p - vec3(eps, 0, 0)),
        evaluateSDF(p + vec3(0, eps, 0)) - evaluateSDF(p - vec3(0, eps, 0)),
        evaluateSDF(p + vec3(0, 0, eps)) - evaluateSDF(p - vec3(0, 0, eps))
    ));
}
```

### Soft Shadows

```glsl
float softShadow(vec3 ro, vec3 rd, float mint, float maxt, float k) {
    float res = 1.0;
    float t = mint;

    for (int i = 0; i < 64 && t < maxt; i++) {
        float h = evaluateSDF(ro + rd * t);
        if (h < 0.001) return 0.0;
        res = min(res, k * h / t);
        t += h;
    }

    return clamp(res, 0.0, 1.0);
}
```

### Ambient Occlusion

```glsl
float ambientOcclusion(vec3 p, vec3 n) {
    float occ = 0.0;
    float sca = 1.0;

    for (int i = 0; i < 5; i++) {
        float h = 0.01 + 0.12 * float(i);
        float d = evaluateSDF(p + h * n);
        occ += (h - d) * sca;
        sca *= 0.95;
    }

    return clamp(1.0 - 3.0 * occ, 0.0, 1.0);
}
```

---

## Character Construction Workflow

### Step 1: Block Out Major Forms

```
1. Torso (Ellipsoid or Rounded Box)
   └── Position: (0, 1.0, 0)
   └── Radii: (0.3, 0.4, 0.2)

2. Head (Ellipsoid)
   └── Position: (0, 1.6, 0)
   └── Radii: (0.15, 0.18, 0.15)
   └── Blend: Smooth Union k=0.1 with torso

3. Hips (Ellipsoid)
   └── Position: (0, 0.7, 0)
   └── Radii: (0.25, 0.15, 0.15)
   └── Blend: Smooth Union k=0.1 with torso
```

### Step 2: Add Limbs

```
4. Upper Arms (Capsule each)
   └── Start: shoulder position
   └── End: elbow position
   └── Radius: 0.06
   └── Blend: Smooth Union k=0.08 with torso

5. Lower Arms (Capsule each)
   └── Tapered radius (0.05 to 0.04)
   └── Blend: Smooth Union k=0.05 with upper arm

6. Legs (same pattern as arms)
```

### Step 3: Add Details

```
7. Eyes (Spheres)
   └── Smooth Union k=0.02 with head
   └── Material: shiny, dark

8. Nose, Ears, Fingers...
   └── Small primitives
   └── Subtle blends (k=0.01-0.03)
```

### Step 4: Apply Materials

```
9. Assign colors per primitive
10. Set metallic/roughness per region
11. Add emissive for glowing elements
```

---

## AI-Assisted Pipeline

### Concept Art to SDF Workflow

```
┌──────────────────┐    ┌──────────────────┐    ┌──────────────────┐
│  Concept Art     │───>│  AI SVG Gen      │───>│  SDF JSON Gen    │
│  (Image/Text)    │    │  (3 Views)       │    │  (Primitives)    │
└──────────────────┘    └──────────────────┘    └──────────────────┘
                                                         │
┌──────────────────┐    ┌──────────────────┐            │
│  Rendered Icon   │<───│  SDF Renderer    │<───────────┘
│  (PNG/Video)     │    │  (Nova3D)        │
└──────────────────┘    └──────────────────┘
```

### SVG Coordinate Mapping

From SVG pixel coordinates to SDF world units:
```
SDF_X = (SVG_X - 200) * 0.005    // Center at X=200
SDF_Y = (400 - SVG_Y) * 0.005    // Invert Y, center at ~Y=200
SDF_Z = 0.0                       // Front view
```

### AI Prompt Template

When asking AI to generate SDF content:
```
Given this SVG concept art with coordinate markers:
- Front view: [SVG data]
- Side view: [SVG data]
- Top view: [SVG data]

Generate SDF primitives using:
1. Ellipsoid for organic shapes (heads, torsos)
2. Capsule for limbs
3. Sphere for joints and eyes
4. Smooth Union (k=0.05-0.15) for organic connections

Use the coordinate markers in the SVG to position primitives.
Output JSON format with primitives[], materials[], and skeleton[].
```

---

## References

- [Inigo Quilez - Distance Functions](https://iquilezles.org/articles/distfunctions/)
- [Maxime Heckel - Painting with Math](https://blog.maximeheckel.com/posts/painting-with-math-a-gentle-study-of-raymarching/)
- [Shadertoy](https://www.shadertoy.com)
- [Mercury hg_sdf Library](http://mercury.sexy/hg_sdf/)
- [SDF Resources Collection](https://github.com/CedricGuillemet/SDF)

---

*This document is part of the Nova3D Engine documentation.*
