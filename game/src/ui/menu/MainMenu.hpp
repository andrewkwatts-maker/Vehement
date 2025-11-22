#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Engine { namespace UI { class RuntimeUIManager; class UIWindow; } }

namespace Game {
namespace UI {
namespace Menu {

/**
 * @brief Menu states for the state machine
 */
enum class MenuState {
    Main,
    Campaign,
    Multiplayer,
    CustomGames,
    Editor,
    Settings,
    Credits,
    Loading,
    None
};

/**
 * @brief Player profile data
 */
struct PlayerProfile {
    std::string name;
    std::string avatarPath;
    std::string rankIcon;
    std::string rank;
    int level = 1;
    int wins = 0;
    int losses = 0;
    int experience = 0;
};

/**
 * @brief News item data
 */
struct NewsItem {
    int id;
    std::string title;
    std::string excerpt;
    std::string content;
    std::string date;
    std::string type; // "patch", "event", "update", "maintenance"
    std::string imageUrl;
};

/**
 * @brief Menu transition animation type
 */
enum class TransitionType {
    Fade,
    SlideLeft,
    SlideRight,
    SlideUp,
    Scale,
    None
};

/**
 * @brief Background music track
 */
struct MusicTrack {
    std::string name;
    std::string artist;
    std::string filePath;
    float duration;
};

/**
 * @brief Main Menu Manager
 *
 * Manages the main menu system including state machine, transitions,
 * background rendering, music, and profile data loading.
 */
class MainMenu {
public:
    MainMenu();
    ~MainMenu();

    /**
     * @brief Initialize the menu system
     * @param uiManager Pointer to the UI manager
     * @return true if initialization succeeded
     */
    bool Initialize(Engine::UI::RuntimeUIManager* uiManager);

    /**
     * @brief Shutdown the menu system
     */
    void Shutdown();

    /**
     * @brief Update the menu (call every frame)
     * @param deltaTime Time since last frame
     */
    void Update(float deltaTime);

    // State Management

    /**
     * @brief Get current menu state
     */
    MenuState GetCurrentState() const { return m_currentState; }

    /**
     * @brief Navigate to a menu state
     * @param state Target state
     * @param transition Transition animation type
     */
    void NavigateTo(MenuState state, TransitionType transition = TransitionType::SlideLeft);

    /**
     * @brief Navigate back to previous state
     */
    void NavigateBack();

    /**
     * @brief Check if can go back
     */
    bool CanGoBack() const { return !m_stateHistory.empty(); }

    /**
     * @brief Reset navigation history
     */
    void ResetNavigation();

    // Visibility

    /**
     * @brief Show the main menu
     */
    void Show();

    /**
     * @brief Hide the main menu
     */
    void Hide();

    /**
     * @brief Check if menu is visible
     */
    bool IsVisible() const { return m_visible; }

    // Profile Management

    /**
     * @brief Set player profile data
     */
    void SetPlayerProfile(const PlayerProfile& profile);

    /**
     * @brief Get current player profile
     */
    const PlayerProfile& GetPlayerProfile() const { return m_playerProfile; }

    /**
     * @brief Load player profile from backend
     */
    void LoadPlayerProfile();

    // News Management

    /**
     * @brief Set news items
     */
    void SetNews(const std::vector<NewsItem>& news);

    /**
     * @brief Get news items
     */
    const std::vector<NewsItem>& GetNews() const { return m_newsItems; }

    /**
     * @brief Refresh news from backend
     */
    void RefreshNews();

    // Background & Music

    /**
     * @brief Set background configuration
     * @param layers Vector of layer image paths (back to front)
     */
    void SetBackgroundLayers(const std::vector<std::string>& layers);

    /**
     * @brief Set music playlist
     */
    void SetMusicPlaylist(const std::vector<MusicTrack>& tracks);

    /**
     * @brief Play background music
     */
    void PlayMusic();

    /**
     * @brief Pause background music
     */
    void PauseMusic();

    /**
     * @brief Stop background music
     */
    void StopMusic();

    /**
     * @brief Skip to next track
     */
    void NextTrack();

    /**
     * @brief Skip to previous track
     */
    void PreviousTrack();

    /**
     * @brief Set music volume (0-1)
     */
    void SetMusicVolume(float volume);

    /**
     * @brief Get current music volume
     */
    float GetMusicVolume() const { return m_musicVolume; }

    // Game Info

    /**
     * @brief Set game title
     */
    void SetGameTitle(const std::string& title, const std::string& subtitle = "");

    /**
     * @brief Set game logo image path
     */
    void SetGameLogo(const std::string& logoPath);

    /**
     * @brief Set game version string
     */
    void SetVersion(const std::string& version);

    // Callbacks

    /**
     * @brief Set callback for menu item selection
     */
    void SetOnMenuAction(std::function<void(const std::string&)> callback);

    /**
     * @brief Set callback for state change
     */
    void SetOnStateChange(std::function<void(MenuState, MenuState)> callback);

    /**
     * @brief Set callback for exit game request
     */
    void SetOnExitRequest(std::function<void()> callback);

    /**
     * @brief Set callback for news item click
     */
    void SetOnNewsClick(std::function<void(int)> callback);

private:
    void SetupEventHandlers();
    void UpdateTransition(float deltaTime);
    void PerformTransition(MenuState from, MenuState to, TransitionType type);
    void ShowStateWindow(MenuState state);
    void HideStateWindow(MenuState state);
    std::string GetWindowIdForState(MenuState state) const;
    std::string GetHtmlPathForState(MenuState state) const;
    void UpdateMusicDisplay();
    void OnJavaScriptMessage(const std::string& message, const std::string& data);

    Engine::UI::RuntimeUIManager* m_uiManager = nullptr;
    std::unordered_map<MenuState, Engine::UI::UIWindow*> m_stateWindows;

    MenuState m_currentState = MenuState::None;
    MenuState m_targetState = MenuState::None;
    std::vector<MenuState> m_stateHistory;

    bool m_visible = false;
    bool m_transitioning = false;
    TransitionType m_currentTransition = TransitionType::None;
    float m_transitionProgress = 0.0f;
    float m_transitionDuration = 0.3f;

    PlayerProfile m_playerProfile;
    std::vector<NewsItem> m_newsItems;

    std::vector<std::string> m_backgroundLayers;
    std::vector<MusicTrack> m_musicPlaylist;
    int m_currentTrackIndex = 0;
    bool m_musicPlaying = false;
    float m_musicVolume = 0.7f;

    std::string m_gameTitle;
    std::string m_gameSubtitle;
    std::string m_gameLogo;
    std::string m_gameVersion;

    std::function<void(const std::string&)> m_onMenuAction;
    std::function<void(MenuState, MenuState)> m_onStateChange;
    std::function<void()> m_onExitRequest;
    std::function<void(int)> m_onNewsClick;
};

} // namespace Menu
} // namespace UI
} // namespace Game
