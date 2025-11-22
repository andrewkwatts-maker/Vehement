#include "MainMenu.hpp"
#include "engine/ui/runtime/RuntimeUIManager.hpp"
#include "engine/ui/runtime/UIWindow.hpp"
#include "engine/ui/runtime/UIBinding.hpp"
#include <nlohmann/json.hpp>

namespace Game {
namespace UI {
namespace Menu {

MainMenu::MainMenu() = default;
MainMenu::~MainMenu() = default;

bool MainMenu::Initialize(Engine::UI::RuntimeUIManager* uiManager) {
    if (!uiManager) {
        return false;
    }

    m_uiManager = uiManager;
    SetupEventHandlers();

    // Pre-create main menu window
    auto* mainWindow = m_uiManager->CreateWindow(
        "main_menu",
        GetHtmlPathForState(MenuState::Main),
        Engine::UI::UILayer::Background
    );

    if (mainWindow) {
        m_stateWindows[MenuState::Main] = mainWindow;
    }

    return true;
}

void MainMenu::Shutdown() {
    // Close all state windows
    for (auto& [state, window] : m_stateWindows) {
        if (window) {
            m_uiManager->CloseWindow(GetWindowIdForState(state));
        }
    }
    m_stateWindows.clear();

    StopMusic();
    m_uiManager = nullptr;
}

void MainMenu::Update(float deltaTime) {
    if (m_transitioning) {
        UpdateTransition(deltaTime);
    }
}

void MainMenu::SetupEventHandlers() {
    if (!m_uiManager) return;

    auto& binding = m_uiManager->GetBinding();

    // Register menu navigation handlers
    binding.RegisterHandler("Menu.navigate", [this](const nlohmann::json& data) {
        std::string page = data.value("page", "");
        MenuState targetState = MenuState::None;

        if (page == "main") targetState = MenuState::Main;
        else if (page == "campaign") targetState = MenuState::Campaign;
        else if (page == "multiplayer") targetState = MenuState::Multiplayer;
        else if (page == "custom_games") targetState = MenuState::CustomGames;
        else if (page == "editor") targetState = MenuState::Editor;
        else if (page == "settings") targetState = MenuState::Settings;
        else if (page == "credits") targetState = MenuState::Credits;

        if (targetState != MenuState::None) {
            NavigateTo(targetState);
        }
    });

    binding.RegisterHandler("Menu.onBack", [this](const nlohmann::json&) {
        NavigateBack();
    });

    binding.RegisterHandler("Menu.exitGame", [this](const nlohmann::json&) {
        if (m_onExitRequest) {
            m_onExitRequest();
        }
    });

    binding.RegisterHandler("Menu.getPlayerProfile", [this](const nlohmann::json&) -> nlohmann::json {
        return {
            {"name", m_playerProfile.name},
            {"avatar", m_playerProfile.avatarPath},
            {"rank", m_playerProfile.rank},
            {"rankIcon", m_playerProfile.rankIcon},
            {"level", m_playerProfile.level},
            {"wins", m_playerProfile.wins},
            {"losses", m_playerProfile.losses}
        };
    });

    binding.RegisterHandler("Menu.getNews", [this](const nlohmann::json&) -> nlohmann::json {
        nlohmann::json newsArray = nlohmann::json::array();
        for (const auto& item : m_newsItems) {
            newsArray.push_back({
                {"id", item.id},
                {"title", item.title},
                {"excerpt", item.excerpt},
                {"date", item.date},
                {"type", item.type}
            });
        }
        return newsArray;
    });

    binding.RegisterHandler("Menu.openNews", [this](const nlohmann::json& data) {
        int newsId = data.value("newsId", 0);
        if (m_onNewsClick) {
            m_onNewsClick(newsId);
        }
    });

    binding.RegisterHandler("Menu.editProfile", [this](const nlohmann::json&) {
        if (m_onMenuAction) {
            m_onMenuAction("edit_profile");
        }
    });

    binding.RegisterHandler("Menu.openExternalLink", [this](const nlohmann::json& data) {
        std::string type = data.value("type", "");
        if (m_onMenuAction) {
            m_onMenuAction("external_" + type);
        }
    });
}

void MainMenu::NavigateTo(MenuState state, TransitionType transition) {
    if (m_transitioning || state == m_currentState) {
        return;
    }

    MenuState previousState = m_currentState;

    // Save history for back navigation
    if (m_currentState != MenuState::None) {
        m_stateHistory.push_back(m_currentState);
    }

    m_targetState = state;
    m_currentTransition = transition;

    // Perform transition
    PerformTransition(previousState, state, transition);

    // Notify callback
    if (m_onStateChange) {
        m_onStateChange(previousState, state);
    }
}

void MainMenu::NavigateBack() {
    if (m_stateHistory.empty() || m_transitioning) {
        return;
    }

    MenuState previousState = m_stateHistory.back();
    m_stateHistory.pop_back();

    m_targetState = previousState;
    m_currentTransition = TransitionType::SlideRight;

    PerformTransition(m_currentState, previousState, TransitionType::SlideRight);

    if (m_onStateChange) {
        m_onStateChange(m_currentState, previousState);
    }
}

void MainMenu::ResetNavigation() {
    m_stateHistory.clear();
}

void MainMenu::PerformTransition(MenuState from, MenuState to, TransitionType type) {
    if (type == TransitionType::None) {
        // Instant transition
        HideStateWindow(from);
        ShowStateWindow(to);
        m_currentState = to;
        return;
    }

    m_transitioning = true;
    m_transitionProgress = 0.0f;

    // Show target window
    ShowStateWindow(to);

    // Execute transition animation via JavaScript
    std::string fromId = GetWindowIdForState(from);
    std::string toId = GetWindowIdForState(to);

    std::string transitionName;
    switch (type) {
        case TransitionType::Fade: transitionName = "fade"; break;
        case TransitionType::SlideLeft: transitionName = "slide-left"; break;
        case TransitionType::SlideRight: transitionName = "slide-right"; break;
        case TransitionType::SlideUp: transitionName = "slide-up"; break;
        case TransitionType::Scale: transitionName = "scale"; break;
        default: transitionName = "fade"; break;
    }

    // Trigger animation in UI
    if (m_stateWindows.count(to) && m_stateWindows[to]) {
        m_uiManager->ExecuteScript(toId,
            "MenuCore.Animations.enterPage(document.body, '" + transitionName + "')");
    }

    if (m_stateWindows.count(from) && m_stateWindows[from]) {
        m_uiManager->ExecuteScript(fromId,
            "MenuCore.Animations.exitPage(document.body, '" + transitionName + "')");
    }
}

void MainMenu::UpdateTransition(float deltaTime) {
    m_transitionProgress += deltaTime / m_transitionDuration;

    if (m_transitionProgress >= 1.0f) {
        // Transition complete
        m_transitioning = false;
        m_transitionProgress = 1.0f;

        HideStateWindow(m_currentState);
        m_currentState = m_targetState;
    }
}

void MainMenu::ShowStateWindow(MenuState state) {
    std::string windowId = GetWindowIdForState(state);

    // Create window if it doesn't exist
    if (m_stateWindows.find(state) == m_stateWindows.end() || !m_stateWindows[state]) {
        auto* window = m_uiManager->CreateWindow(
            windowId,
            GetHtmlPathForState(state),
            Engine::UI::UILayer::Background
        );
        m_stateWindows[state] = window;
    }

    m_uiManager->ShowWindow(windowId);
}

void MainMenu::HideStateWindow(MenuState state) {
    if (state == MenuState::None) return;

    std::string windowId = GetWindowIdForState(state);
    m_uiManager->HideWindow(windowId);
}

std::string MainMenu::GetWindowIdForState(MenuState state) const {
    switch (state) {
        case MenuState::Main: return "main_menu";
        case MenuState::Campaign: return "campaign_menu";
        case MenuState::Multiplayer: return "multiplayer_menu";
        case MenuState::CustomGames: return "custom_games_menu";
        case MenuState::Editor: return "editor_menu";
        case MenuState::Settings: return "settings_menu";
        case MenuState::Credits: return "credits_menu";
        case MenuState::Loading: return "loading_screen";
        default: return "";
    }
}

std::string MainMenu::GetHtmlPathForState(MenuState state) const {
    std::string basePath = "game/assets/ui/html/menu/";
    switch (state) {
        case MenuState::Main: return basePath + "main_menu.html";
        case MenuState::Campaign: return basePath + "campaign_menu.html";
        case MenuState::Multiplayer: return basePath + "multiplayer_menu.html";
        case MenuState::CustomGames: return basePath + "custom_games_menu.html";
        case MenuState::Editor: return basePath + "editor_menu.html";
        case MenuState::Settings: return basePath + "settings_menu.html";
        case MenuState::Credits: return basePath + "credits_menu.html";
        case MenuState::Loading: return basePath + "loading_screen.html";
        default: return "";
    }
}

void MainMenu::Show() {
    m_visible = true;

    if (m_currentState == MenuState::None) {
        m_currentState = MenuState::Main;
    }

    ShowStateWindow(m_currentState);

    if (m_musicPlaylist.size() > 0) {
        PlayMusic();
    }
}

void MainMenu::Hide() {
    m_visible = false;
    HideStateWindow(m_currentState);
    PauseMusic();
}

void MainMenu::SetPlayerProfile(const PlayerProfile& profile) {
    m_playerProfile = profile;

    // Update UI
    if (m_stateWindows.count(MenuState::Main) && m_visible) {
        nlohmann::json profileData = {
            {"name", profile.name},
            {"level", profile.level},
            {"wins", profile.wins},
            {"rank", profile.rank}
        };

        m_uiManager->ExecuteScript("main_menu",
            "if(MainMenu) MainMenu.updatePlayer(" + profileData.dump() + ")");
    }
}

void MainMenu::LoadPlayerProfile() {
    // This would typically load from a backend/save system
    // For now, use default values
    m_playerProfile.name = "Player";
    m_playerProfile.level = 1;
    m_playerProfile.wins = 0;
    m_playerProfile.losses = 0;
    m_playerProfile.rank = "Unranked";
}

void MainMenu::SetNews(const std::vector<NewsItem>& news) {
    m_newsItems = news;
}

void MainMenu::RefreshNews() {
    // Would typically fetch from backend
    // Trigger UI update
    if (m_stateWindows.count(MenuState::Main) && m_visible) {
        m_uiManager->ExecuteScript("main_menu",
            "if(MainMenu) MainMenu.loadNews()");
    }
}

void MainMenu::SetBackgroundLayers(const std::vector<std::string>& layers) {
    m_backgroundLayers = layers;

    // Update parallax in UI
    if (m_stateWindows.count(MenuState::Main) && m_visible) {
        nlohmann::json layersJson = nlohmann::json::array();
        float depth = 0.05f;
        for (const auto& layer : layers) {
            layersJson.push_back({
                {"image", layer},
                {"depth", depth}
            });
            depth += 0.1f;
        }

        m_uiManager->ExecuteScript("main_menu",
            "if(MenuCore && MenuCore.Parallax) MenuCore.Parallax.setLayers(" + layersJson.dump() + ")");
    }
}

void MainMenu::SetMusicPlaylist(const std::vector<MusicTrack>& tracks) {
    m_musicPlaylist = tracks;
    m_currentTrackIndex = 0;
}

void MainMenu::PlayMusic() {
    if (m_musicPlaylist.empty()) return;

    m_musicPlaying = true;

    const auto& track = m_musicPlaylist[m_currentTrackIndex];
    m_uiManager->ExecuteScript("main_menu",
        "if(MenuCore && MenuCore.Audio) MenuCore.Audio.playMusic('" + track.filePath + "')");

    UpdateMusicDisplay();
}

void MainMenu::PauseMusic() {
    m_musicPlaying = false;
    m_uiManager->ExecuteScript("main_menu",
        "if(MenuCore && MenuCore.Audio) MenuCore.Audio.pauseMusic()");
}

void MainMenu::StopMusic() {
    m_musicPlaying = false;
    m_uiManager->ExecuteScript("main_menu",
        "if(MenuCore && MenuCore.Audio) MenuCore.Audio.stopMusic()");
}

void MainMenu::NextTrack() {
    if (m_musicPlaylist.empty()) return;

    m_currentTrackIndex = (m_currentTrackIndex + 1) % m_musicPlaylist.size();

    if (m_musicPlaying) {
        PlayMusic();
    } else {
        UpdateMusicDisplay();
    }
}

void MainMenu::PreviousTrack() {
    if (m_musicPlaylist.empty()) return;

    m_currentTrackIndex = (m_currentTrackIndex - 1 + m_musicPlaylist.size()) % m_musicPlaylist.size();

    if (m_musicPlaying) {
        PlayMusic();
    } else {
        UpdateMusicDisplay();
    }
}

void MainMenu::SetMusicVolume(float volume) {
    m_musicVolume = std::max(0.0f, std::min(1.0f, volume));

    m_uiManager->ExecuteScript("main_menu",
        "if(MenuCore && MenuCore.Audio) MenuCore.Audio.setMusicVolume(" + std::to_string(m_musicVolume) + ")");
}

void MainMenu::UpdateMusicDisplay() {
    if (m_musicPlaylist.empty()) return;

    const auto& track = m_musicPlaylist[m_currentTrackIndex];
    nlohmann::json trackData = {
        {"name", track.name},
        {"artist", track.artist}
    };

    m_uiManager->ExecuteScript("main_menu",
        "if(MainMenu) { document.getElementById('track-name').textContent = '" + track.name + "';"
        "document.getElementById('track-artist').textContent = '" + track.artist + "'; }");
}

void MainMenu::SetGameTitle(const std::string& title, const std::string& subtitle) {
    m_gameTitle = title;
    m_gameSubtitle = subtitle;

    nlohmann::json data = {
        {"title", title},
        {"subtitle", subtitle}
    };

    m_uiManager->ExecuteScript("main_menu",
        "if(MainMenu) MainMenu.setGameInfo(" + data.dump() + ")");
}

void MainMenu::SetGameLogo(const std::string& logoPath) {
    m_gameLogo = logoPath;

    nlohmann::json data = {
        {"logo", logoPath}
    };

    m_uiManager->ExecuteScript("main_menu",
        "if(MainMenu) MainMenu.setGameInfo(" + data.dump() + ")");
}

void MainMenu::SetVersion(const std::string& version) {
    m_gameVersion = version;

    nlohmann::json data = {
        {"version", version}
    };

    m_uiManager->ExecuteScript("main_menu",
        "if(MainMenu) MainMenu.setGameInfo(" + data.dump() + ")");
}

void MainMenu::SetOnMenuAction(std::function<void(const std::string&)> callback) {
    m_onMenuAction = std::move(callback);
}

void MainMenu::SetOnStateChange(std::function<void(MenuState, MenuState)> callback) {
    m_onStateChange = std::move(callback);
}

void MainMenu::SetOnExitRequest(std::function<void()> callback) {
    m_onExitRequest = std::move(callback);
}

void MainMenu::SetOnNewsClick(std::function<void(int)> callback) {
    m_onNewsClick = std::move(callback);
}

} // namespace Menu
} // namespace UI
} // namespace Game
