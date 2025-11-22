#include "MapEditor.hpp"
#include "InGameEditor.hpp"
#include "MapFile.hpp"
#include "../../world/World.hpp"

#include <imgui.h>
#include <algorithm>
#include <cmath>

namespace Vehement {

MapEditor::MapEditor() = default;
MapEditor::~MapEditor() = default;

bool MapEditor::Initialize(InGameEditor& parent) {
    if (m_initialized) {
        return true;
    }

    m_parent = &parent;

    // Default brush settings
    m_brush.shape = BrushShape::Circle;
    m_brush.size = 4.0f;
    m_brush.strength = 0.5f;
    m_brush.falloff = 0.3f;
    m_brush.smooth = true;

    // Create default layer
    MapLayer defaultLayer;
    defaultLayer.name = "Default";
    defaultLayer.visible = true;
    defaultLayer.locked = false;
    m_layers.push_back(defaultLayer);

    m_initialized = true;
    return true;
}

void MapEditor::Shutdown() {
    m_initialized = false;
    m_parent = nullptr;
    m_objects.clear();
    m_regions.clear();
    m_triggerZones.clear();
    m_spawnPoints.clear();
    m_resourceNodes.clear();
    m_heightmap.clear();
    ClearHistory();
}

bool MapEditor::CreateNew(int width, int height) {
    m_width = width;
    m_height = height;

    // Initialize heightmap
    m_heightmap.resize(static_cast<size_t>(width) * height, 0.0f);

    // Initialize texture blend map (4 layers per pixel)
    m_textureBlendMap.resize(static_cast<size_t>(width) * height * 4, 0);
    // Set first layer to full
    for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i) {
        m_textureBlendMap[i * 4] = 255;
    }

    // Clear objects
    m_objects.clear();
    m_regions.clear();
    m_triggerZones.clear();
    m_spawnPoints.clear();
    m_resourceNodes.clear();

    // Reset IDs
    m_nextObjectId = 1;
    m_nextRegionId = 1;
    m_nextZoneId = 1;
    m_nextSpawnId = 1;
    m_nextResourceId = 1;

    // Clear history
    ClearHistory();

    // Create default playable region
    MapRegion playableRegion;
    playableRegion.name = "Playable Area";
    playableRegion.type = "playable";
    playableRegion.min = glm::vec2(0.0f);
    playableRegion.max = glm::vec2(static_cast<float>(width), static_cast<float>(height));
    playableRegion.color = glm::vec4(0.0f, 1.0f, 0.0f, 0.1f);
    CreateRegion(playableRegion);

    // Add default texture layer
    if (m_textureLayers.empty()) {
        TerrainTextureLayer defaultTex;
        defaultTex.textureId = "grass";
        defaultTex.texturePath = "textures/terrain/grass.png";
        m_textureLayers.push_back(defaultTex);
    }

    return true;
}

bool MapEditor::LoadFromFile(const MapFile& file) {
    // Load map dimensions
    m_width = file.GetWidth();
    m_height = file.GetHeight();

    // Load heightmap
    m_heightmap = file.GetHeightmap();

    // Load texture data
    m_textureLayers = file.GetTextureLayers();
    m_textureBlendMap = file.GetTextureBlendMap();

    // Load water settings
    m_waterLevel = file.GetWaterLevel();
    m_waterEnabled = file.IsWaterEnabled();

    // Load objects
    m_objects = file.GetObjects();
    m_regions = file.GetRegions();
    m_triggerZones = file.GetTriggerZones();
    m_spawnPoints = file.GetSpawnPoints();
    m_resourceNodes = file.GetResourceNodes();
    m_layers = file.GetLayers();

    // Update ID counters
    m_nextObjectId = 1;
    for (const auto& obj : m_objects) {
        m_nextObjectId = std::max(m_nextObjectId, obj.id + 1);
    }
    m_nextRegionId = 1;
    for (const auto& reg : m_regions) {
        m_nextRegionId = std::max(m_nextRegionId, reg.id + 1);
    }
    m_nextZoneId = 1;
    for (const auto& zone : m_triggerZones) {
        m_nextZoneId = std::max(m_nextZoneId, zone.id + 1);
    }
    m_nextSpawnId = 1;
    for (const auto& spawn : m_spawnPoints) {
        m_nextSpawnId = std::max(m_nextSpawnId, spawn.id + 1);
    }
    m_nextResourceId = 1;
    for (const auto& res : m_resourceNodes) {
        m_nextResourceId = std::max(m_nextResourceId, res.id + 1);
    }

    ClearHistory();
    return true;
}

void MapEditor::SaveToFile(MapFile& file) const {
    file.SetDimensions(m_width, m_height);
    file.SetHeightmap(m_heightmap);
    file.SetTextureLayers(m_textureLayers);
    file.SetTextureBlendMap(m_textureBlendMap);
    file.SetWaterLevel(m_waterLevel);
    file.SetWaterEnabled(m_waterEnabled);
    file.SetObjects(m_objects);
    file.SetRegions(m_regions);
    file.SetTriggerZones(m_triggerZones);
    file.SetSpawnPoints(m_spawnPoints);
    file.SetResourceNodes(m_resourceNodes);
    file.SetLayers(m_layers);
}

void MapEditor::ApplyToWorld(World& world) {
    // Apply heightmap to world terrain
    world.SetTerrainHeightmap(m_heightmap, m_width, m_height);

    // Apply water
    if (m_waterEnabled) {
        world.SetWaterLevel(m_waterLevel);
    }

    // Spawn objects
    for (const auto& obj : m_objects) {
        if (obj.objectType == "unit") {
            world.SpawnUnit(obj.objectId, obj.position, obj.player);
        } else if (obj.objectType == "building") {
            world.SpawnBuilding(obj.objectId, obj.position, obj.player);
        } else if (obj.objectType == "doodad") {
            world.SpawnDoodad(obj.objectId, obj.position, obj.rotation, obj.scale);
        }
    }

    // Spawn resources
    for (const auto& res : m_resourceNodes) {
        world.SpawnResource(res.resourceType, res.position, res.amount);
    }
}

void MapEditor::RestoreFromWorld(World& world) {
    // Capture current world state for comparison
    // This allows detecting changes made during testing
}

void MapEditor::Update(float deltaTime) {
    if (!m_initialized) {
        return;
    }

    // Update any animations or real-time previews
    if (m_showPathfinding) {
        UpdatePathfindingPreview();
    }
}

void MapEditor::Render() {
    if (!m_initialized) {
        return;
    }

    // Render terrain modifications overlay
    if (m_showGrid) {
        RenderGrid();
    }

    // Render map objects
    RenderObjects();
    RenderRegions();
    RenderTriggerZones();
    RenderSpawnPoints();
    RenderResourceNodes();

    // Render selection
    if (m_isSelecting) {
        RenderSelectionRect();
    }

    // Render brush preview
    if (m_currentTool == MapTool::Terrain || m_currentTool == MapTool::Texture) {
        RenderBrushPreview();
    }

    // Render pathfinding overlay
    if (m_showPathfinding) {
        RenderPathfindingOverlay();
    }
}

void MapEditor::RenderMinimap() {
    ImVec2 size = ImGui::GetContentRegionAvail();
    float scale = std::min(size.x / m_width, size.y / m_height);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();

    // Draw terrain
    for (int y = 0; y < m_height; y += 2) {
        for (int x = 0; x < m_width; x += 2) {
            float height = m_heightmap[y * m_width + x];
            uint8_t intensity = static_cast<uint8_t>(128 + height * 64);
            ImU32 color = IM_COL32(intensity, intensity, intensity, 255);

            ImVec2 p1(pos.x + x * scale, pos.y + y * scale);
            ImVec2 p2(pos.x + (x + 2) * scale, pos.y + (y + 2) * scale);
            draw->AddRectFilled(p1, p2, color);
        }
    }

    // Draw water
    if (m_waterEnabled) {
        for (int y = 0; y < m_height; y += 2) {
            for (int x = 0; x < m_width; x += 2) {
                float height = m_heightmap[y * m_width + x];
                if (height < m_waterLevel) {
                    ImVec2 p1(pos.x + x * scale, pos.y + y * scale);
                    ImVec2 p2(pos.x + (x + 2) * scale, pos.y + (y + 2) * scale);
                    draw->AddRectFilled(p1, p2, IM_COL32(0, 100, 200, 150));
                }
            }
        }
    }

    // Draw objects
    for (const auto& obj : m_objects) {
        ImVec2 p(pos.x + obj.position.x * scale, pos.y + obj.position.z * scale);
        ImU32 color = IM_COL32(255, 255, 0, 255);
        if (obj.objectType == "unit") {
            color = IM_COL32(0, 255, 0, 255);
        } else if (obj.objectType == "building") {
            color = IM_COL32(255, 128, 0, 255);
        }
        draw->AddCircleFilled(p, 2.0f, color);
    }

    // Draw spawn points
    for (const auto& spawn : m_spawnPoints) {
        ImVec2 p(pos.x + spawn.position.x * scale, pos.y + spawn.position.z * scale);
        draw->AddCircle(p, 4.0f, IM_COL32(255, 0, 255, 255), 6, 2.0f);
    }
}

void MapEditor::ProcessInput() {
    if (!m_initialized) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();

    // Don't process input if ImGui wants it
    if (io.WantCaptureMouse) {
        return;
    }

    // Get mouse position in world space
    glm::vec2 mousePos(io.MousePos.x, io.MousePos.y);

    // Mouse button pressed
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        switch (m_currentTool) {
            case MapTool::Select:
                m_isSelecting = true;
                m_selectionStart = mousePos;
                m_selectionEnd = mousePos;
                break;

            case MapTool::Terrain:
            case MapTool::Texture:
            case MapTool::Water:
                m_isPainting = true;
                m_lastPaintPos = mousePos;
                break;

            case MapTool::PlaceObject:
                // Place object at click position
                {
                    glm::vec3 worldPos(mousePos.x, 0.0f, mousePos.y); // Convert to world
                    HandleObjectPlacement(worldPos);
                }
                break;

            default:
                break;
        }
    }

    // Mouse button held
    if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if (m_isSelecting) {
            m_selectionEnd = mousePos;
        }

        if (m_isPainting) {
            switch (m_currentTool) {
                case MapTool::Terrain:
                    HandleTerrainPaint(mousePos);
                    break;
                case MapTool::Texture:
                    HandleTexturePaint(mousePos);
                    break;
                case MapTool::Water:
                    HandleWaterPaint(mousePos);
                    break;
                default:
                    break;
            }
            m_lastPaintPos = mousePos;
        }
    }

    // Mouse button released
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        if (m_isSelecting) {
            glm::vec2 minP(std::min(m_selectionStart.x, m_selectionEnd.x),
                          std::min(m_selectionStart.y, m_selectionEnd.y));
            glm::vec2 maxP(std::max(m_selectionStart.x, m_selectionEnd.x),
                          std::max(m_selectionStart.y, m_selectionEnd.y));

            if (glm::length(m_selectionEnd - m_selectionStart) < 3.0f) {
                // Click selection
                HandleSelection(mousePos);
            } else {
                // Rectangle selection
                SelectInRect(minP, maxP);
            }
            m_isSelecting = false;
        }

        m_isPainting = false;
    }

    // Keyboard shortcuts
    if (!io.WantCaptureKeyboard) {
        // Tool shortcuts
        if (ImGui::IsKeyPressed(ImGuiKey_Q)) SetTool(MapTool::Select);
        if (ImGui::IsKeyPressed(ImGuiKey_W)) SetTool(MapTool::Terrain);
        if (ImGui::IsKeyPressed(ImGuiKey_E)) SetTool(MapTool::Texture);
        if (ImGui::IsKeyPressed(ImGuiKey_R)) SetTool(MapTool::PlaceObject);
        if (ImGui::IsKeyPressed(ImGuiKey_T)) SetTool(MapTool::Region);
        if (ImGui::IsKeyPressed(ImGuiKey_Delete)) DeleteSelected();

        // Brush size
        if (ImGui::IsKeyPressed(ImGuiKey_LeftBracket)) {
            SetBrushSize(m_brush.size - 1.0f);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_RightBracket)) {
            SetBrushSize(m_brush.size + 1.0f);
        }
    }
}

void MapEditor::SetTool(MapTool tool) {
    m_currentTool = tool;
    DeselectAll();
}

void MapEditor::SetHeightMode(HeightMode mode) {
    m_heightMode = mode;
}

void MapEditor::SetBrush(const TerrainBrush& brush) {
    m_brush = brush;
}

void MapEditor::SetBrushSize(float size) {
    m_brush.size = std::clamp(size, 1.0f, 32.0f);
}

void MapEditor::SetBrushStrength(float strength) {
    m_brush.strength = std::clamp(strength, 0.0f, 1.0f);
}

void MapEditor::SetBrushShape(BrushShape shape) {
    m_brush.shape = shape;
}

void MapEditor::AddTextureLayer(const TerrainTextureLayer& layer) {
    if (m_textureLayers.size() < 4) {
        m_textureLayers.push_back(layer);
    }
}

void MapEditor::RemoveTextureLayer(int index) {
    if (index > 0 && index < static_cast<int>(m_textureLayers.size())) {
        m_textureLayers.erase(m_textureLayers.begin() + index);
        if (m_currentTextureLayer >= static_cast<int>(m_textureLayers.size())) {
            m_currentTextureLayer = static_cast<int>(m_textureLayers.size()) - 1;
        }
    }
}

void MapEditor::SetCurrentTextureLayer(int index) {
    if (index >= 0 && index < static_cast<int>(m_textureLayers.size())) {
        m_currentTextureLayer = index;
    }
}

void MapEditor::SetWaterLevel(float level) {
    m_waterLevel = level;
}

void MapEditor::SetWaterEnabled(bool enabled) {
    m_waterEnabled = enabled;
}

uint32_t MapEditor::PlaceObject(const PlacedObject& obj) {
    PlacedObject newObj = obj;
    newObj.id = GenerateObjectId();

    if (m_snapToGrid) {
        newObj.position = SnapPosition(newObj.position);
    }

    m_objects.push_back(newObj);

    if (OnObjectPlaced) {
        OnObjectPlaced(newObj.id);
    }

    if (OnMapModified) {
        OnMapModified();
    }

    return newObj.id;
}

void MapEditor::RemoveObject(uint32_t id) {
    auto it = std::find_if(m_objects.begin(), m_objects.end(),
                          [id](const PlacedObject& o) { return o.id == id; });
    if (it != m_objects.end()) {
        m_objects.erase(it);

        if (OnObjectRemoved) {
            OnObjectRemoved(id);
        }

        if (OnMapModified) {
            OnMapModified();
        }
    }
}

void MapEditor::UpdateObject(uint32_t id, const PlacedObject& obj) {
    auto it = std::find_if(m_objects.begin(), m_objects.end(),
                          [id](const PlacedObject& o) { return o.id == id; });
    if (it != m_objects.end()) {
        uint32_t oldId = it->id;
        *it = obj;
        it->id = oldId;

        if (OnMapModified) {
            OnMapModified();
        }
    }
}

PlacedObject* MapEditor::GetObject(uint32_t id) {
    auto it = std::find_if(m_objects.begin(), m_objects.end(),
                          [id](const PlacedObject& o) { return o.id == id; });
    return it != m_objects.end() ? &(*it) : nullptr;
}

void MapEditor::SetCurrentObjectType(const std::string& type, const std::string& id) {
    m_currentObjectType = type;
    m_currentObjectId = id;
}

void MapEditor::SelectObject(uint32_t id) {
    auto* obj = GetObject(id);
    if (obj && !obj->isSelected) {
        obj->isSelected = true;
        m_selectedObjects.push_back(id);

        if (OnObjectSelected) {
            OnObjectSelected(id);
        }
    }
}

void MapEditor::DeselectObject(uint32_t id) {
    auto* obj = GetObject(id);
    if (obj) {
        obj->isSelected = false;
    }
    m_selectedObjects.erase(
        std::remove(m_selectedObjects.begin(), m_selectedObjects.end(), id),
        m_selectedObjects.end());
}

void MapEditor::SelectAll() {
    for (auto& obj : m_objects) {
        if (!obj.isSelected) {
            obj.isSelected = true;
            m_selectedObjects.push_back(obj.id);
        }
    }
}

void MapEditor::DeselectAll() {
    for (auto& obj : m_objects) {
        obj.isSelected = false;
    }
    m_selectedObjects.clear();
}

void MapEditor::DeleteSelected() {
    for (uint32_t id : m_selectedObjects) {
        auto it = std::find_if(m_objects.begin(), m_objects.end(),
                              [id](const PlacedObject& o) { return o.id == id; });
        if (it != m_objects.end()) {
            m_objects.erase(it);
            if (OnObjectRemoved) {
                OnObjectRemoved(id);
            }
        }
    }
    m_selectedObjects.clear();

    if (OnMapModified) {
        OnMapModified();
    }
}

void MapEditor::SetSelectionRect(const glm::vec2& start, const glm::vec2& end) {
    m_selectionStart = start;
    m_selectionEnd = end;
}

void MapEditor::SelectInRect(const glm::vec2& min, const glm::vec2& max) {
    if (!ImGui::GetIO().KeyCtrl) {
        DeselectAll();
    }

    for (auto& obj : m_objects) {
        glm::vec2 pos2D(obj.position.x, obj.position.z);
        if (pos2D.x >= min.x && pos2D.x <= max.x &&
            pos2D.y >= min.y && pos2D.y <= max.y) {
            SelectObject(obj.id);
        }
    }
}

uint32_t MapEditor::CreateRegion(const MapRegion& region) {
    MapRegion newRegion = region;
    newRegion.id = GenerateRegionId();
    m_regions.push_back(newRegion);
    return newRegion.id;
}

void MapEditor::UpdateRegion(uint32_t id, const MapRegion& region) {
    auto it = std::find_if(m_regions.begin(), m_regions.end(),
                          [id](const MapRegion& r) { return r.id == id; });
    if (it != m_regions.end()) {
        uint32_t oldId = it->id;
        *it = region;
        it->id = oldId;
    }
}

void MapEditor::RemoveRegion(uint32_t id) {
    m_regions.erase(
        std::remove_if(m_regions.begin(), m_regions.end(),
                      [id](const MapRegion& r) { return r.id == id; }),
        m_regions.end());
}

MapRegion* MapEditor::GetRegion(uint32_t id) {
    auto it = std::find_if(m_regions.begin(), m_regions.end(),
                          [id](const MapRegion& r) { return r.id == id; });
    return it != m_regions.end() ? &(*it) : nullptr;
}

uint32_t MapEditor::CreateTriggerZone(const TriggerZone& zone) {
    TriggerZone newZone = zone;
    newZone.id = GenerateZoneId();
    m_triggerZones.push_back(newZone);
    return newZone.id;
}

void MapEditor::UpdateTriggerZone(uint32_t id, const TriggerZone& zone) {
    auto it = std::find_if(m_triggerZones.begin(), m_triggerZones.end(),
                          [id](const TriggerZone& z) { return z.id == id; });
    if (it != m_triggerZones.end()) {
        uint32_t oldId = it->id;
        *it = zone;
        it->id = oldId;
    }
}

void MapEditor::RemoveTriggerZone(uint32_t id) {
    m_triggerZones.erase(
        std::remove_if(m_triggerZones.begin(), m_triggerZones.end(),
                      [id](const TriggerZone& z) { return z.id == id; }),
        m_triggerZones.end());
}

TriggerZone* MapEditor::GetTriggerZone(uint32_t id) {
    auto it = std::find_if(m_triggerZones.begin(), m_triggerZones.end(),
                          [id](const TriggerZone& z) { return z.id == id; });
    return it != m_triggerZones.end() ? &(*it) : nullptr;
}

uint32_t MapEditor::CreateSpawnPoint(const SpawnPoint& spawn) {
    SpawnPoint newSpawn = spawn;
    newSpawn.id = GenerateSpawnId();
    m_spawnPoints.push_back(newSpawn);
    return newSpawn.id;
}

void MapEditor::UpdateSpawnPoint(uint32_t id, const SpawnPoint& spawn) {
    auto it = std::find_if(m_spawnPoints.begin(), m_spawnPoints.end(),
                          [id](const SpawnPoint& s) { return s.id == id; });
    if (it != m_spawnPoints.end()) {
        uint32_t oldId = it->id;
        *it = spawn;
        it->id = oldId;
    }
}

void MapEditor::RemoveSpawnPoint(uint32_t id) {
    m_spawnPoints.erase(
        std::remove_if(m_spawnPoints.begin(), m_spawnPoints.end(),
                      [id](const SpawnPoint& s) { return s.id == id; }),
        m_spawnPoints.end());
}

SpawnPoint* MapEditor::GetSpawnPoint(uint32_t id) {
    auto it = std::find_if(m_spawnPoints.begin(), m_spawnPoints.end(),
                          [id](const SpawnPoint& s) { return s.id == id; });
    return it != m_spawnPoints.end() ? &(*it) : nullptr;
}

uint32_t MapEditor::CreateResourceNode(const ResourceNode& node) {
    ResourceNode newNode = node;
    newNode.id = GenerateResourceId();
    m_resourceNodes.push_back(newNode);
    return newNode.id;
}

void MapEditor::UpdateResourceNode(uint32_t id, const ResourceNode& node) {
    auto it = std::find_if(m_resourceNodes.begin(), m_resourceNodes.end(),
                          [id](const ResourceNode& n) { return n.id == id; });
    if (it != m_resourceNodes.end()) {
        uint32_t oldId = it->id;
        *it = node;
        it->id = oldId;
    }
}

void MapEditor::RemoveResourceNode(uint32_t id) {
    m_resourceNodes.erase(
        std::remove_if(m_resourceNodes.begin(), m_resourceNodes.end(),
                      [id](const ResourceNode& n) { return n.id == id; }),
        m_resourceNodes.end());
}

ResourceNode* MapEditor::GetResourceNode(uint32_t id) {
    auto it = std::find_if(m_resourceNodes.begin(), m_resourceNodes.end(),
                          [id](const ResourceNode& n) { return n.id == id; });
    return it != m_resourceNodes.end() ? &(*it) : nullptr;
}

int MapEditor::CreateLayer(const std::string& name) {
    MapLayer layer;
    layer.name = name;
    layer.visible = true;
    layer.locked = false;
    m_layers.push_back(layer);
    return static_cast<int>(m_layers.size()) - 1;
}

void MapEditor::RemoveLayer(int index) {
    if (index > 0 && index < static_cast<int>(m_layers.size())) {
        m_layers.erase(m_layers.begin() + index);
        if (m_currentLayer >= static_cast<int>(m_layers.size())) {
            m_currentLayer = static_cast<int>(m_layers.size()) - 1;
        }
    }
}

void MapEditor::SetLayerVisible(int index, bool visible) {
    if (index >= 0 && index < static_cast<int>(m_layers.size())) {
        m_layers[index].visible = visible;
    }
}

void MapEditor::SetLayerLocked(int index, bool locked) {
    if (index >= 0 && index < static_cast<int>(m_layers.size())) {
        m_layers[index].locked = locked;
    }
}

void MapEditor::SetCurrentLayer(int index) {
    if (index >= 0 && index < static_cast<int>(m_layers.size())) {
        m_currentLayer = index;
    }
}

void MapEditor::SetShowPathfinding(bool show) {
    m_showPathfinding = show;
    if (show) {
        UpdatePathfindingPreview();
    }
}

void MapEditor::UpdatePathfindingPreview() {
    m_pathfindingPreview.resize(static_cast<size_t>(m_width) * m_height, 0);

    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            size_t idx = y * m_width + x;
            float height = m_heightmap[idx];

            // Check if tile is walkable
            bool walkable = true;

            // Check water
            if (m_waterEnabled && height < m_waterLevel) {
                walkable = false;
            }

            // Check steep slopes
            if (x > 0 && y > 0 && x < m_width - 1 && y < m_height - 1) {
                float dx = m_heightmap[idx + 1] - m_heightmap[idx - 1];
                float dy = m_heightmap[idx + m_width] - m_heightmap[idx - m_width];
                float slope = std::sqrt(dx * dx + dy * dy) * 0.5f;
                if (slope > 1.0f) {
                    walkable = false;
                }
            }

            // Check for blocking objects
            for (const auto& obj : m_objects) {
                if (obj.objectType == "building") {
                    glm::vec2 objPos(obj.position.x, obj.position.z);
                    glm::vec2 tilePos(static_cast<float>(x), static_cast<float>(y));
                    if (glm::length(objPos - tilePos) < 2.0f) {
                        walkable = false;
                        break;
                    }
                }
            }

            m_pathfindingPreview[idx] = walkable ? 255 : 0;
        }
    }
}

void MapEditor::SetShowGrid(bool show) {
    m_showGrid = show;
}

void MapEditor::SetGridSize(float size) {
    m_gridSize = std::max(0.25f, size);
}

void MapEditor::SetSnapToGrid(bool snap) {
    m_snapToGrid = snap;
}

glm::vec3 MapEditor::SnapPosition(const glm::vec3& pos) const {
    if (!m_snapToGrid) {
        return pos;
    }

    return glm::vec3(
        std::round(pos.x / m_gridSize) * m_gridSize,
        pos.y,
        std::round(pos.z / m_gridSize) * m_gridSize
    );
}

void MapEditor::ExecuteCommand(std::unique_ptr<MapEditorCommand> command) {
    command->Execute();
    m_undoStack.push_back(std::move(command));
    m_redoStack.clear();

    if (m_undoStack.size() > MAX_UNDO_HISTORY) {
        m_undoStack.pop_front();
    }
}

void MapEditor::Undo() {
    if (m_undoStack.empty()) {
        return;
    }

    auto command = std::move(m_undoStack.back());
    m_undoStack.pop_back();
    command->Undo();
    m_redoStack.push_back(std::move(command));
}

void MapEditor::Redo() {
    if (m_redoStack.empty()) {
        return;
    }

    auto command = std::move(m_redoStack.back());
    m_redoStack.pop_back();
    command->Execute();
    m_undoStack.push_back(std::move(command));
}

void MapEditor::ClearHistory() {
    m_undoStack.clear();
    m_redoStack.clear();
}

void MapEditor::HandleTerrainPaint(const glm::vec2& worldPos) {
    glm::vec2 terrainPos = worldPos; // Convert screen to terrain coords

    ApplyBrush(terrainPos, [this](int x, int y, float weight) {
        switch (m_heightMode) {
            case HeightMode::Raise:
                RaiseHeight(x, y, weight);
                break;
            case HeightMode::Lower:
                LowerHeight(x, y, weight);
                break;
            case HeightMode::Smooth:
                SmoothHeight(x, y, weight);
                break;
            case HeightMode::Plateau:
                PlateauHeight(x, y, weight, m_plateauTarget);
                break;
            case HeightMode::Flatten:
                FlattenHeight(x, y, weight, m_plateauTarget);
                break;
            default:
                break;
        }
    });

    if (OnMapModified) {
        OnMapModified();
    }
}

void MapEditor::HandleTexturePaint(const glm::vec2& worldPos) {
    glm::vec2 terrainPos = worldPos;

    ApplyBrush(terrainPos, [this](int x, int y, float weight) {
        size_t idx = (y * m_width + x) * 4;
        if (idx + 3 < m_textureBlendMap.size()) {
            // Increase current layer, decrease others
            int increase = static_cast<int>(weight * 255.0f * m_brush.strength);

            for (int i = 0; i < 4; ++i) {
                if (i == m_currentTextureLayer) {
                    m_textureBlendMap[idx + i] = static_cast<uint8_t>(
                        std::min(255, m_textureBlendMap[idx + i] + increase));
                } else {
                    m_textureBlendMap[idx + i] = static_cast<uint8_t>(
                        std::max(0, m_textureBlendMap[idx + i] - increase / 3));
                }
            }

            // Normalize
            int total = m_textureBlendMap[idx] + m_textureBlendMap[idx + 1] +
                       m_textureBlendMap[idx + 2] + m_textureBlendMap[idx + 3];
            if (total > 0) {
                for (int i = 0; i < 4; ++i) {
                    m_textureBlendMap[idx + i] = static_cast<uint8_t>(
                        m_textureBlendMap[idx + i] * 255 / total);
                }
            }
        }
    });
}

void MapEditor::HandleWaterPaint(const glm::vec2& worldPos) {
    // Set water level at click position
    int x = static_cast<int>(worldPos.x);
    int y = static_cast<int>(worldPos.y);

    if (x >= 0 && x < m_width && y >= 0 && y < m_height) {
        m_waterLevel = m_heightmap[y * m_width + x];
        m_waterEnabled = true;
    }
}

void MapEditor::HandleObjectPlacement(const glm::vec3& worldPos) {
    if (m_currentObjectType.empty() || m_currentObjectId.empty()) {
        return;
    }

    PlacedObject obj;
    obj.objectType = m_currentObjectType;
    obj.objectId = m_currentObjectId;
    obj.position = worldPos;
    obj.player = m_currentPlayer;

    PlaceObject(obj);
}

void MapEditor::HandleSelection(const glm::vec2& screenPos) {
    // Find object at click position
    // This is a simplified version - real implementation would use raycasting

    if (!ImGui::GetIO().KeyCtrl) {
        DeselectAll();
    }

    for (auto& obj : m_objects) {
        glm::vec2 objScreenPos(obj.position.x, obj.position.z); // Simplified
        if (glm::length(screenPos - objScreenPos) < 10.0f) {
            SelectObject(obj.id);
            break;
        }
    }
}

void MapEditor::ApplyBrush(const glm::vec2& center,
                           std::function<void(int, int, float)> operation) {
    int radius = static_cast<int>(m_brush.size);
    int cx = static_cast<int>(center.x);
    int cy = static_cast<int>(center.y);

    for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
            int x = cx + dx;
            int y = cy + dy;

            if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
                continue;
            }

            float weight = GetBrushWeight(center, x, y);
            if (weight > 0.001f) {
                operation(x, y, weight);
            }
        }
    }
}

float MapEditor::GetBrushWeight(const glm::vec2& brushCenter, int x, int y) const {
    float dist = 0.0f;

    switch (m_brush.shape) {
        case BrushShape::Circle: {
            float dx = static_cast<float>(x) - brushCenter.x;
            float dy = static_cast<float>(y) - brushCenter.y;
            dist = std::sqrt(dx * dx + dy * dy);
            break;
        }
        case BrushShape::Square: {
            float dx = std::abs(static_cast<float>(x) - brushCenter.x);
            float dy = std::abs(static_cast<float>(y) - brushCenter.y);
            dist = std::max(dx, dy);
            break;
        }
        case BrushShape::Diamond: {
            float dx = std::abs(static_cast<float>(x) - brushCenter.x);
            float dy = std::abs(static_cast<float>(y) - brushCenter.y);
            dist = dx + dy;
            break;
        }
    }

    if (dist > m_brush.size) {
        return 0.0f;
    }

    // Calculate falloff
    float normalizedDist = dist / m_brush.size;
    float falloffStart = 1.0f - m_brush.falloff;
    if (normalizedDist < falloffStart) {
        return 1.0f;
    }

    float falloffRange = m_brush.falloff;
    float falloffDist = normalizedDist - falloffStart;
    float weight = 1.0f - (falloffDist / falloffRange);

    return m_brush.smooth ? weight * weight * (3.0f - 2.0f * weight) : weight;
}

void MapEditor::RaiseHeight(int x, int y, float weight) {
    size_t idx = y * m_width + x;
    m_heightmap[idx] += weight * m_brush.strength * 0.1f;
}

void MapEditor::LowerHeight(int x, int y, float weight) {
    size_t idx = y * m_width + x;
    m_heightmap[idx] -= weight * m_brush.strength * 0.1f;
}

void MapEditor::SmoothHeight(int x, int y, float weight) {
    if (x <= 0 || x >= m_width - 1 || y <= 0 || y >= m_height - 1) {
        return;
    }

    size_t idx = y * m_width + x;
    float avg = (m_heightmap[idx - 1] + m_heightmap[idx + 1] +
                 m_heightmap[idx - m_width] + m_heightmap[idx + m_width]) * 0.25f;

    m_heightmap[idx] += (avg - m_heightmap[idx]) * weight * m_brush.strength;
}

void MapEditor::PlateauHeight(int x, int y, float weight, float targetHeight) {
    size_t idx = y * m_width + x;
    float diff = targetHeight - m_heightmap[idx];
    m_heightmap[idx] += diff * weight * m_brush.strength;
}

void MapEditor::FlattenHeight(int x, int y, float weight, float targetHeight) {
    size_t idx = y * m_width + x;
    m_heightmap[idx] = targetHeight;
}

uint32_t MapEditor::GenerateObjectId() {
    return m_nextObjectId++;
}

uint32_t MapEditor::GenerateRegionId() {
    return m_nextRegionId++;
}

uint32_t MapEditor::GenerateZoneId() {
    return m_nextZoneId++;
}

uint32_t MapEditor::GenerateSpawnId() {
    return m_nextSpawnId++;
}

uint32_t MapEditor::GenerateResourceId() {
    return m_nextResourceId++;
}

void MapEditor::RenderGrid() {
    // Grid rendering handled by viewport
}

void MapEditor::RenderObjects() {
    // Object rendering handled by viewport
}

void MapEditor::RenderRegions() {
    // Region rendering handled by viewport
}

void MapEditor::RenderTriggerZones() {
    // Zone rendering handled by viewport
}

void MapEditor::RenderSpawnPoints() {
    // Spawn point rendering handled by viewport
}

void MapEditor::RenderResourceNodes() {
    // Resource node rendering handled by viewport
}

void MapEditor::RenderSelectionRect() {
    if (!m_isSelecting) {
        return;
    }

    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    ImVec2 p1(m_selectionStart.x, m_selectionStart.y);
    ImVec2 p2(m_selectionEnd.x, m_selectionEnd.y);

    draw->AddRect(p1, p2, IM_COL32(0, 255, 0, 255), 0.0f, 0, 1.0f);
    draw->AddRectFilled(p1, p2, IM_COL32(0, 255, 0, 30));
}

void MapEditor::RenderBrushPreview() {
    // Brush preview handled by viewport
}

void MapEditor::RenderPathfindingOverlay() {
    // Pathfinding overlay handled by viewport
}

} // namespace Vehement
