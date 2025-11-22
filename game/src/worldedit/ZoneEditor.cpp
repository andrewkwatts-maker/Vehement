#include "ZoneEditor.hpp"
#include <cmath>
#include <algorithm>
#include <sstream>
#include <fstream>

namespace Vehement {

// =============================================================================
// Constructor
// =============================================================================

ZoneEditor::ZoneEditor() {
    // Initialize default effects for each zone type
    m_defaultEffects[ZoneType::SafeZone] = ZoneEffect{
        "Safe Zone Effect", "Protection from PvP and hostile spawns",
        2.0f, 1.0f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 0.0f, true, false, false, false, 0.0f
    };

    m_defaultEffects[ZoneType::PvPZone] = ZoneEffect{
        "PvP Zone Effect", "Combat enabled between players",
        0.5f, 1.2f, 1.0f, 1.0f, 1.5f, 1.0f, 0.0f, 0.0f, false, false, false, false, 0.0f
    };

    m_defaultEffects[ZoneType::ResourceZone] = ZoneEffect{
        "Resource Zone Effect", "Enhanced resource gathering",
        1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 2.0f, 0.0f, 0.0f, false, false, false, false, 0.0f
    };

    m_defaultEffects[ZoneType::DangerZone] = ZoneEffect{
        "Danger Zone Effect", "Increased difficulty and rewards",
        0.5f, 1.0f, 1.5f, 1.0f, 2.0f, 1.5f, 0.0f, 0.0f, false, false, false, false, 0.0f
    };

    m_defaultEffects[ZoneType::BossZone] = ZoneEffect{
        "Boss Zone Effect", "Boss encounter area",
        0.25f, 1.0f, 1.25f, 0.9f, 3.0f, 0.5f, 0.0f, 0.0f, false, true, true, false, 0.0f
    };

    // Initialize default visuals
    m_defaultVisuals[ZoneType::SafeZone] = ZoneVisuals{
        glm::vec4(0.5f, 1.0f, 0.5f, 1.0f), 0.1f, "", 0.0f, "", 1.0f, "", 0.0f,
        glm::vec3(0.5f), true, glm::vec4(0.0f, 1.0f, 0.0f, 0.5f)
    };

    m_defaultVisuals[ZoneType::PvPZone] = ZoneVisuals{
        glm::vec4(1.0f, 0.5f, 0.5f, 1.0f), 0.15f, "", 0.0f, "", 1.0f, "", 0.0f,
        glm::vec3(0.5f), true, glm::vec4(1.0f, 0.0f, 0.0f, 0.5f)
    };

    m_defaultVisuals[ZoneType::DangerZone] = ZoneVisuals{
        glm::vec4(1.0f, 0.3f, 0.0f, 1.0f), 0.2f, "danger_particles", 1.0f, "danger_ambient", 0.5f, "", 0.1f,
        glm::vec3(0.3f, 0.0f, 0.0f), true, glm::vec4(1.0f, 0.5f, 0.0f, 0.5f)
    };

    m_defaultVisuals[ZoneType::BossZone] = ZoneVisuals{
        glm::vec4(0.5f, 0.0f, 0.5f, 1.0f), 0.3f, "boss_particles", 2.0f, "boss_ambient", 0.8f, "", 0.2f,
        glm::vec3(0.2f, 0.0f, 0.2f), true, glm::vec4(0.5f, 0.0f, 0.5f, 0.7f)
    };
}

// =============================================================================
// Zone Type
// =============================================================================

void ZoneEditor::SetZoneType(ZoneType type) {
    m_zoneType = type;
    if (m_isDrawing) {
        ApplyDefaultSettings(type);
    }
}

void ZoneEditor::ApplyDefaultSettings(ZoneType type) {
    m_currentZone.type = type;

    auto effectIt = m_defaultEffects.find(type);
    if (effectIt != m_defaultEffects.end()) {
        m_currentZone.effect = effectIt->second;
    }

    auto visualIt = m_defaultVisuals.find(type);
    if (visualIt != m_defaultVisuals.end()) {
        m_currentZone.visuals = visualIt->second;
    }
}

// =============================================================================
// Zone Drawing
// =============================================================================

void ZoneEditor::BeginZone(const std::string& name) {
    m_currentZone = GameplayZone{};
    m_currentZone.name = name;
    m_currentZone.type = m_zoneType;
    ApplyDefaultSettings(m_zoneType);
    m_isDrawing = true;
}

void ZoneEditor::AddVertex(const glm::vec2& position) {
    if (!m_isDrawing) {
        BeginZone("Zone " + std::to_string(m_zones.size() + 1));
    }

    ZoneVertex vertex;
    vertex.position = position;
    m_currentZone.vertices.push_back(vertex);
}

void ZoneEditor::RemoveLastVertex() {
    if (!m_currentZone.vertices.empty()) {
        m_currentZone.vertices.pop_back();
    }
}

bool ZoneEditor::FinishZone() {
    if (m_currentZone.vertices.size() < 3) {
        CancelZone();
        return false;
    }

    m_zones.push_back(m_currentZone);

    if (m_onZoneCreated) {
        m_onZoneCreated(m_currentZone);
    }

    m_isDrawing = false;
    m_currentZone = GameplayZone{};
    return true;
}

void ZoneEditor::CancelZone() {
    m_isDrawing = false;
    m_currentZone = GameplayZone{};
}

// =============================================================================
// Zone Effects/Visuals
// =============================================================================

void ZoneEditor::SetZoneEffect(const ZoneEffect& effect) {
    if (m_isDrawing) {
        m_currentZone.effect = effect;
    } else if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_zones.size())) {
        m_zones[m_selectedIndex].effect = effect;
        if (m_onZoneModified) {
            m_onZoneModified(m_zones[m_selectedIndex]);
        }
    }
}

const ZoneEffect& ZoneEditor::GetDefaultEffect(ZoneType type) const {
    static ZoneEffect defaultEffect;
    auto it = m_defaultEffects.find(type);
    return (it != m_defaultEffects.end()) ? it->second : defaultEffect;
}

void ZoneEditor::SetZoneVisuals(const ZoneVisuals& visuals) {
    if (m_isDrawing) {
        m_currentZone.visuals = visuals;
    } else if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_zones.size())) {
        m_zones[m_selectedIndex].visuals = visuals;
        if (m_onZoneModified) {
            m_onZoneModified(m_zones[m_selectedIndex]);
        }
    }
}

const ZoneVisuals& ZoneEditor::GetDefaultVisuals(ZoneType type) const {
    static ZoneVisuals defaultVisuals;
    auto it = m_defaultVisuals.find(type);
    return (it != m_defaultVisuals.end()) ? it->second : defaultVisuals;
}

// =============================================================================
// Zone Triggers
// =============================================================================

void ZoneEditor::AddTrigger(const ZoneTrigger& trigger) {
    if (m_isDrawing) {
        m_currentZone.triggers.push_back(trigger);
    } else if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_zones.size())) {
        m_zones[m_selectedIndex].triggers.push_back(trigger);
    }
}

void ZoneEditor::RemoveTrigger(size_t index) {
    std::vector<ZoneTrigger>* triggers = nullptr;

    if (m_isDrawing) {
        triggers = &m_currentZone.triggers;
    } else if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_zones.size())) {
        triggers = &m_zones[m_selectedIndex].triggers;
    }

    if (triggers && index < triggers->size()) {
        triggers->erase(triggers->begin() + index);
    }
}

void ZoneEditor::ClearTriggers() {
    if (m_isDrawing) {
        m_currentZone.triggers.clear();
    } else if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_zones.size())) {
        m_zones[m_selectedIndex].triggers.clear();
    }
}

// =============================================================================
// Zone Management
// =============================================================================

bool ZoneEditor::DeleteZone(int index) {
    if (index < 0 || index >= static_cast<int>(m_zones.size())) {
        return false;
    }

    m_zones.erase(m_zones.begin() + index);

    if (m_selectedIndex == index) {
        m_selectedIndex = -1;
    } else if (m_selectedIndex > index) {
        --m_selectedIndex;
    }

    return true;
}

void ZoneEditor::ClearAllZones() {
    m_zones.clear();
    m_selectedIndex = -1;
}

void ZoneEditor::SelectZone(int index) {
    if (index >= -1 && index < static_cast<int>(m_zones.size())) {
        m_selectedIndex = index;
    }
}

// =============================================================================
// Queries
// =============================================================================

std::vector<GameplayZone*> ZoneEditor::GetZonesAtPosition(const glm::vec3& position) {
    std::vector<GameplayZone*> result;
    glm::vec2 pos2D(position.x, position.z);

    for (auto& zone : m_zones) {
        if (zone.enabled &&
            position.y >= zone.minHeight && position.y <= zone.maxHeight &&
            IsPointInZone(pos2D, zone)) {
            result.push_back(&zone);
        }
    }

    // Sort by priority (highest first)
    std::sort(result.begin(), result.end(),
        [](const GameplayZone* a, const GameplayZone* b) {
            return a->priority > b->priority;
        });

    return result;
}

GameplayZone* ZoneEditor::GetHighestPriorityZoneAt(const glm::vec3& position) {
    auto zones = GetZonesAtPosition(position);
    return zones.empty() ? nullptr : zones[0];
}

bool ZoneEditor::IsInZone(const glm::vec3& position, ZoneType type) const {
    glm::vec2 pos2D(position.x, position.z);

    for (const auto& zone : m_zones) {
        if (zone.enabled && zone.type == type &&
            position.y >= zone.minHeight && position.y <= zone.maxHeight &&
            IsPointInZone(pos2D, zone)) {
            return true;
        }
    }

    return false;
}

ZoneEffect ZoneEditor::GetCombinedEffectsAt(const glm::vec3& position) const {
    ZoneEffect combined;
    glm::vec2 pos2D(position.x, position.z);

    for (const auto& zone : m_zones) {
        if (zone.enabled &&
            position.y >= zone.minHeight && position.y <= zone.maxHeight &&
            IsPointInZone(pos2D, zone)) {

            // Combine multiplicative effects
            combined.healthRegen *= zone.effect.healthRegen;
            combined.damageDealt *= zone.effect.damageDealt;
            combined.damageTaken *= zone.effect.damageTaken;
            combined.moveSpeed *= zone.effect.moveSpeed;
            combined.experienceGain *= zone.effect.experienceGain;
            combined.resourceGatherRate *= zone.effect.resourceGatherRate;

            // Combine additive effects
            combined.healthRegenFlat += zone.effect.healthRegenFlat;
            combined.damagePerSecond += zone.effect.damagePerSecond;

            // Combine flags (any zone can prevent)
            combined.preventCombat = combined.preventCombat || zone.effect.preventCombat;
            combined.preventBuilding = combined.preventBuilding || zone.effect.preventBuilding;
            combined.preventTeleport = combined.preventTeleport || zone.effect.preventTeleport;
        }
    }

    return combined;
}

// =============================================================================
// Tile Coverage
// =============================================================================

std::vector<glm::ivec2> ZoneEditor::GetZoneTiles(const GameplayZone& zone) const {
    std::vector<glm::ivec2> tiles;

    if (zone.vertices.size() < 3) return tiles;

    // Find bounding box
    float minX = zone.vertices[0].position.x;
    float maxX = zone.vertices[0].position.x;
    float minY = zone.vertices[0].position.y;
    float maxY = zone.vertices[0].position.y;

    for (const auto& v : zone.vertices) {
        minX = std::min(minX, v.position.x);
        maxX = std::max(maxX, v.position.x);
        minY = std::min(minY, v.position.y);
        maxY = std::max(maxY, v.position.y);
    }

    // Scan each tile
    for (int y = static_cast<int>(minY); y <= static_cast<int>(maxY); ++y) {
        for (int x = static_cast<int>(minX); x <= static_cast<int>(maxX); ++x) {
            glm::vec2 center(x + 0.5f, y + 0.5f);
            if (IsPointInZone(center, zone)) {
                tiles.emplace_back(x, y);
            }
        }
    }

    return tiles;
}

std::vector<glm::ivec2> ZoneEditor::GetZoneBorderTiles(const GameplayZone& zone) const {
    std::vector<glm::ivec2> borderTiles;
    auto allTiles = GetZoneTiles(zone);

    // Create set for fast lookup
    std::set<std::pair<int, int>> tileSet;
    for (const auto& tile : allTiles) {
        tileSet.insert({tile.x, tile.y});
    }

    // Find border tiles
    for (const auto& tile : allTiles) {
        bool isBorder = false;
        for (int dy = -1; dy <= 1 && !isBorder; ++dy) {
            for (int dx = -1; dx <= 1 && !isBorder; ++dx) {
                if (dx == 0 && dy == 0) continue;
                if (tileSet.find({tile.x + dx, tile.y + dy}) == tileSet.end()) {
                    isBorder = true;
                }
            }
        }
        if (isBorder) {
            borderTiles.push_back(tile);
        }
    }

    return borderTiles;
}

// =============================================================================
// Serialization
// =============================================================================

std::string ZoneEditor::ToJson() const {
    std::ostringstream ss;
    ss << "{\n  \"zones\": [\n";

    for (size_t i = 0; i < m_zones.size(); ++i) {
        const auto& zone = m_zones[i];
        if (i > 0) ss << ",\n";

        ss << "    {\n";
        ss << "      \"name\": \"" << zone.name << "\",\n";
        ss << "      \"type\": " << static_cast<int>(zone.type) << ",\n";
        ss << "      \"priority\": " << zone.priority << ",\n";
        ss << "      \"enabled\": " << (zone.enabled ? "true" : "false") << ",\n";
        ss << "      \"vertices\": [";

        for (size_t j = 0; j < zone.vertices.size(); ++j) {
            if (j > 0) ss << ", ";
            ss << "{\"x\": " << zone.vertices[j].position.x
               << ", \"y\": " << zone.vertices[j].position.y << "}";
        }

        ss << "]\n    }";
    }

    ss << "\n  ]\n}";
    return ss.str();
}

bool ZoneEditor::FromJson(const std::string& /*json*/) {
    // Simplified - would parse JSON in real implementation
    return true;
}

bool ZoneEditor::SaveZonesToFile(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) return false;
    file << ToJson();
    return file.good();
}

bool ZoneEditor::LoadZonesFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::ostringstream ss;
    ss << file.rdbuf();
    return FromJson(ss.str());
}

// =============================================================================
// Private Helpers
// =============================================================================

bool ZoneEditor::IsPointInZone(const glm::vec2& point, const GameplayZone& zone) const {
    if (zone.vertices.size() < 3) return false;

    bool inside = false;
    size_t j = zone.vertices.size() - 1;

    for (size_t i = 0; i < zone.vertices.size(); i++) {
        const glm::vec2& pi = zone.vertices[i].position;
        const glm::vec2& pj = zone.vertices[j].position;

        if ((pi.y > point.y) != (pj.y > point.y) &&
            point.x < (pj.x - pi.x) * (point.y - pi.y) / (pj.y - pi.y) + pi.x) {
            inside = !inside;
        }

        j = i;
    }

    return inside;
}

} // namespace Vehement
