#version 450 core

// Emissive Geometry Shader
// For mesh/SDF-based area lights with physical emission

// Inputs
in vec2 v_TexCoord;
in vec3 v_WorldPos;
in vec3 v_Normal;
in vec4 v_VertexColor;

// Outputs
out vec4 FragColor;

// Emission properties
uniform bool u_UseBlackbody;
uniform float u_Temperature;           // Kelvin
uniform float u_Luminosity;            // cd/m²
uniform vec3 u_EmissionColor;
uniform float u_EmissionStrength;

// Material function modulation
uniform int u_ModulationType;
uniform float u_Time;
uniform float u_Frequency;
uniform float u_Amplitude;
uniform float u_NoiseScale;

// Textures
uniform sampler2D u_EmissionMap;
uniform bool u_UseEmissionMap;

// Constants
const float PI = 3.14159265359;

// ============================================================================
// Blackbody Radiation
// ============================================================================

vec3 TemperatureToRGB(float temp) {
    temp = temp / 100.0;
    float r, g, b;

    if (temp <= 66.0) {
        r = 1.0;
        g = clamp(0.39008157876 * log(temp) - 0.63184144378, 0.0, 1.0);
    } else {
        r = clamp(1.29293618606 * pow(temp - 60.0, -0.1332047592), 0.0, 1.0);
        g = clamp(1.12989086089 * pow(temp - 60.0, -0.0755148492), 0.0, 1.0);
    }

    if (temp >= 66.0) {
        b = 1.0;
    } else if (temp <= 19.0) {
        b = 0.0;
    } else {
        b = clamp(0.54320678911 * log(temp - 10.0) - 1.19625408134, 0.0, 1.0);
    }

    return vec3(r, g, b);
}

float LuminousEfficacy(float temp) {
    // Peak at ~6500K (daylight)
    float peak_temp = 6500.0;
    float max_efficacy = 683.0;  // lm/W at 555nm
    float temp_ratio = temp / peak_temp;
    return max_efficacy * exp(-pow(log(temp_ratio), 2.0) / 0.5);
}

// ============================================================================
// Noise Functions
// ============================================================================

float Hash(vec3 p) {
    p = fract(p * 0.3183099 + 0.1);
    p *= 17.0;
    return fract(p.x * p.y * p.z * (p.x + p.y + p.z));
}

float PerlinNoise(vec3 p) {
    vec3 i = floor(p);
    vec3 f = fract(p);
    f = f * f * (3.0 - 2.0 * f);

    float n000 = Hash(i);
    float n100 = Hash(i + vec3(1.0, 0.0, 0.0));
    float n010 = Hash(i + vec3(0.0, 1.0, 0.0));
    float n110 = Hash(i + vec3(1.0, 1.0, 0.0));
    float n001 = Hash(i + vec3(0.0, 0.0, 1.0));
    float n101 = Hash(i + vec3(1.0, 0.0, 1.0));
    float n011 = Hash(i + vec3(0.0, 1.0, 1.0));
    float n111 = Hash(i + vec3(1.0, 1.0, 1.0));

    float x00 = mix(n000, n100, f.x);
    float x10 = mix(n010, n110, f.x);
    float x01 = mix(n001, n101, f.x);
    float x11 = mix(n011, n111, f.x);

    float y0 = mix(x00, x10, f.y);
    float y1 = mix(x01, x11, f.y);

    return mix(y0, y1, f.z);
}

float FBM(vec3 p, int octaves) {
    float value = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    float maxValue = 0.0;

    for (int i = 0; i < octaves; ++i) {
        value += PerlinNoise(p * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= 0.5;
        frequency *= 2.0;
    }

    return value / maxValue;
}

// ============================================================================
// Material Function Modulation
// ============================================================================

float PulseModulation(float time) {
    return 0.5 + 0.5 * sin(time * u_Frequency * 2.0 * PI);
}

float FlickerModulation(float time) {
    float noise = PerlinNoise(vec3(time * u_Frequency * 10.0, 0.0, 0.0));
    return mix(0.8, 1.0, noise);
}

float FireModulation(vec2 uv, float time) {
    vec2 p1 = uv * u_NoiseScale + vec2(0.0, time * u_Frequency * 0.5);
    vec2 p2 = uv * u_NoiseScale * 2.0 + vec2(time * u_Frequency * 0.3, time * u_Frequency * 0.7);

    float n1 = PerlinNoise(vec3(p1, 0.0));
    float n2 = PerlinNoise(vec3(p2, 1.0)) * 0.5;

    float noise = n1 + n2;
    float heightFactor = 1.0 - smoothstep(0.0, 1.0, uv.y);

    return clamp((noise * heightFactor + 0.5) * u_Amplitude, 0.0, 1.0);
}

float NeonModulation(float time) {
    float baseFlicker = PerlinNoise(vec3(time * u_Frequency * 10.0, 0.0, 0.0));
    float strongFlicker = PerlinNoise(vec3(time * u_Frequency * 0.5, 1.0, 0.0));

    if (strongFlicker < 0.1) {
        return 0.3 * u_Amplitude;
    }

    return mix(0.9, 1.0, baseFlicker) * u_Amplitude;
}

float StrobeModulation(float time) {
    float t = fract(time * u_Frequency);
    return (t < 0.1) ? u_Amplitude : 0.0;
}

float GetModulation() {
    float modulation = 1.0;

    // ModulationType: 0=Constant, 1=Pulse, 2=Flicker, 3=Fire, 4=Neon, 5=Strobe
    switch (u_ModulationType) {
        case 1:
            modulation = PulseModulation(u_Time);
            break;
        case 2:
            modulation = FlickerModulation(u_Time);
            break;
        case 3:
            modulation = FireModulation(v_TexCoord, u_Time);
            break;
        case 4:
            modulation = NeonModulation(u_Time);
            break;
        case 5:
            modulation = StrobeModulation(u_Time);
            break;
        default:
            modulation = 1.0;
            break;
    }

    return modulation;
}

// ============================================================================
// Main Function
// ============================================================================

void main() {
    // Get base emission
    vec3 emission;

    if (u_UseBlackbody) {
        // Blackbody radiation
        emission = TemperatureToRGB(u_Temperature);

        // Apply luminosity
        float efficacy = LuminousEfficacy(u_Temperature);
        emission *= u_Luminosity / max(efficacy, 1.0);
    } else {
        // Direct color emission
        emission = u_EmissionColor * u_EmissionStrength;
    }

    // Apply emission texture
    if (u_UseEmissionMap) {
        vec3 emissionTex = texture(u_EmissionMap, v_TexCoord).rgb;
        emission *= emissionTex;
    }

    // Apply material function modulation
    float modulation = GetModulation();
    emission *= modulation;

    // Apply vertex color (for per-vertex modulation)
    emission *= v_VertexColor.rgb;

    // Viewing angle falloff (emissive surfaces are less intense at grazing angles)
    vec3 viewDir = normalize(v_WorldPos);  // Assuming camera at origin for simplicity
    float viewFalloff = max(dot(normalize(v_Normal), -viewDir), 0.0);
    viewFalloff = pow(viewFalloff, 0.5);  // Soften falloff

    emission *= viewFalloff;

    // Output with full brightness (no tone mapping - this is a light source)
    FragColor = vec4(emission, 1.0);
}
