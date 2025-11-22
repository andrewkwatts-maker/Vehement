#include "MultiplayerLobby.hpp"
#include "engine/ui/runtime/RuntimeUIManager.hpp"
#include "engine/ui/runtime/UIBinding.hpp"
#include <nlohmann/json.hpp>

namespace Game {
namespace UI {
namespace Menu {

MultiplayerLobby::MultiplayerLobby() = default;
MultiplayerLobby::~MultiplayerLobby() = default;

bool MultiplayerLobby::Initialize(Engine::UI::RuntimeUIManager* uiManager) {
    if (!uiManager) return false;

    m_uiManager = uiManager;
    SetupEventHandlers();

    return true;
}

void MultiplayerLobby::Shutdown() {
    if (m_inQueue) CancelQueue();
    if (m_inLobby) LeaveLobby();

    m_uiManager = nullptr;
}

void MultiplayerLobby::Update(float deltaTime) {
    if (m_inQueue) {
        m_queueTime += deltaTime;
        UpdateQueueStatus();
    }
}

void MultiplayerLobby::SetupEventHandlers() {
    if (!m_uiManager) return;

    auto& binding = m_uiManager->GetBinding();

    binding.RegisterHandler("Multiplayer.findMatch", [this](const nlohmann::json& data) {
        std::string typeStr = data.value("type", "ranked");
        std::string modeStr = data.value("mode", "1v1");
        std::string regionStr = data.value("region", "auto");

        MatchType type = typeStr == "unranked" ? MatchType::Unranked : MatchType::Ranked;

        GameMode mode = GameMode::OneVsOne;
        if (modeStr == "2v2") mode = GameMode::TwoVsTwo;
        else if (modeStr == "3v3") mode = GameMode::ThreeVsThree;
        else if (modeStr == "4v4") mode = GameMode::FourVsFour;
        else if (modeStr == "ffa") mode = GameMode::FreeForAll;

        ServerRegion region = ServerRegion::Auto;
        if (regionStr == "na") region = ServerRegion::NorthAmerica;
        else if (regionStr == "eu") region = ServerRegion::Europe;
        else if (regionStr == "asia") region = ServerRegion::AsiaPacific;
        else if (regionStr == "sa") region = ServerRegion::SouthAmerica;
        else if (regionStr == "oce") region = ServerRegion::Oceania;

        FindMatch(type, mode, region);
    });

    binding.RegisterHandler("Multiplayer.cancelQueue", [this](const nlohmann::json&) {
        CancelQueue();
    });

    binding.RegisterHandler("Multiplayer.matchResponse", [this](const nlohmann::json& data) {
        bool accept = data.value("accept", false);
        if (accept) AcceptMatch();
        else DeclineMatch();
    });

    binding.RegisterHandler("Multiplayer.createLobby", [this](const nlohmann::json& data) {
        LobbySettings settings;
        settings.name = data.value("name", "New Lobby");
        settings.password = data.value("password", "");
        settings.mapId = data.value("map", "random");
        settings.maxPlayers = data.value("maxPlayers", 2);
        settings.isPublic = settings.password.empty();

        CreateLobby(settings);
    });

    binding.RegisterHandler("Multiplayer.joinLobby", [this](const nlohmann::json& data) {
        std::string code = data.value("code", "");
        if (!code.empty()) {
            JoinLobbyWithCode(code);
        }
    });

    binding.RegisterHandler("Multiplayer.leaveLobby", [this](const nlohmann::json&) {
        LeaveLobby();
    });

    binding.RegisterHandler("Multiplayer.setReady", [this](const nlohmann::json& data) {
        SetReady(data.value("ready", false));
    });

    binding.RegisterHandler("Multiplayer.startGame", [this](const nlohmann::json&) {
        StartGame();
    });

    binding.RegisterHandler("Multiplayer.addAI", [this](const nlohmann::json& data) {
        AddAI(data.value("team", 1), data.value("slot", 0), data.value("difficulty", 2));
    });

    binding.RegisterHandler("Multiplayer.inviteFriend", [this](const nlohmann::json& data) {
        InviteFriend(data.value("friendId", ""));
    });

    binding.RegisterHandler("Multiplayer.getStats", [this](const nlohmann::json&) -> nlohmann::json {
        return {
            {"rank", m_playerStats.rank},
            {"mmr", m_playerStats.mmr},
            {"wins", m_playerStats.wins},
            {"losses", m_playerStats.losses},
            {"winrate", m_playerStats.winrate}
        };
    });

    binding.RegisterHandler("Social.getFriends", [this](const nlohmann::json&) -> nlohmann::json {
        nlohmann::json friendsArray = nlohmann::json::array();
        for (const auto& f : m_friends) {
            friendsArray.push_back({
                {"id", f.id},
                {"name", f.name},
                {"avatar", f.avatarUrl},
                {"status", f.status},
                {"online", f.online}
            });
        }
        return friendsArray;
    });

    binding.RegisterHandler("Chat.send", [this](const nlohmann::json& data) {
        SendChatMessage(data.value("message", ""), data.value("channel", "global"));
    });

    binding.RegisterHandler("Network.getPing", [this](const nlohmann::json&) -> nlohmann::json {
        return m_currentPing;
    });
}

void MultiplayerLobby::FindMatch(MatchType type, GameMode mode, ServerRegion region) {
    if (m_inQueue) return;

    m_inQueue = true;
    m_queueTime = 0.0f;
    m_queueMatchType = type;
    m_queueGameMode = mode;
    m_selectedRegion = region;
    m_matchFound = false;

    // Would send to matchmaking server
    // For now, simulate
}

void MultiplayerLobby::CancelQueue() {
    m_inQueue = false;
    m_queueTime = 0.0f;
    m_matchFound = false;
}

void MultiplayerLobby::AcceptMatch() {
    if (!m_matchFound) return;

    m_matchFound = false;
    m_inQueue = false;

    // Transition to game loading
}

void MultiplayerLobby::DeclineMatch() {
    m_matchFound = false;
    CancelQueue();
}

void MultiplayerLobby::UpdateQueueStatus() {
    // Update UI with queue time
    if (!m_uiManager) return;

    int seconds = static_cast<int>(m_queueTime);
    int minutes = seconds / 60;
    seconds = seconds % 60;

    std::string timeStr = std::to_string(minutes) + ":" + (seconds < 10 ? "0" : "") + std::to_string(seconds);

    m_uiManager->ExecuteScript("multiplayer_menu",
        "if(document.getElementById('queue-time')) document.getElementById('queue-time').textContent = '" + timeStr + "'");
}

void MultiplayerLobby::CreateLobby(const LobbySettings& settings) {
    m_currentLobby = Lobby();
    m_currentLobby.id = "lobby_" + std::to_string(rand());
    m_currentLobby.code = "ABCD-1234"; // Would be generated by server
    m_currentLobby.settings = settings;
    m_currentLobby.hostId = m_localPlayerId;

    // Add self as first player
    LobbyPlayer self;
    self.id = m_localPlayerId;
    self.name = "You"; // Would be actual player name
    self.team = 1;
    self.slot = 0;
    self.isHost = true;
    m_currentLobby.players.push_back(self);

    m_inLobby = true;
    m_isHost = true;
    m_isReady = false;

    UpdateLobbyUI();

    if (m_onLobbyUpdate) {
        m_onLobbyUpdate(m_currentLobby);
    }
}

void MultiplayerLobby::JoinLobbyWithCode(const std::string& code) {
    // Would send to server to find and join lobby
    // For now, create a mock lobby

    m_currentLobby = Lobby();
    m_currentLobby.id = "lobby_joined";
    m_currentLobby.code = code;
    m_currentLobby.settings.name = "Friend's Lobby";

    m_inLobby = true;
    m_isHost = false;
    m_isReady = false;

    UpdateLobbyUI();
}

void MultiplayerLobby::JoinLobby(const std::string& lobbyId) {
    // Similar to JoinLobbyWithCode but by ID
}

void MultiplayerLobby::LeaveLobby() {
    m_inLobby = false;
    m_isHost = false;
    m_isReady = false;
    m_currentLobby = Lobby();
}

void MultiplayerLobby::UpdateLobbySettings(const LobbySettings& settings) {
    if (!m_isHost) return;

    m_currentLobby.settings = settings;
    UpdateLobbyUI();

    if (m_onLobbyUpdate) {
        m_onLobbyUpdate(m_currentLobby);
    }
}

void MultiplayerLobby::KickPlayer(const std::string& playerId) {
    if (!m_isHost) return;

    auto& players = m_currentLobby.players;
    players.erase(
        std::remove_if(players.begin(), players.end(),
            [&playerId](const LobbyPlayer& p) { return p.id == playerId; }),
        players.end()
    );

    UpdateLobbyUI();
}

void MultiplayerLobby::AddAI(int team, int slot, int difficulty) {
    if (!m_isHost) return;

    LobbyPlayer ai;
    ai.id = "ai_" + std::to_string(slot);
    ai.name = "AI (" + std::to_string(difficulty) + ")";
    ai.team = team;
    ai.slot = slot;
    ai.isAI = true;
    ai.aiDifficulty = difficulty;
    ai.readyState = PlayerReadyState::Ready;

    m_currentLobby.players.push_back(ai);
    UpdateLobbyUI();
}

void MultiplayerLobby::RemoveAI(int slot) {
    if (!m_isHost) return;

    auto& players = m_currentLobby.players;
    players.erase(
        std::remove_if(players.begin(), players.end(),
            [slot](const LobbyPlayer& p) { return p.isAI && p.slot == slot; }),
        players.end()
    );

    UpdateLobbyUI();
}

void MultiplayerLobby::StartGame() {
    if (!m_isHost) return;

    // Check all players ready
    bool allReady = true;
    for (const auto& player : m_currentLobby.players) {
        if (!player.isAI && player.readyState != PlayerReadyState::Ready && player.id != m_localPlayerId) {
            allReady = false;
            break;
        }
    }

    if (!allReady) {
        if (m_onError) m_onError("Not all players are ready");
        return;
    }

    m_currentLobby.gameStarting = true;

    if (m_onGameStart) {
        m_onGameStart(m_currentLobby);
    }
}

void MultiplayerLobby::SetReady(bool ready) {
    m_isReady = ready;

    // Update own player in lobby
    for (auto& player : m_currentLobby.players) {
        if (player.id == m_localPlayerId) {
            player.readyState = ready ? PlayerReadyState::Ready : PlayerReadyState::NotReady;
            break;
        }
    }

    UpdateLobbyUI();
}

void MultiplayerLobby::ToggleReady() {
    SetReady(!m_isReady);
}

void MultiplayerLobby::ChangeTeam(int team) {
    for (auto& player : m_currentLobby.players) {
        if (player.id == m_localPlayerId) {
            player.team = team;
            break;
        }
    }
    UpdateLobbyUI();
}

void MultiplayerLobby::ChangeRace(const std::string& raceId) {
    for (auto& player : m_currentLobby.players) {
        if (player.id == m_localPlayerId) {
            player.raceId = raceId;
            break;
        }
    }
    UpdateLobbyUI();
}

void MultiplayerLobby::ChangeColor(const std::string& color) {
    for (auto& player : m_currentLobby.players) {
        if (player.id == m_localPlayerId) {
            player.color = color;
            break;
        }
    }
    UpdateLobbyUI();
}

void MultiplayerLobby::InviteFriend(const std::string& friendId) {
    // Would send invite through server
}

void MultiplayerLobby::RefreshFriends() {
    // Would fetch from server
}

void MultiplayerLobby::SendChatMessage(const std::string& message, const std::string& channel) {
    if (message.empty()) return;

    ChatMessage msg;
    msg.sender = "You"; // Would be actual player name
    msg.text = message;
    msg.channel = channel;
    msg.isSystem = false;

    // Add timestamp
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%H:%M", std::localtime(&time));
    msg.timestamp = buf;

    m_chatHistory.push_back(msg);

    // Trim history if too long
    while (m_chatHistory.size() > m_maxChatHistory) {
        m_chatHistory.erase(m_chatHistory.begin());
    }

    if (m_onChatMessage) {
        m_onChatMessage(msg);
    }
}

void MultiplayerLobby::SetRegion(ServerRegion region) {
    m_selectedRegion = region;
}

void MultiplayerLobby::UpdateLobbyUI() {
    if (!m_uiManager || !m_inLobby) return;

    nlohmann::json lobbyData = {
        {"name", m_currentLobby.settings.name},
        {"code", m_currentLobby.code},
        {"players", nlohmann::json::array()}
    };

    for (const auto& player : m_currentLobby.players) {
        lobbyData["players"].push_back({
            {"id", player.id},
            {"name", player.name},
            {"team", player.team},
            {"slot", player.slot},
            {"ready", player.readyState == PlayerReadyState::Ready},
            {"isAI", player.isAI}
        });
    }

    m_uiManager->ExecuteScript("multiplayer_menu",
        "if(MultiplayerMenu) MultiplayerMenu.updateLobbyPlayers(" + lobbyData["players"].dump() + ")");
}

std::string MultiplayerLobby::GameModeToString(GameMode mode) const {
    switch (mode) {
        case GameMode::OneVsOne: return "1v1";
        case GameMode::TwoVsTwo: return "2v2";
        case GameMode::ThreeVsThree: return "3v3";
        case GameMode::FourVsFour: return "4v4";
        case GameMode::FreeForAll: return "FFA";
        default: return "1v1";
    }
}

std::string MultiplayerLobby::RegionToString(ServerRegion region) const {
    switch (region) {
        case ServerRegion::Auto: return "auto";
        case ServerRegion::NorthAmerica: return "na";
        case ServerRegion::Europe: return "eu";
        case ServerRegion::AsiaPacific: return "asia";
        case ServerRegion::SouthAmerica: return "sa";
        case ServerRegion::Oceania: return "oce";
        default: return "auto";
    }
}

void MultiplayerLobby::SetOnMatchFound(std::function<void(const std::string&, const std::string&)> callback) {
    m_onMatchFound = std::move(callback);
}

void MultiplayerLobby::SetOnLobbyUpdate(std::function<void(const Lobby&)> callback) {
    m_onLobbyUpdate = std::move(callback);
}

void MultiplayerLobby::SetOnGameStart(std::function<void(const Lobby&)> callback) {
    m_onGameStart = std::move(callback);
}

void MultiplayerLobby::SetOnChatMessage(std::function<void(const ChatMessage&)> callback) {
    m_onChatMessage = std::move(callback);
}

void MultiplayerLobby::SetOnError(std::function<void(const std::string&)> callback) {
    m_onError = std::move(callback);
}

} // namespace Menu
} // namespace UI
} // namespace Game
