/**
 * @file MaterialEditor.cpp
 * @brief Implementation of multi-object material editor
 */

#include "MaterialEditor.hpp"
#include "../graphics/Texture.hpp"
#include <imgui.h>
#include <algorithm>
#include <sstream>
#include <unordered_set>

namespace Nova {

// =============================================================================
// Property Enum Conversion
// =============================================================================

std::string MaterialPropertyToString(MaterialProperty prop) {
    switch (prop) {
        case MaterialProperty::Albedo: return "Albedo";
        case MaterialProperty::Metallic: return "Metallic";
        case MaterialProperty::Roughness: return "Roughness";
        case MaterialProperty::AO: return "AO";
        case MaterialProperty::Emissive: return "Emissive";
        case MaterialProperty::IOR: return "IOR";
        case MaterialProperty::Transmission: return "Transmission";
        case MaterialProperty::Thickness: return "Thickness";
        case MaterialProperty::AlbedoMap: return "AlbedoMap";
        case MaterialProperty::NormalMap: return "NormalMap";
        case MaterialProperty::MetallicMap: return "MetallicMap";
        case MaterialProperty::RoughnessMap: return "RoughnessMap";
        case MaterialProperty::AOMap: return "AOMap";
        case MaterialProperty::EmissiveMap: return "EmissiveMap";
        case MaterialProperty::TwoSided: return "TwoSided";
        case MaterialProperty::Transparent: return "Transparent";
        case MaterialProperty::ClearCoat: return "ClearCoat";
        case MaterialProperty::ClearCoatRoughness: return "ClearCoatRoughness";
        case MaterialProperty::Sheen: return "Sheen";
        case MaterialProperty::SheenTint: return "SheenTint";
        case MaterialProperty::Anisotropic: return "Anisotropic";
        case MaterialProperty::AnisotropicRotation: return "AnisotropicRotation";
        case MaterialProperty::SubsurfaceRadius: return "SubsurfaceRadius";
        case MaterialProperty::SubsurfaceColor: return "SubsurfaceColor";
        default: return "Unknown";
    }
}

MaterialProperty StringToMaterialProperty(const std::string& str) {
    if (str == "Albedo") return MaterialProperty::Albedo;
    if (str == "Metallic") return MaterialProperty::Metallic;
    if (str == "Roughness") return MaterialProperty::Roughness;
    if (str == "AO") return MaterialProperty::AO;
    if (str == "Emissive") return MaterialProperty::Emissive;
    if (str == "IOR") return MaterialProperty::IOR;
    if (str == "Transmission") return MaterialProperty::Transmission;
    if (str == "Thickness") return MaterialProperty::Thickness;
    if (str == "AlbedoMap") return MaterialProperty::AlbedoMap;
    if (str == "NormalMap") return MaterialProperty::NormalMap;
    if (str == "MetallicMap") return MaterialProperty::MetallicMap;
    if (str == "RoughnessMap") return MaterialProperty::RoughnessMap;
    if (str == "AOMap") return MaterialProperty::AOMap;
    if (str == "EmissiveMap") return MaterialProperty::EmissiveMap;
    if (str == "TwoSided") return MaterialProperty::TwoSided;
    if (str == "Transparent") return MaterialProperty::Transparent;
    if (str == "ClearCoat") return MaterialProperty::ClearCoat;
    if (str == "ClearCoatRoughness") return MaterialProperty::ClearCoatRoughness;
    if (str == "Sheen") return MaterialProperty::Sheen;
    if (str == "SheenTint") return MaterialProperty::SheenTint;
    if (str == "Anisotropic") return MaterialProperty::Anisotropic;
    if (str == "AnisotropicRotation") return MaterialProperty::AnisotropicRotation;
    if (str == "SubsurfaceRadius") return MaterialProperty::SubsurfaceRadius;
    if (str == "SubsurfaceColor") return MaterialProperty::SubsurfaceColor;
    return MaterialProperty::Albedo;  // Default
}

// =============================================================================
// MaterialSnapshot Implementation
// =============================================================================

MaterialSnapshot MaterialSnapshot::Capture(const Material* material) {
    MaterialSnapshot snapshot;
    if (!material) return snapshot;

    // Note: Material class doesn't expose all these properties directly,
    // so we capture what we can access through the public API
    // In a real implementation, Material would expose these properties
    snapshot.twoSided = material->IsTwoSided();
    snapshot.transparent = material->IsTransparent();

    return snapshot;
}

void MaterialSnapshot::Apply(Material* material) const {
    if (!material) return;

    material->SetTwoSided(twoSided);
    material->SetTransparent(transparent);
    material->SetAlbedo(albedo);
    material->SetMetallic(metallic);
    material->SetRoughness(roughness);
    material->SetAO(ao);
    material->SetEmissive(emissive);
}

bool MaterialSnapshot::operator==(const MaterialSnapshot& other) const {
    return albedo == other.albedo &&
           metallic == other.metallic &&
           roughness == other.roughness &&
           ao == other.ao &&
           emissive == other.emissive &&
           ior == other.ior &&
           transmission == other.transmission &&
           thickness == other.thickness &&
           twoSided == other.twoSided &&
           transparent == other.transparent;
}

// =============================================================================
// MaterialPropertyCommand Implementation
// =============================================================================

MaterialPropertyCommand::MaterialPropertyCommand(
    SceneNode* node,
    MaterialProperty property,
    const MaterialPropertyValue& newValue)
    : m_property(property)
{
    m_nodeStates.push_back({node, {}, newValue});
    CaptureOldValues();
}

MaterialPropertyCommand::MaterialPropertyCommand(
    const std::vector<SceneNode*>& nodes,
    MaterialProperty property,
    const MaterialPropertyValue& newValue)
    : m_property(property)
{
    m_nodeStates.reserve(nodes.size());
    for (auto* node : nodes) {
        m_nodeStates.push_back({node, {}, newValue});
    }
    CaptureOldValues();
}

void MaterialPropertyCommand::CaptureOldValues() {
    for (auto& state : m_nodeStates) {
        if (state.node) {
            state.oldValue = GetValue(state.node);
        }
    }
}

bool MaterialPropertyCommand::Execute() {
    for (auto& state : m_nodeStates) {
        if (state.node) {
            ApplyValue(state.node, state.newValue);
        }
    }
    return true;
}

bool MaterialPropertyCommand::Undo() {
    for (auto& state : m_nodeStates) {
        if (state.node) {
            ApplyValue(state.node, state.oldValue);
        }
    }
    return true;
}

std::string MaterialPropertyCommand::GetName() const {
    std::ostringstream ss;
    ss << "Set " << MaterialPropertyToString(m_property);
    if (m_nodeStates.size() > 1) {
        ss << " (" << m_nodeStates.size() << " objects)";
    }
    return ss.str();
}

CommandTypeId MaterialPropertyCommand::GetTypeId() const {
    return GetCommandTypeId<MaterialPropertyCommand>();
}

bool MaterialPropertyCommand::CanMergeWith(const ICommand& other) const {
    if (other.GetTypeId() != GetTypeId()) {
        return false;
    }

    const auto& otherCmd = static_cast<const MaterialPropertyCommand&>(other);

    // Can only merge if same property and same nodes
    if (otherCmd.m_property != m_property) {
        return false;
    }

    if (otherCmd.m_nodeStates.size() != m_nodeStates.size()) {
        return false;
    }

    // Check if same nodes
    for (size_t i = 0; i < m_nodeStates.size(); ++i) {
        if (m_nodeStates[i].node != otherCmd.m_nodeStates[i].node) {
            return false;
        }
    }

    // Must be within time window
    return IsWithinMergeWindow();
}

bool MaterialPropertyCommand::MergeWith(const ICommand& other) {
    if (!CanMergeWith(other)) {
        return false;
    }

    const auto& otherCmd = static_cast<const MaterialPropertyCommand&>(other);

    // Keep our old values, take their new values
    for (size_t i = 0; i < m_nodeStates.size(); ++i) {
        m_nodeStates[i].newValue = otherCmd.m_nodeStates[i].newValue;
    }

    m_timestamp = std::chrono::steady_clock::now();
    return true;
}

MaterialPropertyValue MaterialPropertyCommand::GetValue(SceneNode* node) const {
    if (!node || !node->HasMaterial()) {
        return float(0.0f);
    }

    auto material = node->GetMaterial();

    // Note: This is a simplified implementation
    // A real implementation would need Material to expose property getters
    switch (m_property) {
        case MaterialProperty::TwoSided:
            return material->IsTwoSided();
        case MaterialProperty::Transparent:
            return material->IsTransparent();
        default:
            return float(0.0f);
    }
}

void MaterialPropertyCommand::ApplyValue(SceneNode* node, const MaterialPropertyValue& value) {
    if (!node || !node->HasMaterial()) {
        return;
    }

    auto material = node->GetMaterial();

    switch (m_property) {
        case MaterialProperty::Albedo:
            if (std::holds_alternative<glm::vec3>(value)) {
                material->SetAlbedo(std::get<glm::vec3>(value));
            }
            break;
        case MaterialProperty::Metallic:
            if (std::holds_alternative<float>(value)) {
                material->SetMetallic(std::get<float>(value));
            }
            break;
        case MaterialProperty::Roughness:
            if (std::holds_alternative<float>(value)) {
                material->SetRoughness(std::get<float>(value));
            }
            break;
        case MaterialProperty::AO:
            if (std::holds_alternative<float>(value)) {
                material->SetAO(std::get<float>(value));
            }
            break;
        case MaterialProperty::Emissive:
            if (std::holds_alternative<glm::vec3>(value)) {
                material->SetEmissive(std::get<glm::vec3>(value));
            }
            break;
        case MaterialProperty::TwoSided:
            if (std::holds_alternative<bool>(value)) {
                material->SetTwoSided(std::get<bool>(value));
            }
            break;
        case MaterialProperty::Transparent:
            if (std::holds_alternative<bool>(value)) {
                material->SetTransparent(std::get<bool>(value));
            }
            break;
        default:
            break;
    }
}

// =============================================================================
// AssignMaterialCommand Implementation
// =============================================================================

AssignMaterialCommand::AssignMaterialCommand(
    SceneNode* node,
    std::shared_ptr<Material> newMaterial)
    : m_newMaterial(std::move(newMaterial))
{
    m_nodeStates.push_back({node, node ? node->GetMaterial() : nullptr});
}

AssignMaterialCommand::AssignMaterialCommand(
    const std::vector<SceneNode*>& nodes,
    std::shared_ptr<Material> newMaterial)
    : m_newMaterial(std::move(newMaterial))
{
    m_nodeStates.reserve(nodes.size());
    for (auto* node : nodes) {
        m_nodeStates.push_back({node, node ? node->GetMaterial() : nullptr});
    }
}

bool AssignMaterialCommand::Execute() {
    for (auto& state : m_nodeStates) {
        if (state.node) {
            state.node->SetMaterial(m_newMaterial);
        }
    }
    return true;
}

bool AssignMaterialCommand::Undo() {
    for (auto& state : m_nodeStates) {
        if (state.node) {
            state.node->SetMaterial(state.oldMaterial);
        }
    }
    return true;
}

std::string AssignMaterialCommand::GetName() const {
    std::ostringstream ss;
    ss << "Assign Material";
    if (m_nodeStates.size() > 1) {
        ss << " to " << m_nodeStates.size() << " objects";
    }
    return ss.str();
}

CommandTypeId AssignMaterialCommand::GetTypeId() const {
    return GetCommandTypeId<AssignMaterialCommand>();
}

// =============================================================================
// MakeUniqueMaterialCommand Implementation
// =============================================================================

MakeUniqueMaterialCommand::MakeUniqueMaterialCommand(SceneNode* node) {
    m_nodeStates.push_back({node, nullptr, nullptr});
}

MakeUniqueMaterialCommand::MakeUniqueMaterialCommand(const std::vector<SceneNode*>& nodes) {
    m_nodeStates.reserve(nodes.size());
    for (auto* node : nodes) {
        m_nodeStates.push_back({node, nullptr, nullptr});
    }
}

std::shared_ptr<Material> MakeUniqueMaterialCommand::CloneMaterial(const std::shared_ptr<Material>& source) {
    if (!source) {
        return std::make_shared<Material>();
    }

    auto clone = std::make_shared<Material>();

    // Copy shader
    clone->SetShader(source->GetShaderPtr());

    // Copy properties
    clone->SetTwoSided(source->IsTwoSided());
    clone->SetTransparent(source->IsTransparent());

    return clone;
}

bool MakeUniqueMaterialCommand::Execute() {
    m_uniqueMaterials.clear();
    m_uniqueMaterials.reserve(m_nodeStates.size());

    for (auto& state : m_nodeStates) {
        if (!state.node) continue;

        state.originalMaterial = state.node->GetMaterial();
        state.uniqueMaterial = CloneMaterial(state.originalMaterial);
        state.node->SetMaterial(state.uniqueMaterial);
        m_uniqueMaterials.push_back(state.uniqueMaterial);
    }
    return true;
}

bool MakeUniqueMaterialCommand::Undo() {
    for (auto& state : m_nodeStates) {
        if (state.node) {
            state.node->SetMaterial(state.originalMaterial);
        }
    }
    return true;
}

std::string MakeUniqueMaterialCommand::GetName() const {
    std::ostringstream ss;
    ss << "Make Material Unique";
    if (m_nodeStates.size() > 1) {
        ss << " (" << m_nodeStates.size() << " objects)";
    }
    return ss.str();
}

CommandTypeId MakeUniqueMaterialCommand::GetTypeId() const {
    return GetCommandTypeId<MakeUniqueMaterialCommand>();
}

// =============================================================================
// CopyMaterialPropertiesCommand Implementation
// =============================================================================

CopyMaterialPropertiesCommand::CopyMaterialPropertiesCommand(
    const Material* source,
    const std::vector<SceneNode*>& targets,
    const std::vector<MaterialProperty>& propertiesToCopy)
    : m_sourceSnapshot(MaterialSnapshot::Capture(source))
    , m_propertiesToCopy(propertiesToCopy)
{
    m_nodeStates.reserve(targets.size());
    for (auto* target : targets) {
        if (target && target->HasMaterial()) {
            m_nodeStates.push_back({
                target,
                MaterialSnapshot::Capture(target->GetMaterial().get())
            });
        }
    }
}

bool CopyMaterialPropertiesCommand::Execute() {
    for (auto& state : m_nodeStates) {
        if (!state.node || !state.node->HasMaterial()) continue;

        auto material = state.node->GetMaterial();

        for (auto prop : m_propertiesToCopy) {
            switch (prop) {
                case MaterialProperty::Albedo:
                    material->SetAlbedo(m_sourceSnapshot.albedo);
                    break;
                case MaterialProperty::Metallic:
                    material->SetMetallic(m_sourceSnapshot.metallic);
                    break;
                case MaterialProperty::Roughness:
                    material->SetRoughness(m_sourceSnapshot.roughness);
                    break;
                case MaterialProperty::AO:
                    material->SetAO(m_sourceSnapshot.ao);
                    break;
                case MaterialProperty::Emissive:
                    material->SetEmissive(m_sourceSnapshot.emissive);
                    break;
                case MaterialProperty::TwoSided:
                    material->SetTwoSided(m_sourceSnapshot.twoSided);
                    break;
                case MaterialProperty::Transparent:
                    material->SetTransparent(m_sourceSnapshot.transparent);
                    break;
                default:
                    break;
            }
        }
    }
    return true;
}

bool CopyMaterialPropertiesCommand::Undo() {
    for (auto& state : m_nodeStates) {
        if (state.node && state.node->HasMaterial()) {
            state.oldSnapshot.Apply(state.node->GetMaterial().get());
        }
    }
    return true;
}

std::string CopyMaterialPropertiesCommand::GetName() const {
    std::ostringstream ss;
    ss << "Copy Material Properties";
    if (m_nodeStates.size() > 1) {
        ss << " to " << m_nodeStates.size() << " objects";
    }
    return ss.str();
}

CommandTypeId CopyMaterialPropertiesCommand::GetTypeId() const {
    return GetCommandTypeId<CopyMaterialPropertiesCommand>();
}

// =============================================================================
// MaterialComparison Implementation
// =============================================================================

float MaterialComparison::GetSimilarity() const {
    size_t total = differences.size() + matchingProperties.size();
    if (total == 0) return 1.0f;
    return static_cast<float>(matchingProperties.size()) / static_cast<float>(total);
}

MaterialComparison CompareMaterials(const Material* a, const Material* b) {
    MaterialComparison result;

    if (!a || !b) {
        result.areIdentical = (a == b);
        return result;
    }

    // Compare each property
    auto checkBool = [&](MaterialProperty prop, bool valA, bool valB) {
        if (valA == valB) {
            result.matchingProperties.push_back(prop);
        } else {
            result.differences.push_back({
                prop, valA, valB, MaterialPropertyToString(prop)
            });
        }
    };

    checkBool(MaterialProperty::TwoSided, a->IsTwoSided(), b->IsTwoSided());
    checkBool(MaterialProperty::Transparent, a->IsTransparent(), b->IsTransparent());

    result.areIdentical = result.differences.empty();
    return result;
}

// =============================================================================
// MaterialEditor Implementation
// =============================================================================

MaterialEditor::MaterialEditor() = default;
MaterialEditor::~MaterialEditor() = default;

// -----------------------------------------------------------------------------
// Selection Management
// -----------------------------------------------------------------------------

void MaterialEditor::SetSelection(const std::vector<SceneNode*>& nodes) {
    m_selection = nodes;
    m_selectionSet.clear();
    for (auto* node : nodes) {
        if (node) {
            m_selectionSet.insert(node);
        }
    }
    UpdatePropertyCache();
    NotifySelectionChanged();
}

void MaterialEditor::AddToSelection(const std::vector<SceneNode*>& nodes) {
    for (auto* node : nodes) {
        if (node && m_selectionSet.find(node) == m_selectionSet.end()) {
            m_selection.push_back(node);
            m_selectionSet.insert(node);
        }
    }
    UpdatePropertyCache();
    NotifySelectionChanged();
}

void MaterialEditor::RemoveFromSelection(const std::vector<SceneNode*>& nodes) {
    for (auto* node : nodes) {
        m_selectionSet.erase(node);
    }
    m_selection.erase(
        std::remove_if(m_selection.begin(), m_selection.end(),
            [this](SceneNode* n) { return m_selectionSet.find(n) == m_selectionSet.end(); }),
        m_selection.end()
    );
    UpdatePropertyCache();
    NotifySelectionChanged();
}

void MaterialEditor::ClearSelection() {
    m_selection.clear();
    m_selectionSet.clear();
    UpdatePropertyCache();
    NotifySelectionChanged();
}

// -----------------------------------------------------------------------------
// Property Access
// -----------------------------------------------------------------------------

PropertyState MaterialEditor::GetPropertyState(MaterialProperty property) const {
    if (m_selection.empty()) {
        return PropertyState::Undefined;
    }

    // Check if all values are the same
    bool hasFirst = false;
    MaterialPropertyValue firstValue;

    for (const auto* node : m_selection) {
        if (!node || !node->HasMaterial()) continue;

        auto value = ExtractPropertyValue(node->GetMaterial().get(), property);

        if (!hasFirst) {
            firstValue = value;
            hasFirst = true;
        } else if (value != firstValue) {
            return PropertyState::Mixed;
        }
    }

    return hasFirst ? PropertyState::Uniform : PropertyState::Undefined;
}

bool MaterialEditor::IsPropertyMixed(MaterialProperty property) const {
    return GetPropertyState(property) == PropertyState::Mixed;
}

std::vector<MaterialProperty> MaterialEditor::GetMixedProperties() const {
    std::vector<MaterialProperty> mixed;

    static const MaterialProperty allProperties[] = {
        MaterialProperty::Albedo,
        MaterialProperty::Metallic,
        MaterialProperty::Roughness,
        MaterialProperty::AO,
        MaterialProperty::Emissive,
        MaterialProperty::TwoSided,
        MaterialProperty::Transparent
    };

    for (auto prop : allProperties) {
        if (IsPropertyMixed(prop)) {
            mixed.push_back(prop);
        }
    }

    return mixed;
}

// -----------------------------------------------------------------------------
// Property Modification
// -----------------------------------------------------------------------------

void MaterialEditor::SetPropertyValue(MaterialProperty property, const MaterialPropertyValue& value) {
    if (m_selection.empty()) return;

    auto command = std::make_unique<MaterialPropertyCommand>(m_selection, property, value);
    ExecuteCommand(std::move(command));
    NotifyPropertyChanged(property);
}

void MaterialEditor::ApplyToAll(MaterialProperty property, const MaterialPropertyValue& value, bool createCommand) {
    if (m_selection.empty()) return;

    if (createCommand) {
        SetPropertyValue(property, value);
        return;
    }

    // Live preview - apply directly without command
    if (!m_currentEdit || m_currentEdit->property != property) {
        // Start new edit
        m_currentEdit = EditState{};
        m_currentEdit->property = property;
        m_currentEdit->startValue = value;
        m_currentEdit->originalValues.reserve(m_selection.size());

        for (auto* node : m_selection) {
            if (node && node->HasMaterial()) {
                m_currentEdit->originalValues.push_back(
                    ExtractPropertyValue(node->GetMaterial().get(), property)
                );
            }
        }
        m_currentEdit->isDragging = true;
    }

    // Apply value directly
    for (auto* node : m_selection) {
        if (node && node->HasMaterial()) {
            ApplyPropertyValue(node->GetMaterial().get(), property, value);
        }
    }
}

void MaterialEditor::FinalizePropertyChange(MaterialProperty property) {
    if (!m_currentEdit || m_currentEdit->property != property) return;

    // Create command with original values
    auto command = std::make_unique<MaterialPropertyCommand>(
        m_selection, property,
        ExtractPropertyValue(m_selection[0]->GetMaterial().get(), property)
    );

    ExecuteCommand(std::move(command));
    m_currentEdit.reset();
    NotifyPropertyChanged(property);
}

void MaterialEditor::ResetProperty(MaterialProperty property) {
    MaterialPropertyValue defaultValue;

    switch (property) {
        case MaterialProperty::Albedo:
            defaultValue = glm::vec3(1.0f);
            break;
        case MaterialProperty::Metallic:
            defaultValue = 0.0f;
            break;
        case MaterialProperty::Roughness:
            defaultValue = 0.5f;
            break;
        case MaterialProperty::AO:
            defaultValue = 1.0f;
            break;
        case MaterialProperty::Emissive:
            defaultValue = glm::vec3(0.0f);
            break;
        case MaterialProperty::TwoSided:
            defaultValue = false;
            break;
        case MaterialProperty::Transparent:
            defaultValue = false;
            break;
        default:
            return;
    }

    SetPropertyValue(property, defaultValue);
}

void MaterialEditor::CopyPropertyToAll(SceneNode* sourceNode, MaterialProperty property) {
    if (!sourceNode || !sourceNode->HasMaterial() || m_selection.empty()) return;

    auto value = ExtractPropertyValue(sourceNode->GetMaterial().get(), property);
    SetPropertyValue(property, value);
}

// -----------------------------------------------------------------------------
// Material Assignment
// -----------------------------------------------------------------------------

void MaterialEditor::BatchAssignMaterial(std::shared_ptr<Material> material) {
    if (m_selection.empty()) return;

    auto command = std::make_unique<AssignMaterialCommand>(m_selection, std::move(material));
    ExecuteCommand(std::move(command));
}

void MaterialEditor::MakeUnique() {
    if (m_selection.empty()) return;

    auto command = std::make_unique<MakeUniqueMaterialCommand>(m_selection);
    ExecuteCommand(std::move(command));
}

bool MaterialEditor::HasSharedMaterials() const {
    if (m_selection.size() < 2) return false;

    std::unordered_set<Material*> seen;
    for (const auto* node : m_selection) {
        if (!node || !node->HasMaterial()) continue;

        Material* mat = node->GetMaterial().get();
        if (seen.find(mat) != seen.end()) {
            return true;
        }
        seen.insert(mat);
    }
    return false;
}

std::vector<std::shared_ptr<Material>> MaterialEditor::GetUniqueMaterials() const {
    std::vector<std::shared_ptr<Material>> materials;
    std::unordered_set<Material*> seen;

    for (const auto* node : m_selection) {
        if (!node || !node->HasMaterial()) continue;

        auto mat = node->GetMaterial();
        if (seen.find(mat.get()) == seen.end()) {
            materials.push_back(mat);
            seen.insert(mat.get());
        }
    }
    return materials;
}

size_t MaterialEditor::GetUniqueMaterialCount() const {
    return GetUniqueMaterials().size();
}

// -----------------------------------------------------------------------------
// Material Comparison
// -----------------------------------------------------------------------------

MaterialComparison MaterialEditor::CompareSelectedMaterials(size_t indexA, size_t indexB) const {
    if (indexA >= m_selection.size() || indexB >= m_selection.size()) {
        return MaterialComparison{};
    }

    const Material* matA = m_selection[indexA]->HasMaterial() ?
        m_selection[indexA]->GetMaterial().get() : nullptr;
    const Material* matB = m_selection[indexB]->HasMaterial() ?
        m_selection[indexB]->GetMaterial().get() : nullptr;

    return CompareMaterials(matA, matB);
}

std::vector<MaterialComparison> MaterialEditor::CompareAllToFirst() const {
    std::vector<MaterialComparison> results;
    if (m_selection.size() < 2) return results;

    results.reserve(m_selection.size() - 1);
    for (size_t i = 1; i < m_selection.size(); ++i) {
        results.push_back(CompareSelectedMaterials(0, i));
    }
    return results;
}

// -----------------------------------------------------------------------------
// Undo/Redo Integration
// -----------------------------------------------------------------------------

void MaterialEditor::BeginTransaction(const std::string& name) {
    if (m_commandHistory) {
        m_commandHistory->BeginTransaction(name);
    }
}

void MaterialEditor::CommitTransaction() {
    if (m_commandHistory) {
        m_commandHistory->CommitTransaction();
    }
}

void MaterialEditor::RollbackTransaction() {
    if (m_commandHistory) {
        m_commandHistory->RollbackTransaction();
    }
}

bool MaterialEditor::IsTransactionActive() const {
    return m_commandHistory && m_commandHistory->IsTransactionActive();
}

// -----------------------------------------------------------------------------
// UI Rendering
// -----------------------------------------------------------------------------

void MaterialEditor::RenderUI() {
    if (!HasSelection()) {
        ImGui::TextDisabled("No objects selected");
        return;
    }

    RenderSelectionInfo();
    ImGui::Separator();

    if (ImGui::BeginTabBar("MaterialEditorTabs")) {
        if (ImGui::BeginTabItem("Properties")) {
            RenderBasicProperties();
            ImGui::Separator();
            RenderOpticalProperties();
            ImGui::Separator();
            RenderRenderingOptions();

            if (m_showAdvancedProperties) {
                ImGui::Separator();
                RenderAdvancedProperties();
            }

            ImGui::Checkbox("Show Advanced", &m_showAdvancedProperties);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Textures")) {
            RenderTextureProperties();
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Batch Operations")) {
            RenderBatchOperations();
            ImGui::EndTabItem();
        }

        if (IsMultiSelection() && ImGui::BeginTabItem("Comparison")) {
            RenderComparisonView();
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void MaterialEditor::RenderCompactUI() {
    if (!HasSelection()) {
        ImGui::TextDisabled("No selection");
        return;
    }

    // Compact single-line properties
    HandleMixedProperty("Metallic", MaterialProperty::Metallic, 0.0f, 1.0f);
    HandleMixedProperty("Roughness", MaterialProperty::Roughness, 0.0f, 1.0f);
    HandleMixedColorProperty("Albedo", MaterialProperty::Albedo);
}

void MaterialEditor::RenderComparisonView() {
    if (m_selection.size() < 2) {
        ImGui::TextDisabled("Select at least 2 objects to compare");
        return;
    }

    ImGui::Text("Compare Materials");

    // Source selection
    ImGui::SetNextItemWidth(150);
    if (ImGui::BeginCombo("Source", m_selection[m_comparisonSourceIndex]->GetName().c_str())) {
        for (size_t i = 0; i < m_selection.size(); ++i) {
            bool selected = (i == static_cast<size_t>(m_comparisonSourceIndex));
            if (ImGui::Selectable(m_selection[i]->GetName().c_str(), selected)) {
                m_comparisonSourceIndex = static_cast<int>(i);
            }
        }
        ImGui::EndCombo();
    }

    ImGui::SameLine();

    // Target selection
    ImGui::SetNextItemWidth(150);
    if (ImGui::BeginCombo("Target", m_selection[m_comparisonTargetIndex]->GetName().c_str())) {
        for (size_t i = 0; i < m_selection.size(); ++i) {
            bool selected = (i == static_cast<size_t>(m_comparisonTargetIndex));
            if (ImGui::Selectable(m_selection[i]->GetName().c_str(), selected)) {
                m_comparisonTargetIndex = static_cast<int>(i);
            }
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    // Show comparison results
    auto comparison = CompareSelectedMaterials(
        static_cast<size_t>(m_comparisonSourceIndex),
        static_cast<size_t>(m_comparisonTargetIndex)
    );

    float similarity = comparison.GetSimilarity() * 100.0f;
    ImGui::Text("Similarity: %.1f%%", similarity);
    ImGui::ProgressBar(comparison.GetSimilarity());

    if (comparison.areIdentical) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "Materials are identical");
    } else {
        ImGui::Text("Differences (%zu):", comparison.GetDifferenceCount());

        if (ImGui::BeginTable("Differences", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
            ImGui::TableSetupColumn("Property");
            ImGui::TableSetupColumn("Source");
            ImGui::TableSetupColumn("Target");
            ImGui::TableHeadersRow();

            for (const auto& diff : comparison.differences) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", diff.propertyName.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("Value A");  // Placeholder - would format actual value
                ImGui::TableNextColumn();
                ImGui::Text("Value B");
            }

            ImGui::EndTable();
        }
    }

    if (ImGui::Button("Copy Source to Target")) {
        CopyPropertyToAll(m_selection[m_comparisonSourceIndex], MaterialProperty::Albedo);
        // Copy other properties...
    }
}

void MaterialEditor::RenderAssignmentPanel() {
    ImGui::Text("Material Assignment");
    ImGui::Separator();

    if (HasSharedMaterials()) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
            "Warning: Some objects share materials");
        if (ImGui::Button("Make All Unique")) {
            MakeUnique();
        }
    }

    ImGui::Text("Unique materials: %zu", GetUniqueMaterialCount());

    // Material library browser would go here
    ImGui::TextDisabled("Drag material from library to assign");
}

// -----------------------------------------------------------------------------
// Mixed Property UI Helpers
// -----------------------------------------------------------------------------

bool MaterialEditor::HandleMixedProperty(
    const char* label,
    MaterialProperty property,
    float min,
    float max)
{
    return RenderMixedFloatSlider(label, property, min, max);
}

bool MaterialEditor::HandleMixedColorProperty(
    const char* label,
    MaterialProperty property)
{
    return RenderMixedColorEdit(label, property);
}

bool MaterialEditor::HandleMixedBoolProperty(
    const char* label,
    MaterialProperty property)
{
    return RenderMixedCheckbox(label, property);
}

bool MaterialEditor::HandleMixedTextureProperty(
    const char* label,
    MaterialProperty property)
{
    return RenderMixedTextureSlot(label, property);
}

// -----------------------------------------------------------------------------
// Internal UI Rendering Helpers
// -----------------------------------------------------------------------------

void MaterialEditor::RenderBasicProperties() {
    ImGui::Text("Basic Properties");

    HandleMixedColorProperty("Albedo", MaterialProperty::Albedo);
    HandleMixedProperty("Metallic", MaterialProperty::Metallic, 0.0f, 1.0f);
    HandleMixedProperty("Roughness", MaterialProperty::Roughness, 0.0f, 1.0f);
    HandleMixedProperty("AO", MaterialProperty::AO, 0.0f, 1.0f);
    HandleMixedColorProperty("Emissive", MaterialProperty::Emissive);
}

void MaterialEditor::RenderOpticalProperties() {
    ImGui::Text("Optical Properties");

    HandleMixedProperty("IOR", MaterialProperty::IOR, 1.0f, 3.0f);
    HandleMixedProperty("Transmission", MaterialProperty::Transmission, 0.0f, 1.0f);
    HandleMixedProperty("Thickness", MaterialProperty::Thickness, 0.0f, 10.0f);
}

void MaterialEditor::RenderTextureProperties() {
    ImGui::Text("Texture Maps");

    HandleMixedTextureProperty("Albedo Map", MaterialProperty::AlbedoMap);
    HandleMixedTextureProperty("Normal Map", MaterialProperty::NormalMap);
    HandleMixedTextureProperty("Metallic Map", MaterialProperty::MetallicMap);
    HandleMixedTextureProperty("Roughness Map", MaterialProperty::RoughnessMap);
    HandleMixedTextureProperty("AO Map", MaterialProperty::AOMap);
    HandleMixedTextureProperty("Emissive Map", MaterialProperty::EmissiveMap);
}

void MaterialEditor::RenderRenderingOptions() {
    ImGui::Text("Rendering Options");

    HandleMixedBoolProperty("Two-Sided", MaterialProperty::TwoSided);
    HandleMixedBoolProperty("Transparent", MaterialProperty::Transparent);
}

void MaterialEditor::RenderAdvancedProperties() {
    ImGui::Text("Advanced Properties");

    HandleMixedProperty("Clear Coat", MaterialProperty::ClearCoat, 0.0f, 1.0f);
    HandleMixedProperty("Clear Coat Roughness", MaterialProperty::ClearCoatRoughness, 0.0f, 1.0f);
    HandleMixedProperty("Sheen", MaterialProperty::Sheen, 0.0f, 1.0f);
    HandleMixedProperty("Sheen Tint", MaterialProperty::SheenTint, 0.0f, 1.0f);
    HandleMixedProperty("Anisotropic", MaterialProperty::Anisotropic, 0.0f, 1.0f);
    HandleMixedProperty("Anisotropic Rotation", MaterialProperty::AnisotropicRotation, 0.0f, 1.0f);
    HandleMixedProperty("Subsurface Radius", MaterialProperty::SubsurfaceRadius, 0.0f, 10.0f);
    HandleMixedColorProperty("Subsurface Color", MaterialProperty::SubsurfaceColor);
}

void MaterialEditor::RenderSelectionInfo() {
    ImGui::Text("Selection: %zu objects", m_selection.size());

    if (IsMultiSelection()) {
        size_t uniqueMats = GetUniqueMaterialCount();
        ImGui::SameLine();
        ImGui::Text("| %zu unique materials", uniqueMats);

        auto mixedProps = GetMixedProperties();
        if (!mixedProps.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f),
                "%zu properties have mixed values", mixedProps.size());
        }
    }
}

void MaterialEditor::RenderBatchOperations() {
    ImGui::Text("Batch Operations");
    ImGui::Separator();

    if (ImGui::Button("Make All Unique", ImVec2(-1, 0))) {
        MakeUnique();
    }
    ImGui::SetItemTooltip("Create independent material copies for each object");

    ImGui::Separator();
    ImGui::Text("Reset Properties");

    if (ImGui::Button("Reset Metallic")) {
        ResetProperty(MaterialProperty::Metallic);
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Roughness")) {
        ResetProperty(MaterialProperty::Roughness);
    }

    if (ImGui::Button("Reset All to Defaults")) {
        BeginTransaction("Reset All Properties");
        ResetProperty(MaterialProperty::Albedo);
        ResetProperty(MaterialProperty::Metallic);
        ResetProperty(MaterialProperty::Roughness);
        ResetProperty(MaterialProperty::AO);
        ResetProperty(MaterialProperty::Emissive);
        ResetProperty(MaterialProperty::TwoSided);
        ResetProperty(MaterialProperty::Transparent);
        CommitTransaction();
    }

    ImGui::Separator();
    RenderAssignmentPanel();
}

bool MaterialEditor::RenderMixedFloatSlider(
    const char* label,
    MaterialProperty property,
    float min,
    float max,
    const char* format)
{
    auto prop = GetProperty<float>(property);

    if (prop.IsUndefined()) {
        ImGui::BeginDisabled();
        float dummy = 0.0f;
        ImGui::SliderFloat(label, &dummy, min, max, format);
        ImGui::EndDisabled();
        return false;
    }

    float value = prop.GetValue();

    if (prop.IsMixed()) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.4f, 0.2f, 1.0f));
        RenderMixedIndicator("Multiple values - editing will apply to all");
    }

    bool changed = ImGui::SliderFloat(label, &value, min, max, format);

    if (prop.IsMixed()) {
        ImGui::PopStyleColor();
    }

    if (changed) {
        if (m_livePreview) {
            ApplyToAll(property, value, false);
        }
    }

    if (ImGui::IsItemDeactivatedAfterEdit()) {
        if (m_livePreview && m_currentEdit) {
            FinalizePropertyChange(property);
        } else {
            SetPropertyValue(property, value);
        }
        return true;
    }

    return false;
}

bool MaterialEditor::RenderMixedColorEdit(const char* label, MaterialProperty property) {
    auto prop = GetProperty<glm::vec3>(property);

    if (prop.IsUndefined()) {
        ImGui::BeginDisabled();
        float dummy[3] = {0.0f, 0.0f, 0.0f};
        ImGui::ColorEdit3(label, dummy);
        ImGui::EndDisabled();
        return false;
    }

    glm::vec3 value = prop.GetValue();
    float color[3] = {value.r, value.g, value.b};

    if (prop.IsMixed()) {
        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.5f, 0.4f, 0.2f, 1.0f));
        RenderMixedIndicator("Multiple values - editing will apply to all");
    }

    bool changed = ImGui::ColorEdit3(label, color);

    if (prop.IsMixed()) {
        ImGui::PopStyleColor();
    }

    if (changed) {
        glm::vec3 newValue(color[0], color[1], color[2]);
        if (m_livePreview) {
            ApplyToAll(property, newValue, false);
        }
    }

    if (ImGui::IsItemDeactivatedAfterEdit()) {
        glm::vec3 newValue(color[0], color[1], color[2]);
        if (m_livePreview && m_currentEdit) {
            FinalizePropertyChange(property);
        } else {
            SetPropertyValue(property, newValue);
        }
        return true;
    }

    return false;
}

bool MaterialEditor::RenderMixedCheckbox(const char* label, MaterialProperty property) {
    auto prop = GetProperty<bool>(property);

    if (prop.IsUndefined()) {
        ImGui::BeginDisabled();
        bool dummy = false;
        ImGui::Checkbox(label, &dummy);
        ImGui::EndDisabled();
        return false;
    }

    bool value = prop.GetValue();

    if (prop.IsMixed()) {
        // Show indeterminate state
        ImGui::PushStyleColor(ImGuiCol_CheckMark, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        RenderMixedIndicator("Multiple values - clicking will set all");
    }

    bool changed = ImGui::Checkbox(label, &value);

    if (prop.IsMixed()) {
        ImGui::PopStyleColor();
    }

    if (changed) {
        SetPropertyValue(property, value);
        return true;
    }

    return false;
}

bool MaterialEditor::RenderMixedTextureSlot(const char* label, MaterialProperty property) {
    auto state = GetPropertyState(property);

    ImGui::Text("%s:", label);
    ImGui::SameLine();

    if (state == PropertyState::Mixed) {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[%s]", m_mixedValueText.c_str());
        RenderMixedIndicator("Objects have different textures assigned");
    } else if (state == PropertyState::Undefined) {
        ImGui::TextDisabled("(none)");
    } else {
        ImGui::TextDisabled("(texture)");  // Would show texture preview
    }

    // Drop target for texture drag-and-drop
    if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_ASSET")) {
            // Handle texture assignment
            // This would integrate with the asset system
        }
        ImGui::EndDragDropTarget();
    }

    return false;
}

void MaterialEditor::RenderMixedIndicator(const char* tooltip) {
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.0f, 1.0f), "[%s]", m_mixedValueText.c_str());
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
}

// -----------------------------------------------------------------------------
// Internal Helpers
// -----------------------------------------------------------------------------

void MaterialEditor::UpdatePropertyCache() {
    // Property cache update if needed for performance
}

void MaterialEditor::NotifyPropertyChanged(MaterialProperty property) {
    if (m_onPropertyChanged) {
        m_onPropertyChanged(property, m_selection);
    }
}

void MaterialEditor::NotifySelectionChanged() {
    if (m_onSelectionChanged) {
        m_onSelectionChanged(m_selection);
    }
}

void MaterialEditor::ExecuteCommand(CommandPtr command) {
    if (m_commandHistory) {
        m_commandHistory->ExecuteCommand(std::move(command));
    } else {
        // Execute directly if no history
        command->Execute();
    }
}

MaterialPropertyValue MaterialEditor::ExtractPropertyValue(
    const Material* material,
    MaterialProperty property) const
{
    if (!material) {
        return float(0.0f);
    }

    switch (property) {
        case MaterialProperty::TwoSided:
            return material->IsTwoSided();
        case MaterialProperty::Transparent:
            return material->IsTransparent();
        // Other properties would need Material API expansion
        default:
            return float(0.0f);
    }
}

void MaterialEditor::ApplyPropertyValue(
    Material* material,
    MaterialProperty property,
    const MaterialPropertyValue& value)
{
    if (!material) return;

    switch (property) {
        case MaterialProperty::Albedo:
            if (std::holds_alternative<glm::vec3>(value)) {
                material->SetAlbedo(std::get<glm::vec3>(value));
            }
            break;
        case MaterialProperty::Metallic:
            if (std::holds_alternative<float>(value)) {
                material->SetMetallic(std::get<float>(value));
            }
            break;
        case MaterialProperty::Roughness:
            if (std::holds_alternative<float>(value)) {
                material->SetRoughness(std::get<float>(value));
            }
            break;
        case MaterialProperty::AO:
            if (std::holds_alternative<float>(value)) {
                material->SetAO(std::get<float>(value));
            }
            break;
        case MaterialProperty::Emissive:
            if (std::holds_alternative<glm::vec3>(value)) {
                material->SetEmissive(std::get<glm::vec3>(value));
            }
            break;
        case MaterialProperty::TwoSided:
            if (std::holds_alternative<bool>(value)) {
                material->SetTwoSided(std::get<bool>(value));
            }
            break;
        case MaterialProperty::Transparent:
            if (std::holds_alternative<bool>(value)) {
                material->SetTransparent(std::get<bool>(value));
            }
            break;
        default:
            break;
    }
}

} // namespace Nova
