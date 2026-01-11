#include "NewWorldDialog.hpp"
#include "WorldTemplateLibrary.hpp"
#include <imgui.h>
#include <random>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace Vehement {

// =============================================================================
// WorldCreationParams Implementation
// =============================================================================

glm::ivec2 WorldCreationParams::GetWorldSize() const {
    if (sizePreset == WorldSizePreset::Custom) {
        return customSize;
    }
    return WorldCreationUtils::PresetToSize(sizePreset);
}

void WorldCreationParams::GenerateRandomSeed() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 999999);
    seed = dis(gen);
}

// =============================================================================
// NewWorldDialog Implementation
// =============================================================================

NewWorldDialog::NewWorldDialog()
    : m_isVisible(false)
{
    m_params.GenerateRandomSeed();
    std::snprintf(m_seedBuffer, sizeof(m_seedBuffer), "%d", m_params.seed);
}

NewWorldDialog::~NewWorldDialog() = default;

void NewWorldDialog::Show() {
    if (!m_isVisible) {
        m_isVisible = true;
        RefreshTemplateList();
        Reset();
    }
}

void NewWorldDialog::Hide() {
    m_isVisible = false;
}

void NewWorldDialog::Reset() {
    m_params = WorldCreationParams();
    m_params.GenerateRandomSeed();
    std::snprintf(m_seedBuffer, sizeof(m_seedBuffer), "%d", m_params.seed);
    std::snprintf(m_worldNameBuffer, sizeof(m_worldNameBuffer), "New World");
    m_selectedTemplateIndex = 0;
    m_showAdvancedSettings = false;
    m_hasValidationError = false;
    m_validationError.clear();
    LoadSelectedTemplate();
}

void NewWorldDialog::Render() {
    if (!m_isVisible) return;

    ImGui::SetNextWindowSize(ImVec2(800, 700), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, ImGui::GetIO().DisplaySize.y * 0.5f),
                            ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));

    if (ImGui::Begin("New World", &m_isVisible, ImGuiWindowFlags_NoCollapse)) {
        RenderTemplateSelection();
        ImGui::Separator();

        ImGui::BeginChild("WorldSettings", ImVec2(0, -50), true);
        {
            RenderTemplateInfo();
            ImGui::Spacing();
            RenderTemplatePreview();
            ImGui::Spacing();
            RenderBasicSettings();
            ImGui::Spacing();
            RenderWorldSizeSelection();
            ImGui::Spacing();
            RenderAdvancedSettings();
        }
        ImGui::EndChild();

        ImGui::Separator();
        RenderActionButtons();
    }
    ImGui::End();
}

void NewWorldDialog::RefreshTemplateList() {
    m_availableTemplateIds.clear();
    if (m_templateLibrary) {
        m_availableTemplateIds = m_templateLibrary->GetAllTemplateIds();
    }

    if (m_selectedTemplateIndex >= static_cast<int>(m_availableTemplateIds.size())) {
        m_selectedTemplateIndex = 0;
    }

    LoadSelectedTemplate();
}

void NewWorldDialog::LoadSelectedTemplate() {
    if (!m_templateLibrary || m_availableTemplateIds.empty()) {
        m_currentTemplate = nullptr;
        return;
    }

    if (m_selectedTemplateIndex >= 0 && m_selectedTemplateIndex < static_cast<int>(m_availableTemplateIds.size())) {
        m_params.templateId = m_availableTemplateIds[m_selectedTemplateIndex];
        m_currentTemplate = m_templateLibrary->GetTemplate(m_params.templateId);
    }
}

void NewWorldDialog::RenderTemplateSelection() {
    ImGui::Text("Select World Template");
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Choose a pre-configured template to generate your world");
    }

    if (m_availableTemplateIds.empty()) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No templates available. Loading templates...");
        return;
    }

    std::vector<const char*> templateNames;
    for (const auto& id : m_availableTemplateIds) {
        templateNames.push_back(id.c_str());
    }

    if (ImGui::Combo("##Template", &m_selectedTemplateIndex, templateNames.data(),
                     static_cast<int>(templateNames.size()))) {
        LoadSelectedTemplate();
    }
}

void NewWorldDialog::RenderTemplateInfo() {
    if (!m_currentTemplate) {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No template selected");
        return;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
    ImGui::Text("%s", m_currentTemplate->name.c_str());
    ImGui::PopStyleColor();

    ImGui::TextWrapped("%s", m_currentTemplate->description.c_str());
    ImGui::Spacing();

    // Display biomes
    if (!m_currentTemplate->biomes.empty()) {
        ImGui::Text("Biomes:");
        ImGui::SameLine();
        for (size_t i = 0; i < m_currentTemplate->biomes.size() && i < 5; ++i) {
            if (i > 0) ImGui::SameLine();
            ImGui::TextColored(
                ImVec4(m_currentTemplate->biomes[i].color.r,
                       m_currentTemplate->biomes[i].color.g,
                       m_currentTemplate->biomes[i].color.b, 1.0f),
                "%s", m_currentTemplate->biomes[i].name.c_str());
            if (i < m_currentTemplate->biomes.size() - 1) {
                ImGui::SameLine();
                ImGui::Text(",");
            }
        }
        if (m_currentTemplate->biomes.size() > 5) {
            ImGui::SameLine();
            ImGui::Text("... (+%zu more)", m_currentTemplate->biomes.size() - 5);
        }
    }

    // Display features
    ImGui::Text("Features:");
    ImGui::BulletText("%zu resource types", m_currentTemplate->ores.size());
    ImGui::BulletText("%zu structure types",
                      m_currentTemplate->ruins.size() +
                      m_currentTemplate->ancients.size() +
                      m_currentTemplate->buildings.size());
    ImGui::BulletText("Erosion: %d iterations", m_currentTemplate->erosionIterations);

    // Display tags
    if (!m_currentTemplate->tags.empty()) {
        ImGui::Text("Tags:");
        ImGui::SameLine();
        for (size_t i = 0; i < m_currentTemplate->tags.size(); ++i) {
            if (i > 0) ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "[%s]", m_currentTemplate->tags[i].c_str());
        }
    }
}

void NewWorldDialog::RenderTemplatePreview() {
    // For now, just show a placeholder
    // In a full implementation, this would display a generated thumbnail image
    ImGui::BeginChild("Preview", ImVec2(256, 256), true);
    {
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImDrawList* drawList = ImGui::GetWindowDrawList();

        // Draw placeholder gradient
        drawList->AddRectFilledMultiColor(
            pos, ImVec2(pos.x + 256, pos.y + 256),
            IM_COL32(50, 100, 150, 255),   // Top-left
            IM_COL32(100, 150, 200, 255),  // Top-right
            IM_COL32(150, 200, 100, 255),  // Bottom-right
            IM_COL32(100, 150, 50, 255)    // Bottom-left
        );

        // Draw text overlay
        ImVec2 textPos = ImVec2(pos.x + 128 - 50, pos.y + 128 - 10);
        drawList->AddText(textPos, IM_COL32(255, 255, 255, 200), "Preview Coming Soon");
    }
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("PreviewInfo", ImVec2(0, 256));
    {
        if (m_currentTemplate) {
            ImGui::Text("Template Details:");
            ImGui::Separator();
            ImGui::Text("Version: %s", m_currentTemplate->version.c_str());
            ImGui::Text("Default Seed: %d", m_currentTemplate->seed);
            ImGui::Text("Default Size: %dx%d",
                       m_currentTemplate->worldSize.x,
                       m_currentTemplate->worldSize.y);
            ImGui::Text("Max Height: %d", m_currentTemplate->maxHeight);

            if (!m_currentTemplate->author.empty()) {
                ImGui::Spacing();
                ImGui::Text("Author: %s", m_currentTemplate->author.c_str());
            }
        }
    }
    ImGui::EndChild();
}

void NewWorldDialog::RenderBasicSettings() {
    ImGui::Text("Basic Settings");
    ImGui::Separator();

    // World name
    ImGui::InputText("World Name", m_worldNameBuffer, sizeof(m_worldNameBuffer));
    m_params.worldName = m_worldNameBuffer;

    // Seed
    ImGui::InputText("Seed", m_seedBuffer, sizeof(m_seedBuffer));
    ImGui::SameLine();
    if (ImGui::Button("Random")) {
        m_params.GenerateRandomSeed();
        std::snprintf(m_seedBuffer, sizeof(m_seedBuffer), "%d", m_params.seed);
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Random seed controls world generation. Same seed = same world");
    }

    // Parse seed from buffer
    m_params.seed = WorldCreationUtils::ParseSeed(m_seedBuffer);
}

void NewWorldDialog::RenderWorldSizeSelection() {
    ImGui::Text("World Size");
    ImGui::Separator();

    const char* sizePresets[] = { "Small (2500x2500m)", "Medium (5000x5000m)",
                                  "Large (10000x10000m)", "Huge (20000x20000m)", "Custom" };
    int currentPreset = static_cast<int>(m_params.sizePreset);

    if (ImGui::Combo("Size Preset", &currentPreset, sizePresets, IM_ARRAYSIZE(sizePresets))) {
        m_params.sizePreset = static_cast<WorldSizePreset>(currentPreset);
    }

    if (m_params.sizePreset == WorldSizePreset::Custom) {
        ImGui::InputInt2("Custom Size (m)", &m_params.customSize.x);
        m_params.customSize = glm::max(m_params.customSize, glm::ivec2(100, 100));
    }

    glm::ivec2 actualSize = m_params.GetWorldSize();
    ImGui::Text("Actual size: %dx%d meters", actualSize.x, actualSize.y);

    // Estimate chunk count
    int chunkSize = 64; // Default chunk size
    int chunkCountX = (actualSize.x + chunkSize - 1) / chunkSize;
    int chunkCountY = (actualSize.y + chunkSize - 1) / chunkSize;
    ImGui::Text("Approximate chunks: %d (%dx%d)", chunkCountX * chunkCountY, chunkCountX, chunkCountY);
}

void NewWorldDialog::RenderAdvancedSettings() {
    if (ImGui::CollapsingHeader("Advanced Settings", &m_showAdvancedSettings)) {
        // Erosion
        ImGui::SliderInt("Erosion Iterations", &m_params.erosionIterations, 0, 500);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("More iterations = more realistic erosion (slower generation)");
        }

        // Resource density
        ImGui::SliderFloat("Resource Density", &m_params.resourceDensity, 0.1f, 3.0f, "%.1fx");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Multiplier for resource placement density");
        }

        // Structure density
        ImGui::SliderFloat("Structure Density", &m_params.structureDensity, 0.1f, 3.0f, "%.1fx");
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Multiplier for structure placement density");
        }

        // Terrain roughness
        ImGui::SliderFloat("Terrain Roughness", &m_params.terrainRoughness, 0.1f, 2.0f, "%.1fx");

        // Water level
        ImGui::SliderFloat("Water Level", &m_params.waterLevel, -10.0f, 50.0f, "%.1f m");

        // Real-world data
        ImGui::Checkbox("Use Real-World Data", &m_params.useRealWorldData);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Use real-world terrain data if available (experimental)");
        }

        // Generate immediately
        ImGui::Checkbox("Generate Immediately", &m_params.generateImmediately);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Generate all chunks now (vs. streaming on-demand)");
        }
    }
}

void NewWorldDialog::RenderActionButtons() {
    // Validation error display
    if (m_hasValidationError) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::TextWrapped("Error: %s", m_validationError.c_str());
        ImGui::PopStyleColor();
    }

    float buttonWidth = 150.0f;
    float spacing = 10.0f;
    float totalWidth = buttonWidth * 3 + spacing * 2;
    float offset = (ImGui::GetContentRegionAvail().x - totalWidth) * 0.5f;

    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset);

    // Create Empty button
    if (ImGui::Button("Create Empty", ImVec2(buttonWidth, 0))) {
        CreateEmptyWorld();
    }
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Create a flat, empty world without using a template");
    }

    ImGui::SameLine(0, spacing);

    // Cancel button
    if (ImGui::Button("Cancel", ImVec2(buttonWidth, 0))) {
        Hide();
    }

    ImGui::SameLine(0, spacing);

    // Create World button
    bool canCreate = m_currentTemplate != nullptr;
    if (!canCreate) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
    }

    if (ImGui::Button("Create World", ImVec2(buttonWidth, 0)) && canCreate) {
        if (ValidateParams()) {
            CreateWorld();
        }
    }

    if (!canCreate) {
        ImGui::PopStyleVar();
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Please select a template first");
        }
    } else if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Create world from selected template");
    }
}

bool NewWorldDialog::ValidateParams() {
    m_hasValidationError = false;
    m_validationError.clear();

    // Validate world name
    if (m_params.worldName.empty()) {
        m_hasValidationError = true;
        m_validationError = "World name cannot be empty";
        return false;
    }

    // Validate world size
    glm::ivec2 size = m_params.GetWorldSize();
    if (size.x < 100 || size.y < 100) {
        m_hasValidationError = true;
        m_validationError = "World size must be at least 100x100 meters";
        return false;
    }

    if (size.x > 100000 || size.y > 100000) {
        m_hasValidationError = true;
        m_validationError = "World size cannot exceed 100000x100000 meters";
        return false;
    }

    // Validate template
    if (!m_currentTemplate) {
        m_hasValidationError = true;
        m_validationError = "No template selected";
        return false;
    }

    return true;
}

void NewWorldDialog::CreateWorld() {
    if (m_onWorldCreated) {
        m_onWorldCreated(m_params);
    }
    Hide();
}

void NewWorldDialog::CreateEmptyWorld() {
    m_params.templateId = "empty_world";
    if (m_onWorldCreated) {
        m_onWorldCreated(m_params);
    }
    Hide();
}

std::string NewWorldDialog::GetWorldSizeDescription(WorldSizePreset preset) const {
    glm::ivec2 size = WorldCreationUtils::PresetToSize(preset);
    return std::to_string(size.x) + "x" + std::to_string(size.y) + "m";
}

int NewWorldDialog::GenerateRandomSeed() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1, 999999);
    return dis(gen);
}

// =============================================================================
// WorldCreationUtils Implementation
// =============================================================================

glm::ivec2 WorldCreationUtils::PresetToSize(WorldSizePreset preset) {
    switch (preset) {
        case WorldSizePreset::Small:  return glm::ivec2(2500, 2500);
        case WorldSizePreset::Medium: return glm::ivec2(5000, 5000);
        case WorldSizePreset::Large:  return glm::ivec2(10000, 10000);
        case WorldSizePreset::Huge:   return glm::ivec2(20000, 20000);
        case WorldSizePreset::Custom: return glm::ivec2(5000, 5000); // Default
        default: return glm::ivec2(5000, 5000);
    }
}

const char* WorldCreationUtils::GetPresetName(WorldSizePreset preset) {
    switch (preset) {
        case WorldSizePreset::Small:  return "Small";
        case WorldSizePreset::Medium: return "Medium";
        case WorldSizePreset::Large:  return "Large";
        case WorldSizePreset::Huge:   return "Huge";
        case WorldSizePreset::Custom: return "Custom";
        default: return "Unknown";
    }
}

int WorldCreationUtils::ParseSeed(const std::string& seedStr) {
    try {
        return std::stoi(seedStr);
    } catch (...) {
        return 12345; // Default seed
    }
}

} // namespace Vehement
