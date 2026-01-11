#include "PhysicalLight.hpp"
#include "../physics/BlackbodyRadiation.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <cmath>

using json = nlohmann::json;

namespace Engine {

PhysicalLight::PhysicalLight(LightType type)
    : type(type) {
}

PhysicalLight::~PhysicalLight() {
}

glm::vec3 PhysicalLight::Evaluate(const glm::vec3& worldPos, const glm::vec3& normal, float time) const {
    glm::vec3 lightColor = GetColor();
    float lightIntensity = intensity;

    // Get light direction
    glm::vec3 lightDir = GetLightDirection(worldPos);
    float NdotL = glm::max(glm::dot(normal, lightDir), 0.0f);

    // Calculate distance and attenuation
    float distance = glm::length(worldPos - position);
    float attenuation = 1.0f;

    switch (type) {
        case LightType::Point:
            attenuation = GetAttenuation(distance);
            break;

        case LightType::Spot:
            attenuation = GetAttenuation(distance) * GetSpotAttenuation(worldPos);
            break;

        case LightType::Directional:
            // Directional lights don't attenuate
            lightIntensity = illuminance;
            attenuation = 1.0f;
            break;

        case LightType::Area:
            // Area lights need special handling (integration over surface)
            lightIntensity = luminance;
            attenuation = GetAttenuation(distance);
            break;

        case LightType::Line:
            // Line light approximation
            attenuation = GetAttenuation(distance);
            break;

        default:
            break;
    }

    // Apply material function modulation
    float modulation = 1.0f;
    if (type == LightType::Point) {
        modulation = materialFunction.EvaluatePointLight(worldPos, position, time);
    } else if (type == LightType::Spot) {
        modulation = materialFunction.EvaluateSpotLight(worldPos, position, direction, time);
    }

    // Final radiance
    glm::vec3 radiance = lightColor * lightIntensity * attenuation * modulation * NdotL;

    return radiance;
}

glm::vec3 PhysicalLight::GetLightDirection(const glm::vec3& worldPos) const {
    switch (type) {
        case LightType::Directional:
            return -direction;

        default:
            return glm::normalize(position - worldPos);
    }
}

float PhysicalLight::GetAttenuation(float distance) const {
    if (distance > range) {
        return 0.0f;
    }

    if (usePhysicalAttenuation) {
        return CalculatePhysicalAttenuation(distance);
    } else {
        // Classic attenuation formula
        return 1.0f / (constantAttenuation + linearAttenuation * distance + quadraticAttenuation * distance * distance);
    }
}

float PhysicalLight::GetSpotAttenuation(const glm::vec3& worldPos) const {
    if (type != LightType::Spot) {
        return 1.0f;
    }

    glm::vec3 lightToPoint = glm::normalize(worldPos - position);
    return CalculateSpotCone(lightToPoint);
}

glm::vec3 PhysicalLight::GetColor() const {
    if (useTemperature) {
        return BlackbodyRadiation::TemperatureToRGB(temperature) * color;
    }
    return color;
}

float PhysicalLight::GetIntensityInDirection(const glm::vec3& dir) const {
    if (type == LightType::Spot) {
        float cosAngle = glm::dot(glm::normalize(dir), direction);
        float spotFactor = CalculateSpotCone(dir);
        return intensity * spotFactor;
    }

    return intensity;
}

float PhysicalLight::LumensToIntensity() const {
    // For point light: I = Φ / (4π)
    // For area light: depends on geometry
    if (type == LightType::Point) {
        return luminousFlux / (4.0f * glm::pi<float>());
    } else if (type == LightType::Area) {
        float area = 0.0f;
        switch (areaShape) {
            case AreaLightShape::Rectangular:
                area = areaSize.x * areaSize.y;
                break;
            case AreaLightShape::Disk:
                area = glm::pi<float>() * areaRadius * areaRadius;
                break;
            case AreaLightShape::Sphere:
                area = 4.0f * glm::pi<float>() * areaRadius * areaRadius;
                break;
            default:
                area = 1.0f;
        }
        return luminousFlux / (glm::pi<float>() * area);
    }

    return intensity;
}

glm::vec3 PhysicalLight::SampleAreaLight(float u, float v) const {
    switch (areaShape) {
        case AreaLightShape::Rectangular:
            return SampleRectangularLight(u, v);
        case AreaLightShape::Disk:
            return SampleDiskLight(u, v);
        case AreaLightShape::Sphere:
            return SampleSphereLight(u, v);
        default:
            return position;
    }
}

float PhysicalLight::GetAreaLightPDF(const glm::vec3& worldPos) const {
    float area = 0.0f;
    switch (areaShape) {
        case AreaLightShape::Rectangular:
            area = areaSize.x * areaSize.y;
            break;
        case AreaLightShape::Disk:
            area = glm::pi<float>() * areaRadius * areaRadius;
            break;
        case AreaLightShape::Sphere:
            area = 4.0f * glm::pi<float>() * areaRadius * areaRadius;
            break;
        default:
            area = 1.0f;
    }

    return 1.0f / area;
}

float PhysicalLight::CalculatePhysicalAttenuation(float distance) const {
    // Inverse-square law
    if (distance < 0.01f) distance = 0.01f;  // Avoid division by zero
    return 1.0f / (distance * distance);
}

float PhysicalLight::CalculateSpotCone(const glm::vec3& lightToPoint) const {
    float cosOuterCone = std::cos(glm::radians(outerConeAngle));
    float cosInnerCone = std::cos(glm::radians(innerConeAngle));
    float cosAngle = glm::dot(glm::normalize(lightToPoint), direction);

    // Smooth transition between inner and outer cone
    float epsilon = cosInnerCone - cosOuterCone;
    float spotFactor = glm::clamp((cosAngle - cosOuterCone) / epsilon, 0.0f, 1.0f);

    // Apply falloff
    spotFactor = std::pow(spotFactor, spotFalloff);

    return spotFactor;
}

glm::vec3 PhysicalLight::SampleRectangularLight(float u, float v) const {
    float x = (u - 0.5f) * areaSize.x;
    float y = (v - 0.5f) * areaSize.y;

    // Transform to world space (assuming light is axis-aligned for simplicity)
    return position + glm::vec3(x, 0.0f, y);
}

glm::vec3 PhysicalLight::SampleDiskLight(float u, float v) const {
    float angle = u * 2.0f * glm::pi<float>();
    float radius = std::sqrt(v) * areaRadius;

    float x = std::cos(angle) * radius;
    float y = std::sin(angle) * radius;

    return position + glm::vec3(x, 0.0f, y);
}

glm::vec3 PhysicalLight::SampleSphereLight(float u, float v) const {
    float theta = 2.0f * glm::pi<float>() * u;
    float phi = std::acos(2.0f * v - 1.0f);

    float x = areaRadius * std::sin(phi) * std::cos(theta);
    float y = areaRadius * std::sin(phi) * std::sin(theta);
    float z = areaRadius * std::cos(phi);

    return position + glm::vec3(x, y, z);
}

void PhysicalLight::Save(const std::string& filepath) const {
    json j;
    j["type"] = static_cast<int>(type);
    j["position"] = {position.x, position.y, position.z};
    j["direction"] = {direction.x, direction.y, direction.z};
    j["intensity"] = intensity;
    j["temperature"] = temperature;
    j["color"] = {color.x, color.y, color.z};
    j["range"] = range;

    std::ofstream file(filepath);
    file << j.dump(4);
    file.close();
}

void PhysicalLight::Load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return;

    json j;
    file >> j;

    type = static_cast<LightType>(j["type"]);
    position = glm::vec3(j["position"][0], j["position"][1], j["position"][2]);
    direction = glm::vec3(j["direction"][0], j["direction"][1], j["direction"][2]);
    intensity = j["intensity"];
    temperature = j["temperature"];
    color = glm::vec3(j["color"][0], j["color"][1], j["color"][2]);
    range = j["range"];

    file.close();
}

// Preset implementations
PhysicalLight PhysicalLightPresets::CreateIncandescentBulb60W() {
    PhysicalLight light(LightType::Point);
    light.luminousFlux = 800.0f;  // lumens
    light.intensity = light.LumensToIntensity();
    light.useTemperature = true;
    light.temperature = 2700.0f;  // Warm white
    light.range = 10.0f;
    return light;
}

PhysicalLight PhysicalLightPresets::CreateIncandescentBulb100W() {
    PhysicalLight light(LightType::Point);
    light.luminousFlux = 1600.0f;
    light.intensity = light.LumensToIntensity();
    light.useTemperature = true;
    light.temperature = 2850.0f;
    light.range = 15.0f;
    return light;
}

PhysicalLight PhysicalLightPresets::CreateCandle() {
    PhysicalLight light(LightType::Point);
    light.intensity = 12.0f;  // ~1 candela
    light.useTemperature = true;
    light.temperature = 1850.0f;
    light.range = 5.0f;
    light.materialFunction = LightMaterialFunctionPresets::CreateCandle();
    return light;
}

PhysicalLight PhysicalLightPresets::CreateSunlight() {
    PhysicalLight light(LightType::Directional);
    light.illuminance = 100000.0f;  // lux (direct sunlight)
    light.useTemperature = true;
    light.temperature = 5800.0f;
    light.direction = glm::normalize(glm::vec3(0.3f, -1.0f, 0.2f));
    return light;
}

PhysicalLight PhysicalLightPresets::CreateMoonlight() {
    PhysicalLight light(LightType::Directional);
    light.illuminance = 0.25f;  // lux (full moon)
    light.useTemperature = true;
    light.temperature = 4100.0f;
    light.direction = glm::normalize(glm::vec3(0.2f, -1.0f, 0.3f));
    return light;
}

PhysicalLight PhysicalLightPresets::CreateSpotlight() {
    PhysicalLight light(LightType::Spot);
    light.intensity = 10000.0f;  // cd
    light.useTemperature = true;
    light.temperature = 3200.0f;
    light.innerConeAngle = 25.0f;
    light.outerConeAngle = 40.0f;
    light.range = 20.0f;
    return light;
}

PhysicalLight PhysicalLightPresets::CreateNeonTube(const glm::vec3& color) {
    PhysicalLight light(LightType::Line);
    light.intensity = 500.0f;
    light.color = color;
    light.lineLength = 1.0f;
    light.range = 8.0f;
    light.materialFunction = LightMaterialFunctionPresets::CreateNeonSign();
    return light;
}

PhysicalLight PhysicalLightPresets::CreateFireplace() {
    PhysicalLight light(LightType::Area);
    light.areaShape = AreaLightShape::Rectangular;
    light.areaSize = glm::vec2(1.0f, 0.5f);
    light.luminance = 2000.0f;
    light.useTemperature = true;
    light.temperature = 1800.0f;
    light.materialFunction = LightMaterialFunctionPresets::CreateFireplace();
    return light;
}

} // namespace Engine
