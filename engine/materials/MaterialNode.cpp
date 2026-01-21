#include "MaterialNode.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <sstream>

using json = nlohmann::json;

namespace Engine {

int MaterialNode::s_nextPinId = 1;

// MaterialNode base implementation
MaterialNode::MaterialNode(MaterialNodeType type, const std::string& name)
    : type(type), name(name) {
}

void MaterialNode::AddInputPin(const std::string& name, MaterialNodePin::Type type, MaterialNodeValue defaultValue) {
    MaterialNodePin pin;
    pin.id = s_nextPinId++;
    pin.name = name;
    pin.type = type;
    pin.defaultValue = defaultValue;
    inputs[name] = pin;
}

void MaterialNode::AddOutputPin(const std::string& name, MaterialNodePin::Type type) {
    MaterialNodePin pin;
    pin.id = s_nextPinId++;
    pin.name = name;
    pin.type = type;
    outputs[name] = pin;
}

float MaterialNode::GetFloatParam(const std::string& name, float defaultValue) const {
    auto it = floatParams.find(name);
    return (it != floatParams.end()) ? it->second : defaultValue;
}

glm::vec2 MaterialNode::GetVec2Param(const std::string& name, const glm::vec2& defaultValue) const {
    auto it = vec2Params.find(name);
    return (it != vec2Params.end()) ? it->second : defaultValue;
}

glm::vec3 MaterialNode::GetVec3Param(const std::string& name, const glm::vec3& defaultValue) const {
    auto it = vec3Params.find(name);
    return (it != vec3Params.end()) ? it->second : defaultValue;
}

std::string MaterialNode::GetStringParam(const std::string& name, const std::string& defaultValue) const {
    auto it = stringParams.find(name);
    return (it != stringParams.end()) ? it->second : defaultValue;
}

bool MaterialNode::GetBoolParam(const std::string& name, bool defaultValue) const {
    auto it = boolParams.find(name);
    return (it != boolParams.end()) ? it->second : defaultValue;
}

void MaterialNode::Serialize(json& j) const {
    j["id"] = id;
    j["type"] = static_cast<int>(type);
    j["name"] = name;
    j["position"] = {position.x, position.y};
}

void MaterialNode::Deserialize(const json& j) {
    id = j["id"];
    name = j["name"];
    position = glm::vec2(j["position"][0], j["position"][1]);
}

// Helper functions
static std::string GetVec3String(const glm::vec3& v) {
    std::ostringstream oss;
    oss << "vec3(" << v.x << ", " << v.y << ", " << v.z << ")";
    return oss.str();
}

// Infer GLSL type from variable name patterns
// Returns the highest-dimensional type from the inputs, defaulting to float
static std::string InferGLSLType(const std::string& varA, const std::string& varB) {
    // Check for vec4 patterns
    if (varA.find("vec4") != std::string::npos || varB.find("vec4") != std::string::npos) {
        return "vec4";
    }
    // Check for vec3 patterns
    if (varA.find("vec3") != std::string::npos || varB.find("vec3") != std::string::npos) {
        return "vec3";
    }
    // Check for vec2 patterns
    if (varA.find("vec2") != std::string::npos || varB.find("vec2") != std::string::npos) {
        return "vec2";
    }
    // Check for texture sample outputs (which are vec4)
    if (varA.find("_tex") != std::string::npos || varB.find("_tex") != std::string::npos ||
        varA.find("Tex") != std::string::npos || varB.find("Tex") != std::string::npos) {
        return "vec4";
    }
    // Check for common vec3 variable patterns (normals, positions, colors)
    if (varA.find("Normal") != std::string::npos || varB.find("Normal") != std::string::npos ||
        varA.find("Pos") != std::string::npos || varB.find("Pos") != std::string::npos ||
        varA.find("Color") != std::string::npos || varB.find("Color") != std::string::npos ||
        varA.find("RGB") != std::string::npos || varB.find("RGB") != std::string::npos ||
        varA.find("rgb") != std::string::npos || varB.find("rgb") != std::string::npos) {
        return "vec3";
    }
    // Check for common vec2 variable patterns (UVs, texture coordinates)
    if (varA.find("UV") != std::string::npos || varB.find("UV") != std::string::npos ||
        varA.find("TexCoord") != std::string::npos || varB.find("TexCoord") != std::string::npos) {
        return "vec2";
    }
    // Default to float for scalar operations
    return "float";
}

// UV Node
UVNode::UVNode() : MaterialNode(MaterialNodeType::UV, "UV") {
    AddOutputPin("UV", MaterialNodePin::Type::Vec2);
}

MaterialNodeValue UVNode::Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const {
    return glm::vec2(0.0f);  // Runtime evaluation
}

std::string UVNode::GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const {
    return "vec2 " + outputVarName + " = v_TexCoord;\n";
}

// Texture Sample Node
TextureSampleNode::TextureSampleNode() : MaterialNode(MaterialNodeType::TextureSample, "Texture Sample") {
    AddInputPin("UV", MaterialNodePin::Type::Vec2, glm::vec2(0.0f));
    AddOutputPin("RGB", MaterialNodePin::Type::Vec3);
    AddOutputPin("R", MaterialNodePin::Type::Float);
    AddOutputPin("G", MaterialNodePin::Type::Float);
    AddOutputPin("B", MaterialNodePin::Type::Float);
    AddOutputPin("A", MaterialNodePin::Type::Float);

    stringParams["textureName"] = "u_Texture";
}

MaterialNodeValue TextureSampleNode::Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const {
    return glm::vec4(1.0f);  // Runtime evaluation
}

std::string TextureSampleNode::GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const {
    std::string texName = GetStringParam("textureName", "u_Texture");
    auto uvIt = inputVarNames.find("UV");
    std::string uvVar = (uvIt != inputVarNames.end()) ? uvIt->second : "v_TexCoord";

    return "vec4 " + outputVarName + " = texture(" + texName + ", " + uvVar + ");\n";
}

// Add Node
AddNode::AddNode() : MaterialNode(MaterialNodeType::Add, "Add") {
    AddInputPin("A", MaterialNodePin::Type::Any, 0.0f);
    AddInputPin("B", MaterialNodePin::Type::Any, 0.0f);
    AddOutputPin("Result", MaterialNodePin::Type::Any);
}

MaterialNodeValue AddNode::Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const {
    auto aIt = inputValues.find("A");
    auto bIt = inputValues.find("B");

    if (aIt != inputValues.end() && bIt != inputValues.end()) {
        // Handle different type combinations
        if (std::holds_alternative<float>(aIt->second) && std::holds_alternative<float>(bIt->second)) {
            return std::get<float>(aIt->second) + std::get<float>(bIt->second);
        }
        else if (std::holds_alternative<glm::vec3>(aIt->second) && std::holds_alternative<glm::vec3>(bIt->second)) {
            return std::get<glm::vec3>(aIt->second) + std::get<glm::vec3>(bIt->second);
        }
    }

    return 0.0f;
}

std::string AddNode::GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const {
    auto aIt = inputVarNames.find("A");
    auto bIt = inputVarNames.find("B");

    std::string aVar = (aIt != inputVarNames.end()) ? aIt->second : "0.0";
    std::string bVar = (bIt != inputVarNames.end()) ? bIt->second : "0.0";

    std::string glslType = InferGLSLType(aVar, bVar);
    return glslType + " " + outputVarName + " = " + aVar + " + " + bVar + ";\n";
}

// Multiply Node
MultiplyNode::MultiplyNode() : MaterialNode(MaterialNodeType::Multiply, "Multiply") {
    AddInputPin("A", MaterialNodePin::Type::Any, 1.0f);
    AddInputPin("B", MaterialNodePin::Type::Any, 1.0f);
    AddOutputPin("Result", MaterialNodePin::Type::Any);
}

MaterialNodeValue MultiplyNode::Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const {
    auto aIt = inputValues.find("A");
    auto bIt = inputValues.find("B");

    if (aIt != inputValues.end() && bIt != inputValues.end()) {
        if (std::holds_alternative<float>(aIt->second) && std::holds_alternative<float>(bIt->second)) {
            return std::get<float>(aIt->second) * std::get<float>(bIt->second);
        }
        else if (std::holds_alternative<glm::vec3>(aIt->second) && std::holds_alternative<glm::vec3>(bIt->second)) {
            return std::get<glm::vec3>(aIt->second) * std::get<glm::vec3>(bIt->second);
        }
    }

    return 1.0f;
}

std::string MultiplyNode::GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const {
    auto aIt = inputVarNames.find("A");
    auto bIt = inputVarNames.find("B");

    std::string aVar = (aIt != inputVarNames.end()) ? aIt->second : "1.0";
    std::string bVar = (bIt != inputVarNames.end()) ? bIt->second : "1.0";

    std::string glslType = InferGLSLType(aVar, bVar);
    return glslType + " " + outputVarName + " = " + aVar + " * " + bVar + ";\n";
}

// Lerp Node
LerpNode::LerpNode() : MaterialNode(MaterialNodeType::Lerp, "Lerp") {
    AddInputPin("A", MaterialNodePin::Type::Any, 0.0f);
    AddInputPin("B", MaterialNodePin::Type::Any, 1.0f);
    AddInputPin("T", MaterialNodePin::Type::Float, 0.5f);
    AddOutputPin("Result", MaterialNodePin::Type::Any);
}

MaterialNodeValue LerpNode::Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const {
    auto aIt = inputValues.find("A");
    auto bIt = inputValues.find("B");
    auto tIt = inputValues.find("T");

    if (aIt != inputValues.end() && bIt != inputValues.end() && tIt != inputValues.end()) {
        float t = std::get<float>(tIt->second);

        if (std::holds_alternative<float>(aIt->second)) {
            float a = std::get<float>(aIt->second);
            float b = std::get<float>(bIt->second);
            return a + (b - a) * t;
        }
        else if (std::holds_alternative<glm::vec3>(aIt->second)) {
            glm::vec3 a = std::get<glm::vec3>(aIt->second);
            glm::vec3 b = std::get<glm::vec3>(bIt->second);
            return glm::mix(a, b, t);
        }
    }

    return 0.0f;
}

std::string LerpNode::GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const {
    auto aIt = inputVarNames.find("A");
    auto bIt = inputVarNames.find("B");
    auto tIt = inputVarNames.find("T");

    std::string aVar = (aIt != inputVarNames.end()) ? aIt->second : "0.0";
    std::string bVar = (bIt != inputVarNames.end()) ? bIt->second : "1.0";
    std::string tVar = (tIt != inputVarNames.end()) ? tIt->second : "0.5";

    // Infer type from A and B inputs (T is always a float interpolation factor)
    std::string glslType = InferGLSLType(aVar, bVar);
    return glslType + " " + outputVarName + " = mix(" + aVar + ", " + bVar + ", " + tVar + ");\n";
}

// Fresnel Node
FresnelNode::FresnelNode() : MaterialNode(MaterialNodeType::Fresnel, "Fresnel") {
    AddInputPin("Normal", MaterialNodePin::Type::Vec3, glm::vec3(0.0f, 0.0f, 1.0f));
    AddInputPin("ViewDir", MaterialNodePin::Type::Vec3, glm::vec3(0.0f, 0.0f, 1.0f));
    AddInputPin("IOR", MaterialNodePin::Type::Float, 1.5f);
    AddOutputPin("Fresnel", MaterialNodePin::Type::Float);

    floatParams["power"] = 5.0f;
}

MaterialNodeValue FresnelNode::Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const {
    auto normalIt = inputValues.find("Normal");
    auto viewDirIt = inputValues.find("ViewDir");

    if (normalIt != inputValues.end() && viewDirIt != inputValues.end()) {
        glm::vec3 N = std::get<glm::vec3>(normalIt->second);
        glm::vec3 V = std::get<glm::vec3>(viewDirIt->second);

        float NdotV = glm::max(glm::dot(N, V), 0.0f);
        float power = GetFloatParam("power", 5.0f);
        return std::pow(1.0f - NdotV, power);
    }

    return 0.0f;
}

std::string FresnelNode::GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const {
    auto normalIt = inputVarNames.find("Normal");
    auto viewDirIt = inputVarNames.find("ViewDir");

    std::string normalVar = (normalIt != inputVarNames.end()) ? normalIt->second : "v_Normal";
    std::string viewDirVar = (viewDirIt != inputVarNames.end()) ? viewDirIt->second : "normalize(u_CameraPos - v_WorldPos)";

    float power = GetFloatParam("power", 5.0f);

    std::ostringstream oss;
    oss << "float " << outputVarName << " = pow(1.0 - max(dot(" << normalVar << ", " << viewDirVar << "), 0.0), " << power << ");\n";
    return oss.str();
}

// Temperature to RGB Node
TemperatureToRGBNode::TemperatureToRGBNode() : MaterialNode(MaterialNodeType::Temperature_to_RGB, "Temperature to RGB") {
    AddInputPin("Temperature", MaterialNodePin::Type::Float, 6500.0f);
    AddOutputPin("RGB", MaterialNodePin::Type::Vec3);
}

MaterialNodeValue TemperatureToRGBNode::Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const {
    auto tempIt = inputValues.find("Temperature");
    if (tempIt != inputValues.end()) {
        float temperature = std::get<float>(tempIt->second);
        // Mitchell's blackbody approximation
        float temp = temperature / 100.0f;
        float r, g, b;

        if (temp <= 66.0f) {
            r = 1.0f;
            g = glm::clamp(0.39f * std::log(temp) - 0.63f, 0.0f, 1.0f);
        } else {
            r = glm::clamp(1.29f * std::pow(temp - 60.0f, -0.13f), 0.0f, 1.0f);
            g = glm::clamp(1.13f * std::pow(temp - 60.0f, -0.08f), 0.0f, 1.0f);
        }

        if (temp >= 66.0f) {
            b = 1.0f;
        } else if (temp <= 19.0f) {
            b = 0.0f;
        } else {
            b = glm::clamp(0.54f * std::log(temp - 10.0f) - 1.19f, 0.0f, 1.0f);
        }

        return glm::vec3(r, g, b);
    }

    return glm::vec3(1.0f);
}

std::string TemperatureToRGBNode::GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const {
    auto tempIt = inputVarNames.find("Temperature");
    std::string tempVar = (tempIt != inputVarNames.end()) ? tempIt->second : "6500.0";

    std::ostringstream oss;
    oss << "vec3 " << outputVarName << ";\n";
    oss << "{\n";
    oss << "    float temp = " << tempVar << " / 100.0;\n";
    oss << "    float r, g, b;\n";
    oss << "    if (temp <= 66.0) {\n";
    oss << "        r = 1.0;\n";
    oss << "        g = clamp(0.39 * log(temp) - 0.63, 0.0, 1.0);\n";
    oss << "    } else {\n";
    oss << "        r = clamp(1.29 * pow(temp - 60.0, -0.13), 0.0, 1.0);\n";
    oss << "        g = clamp(1.13 * pow(temp - 60.0, -0.08), 0.0, 1.0);\n";
    oss << "    }\n";
    oss << "    if (temp >= 66.0) {\n";
    oss << "        b = 1.0;\n";
    oss << "    } else if (temp <= 19.0) {\n";
    oss << "        b = 0.0;\n";
    oss << "    } else {\n";
    oss << "        b = clamp(0.54 * log(temp - 10.0) - 1.19, 0.0, 1.0);\n";
    oss << "    }\n";
    oss << "    " << outputVarName << " = vec3(r, g, b);\n";
    oss << "}\n";

    return oss.str();
}

// Blackbody Node
BlackbodyNode::BlackbodyNode() : MaterialNode(MaterialNodeType::Blackbody, "Blackbody") {
    AddInputPin("Temperature", MaterialNodePin::Type::Float, 6500.0f);
    AddInputPin("Wavelength", MaterialNodePin::Type::Float, 550.0f);
    AddOutputPin("Radiance", MaterialNodePin::Type::Float);
    AddOutputPin("RGB", MaterialNodePin::Type::Vec3);
}

MaterialNodeValue BlackbodyNode::Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const {
    auto tempIt = inputValues.find("Temperature");
    auto wavelengthIt = inputValues.find("Wavelength");

    if (tempIt != inputValues.end() && wavelengthIt != inputValues.end()) {
        float temperature = std::get<float>(tempIt->second);
        float wavelength_nm = std::get<float>(wavelengthIt->second);

        // Planck's law constants
        const float h = 6.62607015e-34f;
        const float c = 299792458.0f;
        const float k = 1.380649e-23f;

        float lambda = wavelength_nm * 1e-9f;
        float numerator = 2.0f * h * c * c / std::pow(lambda, 5.0f);
        float denominator = std::exp((h * c) / (lambda * k * temperature)) - 1.0f;

        return numerator / denominator;
    }

    return 0.0f;
}

std::string BlackbodyNode::GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const {
    auto tempIt = inputVarNames.find("Temperature");
    std::string tempVar = (tempIt != inputVarNames.end()) ? tempIt->second : "6500.0";

    // For simplicity, output RGB using temperature to RGB conversion
    std::ostringstream oss;
    oss << "vec3 " << outputVarName << "_rgb = temperatureToRGB(" << tempVar << ");\n";
    oss << "float " << outputVarName << "_radiance = luminance(" << outputVarName << "_rgb);\n";

    return oss.str();
}

// Perlin Noise Node
NoisePerlinNode::NoisePerlinNode() : MaterialNode(MaterialNodeType::NoisePerlin, "Perlin Noise") {
    AddInputPin("Position", MaterialNodePin::Type::Vec3, glm::vec3(0.0f));
    AddOutputPin("Noise", MaterialNodePin::Type::Float);

    floatParams["scale"] = 1.0f;
    floatParams["octaves"] = 4.0f;
}

MaterialNodeValue NoisePerlinNode::Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const {
    return 0.5f;  // Simplified for example
}

std::string NoisePerlinNode::GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const {
    auto posIt = inputVarNames.find("Position");
    std::string posVar = (posIt != inputVarNames.end()) ? posIt->second : "v_WorldPos";

    float scale = GetFloatParam("scale", 1.0f);
    int octaves = static_cast<int>(GetFloatParam("octaves", 4.0f));

    std::ostringstream oss;
    oss << "float " << outputVarName << " = perlinNoise(" << posVar << " * " << scale << ", " << octaves << ");\n";

    return oss.str();
}

// GGX BRDF Node
GGX_BRDFNode::GGX_BRDFNode() : MaterialNode(MaterialNodeType::GGX_BRDF, "GGX BRDF") {
    AddInputPin("Normal", MaterialNodePin::Type::Vec3, glm::vec3(0.0f, 0.0f, 1.0f));
    AddInputPin("ViewDir", MaterialNodePin::Type::Vec3, glm::vec3(0.0f, 0.0f, 1.0f));
    AddInputPin("LightDir", MaterialNodePin::Type::Vec3, glm::vec3(0.0f, 0.0f, 1.0f));
    AddInputPin("Roughness", MaterialNodePin::Type::Float, 0.5f);
    AddInputPin("F0", MaterialNodePin::Type::Vec3, glm::vec3(0.04f));
    AddOutputPin("Specular", MaterialNodePin::Type::Vec3);
}

MaterialNodeValue GGX_BRDFNode::Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const {
    return glm::vec3(1.0f);  // Complex calculation, implement in shader
}

std::string GGX_BRDFNode::GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const {
    auto normalIt = inputVarNames.find("Normal");
    auto viewDirIt = inputVarNames.find("ViewDir");
    auto lightDirIt = inputVarNames.find("LightDir");
    auto roughnessIt = inputVarNames.find("Roughness");
    auto f0It = inputVarNames.find("F0");

    std::string normalVar = (normalIt != inputVarNames.end()) ? normalIt->second : "v_Normal";
    std::string viewDirVar = (viewDirIt != inputVarNames.end()) ? viewDirIt->second : "v_ViewDir";
    std::string lightDirVar = (lightDirIt != inputVarNames.end()) ? lightDirIt->second : "v_LightDir";
    std::string roughnessVar = (roughnessIt != inputVarNames.end()) ? roughnessIt->second : "0.5";
    std::string f0Var = (f0It != inputVarNames.end()) ? f0It->second : "vec3(0.04)";

    std::ostringstream oss;
    oss << "vec3 " << outputVarName << " = GGX_BRDF(" << normalVar << ", " << viewDirVar << ", "
        << lightDirVar << ", " << roughnessVar << ", " << f0Var << ");\n";

    return oss.str();
}

// Dispersion Node
DispersionNode::DispersionNode() : MaterialNode(MaterialNodeType::Dispersion, "Dispersion") {
    AddInputPin("IOR", MaterialNodePin::Type::Float, 1.5f);
    AddInputPin("AbbeNumber", MaterialNodePin::Type::Float, 55.0f);
    AddInputPin("Wavelength", MaterialNodePin::Type::Float, 550.0f);
    AddOutputPin("IOR_dispersed", MaterialNodePin::Type::Float);
}

MaterialNodeValue DispersionNode::Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const {
    auto iorIt = inputValues.find("IOR");
    auto abbeIt = inputValues.find("AbbeNumber");
    auto wavelengthIt = inputValues.find("Wavelength");

    if (iorIt != inputValues.end() && abbeIt != inputValues.end() && wavelengthIt != inputValues.end()) {
        float ior = std::get<float>(iorIt->second);
        float abbe = std::get<float>(abbeIt->second);
        float wavelength = std::get<float>(wavelengthIt->second);

        // Approximate dispersion using Abbe number
        const float lambda_d = 587.6f;
        const float lambda_F = 486.1f;
        const float lambda_C = 656.3f;

        float delta_n = (ior - 1.0f) / abbe;
        float wavelength_factor = (wavelength - lambda_d) / (lambda_F - lambda_C);

        return ior + delta_n * wavelength_factor;
    }

    return 1.5f;
}

std::string DispersionNode::GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const {
    auto iorIt = inputVarNames.find("IOR");
    auto abbeIt = inputVarNames.find("AbbeNumber");
    auto wavelengthIt = inputVarNames.find("Wavelength");

    std::string iorVar = (iorIt != inputVarNames.end()) ? iorIt->second : "1.5";
    std::string abbeVar = (abbeIt != inputVarNames.end()) ? abbeIt->second : "55.0";
    std::string wavelengthVar = (wavelengthIt != inputVarNames.end()) ? wavelengthIt->second : "550.0";

    std::ostringstream oss;
    oss << "float " << outputVarName << " = calculateDispersedIOR(" << iorVar << ", "
        << abbeVar << ", " << wavelengthVar << ");\n";

    return oss.str();
}

// RGB to HSV Node
RGB_to_HSVNode::RGB_to_HSVNode() : MaterialNode(MaterialNodeType::RGB_to_HSV, "RGB to HSV") {
    AddInputPin("RGB", MaterialNodePin::Type::Vec3, glm::vec3(1.0f));
    AddOutputPin("HSV", MaterialNodePin::Type::Vec3);
    AddOutputPin("H", MaterialNodePin::Type::Float);
    AddOutputPin("S", MaterialNodePin::Type::Float);
    AddOutputPin("V", MaterialNodePin::Type::Float);
}

MaterialNodeValue RGB_to_HSVNode::Evaluate(const std::map<std::string, MaterialNodeValue>& inputValues) const {
    auto rgbIt = inputValues.find("RGB");
    if (rgbIt != inputValues.end()) {
        glm::vec3 rgb = std::get<glm::vec3>(rgbIt->second);

        float maxC = glm::max(glm::max(rgb.r, rgb.g), rgb.b);
        float minC = glm::min(glm::min(rgb.r, rgb.g), rgb.b);
        float delta = maxC - minC;

        float h = 0.0f;
        if (delta > 0.0f) {
            if (maxC == rgb.r) {
                h = 60.0f * fmod(((rgb.g - rgb.b) / delta), 6.0f);
            } else if (maxC == rgb.g) {
                h = 60.0f * (((rgb.b - rgb.r) / delta) + 2.0f);
            } else {
                h = 60.0f * (((rgb.r - rgb.g) / delta) + 4.0f);
            }
        }

        float s = (maxC > 0.0f) ? (delta / maxC) : 0.0f;
        float v = maxC;

        return glm::vec3(h / 360.0f, s, v);
    }

    return glm::vec3(0.0f);
}

std::string RGB_to_HSVNode::GenerateGLSL(const std::map<std::string, std::string>& inputVarNames, const std::string& outputVarName) const {
    auto rgbIt = inputVarNames.find("RGB");
    std::string rgbVar = (rgbIt != inputVarNames.end()) ? rgbIt->second : "vec3(1.0)";

    return "vec3 " + outputVarName + " = rgbToHsv(" + rgbVar + ");\n";
}

// Node Factory Implementation
std::unique_ptr<MaterialNode> MaterialNodeFactory::CreateNode(MaterialNodeType type) {
    switch (type) {
        case MaterialNodeType::UV:
            return std::make_unique<UVNode>();
        case MaterialNodeType::TextureSample:
            return std::make_unique<TextureSampleNode>();
        case MaterialNodeType::Add:
            return std::make_unique<AddNode>();
        case MaterialNodeType::Multiply:
            return std::make_unique<MultiplyNode>();
        case MaterialNodeType::Lerp:
            return std::make_unique<LerpNode>();
        case MaterialNodeType::Fresnel:
            return std::make_unique<FresnelNode>();
        case MaterialNodeType::Temperature_to_RGB:
            return std::make_unique<TemperatureToRGBNode>();
        case MaterialNodeType::Blackbody:
            return std::make_unique<BlackbodyNode>();
        case MaterialNodeType::NoisePerlin:
            return std::make_unique<NoisePerlinNode>();
        case MaterialNodeType::GGX_BRDF:
            return std::make_unique<GGX_BRDFNode>();
        case MaterialNodeType::Dispersion:
            return std::make_unique<DispersionNode>();
        case MaterialNodeType::RGB_to_HSV:
            return std::make_unique<RGB_to_HSVNode>();

        // Add more node types as needed
        default:
            return nullptr;
    }
}

std::vector<MaterialNodeType> MaterialNodeFactory::GetAllNodeTypes() {
    return {
        MaterialNodeType::UV,
        MaterialNodeType::WorldPos,
        MaterialNodeType::Normal,
        MaterialNodeType::ViewDir,
        MaterialNodeType::Time,
        MaterialNodeType::FloatConstant,
        MaterialNodeType::Vec3Constant,
        MaterialNodeType::Add,
        MaterialNodeType::Subtract,
        MaterialNodeType::Multiply,
        MaterialNodeType::Divide,
        MaterialNodeType::Power,
        MaterialNodeType::Sqrt,
        MaterialNodeType::Sin,
        MaterialNodeType::Cos,
        MaterialNodeType::Lerp,
        MaterialNodeType::Clamp,
        MaterialNodeType::TextureSample,
        MaterialNodeType::NoisePerlin,
        MaterialNodeType::NoiseVoronoi,
        MaterialNodeType::RGB_to_HSV,
        MaterialNodeType::HSV_to_RGB,
        MaterialNodeType::Fresnel,
        MaterialNodeType::Lambert,
        MaterialNodeType::GGX_BRDF,
        MaterialNodeType::IOR_to_Reflectance,
        MaterialNodeType::Temperature_to_RGB,
        MaterialNodeType::Blackbody,
        MaterialNodeType::Dispersion,
        MaterialNodeType::OutputAlbedo,
        MaterialNodeType::OutputNormal,
        MaterialNodeType::OutputRoughness,
        MaterialNodeType::OutputMetallic,
        MaterialNodeType::OutputEmission,
    };
}

std::string MaterialNodeFactory::GetNodeTypeName(MaterialNodeType type) {
    switch (type) {
        case MaterialNodeType::UV: return "UV";
        case MaterialNodeType::WorldPos: return "World Position";
        case MaterialNodeType::Normal: return "Normal";
        case MaterialNodeType::ViewDir: return "View Direction";
        case MaterialNodeType::Time: return "Time";
        case MaterialNodeType::Add: return "Add";
        case MaterialNodeType::Multiply: return "Multiply";
        case MaterialNodeType::Lerp: return "Lerp";
        case MaterialNodeType::TextureSample: return "Texture Sample";
        case MaterialNodeType::Fresnel: return "Fresnel";
        case MaterialNodeType::Temperature_to_RGB: return "Temperature to RGB";
        case MaterialNodeType::Blackbody: return "Blackbody";
        case MaterialNodeType::GGX_BRDF: return "GGX BRDF";
        default: return "Unknown";
    }
}

std::string MaterialNodeFactory::GetNodeCategory(MaterialNodeType type) {
    if (type >= MaterialNodeType::UV && type <= MaterialNodeType::CameraPos)
        return "Input";
    if (type >= MaterialNodeType::FloatConstant && type <= MaterialNodeType::ColorConstant)
        return "Constants";
    if (type >= MaterialNodeType::Add && type <= MaterialNodeType::Sign)
        return "Math";
    if (type >= MaterialNodeType::Sin && type <= MaterialNodeType::Atan2)
        return "Trigonometry";
    if (type >= MaterialNodeType::TextureSample && type <= MaterialNodeType::CubemapSample)
        return "Texture";
    if (type >= MaterialNodeType::NoisePerlin && type <= MaterialNodeType::NoiseTurbulence)
        return "Noise";
    if (type >= MaterialNodeType::RGB_to_HSV && type <= MaterialNodeType::ColorMix)
        return "Color";
    if (type >= MaterialNodeType::Fresnel && type <= MaterialNodeType::SmithG)
        return "Lighting";
    if (type >= MaterialNodeType::IOR_to_Reflectance && type <= MaterialNodeType::Dispersion)
        return "Physics";
    if (type >= MaterialNodeType::OutputAlbedo && type <= MaterialNodeType::OutputAO)
        return "Output";

    return "Other";
}

} // namespace Engine
