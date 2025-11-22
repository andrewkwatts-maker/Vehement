#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Engine { namespace UI { class RuntimeUIManager; } }

namespace Game {
namespace UI {
namespace Menu {

/**
 * @brief Game status in browser
 */
enum class CustomGameStatus {
    Waiting,
    Starting,
    InProgress,
    Finished
};

/**
 * @brief Custom game listing
 */
struct CustomGameListing {
    std::string id;
    std::string name;
    std::string hostName;
    std::string hostId;
    std::string mapId;
    std::string mapName;
    std::string gameMode;
    int currentPlayers = 0;
    int maxPlayers = 0;
    int ping = 0;
    bool hasPassword = false;
    bool allowSpectators = true;
    CustomGameStatus status = CustomGameStatus::Waiting;
    std::string region;
    std::string createdAt;
};

/**
 * @brief Filter criteria for game browser
 */
struct GameBrowserFilters {
    std::string gameMode;
    std::string mapId;
    std::string mapSize;        // "small", "medium", "large", "massive"
    std::string status;         // "waiting", "starting", ""
    bool hidePasswordProtected = false;
    bool hasOpenSlots = true;
    int maxPing = 500;
    std::string searchText;
};

/**
 * @brief Sort options for game browser
 */
enum class GameSortField {
    Name,
    Host,
    Map,
    Mode,
    Players,
    Ping,
    Status
};

/**
 * @brief Host game settings
 */
struct HostGameSettings {
    std::string name;
    std::string mapId;
    std::string gameMode;
    int maxPlayers = 8;
    std::string password;
    bool allowSpectators = true;
    int gameSpeed = 1;
    std::string region;
};

/**
 * @brief Recent game entry
 */
struct RecentGame {
    std::string name;
    std::string mapName;
    std::string result;     // "Victory", "Defeat", "Draw"
    std::string playedAt;
    int duration;           // In seconds
};

/**
 * @brief Custom Game Browser
 *
 * Manages custom game browsing, filtering, hosting, and joining.
 */
class CustomGameBrowser {
public:
    CustomGameBrowser();
    ~CustomGameBrowser();

    /**
     * @brief Initialize
     */
    bool Initialize(Engine::UI::RuntimeUIManager* uiManager);

    /**
     * @brief Shutdown
     */
    void Shutdown();

    /**
     * @brief Update (call every frame)
     */
    void Update(float deltaTime);

    // Game List

    /**
     * @brief Refresh game list from server
     */
    void RefreshGameList();

    /**
     * @brief Get filtered and sorted game list
     */
    std::vector<CustomGameListing> GetFilteredGames() const;

    /**
     * @brief Get all games
     */
    const std::vector<CustomGameListing>& GetAllGames() const { return m_games; }

    /**
     * @brief Get game by ID
     */
    const CustomGameListing* GetGame(const std::string& gameId) const;

    // Filtering & Sorting

    /**
     * @brief Set filter criteria
     */
    void SetFilters(const GameBrowserFilters& filters);

    /**
     * @brief Get current filters
     */
    const GameBrowserFilters& GetFilters() const { return m_filters; }

    /**
     * @brief Clear all filters
     */
    void ClearFilters();

    /**
     * @brief Set sort field
     */
    void SetSortField(GameSortField field, bool ascending = true);

    /**
     * @brief Get current sort field
     */
    GameSortField GetSortField() const { return m_sortField; }

    /**
     * @brief Check if sorting ascending
     */
    bool IsSortAscending() const { return m_sortAscending; }

    // Selection

    /**
     * @brief Select a game
     */
    void SelectGame(const std::string& gameId);

    /**
     * @brief Get selected game
     */
    const CustomGameListing* GetSelectedGame() const;

    /**
     * @brief Clear selection
     */
    void ClearSelection();

    // Actions

    /**
     * @brief Join selected game
     */
    void JoinGame(const std::string& password = "");

    /**
     * @brief Join game by ID
     */
    void JoinGameById(const std::string& gameId, const std::string& password = "");

    /**
     * @brief Spectate selected game
     */
    void SpectateGame();

    /**
     * @brief Host a new game
     */
    void HostGame(const HostGameSettings& settings);

    // Recent Games

    /**
     * @brief Get recent games
     */
    const std::vector<RecentGame>& GetRecentGames() const { return m_recentGames; }

    /**
     * @brief Clear recent games history
     */
    void ClearRecentGames();

    // Maps

    /**
     * @brief Get available maps for hosting
     */
    struct MapInfo {
        std::string id;
        std::string name;
        std::string size;
        int minPlayers;
        int maxPlayers;
        std::string previewImage;
    };
    const std::vector<MapInfo>& GetAvailableMaps() const { return m_availableMaps; }

    // Callbacks

    void SetOnGameListUpdate(std::function<void(const std::vector<CustomGameListing>&)> callback);
    void SetOnGameSelect(std::function<void(const CustomGameListing*)> callback);
    void SetOnJoinGame(std::function<void(const std::string&)> callback);
    void SetOnHostGame(std::function<void(const HostGameSettings&)> callback);
    void SetOnError(std::function<void(const std::string&)> callback);

private:
    void SetupEventHandlers();
    void SortGames();
    bool MatchesFilters(const CustomGameListing& game) const;
    void UpdateUI();
    void LoadRecentGames();
    void SaveRecentGames();

    Engine::UI::RuntimeUIManager* m_uiManager = nullptr;

    std::vector<CustomGameListing> m_games;
    std::vector<RecentGame> m_recentGames;
    std::vector<MapInfo> m_availableMaps;

    GameBrowserFilters m_filters;
    GameSortField m_sortField = GameSortField::Players;
    bool m_sortAscending = false;

    std::string m_selectedGameId;

    float m_refreshTimer = 0.0f;
    float m_autoRefreshInterval = 10.0f;
    bool m_autoRefresh = true;

    std::function<void(const std::vector<CustomGameListing>&)> m_onGameListUpdate;
    std::function<void(const CustomGameListing*)> m_onGameSelect;
    std::function<void(const std::string&)> m_onJoinGame;
    std::function<void(const HostGameSettings&)> m_onHostGame;
    std::function<void(const std::string&)> m_onError;
};

} // namespace Menu
} // namespace UI
} // namespace Game
