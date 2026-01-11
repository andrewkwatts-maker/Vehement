#include "TerrainEditor.hpp"
#include "../Editor.hpp"
#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace Vehement {

TerrainEditor::TerrainEditor() = default;
TerrainEditor::~TerrainEditor() = default;

void TerrainEditor::Initialize(Editor* editor, Nova::VoxelTerrain* terrain, const Config& config) {
    m_editor = editor;
    m_terrain = terrain;
    m_config = config;

    InitializeDefaults();

    m_initialized = true;
}

void TerrainEditor::InitializeDefaults() {
    // Default stamps
    m_stamps = {
        {"Sphere", "Basic sphere shape", Nova::SDFBrushShape::Sphere, glm::vec3(1.0f), glm::quat(1, 0, 0, 0), 0.3f, ""},
        {"Box", "Basic box shape", Nova::SDFBrushShape::Box, glm::vec3(1.0f), glm::quat(1, 0, 0, 0), 0.3f, ""},
        {"Cylinder", "Vertical cylinder", Nova::SDFBrushShape::Cylinder, glm::vec3(1.0f, 2.0f, 1.0f), glm::quat(1, 0, 0, 0), 0.3f, ""},
        {"Capsule", "Rounded cylinder", Nova::SDFBrushShape::Capsule, glm::vec3(1.0f, 2.0f, 1.0f), glm::quat(1, 0, 0, 0), 0.3f, ""},
        {"Cone", "Pointed cone", Nova::SDFBrushShape::Cone, glm::vec3(1.0f, 2.0f, 1.0f), glm::quat(1, 0, 0, 0), 0.3f, ""},
        {"Torus", "Ring/donut shape", Nova::SDFBrushShape::Torus, glm::vec3(2.0f, 0.5f, 2.0f), glm::quat(1, 0, 0, 0), 0.3f, ""},
    };

    // Default material presets
    m_materialPresets = {
        {"Dirt", Nova::VoxelMaterial::Dirt, glm::vec3(0.5f, 0.4f, 0.3f), ""},
        {"Stone", Nova::VoxelMaterial::Stone, glm::vec3(0.5f, 0.5f, 0.5f), ""},
        {"Grass", Nova::VoxelMaterial::Grass, glm::vec3(0.3f, 0.6f, 0.2f), ""},
        {"Sand", Nova::VoxelMaterial::Sand, glm::vec3(0.9f, 0.85f, 0.6f), ""},
        {"Snow", Nova::VoxelMaterial::Snow, glm::vec3(0.95f, 0.95f, 0.98f), ""},
        {"Clay", Nova::VoxelMaterial::Clay, glm::vec3(0.7f, 0.5f, 0.4f), ""},
        {"Gravel", Nova::VoxelMaterial::Gravel, glm::vec3(0.6f, 0.6f, 0.55f), ""},
        {"Mud", Nova::VoxelMaterial::Mud, glm::vec3(0.3f, 0.25f, 0.2f), ""},
        {"Ice", Nova::VoxelMaterial::Ice, glm::vec3(0.7f, 0.85f, 0.95f), ""},
        {"Ore", Nova::VoxelMaterial::Ore, glm::vec3(0.4f, 0.35f, 0.5f), ""},
        {"Crystal", Nova::VoxelMaterial::Crystal, glm::vec3(0.6f, 0.3f, 0.8f), ""},
    };
}

// =========================================================================
// Tool Selection
// =========================================================================

void TerrainEditor::SetTool(TerrainToolType tool) {
    m_brush.tool = tool;

    // Reset mode states
    m_isTunnelMode = false;
    m_isDrawingPath = false;
    m_pathPoints.clear();

    if (OnToolChanged) OnToolChanged(tool);
}

// =========================================================================
// Brush Operations
// =========================================================================

void TerrainEditor::ApplyBrush(const glm::vec3& position) {
    if (!m_terrain) return;

    switch (m_brush.tool) {
        case TerrainToolType::Sculpt:
            ApplySculptTool(position);
            break;
        case TerrainToolType::Smooth:
            ApplySmoothTool(position);
            break;
        case TerrainToolType::Flatten:
            ApplyFlattenTool(position);
            break;
        case TerrainToolType::Raise:
            ApplyRaiseTool(position);
            break;
        case TerrainToolType::Lower:
            ApplyLowerTool(position);
            break;
        case TerrainToolType::Paint:
            ApplyPaintTool(position);
            break;
        case TerrainToolType::Noise:
            ApplyNoiseTool(position);
            break;
        case TerrainToolType::Erode:
            ApplyErodeTool(position);
            break;
        case TerrainToolType::Cliff:
            ApplyCliffTool(position);
            break;
        case TerrainToolType::Cave:
            CreateCave(position, glm::vec3(m_brush.radius));
            break;
        case TerrainToolType::Stamp:
            ApplyStamp(position, false);
            break;
        default:
            break;
    }

    if (OnBrushApplied) OnBrushApplied();
    if (OnTerrainModified) OnTerrainModified();
}

void TerrainEditor::ApplyBrushStroke(const glm::vec3& start, const glm::vec3& end) {
    float distance = glm::length(end - start);
    glm::vec3 direction = glm::normalize(end - start);

    float spacing = m_brush.radius * m_strokeSpacing;
    int steps = std::max(1, static_cast<int>(distance / spacing));

    for (int i = 0; i <= steps; i++) {
        float t = static_cast<float>(i) / static_cast<float>(steps);
        glm::vec3 pos = start + direction * (t * distance);
        ApplyBrush(pos);
    }
}

void TerrainEditor::BeginStroke(const glm::vec3& position) {
    m_isStroking = true;
    m_lastStrokePosition = position;
    ApplyBrush(position);
}

void TerrainEditor::ContinueStroke(const glm::vec3& position) {
    if (!m_isStroking) return;

    float distance = glm::length(position - m_lastStrokePosition);
    float spacing = m_brush.radius * m_strokeSpacing;

    if (distance >= spacing) {
        ApplyBrushStroke(m_lastStrokePosition, position);
        m_lastStrokePosition = position;
    }
}

void TerrainEditor::EndStroke() {
    m_isStroking = false;
}

// =========================================================================
// Tunnel/Cave Tools
// =========================================================================

void TerrainEditor::BeginTunnel(const glm::vec3& start) {
    m_isTunnelMode = true;
    m_tunnelStart = start;
    m_tunnelEnd = start;
}

void TerrainEditor::PreviewTunnel(const glm::vec3& end) {
    if (!m_isTunnelMode) return;
    m_tunnelEnd = end;
    // Preview would be rendered separately
}

void TerrainEditor::CompleteTunnel(const glm::vec3& end) {
    if (!m_isTunnelMode || !m_terrain) return;

    m_terrain->DigTunnel(m_tunnelStart, end, m_brush.radius, m_brush.smoothness);
    m_isTunnelMode = false;

    if (OnTerrainModified) OnTerrainModified();
}

void TerrainEditor::CancelTunnel() {
    m_isTunnelMode = false;
}

void TerrainEditor::CreateCave(const glm::vec3& center, const glm::vec3& size) {
    if (!m_terrain) return;

    m_terrain->CreateCave(center, size, m_brush.noiseScale, 0);

    if (OnTerrainModified) OnTerrainModified();
}

// =========================================================================
// Stamp System
// =========================================================================

void TerrainEditor::SelectStamp(size_t index) {
    if (index < m_stamps.size()) {
        m_selectedStampIndex = index;
    }
}

void TerrainEditor::ApplyStamp(const glm::vec3& position, bool subtract) {
    if (!m_terrain || m_selectedStampIndex >= m_stamps.size()) return;

    const TerrainStamp& stamp = m_stamps[m_selectedStampIndex];

    Nova::SDFBrush brush;
    brush.shape = stamp.shape;
    brush.operation = subtract ? Nova::SDFOperation::SmoothSubtract : Nova::SDFOperation::SmoothUnion;
    brush.position = position;
    brush.size = stamp.size * m_brush.radius;
    brush.rotation = stamp.rotation;
    brush.smoothness = stamp.smoothness * m_brush.smoothness;
    brush.material = m_brush.material;
    brush.color = m_brush.color;
    brush.customSDF = stamp.customSDF;

    m_terrain->ApplyBrush(brush);

    if (OnTerrainModified) OnTerrainModified();
}

void TerrainEditor::AddStamp(const TerrainStamp& stamp) {
    m_stamps.push_back(stamp);
}

bool TerrainEditor::LoadStamps(const std::string& path) {
    // TODO: Load stamps from JSON
    return false;
}

// =========================================================================
// Material System
// =========================================================================

void TerrainEditor::SelectMaterial(size_t index) {
    if (index < m_materialPresets.size()) {
        m_selectedMaterialIndex = index;
        m_brush.material = m_materialPresets[index].material;
        m_brush.color = m_materialPresets[index].color;
    }
}

void TerrainEditor::AddMaterialPreset(const MaterialPreset& preset) {
    m_materialPresets.push_back(preset);
}

// =========================================================================
// Preview
// =========================================================================

void TerrainEditor::UpdatePreview(const glm::vec3& position) {
    m_previewPosition = position;
    m_hasValidPreview = true;

    // Generate preview mesh based on brush shape
    m_previewVertices.clear();
    m_previewIndices.clear();

    switch (m_brush.shape) {
        case TerrainBrushShape::Sphere:
            GenerateSpherePreview();
            break;
        case TerrainBrushShape::Cube:
            GenerateCubePreview();
            break;
        case TerrainBrushShape::Cylinder:
            GenerateCylinderPreview();
            break;
        default:
            GenerateSpherePreview();
            break;
    }
}

void TerrainEditor::GetPreviewMesh(std::vector<glm::vec3>& vertices, std::vector<uint32_t>& indices) const {
    vertices = m_previewVertices;
    indices = m_previewIndices;
}

void TerrainEditor::GenerateSpherePreview() {
    int segments = m_config.previewResolution;
    int rings = segments / 2;

    for (int i = 0; i <= rings; i++) {
        float phi = glm::pi<float>() * static_cast<float>(i) / static_cast<float>(rings);
        for (int j = 0; j <= segments; j++) {
            float theta = 2.0f * glm::pi<float>() * static_cast<float>(j) / static_cast<float>(segments);

            glm::vec3 pos;
            pos.x = m_brush.radius * std::sin(phi) * std::cos(theta);
            pos.y = m_brush.radius * std::cos(phi);
            pos.z = m_brush.radius * std::sin(phi) * std::sin(theta);

            m_previewVertices.push_back(m_previewPosition + pos);
        }
    }

    // Generate indices
    for (int i = 0; i < rings; i++) {
        for (int j = 0; j < segments; j++) {
            uint32_t first = i * (segments + 1) + j;
            uint32_t second = first + segments + 1;

            m_previewIndices.push_back(first);
            m_previewIndices.push_back(second);
            m_previewIndices.push_back(first + 1);

            m_previewIndices.push_back(second);
            m_previewIndices.push_back(second + 1);
            m_previewIndices.push_back(first + 1);
        }
    }
}

void TerrainEditor::GenerateCubePreview() {
    float r = m_brush.radius;
    glm::vec3 p = m_previewPosition;

    // 8 corners
    glm::vec3 corners[8] = {
        p + glm::vec3(-r, -r, -r),
        p + glm::vec3(r, -r, -r),
        p + glm::vec3(r, r, -r),
        p + glm::vec3(-r, r, -r),
        p + glm::vec3(-r, -r, r),
        p + glm::vec3(r, -r, r),
        p + glm::vec3(r, r, r),
        p + glm::vec3(-r, r, r),
    };

    for (int i = 0; i < 8; i++) {
        m_previewVertices.push_back(corners[i]);
    }

    // 6 faces
    uint32_t faces[36] = {
        0, 1, 2, 2, 3, 0,  // front
        1, 5, 6, 6, 2, 1,  // right
        5, 4, 7, 7, 6, 5,  // back
        4, 0, 3, 3, 7, 4,  // left
        3, 2, 6, 6, 7, 3,  // top
        4, 5, 1, 1, 0, 4   // bottom
    };

    for (int i = 0; i < 36; i++) {
        m_previewIndices.push_back(faces[i]);
    }
}

void TerrainEditor::GenerateCylinderPreview() {
    int segments = m_config.previewResolution;
    float r = m_brush.radius;
    float h = m_brush.radius;

    // Top and bottom circles
    for (int c = 0; c < 2; c++) {
        float y = c == 0 ? -h : h;
        for (int i = 0; i <= segments; i++) {
            float theta = 2.0f * glm::pi<float>() * static_cast<float>(i) / static_cast<float>(segments);
            m_previewVertices.push_back(m_previewPosition + glm::vec3(r * std::cos(theta), y, r * std::sin(theta)));
        }
    }

    // Side faces
    for (int i = 0; i < segments; i++) {
        uint32_t bl = i;
        uint32_t br = i + 1;
        uint32_t tl = i + segments + 1;
        uint32_t tr = i + segments + 2;

        m_previewIndices.push_back(bl);
        m_previewIndices.push_back(br);
        m_previewIndices.push_back(tl);

        m_previewIndices.push_back(br);
        m_previewIndices.push_back(tr);
        m_previewIndices.push_back(tl);
    }
}

// =========================================================================
// Undo/Redo
// =========================================================================

void TerrainEditor::Undo() {
    if (m_terrain) {
        m_terrain->Undo();
        if (OnTerrainModified) OnTerrainModified();
    }
}

void TerrainEditor::Redo() {
    if (m_terrain) {
        m_terrain->Redo();
        if (OnTerrainModified) OnTerrainModified();
    }
}

// =========================================================================
// Utility Tools
// =========================================================================

float TerrainEditor::SampleHeight(float x, float z) const {
    if (!m_terrain) return 0.0f;
    return m_terrain->GetHeightAt(x, z);
}

bool TerrainEditor::RaycastTerrain(const glm::vec3& origin, const glm::vec3& direction, glm::vec3& hitPoint, glm::vec3& hitNormal) const {
    if (!m_terrain) return false;
    return m_terrain->Raycast(origin, direction, 1000.0f, hitPoint, hitNormal);
}

void TerrainEditor::FillFlat(float height) {
    if (!m_terrain) return;
    m_terrain->GenerateFlatTerrain(height);
    m_terrain->RebuildAllMeshes();
    if (OnTerrainModified) OnTerrainModified();
}

void TerrainEditor::GenerateProcedural(int seed, float scale, int octaves) {
    if (!m_terrain) return;
    m_terrain->GenerateTerrain(seed, scale, octaves, 0.5f, 2.0f);
    m_terrain->RebuildAllMeshes();
    if (OnTerrainModified) OnTerrainModified();
}

void TerrainEditor::ImportHeightmap(const std::string& path, float heightScale) {
    // TODO: Implement heightmap import
}

void TerrainEditor::ExportHeightmap(const std::string& path, int resolution) const {
    // TODO: Implement heightmap export
}

// =========================================================================
// Tool Implementations
// =========================================================================

void TerrainEditor::ApplySculptTool(const glm::vec3& position) {
    Nova::SDFBrush brush;
    brush.shape = Nova::SDFBrushShape::Sphere;
    brush.operation = Nova::SDFOperation::SmoothUnion;
    brush.position = position;
    brush.size = glm::vec3(m_brush.radius);
    brush.smoothness = m_brush.smoothness;
    brush.material = m_brush.material;
    brush.color = m_brush.color;

    m_terrain->ApplyBrush(brush);
}

void TerrainEditor::ApplySmoothTool(const glm::vec3& position) {
    m_terrain->SmoothTerrain(position, m_brush.radius, m_brush.strength);
}

void TerrainEditor::ApplyFlattenTool(const glm::vec3& position) {
    m_terrain->FlattenTerrain(position, m_brush.radius, m_brush.targetHeight, m_brush.strength);
}

void TerrainEditor::ApplyRaiseTool(const glm::vec3& position) {
    Nova::SDFBrush brush;
    brush.shape = Nova::SDFBrushShape::Sphere;
    brush.operation = Nova::SDFOperation::SmoothUnion;
    brush.position = position;
    brush.size = glm::vec3(m_brush.radius);
    brush.smoothness = m_brush.smoothness;
    brush.material = m_brush.material;
    brush.color = m_brush.color;

    m_terrain->ApplyBrush(brush);
}

void TerrainEditor::ApplyLowerTool(const glm::vec3& position) {
    Nova::SDFBrush brush;
    brush.shape = Nova::SDFBrushShape::Sphere;
    brush.operation = Nova::SDFOperation::SmoothSubtract;
    brush.position = position;
    brush.size = glm::vec3(m_brush.radius);
    brush.smoothness = m_brush.smoothness;

    m_terrain->ApplyBrush(brush);
}

void TerrainEditor::ApplyPaintTool(const glm::vec3& position) {
    m_terrain->PaintMaterial(position, m_brush.radius, m_brush.material, m_brush.color);
}

void TerrainEditor::ApplyNoiseTool(const glm::vec3& position) {
    Nova::SDFBrush brush;
    brush.shape = Nova::SDFBrushShape::Custom;
    brush.operation = Nova::SDFOperation::SmoothUnion;
    brush.position = position;
    brush.size = glm::vec3(m_brush.radius);
    brush.smoothness = m_brush.smoothness;
    brush.material = m_brush.material;
    brush.color = m_brush.color;

    float noiseScale = m_brush.noiseScale;
    float strength = m_brush.strength;

    brush.customSDF = [noiseScale, strength](const glm::vec3& p) -> float {
        // Simple noise - in real implementation use proper noise function
        float noise = std::sin(p.x * noiseScale) * std::cos(p.z * noiseScale);
        return glm::length(p) - 1.0f + noise * strength;
    };

    m_terrain->ApplyBrush(brush);
}

void TerrainEditor::ApplyErodeTool(const glm::vec3& position) {
    // Simple erosion - smooth with downward bias
    m_terrain->SmoothTerrain(position, m_brush.radius, m_brush.erosionStrength);
}

void TerrainEditor::ApplyPathTool(const glm::vec3& start, const glm::vec3& end) {
    // Flatten along path
    glm::vec3 dir = glm::normalize(end - start);
    float length = glm::length(end - start);
    float spacing = m_brush.pathWidth * 0.5f;
    int steps = static_cast<int>(length / spacing);

    float avgHeight = (SampleHeight(start.x, start.z) + SampleHeight(end.x, end.z)) * 0.5f;

    for (int i = 0; i <= steps; i++) {
        float t = static_cast<float>(i) / static_cast<float>(std::max(1, steps));
        glm::vec3 pos = start + dir * (t * length);
        pos.y = avgHeight;
        m_terrain->FlattenTerrain(pos, m_brush.pathWidth, avgHeight, m_brush.strength);
    }
}

void TerrainEditor::ApplyCliffTool(const glm::vec3& position) {
    // Create vertical surfaces
    Nova::SDFBrush brush;
    brush.shape = Nova::SDFBrushShape::Box;
    brush.operation = Nova::SDFOperation::SmoothUnion;
    brush.position = position;
    brush.size = glm::vec3(m_brush.radius * 0.3f, m_brush.radius, m_brush.radius * 0.3f);
    brush.smoothness = m_brush.smoothness * 0.5f;
    brush.material = Nova::VoxelMaterial::Stone;
    brush.color = glm::vec3(0.5f, 0.5f, 0.5f);

    m_terrain->ApplyBrush(brush);
}

float TerrainEditor::EvaluateBrush(const glm::vec3& worldPos, const glm::vec3& brushCenter) const {
    float dist = glm::length(worldPos - brushCenter);

    if (dist > m_brush.radius) return 0.0f;

    // Apply falloff
    float t = dist / m_brush.radius;
    float falloff = 1.0f - std::pow(t, 1.0f / std::max(0.001f, m_brush.falloff));

    return falloff * m_brush.strength;
}

// =========================================================================
// UI
// =========================================================================

void TerrainEditor::RenderUI() {
    if (!m_initialized) return;

    ImGui::Begin("Terrain Editor");

    RenderToolPanel();
    ImGui::Separator();
    RenderBrushPanel();

    if (m_brush.tool == TerrainToolType::Paint) {
        ImGui::Separator();
        RenderMaterialPanel();
    }

    if (m_brush.tool == TerrainToolType::Stamp) {
        ImGui::Separator();
        RenderStampPanel();
    }

    ImGui::Separator();
    RenderTerrainInfoPanel();

    ImGui::End();
}

void TerrainEditor::RenderToolPanel() {
    ImGui::Text("Tools");

    struct ToolInfo {
        TerrainToolType type;
        const char* name;
        const char* icon;
    };

    ToolInfo tools[] = {
        {TerrainToolType::Sculpt, "Sculpt", "S"},
        {TerrainToolType::Raise, "Raise", "^"},
        {TerrainToolType::Lower, "Lower", "v"},
        {TerrainToolType::Smooth, "Smooth", "~"},
        {TerrainToolType::Flatten, "Flatten", "_"},
        {TerrainToolType::Paint, "Paint", "P"},
        {TerrainToolType::Tunnel, "Tunnel", "T"},
        {TerrainToolType::Cave, "Cave", "C"},
        {TerrainToolType::Stamp, "Stamp", "X"},
        {TerrainToolType::Noise, "Noise", "N"},
        {TerrainToolType::Erode, "Erode", "E"},
        {TerrainToolType::Path, "Path", "/"},
        {TerrainToolType::Cliff, "Cliff", "|"},
    };

    int cols = 4;
    for (int i = 0; i < 13; i++) {
        bool selected = m_brush.tool == tools[i].type;
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
        }

        if (ImGui::Button(tools[i].name, ImVec2(60, 30))) {
            SetTool(tools[i].type);
        }

        if (selected) {
            ImGui::PopStyleColor();
        }

        if ((i + 1) % cols != 0) {
            ImGui::SameLine();
        }
    }
}

void TerrainEditor::RenderBrushPanel() {
    ImGui::Text("Brush Settings");

    ImGui::SliderFloat("Radius", &m_brush.radius, m_config.minBrushRadius, m_config.maxBrushRadius);
    ImGui::SliderFloat("Strength", &m_brush.strength, m_config.minStrength, m_config.maxStrength);
    ImGui::SliderFloat("Falloff", &m_brush.falloff, 0.0f, 1.0f);
    ImGui::SliderFloat("Smoothness", &m_brush.smoothness, 0.0f, 1.0f);

    // Tool-specific settings
    if (m_brush.tool == TerrainToolType::Flatten) {
        ImGui::SliderFloat("Target Height", &m_brush.targetHeight, -100.0f, 100.0f);
        if (ImGui::Button("Sample Height")) {
            m_brush.targetHeight = SampleHeight(m_previewPosition.x, m_previewPosition.z);
        }
    }

    if (m_brush.tool == TerrainToolType::Noise || m_brush.tool == TerrainToolType::Cave) {
        ImGui::SliderFloat("Noise Scale", &m_brush.noiseScale, 0.01f, 10.0f);
        ImGui::SliderInt("Octaves", &m_brush.noiseOctaves, 1, 8);
    }

    if (m_brush.tool == TerrainToolType::Erode) {
        ImGui::SliderFloat("Erosion Strength", &m_brush.erosionStrength, 0.0f, 1.0f);
    }

    if (m_brush.tool == TerrainToolType::Path) {
        ImGui::SliderFloat("Path Width", &m_brush.pathWidth, 1.0f, 20.0f);
    }

    // Brush shape
    const char* shapes[] = {"Sphere", "Cube", "Cylinder", "Cone", "Custom"};
    int currentShape = static_cast<int>(m_brush.shape);
    if (ImGui::Combo("Shape", &currentShape, shapes, IM_ARRAYSIZE(shapes))) {
        m_brush.shape = static_cast<TerrainBrushShape>(currentShape);
    }
}

void TerrainEditor::RenderMaterialPanel() {
    ImGui::Text("Materials");

    int cols = 3;
    for (size_t i = 0; i < m_materialPresets.size(); i++) {
        bool selected = i == m_selectedMaterialIndex;
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
        }

        // Color preview
        ImVec4 col(m_materialPresets[i].color.r, m_materialPresets[i].color.g, m_materialPresets[i].color.b, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, col);

        if (ImGui::Button(m_materialPresets[i].name.c_str(), ImVec2(80, 30))) {
            SelectMaterial(i);
        }

        ImGui::PopStyleColor();

        if (selected) {
            ImGui::PopStyleColor();
        }

        if ((i + 1) % cols != 0 && i < m_materialPresets.size() - 1) {
            ImGui::SameLine();
        }
    }

    // Custom color picker
    ImGui::ColorEdit3("Custom Color", &m_brush.color.x);
}

void TerrainEditor::RenderStampPanel() {
    ImGui::Text("Stamps");

    for (size_t i = 0; i < m_stamps.size(); i++) {
        bool selected = i == m_selectedStampIndex;
        if (selected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.5f, 0.8f, 1.0f));
        }

        if (ImGui::Button(m_stamps[i].name.c_str(), ImVec2(100, 30))) {
            SelectStamp(i);
        }

        if (selected) {
            ImGui::PopStyleColor();
        }

        if ((i + 1) % 3 != 0 && i < m_stamps.size() - 1) {
            ImGui::SameLine();
        }
    }

    if (m_selectedStampIndex < m_stamps.size()) {
        ImGui::Text("Description: %s", m_stamps[m_selectedStampIndex].description.c_str());
    }
}

void TerrainEditor::RenderTerrainInfoPanel() {
    ImGui::Text("Terrain Info");

    if (m_hasValidPreview) {
        ImGui::Text("Position: %.1f, %.1f, %.1f", m_previewPosition.x, m_previewPosition.y, m_previewPosition.z);
        float height = SampleHeight(m_previewPosition.x, m_previewPosition.z);
        ImGui::Text("Height: %.2f", height);
    }

    ImGui::Separator();

    // Undo/Redo buttons
    ImGui::BeginDisabled(!CanUndo());
    if (ImGui::Button("Undo")) {
        Undo();
    }
    ImGui::EndDisabled();

    ImGui::SameLine();

    ImGui::BeginDisabled(!CanRedo());
    if (ImGui::Button("Redo")) {
        Redo();
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    // Quick generation
    if (ImGui::Button("Fill Flat")) {
        FillFlat(0.0f);
    }

    ImGui::SameLine();

    if (ImGui::Button("Generate")) {
        GenerateProcedural(42, 0.02f, 4);
    }
}

void TerrainEditor::ProcessInput() {
    // Input handling would be done in Update or by the editor
}

void TerrainEditor::Update(float deltaTime) {
    // Update logic if needed
}

} // namespace Vehement
