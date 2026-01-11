#include "AdvancedMaterial.hpp"
#include "MaterialGraph.hpp"
#include "../physics/BlackbodyRadiation.hpp"
#include "../graphics/Texture.hpp"
#include "../graphics/Shader.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <cmath>

using json = nlohmann::json;

namespace Engine {

// Sellmeier equation implementation
float SellmeierCoefficients::CalculateIOR(float wavelength_nm) const {
    float lambda_um = wavelength_nm / 1000.0f;  // Convert to micrometers
    float lambda2 = lambda_um * lambda_um;

    float n2 = 1.0f +
        (B1 * lambda2) / (lambda2 - C1) +
        (B2 * lambda2) / (lambda2 - C2) +
        (B3 * lambda2) / (lambda2 - C3);

    return std::sqrt(n2);
}

float DispersionProperties::GetIOR(float wavelength_nm, float baseIOR) const {
    if (!enableDispersion) {
        return baseIOR;
    }

    if (abbeNumber > 0.0f) {
        // Approximate dispersion using Abbe number
        // Reference wavelengths: d-line (587.6nm), F-line (486.1nm), C-line (656.3nm)
        const float lambda_d = 587.6f;
        const float lambda_F = 486.1f;
        const float lambda_C = 656.3f;

        // Calculate dispersion spread
        float V_d = abbeNumber;
        float n_d = baseIOR;
        float delta_n = (n_d - 1.0f) / V_d;

        // Approximate IOR at wavelength
        float wavelength_factor = (wavelength_nm - lambda_d) / (lambda_F - lambda_C);
        return n_d + delta_n * wavelength_factor;
    }

    // Use Sellmeier equation for more accurate dispersion
    return sellmeier.CalculateIOR(wavelength_nm);
}

// AdvancedMaterial implementation
AdvancedMaterial::AdvancedMaterial() {
    InitializeDefaultShader();
}

AdvancedMaterial::~AdvancedMaterial() {
}

void AdvancedMaterial::SetAlbedo(const glm::vec3& color) {
    albedo = glm::clamp(color, 0.0f, 1.0f);
}

void AdvancedMaterial::SetMetallic(float value) {
    metallic = glm::clamp(value, 0.0f, 1.0f);
    CalculateF0();
}

void AdvancedMaterial::SetRoughness(float value) {
    roughness = glm::clamp(value, 0.01f, 1.0f);  // Prevent zero roughness
}

void AdvancedMaterial::SetIOR(float value) {
    ior = glm::max(value, 1.0f);  // IOR must be >= 1
    CalculateF0();
}

void AdvancedMaterial::SetEmission(const glm::vec3& color, float strength) {
    emission.enabled = true;
    emission.useBlackbody = false;
    emission.emissionColor = color;
    emission.emissionStrength = strength;
}

void AdvancedMaterial::SetTemperature(float kelvin) {
    emission.enabled = true;
    emission.useBlackbody = true;
    emission.temperature = glm::clamp(kelvin, 1000.0f, 40000.0f);
}

glm::vec3 AdvancedMaterial::GetF0() const {
    if (metallic > 0.5f) {
        // Metallic materials use albedo as F0
        return glm::mix(f0, albedo, metallic);
    } else {
        // Dielectric materials calculate F0 from IOR
        float F0_scalar = std::pow((ior - 1.0f) / (ior + 1.0f), 2.0f);
        return glm::vec3(F0_scalar);
    }
}

float AdvancedMaterial::GetIORAtWavelength(float wavelength_nm) const {
    return dispersion.GetIOR(wavelength_nm, ior);
}

glm::vec3 AdvancedMaterial::GetEmissionColor(float time) const {
    if (!emission.enabled) {
        return glm::vec3(0.0f);
    }

    glm::vec3 color;

    if (emission.useBlackbody) {
        color = BlackbodyRadiation::TemperatureToRGB(emission.temperature);
    } else {
        color = emission.emissionColor;
    }

    return color * emission.emissionStrength;
}

float AdvancedMaterial::GetEmissionIntensity() const {
    if (!emission.enabled) {
        return 0.0f;
    }

    if (emission.luminosity > 0.0f) {
        return emission.luminosity;
    }

    if (emission.useBlackbody) {
        // Calculate luminous efficacy from temperature
        return BlackbodyRadiation::LuminousEfficacy(emission.temperature);
    }

    return emission.emissionStrength;
}

void AdvancedMaterial::SetMaterialGraph(std::shared_ptr<MaterialGraph> graph) {
    materialGraph = graph;
    useGraphShader = (graph != nullptr);
}

void AdvancedMaterial::CompileGraphToShader() {
    if (!materialGraph) {
        return;
    }

    std::string shaderCode = materialGraph->CompileToGLSL();

    // Save to file
    std::ofstream file(shaderPath);
    if (file.is_open()) {
        file << shaderCode;
        file.close();

        // Reload shader
        if (shader) {
            shader->Reload();
        }
    }
}

void AdvancedMaterial::SetAlbedoMap(Texture* texture) {
    albedoMap = texture;
}

void AdvancedMaterial::SetNormalMap(Texture* texture) {
    normalMap = texture;
}

void AdvancedMaterial::SetMetallicMap(Texture* texture) {
    metallicMap = texture;
}

void AdvancedMaterial::SetRoughnessMap(Texture* texture) {
    roughnessMap = texture;
}

void AdvancedMaterial::SetEmissionMap(Texture* texture) {
    emission.emissionMap = texture;
}

void AdvancedMaterial::SetIORMap(Texture* texture) {
    iorMap = texture;
}

void AdvancedMaterial::Save(const std::string& filepath) const {
    json j;

    // Basic properties
    j["name"] = name;
    j["albedo"] = {albedo.x, albedo.y, albedo.z};
    j["metallic"] = metallic;
    j["roughness"] = roughness;
    j["specular"] = specular;
    j["ior"] = ior;
    j["transmission"] = transmission;
    j["thickness"] = thickness;

    // Dispersion
    j["dispersion"]["enabled"] = dispersion.enableDispersion;
    j["dispersion"]["abbeNumber"] = dispersion.abbeNumber;

    // Subsurface scattering
    j["subsurface"]["enabled"] = subsurface.enabled;
    j["subsurface"]["radius"] = subsurface.radius;
    j["subsurface"]["color"] = {subsurface.color.x, subsurface.color.y, subsurface.color.z};
    j["subsurface"]["density"] = subsurface.scatteringDensity;

    // Emission
    j["emission"]["enabled"] = emission.enabled;
    j["emission"]["useBlackbody"] = emission.useBlackbody;
    j["emission"]["temperature"] = emission.temperature;
    j["emission"]["luminosity"] = emission.luminosity;
    j["emission"]["color"] = {emission.emissionColor.x, emission.emissionColor.y, emission.emissionColor.z};
    j["emission"]["strength"] = emission.emissionStrength;

    // Fluorescence
    j["fluorescence"]["enabled"] = fluorescence.enabled;
    j["fluorescence"]["strength"] = fluorescence.strength;
    j["fluorescence"]["absorptionWavelength"] = fluorescence.absorptionWavelength;
    j["fluorescence"]["emissionWavelength"] = fluorescence.emissionWavelength;

    // Anisotropic
    j["anisotropic"]["enabled"] = anisotropic.enabled;
    j["anisotropic"]["strength"] = anisotropic.anisotropicStrength;

    // Clear coat
    j["clearCoat"]["enabled"] = clearCoat.enabled;
    j["clearCoat"]["strength"] = clearCoat.strength;
    j["clearCoat"]["roughness"] = clearCoat.roughness;

    // Sheen
    j["sheen"]["enabled"] = sheen.enabled;
    j["sheen"]["strength"] = sheen.strength;
    j["sheen"]["color"] = {sheen.color.x, sheen.color.y, sheen.color.z};

    // Write to file
    std::ofstream file(filepath);
    file << j.dump(4);
    file.close();
}

void AdvancedMaterial::Load(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return;
    }

    json j;
    file >> j;

    // Basic properties
    if (j.contains("name")) name = j["name"];
    if (j.contains("albedo")) {
        albedo = glm::vec3(j["albedo"][0], j["albedo"][1], j["albedo"][2]);
    }
    if (j.contains("metallic")) metallic = j["metallic"];
    if (j.contains("roughness")) roughness = j["roughness"];
    if (j.contains("ior")) ior = j["ior"];

    // Dispersion
    if (j.contains("dispersion")) {
        dispersion.enableDispersion = j["dispersion"]["enabled"];
        dispersion.abbeNumber = j["dispersion"]["abbeNumber"];
    }

    // Emission
    if (j.contains("emission")) {
        emission.enabled = j["emission"]["enabled"];
        emission.useBlackbody = j["emission"]["useBlackbody"];
        emission.temperature = j["emission"]["temperature"];
        emission.luminosity = j["emission"]["luminosity"];
        emission.emissionStrength = j["emission"]["strength"];
    }

    file.close();
}

// Material presets
AdvancedMaterial AdvancedMaterial::CreateGlass(float ior_value) {
    AdvancedMaterial mat;
    mat.name = "Glass";
    mat.albedo = glm::vec3(1.0f);
    mat.metallic = 0.0f;
    mat.roughness = 0.05f;
    mat.ior = ior_value;
    mat.transmission = 1.0f;
    mat.dispersion.enableDispersion = true;
    mat.dispersion.abbeNumber = 60.0f;  // Low dispersion for window glass
    return mat;
}

AdvancedMaterial AdvancedMaterial::CreateWater() {
    AdvancedMaterial mat;
    mat.name = "Water";
    mat.albedo = glm::vec3(0.8f, 0.9f, 1.0f);
    mat.metallic = 0.0f;
    mat.roughness = 0.02f;
    mat.ior = 1.333f;
    mat.transmission = 0.95f;
    mat.transmittanceColor = glm::vec3(0.7f, 0.85f, 0.95f);
    mat.volumetric.type = VolumetricScatteringProperties::ScatteringType::Rayleigh;
    mat.volumetric.density = 0.1f;
    return mat;
}

AdvancedMaterial AdvancedMaterial::CreateGold() {
    AdvancedMaterial mat;
    mat.name = "Gold";
    mat.albedo = glm::vec3(1.0f, 0.766f, 0.336f);
    mat.metallic = 1.0f;
    mat.roughness = 0.2f;
    mat.f0 = glm::vec3(1.0f, 0.86f, 0.57f);
    return mat;
}

AdvancedMaterial AdvancedMaterial::CreateCopper() {
    AdvancedMaterial mat;
    mat.name = "Copper";
    mat.albedo = glm::vec3(0.955f, 0.638f, 0.538f);
    mat.metallic = 1.0f;
    mat.roughness = 0.25f;
    mat.f0 = glm::vec3(0.955f, 0.638f, 0.538f);
    return mat;
}

AdvancedMaterial AdvancedMaterial::CreateDiamond() {
    AdvancedMaterial mat;
    mat.name = "Diamond";
    mat.albedo = glm::vec3(1.0f);
    mat.metallic = 0.0f;
    mat.roughness = 0.01f;
    mat.ior = 2.417f;
    mat.transmission = 1.0f;
    mat.dispersion.enableDispersion = true;
    mat.dispersion.abbeNumber = 55.3f;  // High dispersion

    // Sellmeier coefficients for diamond
    mat.dispersion.sellmeier.B1 = 0.3306f;
    mat.dispersion.sellmeier.B2 = 4.3356f;
    mat.dispersion.sellmeier.B3 = 0.0f;
    mat.dispersion.sellmeier.C1 = 0.1750f * 0.1750f;
    mat.dispersion.sellmeier.C2 = 0.1060f * 0.1060f;
    mat.dispersion.sellmeier.C3 = 0.0f;

    return mat;
}

AdvancedMaterial AdvancedMaterial::CreatePlastic(const glm::vec3& color) {
    AdvancedMaterial mat;
    mat.name = "Plastic";
    mat.albedo = color;
    mat.metallic = 0.0f;
    mat.roughness = 0.4f;
    mat.ior = 1.46f;
    mat.clearCoat.enabled = true;
    mat.clearCoat.strength = 0.5f;
    mat.clearCoat.roughness = 0.1f;
    return mat;
}

AdvancedMaterial AdvancedMaterial::CreateSkin() {
    AdvancedMaterial mat;
    mat.name = "Skin";
    mat.albedo = glm::vec3(0.95f, 0.75f, 0.65f);
    mat.metallic = 0.0f;
    mat.roughness = 0.6f;

    // Subsurface scattering
    mat.subsurface.enabled = true;
    mat.subsurface.radius = 3.0f;  // 3mm
    mat.subsurface.color = glm::vec3(0.8f, 0.3f, 0.2f);
    mat.subsurface.scatteringDensity = 0.7f;
    mat.subsurface.albedo = glm::vec3(0.9f, 0.6f, 0.5f);

    return mat;
}

AdvancedMaterial AdvancedMaterial::CreateMarble() {
    AdvancedMaterial mat;
    mat.name = "Marble";
    mat.albedo = glm::vec3(0.95f);
    mat.metallic = 0.0f;
    mat.roughness = 0.3f;

    // Subsurface scattering for translucency
    mat.subsurface.enabled = true;
    mat.subsurface.radius = 5.0f;
    mat.subsurface.color = glm::vec3(0.9f, 0.9f, 0.85f);
    mat.subsurface.scatteringDensity = 0.3f;

    return mat;
}

AdvancedMaterial AdvancedMaterial::CreateWax(float temperature) {
    AdvancedMaterial mat;
    mat.name = "Wax";
    mat.albedo = glm::vec3(0.98f, 0.95f, 0.85f);
    mat.metallic = 0.0f;
    mat.roughness = 0.3f;

    // Subsurface scattering
    mat.subsurface.enabled = true;
    mat.subsurface.radius = 2.0f;
    mat.subsurface.color = glm::vec3(1.0f, 0.9f, 0.7f);
    mat.subsurface.scatteringDensity = 0.5f;

    // Emission (candle flame)
    if (temperature > 1000.0f) {
        mat.emission.enabled = true;
        mat.emission.useBlackbody = true;
        mat.emission.temperature = temperature;
        mat.emission.luminosity = 1.0f;
    }

    return mat;
}

AdvancedMaterial AdvancedMaterial::CreateEmissive(float temperature, float luminosity) {
    AdvancedMaterial mat;
    mat.name = "Emissive";
    mat.albedo = glm::vec3(0.1f);
    mat.metallic = 0.0f;
    mat.roughness = 0.5f;

    mat.emission.enabled = true;
    mat.emission.useBlackbody = true;
    mat.emission.temperature = temperature;
    mat.emission.luminosity = luminosity;

    return mat;
}

AdvancedMaterial AdvancedMaterial::CreateNeon(const glm::vec3& color) {
    AdvancedMaterial mat;
    mat.name = "Neon";
    mat.albedo = color * 0.1f;
    mat.metallic = 0.0f;
    mat.roughness = 0.3f;

    mat.emission.enabled = true;
    mat.emission.useBlackbody = false;
    mat.emission.emissionColor = color;
    mat.emission.emissionStrength = 10.0f;
    mat.emission.luminosity = 1000.0f;  // cd/m²

    // Slight volumetric glow
    mat.volumetric.type = VolumetricScatteringProperties::ScatteringType::Mie;
    mat.volumetric.density = 0.2f;
    mat.volumetric.scatteringCoefficient = color * 0.5f;

    return mat;
}

AdvancedMaterial AdvancedMaterial::CreateVelvet(const glm::vec3& color) {
    AdvancedMaterial mat;
    mat.name = "Velvet";
    mat.albedo = color;
    mat.metallic = 0.0f;
    mat.roughness = 0.8f;

    // Sheen for fabric appearance
    mat.sheen.enabled = true;
    mat.sheen.strength = 0.7f;
    mat.sheen.color = color * 1.2f;
    mat.sheen.roughness = 0.6f;

    return mat;
}

AdvancedMaterial AdvancedMaterial::CreateCarPaint(const glm::vec3& color) {
    AdvancedMaterial mat;
    mat.name = "Car Paint";
    mat.albedo = color;
    mat.metallic = 0.8f;
    mat.roughness = 0.3f;

    // Clear coat layer
    mat.clearCoat.enabled = true;
    mat.clearCoat.strength = 1.0f;
    mat.clearCoat.roughness = 0.05f;
    mat.clearCoat.ior = 1.5f;

    return mat;
}

bool AdvancedMaterial::Validate() const {
    m_validationErrors.clear();

    if (roughness < 0.01f || roughness > 1.0f) {
        m_validationErrors.push_back("Roughness out of range [0.01, 1.0]");
    }

    if (metallic < 0.0f || metallic > 1.0f) {
        m_validationErrors.push_back("Metallic out of range [0.0, 1.0]");
    }

    if (ior < 1.0f) {
        m_validationErrors.push_back("IOR must be >= 1.0");
    }

    if (emission.enabled && emission.useBlackbody) {
        if (emission.temperature < 1000.0f || emission.temperature > 40000.0f) {
            m_validationErrors.push_back("Temperature out of range [1000, 40000] K");
        }
    }

    return m_validationErrors.empty();
}

std::vector<std::string> AdvancedMaterial::GetValidationErrors() const {
    return m_validationErrors;
}

void AdvancedMaterial::InitializeDefaultShader() {
    // Initialize with default PBR shader
    shaderPath = "assets/shaders/advanced_material.frag";
}

void AdvancedMaterial::CalculateF0() {
    f0 = GetF0();
}

void AdvancedMaterial::UpdateShaderUniforms() {
    if (!shader) {
        return;
    }

    shader->Use();
    shader->SetVec3("material.albedo", albedo);
    shader->SetFloat("material.metallic", metallic);
    shader->SetFloat("material.roughness", roughness);
    shader->SetFloat("material.ior", ior);
    shader->SetVec3("material.f0", GetF0());

    // Emission
    shader->SetBool("material.emission.enabled", emission.enabled);
    shader->SetVec3("material.emission.color", GetEmissionColor());
    shader->SetFloat("material.emission.intensity", GetEmissionIntensity());

    // Subsurface scattering
    shader->SetBool("material.subsurface.enabled", subsurface.enabled);
    shader->SetFloat("material.subsurface.radius", subsurface.radius);
    shader->SetVec3("material.subsurface.color", subsurface.color);
}

// MaterialLibrary implementation
MaterialLibrary& MaterialLibrary::GetInstance() {
    static MaterialLibrary instance;
    return instance;
}

void MaterialLibrary::AddMaterial(const std::string& name, std::shared_ptr<AdvancedMaterial> material) {
    m_materials[name] = material;
}

std::shared_ptr<AdvancedMaterial> MaterialLibrary::GetMaterial(const std::string& name) const {
    auto it = m_materials.find(name);
    if (it != m_materials.end()) {
        return it->second;
    }
    return nullptr;
}

void MaterialLibrary::RemoveMaterial(const std::string& name) {
    m_materials.erase(name);
}

std::vector<std::string> MaterialLibrary::GetMaterialNames() const {
    std::vector<std::string> names;
    for (const auto& [name, material] : m_materials) {
        names.push_back(name);
    }
    return names;
}

void MaterialLibrary::Clear() {
    m_materials.clear();
}

void MaterialLibrary::LoadLibrary(const std::string& filepath) {
    // Load material library from JSON
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return;
    }

    json j;
    file >> j;

    for (const auto& item : j["materials"]) {
        auto material = std::make_shared<AdvancedMaterial>();
        // Load material properties from JSON
        AddMaterial(item["name"], material);
    }

    file.close();
}

void MaterialLibrary::SaveLibrary(const std::string& filepath) const {
    json j;
    j["materials"] = json::array();

    for (const auto& [name, material] : m_materials) {
        json mat;
        mat["name"] = name;
        // Save material properties
        j["materials"].push_back(mat);
    }

    std::ofstream file(filepath);
    file << j.dump(4);
    file.close();
}

void MaterialLibrary::LoadDefaultPresets() {
    AddMaterial("Glass", std::make_shared<AdvancedMaterial>(AdvancedMaterial::CreateGlass()));
    AddMaterial("Water", std::make_shared<AdvancedMaterial>(AdvancedMaterial::CreateWater()));
    AddMaterial("Gold", std::make_shared<AdvancedMaterial>(AdvancedMaterial::CreateGold()));
    AddMaterial("Diamond", std::make_shared<AdvancedMaterial>(AdvancedMaterial::CreateDiamond()));
    AddMaterial("Plastic", std::make_shared<AdvancedMaterial>(AdvancedMaterial::CreatePlastic(glm::vec3(1.0f))));
    AddMaterial("Skin", std::make_shared<AdvancedMaterial>(AdvancedMaterial::CreateSkin()));
}

} // namespace Engine
