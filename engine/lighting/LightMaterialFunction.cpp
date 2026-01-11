#include "LightMaterialFunction.hpp"
#include "../graphics/Texture.hpp"
#include "../materials/MaterialGraph.hpp"
#include <cmath>
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace Engine {

LightMaterialFunction::LightMaterialFunction() {
}

LightMaterialFunction::~LightMaterialFunction() {
}

float LightMaterialFunction::Evaluate(const glm::vec2& uv, float time) const {
    float modulation = 1.0f;

    // Apply animation
    switch (type) {
        case ModulationType::Constant:
            modulation = 1.0f;
            break;

        case ModulationType::Pulse:
            modulation = PulseAnimation(time);
            break;

        case ModulationType::Flicker:
            modulation = FlickerAnimation(time);
            break;

        case ModulationType::Fire:
            modulation = FireAnimation(uv, time);
            break;

        case ModulationType::Neon:
            modulation = NeonAnimation(time);
            break;

        case ModulationType::Strobe:
            modulation = StrobeAnimation(time);
            break;

        case ModulationType::Breathe:
            modulation = BreatheAnimation(time);
            break;

        case ModulationType::Wave:
            modulation = WaveAnimation(uv, time);
            break;

        case ModulationType::UVTexture:
        case ModulationType::LatLong:
        case ModulationType::Gobo:
            if (modulationTexture && modulationTextureEnabled) {
                glm::vec2 transformedUV = TransformUV(uv);
                modulation = modulationTexture->Sample(transformedUV).r;
            }
            break;

        case ModulationType::Custom:
            // Custom shader graph evaluation would go here
            modulation = 1.0f;
            break;
    }

    return glm::clamp(modulation * amplitude, 0.0f, 1.0f);
}

float LightMaterialFunction::EvaluateDirection(const glm::vec3& direction, float time) const {
    if (type == ModulationType::LatLong) {
        glm::vec2 uv = DirectionToLatLong(direction);
        return Evaluate(uv, time);
    }

    // For other types, convert direction to UV
    glm::vec2 uv = DirectionToUV(direction);
    return Evaluate(uv, time);
}

glm::vec3 LightMaterialFunction::EvaluateColor(const glm::vec2& uv, float time) const {
    if (!useColorModulation) {
        return glm::vec3(1.0f);
    }

    glm::vec3 color = colorTint;

    // Add variation based on animation
    if (type == ModulationType::Fire) {
        float noise = FractalNoise(uv * 5.0f + glm::vec2(time * 0.5f), 3);
        color += colorVariation * noise;
    }

    return glm::clamp(color, 0.0f, 1.0f);
}

float LightMaterialFunction::EvaluatePointLight(const glm::vec3& worldPos, const glm::vec3& lightPos, float time) const {
    glm::vec3 direction = glm::normalize(worldPos - lightPos);
    return EvaluateDirection(direction, time);
}

float LightMaterialFunction::EvaluateSpotLight(const glm::vec3& worldPos, const glm::vec3& lightPos,
                                                const glm::vec3& lightDir, float time) const {
    // Project world position onto light's view plane
    glm::vec3 toPoint = worldPos - lightPos;
    float distance = glm::length(toPoint);
    glm::vec3 direction = toPoint / distance;

    // Calculate UV coordinates in spotlight cone
    glm::vec3 right = glm::normalize(glm::cross(lightDir, glm::vec3(0.0f, 1.0f, 0.0f)));
    glm::vec3 up = glm::cross(right, lightDir);

    float u = glm::dot(direction, right) * 0.5f + 0.5f;
    float v = glm::dot(direction, up) * 0.5f + 0.5f;

    return Evaluate(glm::vec2(u, v), time);
}

// Animation implementations
float LightMaterialFunction::PulseAnimation(float time) const {
    float t = (time + offset) * frequency * 2.0f * glm::pi<float>();
    return 0.5f + 0.5f * std::sin(t + phase * 2.0f * glm::pi<float>());
}

float LightMaterialFunction::FlickerAnimation(float time) const {
    float t = time + offset;
    float noise = PerlinNoise(glm::vec3(t * frequency, phase, 0.0f));
    return glm::mix(1.0f - noiseStrength, 1.0f, noise);
}

float LightMaterialFunction::FireAnimation(const glm::vec2& uv, float time) const {
    float t = time + offset;

    // Multiple octaves of noise for realistic fire
    glm::vec2 p1 = uv * noiseScale + glm::vec2(0.0f, t * frequency * 0.5f);
    glm::vec2 p2 = uv * noiseScale * 2.0f + glm::vec2(t * frequency * 0.3f, t * frequency * 0.7f);
    glm::vec2 p3 = uv * noiseScale * 4.0f + glm::vec2(t * frequency * 0.1f, t * frequency);

    float n1 = PerlinNoise(glm::vec3(p1.x, p1.y, 0.0f));
    float n2 = PerlinNoise(glm::vec3(p2.x, p2.y, 1.0f)) * 0.5f;
    float n3 = PerlinNoise(glm::vec3(p3.x, p3.y, 2.0f)) * 0.25f;

    float noise = n1 + n2 + n3;

    // Attenuate based on height (fire is brighter at base)
    float heightFactor = 1.0f - glm::smoothstep(0.0f, 1.0f, uv.y);

    return glm::clamp((noise * heightFactor + 0.5f) * amplitude, 0.0f, 1.0f);
}

float LightMaterialFunction::NeonAnimation(float time) const {
    float t = time + offset;

    // Base flicker
    float baseFlicker = PerlinNoise(glm::vec3(t * frequency * 10.0f, phase, 0.0f));

    // Occasional strong flickers
    float strongFlicker = PerlinNoise(glm::vec3(t * frequency * 0.5f, phase + 1.0f, 0.0f));
    if (strongFlicker < 0.1f) {
        return 0.3f * amplitude;
    }

    return glm::mix(0.9f, 1.0f, baseFlicker) * amplitude;
}

float LightMaterialFunction::StrobeAnimation(float time) const {
    float t = std::fmod((time + offset) * frequency, 1.0f);
    return (t < dutyCycle) ? amplitude : 0.0f;
}

float LightMaterialFunction::BreatheAnimation(float time) const {
    float t = (time + offset) * frequency * 2.0f * glm::pi<float>();
    float breath = std::sin(t + phase * 2.0f * glm::pi<float>());

    // Smooth cubic easing
    breath = breath * breath * breath;

    return glm::mix(1.0f - noiseStrength, 1.0f, (breath + 1.0f) * 0.5f) * amplitude;
}

float LightMaterialFunction::WaveAnimation(const glm::vec2& uv, float time) const {
    float t = time + offset;
    float wave = std::sin(uv.x * noiseScale + t * frequency * 2.0f * glm::pi<float>() + phase * 2.0f * glm::pi<float>());
    return 0.5f + 0.5f * wave * amplitude;
}

// Noise implementations (simplified Perlin noise)
float LightMaterialFunction::PerlinNoise(const glm::vec2& p) const {
    return PerlinNoise(glm::vec3(p.x, p.y, 0.0f));
}

float LightMaterialFunction::PerlinNoise(const glm::vec3& p) const {
    // Simplified noise function
    glm::vec3 i = glm::floor(p);
    glm::vec3 f = glm::fract(p);

    // Smoothstep
    f = f * f * (3.0f - 2.0f * f);

    // Hash function
    auto hash = [](const glm::vec3& p) -> float {
        float n = p.x + p.y * 157.0f + p.z * 113.0f;
        return std::fmod(std::sin(n) * 43758.5453f, 1.0f);
    };

    // Interpolate
    float n000 = hash(i);
    float n100 = hash(i + glm::vec3(1.0f, 0.0f, 0.0f));
    float n010 = hash(i + glm::vec3(0.0f, 1.0f, 0.0f));
    float n110 = hash(i + glm::vec3(1.0f, 1.0f, 0.0f));
    float n001 = hash(i + glm::vec3(0.0f, 0.0f, 1.0f));
    float n101 = hash(i + glm::vec3(1.0f, 0.0f, 1.0f));
    float n011 = hash(i + glm::vec3(0.0f, 1.0f, 1.0f));
    float n111 = hash(i + glm::vec3(1.0f, 1.0f, 1.0f));

    float x00 = glm::mix(n000, n100, f.x);
    float x10 = glm::mix(n010, n110, f.x);
    float x01 = glm::mix(n001, n101, f.x);
    float x11 = glm::mix(n011, n111, f.x);

    float y0 = glm::mix(x00, x10, f.y);
    float y1 = glm::mix(x01, x11, f.y);

    return glm::mix(y0, y1, f.z);
}

float LightMaterialFunction::FractalNoise(const glm::vec2& p, int octaves) const {
    float value = 0.0f;
    float amplitude = 1.0f;
    float frequency = 1.0f;
    float maxValue = 0.0f;

    for (int i = 0; i < octaves; ++i) {
        value += PerlinNoise(p * frequency) * amplitude;
        maxValue += amplitude;
        amplitude *= 0.5f;
        frequency *= 2.0f;
    }

    return value / maxValue;
}

glm::vec2 LightMaterialFunction::TransformUV(const glm::vec2& uv) const {
    glm::vec2 transformed = uv;

    // Scale
    transformed = transformed * textureScale;

    // Rotate
    if (textureRotation != 0.0f) {
        float angle = textureRotation * glm::pi<float>() / 180.0f;
        float c = std::cos(angle);
        float s = std::sin(angle);
        glm::mat2 rot(c, -s, s, c);
        transformed = rot * (transformed - 0.5f) + 0.5f;
    }

    // Offset
    transformed += textureOffset;

    return transformed;
}

glm::vec2 LightMaterialFunction::DirectionToLatLong(const glm::vec3& direction) const {
    glm::vec3 dir = glm::normalize(direction);

    float u = 0.5f + std::atan2(dir.z, dir.x) / (2.0f * glm::pi<float>());
    float v = 0.5f - std::asin(dir.y) / glm::pi<float>();

    return glm::vec2(u, v);
}

glm::vec2 LightMaterialFunction::DirectionToUV(const glm::vec3& direction) const {
    // Spherical projection
    return DirectionToLatLong(direction);
}

void LightMaterialFunction::Save(const std::string& filepath) const {
    json j;
    j["type"] = static_cast<int>(type);
    j["frequency"] = frequency;
    j["amplitude"] = amplitude;
    j["offset"] = offset;
    j["phase"] = phase;
    j["noiseScale"] = noiseScale;
    j["noiseStrength"] = noiseStrength;

    std::ofstream file(filepath);
    file << j.dump(4);
    file.close();
}

void LightMaterialFunction::Load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return;

    json j;
    file >> j;

    type = static_cast<ModulationType>(j["type"]);
    frequency = j["frequency"];
    amplitude = j["amplitude"];
    offset = j["offset"];
    phase = j["phase"];

    file.close();
}

// Preset implementations
LightMaterialFunction LightMaterialFunctionPresets::CreateCandle() {
    LightMaterialFunction func;
    func.type = LightMaterialFunction::ModulationType::Fire;
    func.frequency = 2.0f;
    func.amplitude = 0.9f;
    func.noiseScale = 3.0f;
    func.noiseStrength = 0.3f;
    func.useColorModulation = true;
    func.colorTint = glm::vec3(1.0f, 0.7f, 0.3f);
    func.colorVariation = glm::vec3(0.2f, 0.1f, 0.0f);
    return func;
}

LightMaterialFunction LightMaterialFunctionPresets::CreateFireplace() {
    LightMaterialFunction func;
    func.type = LightMaterialFunction::ModulationType::Fire;
    func.frequency = 1.5f;
    func.amplitude = 1.0f;
    func.noiseScale = 5.0f;
    func.noiseStrength = 0.5f;
    func.useColorModulation = true;
    func.colorTint = glm::vec3(1.0f, 0.6f, 0.2f);
    func.colorVariation = glm::vec3(0.3f, 0.2f, 0.0f);
    return func;
}

LightMaterialFunction LightMaterialFunctionPresets::CreateFlashlightFlicker() {
    LightMaterialFunction func;
    func.type = LightMaterialFunction::ModulationType::Flicker;
    func.frequency = 30.0f;
    func.amplitude = 1.0f;
    func.noiseStrength = 0.1f;
    return func;
}

LightMaterialFunction LightMaterialFunctionPresets::CreateNeonSign() {
    LightMaterialFunction func;
    func.type = LightMaterialFunction::ModulationType::Neon;
    func.frequency = 1.0f;
    func.amplitude = 1.0f;
    return func;
}

LightMaterialFunction LightMaterialFunctionPresets::CreateStrobe(float frequency) {
    LightMaterialFunction func;
    func.type = LightMaterialFunction::ModulationType::Strobe;
    func.frequency = frequency;
    func.amplitude = 1.0f;
    func.dutyCycle = 0.1f;
    return func;
}

LightMaterialFunction LightMaterialFunctionPresets::CreatePulse(float frequency) {
    LightMaterialFunction func;
    func.type = LightMaterialFunction::ModulationType::Pulse;
    func.frequency = frequency;
    func.amplitude = 1.0f;
    return func;
}

LightMaterialFunction LightMaterialFunctionPresets::CreateBreathe(float frequency) {
    LightMaterialFunction func;
    func.type = LightMaterialFunction::ModulationType::Breathe;
    func.frequency = frequency;
    func.amplitude = 1.0f;
    return func;
}

LightMaterialFunction LightMaterialFunctionPresets::CreateLightning() {
    LightMaterialFunction func;
    func.type = LightMaterialFunction::ModulationType::Flicker;
    func.frequency = 50.0f;
    func.amplitude = 1.0f;
    func.noiseStrength = 0.9f;
    return func;
}

LightMaterialFunction LightMaterialFunctionPresets::CreateGobo(Texture* goboTexture) {
    LightMaterialFunction func;
    func.type = LightMaterialFunction::ModulationType::Gobo;
    func.modulationTexture = goboTexture;
    func.modulationTextureEnabled = true;
    func.amplitude = 1.0f;
    return func;
}

} // namespace Engine
