#include "MenuUI.hpp"
#include "engine/ui/runtime/RuntimeUIManager.hpp"
#include "engine/ui/runtime/UIWindow.hpp"
#include "engine/ui/runtime/UIBinding.hpp"
#include "engine/ui/runtime/UIAnimation.hpp"

#include <fstream>

namespace Game {
namespace UI {

MenuUI::MenuUI() = default;
MenuUI::~MenuUI() { Shutdown(); }

bool MenuUI::Initialize(Engine::UI::RuntimeUIManager* uiManager) {
    if (!uiManager) return false;
    m_uiManager = uiManager;

    // Create main menu window
    m_mainMenuWindow = m_uiManager->CreateWindow("main_menu", "game/assets/ui/html/menu.html",
                                                 Engine::UI::UILayer::Modals);
    if (m_mainMenuWindow) {
        m_mainMenuWindow->SetTitleBarVisible(false);
        m_mainMenuWindow->Hide();
    }

    // Create pause menu window
    m_pauseWindow = m_uiManager->CreateWindow("pause_menu", "game/assets/ui/html/menu.html",
                                              Engine::UI::UILayer::Modals);
    if (m_pauseWindow) {
        m_pauseWindow->SetTitleBarVisible(false);
        m_pauseWindow->SetBackgroundColor(Engine::UI::Color(0, 0, 0, 180));
        m_pauseWindow->Hide();
    }

    // Create settings window
    m_settingsWindow = m_uiManager->CreateWindow("settings", "game/assets/ui/html/settings.html",
                                                 Engine::UI::UILayer::Modals);
    if (m_settingsWindow) {
        m_settingsWindow->SetTitle("Settings");
        m_settingsWindow->Hide();
    }

    SetupEventHandlers();
    return true;
}

void MenuUI::Shutdown() {
    if (m_uiManager) {
        if (m_mainMenuWindow) m_uiManager->CloseWindow("main_menu");
        if (m_pauseWindow) m_uiManager->CloseWindow("pause_menu");
        if (m_settingsWindow) m_uiManager->CloseWindow("settings");
    }
    m_mainMenuWindow = nullptr;
    m_pauseWindow = nullptr;
    m_settingsWindow = nullptr;
}

void MenuUI::Update(float deltaTime) {
    // Handle key bind capture
}

void MenuUI::Show() {
    m_visible = true;
    if (m_onMenuOpen) m_onMenuOpen();
}

void MenuUI::Hide() {
    m_visible = false;
    HideMainMenu();
    HidePauseMenu();
    HideSettings();
    if (m_onMenuClose) m_onMenuClose();
}

void MenuUI::ShowMainMenu() {
    m_mainMenuVisible = true;
    if (m_mainMenuWindow) {
        m_mainMenuWindow->Show();
        m_uiManager->GetAnimation().Play(m_transitionAnimation, "main_menu");
    }
    Show();
}

void MenuUI::HideMainMenu() {
    m_mainMenuVisible = false;
    if (m_mainMenuWindow) {
        m_mainMenuWindow->Hide();
    }
}

void MenuUI::SetMainMenuItems(const std::vector<MenuItem>& items) {
    MenuPage page;
    page.id = "main";
    page.title = "";
    page.items = items;
    m_pages["main"] = page;

    if (m_mainMenuVisible) {
        RenderMenuItems(items);
    }
}

void MenuUI::SetMainMenuBackground(const std::string& imagePath) {
    m_uiManager->GetBinding().CallJS("Menu.setBackground", {{"path", imagePath}});
}

void MenuUI::SetGameLogo(const std::string& imagePath) {
    m_logoPath = imagePath;
    m_uiManager->GetBinding().CallJS("Menu.setLogo", {{"path", imagePath}});
}

void MenuUI::SetVersion(const std::string& version) {
    m_version = version;
    m_uiManager->GetBinding().CallJS("Menu.setVersion", {{"version", version}});
}

void MenuUI::ShowPauseMenu() {
    m_pauseVisible = true;
    if (m_pauseWindow) {
        m_pauseWindow->Show();
        m_uiManager->GetAnimation().Play("fadeIn", "pause_menu");
    }
    Show();
}

void MenuUI::HidePauseMenu() {
    m_pauseVisible = false;
    if (m_pauseWindow) {
        m_pauseWindow->Hide();
    }
}

void MenuUI::SetPauseMenuItems(const std::vector<MenuItem>& items) {
    MenuPage page;
    page.id = "pause";
    page.title = "Paused";
    page.items = items;
    m_pages["pause"] = page;
}

void MenuUI::ShowSettings() {
    m_settingsVisible = true;
    if (m_settingsWindow) {
        m_settingsWindow->Show();
        m_settingsWindow->Center();
        m_uiManager->GetAnimation().Play("scaleIn", "settings");
    }

    // Render categories
    nlohmann::json categoriesJson = nlohmann::json::array();
    for (const auto& cat : m_settingsCategories) {
        nlohmann::json c;
        c["id"] = cat.id;
        c["name"] = cat.name;
        c["icon"] = cat.iconPath;
        categoriesJson.push_back(c);
    }
    m_uiManager->GetBinding().CallJS("Settings.setCategories", categoriesJson);

    if (!m_settingsCategories.empty()) {
        m_currentSettingsCategory = m_settingsCategories[0].id;
    }
}

void MenuUI::HideSettings() {
    m_settingsVisible = false;
    if (m_settingsWindow) {
        m_settingsWindow->Hide();
    }
}

void MenuUI::AddSettingsCategory(const SettingsCategory& category) {
    m_settingsCategories.push_back(category);
}

void MenuUI::SetSettingsCategories(const std::vector<SettingsCategory>& categories) {
    m_settingsCategories = categories;
}

void MenuUI::SetSettingValue(const std::string& categoryId, const std::string& settingId, const nlohmann::json& value) {
    m_settingsValues[categoryId][settingId] = value;
    m_uiManager->GetBinding().CallJS("Settings.setValue",
        {{"category", categoryId}, {"setting", settingId}, {"value", value}});
}

nlohmann::json MenuUI::GetSettingValue(const std::string& categoryId, const std::string& settingId) const {
    if (m_settingsValues.contains(categoryId) && m_settingsValues[categoryId].contains(settingId)) {
        return m_settingsValues[categoryId][settingId];
    }
    return nullptr;
}

void MenuUI::ApplySettings() {
    if (m_onSettingsApply) {
        m_onSettingsApply();
    }
    m_uiManager->GetBinding().CallJS("Settings.onApply", {});
}

void MenuUI::ResetSettings() {
    m_settingsValues = m_defaultSettings;
    m_uiManager->GetBinding().CallJS("Settings.onReset", m_defaultSettings);
}

void MenuUI::SaveSettings(const std::string& path) {
    std::string savePath = path.empty() ? "game/settings.json" : path;
    std::ofstream file(savePath);
    if (file.is_open()) {
        file << m_settingsValues.dump(2);
    }
}

void MenuUI::LoadSettings(const std::string& path) {
    std::string loadPath = path.empty() ? "game/settings.json" : path;
    std::ifstream file(loadPath);
    if (file.is_open()) {
        m_settingsValues = nlohmann::json::parse(file);
    }
}

void MenuUI::ShowKeyBindings() {
    m_keyBindVisible = true;
    m_uiManager->GetBinding().CallJS("Settings.showKeyBindings", {});

    nlohmann::json bindingsJson = nlohmann::json::object();
    for (const auto& [action, key] : m_keyBindings) {
        bindingsJson[action] = key;
    }
    m_uiManager->GetBinding().CallJS("Settings.setKeyBindings", bindingsJson);
}

void MenuUI::HideKeyBindings() {
    m_keyBindVisible = false;
    CancelKeyBindCapture();
}

void MenuUI::SetKeyBindings(const std::unordered_map<std::string, std::string>& bindings) {
    m_keyBindings = bindings;
    m_defaultKeyBindings = bindings;
}

void MenuUI::SetKeyBinding(const std::string& action, const std::string& key) {
    std::string oldKey = m_keyBindings[action];
    m_keyBindings[action] = key;

    if (m_onKeyBindChange) {
        m_onKeyBindChange(action, key);
    }

    m_uiManager->GetBinding().CallJS("Settings.updateKeyBind",
        {{"action", action}, {"key", key}});
}

std::string MenuUI::GetKeyBinding(const std::string& action) const {
    auto it = m_keyBindings.find(action);
    return it != m_keyBindings.end() ? it->second : "";
}

void MenuUI::ResetKeyBindings() {
    m_keyBindings = m_defaultKeyBindings;
    ShowKeyBindings(); // Refresh display
}

void MenuUI::StartKeyBindCapture(const std::string& action) {
    m_capturingKeyBind = true;
    m_capturingAction = action;
    m_uiManager->GetBinding().CallJS("Settings.startKeyCapture", {{"action", action}});
}

void MenuUI::CancelKeyBindCapture() {
    m_capturingKeyBind = false;
    m_capturingAction = "";
    m_uiManager->GetBinding().CallJS("Settings.cancelKeyCapture", {});
}

void MenuUI::NavigateToPage(const std::string& pageId) {
    if (m_pages.find(pageId) == m_pages.end()) return;

    m_navigationHistory.push_back(m_currentPageId);
    m_currentPageId = pageId;
    RefreshCurrentPage();
}

void MenuUI::NavigateBack() {
    if (m_navigationHistory.empty()) return;

    m_currentPageId = m_navigationHistory.back();
    m_navigationHistory.pop_back();
    RefreshCurrentPage();
}

void MenuUI::SetCurrentPage(const std::string& pageId) {
    m_currentPageId = pageId;
    m_navigationHistory.clear();
    RefreshCurrentPage();
}

void MenuUI::AddPage(const MenuPage& page) {
    m_pages[page.id] = page;
}

void MenuUI::RemovePage(const std::string& pageId) {
    m_pages.erase(pageId);
}

MenuPage* MenuUI::GetPage(const std::string& pageId) {
    auto it = m_pages.find(pageId);
    return it != m_pages.end() ? &it->second : nullptr;
}

void MenuUI::SetItemEnabled(const std::string& pageId, const std::string& itemId, bool enabled) {
    auto* page = GetPage(pageId);
    if (!page) return;

    for (auto& item : page->items) {
        if (item.id == itemId) {
            item.enabled = enabled;
            break;
        }
    }

    if (m_currentPageId == pageId) {
        RefreshCurrentPage();
    }
}

void MenuUI::SetItemVisible(const std::string& pageId, const std::string& itemId, bool visible) {
    auto* page = GetPage(pageId);
    if (!page) return;

    for (auto& item : page->items) {
        if (item.id == itemId) {
            item.visible = visible;
            break;
        }
    }

    if (m_currentPageId == pageId) {
        RefreshCurrentPage();
    }
}

void MenuUI::UpdateItemLabel(const std::string& pageId, const std::string& itemId, const std::string& label) {
    auto* page = GetPage(pageId);
    if (!page) return;

    for (auto& item : page->items) {
        if (item.id == itemId) {
            item.label = label;
            break;
        }
    }

    if (m_currentPageId == pageId) {
        RefreshCurrentPage();
    }
}

void MenuUI::SetOnMenuOpen(std::function<void()> callback) {
    m_onMenuOpen = callback;
}

void MenuUI::SetOnMenuClose(std::function<void()> callback) {
    m_onMenuClose = callback;
}

void MenuUI::SetOnSettingsApply(std::function<void()> callback) {
    m_onSettingsApply = callback;
}

void MenuUI::SetOnKeyBindChange(std::function<void(const std::string&, const std::string&)> callback) {
    m_onKeyBindChange = callback;
}

void MenuUI::SetTransitionAnimation(const std::string& animationName) {
    m_transitionAnimation = animationName;
}

void MenuUI::SetupEventHandlers() {
    auto& binding = m_uiManager->GetBinding();

    binding.ExposeFunction("Menu.onItemClick", [this](const nlohmann::json& args) -> nlohmann::json {
        if (!args.contains("itemId")) return nullptr;

        std::string itemId = args["itemId"].get<std::string>();
        auto* page = GetPage(m_currentPageId);
        if (!page) return nullptr;

        for (auto& item : page->items) {
            if (item.id == itemId && item.enabled) {
                if (item.onClick) item.onClick();
                if (!item.subMenuId.empty()) NavigateToPage(item.subMenuId);
                break;
            }
        }
        return nullptr;
    });

    binding.ExposeFunction("Menu.onToggle", [this](const nlohmann::json& args) -> nlohmann::json {
        if (!args.contains("itemId") || !args.contains("value")) return nullptr;

        std::string itemId = args["itemId"].get<std::string>();
        bool value = args["value"].get<bool>();

        auto* page = GetPage(m_currentPageId);
        if (!page) return nullptr;

        for (auto& item : page->items) {
            if (item.id == itemId) {
                item.toggleValue = value;
                if (item.onToggle) item.onToggle(value);
                break;
            }
        }
        return nullptr;
    });

    binding.ExposeFunction("Menu.onSlider", [this](const nlohmann::json& args) -> nlohmann::json {
        if (!args.contains("itemId") || !args.contains("value")) return nullptr;

        std::string itemId = args["itemId"].get<std::string>();
        float value = args["value"].get<float>();

        auto* page = GetPage(m_currentPageId);
        if (!page) return nullptr;

        for (auto& item : page->items) {
            if (item.id == itemId) {
                item.sliderValue = value;
                if (item.onSliderChange) item.onSliderChange(value);
                break;
            }
        }
        return nullptr;
    });

    binding.ExposeFunction("Menu.onBack", [this](const nlohmann::json&) -> nlohmann::json {
        NavigateBack();
        return nullptr;
    });

    binding.ExposeFunction("Settings.onCategorySelect", [this](const nlohmann::json& args) -> nlohmann::json {
        if (args.contains("id")) {
            m_currentSettingsCategory = args["id"].get<std::string>();
        }
        return nullptr;
    });

    binding.ExposeFunction("Settings.onKeyPress", [this](const nlohmann::json& args) -> nlohmann::json {
        if (m_capturingKeyBind && args.contains("key")) {
            SetKeyBinding(m_capturingAction, args["key"].get<std::string>());
            m_capturingKeyBind = false;
        }
        return nullptr;
    });
}

void MenuUI::RefreshCurrentPage() {
    auto* page = GetPage(m_currentPageId);
    if (!page) return;

    m_uiManager->GetBinding().CallJS("Menu.setTitle", {{"title", page->title}});
    RenderMenuItems(page->items);
}

void MenuUI::RenderMenuItems(const std::vector<MenuItem>& items) {
    nlohmann::json itemsJson = nlohmann::json::array();

    for (const auto& item : items) {
        if (!item.visible) continue;

        nlohmann::json i;
        i["id"] = item.id;
        i["label"] = item.label;
        i["type"] = static_cast<int>(item.type);
        i["enabled"] = item.enabled;

        switch (item.type) {
            case MenuItemType::Toggle:
                i["value"] = item.toggleValue;
                break;
            case MenuItemType::Slider:
                i["value"] = item.sliderValue;
                i["min"] = item.sliderMin;
                i["max"] = item.sliderMax;
                i["step"] = item.sliderStep;
                break;
            case MenuItemType::Dropdown:
                i["options"] = item.dropdownOptions;
                i["selected"] = item.selectedOption;
                break;
            case MenuItemType::KeyBind:
                i["action"] = item.keyBindAction;
                i["key"] = item.currentKey;
                break;
            default:
                break;
        }

        itemsJson.push_back(i);
    }

    m_uiManager->GetBinding().CallJS("Menu.setItems", itemsJson);
}

void MenuUI::HandleKeyPress(int keyCode) {
    if (m_capturingKeyBind) {
        // Convert key code to string
        std::string keyName = "Key" + std::to_string(keyCode);
        SetKeyBinding(m_capturingAction, keyName);
        CancelKeyBindCapture();
    }
}

} // namespace UI
} // namespace Game
