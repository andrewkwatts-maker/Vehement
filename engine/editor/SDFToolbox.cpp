/**
 * @file SDFToolbox.cpp
 * @brief Implementation of SDF Toolbox panel for the Nova3D/Vehement editor
 */

#include "SDFToolbox.hpp"
#include "../scene/Scene.hpp"
#include "../scene/SceneNode.hpp"
#include <imgui.h>
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <cmath>

namespace Nova {

// =============================================================================
// Tool Mode Helpers
// =============================================================================

const char* GetToolModeName(SDFToolMode mode) {
    switch (mode) {
        case SDFToolMode::Create: return "Create";
        case SDFToolMode::Edit:   return "Edit";
        case SDFToolMode::CSG:    return "CSG";
        default:                  return "Unknown";
    }
}

const char* GetToolModeIcon(SDFToolMode mode) {
    switch (mode) {
        case SDFToolMode::Create: return "\xef\x81\xa7";  // Plus icon
        case SDFToolMode::Edit:   return "\xef\x8c\x84";  // Edit icon
        case SDFToolMode::CSG:    return "\xef\x81\x88";  // Layer icon
        default:                  return "\xef\x80\x88";  // Question mark
    }
}

// =============================================================================
// SDFPrimitivePreset
// =============================================================================

std::unique_ptr<SDFPrimitive> SDFPrimitivePreset::CreatePrimitive() const {
    auto primitive = std::make_unique<SDFPrimitive>(name, type);
    primitive->SetParameters(parameters);
    primitive->SetMaterial(material);
    return primitive;
}

// =============================================================================
// CSGTreeNode
// =============================================================================

CSGTreeNode CSGTreeNode::BuildFromPrimitive(SDFPrimitive* root) {
    CSGTreeNode node;
    if (!root) return node;

    node.primitive = root;
    node.displayName = root->GetName();
    node.operation = root->GetCSGOperation();
    node.expanded = true;

    for (auto& child : root->GetChildren()) {
        node.children.push_back(BuildFromPrimitive(child.get()));
    }

    return node;
}

// =============================================================================
// CreateSDFPrimitiveCommand
// =============================================================================

CreateSDFPrimitiveCommand::CreateSDFPrimitiveCommand(
    SDFModel* model,
    SDFPrimitiveType type,
    const glm::vec3& position,
    const SDFParameters& parameters,
    SDFPrimitive* parent
)
    : m_model(model)
    , m_type(type)
    , m_position(position)
    , m_parameters(parameters)
    , m_parent(parent)
{
}

CreateSDFPrimitiveCommand::CreateSDFPrimitiveCommand(
    SDFModel* model,
    std::unique_ptr<SDFPrimitive> primitive,
    SDFPrimitive* parent
)
    : m_model(model)
    , m_type(primitive ? primitive->GetType() : SDFPrimitiveType::Sphere)
    , m_position(primitive ? primitive->GetLocalTransform().position : glm::vec3(0.0f))
    , m_parameters(primitive ? primitive->GetParameters() : SDFParameters{})
    , m_parent(parent)
    , m_ownedPrimitive(std::move(primitive))
{
    if (m_ownedPrimitive) {
        m_primitiveName = m_ownedPrimitive->GetName();
    }
}

bool CreateSDFPrimitiveCommand::Execute() {
    if (!m_model) return false;

    if (!m_ownedPrimitive) {
        // Create new primitive
        m_ownedPrimitive = std::make_unique<SDFPrimitive>(m_type);
        SDFTransform transform;
        transform.position = m_position;
        m_ownedPrimitive->SetLocalTransform(transform);
        m_ownedPrimitive->SetParameters(m_parameters);
        m_primitiveName = m_ownedPrimitive->GetName();
    }

    // Add to model/parent
    SDFPrimitive* parent = m_parent ? m_parent : m_model->GetRoot();
    if (parent) {
        m_primitivePtr = parent->AddChild(std::move(m_ownedPrimitive));
    } else {
        // Set as root if no parent
        m_model->SetRoot(std::move(m_ownedPrimitive));
        m_primitivePtr = m_model->GetRoot();
    }

    return m_primitivePtr != nullptr;
}

bool CreateSDFPrimitiveCommand::Undo() {
    if (!m_model || !m_primitivePtr) return false;

    SDFPrimitive* parent = m_primitivePtr->GetParent();
    if (parent) {
        // Remove from parent and reclaim ownership
        for (size_t i = 0; i < parent->GetChildren().size(); ++i) {
            if (parent->GetChildren()[i].get() == m_primitivePtr) {
                m_ownedPrimitive = std::move(parent->GetChildren()[i]);
                parent->RemoveChild(i);
                break;
            }
        }
    }

    m_primitivePtr = nullptr;
    return true;
}

std::string CreateSDFPrimitiveCommand::GetName() const {
    return "Create " + m_primitiveName;
}

CommandTypeId CreateSDFPrimitiveCommand::GetTypeId() const {
    return GetCommandTypeId<CreateSDFPrimitiveCommand>();
}

// =============================================================================
// DeleteSDFPrimitiveCommand
// =============================================================================

DeleteSDFPrimitiveCommand::DeleteSDFPrimitiveCommand(SDFModel* model, SDFPrimitive* primitive)
    : m_model(model)
    , m_primitivePtr(primitive)
{
    if (primitive) {
        m_primitiveName = primitive->GetName();
        m_parent = primitive->GetParent();
        // Find sibling index
        if (m_parent) {
            for (size_t i = 0; i < m_parent->GetChildren().size(); ++i) {
                if (m_parent->GetChildren()[i].get() == primitive) {
                    m_siblingIndex = i;
                    break;
                }
            }
        }
    }
}

bool DeleteSDFPrimitiveCommand::Execute() {
    if (!m_model || !m_primitivePtr || !m_parent) return false;

    // Remove from parent and take ownership
    for (size_t i = 0; i < m_parent->GetChildren().size(); ++i) {
        if (m_parent->GetChildren()[i].get() == m_primitivePtr) {
            m_ownedPrimitive = std::move(m_parent->GetChildren()[i]);
            m_parent->RemoveChild(i);
            break;
        }
    }

    return m_ownedPrimitive != nullptr;
}

bool DeleteSDFPrimitiveCommand::Undo() {
    if (!m_model || !m_ownedPrimitive || !m_parent) return false;

    // Re-insert at original position
    m_primitivePtr = m_parent->AddChild(std::move(m_ownedPrimitive));
    // FUTURE: Restore exact sibling index if needed

    return m_primitivePtr != nullptr;
}

std::string DeleteSDFPrimitiveCommand::GetName() const {
    return "Delete " + m_primitiveName;
}

CommandTypeId DeleteSDFPrimitiveCommand::GetTypeId() const {
    return GetCommandTypeId<DeleteSDFPrimitiveCommand>();
}

// =============================================================================
// CSGOperationCommand
// =============================================================================

CSGOperationCommand::CSGOperationCommand(
    SDFModel* model,
    SDFPrimitive* primitiveA,
    SDFPrimitive* primitiveB,
    CSGOperation operation,
    float smoothness
)
    : m_model(model)
    , m_primitiveA(primitiveA)
    , m_primitiveB(primitiveB)
    , m_operation(operation)
    , m_smoothness(smoothness)
{
    if (primitiveA) {
        m_originalParentA = primitiveA->GetParent();
    }
    if (primitiveB) {
        m_originalParentB = primitiveB->GetParent();
        m_originalOperationB = primitiveB->GetCSGOperation();
    }
}

bool CSGOperationCommand::Execute() {
    if (!m_model || !m_primitiveA || !m_primitiveB) return false;

    // Set the CSG operation on primitiveB to combine with primitiveA
    m_primitiveB->SetCSGOperation(m_operation);
    m_primitiveB->GetParameters().smoothness = m_smoothness;

    // Reparent B under A
    if (m_originalParentB) {
        // Find and remove from original parent
        for (size_t i = 0; i < m_originalParentB->GetChildren().size(); ++i) {
            if (m_originalParentB->GetChildren()[i].get() == m_primitiveB) {
                m_ownedResult = std::move(m_originalParentB->GetChildren()[i]);
                m_originalParentB->RemoveChild(i);
                break;
            }
        }
    }

    if (m_ownedResult) {
        m_resultPrimitive = m_primitiveA->AddChild(std::move(m_ownedResult));
    }

    return m_resultPrimitive != nullptr;
}

bool CSGOperationCommand::Undo() {
    if (!m_model || !m_primitiveA || !m_resultPrimitive) return false;

    // Remove from primitiveA
    for (size_t i = 0; i < m_primitiveA->GetChildren().size(); ++i) {
        if (m_primitiveA->GetChildren()[i].get() == m_resultPrimitive) {
            m_ownedResult = std::move(m_primitiveA->GetChildren()[i]);
            m_primitiveA->RemoveChild(i);
            break;
        }
    }

    // Restore original operation
    if (m_ownedResult) {
        m_ownedResult->SetCSGOperation(m_originalOperationB);
    }

    // Restore to original parent
    if (m_originalParentB && m_ownedResult) {
        m_primitiveB = m_originalParentB->AddChild(std::move(m_ownedResult));
    }

    m_resultPrimitive = nullptr;
    return true;
}

std::string CSGOperationCommand::GetName() const {
    const char* opName = "CSG";
    switch (m_operation) {
        case CSGOperation::Union:              opName = "Union"; break;
        case CSGOperation::Subtraction:        opName = "Subtract"; break;
        case CSGOperation::Intersection:       opName = "Intersect"; break;
        case CSGOperation::SmoothUnion:        opName = "Smooth Union"; break;
        case CSGOperation::SmoothSubtraction:  opName = "Smooth Subtract"; break;
        case CSGOperation::SmoothIntersection: opName = "Smooth Intersect"; break;
    }
    return std::string("CSG ") + opName;
}

CommandTypeId CSGOperationCommand::GetTypeId() const {
    return GetCommandTypeId<CSGOperationCommand>();
}

// =============================================================================
// ModifySDFParametersCommand
// =============================================================================

ModifySDFParametersCommand::ModifySDFParametersCommand(
    SDFPrimitive* primitive,
    const SDFParameters& newParams
)
    : m_primitive(primitive)
    , m_newParams(newParams)
{
    if (primitive) {
        m_oldParams = primitive->GetParameters();
    }
}

bool ModifySDFParametersCommand::Execute() {
    if (!m_primitive) return false;
    m_primitive->SetParameters(m_newParams);
    return true;
}

bool ModifySDFParametersCommand::Undo() {
    if (!m_primitive) return false;
    m_primitive->SetParameters(m_oldParams);
    return true;
}

std::string ModifySDFParametersCommand::GetName() const {
    return "Modify Parameters";
}

CommandTypeId ModifySDFParametersCommand::GetTypeId() const {
    return GetCommandTypeId<ModifySDFParametersCommand>();
}

bool ModifySDFParametersCommand::CanMergeWith(const ICommand& other) const {
    if (other.GetTypeId() != GetTypeId()) return false;
    auto* otherCmd = static_cast<const ModifySDFParametersCommand*>(&other);
    return m_primitive == otherCmd->m_primitive && IsWithinMergeWindow();
}

bool ModifySDFParametersCommand::MergeWith(const ICommand& other) {
    auto* otherCmd = static_cast<const ModifySDFParametersCommand*>(&other);
    m_newParams = otherCmd->m_newParams;
    return true;
}

// =============================================================================
// TransformSDFPrimitiveCommand
// =============================================================================

TransformSDFPrimitiveCommand::TransformSDFPrimitiveCommand(
    SDFPrimitive* primitive,
    const SDFTransform& newTransform
)
    : m_primitive(primitive)
    , m_newTransform(newTransform)
{
    if (primitive) {
        m_oldTransform = primitive->GetLocalTransform();
    }
}

bool TransformSDFPrimitiveCommand::Execute() {
    if (!m_primitive) return false;
    m_primitive->SetLocalTransform(m_newTransform);
    return true;
}

bool TransformSDFPrimitiveCommand::Undo() {
    if (!m_primitive) return false;
    m_primitive->SetLocalTransform(m_oldTransform);
    return true;
}

std::string TransformSDFPrimitiveCommand::GetName() const {
    return "Transform Primitive";
}

CommandTypeId TransformSDFPrimitiveCommand::GetTypeId() const {
    return GetCommandTypeId<TransformSDFPrimitiveCommand>();
}

bool TransformSDFPrimitiveCommand::CanMergeWith(const ICommand& other) const {
    if (other.GetTypeId() != GetTypeId()) return false;
    auto* otherCmd = static_cast<const TransformSDFPrimitiveCommand*>(&other);
    return m_primitive == otherCmd->m_primitive && IsWithinMergeWindow();
}

bool TransformSDFPrimitiveCommand::MergeWith(const ICommand& other) {
    auto* otherCmd = static_cast<const TransformSDFPrimitiveCommand*>(&other);
    m_newTransform = otherCmd->m_newTransform;
    return true;
}

// =============================================================================
// MirrorSDFPrimitiveCommand
// =============================================================================

MirrorSDFPrimitiveCommand::MirrorSDFPrimitiveCommand(
    SDFModel* model,
    SDFPrimitive* primitive,
    Axis axis
)
    : m_model(model)
    , m_original(primitive)
    , m_axis(axis)
{
}

bool MirrorSDFPrimitiveCommand::Execute() {
    if (!m_model || !m_original) return false;

    // Clone the primitive
    m_ownedMirrored = m_original->Clone();
    if (!m_ownedMirrored) return false;

    // Apply mirror transform
    SDFTransform transform = m_ownedMirrored->GetLocalTransform();
    switch (m_axis) {
        case Axis::X:
            transform.position.x = -transform.position.x;
            transform.scale.x = -transform.scale.x;
            break;
        case Axis::Y:
            transform.position.y = -transform.position.y;
            transform.scale.y = -transform.scale.y;
            break;
        case Axis::Z:
            transform.position.z = -transform.position.z;
            transform.scale.z = -transform.scale.z;
            break;
    }
    m_ownedMirrored->SetLocalTransform(transform);
    m_ownedMirrored->SetName(m_original->GetName() + "_mirrored");

    // Add to same parent
    SDFPrimitive* parent = m_original->GetParent();
    if (parent) {
        m_mirroredPtr = parent->AddChild(std::move(m_ownedMirrored));
    } else if (m_model->GetRoot()) {
        m_mirroredPtr = m_model->GetRoot()->AddChild(std::move(m_ownedMirrored));
    }

    return m_mirroredPtr != nullptr;
}

bool MirrorSDFPrimitiveCommand::Undo() {
    if (!m_model || !m_mirroredPtr) return false;

    SDFPrimitive* parent = m_mirroredPtr->GetParent();
    if (parent) {
        for (size_t i = 0; i < parent->GetChildren().size(); ++i) {
            if (parent->GetChildren()[i].get() == m_mirroredPtr) {
                m_ownedMirrored = std::move(parent->GetChildren()[i]);
                parent->RemoveChild(i);
                break;
            }
        }
    }

    m_mirroredPtr = nullptr;
    return true;
}

std::string MirrorSDFPrimitiveCommand::GetName() const {
    const char* axisName = "X";
    switch (m_axis) {
        case Axis::X: axisName = "X"; break;
        case Axis::Y: axisName = "Y"; break;
        case Axis::Z: axisName = "Z"; break;
    }
    return std::string("Mirror ") + axisName;
}

CommandTypeId MirrorSDFPrimitiveCommand::GetTypeId() const {
    return GetCommandTypeId<MirrorSDFPrimitiveCommand>();
}

// =============================================================================
// SDFToolbox Implementation
// =============================================================================

SDFToolbox::SDFToolbox() {
    // Initialize with default presets
    LoadDefaultPresets();
}

SDFToolbox::~SDFToolbox() {
    // Save presets on destruction
    SavePresetsToFile();
}

// =========================================================================
// Model Management
// =========================================================================

void SDFToolbox::SetActiveModel(SDFModel* model) {
    if (m_activeModel != model) {
        m_activeModel = model;
        ClearSelection();
        m_csgTreeNeedsRebuild = true;
    }
}

// =========================================================================
// Selection
// =========================================================================

void SDFToolbox::SelectPrimitive(SDFPrimitive* primitive, bool addToSelection) {
    if (!addToSelection) {
        ClearSelection();
    }

    if (primitive && m_selectedSet.find(primitive) == m_selectedSet.end()) {
        m_selectedPrimitives.push_back(primitive);
        m_selectedSet.insert(primitive);
        NotifySelectionChanged();

        if (Callbacks.onPrimitiveSelected) {
            Callbacks.onPrimitiveSelected(primitive);
        }
    }
}

void SDFToolbox::ClearSelection() {
    if (!m_selectedPrimitives.empty()) {
        m_selectedPrimitives.clear();
        m_selectedSet.clear();
        NotifySelectionChanged();
    }
}

SDFPrimitive* SDFToolbox::GetPrimarySelection() const {
    return m_selectedPrimitives.empty() ? nullptr : m_selectedPrimitives.back();
}

// =========================================================================
// Tool Mode
// =========================================================================

void SDFToolbox::SetToolMode(SDFToolMode mode) {
    if (m_toolMode != mode) {
        m_toolMode = mode;
        CancelDragCreate();

        if (Callbacks.onToolModeChanged) {
            Callbacks.onToolModeChanged(mode);
        }
    }
}

void SDFToolbox::SetActivePrimitiveType(SDFPrimitiveType type) {
    m_activePrimitiveType = type;
}

// =========================================================================
// Primitive Creation
// =========================================================================

SDFPrimitive* SDFToolbox::CreatePrimitive(
    SDFPrimitiveType type,
    const glm::vec3& position,
    const SDFParameters& parameters
) {
    if (!m_activeModel) return nullptr;

    glm::vec3 finalPos = m_snapToGrid ? SnapToGrid(position) : position;
    SDFParameters params = parameters;

    // Use defaults if parameters are empty
    if (params.radius == 0.5f && params.dimensions == glm::vec3(1.0f)) {
        params = GetDefaultParameters(type);
    }

    // Create command
    auto cmd = std::make_unique<CreateSDFPrimitiveCommand>(
        m_activeModel, type, finalPos, params, nullptr
    );

    SDFPrimitive* result = nullptr;

    if (m_commandHistory) {
        if (m_commandHistory->ExecuteCommand(std::move(cmd))) {
            auto* lastCmd = static_cast<const CreateSDFPrimitiveCommand*>(m_commandHistory->PeekUndo());
            result = lastCmd ? lastCmd->GetCreatedPrimitive() : nullptr;
        }
    } else {
        if (cmd->Execute()) {
            result = cmd->GetCreatedPrimitive();
        }
    }

    if (result) {
        // Update naming counter
        m_primitiveCounters[type]++;
        result->SetName(GeneratePrimitiveName(type));

        // Select the new primitive
        SelectPrimitive(result);

        // Mark tree for rebuild
        m_csgTreeNeedsRebuild = true;

        // Notify
        if (Callbacks.onPrimitiveCreated) {
            Callbacks.onPrimitiveCreated(result);
        }
    }

    return result;
}

SDFPrimitive* SDFToolbox::CreateFromPreset(
    const SDFPrimitivePreset& preset,
    const glm::vec3& position
) {
    if (!m_activeModel) return nullptr;

    auto primitive = preset.CreatePrimitive();
    SDFTransform transform = primitive->GetLocalTransform();
    transform.position = m_snapToGrid ? SnapToGrid(position) : position;
    primitive->SetLocalTransform(transform);

    auto cmd = std::make_unique<CreateSDFPrimitiveCommand>(
        m_activeModel, std::move(primitive), nullptr
    );

    SDFPrimitive* result = nullptr;

    if (m_commandHistory) {
        if (m_commandHistory->ExecuteCommand(std::move(cmd))) {
            auto* lastCmd = static_cast<const CreateSDFPrimitiveCommand*>(m_commandHistory->PeekUndo());
            result = lastCmd ? lastCmd->GetCreatedPrimitive() : nullptr;
        }
    } else {
        if (cmd->Execute()) {
            result = cmd->GetCreatedPrimitive();
        }
    }

    if (result) {
        SelectPrimitive(result);
        m_csgTreeNeedsRebuild = true;

        if (Callbacks.onPrimitiveCreated) {
            Callbacks.onPrimitiveCreated(result);
        }
    }

    return result;
}

void SDFToolbox::BeginDragCreate(SDFPrimitiveType type, const glm::vec3& startPos) {
    m_isDragCreating = true;
    m_dragStartPos = m_snapToGrid ? SnapToGrid(startPos) : startPos;
    m_dragCurrentPos = m_dragStartPos;
    m_activePrimitiveType = type;

    // Create preview primitive (not in model yet)
    m_dragPreviewPrimitive = CreatePrimitive(type, m_dragStartPos, GetDefaultParameters(type));
}

void SDFToolbox::UpdateDragCreate(const glm::vec3& currentPos) {
    if (!m_isDragCreating || !m_dragPreviewPrimitive) return;

    m_dragCurrentPos = m_snapToGrid ? SnapToGrid(currentPos) : currentPos;

    // Calculate new parameters based on drag
    SDFParameters params = CalculateParametersFromDrag(
        m_activePrimitiveType,
        m_dragStartPos,
        m_dragCurrentPos
    );

    m_dragPreviewPrimitive->SetParameters(params);
}

SDFPrimitive* SDFToolbox::EndDragCreate() {
    if (!m_isDragCreating) return nullptr;

    SDFPrimitive* result = m_dragPreviewPrimitive;
    m_isDragCreating = false;
    m_dragPreviewPrimitive = nullptr;

    return result;
}

void SDFToolbox::CancelDragCreate() {
    if (m_isDragCreating && m_dragPreviewPrimitive) {
        // Delete the preview primitive
        if (m_activeModel) {
            m_activeModel->DeletePrimitive(m_dragPreviewPrimitive);
        }
    }

    m_isDragCreating = false;
    m_dragPreviewPrimitive = nullptr;
}

// =========================================================================
// CSG Operations
// =========================================================================

SDFPrimitive* SDFToolbox::ApplyCSGOperation(CSGOperation operation, float smoothness) {
    if (m_selectedPrimitives.size() < 2 || !m_activeModel) return nullptr;

    SDFPrimitive* primitiveA = m_selectedPrimitives[0];
    SDFPrimitive* primitiveB = m_selectedPrimitives[1];

    auto cmd = std::make_unique<CSGOperationCommand>(
        m_activeModel, primitiveA, primitiveB, operation, smoothness
    );

    SDFPrimitive* result = nullptr;

    if (m_commandHistory) {
        if (m_commandHistory->ExecuteCommand(std::move(cmd))) {
            auto* lastCmd = static_cast<const CSGOperationCommand*>(m_commandHistory->PeekUndo());
            result = lastCmd ? lastCmd->GetResultPrimitive() : nullptr;
        }
    } else {
        if (cmd->Execute()) {
            result = cmd->GetResultPrimitive();
        }
    }

    if (result) {
        // Clear selection and select result
        ClearSelection();
        SelectPrimitive(primitiveA);  // Select the parent of the CSG result

        m_csgTreeNeedsRebuild = true;

        if (Callbacks.onCSGApplied) {
            Callbacks.onCSGApplied(result);
        }
    }

    return result;
}

void SDFToolbox::SetCSGPreviewOperation(CSGOperation operation) {
    m_csgPreviewOperation = operation;
}

void SDFToolbox::ClearCSGPreview() {
    m_csgPreviewOperation.reset();
}

// =========================================================================
// Quick Actions
// =========================================================================

std::vector<SDFPrimitive*> SDFToolbox::DuplicateSelected() {
    std::vector<SDFPrimitive*> duplicates;

    if (!m_activeModel) return duplicates;

    for (SDFPrimitive* original : m_selectedPrimitives) {
        auto clone = original->Clone();
        if (clone) {
            // Offset position slightly
            SDFTransform transform = clone->GetLocalTransform();
            transform.position += glm::vec3(0.5f, 0.0f, 0.5f);
            clone->SetLocalTransform(transform);
            clone->SetName(original->GetName() + "_copy");

            auto cmd = std::make_unique<CreateSDFPrimitiveCommand>(
                m_activeModel, std::move(clone), original->GetParent()
            );

            if (m_commandHistory) {
                if (m_commandHistory->ExecuteCommand(std::move(cmd))) {
                    auto* lastCmd = static_cast<const CreateSDFPrimitiveCommand*>(m_commandHistory->PeekUndo());
                    if (lastCmd && lastCmd->GetCreatedPrimitive()) {
                        duplicates.push_back(lastCmd->GetCreatedPrimitive());
                    }
                }
            } else {
                if (cmd->Execute()) {
                    duplicates.push_back(cmd->GetCreatedPrimitive());
                }
            }
        }
    }

    // Select duplicates
    ClearSelection();
    for (SDFPrimitive* dup : duplicates) {
        SelectPrimitive(dup, true);
    }

    m_csgTreeNeedsRebuild = true;
    return duplicates;
}

SDFPrimitive* SDFToolbox::MirrorSelected(int axis) {
    SDFPrimitive* primary = GetPrimarySelection();
    if (!primary || !m_activeModel) return nullptr;

    MirrorSDFPrimitiveCommand::Axis mirrorAxis;
    switch (axis) {
        case 0: mirrorAxis = MirrorSDFPrimitiveCommand::Axis::X; break;
        case 1: mirrorAxis = MirrorSDFPrimitiveCommand::Axis::Y; break;
        case 2: mirrorAxis = MirrorSDFPrimitiveCommand::Axis::Z; break;
        default: return nullptr;
    }

    auto cmd = std::make_unique<MirrorSDFPrimitiveCommand>(m_activeModel, primary, mirrorAxis);

    SDFPrimitive* result = nullptr;

    if (m_commandHistory) {
        if (m_commandHistory->ExecuteCommand(std::move(cmd))) {
            auto* lastCmd = static_cast<const MirrorSDFPrimitiveCommand*>(m_commandHistory->PeekUndo());
            result = lastCmd ? lastCmd->GetMirroredPrimitive() : nullptr;
        }
    } else {
        if (cmd->Execute()) {
            result = cmd->GetMirroredPrimitive();
        }
    }

    if (result) {
        SelectPrimitive(result, true);
        m_csgTreeNeedsRebuild = true;
    }

    return result;
}

void SDFToolbox::CenterToOrigin() {
    SDFPrimitive* primary = GetPrimarySelection();
    if (!primary) return;

    SDFTransform transform = primary->GetLocalTransform();
    transform.position = glm::vec3(0.0f);

    auto cmd = std::make_unique<TransformSDFPrimitiveCommand>(primary, transform);

    if (m_commandHistory) {
        m_commandHistory->ExecuteCommand(std::move(cmd));
    } else {
        cmd->Execute();
    }
}

void SDFToolbox::ResetTransform() {
    SDFPrimitive* primary = GetPrimarySelection();
    if (!primary) return;

    SDFTransform transform = SDFTransform::Identity();

    auto cmd = std::make_unique<TransformSDFPrimitiveCommand>(primary, transform);

    if (m_commandHistory) {
        m_commandHistory->ExecuteCommand(std::move(cmd));
    } else {
        cmd->Execute();
    }
}

std::string SDFToolbox::ConvertToMesh(const SDFMeshSettings& settings) {
    if (!m_activeModel) return "";

    auto mesh = m_activeModel->GenerateMesh(settings);
    if (!mesh) return "";

    // Generate path
    std::string path = "assets/meshes/" + m_activeModel->GetName() + ".mesh";

    // FUTURE: Save mesh to file
    // mesh->SaveToFile(path);

    if (Callbacks.onConvertedToMesh) {
        Callbacks.onConvertedToMesh(m_activeModel, path);
    }

    return path;
}

SDFModel* SDFToolbox::ConvertFromMesh(const std::string& meshPath) {
    // FUTURE: Implement mesh to SDF conversion
    // This would use a voxelization approach

    if (Callbacks.onConvertedFromMesh) {
        // Callbacks.onConvertedFromMesh(meshPath, newModel);
    }

    return nullptr;
}

// =========================================================================
// Presets
// =========================================================================

void SDFToolbox::SavePreset(const std::string& name, const std::string& category) {
    SDFPrimitive* primary = GetPrimarySelection();
    if (!primary) return;

    SDFPrimitivePreset preset;
    preset.name = name;
    preset.category = category;
    preset.type = primary->GetType();
    preset.parameters = primary->GetParameters();
    preset.material = primary->GetMaterial();

    // Check if preset with same name exists
    auto it = std::find_if(m_presets.begin(), m_presets.end(),
        [&name](const SDFPrimitivePreset& p) { return p.name == name; });

    if (it != m_presets.end()) {
        *it = preset;
    } else {
        m_presets.push_back(preset);
    }

    SavePresetsToFile();
}

const SDFPrimitivePreset* SDFToolbox::GetPreset(const std::string& name) const {
    auto it = std::find_if(m_presets.begin(), m_presets.end(),
        [&name](const SDFPrimitivePreset& p) { return p.name == name; });

    return it != m_presets.end() ? &(*it) : nullptr;
}

std::vector<const SDFPrimitivePreset*> SDFToolbox::GetPresetsByCategory(const std::string& category) const {
    std::vector<const SDFPrimitivePreset*> result;
    for (const auto& preset : m_presets) {
        if (preset.category == category) {
            result.push_back(&preset);
        }
    }
    return result;
}

bool SDFToolbox::DeletePreset(const std::string& name) {
    auto it = std::find_if(m_presets.begin(), m_presets.end(),
        [&name](const SDFPrimitivePreset& p) { return p.name == name; });

    if (it != m_presets.end()) {
        m_presets.erase(it);
        SavePresetsToFile();
        return true;
    }
    return false;
}

void SDFToolbox::TogglePresetFavorite(const std::string& name) {
    auto it = std::find_if(m_presets.begin(), m_presets.end(),
        [&name](const SDFPrimitivePreset& p) { return p.name == name; });

    if (it != m_presets.end()) {
        it->isFavorite = !it->isFavorite;
        SavePresetsToFile();
    }
}

std::vector<const SDFPrimitivePreset*> SDFToolbox::GetFavoritePresets() const {
    std::vector<const SDFPrimitivePreset*> result;
    for (const auto& preset : m_presets) {
        if (preset.isFavorite) {
            result.push_back(&preset);
        }
    }
    return result;
}

// =========================================================================
// Asset Library
// =========================================================================

void SDFToolbox::RefreshAssetLibrary() {
    m_libraryItems.clear();

    try {
        std::filesystem::path libPath(m_libraryPath);
        if (std::filesystem::exists(libPath)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(libPath)) {
                if (entry.is_regular_file()) {
                    std::string ext = entry.path().extension().string();
                    if (ext == ".sdf" || ext == ".json") {
                        SDFAssetLibraryItem item;
                        item.name = entry.path().stem().string();
                        item.path = entry.path().string();
                        item.category = entry.path().parent_path().filename().string();
                        m_libraryItems.push_back(item);
                    }
                }
            }
        }
    } catch (...) {
        // Filesystem error - ignore
    }

    m_libraryNeedsRefresh = false;
}

SDFModel* SDFToolbox::LoadAsset(const std::string& path) {
    // FUTURE: Implement asset loading
    // auto model = std::make_unique<SDFModel>();
    // model->LoadFromFile(path);
    return nullptr;
}

// =========================================================================
// Configuration
// =========================================================================

void SDFToolbox::SetSnapToGrid(bool snap, float gridSize) {
    m_snapToGrid = snap;
    m_gridSize = gridSize;
}

// =========================================================================
// EditorPanel Overrides
// =========================================================================

void SDFToolbox::OnInitialize() {
    LoadPresetsFromFile();
    RefreshAssetLibrary();
}

void SDFToolbox::OnShutdown() {
    SavePresetsToFile();
}

void SDFToolbox::Update(float deltaTime) {
    (void)deltaTime;

    // Handle keyboard shortcuts
    HandleKeyboardShortcuts();

    // Rebuild CSG tree if needed
    if (m_csgTreeNeedsRebuild && m_activeModel && m_activeModel->GetRoot()) {
        RebuildCSGTree();
    }
}

void SDFToolbox::OnRender() {
    // Tool mode selector
    RenderToolModeSelector();

    ImGui::Separator();

    // Primitive creation section
    if (ImGui::CollapsingHeader("Primitives", m_showPrimitiveSection ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
        m_showPrimitiveSection = true;
        RenderPrimitiveGrid();
    } else {
        m_showPrimitiveSection = false;
    }

    // CSG operations section
    if (ImGui::CollapsingHeader("CSG Operations", m_showCSGSection ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
        m_showCSGSection = true;
        RenderCSGOperations();
    } else {
        m_showCSGSection = false;
    }

    // CSG tree view
    if (m_activeModel && ImGui::CollapsingHeader("CSG Tree")) {
        RenderCSGTreeView();
    }

    // Quick actions section
    if (ImGui::CollapsingHeader("Quick Actions", m_showQuickActionsSection ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
        m_showQuickActionsSection = true;
        RenderQuickActions();
    } else {
        m_showQuickActionsSection = false;
    }

    // Parameter editor (when primitive selected)
    if (GetPrimarySelection()) {
        if (ImGui::CollapsingHeader("Parameters", m_showParameterSection ? ImGuiTreeNodeFlags_DefaultOpen : 0)) {
            m_showParameterSection = true;
            RenderParameterEditor();
        } else {
            m_showParameterSection = false;
        }
    }

    // Preset library
    if (ImGui::CollapsingHeader("Presets")) {
        RenderPresetLibrary();
    }

    // Asset browser
    if (ImGui::CollapsingHeader("Asset Library")) {
        RenderAssetBrowser();
    }
}

void SDFToolbox::OnRenderToolbar() {
    auto& theme = EditorTheme::Instance();

    // Tool mode buttons
    for (int i = 0; i < 3; ++i) {
        SDFToolMode mode = static_cast<SDFToolMode>(i);
        bool isActive = m_toolMode == mode;

        if (i > 0) ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_Button,
            isActive ? EditorTheme::ToImVec4(theme.GetColors().accent)
                     : EditorTheme::ToImVec4(theme.GetColors().button));

        if (ImGui::Button(GetToolModeIcon(mode))) {
            SetToolMode(mode);
        }

        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s Mode", GetToolModeName(mode));
        }
    }

    ImGui::SameLine();
    ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
    ImGui::SameLine();

    // Snap toggle
    if (ImGui::Checkbox("Snap", &m_snapToGrid)) {
        // Snap toggled
    }

    if (m_snapToGrid) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60.0f);
        ImGui::DragFloat("##GridSize", &m_gridSize, 0.1f, 0.1f, 10.0f, "%.2f");
    }
}

void SDFToolbox::OnRenderMenuBar() {
    if (ImGui::BeginMenu("Create")) {
        for (int i = 0; i < static_cast<int>(SDFPrimitiveType::Custom); ++i) {
            SDFPrimitiveType type = static_cast<SDFPrimitiveType>(i);
            if (ImGui::MenuItem(GetPrimitiveName(type))) {
                CreatePrimitive(type, m_defaultPosition);
            }
        }
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("CSG")) {
        bool hasTwo = m_selectedPrimitives.size() >= 2;

        if (ImGui::MenuItem("Union", "Ctrl+U", false, hasTwo)) {
            ApplyCSGOperation(CSGOperation::Union);
        }
        if (ImGui::MenuItem("Subtract", "Ctrl+S", false, hasTwo)) {
            ApplyCSGOperation(CSGOperation::Subtraction);
        }
        if (ImGui::MenuItem("Intersect", "Ctrl+I", false, hasTwo)) {
            ApplyCSGOperation(CSGOperation::Intersection);
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Smooth Union", nullptr, false, hasTwo)) {
            ApplyCSGOperation(CSGOperation::SmoothUnion, m_defaultSmoothness);
        }
        if (ImGui::MenuItem("Smooth Subtract", nullptr, false, hasTwo)) {
            ApplyCSGOperation(CSGOperation::SmoothSubtraction, m_defaultSmoothness);
        }
        if (ImGui::MenuItem("Smooth Intersect", nullptr, false, hasTwo)) {
            ApplyCSGOperation(CSGOperation::SmoothIntersection, m_defaultSmoothness);
        }

        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
        bool hasSelection = !m_selectedPrimitives.empty();

        if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, hasSelection)) {
            DuplicateSelected();
        }
        if (ImGui::MenuItem("Delete", "Del", false, hasSelection)) {
            // Delete selected
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Mirror X", nullptr, false, hasSelection)) {
            MirrorSelected(0);
        }
        if (ImGui::MenuItem("Mirror Y", nullptr, false, hasSelection)) {
            MirrorSelected(1);
        }
        if (ImGui::MenuItem("Mirror Z", nullptr, false, hasSelection)) {
            MirrorSelected(2);
        }

        ImGui::Separator();

        if (ImGui::MenuItem("Center to Origin", nullptr, false, hasSelection)) {
            CenterToOrigin();
        }
        if (ImGui::MenuItem("Reset Transform", nullptr, false, hasSelection)) {
            ResetTransform();
        }

        ImGui::EndMenu();
    }
}

void SDFToolbox::OnSearchChanged(const std::string& filter) {
    m_presetFilter = filter;
    m_libraryFilter = filter;
}

bool SDFToolbox::CanUndo() const {
    return m_commandHistory && m_commandHistory->CanUndo();
}

bool SDFToolbox::CanRedo() const {
    return m_commandHistory && m_commandHistory->CanRedo();
}

void SDFToolbox::OnUndo() {
    if (m_commandHistory) {
        m_commandHistory->Undo();
        m_csgTreeNeedsRebuild = true;
    }
}

void SDFToolbox::OnRedo() {
    if (m_commandHistory) {
        m_commandHistory->Redo();
        m_csgTreeNeedsRebuild = true;
    }
}

// =========================================================================
// Rendering Sections
// =========================================================================

void SDFToolbox::RenderToolModeSelector() {
    ImGui::Text("Tool Mode:");
    ImGui::SameLine();

    const char* modeNames[] = { "Create", "Edit", "CSG" };
    int currentMode = static_cast<int>(m_toolMode);

    ImGui::SetNextItemWidth(-1);
    if (ImGui::Combo("##ToolMode", &currentMode, modeNames, 3)) {
        SetToolMode(static_cast<SDFToolMode>(currentMode));
    }
}

void SDFToolbox::RenderPrimitiveGrid() {
    auto& theme = EditorTheme::Instance();

    const float buttonSize = 48.0f;
    const float padding = 4.0f;
    float availWidth = ImGui::GetContentRegionAvail().x;
    int buttonsPerRow = std::max(1, static_cast<int>(availWidth / (buttonSize + padding)));

    struct PrimitiveInfo {
        SDFPrimitiveType type;
        const char* icon;
        const char* name;
    };

    PrimitiveInfo primitives[] = {
        { SDFPrimitiveType::Sphere,     "\xef\x84\x91", "Sphere" },
        { SDFPrimitiveType::Box,        "\xef\x80\xa6", "Box" },
        { SDFPrimitiveType::Cylinder,   "\xef\x83\x87", "Cylinder" },
        { SDFPrimitiveType::Capsule,    "\xef\x92\x8a", "Capsule" },
        { SDFPrimitiveType::Cone,       "\xef\x83\xad", "Cone" },
        { SDFPrimitiveType::Torus,      "\xef\x85\x91", "Torus" },
        { SDFPrimitiveType::Plane,      "\xef\x80\x83", "Plane" },
        { SDFPrimitiveType::RoundedBox, "\xef\x81\x83", "Rounded Box" },
        { SDFPrimitiveType::Ellipsoid,  "\xef\x84\x91", "Ellipsoid" },
        { SDFPrimitiveType::Pyramid,    "\xef\x83\xad", "Pyramid" },
        { SDFPrimitiveType::Prism,      "\xef\x85\x9c", "Prism" },
    };

    int count = 0;
    for (const auto& prim : primitives) {
        if (count > 0 && count % buttonsPerRow != 0) {
            ImGui::SameLine();
        }

        bool isActive = m_activePrimitiveType == prim.type;

        ImGui::PushStyleColor(ImGuiCol_Button,
            isActive ? EditorTheme::ToImVec4(theme.GetColors().accent)
                     : EditorTheme::ToImVec4(theme.GetColors().button));

        ImGui::PushID(static_cast<int>(prim.type));

        if (ImGui::Button(prim.icon, ImVec2(buttonSize, buttonSize))) {
            if (m_toolMode == SDFToolMode::Create) {
                // In create mode, clicking creates immediately
                CreatePrimitive(prim.type, m_defaultPosition);
            } else {
                // Otherwise, just select the type
                SetActivePrimitiveType(prim.type);
            }
        }

        ImGui::PopID();
        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\nShortcut: %d", prim.name, count + 1);
        }

        // Context menu for shift+click
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && ImGui::GetIO().KeyShift) {
            if (Callbacks.onPrecisePositionDialog) {
                glm::vec3 position = m_defaultPosition;
                glm::vec3 size = glm::vec3(1.0f);
                if (Callbacks.onPrecisePositionDialog(position, size)) {
                    SDFParameters params = GetDefaultParameters(prim.type);
                    // Apply size to parameters
                    params.radius = size.x * 0.5f;
                    params.dimensions = size;
                    CreatePrimitive(prim.type, position, params);
                }
            }
        }

        count++;
    }
}

void SDFToolbox::RenderCSGOperations() {
    auto& theme = EditorTheme::Instance();

    bool hasTwo = m_selectedPrimitives.size() >= 2;

    ImGui::BeginDisabled(!hasTwo);

    // Operation buttons
    ImGui::Text("Operations:");

    if (ImGui::Button("Union", ImVec2(80, 0))) {
        ApplyCSGOperation(m_smoothCSG ? CSGOperation::SmoothUnion : CSGOperation::Union,
                          m_smoothCSG ? m_csgSmoothness : 0.0f);
    }
    ImGui::SameLine();

    if (ImGui::Button("Subtract", ImVec2(80, 0))) {
        ApplyCSGOperation(m_smoothCSG ? CSGOperation::SmoothSubtraction : CSGOperation::Subtraction,
                          m_smoothCSG ? m_csgSmoothness : 0.0f);
    }
    ImGui::SameLine();

    if (ImGui::Button("Intersect", ImVec2(80, 0))) {
        ApplyCSGOperation(m_smoothCSG ? CSGOperation::SmoothIntersection : CSGOperation::Intersection,
                          m_smoothCSG ? m_csgSmoothness : 0.0f);
    }

    ImGui::EndDisabled();

    // Smooth options
    ImGui::Checkbox("Smooth", &m_smoothCSG);

    if (m_smoothCSG) {
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::SliderFloat("Blend", &m_csgSmoothness, 0.01f, 1.0f, "%.2f");
    }

    // Selection info
    if (m_selectedPrimitives.size() == 0) {
        ImGui::TextColored(EditorTheme::ToImVec4(theme.GetColors().textSecondary),
                           "Select two primitives to combine");
    } else if (m_selectedPrimitives.size() == 1) {
        ImGui::TextColored(EditorTheme::ToImVec4(theme.GetColors().warning),
                           "Select one more primitive");
    } else {
        ImGui::TextColored(EditorTheme::ToImVec4(theme.GetColors().success),
                           "%zu primitives selected", m_selectedPrimitives.size());
    }
}

void SDFToolbox::RenderCSGTreeView() {
    if (m_csgTreeRoot.primitive) {
        RenderCSGTreeNode(m_csgTreeRoot);
    } else {
        ImGui::TextDisabled("No CSG tree");
    }
}

void SDFToolbox::RenderCSGTreeNode(const CSGTreeNode& node, int depth) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

    if (node.children.empty()) {
        flags |= ImGuiTreeNodeFlags_Leaf;
    }

    if (node.selected || (node.primitive && m_selectedSet.count(node.primitive) > 0)) {
        flags |= ImGuiTreeNodeFlags_Selected;
    }

    // Build label with operation indicator
    std::string label = node.displayName;
    if (depth > 0) {
        label = GetCSGOperationIcon(node.operation) + std::string(" ") + label;
    }

    bool opened = ImGui::TreeNodeEx(label.c_str(), flags);

    // Handle selection
    if (ImGui::IsItemClicked() && node.primitive) {
        bool addToSelection = ImGui::GetIO().KeyCtrl;
        SelectPrimitive(node.primitive, addToSelection);
    }

    if (opened) {
        for (const auto& child : node.children) {
            RenderCSGTreeNode(child, depth + 1);
        }
        ImGui::TreePop();
    }
}

void SDFToolbox::RenderQuickActions() {
    auto& theme = EditorTheme::Instance();

    bool hasSelection = !m_selectedPrimitives.empty();

    ImGui::BeginDisabled(!hasSelection);

    // Row 1: Transform actions
    if (ImGui::Button("Duplicate", ImVec2(-1, 0))) {
        DuplicateSelected();
    }

    // Row 2: Mirror buttons
    if (ImGui::Button("Mirror X", ImVec2(0, 0))) {
        MirrorSelected(0);
    }
    ImGui::SameLine();
    if (ImGui::Button("Mirror Y", ImVec2(0, 0))) {
        MirrorSelected(1);
    }
    ImGui::SameLine();
    if (ImGui::Button("Mirror Z", ImVec2(0, 0))) {
        MirrorSelected(2);
    }

    // Row 3: Transform reset
    if (ImGui::Button("Center", ImVec2(0, 0))) {
        CenterToOrigin();
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Transform", ImVec2(-1, 0))) {
        ResetTransform();
    }

    ImGui::EndDisabled();

    ImGui::Separator();

    // Conversion buttons
    ImGui::BeginDisabled(!m_activeModel);

    if (ImGui::Button("Convert to Mesh", ImVec2(-1, 0))) {
        ConvertToMesh();
    }

    ImGui::EndDisabled();

    if (ImGui::Button("Import Mesh as SDF...", ImVec2(-1, 0))) {
        // FUTURE: Open file dialog
    }
}

void SDFToolbox::RenderParameterEditor() {
    SDFPrimitive* primitive = GetPrimarySelection();
    if (!primitive) return;

    auto& theme = EditorTheme::Instance();

    // Name
    char nameBuffer[256];
    strncpy(nameBuffer, primitive->GetName().c_str(), sizeof(nameBuffer) - 1);
    nameBuffer[sizeof(nameBuffer) - 1] = '\0';

    if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
        primitive->SetName(nameBuffer);
    }

    // Type display
    ImGui::Text("Type: %s", GetPrimitiveName(primitive->GetType()));

    ImGui::Separator();

    // Parameters based on type
    SDFParameters params = primitive->GetParameters();
    bool changed = false;

    switch (primitive->GetType()) {
        case SDFPrimitiveType::Sphere:
            changed |= ImGui::DragFloat("Radius", &params.radius, 0.01f, 0.01f, 100.0f);
            break;

        case SDFPrimitiveType::Box:
            changed |= ImGui::DragFloat3("Dimensions", &params.dimensions.x, 0.01f, 0.01f, 100.0f);
            break;

        case SDFPrimitiveType::RoundedBox:
            changed |= ImGui::DragFloat3("Dimensions", &params.dimensions.x, 0.01f, 0.01f, 100.0f);
            changed |= ImGui::DragFloat("Corner Radius", &params.cornerRadius, 0.01f, 0.0f, 1.0f);
            break;

        case SDFPrimitiveType::Cylinder:
            changed |= ImGui::DragFloat("Height", &params.height, 0.01f, 0.01f, 100.0f);
            changed |= ImGui::DragFloat("Radius", &params.topRadius, 0.01f, 0.01f, 100.0f);
            params.bottomRadius = params.topRadius;
            break;

        case SDFPrimitiveType::Capsule:
            changed |= ImGui::DragFloat("Height", &params.height, 0.01f, 0.01f, 100.0f);
            changed |= ImGui::DragFloat("Radius", &params.topRadius, 0.01f, 0.01f, 100.0f);
            params.bottomRadius = params.topRadius;
            break;

        case SDFPrimitiveType::Cone:
            changed |= ImGui::DragFloat("Height", &params.height, 0.01f, 0.01f, 100.0f);
            changed |= ImGui::DragFloat("Top Radius", &params.topRadius, 0.01f, 0.0f, 100.0f);
            changed |= ImGui::DragFloat("Bottom Radius", &params.bottomRadius, 0.01f, 0.01f, 100.0f);
            break;

        case SDFPrimitiveType::Torus:
            changed |= ImGui::DragFloat("Major Radius", &params.majorRadius, 0.01f, 0.01f, 100.0f);
            changed |= ImGui::DragFloat("Minor Radius", &params.minorRadius, 0.01f, 0.01f, params.majorRadius);
            break;

        case SDFPrimitiveType::Ellipsoid:
            changed |= ImGui::DragFloat3("Radii", &params.radii.x, 0.01f, 0.01f, 100.0f);
            break;

        case SDFPrimitiveType::Pyramid:
            changed |= ImGui::DragFloat("Height", &params.height, 0.01f, 0.01f, 100.0f);
            changed |= ImGui::DragFloat("Base Size", &params.dimensions.x, 0.01f, 0.01f, 100.0f);
            break;

        case SDFPrimitiveType::Prism:
            changed |= ImGui::DragInt("Sides", &params.sides, 1, 3, 32);
            changed |= ImGui::DragFloat("Radius", &params.radius, 0.01f, 0.01f, 100.0f);
            changed |= ImGui::DragFloat("Height", &params.height, 0.01f, 0.01f, 100.0f);
            break;

        case SDFPrimitiveType::Plane:
            // Plane has no additional parameters (defined by transform)
            ImGui::TextDisabled("Plane defined by transform");
            break;

        default:
            break;
    }

    if (changed) {
        auto cmd = std::make_unique<ModifySDFParametersCommand>(primitive, params);

        if (m_commandHistory) {
            m_commandHistory->ExecuteCommand(std::move(cmd));
        } else {
            cmd->Execute();
        }
    }

    // CSG operation for this primitive
    ImGui::Separator();
    ImGui::Text("CSG Operation:");

    const char* opNames[] = { "Union", "Subtract", "Intersect", "Smooth Union", "Smooth Subtract", "Smooth Intersect" };
    int currentOp = static_cast<int>(primitive->GetCSGOperation());

    if (ImGui::Combo("##CSGOp", &currentOp, opNames, 6)) {
        primitive->SetCSGOperation(static_cast<CSGOperation>(currentOp));
        m_csgTreeNeedsRebuild = true;
    }

    if (currentOp >= 3) {  // Smooth operations
        if (ImGui::DragFloat("Smoothness", &params.smoothness, 0.01f, 0.0f, 1.0f)) {
            primitive->SetParameters(params);
        }
    }

    // Material preview
    ImGui::Separator();
    if (ImGui::CollapsingHeader("Material")) {
        SDFMaterial& material = primitive->GetMaterial();

        ImGui::ColorEdit4("Base Color", &material.baseColor.x);
        ImGui::SliderFloat("Metallic", &material.metallic, 0.0f, 1.0f);
        ImGui::SliderFloat("Roughness", &material.roughness, 0.0f, 1.0f);
        ImGui::SliderFloat("Emissive", &material.emissive, 0.0f, 10.0f);

        if (material.emissive > 0.0f) {
            ImGui::ColorEdit3("Emissive Color", &material.emissiveColor.x);
        }
    }
}

void SDFToolbox::RenderPresetLibrary() {
    // Filter
    ImGui::InputText("Filter##PresetFilter", m_presetFilterBuffer, sizeof(m_presetFilterBuffer));
    m_presetFilter = m_presetFilterBuffer;

    // Save preset button
    if (!m_selectedPrimitives.empty()) {
        ImGui::SameLine();
        if (ImGui::Button("Save")) {
            m_showPresetSaveDialog = true;
        }
    }

    // Favorites section
    auto favorites = GetFavoritePresets();
    if (!favorites.empty()) {
        if (ImGui::TreeNode("Favorites")) {
            for (const auto* preset : favorites) {
                if (m_presetFilter.empty() ||
                    preset->name.find(m_presetFilter) != std::string::npos) {

                    ImGui::PushID(preset->name.c_str());

                    if (ImGui::Selectable(preset->name.c_str())) {
                        CreateFromPreset(*preset, m_defaultPosition);
                    }

                    // Drag source
                    if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
                        ImGui::SetDragDropPayload("SDF_PRESET", &preset, sizeof(const SDFPrimitivePreset*));
                        ImGui::Text("%s", preset->name.c_str());
                        ImGui::EndDragDropSource();
                    }

                    ImGui::PopID();
                }
            }
            ImGui::TreePop();
        }
    }

    // All presets by category
    std::unordered_map<std::string, std::vector<const SDFPrimitivePreset*>> byCategory;
    for (const auto& preset : m_presets) {
        byCategory[preset.category].push_back(&preset);
    }

    for (const auto& [category, presets] : byCategory) {
        if (ImGui::TreeNode(category.c_str())) {
            for (const auto* preset : presets) {
                if (m_presetFilter.empty() ||
                    preset->name.find(m_presetFilter) != std::string::npos) {

                    ImGui::PushID(preset->name.c_str());

                    // Star for favorite
                    if (preset->isFavorite) {
                        ImGui::TextColored(ImVec4(1, 1, 0, 1), "*");
                        ImGui::SameLine();
                    }

                    if (ImGui::Selectable(preset->name.c_str())) {
                        CreateFromPreset(*preset, m_defaultPosition);
                    }

                    // Context menu
                    if (ImGui::BeginPopupContextItem()) {
                        if (ImGui::MenuItem(preset->isFavorite ? "Remove from Favorites" : "Add to Favorites")) {
                            const_cast<SDFToolbox*>(this)->TogglePresetFavorite(preset->name);
                        }
                        if (ImGui::MenuItem("Delete")) {
                            const_cast<SDFToolbox*>(this)->DeletePreset(preset->name);
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::PopID();
                }
            }
            ImGui::TreePop();
        }
    }

    // Save preset dialog
    if (m_showPresetSaveDialog) {
        ImGui::OpenPopup("Save Preset");
        m_showPresetSaveDialog = false;
    }

    if (ImGui::BeginPopupModal("Save Preset", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        static char nameBuffer[256] = "";
        static char categoryBuffer[256] = "Custom";

        ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer));
        ImGui::InputText("Category", categoryBuffer, sizeof(categoryBuffer));

        if (ImGui::Button("Save", ImVec2(120, 0))) {
            if (strlen(nameBuffer) > 0) {
                SavePreset(nameBuffer, categoryBuffer);
                nameBuffer[0] = '\0';
            }
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        if (ImGui::Button("Cancel", ImVec2(120, 0))) {
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

void SDFToolbox::RenderAssetBrowser() {
    // Refresh button
    if (ImGui::Button("Refresh")) {
        RefreshAssetLibrary();
    }

    ImGui::SameLine();

    // Filter
    ImGui::InputText("Filter##LibraryFilter", m_libraryFilterBuffer, sizeof(m_libraryFilterBuffer));
    m_libraryFilter = m_libraryFilterBuffer;

    // Asset grid
    if (m_libraryItems.empty()) {
        ImGui::TextDisabled("No SDF assets found in %s", m_libraryPath.c_str());
        return;
    }

    const float thumbnailSize = 64.0f;
    float availWidth = ImGui::GetContentRegionAvail().x;
    int itemsPerRow = std::max(1, static_cast<int>(availWidth / (thumbnailSize + 8.0f)));

    int count = 0;
    for (const auto& item : m_libraryItems) {
        if (!m_libraryFilter.empty() &&
            item.name.find(m_libraryFilter) == std::string::npos) {
            continue;
        }

        if (count > 0 && count % itemsPerRow != 0) {
            ImGui::SameLine();
        }

        ImGui::PushID(item.path.c_str());

        ImGui::BeginGroup();

        // Thumbnail or placeholder
        if (item.thumbnail) {
            // FUTURE: ImGui::Image with texture
            ImGui::Button("##Thumb", ImVec2(thumbnailSize, thumbnailSize));
        } else {
            ImGui::Button("SDF", ImVec2(thumbnailSize, thumbnailSize));
        }

        // Label
        ImGui::TextWrapped("%s", item.name.c_str());

        ImGui::EndGroup();

        // Drag source
        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("SDF_ASSET", item.path.c_str(), item.path.size() + 1);
            ImGui::Text("%s", item.name.c_str());
            ImGui::EndDragDropSource();
        }

        // Double-click to load
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
            LoadAsset(item.path);
        }

        ImGui::PopID();
        count++;
    }
}

void SDFToolbox::RenderPrimitiveButton(SDFPrimitiveType type, const char* icon, const char* tooltip) {
    // Helper for rendering individual primitive buttons (if needed)
}

// =========================================================================
// Input Handling
// =========================================================================

void SDFToolbox::HandleKeyboardShortcuts() {
    if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) return;

    ImGuiIO& io = ImGui::GetIO();

    // Number keys 1-9 for primitive shortcuts
    for (int i = 0; i < 9 && i < NUM_PRIMITIVE_SHORTCUTS; ++i) {
        if (ImGui::IsKeyPressed(static_cast<ImGuiKey>(ImGuiKey_1 + i)) && !io.KeyCtrl) {
            HandlePrimitiveShortcut(i);
        }
    }

    // Ctrl+G: Group as CSG union
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G)) {
        if (!io.KeyShift && m_selectedPrimitives.size() >= 2) {
            ApplyCSGOperation(CSGOperation::Union);
        }
    }

    // Ctrl+D: Duplicate
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
        DuplicateSelected();
    }

    // Delete: Delete selected
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !m_selectedPrimitives.empty()) {
        // FUTURE: Delete command
    }

    // Ctrl+Z: Undo
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z) && !io.KeyShift) {
        OnUndo();
    }

    // Ctrl+Shift+Z or Ctrl+Y: Redo
    if ((io.KeyCtrl && io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Z)) ||
        (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y))) {
        OnRedo();
    }
}

void SDFToolbox::HandlePrimitiveShortcut(int number) {
    if (number >= 0 && number < NUM_PRIMITIVE_SHORTCUTS) {
        if (m_toolMode == SDFToolMode::Create) {
            CreatePrimitive(m_shortcutTypes[number], m_defaultPosition);
        } else {
            SetActivePrimitiveType(m_shortcutTypes[number]);
        }
    }
}

// =========================================================================
// Utility
// =========================================================================

glm::vec3 SDFToolbox::SnapToGrid(const glm::vec3& position) const {
    return glm::vec3(
        std::round(position.x / m_gridSize) * m_gridSize,
        std::round(position.y / m_gridSize) * m_gridSize,
        std::round(position.z / m_gridSize) * m_gridSize
    );
}

std::string SDFToolbox::GeneratePrimitiveName(SDFPrimitiveType type) const {
    const char* baseName = GetPrimitiveName(type);
    uint32_t count = 0;

    auto it = m_primitiveCounters.find(type);
    if (it != m_primitiveCounters.end()) {
        count = it->second;
    }

    return std::string(baseName) + "_" + std::to_string(count);
}

void SDFToolbox::LoadDefaultPresets() {
    // Basic shapes
    m_presets.push_back({"Unit Sphere", "Basic", SDFPrimitiveType::Sphere, GetDefaultParameters(SDFPrimitiveType::Sphere), {}, false, ""});
    m_presets.push_back({"Unit Cube", "Basic", SDFPrimitiveType::Box, GetDefaultParameters(SDFPrimitiveType::Box), {}, false, ""});
    m_presets.push_back({"Unit Cylinder", "Basic", SDFPrimitiveType::Cylinder, GetDefaultParameters(SDFPrimitiveType::Cylinder), {}, false, ""});

    // Rounded shapes
    SDFParameters roundedBoxParams;
    roundedBoxParams.dimensions = glm::vec3(1.0f);
    roundedBoxParams.cornerRadius = 0.1f;
    m_presets.push_back({"Rounded Cube", "Rounded", SDFPrimitiveType::RoundedBox, roundedBoxParams, {}, false, ""});

    // Small sphere
    SDFParameters smallSphereParams;
    smallSphereParams.radius = 0.25f;
    m_presets.push_back({"Small Sphere", "Basic", SDFPrimitiveType::Sphere, smallSphereParams, {}, false, ""});

    // Large sphere
    SDFParameters largeSphereParams;
    largeSphereParams.radius = 2.0f;
    m_presets.push_back({"Large Sphere", "Basic", SDFPrimitiveType::Sphere, largeSphereParams, {}, false, ""});

    // Torus presets
    SDFParameters thinTorusParams;
    thinTorusParams.majorRadius = 0.5f;
    thinTorusParams.minorRadius = 0.05f;
    m_presets.push_back({"Thin Ring", "Torus", SDFPrimitiveType::Torus, thinTorusParams, {}, false, ""});

    SDFParameters thickTorusParams;
    thickTorusParams.majorRadius = 0.4f;
    thickTorusParams.minorRadius = 0.2f;
    m_presets.push_back({"Thick Ring", "Torus", SDFPrimitiveType::Torus, thickTorusParams, {}, false, ""});

    // Prism presets
    SDFParameters hexPrismParams;
    hexPrismParams.sides = 6;
    hexPrismParams.radius = 0.5f;
    hexPrismParams.height = 1.0f;
    m_presets.push_back({"Hexagonal Prism", "Prism", SDFPrimitiveType::Prism, hexPrismParams, {}, false, ""});

    SDFParameters triPrismParams;
    triPrismParams.sides = 3;
    triPrismParams.radius = 0.5f;
    triPrismParams.height = 1.0f;
    m_presets.push_back({"Triangular Prism", "Prism", SDFPrimitiveType::Prism, triPrismParams, {}, false, ""});
}

void SDFToolbox::SavePresetsToFile() {
    // FUTURE: Implement JSON serialization
    // std::ofstream file("config/sdf_presets.json");
    // ...
}

void SDFToolbox::LoadPresetsFromFile() {
    // FUTURE: Implement JSON deserialization
    // std::ifstream file("config/sdf_presets.json");
    // ...
}

void SDFToolbox::NotifySelectionChanged() {
    if (Callbacks.onSelectionChanged) {
        Callbacks.onSelectionChanged(m_selectedPrimitives);
    }
}

void SDFToolbox::RebuildCSGTree() {
    if (m_activeModel && m_activeModel->GetRoot()) {
        m_csgTreeRoot = CSGTreeNode::BuildFromPrimitive(m_activeModel->GetRoot());
    } else {
        m_csgTreeRoot = CSGTreeNode{};
    }
    m_csgTreeNeedsRebuild = false;
}

const char* SDFToolbox::GetPrimitiveIcon(SDFPrimitiveType type) const {
    switch (type) {
        case SDFPrimitiveType::Sphere:     return "\xef\x84\x91";
        case SDFPrimitiveType::Box:        return "\xef\x80\xa6";
        case SDFPrimitiveType::Cylinder:   return "\xef\x83\x87";
        case SDFPrimitiveType::Capsule:    return "\xef\x92\x8a";
        case SDFPrimitiveType::Cone:       return "\xef\x83\xad";
        case SDFPrimitiveType::Torus:      return "\xef\x85\x91";
        case SDFPrimitiveType::Plane:      return "\xef\x80\x83";
        case SDFPrimitiveType::RoundedBox: return "\xef\x81\x83";
        case SDFPrimitiveType::Ellipsoid:  return "\xef\x84\x91";
        case SDFPrimitiveType::Pyramid:    return "\xef\x83\xad";
        case SDFPrimitiveType::Prism:      return "\xef\x85\x9c";
        default:                           return "\xef\x80\x88";
    }
}

const char* SDFToolbox::GetPrimitiveName(SDFPrimitiveType type) const {
    switch (type) {
        case SDFPrimitiveType::Sphere:     return "Sphere";
        case SDFPrimitiveType::Box:        return "Box";
        case SDFPrimitiveType::Cylinder:   return "Cylinder";
        case SDFPrimitiveType::Capsule:    return "Capsule";
        case SDFPrimitiveType::Cone:       return "Cone";
        case SDFPrimitiveType::Torus:      return "Torus";
        case SDFPrimitiveType::Plane:      return "Plane";
        case SDFPrimitiveType::RoundedBox: return "Rounded Box";
        case SDFPrimitiveType::Ellipsoid:  return "Ellipsoid";
        case SDFPrimitiveType::Pyramid:    return "Pyramid";
        case SDFPrimitiveType::Prism:      return "Prism";
        case SDFPrimitiveType::Custom:     return "Custom";
        default:                           return "Unknown";
    }
}

const char* SDFToolbox::GetCSGOperationIcon(CSGOperation op) const {
    switch (op) {
        case CSGOperation::Union:              return "+";
        case CSGOperation::Subtraction:        return "-";
        case CSGOperation::Intersection:       return "&";
        case CSGOperation::SmoothUnion:        return "~+";
        case CSGOperation::SmoothSubtraction:  return "~-";
        case CSGOperation::SmoothIntersection: return "~&";
        default:                               return "?";
    }
}

const char* SDFToolbox::GetCSGOperationName(CSGOperation op) const {
    switch (op) {
        case CSGOperation::Union:              return "Union";
        case CSGOperation::Subtraction:        return "Subtraction";
        case CSGOperation::Intersection:       return "Intersection";
        case CSGOperation::SmoothUnion:        return "Smooth Union";
        case CSGOperation::SmoothSubtraction:  return "Smooth Subtraction";
        case CSGOperation::SmoothIntersection: return "Smooth Intersection";
        default:                               return "Unknown";
    }
}

// =============================================================================
// Utility Functions
// =============================================================================

SDFParameters GetDefaultParameters(SDFPrimitiveType type) {
    SDFParameters params;

    switch (type) {
        case SDFPrimitiveType::Sphere:
            params.radius = 0.5f;
            break;

        case SDFPrimitiveType::Box:
            params.dimensions = glm::vec3(1.0f);
            break;

        case SDFPrimitiveType::RoundedBox:
            params.dimensions = glm::vec3(1.0f);
            params.cornerRadius = 0.1f;
            break;

        case SDFPrimitiveType::Cylinder:
            params.height = 1.0f;
            params.topRadius = 0.5f;
            params.bottomRadius = 0.5f;
            break;

        case SDFPrimitiveType::Capsule:
            params.height = 1.0f;
            params.topRadius = 0.25f;
            params.bottomRadius = 0.25f;
            break;

        case SDFPrimitiveType::Cone:
            params.height = 1.0f;
            params.topRadius = 0.0f;
            params.bottomRadius = 0.5f;
            break;

        case SDFPrimitiveType::Torus:
            params.majorRadius = 0.5f;
            params.minorRadius = 0.15f;
            break;

        case SDFPrimitiveType::Plane:
            // Plane defined by transform
            break;

        case SDFPrimitiveType::Ellipsoid:
            params.radii = glm::vec3(0.5f, 0.35f, 0.25f);
            break;

        case SDFPrimitiveType::Pyramid:
            params.height = 1.0f;
            params.dimensions.x = 1.0f;  // Base size
            break;

        case SDFPrimitiveType::Prism:
            params.sides = 6;
            params.radius = 0.5f;
            params.height = 1.0f;
            break;

        default:
            break;
    }

    return params;
}

glm::vec3 EstimatePrimitiveSize(SDFPrimitiveType type, const SDFParameters& params) {
    switch (type) {
        case SDFPrimitiveType::Sphere:
            return glm::vec3(params.radius * 2.0f);

        case SDFPrimitiveType::Box:
        case SDFPrimitiveType::RoundedBox:
            return params.dimensions;

        case SDFPrimitiveType::Cylinder:
        case SDFPrimitiveType::Capsule:
            return glm::vec3(params.topRadius * 2.0f, params.height, params.topRadius * 2.0f);

        case SDFPrimitiveType::Cone:
            return glm::vec3(params.bottomRadius * 2.0f, params.height, params.bottomRadius * 2.0f);

        case SDFPrimitiveType::Torus:
            return glm::vec3((params.majorRadius + params.minorRadius) * 2.0f,
                             params.minorRadius * 2.0f,
                             (params.majorRadius + params.minorRadius) * 2.0f);

        case SDFPrimitiveType::Ellipsoid:
            return params.radii * 2.0f;

        case SDFPrimitiveType::Pyramid:
            return glm::vec3(params.dimensions.x, params.height, params.dimensions.x);

        case SDFPrimitiveType::Prism:
            return glm::vec3(params.radius * 2.0f, params.height, params.radius * 2.0f);

        default:
            return glm::vec3(1.0f);
    }
}

SDFParameters CalculateParametersFromDrag(
    SDFPrimitiveType type,
    const glm::vec3& startPos,
    const glm::vec3& endPos
) {
    SDFParameters params = GetDefaultParameters(type);

    glm::vec3 delta = endPos - startPos;
    float distance = glm::length(delta);

    switch (type) {
        case SDFPrimitiveType::Sphere:
            params.radius = std::max(0.1f, distance * 0.5f);
            break;

        case SDFPrimitiveType::Box:
        case SDFPrimitiveType::RoundedBox:
            params.dimensions = glm::abs(delta);
            params.dimensions = glm::max(params.dimensions, glm::vec3(0.1f));
            break;

        case SDFPrimitiveType::Cylinder:
        case SDFPrimitiveType::Capsule:
            params.height = std::max(0.1f, std::abs(delta.y));
            params.topRadius = std::max(0.1f, glm::length(glm::vec2(delta.x, delta.z)) * 0.5f);
            params.bottomRadius = params.topRadius;
            break;

        case SDFPrimitiveType::Cone:
            params.height = std::max(0.1f, std::abs(delta.y));
            params.bottomRadius = std::max(0.1f, glm::length(glm::vec2(delta.x, delta.z)) * 0.5f);
            break;

        case SDFPrimitiveType::Torus:
            params.majorRadius = std::max(0.1f, glm::length(glm::vec2(delta.x, delta.z)) * 0.5f);
            params.minorRadius = std::max(0.05f, std::abs(delta.y) * 0.5f);
            break;

        case SDFPrimitiveType::Ellipsoid:
            params.radii = glm::abs(delta) * 0.5f;
            params.radii = glm::max(params.radii, glm::vec3(0.1f));
            break;

        case SDFPrimitiveType::Pyramid:
            params.height = std::max(0.1f, std::abs(delta.y));
            params.dimensions.x = std::max(0.1f, glm::length(glm::vec2(delta.x, delta.z)));
            break;

        case SDFPrimitiveType::Prism:
            params.height = std::max(0.1f, std::abs(delta.y));
            params.radius = std::max(0.1f, glm::length(glm::vec2(delta.x, delta.z)) * 0.5f);
            break;

        default:
            break;
    }

    return params;
}

} // namespace Nova
