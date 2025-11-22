#pragma once

#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <memory>

namespace Engine { namespace UI { class RuntimeUIManager; class UIWindow; } }

namespace Game {
namespace UI {

/**
 * @brief Menu item types
 */
enum class MenuItemType {
    Button,
    Toggle,
    Slider,
    Dropdown,
    KeyBind,
    Separator,
    SubMenu
};

/**
 * @brief Menu item data
 */
struct MenuItem {
    std::string id;
    std::string label;
    MenuItemType type = MenuItemType::Button;
    bool enabled = true;
    bool visible = true;

    // Button
    std::function<void()> onClick;

    // Toggle
    bool toggleValue = false;
    std::function<void(bool)> onToggle;

    // Slider
    float sliderValue = 0;
    float sliderMin = 0;
    float sliderMax = 100;
    float sliderStep = 1;
    std::function<void(float)> onSliderChange;

    // Dropdown
    std::vector<std::string> dropdownOptions;
    int selectedOption = 0;
    std::function<void(int, const std::string&)> onDropdownChange;

    // KeyBind
    std::string keyBindAction;
    std::string currentKey;
    std::function<void(const std::string&)> onKeyBind;

    // SubMenu
    std::string subMenuId;
};

/**
 * @brief Menu page/screen
 */
struct MenuPage {
    std::string id;
    std::string title;
    std::vector<MenuItem> items;
    std::string parentPage; // For back navigation
    std::string backgroundImage;
};

/**
 * @brief Settings category
 */
struct SettingsCategory {
    std::string id;
    std::string name;
    std::string iconPath;
    std::vector<MenuItem> settings;
};

/**
 * @brief Menu UI System
 *
 * Main menu, pause menu, settings menu, and key bindings.
 */
class MenuUI {
public:
    MenuUI();
    ~MenuUI();

    bool Initialize(Engine::UI::RuntimeUIManager* uiManager);
    void Shutdown();
    void Update(float deltaTime);

    // Visibility
    void Show();
    void Hide();
    bool IsVisible() const { return m_visible; }

    // Main Menu
    void ShowMainMenu();
    void HideMainMenu();
    void SetMainMenuItems(const std::vector<MenuItem>& items);
    void SetMainMenuBackground(const std::string& imagePath);
    void SetGameLogo(const std::string& imagePath);
    void SetVersion(const std::string& version);

    // Pause Menu
    void ShowPauseMenu();
    void HidePauseMenu();
    void SetPauseMenuItems(const std::vector<MenuItem>& items);
    bool IsPaused() const { return m_pauseVisible; }

    // Settings Menu
    void ShowSettings();
    void HideSettings();
    void AddSettingsCategory(const SettingsCategory& category);
    void SetSettingsCategories(const std::vector<SettingsCategory>& categories);
    void SetSettingValue(const std::string& categoryId, const std::string& settingId, const nlohmann::json& value);
    nlohmann::json GetSettingValue(const std::string& categoryId, const std::string& settingId) const;
    void ApplySettings();
    void ResetSettings();
    void SaveSettings(const std::string& path = "");
    void LoadSettings(const std::string& path = "");

    // Key Bindings
    void ShowKeyBindings();
    void HideKeyBindings();
    void SetKeyBindings(const std::unordered_map<std::string, std::string>& bindings);
    void SetKeyBinding(const std::string& action, const std::string& key);
    std::string GetKeyBinding(const std::string& action) const;
    void ResetKeyBindings();
    void StartKeyBindCapture(const std::string& action);
    void CancelKeyBindCapture();
    bool IsCapturingKeyBind() const { return m_capturingKeyBind; }

    // Navigation
    void NavigateToPage(const std::string& pageId);
    void NavigateBack();
    void SetCurrentPage(const std::string& pageId);
    std::string GetCurrentPage() const { return m_currentPageId; }

    // Menu Pages
    void AddPage(const MenuPage& page);
    void RemovePage(const std::string& pageId);
    MenuPage* GetPage(const std::string& pageId);

    // Item Management
    void SetItemEnabled(const std::string& pageId, const std::string& itemId, bool enabled);
    void SetItemVisible(const std::string& pageId, const std::string& itemId, bool visible);
    void UpdateItemLabel(const std::string& pageId, const std::string& itemId, const std::string& label);

    // Callbacks
    void SetOnMenuOpen(std::function<void()> callback);
    void SetOnMenuClose(std::function<void()> callback);
    void SetOnSettingsApply(std::function<void()> callback);
    void SetOnKeyBindChange(std::function<void(const std::string&, const std::string&)> callback);

    // Animations
    void SetTransitionAnimation(const std::string& animationName);

private:
    void SetupEventHandlers();
    void RefreshCurrentPage();
    void RenderMenuItems(const std::vector<MenuItem>& items);
    void HandleKeyPress(int keyCode);

    Engine::UI::RuntimeUIManager* m_uiManager = nullptr;
    Engine::UI::UIWindow* m_mainMenuWindow = nullptr;
    Engine::UI::UIWindow* m_pauseWindow = nullptr;
    Engine::UI::UIWindow* m_settingsWindow = nullptr;
    Engine::UI::UIWindow* m_keyBindWindow = nullptr;

    bool m_visible = false;
    bool m_mainMenuVisible = false;
    bool m_pauseVisible = false;
    bool m_settingsVisible = false;
    bool m_keyBindVisible = false;

    std::unordered_map<std::string, MenuPage> m_pages;
    std::string m_currentPageId;
    std::vector<std::string> m_navigationHistory;

    std::vector<SettingsCategory> m_settingsCategories;
    std::string m_currentSettingsCategory;
    nlohmann::json m_settingsValues;
    nlohmann::json m_defaultSettings;

    std::unordered_map<std::string, std::string> m_keyBindings;
    std::unordered_map<std::string, std::string> m_defaultKeyBindings;
    bool m_capturingKeyBind = false;
    std::string m_capturingAction;

    std::function<void()> m_onMenuOpen;
    std::function<void()> m_onMenuClose;
    std::function<void()> m_onSettingsApply;
    std::function<void(const std::string&, const std::string&)> m_onKeyBindChange;

    std::string m_transitionAnimation = "fadeIn";
    std::string m_logoPath;
    std::string m_version;
};

} // namespace UI
} // namespace Game
