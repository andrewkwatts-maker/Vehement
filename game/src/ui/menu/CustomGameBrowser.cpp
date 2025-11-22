#include "CustomGameBrowser.hpp"
#include "engine/ui/runtime/RuntimeUIManager.hpp"
#include "engine/ui/runtime/UIBinding.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>

namespace Game {
namespace UI {
namespace Menu {

CustomGameBrowser::CustomGameBrowser() = default;
CustomGameBrowser::~CustomGameBrowser() = default;

bool CustomGameBrowser::Initialize(Engine::UI::RuntimeUIManager* uiManager) {
    if (!uiManager) return false;

    m_uiManager = uiManager;
    SetupEventHandlers();

    // Initialize default maps
    m_availableMaps = {
        {"contested_valley", "Contested Valley", "medium", 2, 6, ""},
        {"twin_rivers", "Twin Rivers", "medium", 2, 4, ""},
        {"highland_fortress", "Highland Fortress", "large", 4, 8, ""},
        {"desert_storm", "Desert Storm", "large", 2, 8, ""},
        {"frozen_throne", "Frozen Throne", "small", 2, 4, ""},
        {"random", "Random", "any", 2, 8, ""}
    };

    LoadRecentGames();

    return true;
}

void CustomGameBrowser::Shutdown() {
    SaveRecentGames();
    m_games.clear();
    m_uiManager = nullptr;
}

void CustomGameBrowser::Update(float deltaTime) {
    if (m_autoRefresh) {
        m_refreshTimer += deltaTime;
        if (m_refreshTimer >= m_autoRefreshInterval) {
            m_refreshTimer = 0.0f;
            RefreshGameList();
        }
    }
}

void CustomGameBrowser::SetupEventHandlers() {
    if (!m_uiManager) return;

    auto& binding = m_uiManager->GetBinding();

    binding.RegisterHandler("CustomGames.getList", [this](const nlohmann::json& data) -> nlohmann::json {
        GameBrowserFilters filters;
        filters.gameMode = data.value("mode", "");
        filters.mapId = data.value("map", "");
        filters.status = data.value("status", "");
        filters.hidePasswordProtected = data.value("hidePassword", false);
        filters.hasOpenSlots = data.value("hasSlots", true);
        filters.maxPing = data.value("maxPing", 500);

        SetFilters(filters);

        nlohmann::json result = nlohmann::json::array();
        for (const auto& game : GetFilteredGames()) {
            result.push_back({
                {"id", game.id},
                {"name", game.name},
                {"host", game.hostName},
                {"map", game.mapName},
                {"mode", game.gameMode},
                {"players", std::to_string(game.currentPlayers) + "/" + std::to_string(game.maxPlayers)},
                {"ping", game.ping},
                {"hasPassword", game.hasPassword},
                {"status", game.status == CustomGameStatus::Waiting ? "waiting" : "starting"}
            });
        }
        return result;
    });

    binding.RegisterHandler("CustomGames.select", [this](const nlohmann::json& data) {
        SelectGame(data.value("gameId", ""));
    });

    binding.RegisterHandler("CustomGames.join", [this](const nlohmann::json& data) {
        std::string gameId = data.value("gameId", m_selectedGameId);
        std::string password = data.value("password", "");
        JoinGameById(gameId, password);
    });

    binding.RegisterHandler("CustomGames.spectate", [this](const nlohmann::json& data) {
        std::string gameId = data.value("gameId", m_selectedGameId);
        SelectGame(gameId);
        SpectateGame();
    });

    binding.RegisterHandler("CustomGames.create", [this](const nlohmann::json& data) {
        HostGameSettings settings;
        settings.name = data.value("name", "New Game");
        settings.mapId = data.value("map", "random");
        settings.gameMode = data.value("mode", "melee");
        settings.maxPlayers = data.value("maxPlayers", 8);
        settings.password = data.value("password", "");
        settings.allowSpectators = data.value("allowSpectators", true);

        HostGame(settings);
    });

    binding.RegisterHandler("CustomGames.refresh", [this](const nlohmann::json&) {
        RefreshGameList();
    });

    binding.RegisterHandler("CustomGames.getMaps", [this](const nlohmann::json&) -> nlohmann::json {
        nlohmann::json result = nlohmann::json::array();
        for (const auto& map : m_availableMaps) {
            result.push_back({
                {"id", map.id},
                {"name", map.name},
                {"size", map.size},
                {"minPlayers", map.minPlayers},
                {"maxPlayers", map.maxPlayers}
            });
        }
        return result;
    });

    binding.RegisterHandler("CustomGames.getRecent", [this](const nlohmann::json&) -> nlohmann::json {
        nlohmann::json result = nlohmann::json::array();
        for (const auto& game : m_recentGames) {
            result.push_back({
                {"name", game.name},
                {"map", game.mapName},
                {"result", game.result},
                {"playedAt", game.playedAt},
                {"duration", game.duration}
            });
        }
        return result;
    });
}

void CustomGameBrowser::RefreshGameList() {
    // In real implementation, this would query the server
    // For now, generate mock data

    m_games.clear();

    // Mock games
    m_games = {
        {"game_1", "Pros Only 1v1", "Champion", "user_1", "contested_valley", "Contested Valley",
         "Melee", 1, 2, 45, false, true, CustomGameStatus::Waiting, "na", ""},
        {"game_2", "Casual Fun Game", "FriendlyGuy", "user_2", "twin_rivers", "Twin Rivers",
         "Melee", 3, 6, 78, false, true, CustomGameStatus::Waiting, "eu", ""},
        {"game_3", "Co-op Nightmare", "TeamPlayer", "user_3", "highland_fortress", "Highland Fortress",
         "Co-op vs AI", 2, 4, 120, true, false, CustomGameStatus::Waiting, "na", ""},
        {"game_4", "Survival Challenge", "Survivor99", "user_4", "desert_storm", "Desert Storm",
         "Survival", 5, 8, 95, false, true, CustomGameStatus::Starting, "eu", ""},
        {"game_5", "Tower Defense Masters", "Builder", "user_5", "frozen_throne", "Frozen Throne",
         "Tower Defense", 4, 4, 32, false, true, CustomGameStatus::Starting, "na", ""}
    };

    SortGames();

    if (m_onGameListUpdate) {
        m_onGameListUpdate(GetFilteredGames());
    }

    UpdateUI();
}

std::vector<CustomGameListing> CustomGameBrowser::GetFilteredGames() const {
    std::vector<CustomGameListing> filtered;

    for (const auto& game : m_games) {
        if (MatchesFilters(game)) {
            filtered.push_back(game);
        }
    }

    return filtered;
}

const CustomGameListing* CustomGameBrowser::GetGame(const std::string& gameId) const {
    auto it = std::find_if(m_games.begin(), m_games.end(),
        [&gameId](const CustomGameListing& g) { return g.id == gameId; });

    return it != m_games.end() ? &(*it) : nullptr;
}

void CustomGameBrowser::SetFilters(const GameBrowserFilters& filters) {
    m_filters = filters;
    UpdateUI();
}

void CustomGameBrowser::ClearFilters() {
    m_filters = GameBrowserFilters();
    UpdateUI();
}

void CustomGameBrowser::SetSortField(GameSortField field, bool ascending) {
    if (m_sortField == field) {
        m_sortAscending = !m_sortAscending;
    } else {
        m_sortField = field;
        m_sortAscending = ascending;
    }

    SortGames();
    UpdateUI();
}

void CustomGameBrowser::SortGames() {
    std::sort(m_games.begin(), m_games.end(),
        [this](const CustomGameListing& a, const CustomGameListing& b) {
            int result = 0;

            switch (m_sortField) {
                case GameSortField::Name:
                    result = a.name.compare(b.name);
                    break;
                case GameSortField::Host:
                    result = a.hostName.compare(b.hostName);
                    break;
                case GameSortField::Map:
                    result = a.mapName.compare(b.mapName);
                    break;
                case GameSortField::Mode:
                    result = a.gameMode.compare(b.gameMode);
                    break;
                case GameSortField::Players:
                    result = (a.maxPlayers - a.currentPlayers) - (b.maxPlayers - b.currentPlayers);
                    break;
                case GameSortField::Ping:
                    result = a.ping - b.ping;
                    break;
                case GameSortField::Status:
                    result = static_cast<int>(a.status) - static_cast<int>(b.status);
                    break;
            }

            return m_sortAscending ? (result < 0) : (result > 0);
        }
    );
}

bool CustomGameBrowser::MatchesFilters(const CustomGameListing& game) const {
    // Game mode filter
    if (!m_filters.gameMode.empty() &&
        game.gameMode.find(m_filters.gameMode) == std::string::npos) {
        return false;
    }

    // Map filter
    if (!m_filters.mapId.empty() && game.mapId != m_filters.mapId) {
        return false;
    }

    // Status filter
    if (!m_filters.status.empty()) {
        if (m_filters.status == "waiting" && game.status != CustomGameStatus::Waiting) {
            return false;
        }
        if (m_filters.status == "starting" && game.status != CustomGameStatus::Starting) {
            return false;
        }
    }

    // Password filter
    if (m_filters.hidePasswordProtected && game.hasPassword) {
        return false;
    }

    // Open slots filter
    if (m_filters.hasOpenSlots && game.currentPlayers >= game.maxPlayers) {
        return false;
    }

    // Ping filter
    if (game.ping > m_filters.maxPing) {
        return false;
    }

    // Search text filter
    if (!m_filters.searchText.empty()) {
        std::string searchLower = m_filters.searchText;
        std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);

        std::string nameLower = game.name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);

        std::string hostLower = game.hostName;
        std::transform(hostLower.begin(), hostLower.end(), hostLower.begin(), ::tolower);

        if (nameLower.find(searchLower) == std::string::npos &&
            hostLower.find(searchLower) == std::string::npos) {
            return false;
        }
    }

    return true;
}

void CustomGameBrowser::SelectGame(const std::string& gameId) {
    m_selectedGameId = gameId;

    const CustomGameListing* game = GetGame(gameId);
    if (m_onGameSelect) {
        m_onGameSelect(game);
    }
}

const CustomGameListing* CustomGameBrowser::GetSelectedGame() const {
    return GetGame(m_selectedGameId);
}

void CustomGameBrowser::ClearSelection() {
    m_selectedGameId = "";
}

void CustomGameBrowser::JoinGame(const std::string& password) {
    JoinGameById(m_selectedGameId, password);
}

void CustomGameBrowser::JoinGameById(const std::string& gameId, const std::string& password) {
    const CustomGameListing* game = GetGame(gameId);
    if (!game) {
        if (m_onError) m_onError("Game not found");
        return;
    }

    if (game->currentPlayers >= game->maxPlayers) {
        if (m_onError) m_onError("Game is full");
        return;
    }

    if (game->hasPassword && password.empty()) {
        if (m_onError) m_onError("Password required");
        return;
    }

    if (m_onJoinGame) {
        m_onJoinGame(gameId);
    }
}

void CustomGameBrowser::SpectateGame() {
    const CustomGameListing* game = GetSelectedGame();
    if (!game) {
        if (m_onError) m_onError("No game selected");
        return;
    }

    if (!game->allowSpectators) {
        if (m_onError) m_onError("Spectating not allowed");
        return;
    }

    // Would initiate spectate connection
}

void CustomGameBrowser::HostGame(const HostGameSettings& settings) {
    if (settings.name.empty()) {
        if (m_onError) m_onError("Game name required");
        return;
    }

    if (m_onHostGame) {
        m_onHostGame(settings);
    }
}

void CustomGameBrowser::ClearRecentGames() {
    m_recentGames.clear();
    SaveRecentGames();
}

void CustomGameBrowser::LoadRecentGames() {
    std::ifstream file("saves/recent_games.json");
    if (!file.is_open()) return;

    try {
        nlohmann::json data;
        file >> data;
        file.close();

        m_recentGames.clear();
        for (const auto& item : data) {
            RecentGame game;
            game.name = item.value("name", "");
            game.mapName = item.value("map", "");
            game.result = item.value("result", "");
            game.playedAt = item.value("playedAt", "");
            game.duration = item.value("duration", 0);
            m_recentGames.push_back(game);
        }
    } catch (...) {
        // Ignore parse errors
    }
}

void CustomGameBrowser::SaveRecentGames() {
    nlohmann::json data = nlohmann::json::array();
    for (const auto& game : m_recentGames) {
        data.push_back({
            {"name", game.name},
            {"map", game.mapName},
            {"result", game.result},
            {"playedAt", game.playedAt},
            {"duration", game.duration}
        });
    }

    std::ofstream file("saves/recent_games.json");
    if (file.is_open()) {
        file << data.dump(2);
        file.close();
    }
}

void CustomGameBrowser::UpdateUI() {
    if (!m_uiManager) return;

    auto filtered = GetFilteredGames();

    nlohmann::json gamesJson = nlohmann::json::array();
    for (const auto& game : filtered) {
        gamesJson.push_back({
            {"id", game.id},
            {"name", game.name},
            {"host", game.hostName},
            {"map", game.mapName},
            {"mode", game.gameMode},
            {"players", std::to_string(game.currentPlayers) + "/" + std::to_string(game.maxPlayers)},
            {"ping", game.ping},
            {"hasPassword", game.hasPassword},
            {"status", game.status == CustomGameStatus::Waiting ? "waiting" : "starting"}
        });
    }

    m_uiManager->ExecuteScript("custom_games_menu",
        "if(CustomGamesMenu) { CustomGamesMenu._games = " + gamesJson.dump() + "; CustomGamesMenu.renderGamesList(); }");
}

void CustomGameBrowser::SetOnGameListUpdate(std::function<void(const std::vector<CustomGameListing>&)> callback) {
    m_onGameListUpdate = std::move(callback);
}

void CustomGameBrowser::SetOnGameSelect(std::function<void(const CustomGameListing*)> callback) {
    m_onGameSelect = std::move(callback);
}

void CustomGameBrowser::SetOnJoinGame(std::function<void(const std::string&)> callback) {
    m_onJoinGame = std::move(callback);
}

void CustomGameBrowser::SetOnHostGame(std::function<void(const HostGameSettings&)> callback) {
    m_onHostGame = std::move(callback);
}

void CustomGameBrowser::SetOnError(std::function<void(const std::string&)> callback) {
    m_onError = std::move(callback);
}

} // namespace Menu
} // namespace UI
} // namespace Game
