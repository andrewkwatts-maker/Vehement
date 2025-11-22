#include "GameHUD.hpp"
#include "engine/ui/runtime/RuntimeUIManager.hpp"
#include "engine/ui/runtime/UIWindow.hpp"
#include "engine/ui/runtime/UIDataBinding.hpp"
#include "engine/ui/runtime/UIBinding.hpp"
#include "engine/ui/runtime/UIAnimation.hpp"

#include <sstream>
#include <algorithm>

namespace Game {
namespace UI {

GameHUD::GameHUD() = default;
GameHUD::~GameHUD() { Shutdown(); }

bool GameHUD::Initialize(Engine::UI::RuntimeUIManager* uiManager) {
    if (!uiManager) return false;

    m_uiManager = uiManager;

    // Create HUD window
    m_hudWindow = m_uiManager->CreateWindow("game_hud", "game/assets/ui/html/hud.html",
                                           Engine::UI::UILayer::HUD);
    if (!m_hudWindow) return false;

    m_hudWindow->SetTitleBarVisible(false);
    m_hudWindow->SetResizable(false);
    m_hudWindow->SetDraggable(false);
    m_hudWindow->SetBackgroundColor(Engine::UI::Color(0, 0, 0, 0));

    // Get data binding system
    m_dataBinding = &m_uiManager->GetBinding().GetDataBinding();

    SetupDataBindings();
    SetupEventHandlers();

    return true;
}

void GameHUD::Shutdown() {
    if (m_uiManager && m_hudWindow) {
        m_uiManager->CloseWindow("game_hud");
        m_hudWindow = nullptr;
    }
    m_uiManager = nullptr;
}

void GameHUD::Update(float deltaTime) {
    if (!m_visible) return;

    // Update ability cooldowns
    for (auto& ability : m_abilities) {
        if (ability.cooldown > 0) {
            ability.cooldown -= deltaTime;
            if (ability.cooldown < 0) ability.cooldown = 0;
        }
    }

    UpdateAbilityBar();
}

void GameHUD::Show() {
    m_visible = true;
    if (m_hudWindow) {
        m_hudWindow->Show();
        m_uiManager->GetAnimation().Play("fadeIn", "game_hud");
    }
}

void GameHUD::Hide() {
    m_visible = false;
    if (m_hudWindow) {
        m_uiManager->GetAnimation().Play("fadeOut", "game_hud");
        // Hide after animation
        m_hudWindow->Hide();
    }
}

void GameHUD::SetPlayerHealth(int current, int max) {
    m_health = current;
    m_maxHealth = max;

    if (m_dataBinding) {
        m_dataBinding->SetValue("player.health", current);
        m_dataBinding->SetValue("player.maxHealth", max);
        m_dataBinding->SetValue("player.healthPercent", max > 0 ? (current * 100 / max) : 0);
    }

    // Trigger damage animation if health decreased significantly
    static int lastHealth = current;
    if (current < lastHealth - 10) {
        m_uiManager->GetAnimation().Play("shake", "health-bar");
    }
    lastHealth = current;
}

void GameHUD::SetPlayerMana(int current, int max) {
    m_mana = current;
    m_maxMana = max;

    if (m_dataBinding) {
        m_dataBinding->SetValue("player.mana", current);
        m_dataBinding->SetValue("player.maxMana", max);
        m_dataBinding->SetValue("player.manaPercent", max > 0 ? (current * 100 / max) : 0);
    }
}

void GameHUD::SetPlayerName(const std::string& name) {
    m_playerName = name;
    if (m_dataBinding) {
        m_dataBinding->SetValue("player.name", name);
    }
}

void GameHUD::SetPlayerLevel(int level) {
    m_playerLevel = level;
    if (m_dataBinding) {
        m_dataBinding->SetValue("player.level", level);
    }
}

void GameHUD::SetExperience(int current, int max) {
    m_experience = current;
    m_maxExperience = max;

    if (m_dataBinding) {
        m_dataBinding->SetValue("player.experience", current);
        m_dataBinding->SetValue("player.maxExperience", max);
        m_dataBinding->SetValue("player.expPercent", max > 0 ? (current * 100 / max) : 0);
    }
}

void GameHUD::SetResource(const std::string& type, int current, int max, int rate) {
    for (auto& resource : m_resources) {
        if (resource.type == type) {
            resource.current = current;
            resource.max = max;
            resource.rate = rate;
            break;
        }
    }

    if (m_dataBinding) {
        m_dataBinding->SetValue("resources." + type + ".current", current);
        m_dataBinding->SetValue("resources." + type + ".max", max);
        m_dataBinding->SetValue("resources." + type + ".rate", rate);
    }

    UpdateResourceDisplay();
}

void GameHUD::AddResourceType(const ResourceData& resource) {
    m_resources.push_back(resource);
    UpdateResourceDisplay();
}

void GameHUD::RemoveResourceType(const std::string& type) {
    m_resources.erase(
        std::remove_if(m_resources.begin(), m_resources.end(),
            [&type](const ResourceData& r) { return r.type == type; }),
        m_resources.end());
    UpdateResourceDisplay();
}

void GameHUD::SetSelectedUnit(const UnitInfo& unit) {
    m_selectedUnits.clear();
    m_selectedUnits.push_back(unit);

    if (m_dataBinding) {
        m_dataBinding->SetValue("selection.count", 1);
        m_dataBinding->SetValue("selection.unit.name", unit.name);
        m_dataBinding->SetValue("selection.unit.type", unit.type);
        m_dataBinding->SetValue("selection.unit.health", unit.health);
        m_dataBinding->SetValue("selection.unit.maxHealth", unit.maxHealth);
        m_dataBinding->SetValue("selection.unit.icon", unit.iconPath);
    }
}

void GameHUD::SetSelectedUnits(const std::vector<UnitInfo>& units) {
    m_selectedUnits = units;

    if (m_dataBinding) {
        m_dataBinding->SetValue("selection.count", static_cast<int>(units.size()));

        nlohmann::json unitsJson = nlohmann::json::array();
        for (const auto& unit : units) {
            nlohmann::json u;
            u["id"] = unit.id;
            u["name"] = unit.name;
            u["health"] = unit.health;
            u["maxHealth"] = unit.maxHealth;
            u["icon"] = unit.iconPath;
            unitsJson.push_back(u);
        }
        m_dataBinding->SetValue("selection.units", unitsJson);
    }
}

void GameHUD::ClearSelection() {
    m_selectedUnits.clear();
    if (m_dataBinding) {
        m_dataBinding->SetValue("selection.count", 0);
    }
}

void GameHUD::SetAbilities(const std::vector<AbilityData>& abilities) {
    m_abilities = abilities;
    UpdateAbilityBar();
}

void GameHUD::SetAbilityCooldown(const std::string& abilityId, float cooldown) {
    for (auto& ability : m_abilities) {
        if (ability.id == abilityId) {
            ability.cooldown = cooldown;
            break;
        }
    }
    UpdateAbilityBar();
}

void GameHUD::SetAbilityAvailable(const std::string& abilityId, bool available) {
    for (auto& ability : m_abilities) {
        if (ability.id == abilityId) {
            ability.available = available;
            break;
        }
    }
    UpdateAbilityBar();
}

void GameHUD::HighlightAbility(const std::string& abilityId, bool highlight) {
    for (auto& ability : m_abilities) {
        if (ability.id == abilityId) {
            ability.active = highlight;
            break;
        }
    }

    if (highlight) {
        m_uiManager->GetAnimation().Play("pulse", "ability-" + abilityId);
    } else {
        m_uiManager->GetAnimation().Stop("ability-" + abilityId);
    }
}

void GameHUD::SetAbilityCallback(std::function<void(const std::string&)> callback) {
    m_abilityCallback = callback;
}

void GameHUD::SetMinimapTexture(const std::string& texturePath) {
    if (m_dataBinding) {
        m_dataBinding->SetValue("minimap.texture", texturePath);
    }
}

void GameHUD::SetMinimapBounds(float minX, float minY, float maxX, float maxY) {
    m_minimapBounds[0] = minX;
    m_minimapBounds[1] = minY;
    m_minimapBounds[2] = maxX;
    m_minimapBounds[3] = maxY;
}

void GameHUD::SetMinimapPlayerPosition(float x, float y, float rotation) {
    m_playerPosX = x;
    m_playerPosY = y;
    m_playerRotation = rotation;

    // Convert world position to minimap coordinates
    float mapWidth = m_minimapBounds[2] - m_minimapBounds[0];
    float mapHeight = m_minimapBounds[3] - m_minimapBounds[1];

    float mapX = (x - m_minimapBounds[0]) / mapWidth * 100; // Percentage
    float mapY = (y - m_minimapBounds[1]) / mapHeight * 100;

    if (m_dataBinding) {
        m_dataBinding->SetValue("minimap.player.x", mapX);
        m_dataBinding->SetValue("minimap.player.y", mapY);
        m_dataBinding->SetValue("minimap.player.rotation", rotation);
    }
}

void GameHUD::AddMinimapMarker(const MinimapMarker& marker) {
    m_minimapMarkers.push_back(marker);
    UpdateMinimap();
}

void GameHUD::RemoveMinimapMarker(const std::string& markerId) {
    m_minimapMarkers.erase(
        std::remove_if(m_minimapMarkers.begin(), m_minimapMarkers.end(),
            [&markerId](const MinimapMarker& m) { return m.id == markerId; }),
        m_minimapMarkers.end());
    UpdateMinimap();
}

void GameHUD::ClearMinimapMarkers() {
    m_minimapMarkers.clear();
    UpdateMinimap();
}

void GameHUD::SetMinimapClickCallback(std::function<void(float, float)> callback) {
    m_minimapClickCallback = callback;
}

void GameHUD::AddChatMessage(const ChatMessage& message) {
    m_chatHistory.push_back(message);

    // Limit chat history
    if (m_chatHistory.size() > 100) {
        m_chatHistory.erase(m_chatHistory.begin());
    }

    // Update UI
    nlohmann::json msgJson;
    msgJson["sender"] = message.sender;
    msgJson["message"] = message.message;
    msgJson["channel"] = message.channel;
    msgJson["isSystem"] = message.isSystem;

    m_uiManager->GetBinding().CallJS("UI.addChatMessage", msgJson);

    // Scroll to bottom
    m_uiManager->ExecuteScript("game_hud", "document.getElementById('chat-messages').scrollTop = document.getElementById('chat-messages').scrollHeight;");
}

void GameHUD::ClearChat() {
    m_chatHistory.clear();
    m_uiManager->ExecuteScript("game_hud", "document.getElementById('chat-messages').innerHTML = '';");
}

void GameHUD::SetChatVisible(bool visible) {
    if (m_hudWindow) {
        auto* chatElement = m_hudWindow->GetElementById("chat-window");
        if (chatElement) {
            chatElement->isVisible = visible;
        }
    }
}

void GameHUD::FocusChatInput() {
    m_uiManager->ExecuteScript("game_hud", "document.getElementById('chat-input').focus();");
}

void GameHUD::SetChatCallback(std::function<void(const std::string&, const std::string&)> callback) {
    m_chatCallback = callback;
}

void GameHUD::ShowNotification(const std::string& message, float duration) {
    nlohmann::json args;
    args["message"] = message;
    args["duration"] = duration;
    m_uiManager->GetBinding().CallJS("UI.showNotification", args);
}

void GameHUD::ShowCombatText(float x, float y, const std::string& text, const std::string& type) {
    nlohmann::json args;
    args["x"] = x;
    args["y"] = y;
    args["text"] = text;
    args["type"] = type;
    m_uiManager->GetBinding().CallJS("UI.showCombatText", args);
}

void GameHUD::SetObjective(const std::string& text, bool completed) {
    m_currentObjective = text;
    if (m_dataBinding) {
        m_dataBinding->SetValue("objective.text", text);
        m_dataBinding->SetValue("objective.completed", completed);
    }
}

void GameHUD::AddSubObjective(const std::string& id, const std::string& text, bool completed) {
    m_subObjectives.emplace_back(id, std::make_pair(text, completed));

    nlohmann::json objectives = nlohmann::json::array();
    for (const auto& [objId, objData] : m_subObjectives) {
        nlohmann::json obj;
        obj["id"] = objId;
        obj["text"] = objData.first;
        obj["completed"] = objData.second;
        objectives.push_back(obj);
    }

    if (m_dataBinding) {
        m_dataBinding->SetValue("objective.subObjectives", objectives);
    }
}

void GameHUD::UpdateSubObjective(const std::string& id, bool completed) {
    for (auto& [objId, objData] : m_subObjectives) {
        if (objId == id) {
            objData.second = completed;
            break;
        }
    }

    // Refresh sub-objectives display
    AddSubObjective("", "", false); // Trigger refresh
}

void GameHUD::SetOpacity(float opacity) {
    m_opacity = opacity;
    if (m_hudWindow) {
        m_hudWindow->SetOpacity(opacity);
    }
}

void GameHUD::SetScale(float scale) {
    m_scale = scale;
    if (m_dataBinding) {
        m_dataBinding->SetValue("hud.scale", scale);
    }
}

void GameHUD::SetElementVisible(const std::string& elementId, bool visible) {
    if (m_hudWindow) {
        auto* element = m_hudWindow->GetElementById(elementId);
        if (element) {
            element->isVisible = visible;
        }
    }
}

void GameHUD::SetupDataBindings() {
    if (!m_dataBinding) return;

    // Initialize default values
    m_dataBinding->SetValue("player.name", "Player");
    m_dataBinding->SetValue("player.level", 1);
    m_dataBinding->SetValue("player.health", 100);
    m_dataBinding->SetValue("player.maxHealth", 100);
    m_dataBinding->SetValue("player.healthPercent", 100);
    m_dataBinding->SetValue("player.mana", 0);
    m_dataBinding->SetValue("player.maxMana", 0);
    m_dataBinding->SetValue("player.manaPercent", 0);
    m_dataBinding->SetValue("selection.count", 0);
    m_dataBinding->SetValue("hud.scale", 1.0f);
}

void GameHUD::SetupEventHandlers() {
    auto& binding = m_uiManager->GetBinding();

    // Ability click handler
    binding.ExposeFunction("HUD.onAbilityClick", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("abilityId") && m_abilityCallback) {
            m_abilityCallback(args["abilityId"].get<std::string>());
        }
        return nullptr;
    });

    // Minimap click handler
    binding.ExposeFunction("HUD.onMinimapClick", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("x") && args.contains("y") && m_minimapClickCallback) {
            // Convert minimap percentage to world coordinates
            float mapX = args["x"].get<float>();
            float mapY = args["y"].get<float>();

            float worldX = m_minimapBounds[0] + mapX / 100.0f * (m_minimapBounds[2] - m_minimapBounds[0]);
            float worldY = m_minimapBounds[1] + mapY / 100.0f * (m_minimapBounds[3] - m_minimapBounds[1]);

            m_minimapClickCallback(worldX, worldY);
        }
        return nullptr;
    });

    // Chat send handler
    binding.ExposeFunction("HUD.onChatSend", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("message") && m_chatCallback) {
            std::string channel = args.value("channel", "all");
            m_chatCallback(args["message"].get<std::string>(), channel);
        }
        return nullptr;
    });
}

void GameHUD::UpdateResourceDisplay() {
    if (!m_dataBinding) return;

    nlohmann::json resourcesJson = nlohmann::json::array();
    for (const auto& resource : m_resources) {
        nlohmann::json r;
        r["type"] = resource.type;
        r["current"] = resource.current;
        r["max"] = resource.max;
        r["rate"] = resource.rate;
        r["icon"] = resource.iconPath;
        resourcesJson.push_back(r);
    }

    m_dataBinding->SetValue("resources.list", resourcesJson);
}

void GameHUD::UpdateAbilityBar() {
    if (!m_dataBinding) return;

    nlohmann::json abilitiesJson = nlohmann::json::array();
    for (const auto& ability : m_abilities) {
        nlohmann::json a;
        a["id"] = ability.id;
        a["name"] = ability.name;
        a["icon"] = ability.iconPath;
        a["cooldown"] = ability.cooldown;
        a["maxCooldown"] = ability.maxCooldown;
        a["cooldownPercent"] = ability.maxCooldown > 0 ? (ability.cooldown / ability.maxCooldown * 100) : 0;
        a["manaCost"] = ability.manaCost;
        a["hotkey"] = ability.hotkey;
        a["available"] = ability.available && ability.cooldown <= 0;
        a["active"] = ability.active;
        abilitiesJson.push_back(a);
    }

    m_dataBinding->SetValue("abilities", abilitiesJson);
}

void GameHUD::UpdateMinimap() {
    if (!m_dataBinding) return;

    nlohmann::json markersJson = nlohmann::json::array();
    for (const auto& marker : m_minimapMarkers) {
        // Convert world position to minimap coordinates
        float mapWidth = m_minimapBounds[2] - m_minimapBounds[0];
        float mapHeight = m_minimapBounds[3] - m_minimapBounds[1];

        float mapX = (marker.x - m_minimapBounds[0]) / mapWidth * 100;
        float mapY = (marker.y - m_minimapBounds[1]) / mapHeight * 100;

        nlohmann::json m;
        m["id"] = marker.id;
        m["x"] = mapX;
        m["y"] = mapY;
        m["type"] = marker.type;
        m["color"] = marker.color;
        m["blinking"] = marker.blinking;
        markersJson.push_back(m);
    }

    m_dataBinding->SetValue("minimap.markers", markersJson);
}

} // namespace UI
} // namespace Game
