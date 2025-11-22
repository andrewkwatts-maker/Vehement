#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace Engine { namespace UI { class RuntimeUIManager; class UIWindow; class UIDataBinding; } }

namespace Game {
namespace UI {

/**
 * @brief Unit selection info for HUD display
 */
struct UnitInfo {
    std::string id;
    std::string name;
    std::string type;
    int health = 100;
    int maxHealth = 100;
    int mana = 0;
    int maxMana = 0;
    std::string iconPath;
    std::vector<std::string> abilities;
    bool isSelected = false;
};

/**
 * @brief Resource data for display
 */
struct ResourceData {
    std::string type;
    int current = 0;
    int max = 0;
    int rate = 0; // per second
    std::string iconPath;
};

/**
 * @brief Ability data for ability bar
 */
struct AbilityData {
    std::string id;
    std::string name;
    std::string iconPath;
    float cooldown = 0;
    float maxCooldown = 0;
    int manaCost = 0;
    std::string hotkey;
    bool available = true;
    bool active = false;
};

/**
 * @brief Chat message data
 */
struct ChatMessage {
    std::string sender;
    std::string message;
    std::string channel;
    double timestamp;
    bool isSystem = false;
};

/**
 * @brief Minimap marker data
 */
struct MinimapMarker {
    std::string id;
    float x, y;
    std::string type; // "player", "enemy", "objective", "ping"
    std::string color;
    bool blinking = false;
};

/**
 * @brief Main Game HUD
 *
 * Provides health/mana bars, minimap, resource display,
 * unit selection info, ability bar, and chat window.
 */
class GameHUD {
public:
    GameHUD();
    ~GameHUD();

    /**
     * @brief Initialize the HUD
     * @param uiManager UI manager reference
     * @return true if initialization succeeded
     */
    bool Initialize(Engine::UI::RuntimeUIManager* uiManager);

    /**
     * @brief Shutdown the HUD
     */
    void Shutdown();

    /**
     * @brief Update the HUD
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    /**
     * @brief Show/hide the HUD
     */
    void Show();
    void Hide();
    bool IsVisible() const { return m_visible; }

    // Health/Mana Bars

    /**
     * @brief Update player health bar
     */
    void SetPlayerHealth(int current, int max);

    /**
     * @brief Update player mana bar
     */
    void SetPlayerMana(int current, int max);

    /**
     * @brief Set player name
     */
    void SetPlayerName(const std::string& name);

    /**
     * @brief Set player level
     */
    void SetPlayerLevel(int level);

    /**
     * @brief Set experience progress
     */
    void SetExperience(int current, int max);

    // Resource Display

    /**
     * @brief Set resource values
     */
    void SetResource(const std::string& type, int current, int max, int rate = 0);

    /**
     * @brief Add resource type
     */
    void AddResourceType(const ResourceData& resource);

    /**
     * @brief Remove resource type
     */
    void RemoveResourceType(const std::string& type);

    // Unit Selection

    /**
     * @brief Set selected unit info
     */
    void SetSelectedUnit(const UnitInfo& unit);

    /**
     * @brief Set multiple selected units
     */
    void SetSelectedUnits(const std::vector<UnitInfo>& units);

    /**
     * @brief Clear unit selection display
     */
    void ClearSelection();

    // Ability Bar

    /**
     * @brief Set abilities
     */
    void SetAbilities(const std::vector<AbilityData>& abilities);

    /**
     * @brief Update ability cooldown
     */
    void SetAbilityCooldown(const std::string& abilityId, float cooldown);

    /**
     * @brief Set ability available state
     */
    void SetAbilityAvailable(const std::string& abilityId, bool available);

    /**
     * @brief Highlight ability (when activated)
     */
    void HighlightAbility(const std::string& abilityId, bool highlight);

    /**
     * @brief Set ability click callback
     */
    void SetAbilityCallback(std::function<void(const std::string&)> callback);

    // Minimap

    /**
     * @brief Set minimap texture
     */
    void SetMinimapTexture(const std::string& texturePath);

    /**
     * @brief Set minimap bounds (world coordinates)
     */
    void SetMinimapBounds(float minX, float minY, float maxX, float maxY);

    /**
     * @brief Set player position on minimap
     */
    void SetMinimapPlayerPosition(float x, float y, float rotation);

    /**
     * @brief Add marker to minimap
     */
    void AddMinimapMarker(const MinimapMarker& marker);

    /**
     * @brief Remove marker from minimap
     */
    void RemoveMinimapMarker(const std::string& markerId);

    /**
     * @brief Clear all markers
     */
    void ClearMinimapMarkers();

    /**
     * @brief Set minimap click callback
     */
    void SetMinimapClickCallback(std::function<void(float, float)> callback);

    // Chat Window

    /**
     * @brief Add chat message
     */
    void AddChatMessage(const ChatMessage& message);

    /**
     * @brief Clear chat history
     */
    void ClearChat();

    /**
     * @brief Set chat visible
     */
    void SetChatVisible(bool visible);

    /**
     * @brief Focus chat input
     */
    void FocusChatInput();

    /**
     * @brief Set chat send callback
     */
    void SetChatCallback(std::function<void(const std::string&, const std::string&)> callback);

    // Notifications

    /**
     * @brief Show quick notification on HUD
     */
    void ShowNotification(const std::string& message, float duration = 3.0f);

    /**
     * @brief Show combat text
     */
    void ShowCombatText(float x, float y, const std::string& text, const std::string& type = "damage");

    // Objectives

    /**
     * @brief Set current objective
     */
    void SetObjective(const std::string& text, bool completed = false);

    /**
     * @brief Add sub-objective
     */
    void AddSubObjective(const std::string& id, const std::string& text, bool completed = false);

    /**
     * @brief Update sub-objective
     */
    void UpdateSubObjective(const std::string& id, bool completed);

    // State

    /**
     * @brief Set HUD opacity
     */
    void SetOpacity(float opacity);

    /**
     * @brief Set HUD scale
     */
    void SetScale(float scale);

    /**
     * @brief Enable/disable element
     */
    void SetElementVisible(const std::string& elementId, bool visible);

private:
    void SetupDataBindings();
    void SetupEventHandlers();
    void UpdateResourceDisplay();
    void UpdateAbilityBar();
    void UpdateMinimap();

    Engine::UI::RuntimeUIManager* m_uiManager = nullptr;
    Engine::UI::UIWindow* m_hudWindow = nullptr;
    Engine::UI::UIDataBinding* m_dataBinding = nullptr;

    bool m_visible = true;
    float m_opacity = 1.0f;
    float m_scale = 1.0f;

    // Player data
    std::string m_playerName;
    int m_playerLevel = 1;
    int m_health = 100, m_maxHealth = 100;
    int m_mana = 0, m_maxMana = 0;
    int m_experience = 0, m_maxExperience = 100;

    // Resources
    std::vector<ResourceData> m_resources;

    // Selection
    std::vector<UnitInfo> m_selectedUnits;

    // Abilities
    std::vector<AbilityData> m_abilities;
    std::function<void(const std::string&)> m_abilityCallback;

    // Minimap
    std::vector<MinimapMarker> m_minimapMarkers;
    float m_minimapBounds[4] = {0, 0, 1000, 1000};
    float m_playerPosX = 0, m_playerPosY = 0, m_playerRotation = 0;
    std::function<void(float, float)> m_minimapClickCallback;

    // Chat
    std::vector<ChatMessage> m_chatHistory;
    std::function<void(const std::string&, const std::string&)> m_chatCallback;

    // Objectives
    std::string m_currentObjective;
    std::vector<std::pair<std::string, std::pair<std::string, bool>>> m_subObjectives;
};

} // namespace UI
} // namespace Game
