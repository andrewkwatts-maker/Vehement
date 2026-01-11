# SDF + Radiance Cascade + Advanced Optics Integration

Complete implementation plan for integrating physical light simulation with SDF rendering.

## System Overview

This document outlines the integration of:
1. **Radiance Cascade Global Illumination** - Real-time indirect lighting with 4-level cascades
2. **Spectral Rendering** - Wavelength-dependent refraction, dispersion, diffraction
3. **Black Body Radiation** - Physically-accurate emission from temperature (1000-40000K)
4. **Advanced Material System** - Sellmeier IOR, subsurface scattering, volumetric effects

## Architecture

```
SDFRenderer
├── RadianceCascade (GI system)
│   ├── 4 cascade levels (32³ base resolution)
│   ├── Probe-based light propagation
│   └── Temporal blending (95% history)
├── SpectralRenderer (wavelength simulation)
│   ├── Hero wavelength sampling
│   ├── Chromatic dispersion
│   └── CIE 1931 color matching
└── AdvancedMaterial (per-SDF-primitive)
    ├── EmissionProperties (blackbody)
    ├── DispersionProperties (Sellmeier)
    ├── SubsurfaceScatteringProperties
    └── VolumetricScatteringProperties
```

## Implementation Steps

### Step 1: SDFRenderer Header Integration

**File**: `engine/graphics/SDFRenderer.hpp`

Add forward declarations:
```cpp
class RadianceCascade;
class SpectralRenderer;
```

Add member variables (before `private:` section):
```cpp
// Global illumination
std::shared_ptr<RadianceCascade> m_radianceCascade;
bool m_enableGI = true;

// Spectral rendering
std::unique_ptr<SpectralRenderer> m_spectralRenderer;
SpectralRenderer::Mode m_spectralMode = SpectralRenderer::Mode::HeroWavelength;

// Advanced optics toggles
bool m_enableDispersion = true;
bool m_enableDiffraction = false;  // Expensive
bool m_enableBlackbody = true;
```

Add public methods:
```cpp
/**
 * @brief Set radiance cascade system
 */
void SetRadianceCascade(std::shared_ptr<RadianceCascade> cascade);

/**
 * @brief Enable/disable global illumination
 */
void SetGlobalIlluminationEnabled(bool enabled) { m_enableGI = enabled; }
bool IsGlobalIlluminationEnabled() const { return m_enableGI; }

/**
 * @brief Set spectral rendering mode
 */
void SetSpectralMode(SpectralRenderer::Mode mode) { m_spectralMode = mode; }

/**
 * @brief Enable advanced optics features
 */
void SetDispersionEnabled(bool enabled) { m_enableDispersion = enabled; }
void SetDiffractionEnabled(bool enabled) { m_enableDiffraction = enabled; }
void SetBlackbodyEnabled(bool enabled) { m_enableBlackbody = enabled; }
```

### Step 2: SDFRenderer.cpp Initialization

**File**: `engine/graphics/SDFRenderer.cpp`

In `Initialize()` after shader loading:
```cpp
// Initialize spectral renderer
m_spectralRenderer = std::make_unique<SpectralRenderer>();
m_spectralRenderer->mode = SpectralRenderer::Mode::HeroWavelength;
m_spectralRenderer->spectralSamples = 16;
m_spectralRenderer->wavelengthMin = 380.0f;  // Violet
m_spectralRenderer->wavelengthMax = 780.0f;  // Red

// Note: RadianceCascade is set externally via SetRadianceCascade()
```

Add implementation for new methods:
```cpp
void SDFRenderer::SetRadianceCascade(std::shared_ptr<RadianceCascade> cascade) {
    m_radianceCascade = cascade;

    // Initialize cascade if not already done
    if (m_radianceCascade && !m_radianceCascade->IsInitialized()) {
        RadianceCascade::Config config;
        config.numCascades = 4;
        config.baseResolution = 32;
        config.cascadeScale = 2.0f;
        config.raysPerProbe = 64;
        config.bounces = 2;
        config.updateRadius = 100.0f;
        m_radianceCascade->Initialize(config);
    }
}
```

### Step 3: Inject Emissive SDFs into Radiance Cascade

In `UploadModelData()` after uploading primitives:
```cpp
// Inject emissive primitives into radiance cascade
if (m_radianceCascade && m_radianceCascade->IsEnabled()) {
    for (const auto* prim : allPrimitives) {
        if (!prim->IsVisible()) continue;

        const auto& mat = prim->GetMaterial();
        if (mat.emissive > 0.0f) {
            // Get world position
            glm::vec3 worldPos = glm::vec3(modelTransform *
                                          glm::vec4(prim->GetPosition(), 1.0f));

            // Calculate emission radiance
            glm::vec3 emissionColor = mat.emissiveColor;
            float emissionIntensity = mat.emissive;

            // Black body radiation if enabled
            if (m_enableBlackbody && mat.temperature > 0.0f) {
                emissionColor = CalculateBlackbodyColor(mat.temperature);
                emissionIntensity *= CalculateBlackbodyIntensity(mat.temperature);
            }

            glm::vec3 radiance = emissionColor * emissionIntensity;

            // Inject into cascade
            m_radianceCascade->InjectEmissive(worldPos, radiance, prim->GetRadius());
        }
    }
}
```

Add helper functions:
```cpp
glm::vec3 SDFRenderer::CalculateBlackbodyColor(float temperature) {
    // Planck's law -> XYZ -> RGB conversion
    // Temperature in Kelvin (1000-40000)

    // Simplified approximation for game rendering
    // Full implementation would integrate Planck over visible spectrum

    if (temperature < 1000.0f) temperature = 1000.0f;
    if (temperature > 40000.0f) temperature = 40000.0f;

    // Approximate color temperature
    float t = temperature / 100.0f;
    glm::vec3 color;

    // Red
    if (t <= 66.0f) {
        color.r = 1.0f;
    } else {
        color.r = glm::clamp((329.698727446f * std::pow(t - 60.0f, -0.1332047592f)) / 255.0f,
                              0.0f, 1.0f);
    }

    // Green
    if (t <= 66.0f) {
        color.g = glm::clamp((99.4708025861f * std::log(t) - 161.1195681661f) / 255.0f,
                              0.0f, 1.0f);
    } else {
        color.g = glm::clamp((288.1221695283f * std::pow(t - 60.0f, -0.0755148492f)) / 255.0f,
                              0.0f, 1.0f);
    }

    // Blue
    if (t >= 66.0f) {
        color.b = 1.0f;
    } else if (t <= 19.0f) {
        color.b = 0.0f;
    } else {
        color.b = glm::clamp((138.5177312231f * std::log(t - 10.0f) - 305.0447927307f) / 255.0f,
                              0.0f, 1.0f);
    }

    return color;
}

float SDFRenderer::CalculateBlackbodyIntensity(float temperature) {
    // Stefan-Boltzmann law: j* = σT⁴
    // Returns normalized intensity for rendering
    const float sigma = 5.670374419e-8f;  // Stefan-Boltzmann constant
    const float T4 = std::pow(temperature, 4.0f);

    // Normalize to reasonable rendering range
    // Sun surface (5778K) = reference intensity 1.0
    const float sunTemp = 5778.0f;
    const float sunIntensity = sigma * std::pow(sunTemp, 4.0f);

    return (sigma * T4) / sunIntensity;
}
```

### Step 4: Update SetupUniforms() for GI

In `SetupUniforms()` after existing uniforms:
```cpp
// Radiance cascade uniforms
if (m_radianceCascade && m_enableGI) {
    m_raymarchShader->SetBool("u_enableGI", true);
    m_raymarchShader->SetInt("u_numCascades",
                             m_radianceCascade->GetConfig().numCascades);
    m_raymarchShader->SetVec3("u_cascadeOrigin",
                              m_radianceCascade->GetOrigin());
    m_raymarchShader->SetFloat("u_cascadeSpacing",
                               m_radianceCascade->GetBaseSpacing());

    // Bind cascade textures
    const auto& textures = m_radianceCascade->GetCascadeTextures();
    for (int i = 0; i < textures.size(); ++i) {
        glActiveTexture(GL_TEXTURE10 + i);
        glBindTexture(GL_TEXTURE_3D, textures[i]);
        m_raymarchShader->SetInt("u_cascadeTexture" + std::to_string(i), 10 + i);
    }
} else {
    m_raymarchShader->SetBool("u_enableGI", false);
}

// Spectral rendering uniforms
m_raymarchShader->SetInt("u_spectralMode", static_cast<int>(m_spectralMode));
m_raymarchShader->SetBool("u_enableDispersion", m_enableDispersion);
m_raymarchShader->SetBool("u_enableDiffraction", m_enableDiffraction);
m_raymarchShader->SetFloat("u_wavelengthMin", m_spectralRenderer->wavelengthMin);
m_raymarchShader->SetFloat("u_wavelengthMax", m_spectralRenderer->wavelengthMax);
```

### Step 5: Shader Implementation

**File**: `assets/shaders/sdf_raymarching.frag` (new sections to add)

Add uniform declarations:
```glsl
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
```

Add GI sampling function:
```glsl
vec3 SampleRadianceCascade(vec3 worldPos, vec3 normal) {
    if (!u_enableGI) return vec3(0.0);

    vec3 radiance = vec3(0.0);
    vec3 localPos = worldPos - u_cascadeOrigin;

    // Sample all cascades and blend
    for (int i = 0; i < u_numCascades; ++i) {
        float spacing = u_cascadeSpacing * pow(2.0, float(i));
        vec3 gridPos = localPos / spacing;
        vec3 uvw = gridPos * 0.5 + 0.5;  // [-1,1] -> [0,1]

        // Check if in bounds
        if (all(greaterThan(uvw, vec3(0.0))) && all(lessThan(uvw, vec3(1.0)))) {
            vec4 cascadeSample;

            // Sample appropriate cascade texture
            if (i == 0) cascadeSample = texture(u_cascadeTexture0, uvw);
            else if (i == 1) cascadeSample = texture(u_cascadeTexture1, uvw);
            else if (i == 2) cascadeSample = texture(u_cascadeTexture2, uvw);
            else cascadeSample = texture(u_cascadeTexture3, uvw);

            // cascadeSample.rgb = radiance, cascadeSample.a = validity
            float weight = cascadeSample.a;
            radiance += cascadeSample.rgb * weight;
        }
    }

    // Modulate by normal (diffuse)
    return radiance * max(0.0, dot(normal, vec3(0.0, 1.0, 0.0)));
}
```

Add chromatic dispersion function:
```glsl
// Sellmeier equation for wavelength-dependent IOR
float GetDispersedIOR(float baseIOR, float wavelength_nm) {
    // Simplified Sellmeier for glass (BK7)
    float lambda = wavelength_nm / 1000.0;  // Convert to micrometers
    float lambda2 = lambda * lambda;

    const float B1 = 1.03961212;
    const float B2 = 0.231792344;
    const float B3 = 1.01046945;
    const float C1 = 0.00600069867;
    const float C2 = 0.0200179144;
    const float C3 = 103.560653;

    float n2 = 1.0 +
        (B1 * lambda2) / (lambda2 - C1) +
        (B2 * lambda2) / (lambda2 - C2) +
        (B3 * lambda2) / (lambda2 - C3);

    return sqrt(n2);
}

vec3 ChromaticDispersion(vec3 incident, vec3 normal, float baseIOR) {
    if (!u_enableDispersion) {
        return refract(incident, normal, 1.0 / baseIOR);
    }

    // Sample three wavelengths (RGB)
    float lambda_r = 700.0;  // Red
    float lambda_g = 546.1;  // Green (e-line)
    float lambda_b = 435.8;  // Blue (g-line)

    float ior_r = GetDispersedIOR(baseIOR, lambda_r);
    float ior_g = GetDispersedIOR(baseIOR, lambda_g);
    float ior_b = GetDispersedIOR(baseIOR, lambda_b);

    // Refract each wavelength separately
    vec3 refract_r = refract(incident, normal, 1.0 / ior_r);
    vec3 refract_g = refract(incident, normal, 1.0 / ior_g);
    vec3 refract_b = refract(incident, normal, 1.0 / ior_b);

    // For rendering, use weighted average direction
    // (Full spectral would trace separate rays)
    return normalize(refract_r + refract_g + refract_b);
}
```

Add diffraction (Fraunhofer approximation):
```glsl
float DiffractionIntensity(float featureSize_nm, float wavelength_nm, float angle) {
    if (!u_enableDiffraction) return 1.0;

    // Fraunhofer single-slit diffraction
    // I(θ) = I₀ (sin(β)/β)² where β = πa sin(θ)/λ

    float a = featureSize_nm;
    float lambda = wavelength_nm;
    float beta = 3.14159 * a * sin(angle) / lambda;

    if (abs(beta) < 0.001) return 1.0;  // Avoid division by zero

    float sinc = sin(beta) / beta;
    return sinc * sinc;
}
```

Modify main raymarch loop to include GI:
```glsl
// In the main() function, after hit detection:
if (hit) {
    vec3 hitPos = rayOrigin + rayDir * t;
    vec3 normal = CalculateNormal(hitPos);

    // Direct lighting (existing code)
    vec3 directLight = CalculateDirectLighting(hitPos, normal, material);

    // Global illumination (new)
    vec3 indirectLight = SampleRadianceCascade(hitPos, normal);

    // Combine
    vec3 finalColor = directLight + indirectLight * material.albedo;

    // Handle transmission/refraction
    if (material.transmission > 0.0) {
        vec3 refractDir = ChromaticDispersion(rayDir, normal, material.ior);
        // Continue ray for transparency...
    }

    fragColor = vec4(finalColor, 1.0);
}
```

### Step 6: Update SDF Material Structure

**File**: `engine/sdf/SDFMaterial.hpp` (add fields)

```cpp
struct SDFMaterial {
    // ... existing fields ...

    // Black body emission
    float temperature = 0.0f;           // Kelvin (0 = disabled)
    float luminosity = 0.0f;            // cd/m²

    // Optical properties
    float ior = 1.5f;                   // Base refractive index
    float abbeNumber = 55.0f;           // Dispersion (higher = less)
    float transmission = 0.0f;          // 0 = opaque, 1 = transparent

    // Subsurface scattering
    float sssRadius = 0.0f;             // mm (0 = disabled)
    glm::vec3 sssColor{0.8f, 0.3f, 0.2f};

    // Anisotropy
    float anisotropy = 0.0f;            // 0-1
    glm::vec3 tangentDir{1,0,0};
};
```

### Step 7: Update GPU Primitive Data

**File**: `engine/graphics/SDFRenderer.hpp` - Update `SDFPrimitiveData`

```cpp
struct SDFPrimitiveData {
    // ... existing fields ...

    glm::vec4 opticalProperties;   // ior, abbeNumber, transmission, sssRadius
    glm::vec4 emissionProperties;  // temperature, luminosity, padding, padding
    glm::vec4 sssColor;            // subsurface scattering color
};
```

Update `UploadModelData()` to populate new fields:
```cpp
data.opticalProperties = glm::vec4(
    mat.ior,
    mat.abbeNumber,
    mat.transmission,
    mat.sssRadius
);

data.emissionProperties = glm::vec4(
    mat.temperature,
    mat.luminosity,
    0.0f, 0.0f
);

data.sssColor = glm::vec4(mat.sssColor, 0.0f);
```

## Performance Considerations

### Radiance Cascade
- **Cost**: ~2ms @ 1080p (4 cascades, 32³ base)
- **Optimization**: Update only 1024 probes/frame, temporal reuse
- **Quality**: 2-bounce indirect lighting with soft shadows

### Spectral Rendering
- **Hero Wavelength**: ~0.3ms overhead (recommended)
- **Full Spectral** (16 samples): ~4ms overhead (offline quality)
- **RGB Mode**: No overhead (fallback)

### Chromatic Dispersion
- **Cost**: ~0.5ms for transparent objects
- **Optimization**: Only enabled for glass/crystal materials

### Diffraction
- **Cost**: ~2-3ms (very expensive)
- **Use Case**: Gemstones, holographic materials, CD surfaces
- **Recommendation**: Disabled by default, enable for specific materials

## Testing Plan

### Phase 1: Black Body Emission
1. Create emissive SDF sphere with `temperature = 6500K` (daylight)
2. Verify color matches D65 white point
3. Test range: 1000K (red) -> 40000K (blue-white)

### Phase 2: Radiance Cascade Injection
1. Place emissive sphere in scene
2. Verify cascade textures receive emission data
3. Check indirect lighting on nearby surfaces

### Phase 3: Chromatic Dispersion
1. Create glass prism (IOR=1.5, abbeNumber=59)
2. Shine white light through
3. Verify rainbow spectrum output

### Phase 4: Full Integration
1. Render warrior_hero.json with:
   - Glowing eyes (temperature=4000K, orange)
   - Metallic armor (anisotropy=0.38)
   - Glass visor (dispersion enabled)
2. Verify GI bounces from glowing eyes to armor
3. Check performance: target <6ms total

## Example Usage

```cpp
// In asset_icon_renderer.cpp or RTSApplication.cpp

// Initialize renderer with GI
SDFRenderer renderer;
renderer.Initialize();

// Create radiance cascade
auto cascade = std::make_shared<RadianceCascade>();
RadianceCascade::Config cascadeConfig;
cascadeConfig.numCascades = 4;
cascadeConfig.baseResolution = 32;
cascadeConfig.raysPerProbe = 64;
cascadeConfig.bounces = 2;
cascade->Initialize(cascadeConfig);

renderer.SetRadianceCascade(cascade);
renderer.SetGlobalIlluminationEnabled(true);
renderer.SetSpectralMode(SpectralRenderer::Mode::HeroWavelength);
renderer.SetDispersionEnabled(true);

// Load hero asset
SDFModel model;
model.Load("game/assets/heroes/alien_commander.json");

// Render with full optics
Camera camera;
camera.LookAt(glm::vec3(0, 2, 5), glm::vec3(0, 1, 0));

renderer.Render(model, camera);

// Update cascade each frame
cascade->Update(camera.GetPosition(), deltaTime);
cascade->PropagateLighting();
```

## References

- **Radiance Cascades**: Sannikov (2024) - https://drive.google.com/file/d/1L6v1_7HY2X-LV3Ofb6oyTIxgEaP4LOI6/view
- **Spectral Rendering**: Wilkie & Weidlich (2013) - "Hero Wavelength Spectral Sampling"
- **Black Body Radiation**: Planck's Law, Wien's Displacement
- **Sellmeier Equation**: Wavelength-dependent IOR (1871)
- **Diffraction**: Fraunhofer & Fresnel approximations

## Next Steps

1. Apply API fixes to SDFRenderer.cpp (sed commands)
2. Add new member variables to SDFRenderer.hpp
3. Implement helper functions in SDFRenderer.cpp
4. Create/update sdf_raymarching.frag shader
5. Test with alien_commander.json asset
6. Optimize performance profiling
7. Document shader parameters for artists

---

**Status**: Ready for implementation
**Priority**: High (blocks hero asset rendering with full quality)
**Estimated Time**: 3-4 hours total
