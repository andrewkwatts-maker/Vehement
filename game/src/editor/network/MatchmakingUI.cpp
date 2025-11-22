#include "MatchmakingUI.hpp"
#include "../../network/firebase/FirebaseMatchmaking.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace Editor {
namespace Network {

MatchmakingUI& MatchmakingUI::getInstance() {
    static MatchmakingUI instance;
    return instance;
}

MatchmakingUI::MatchmakingUI() {}

MatchmakingUI::~MatchmakingUI() {}

void MatchmakingUI::update(float deltaTime) {
    if (!m_visible) return;

    // Update match found timer
    if (m_showMatchFound) {
        m_matchFoundTimer += deltaTime;
        if (m_matchFoundTimer >= m_matchFoundTimeout) {
            declineMatch();
        }
    }

    // Update ready check timer
    if (m_showReadyCheck) {
        m_readyCheckTimer += deltaTime;
        if (m_readyCheckTimer >= m_readyCheckTimeout) {
            hideReadyCheck();
        }
    }
}

void MatchmakingUI::render() {
    if (!m_visible) return;
    // Integrate with your rendering system
}

std::string MatchmakingUI::renderHTML() {
    std::ostringstream html;

    html << "<!DOCTYPE html>\n<html>\n<head>\n";
    html << "<title>Matchmaking</title>\n";
    html << "<style>\n";
    html << "* { box-sizing: border-box; margin: 0; padding: 0; }\n";
    html << "body { font-family: 'Segoe UI', Arial, sans-serif; background: linear-gradient(135deg, #1a1a2e, #16213e); color: #fff; min-height: 100vh; }\n";
    html << ".container { max-width: 800px; margin: 0 auto; padding: 20px; }\n";
    html << ".panel { background: rgba(255,255,255,0.05); border-radius: 12px; padding: 20px; margin: 15px 0; backdrop-filter: blur(10px); }\n";
    html << ".btn { padding: 12px 24px; border: none; border-radius: 8px; cursor: pointer; font-size: 16px; transition: all 0.3s; }\n";
    html << ".btn-primary { background: linear-gradient(135deg, #4ecca3, #45b393); color: #fff; }\n";
    html << ".btn-primary:hover { transform: translateY(-2px); box-shadow: 0 4px 15px rgba(78, 204, 163, 0.4); }\n";
    html << ".btn-secondary { background: rgba(255,255,255,0.1); color: #fff; }\n";
    html << ".btn-danger { background: #f38181; color: #fff; }\n";
    html << ".player-list { list-style: none; }\n";
    html << ".player-item { display: flex; align-items: center; padding: 12px; border-radius: 8px; margin: 8px 0; background: rgba(0,0,0,0.2); }\n";
    html << ".player-name { flex: 1; font-weight: 500; }\n";
    html << ".player-ready { width: 20px; height: 20px; border-radius: 50%; background: #f38181; }\n";
    html << ".player-ready.ready { background: #4ecca3; }\n";
    html << ".lobby-item { padding: 15px; border-radius: 8px; margin: 10px 0; background: rgba(0,0,0,0.2); cursor: pointer; transition: all 0.3s; }\n";
    html << ".lobby-item:hover { background: rgba(78, 204, 163, 0.2); }\n";
    html << ".search-spinner { width: 60px; height: 60px; border: 4px solid rgba(78, 204, 163, 0.2); border-top-color: #4ecca3; border-radius: 50%; animation: spin 1s linear infinite; margin: 20px auto; }\n";
    html << "@keyframes spin { to { transform: rotate(360deg); } }\n";
    html << ".progress-bar { height: 8px; background: rgba(255,255,255,0.1); border-radius: 4px; overflow: hidden; }\n";
    html << ".progress-fill { height: 100%; background: linear-gradient(90deg, #4ecca3, #45b393); transition: width 0.3s; }\n";
    html << ".modal { position: fixed; top: 0; left: 0; right: 0; bottom: 0; background: rgba(0,0,0,0.7); display: flex; align-items: center; justify-content: center; }\n";
    html << ".modal-content { background: #16213e; padding: 30px; border-radius: 16px; text-align: center; max-width: 400px; }\n";
    html << ".countdown { font-size: 48px; color: #4ecca3; margin: 20px 0; }\n";
    html << "h1 { font-size: 28px; margin-bottom: 20px; }\n";
    html << "h2 { font-size: 20px; margin-bottom: 15px; color: #4ecca3; }\n";
    html << "</style>\n</head>\n<body>\n";
    html << "<div class='container'>\n";

    switch (m_state) {
        case MatchmakingUIState::Idle:
            html << "<div class='panel'>\n";
            html << "<h1>Matchmaking</h1>\n";
            html << "<button class='btn btn-primary' onclick='startSearch()'>Find Match</button>\n";
            html << "<button class='btn btn-secondary' onclick='showBrowser()'>Browse Lobbies</button>\n";
            html << "<button class='btn btn-secondary' onclick='createLobby()'>Create Lobby</button>\n";
            html << "</div>\n";
            break;

        case MatchmakingUIState::Searching:
            html << renderSearchingHTML();
            break;

        case MatchmakingUIState::MatchFound:
            html << renderMatchFoundHTML();
            break;

        case MatchmakingUIState::InLobby:
            html << renderLobbyHTML();
            break;

        default:
            break;
    }

    if (m_showLobbyBrowser) {
        html << renderLobbyBrowserHTML();
    }

    if (m_showReadyCheck) {
        html << renderReadyCheckHTML();
    }

    if (m_showErrorDialog) {
        html << "<div class='modal'>\n";
        html << "<div class='modal-content'>\n";
        html << "<h2>Error</h2>\n";
        html << "<p>" << m_errorMessage << "</p>\n";
        html << "<button class='btn btn-primary' onclick='hideError()'>OK</button>\n";
        html << "</div>\n</div>\n";
    }

    html << "</div>\n";
    html << "<script>\n";
    html << "function startSearch() { window.location.href = 'matchmaking://search'; }\n";
    html << "function cancelSearch() { window.location.href = 'matchmaking://cancel'; }\n";
    html << "function acceptMatch() { window.location.href = 'matchmaking://accept'; }\n";
    html << "function declineMatch() { window.location.href = 'matchmaking://decline'; }\n";
    html << "function showBrowser() { window.location.href = 'matchmaking://browser'; }\n";
    html << "function createLobby() { window.location.href = 'matchmaking://create'; }\n";
    html << "function joinLobby(id) { window.location.href = 'matchmaking://join/' + id; }\n";
    html << "function setReady(ready) { window.location.href = 'matchmaking://ready/' + ready; }\n";
    html << "function leaveLobby() { window.location.href = 'matchmaking://leave'; }\n";
    html << "function startMatch() { window.location.href = 'matchmaking://start'; }\n";
    html << "function hideError() { window.location.href = 'matchmaking://hideError'; }\n";
    html << "</script>\n";
    html << "</body>\n</html>";

    return html.str();
}

void MatchmakingUI::setState(MatchmakingUIState state) {
    if (m_state != state) {
        m_state = state;
        for (auto& callback : m_stateCallbacks) {
            callback(state);
        }
    }
}

void MatchmakingUI::onStateChange(StateChangeCallback callback) {
    m_stateCallbacks.push_back(callback);
}

void MatchmakingUI::showSearchUI() {
    m_isSearching = true;
    setState(MatchmakingUIState::Searching);
}

void MatchmakingUI::updateSearchProgress(const SearchProgressInfo& info) {
    m_searchProgress = info;
}

void MatchmakingUI::hideSearchUI() {
    m_isSearching = false;
    if (m_state == MatchmakingUIState::Searching) {
        setState(MatchmakingUIState::Idle);
    }
}

void MatchmakingUI::showMatchFound(float acceptTimeout) {
    m_showMatchFound = true;
    m_matchFoundTimeout = acceptTimeout;
    m_matchFoundTimer = 0.0f;
    setState(MatchmakingUIState::MatchFound);
}

void MatchmakingUI::hideMatchFound() {
    m_showMatchFound = false;
}

void MatchmakingUI::acceptMatch() {
    hideMatchFound();
    // Notify Firebase matchmaking
    ::Network::Firebase::FirebaseMatchmaking::getInstance().setReady(true);
}

void MatchmakingUI::declineMatch() {
    hideMatchFound();
    setState(MatchmakingUIState::Idle);
    // Notify Firebase matchmaking
    ::Network::Firebase::FirebaseMatchmaking::getInstance().cancelSearch();
}

void MatchmakingUI::showLobbyBrowser() {
    m_showLobbyBrowser = true;
}

void MatchmakingUI::hideLobbyBrowser() {
    m_showLobbyBrowser = false;
}

void MatchmakingUI::updateLobbyList(const std::vector<LobbyDisplayInfo>& lobbies) {
    m_lobbies = lobbies;
}

void MatchmakingUI::setLobbyFilter(const std::string& filter) {
    m_lobbyFilter = filter;
}

void MatchmakingUI::refreshLobbyList() {
    // Request new lobby list from Firebase
}

void MatchmakingUI::onLobbyJoin(LobbyJoinCallback callback) {
    m_lobbyJoinCallbacks.push_back(callback);
}

void MatchmakingUI::showLobbyUI(const std::string& lobbyId, const std::string& name) {
    m_inLobby = true;
    m_currentLobbyId = lobbyId;
    m_currentLobbyName = name;
    m_showLobbyBrowser = false;
    setState(MatchmakingUIState::InLobby);
}

void MatchmakingUI::hideLobbyUI() {
    m_inLobby = false;
    m_currentLobbyId.clear();
    m_players.clear();
    setState(MatchmakingUIState::Idle);
}

void MatchmakingUI::updatePlayerList(const std::vector<PlayerDisplayInfo>& players) {
    m_players = players;
}

void MatchmakingUI::updateLobbySettings(const std::unordered_map<std::string, std::string>& settings) {
    m_lobbySettings = settings;
}

bool MatchmakingUI::isHost() const {
    for (const auto& player : m_players) {
        if (player.oderId == m_localPlayerId) {
            return player.isHost;
        }
    }
    return false;
}

void MatchmakingUI::setReady(bool ready) {
    m_isReady = ready;
    for (auto& callback : m_readyCallbacks) {
        callback(ready);
    }
}

void MatchmakingUI::onReadyChange(ReadyCallback callback) {
    m_readyCallbacks.push_back(callback);
}

void MatchmakingUI::showReadyCheck(float timeout) {
    m_showReadyCheck = true;
    m_readyCheckTimeout = timeout;
    m_readyCheckTimer = 0.0f;
}

void MatchmakingUI::hideReadyCheck() {
    m_showReadyCheck = false;
}

bool MatchmakingUI::isAllReady() const {
    if (m_players.empty()) return false;
    for (const auto& player : m_players) {
        if (!player.isReady) return false;
    }
    return true;
}

void MatchmakingUI::showInviteCode(const std::string& code) {
    m_inviteCode = code;
}

void MatchmakingUI::copyInviteCode() {
    // Platform-specific clipboard copy
}

void MatchmakingUI::joinByCode(const std::string& code) {
    ::Network::Firebase::FirebaseMatchmaking::getInstance().joinLobbyByCode(code, nullptr);
}

void MatchmakingUI::showError(const std::string& message) {
    m_showErrorDialog = true;
    m_errorMessage = message;
    setState(MatchmakingUIState::Error);
}

void MatchmakingUI::hideError() {
    m_showErrorDialog = false;
    setState(MatchmakingUIState::Idle);
}

// Private rendering methods

std::string MatchmakingUI::renderSearchingHTML() {
    std::ostringstream html;

    html << "<div class='panel' style='text-align: center;'>\n";
    html << "<h1>Finding Match...</h1>\n";
    html << "<div class='search-spinner'></div>\n";
    html << "<p>Search time: " << formatTime(m_searchProgress.searchTime) << "</p>\n";
    html << "<p>MMR Range: +/- " << m_searchProgress.currentMmrRange << "</p>\n";
    html << "<p>Players in queue: " << m_searchProgress.playersInQueue << "</p>\n";
    html << "<p>Estimated wait: " << m_searchProgress.estimatedWaitTime << "</p>\n";
    html << "<button class='btn btn-danger' onclick='cancelSearch()'>Cancel</button>\n";
    html << "</div>\n";

    return html.str();
}

std::string MatchmakingUI::renderMatchFoundHTML() {
    std::ostringstream html;

    float remaining = m_matchFoundTimeout - m_matchFoundTimer;

    html << "<div class='modal'>\n";
    html << "<div class='modal-content'>\n";
    html << "<h2>Match Found!</h2>\n";
    html << "<div class='countdown'>" << static_cast<int>(remaining) << "</div>\n";
    html << "<div class='progress-bar'><div class='progress-fill' style='width: " << (remaining / m_matchFoundTimeout * 100) << "%;'></div></div>\n";
    html << "<div style='margin-top: 20px;'>\n";
    html << "<button class='btn btn-primary' onclick='acceptMatch()'>Accept</button>\n";
    html << "<button class='btn btn-secondary' onclick='declineMatch()'>Decline</button>\n";
    html << "</div>\n";
    html << "</div>\n</div>\n";

    return html.str();
}

std::string MatchmakingUI::renderLobbyBrowserHTML() {
    std::ostringstream html;

    html << "<div class='panel'>\n";
    html << "<h2>Lobby Browser</h2>\n";
    html << "<input type='text' placeholder='Search lobbies...' value='" << m_lobbyFilter << "' style='width: 100%; padding: 10px; margin-bottom: 15px; border-radius: 8px; border: none; background: rgba(0,0,0,0.3); color: #fff;'>\n";

    for (size_t i = 0; i < m_lobbies.size(); i++) {
        const auto& lobby = m_lobbies[i];
        html << "<div class='lobby-item' onclick='joinLobby(\"" << lobby.lobbyId << "\")'>\n";
        html << "<div style='display: flex; justify-content: space-between;'>\n";
        html << "<strong>" << lobby.name << "</strong>\n";
        html << "<span>" << lobby.playerCount << "/" << lobby.maxPlayers << "</span>\n";
        html << "</div>\n";
        html << "<div style='color: #888; font-size: 14px;'>" << lobby.gameMode << " | Host: " << lobby.hostName << "</div>\n";
        html << "</div>\n";
    }

    if (m_lobbies.empty()) {
        html << "<p style='text-align: center; color: #888;'>No lobbies found</p>\n";
    }

    html << "<button class='btn btn-secondary' onclick='hideLobbyBrowser()'>Close</button>\n";
    html << "</div>\n";

    return html.str();
}

std::string MatchmakingUI::renderLobbyHTML() {
    std::ostringstream html;

    html << "<div class='panel'>\n";
    html << "<h1>" << m_currentLobbyName << "</h1>\n";

    if (!m_inviteCode.empty()) {
        html << "<p>Invite Code: <strong>" << m_inviteCode << "</strong> <button class='btn btn-secondary' onclick='copyInviteCode()'>Copy</button></p>\n";
    }

    html << "<h2>Players</h2>\n";
    html << "<ul class='player-list'>\n";

    for (const auto& player : m_players) {
        html << "<li class='player-item'>\n";
        html << "<span class='player-name'>" << player.displayName;
        if (player.isHost) html << " (Host)";
        html << "</span>\n";
        html << "<span>Rating: " << player.rating << "</span>\n";
        html << "<div class='player-ready" << (player.isReady ? " ready" : "") << "'></div>\n";
        html << "</li>\n";
    }

    html << "</ul>\n";

    html << "<div style='margin-top: 20px;'>\n";

    if (m_isReady) {
        html << "<button class='btn btn-secondary' onclick='setReady(false)'>Not Ready</button>\n";
    } else {
        html << "<button class='btn btn-primary' onclick='setReady(true)'>Ready</button>\n";
    }

    if (isHost()) {
        html << "<button class='btn btn-primary' onclick='startMatch()'" << (isAllReady() ? "" : " disabled") << ">Start Match</button>\n";
    }

    html << "<button class='btn btn-danger' onclick='leaveLobby()'>Leave</button>\n";
    html << "</div>\n";
    html << "</div>\n";

    return html.str();
}

std::string MatchmakingUI::renderReadyCheckHTML() {
    std::ostringstream html;

    float remaining = m_readyCheckTimeout - m_readyCheckTimer;

    html << "<div class='modal'>\n";
    html << "<div class='modal-content'>\n";
    html << "<h2>Ready Check</h2>\n";
    html << "<p>Are you ready to start?</p>\n";
    html << "<div class='countdown'>" << static_cast<int>(remaining) << "</div>\n";
    html << "<button class='btn btn-primary' onclick='setReady(true)'>Ready!</button>\n";
    html << "</div>\n</div>\n";

    return html.str();
}

std::string MatchmakingUI::formatTime(float seconds) {
    int mins = static_cast<int>(seconds) / 60;
    int secs = static_cast<int>(seconds) % 60;

    std::ostringstream ss;
    ss << mins << ":" << std::setfill('0') << std::setw(2) << secs;
    return ss.str();
}

} // namespace Network
} // namespace Editor
