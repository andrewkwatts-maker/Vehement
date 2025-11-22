#include "FirebaseMatchmaking.hpp"
#include <sstream>
#include <random>
#include <algorithm>
#include <cmath>

namespace Network {
namespace Firebase {

FirebaseMatchmaking& FirebaseMatchmaking::getInstance() {
    static FirebaseMatchmaking instance;
    return instance;
}

FirebaseMatchmaking::FirebaseMatchmaking() {}

FirebaseMatchmaking::~FirebaseMatchmaking() {
    shutdown();
}

bool FirebaseMatchmaking::initialize() {
    if (m_initialized) return true;

    if (!FirebaseCore::getInstance().isInitialized()) {
        return false;
    }

    m_initialized = true;
    m_matchState = MatchState::None;

    return true;
}

void FirebaseMatchmaking::shutdown() {
    if (!m_initialized) return;

    cancelSearch();
    leaveLobby();

    m_initialized = false;
}

void FirebaseMatchmaking::update(float deltaTime) {
    if (!m_initialized) return;

    // Update search timer and expand if needed
    if (m_isSearching) {
        m_searchExpandTimer += deltaTime;

        if (m_searchExpandTimer >= SEARCH_EXPAND_INTERVAL) {
            m_searchExpandTimer = 0.0f;
            expandSearch();
        }

        // Update search progress callback
        if (m_searchProgressCallback) {
            float elapsed = std::chrono::duration<float>(
                std::chrono::steady_clock::now() - m_searchStartTime).count();
            m_searchProgressCallback(elapsed, m_currentTicket.expandCount);
        }

        processMatchmakingQueue();
    }

    // Update ready check timeout
    if (m_readyCheckActive) {
        float elapsed = std::chrono::duration<float>(
            std::chrono::steady_clock::now() - m_readyCheckStartTime).count();

        if (elapsed >= READY_CHECK_TIMEOUT) {
            // Ready check timed out
            m_readyCheckActive = false;

            for (auto& callback : m_readyCheckCallbacks) {
                callback(false);
            }
        }
    }
}

void FirebaseMatchmaking::startQuickMatch(const MatchFilters& filters, MatchFoundCallback callback) {
    if (!m_initialized || m_isSearching || m_currentLobby) {
        FirebaseError error;
        error.type = FirebaseErrorType::InvalidArgument;
        error.message = m_isSearching ? "Already searching" : "Already in lobby";
        callback(MatchLobby(), error);
        return;
    }

    auto& core = FirebaseCore::getInstance();
    if (!core.isSignedIn()) {
        FirebaseError error;
        error.type = FirebaseErrorType::AuthError;
        error.message = "Not signed in";
        callback(MatchLobby(), error);
        return;
    }

    // Create search ticket
    m_currentTicket.ticketId = generateTicketId();
    m_currentTicket.oderId = core.getCurrentUser().uid;
    m_currentTicket.filters = filters;
    m_currentTicket.createdAt = std::chrono::system_clock::now();
    m_currentTicket.expandCount = 0;

    // Get player rating
    getMyRating([this, callback](const PlayerRating& rating, const FirebaseError& error) {
        if (error) {
            m_currentTicket.rating = PlayerRating();  // Default rating
        } else {
            m_currentTicket.rating = rating;
        }

        m_isSearching = true;
        m_searchStartTime = std::chrono::steady_clock::now();
        m_searchExpandTimer = 0.0f;
        m_matchFoundCallback = callback;
        m_matchState = MatchState::Searching;

        // Add to matchmaking queue
        addToMatchmakingQueue(m_currentTicket);
    });
}

void FirebaseMatchmaking::cancelSearch() {
    if (!m_isSearching) return;

    removeFromMatchmakingQueue(m_currentTicket.ticketId);

    m_isSearching = false;
    m_matchState = MatchState::None;
    m_currentTicket = MatchTicket();
    m_matchFoundCallback = nullptr;
}

float FirebaseMatchmaking::getSearchTime() const {
    if (!m_isSearching) return 0.0f;

    return std::chrono::duration<float>(
        std::chrono::steady_clock::now() - m_searchStartTime).count();
}

void FirebaseMatchmaking::createLobby(const std::string& name, GameMode mode, int maxPlayers,
                                      const std::string& password, MatchFoundCallback callback) {
    if (!m_initialized || m_currentLobby) {
        FirebaseError error;
        error.type = FirebaseErrorType::InvalidArgument;
        error.message = "Already in a lobby";
        callback(MatchLobby(), error);
        return;
    }

    auto& core = FirebaseCore::getInstance();
    if (!core.isSignedIn()) {
        FirebaseError error;
        error.type = FirebaseErrorType::AuthError;
        error.message = "Not signed in";
        callback(MatchLobby(), error);
        return;
    }

    MatchLobby lobby;
    lobby.lobbyId = generateTicketId();
    lobby.hostId = core.getCurrentUser().uid;
    lobby.name = name;
    lobby.password = password;
    lobby.gameMode = mode;
    lobby.maxPlayers = maxPlayers;
    lobby.minPlayers = 2;
    lobby.teamCount = 2;
    lobby.state = MatchState::InLobby;
    lobby.matchMode = password.empty() ? MatchmakingMode::Custom : MatchmakingMode::Private;
    lobby.createdAt = std::chrono::system_clock::now();
    lobby.expiresAt = lobby.createdAt + std::chrono::hours(1);

    // Add host as first player
    LobbyPlayer hostPlayer;
    hostPlayer.odId = core.getCurrentUser().uid;
    hostPlayer.displayName = core.getCurrentUser().displayName;
    hostPlayer.isHost = true;
    hostPlayer.isReady = false;
    hostPlayer.team = 0;
    hostPlayer.slot = 0;
    hostPlayer.joinedAt = std::chrono::system_clock::now();

    lobby.players.push_back(hostPlayer);

    createLobbyDocument(lobby, callback);
}

void FirebaseMatchmaking::joinLobby(const std::string& lobbyId, const std::string& password,
                                    MatchFoundCallback callback) {
    if (!m_initialized || m_currentLobby) {
        FirebaseError error;
        error.type = FirebaseErrorType::InvalidArgument;
        error.message = "Already in a lobby";
        callback(MatchLobby(), error);
        return;
    }

    auto& core = FirebaseCore::getInstance();

    // Fetch lobby from Firestore
    HttpRequest request;
    request.method = "GET";
    request.url = core.getConfig().getFirestoreUrl() + "/lobbies/" + lobbyId;

    core.makeAuthenticatedRequest(request, [this, password, callback](const HttpResponse& response) {
        if (response.statusCode != 200) {
            callback(MatchLobby(), parseFirestoreError(response));
            return;
        }

        MatchLobby lobby = parseLobbyFromFirestore(response.body);

        // Check password
        if (!lobby.password.empty() && lobby.password != password) {
            FirebaseError error;
            error.type = FirebaseErrorType::PermissionDenied;
            error.message = "Invalid password";
            callback(MatchLobby(), error);
            return;
        }

        // Check if full
        if (lobby.isFull()) {
            FirebaseError error;
            error.type = FirebaseErrorType::AlreadyExists;
            error.message = "Lobby is full";
            callback(MatchLobby(), error);
            return;
        }

        // Add player to lobby
        auto& core = FirebaseCore::getInstance();
        LobbyPlayer player;
        player.odId = core.getCurrentUser().uid;
        player.displayName = core.getCurrentUser().displayName;
        player.isHost = false;
        player.isReady = false;
        player.team = lobby.players.size() % lobby.teamCount;
        player.slot = static_cast<int>(lobby.players.size());
        player.joinedAt = std::chrono::system_clock::now();

        lobby.players.push_back(player);

        m_currentLobby = lobby;
        m_matchState = MatchState::InLobby;

        updateLobbyDocument(lobby);
        subscribeToLobby(lobby.lobbyId);

        callback(lobby, FirebaseError());
    });
}

void FirebaseMatchmaking::joinLobbyByCode(const std::string& code, MatchFoundCallback callback) {
    auto& core = FirebaseCore::getInstance();

    // Query Firestore for lobby with this code
    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + ":runQuery";
    request.headers["Content-Type"] = "application/json";

    std::ostringstream query;
    query << "{"
          << "\"structuredQuery\":{"
          << "\"from\":[{\"collectionId\":\"lobbies\"}],"
          << "\"where\":{"
          << "\"fieldFilter\":{"
          << "\"field\":{\"fieldPath\":\"inviteCode\"},"
          << "\"op\":\"EQUAL\","
          << "\"value\":{\"stringValue\":\"" << code << "\"}"
          << "}"
          << "},"
          << "\"limit\":1"
          << "}"
          << "}";
    request.body = query.str();

    core.makeAuthenticatedRequest(request, [this, callback](const HttpResponse& response) {
        if (response.statusCode != 200) {
            callback(MatchLobby(), parseFirestoreError(response));
            return;
        }

        // Parse lobby ID from response and join
        std::string lobbyId = extractLobbyIdFromQuery(response.body);
        if (lobbyId.empty()) {
            FirebaseError error;
            error.type = FirebaseErrorType::NotFound;
            error.message = "Invalid invite code";
            callback(MatchLobby(), error);
            return;
        }

        joinLobby(lobbyId, "", callback);
    });
}

void FirebaseMatchmaking::leaveLobby() {
    if (!m_currentLobby) return;

    auto& core = FirebaseCore::getInstance();
    std::string oderId = core.getCurrentUser().uid;

    // Remove player from lobby
    auto& players = m_currentLobby->players;
    players.erase(
        std::remove_if(players.begin(), players.end(),
            [&oderId](const LobbyPlayer& p) { return p.odId == oderId; }),
        players.end());

    if (players.empty()) {
        // Delete empty lobby
        deleteLobbyDocument(m_currentLobby->lobbyId);
    } else {
        // Transfer host if leaving player was host
        if (m_currentLobby->hostId == oderId && !players.empty()) {
            m_currentLobby->hostId = players[0].odId;
            players[0].isHost = true;
        }
        updateLobbyDocument(*m_currentLobby);
    }

    unsubscribeFromLobby();
    m_currentLobby.reset();
    m_matchState = MatchState::None;
}

void FirebaseMatchmaking::setLobbySettings(const std::unordered_map<std::string, std::string>& settings) {
    if (!m_currentLobby || !isHost()) return;

    m_currentLobby->settings = settings;
    updateLobbyDocument(*m_currentLobby);
    notifyLobbyUpdate();
}

void FirebaseMatchmaking::kickPlayer(const std::string& oderId) {
    if (!m_currentLobby || !isHost()) return;

    auto& players = m_currentLobby->players;
    players.erase(
        std::remove_if(players.begin(), players.end(),
            [&oderId](const LobbyPlayer& p) { return p.odId == oderId; }),
        players.end());

    updateLobbyDocument(*m_currentLobby);

    for (auto& callback : m_playerLeaveCallbacks) {
        callback(oderId);
    }
}

void FirebaseMatchmaking::transferHost(const std::string& newHostId) {
    if (!m_currentLobby || !isHost()) return;

    // Find current host and new host
    for (auto& player : m_currentLobby->players) {
        if (player.odId == m_currentLobby->hostId) {
            player.isHost = false;
        }
        if (player.odId == newHostId) {
            player.isHost = true;
        }
    }

    m_currentLobby->hostId = newHostId;
    updateLobbyDocument(*m_currentLobby);
    notifyLobbyUpdate();
}

void FirebaseMatchmaking::setTeam(const std::string& oderId, int team) {
    if (!m_currentLobby || !isHost()) return;

    for (auto& player : m_currentLobby->players) {
        if (player.odId == oderId) {
            player.team = team;
            break;
        }
    }

    updateLobbyDocument(*m_currentLobby);
    notifyLobbyUpdate();
}

void FirebaseMatchmaking::setMaxPlayers(int maxPlayers) {
    if (!m_currentLobby || !isHost()) return;

    m_currentLobby->maxPlayers = maxPlayers;
    updateLobbyDocument(*m_currentLobby);
    notifyLobbyUpdate();
}

void FirebaseMatchmaking::setPassword(const std::string& password) {
    if (!m_currentLobby || !isHost()) return;

    m_currentLobby->password = password;
    updateLobbyDocument(*m_currentLobby);
}

void FirebaseMatchmaking::setReady(bool ready) {
    if (!m_currentLobby) return;

    auto& core = FirebaseCore::getInstance();
    std::string oderId = core.getCurrentUser().uid;

    for (auto& player : m_currentLobby->players) {
        if (player.odId == oderId) {
            player.isReady = ready;
            break;
        }
    }

    updateLobbyDocument(*m_currentLobby);
    notifyLobbyUpdate();

    // Check if all ready
    bool allReady = std::all_of(m_currentLobby->players.begin(), m_currentLobby->players.end(),
        [](const LobbyPlayer& p) { return p.isReady; });

    if (allReady && m_currentLobby->players.size() >= static_cast<size_t>(m_currentLobby->minPlayers)) {
        for (auto& callback : m_readyCheckCallbacks) {
            callback(true);
        }
    }
}

void FirebaseMatchmaking::startReadyCheck() {
    if (!m_currentLobby || !isHost()) return;

    m_readyCheckActive = true;
    m_readyCheckStartTime = std::chrono::steady_clock::now();
    m_readyResponses.clear();

    // Reset all ready states
    for (auto& player : m_currentLobby->players) {
        player.isReady = false;
    }

    updateLobbyDocument(*m_currentLobby);
    notifyLobbyUpdate();
}

void FirebaseMatchmaking::respondToReadyCheck(bool accept) {
    if (!m_readyCheckActive || !m_currentLobby) return;

    setReady(accept);

    if (!accept) {
        m_readyCheckActive = false;
        for (auto& callback : m_readyCheckCallbacks) {
            callback(false);
        }
    }
}

void FirebaseMatchmaking::forceStart() {
    if (!m_currentLobby || !isHost()) return;

    if (m_currentLobby->players.size() >= static_cast<size_t>(m_currentLobby->minPlayers)) {
        startMatch();
    }
}

void FirebaseMatchmaking::startMatch() {
    if (!m_currentLobby || !isHost()) return;

    m_currentLobby->state = MatchState::Starting;
    updateLobbyDocument(*m_currentLobby);

    // Generate match ID and server info
    std::string matchId = generateTicketId();
    std::string serverInfo = "{}";  // Would contain actual server connection info

    m_matchState = MatchState::Starting;

    // Notify all players
    for (auto& callback : m_matchStartCallbacks) {
        callback(matchId, serverInfo);
    }
}

void FirebaseMatchmaking::requestRematch() {
    if (!m_currentLobby) return;

    m_rematchRequested = true;
    m_rematchResponses.clear();

    auto& core = FirebaseCore::getInstance();
    m_rematchResponses[core.getCurrentUser().uid] = true;

    // Update lobby with rematch request
    m_currentLobby->settings["rematchRequested"] = "true";
    updateLobbyDocument(*m_currentLobby);
}

void FirebaseMatchmaking::acceptRematch(bool accept) {
    if (!m_currentLobby || !m_rematchRequested) return;

    auto& core = FirebaseCore::getInstance();
    m_rematchResponses[core.getCurrentUser().uid] = accept;

    if (!accept) {
        m_rematchRequested = false;
        leaveLobby();
        return;
    }

    // Check if all accepted
    bool allAccepted = m_rematchResponses.size() == m_currentLobby->players.size() &&
        std::all_of(m_rematchResponses.begin(), m_rematchResponses.end(),
            [](const auto& pair) { return pair.second; });

    if (allAccepted) {
        // Reset lobby for rematch
        for (auto& player : m_currentLobby->players) {
            player.isReady = false;
        }
        m_currentLobby->state = MatchState::InLobby;
        m_rematchRequested = false;
        m_rematchResponses.clear();

        updateLobbyDocument(*m_currentLobby);
        notifyLobbyUpdate();
    }
}

void FirebaseMatchmaking::browsersLobbies(const MatchFilters& filters, LobbyBrowserCallback callback) {
    m_lastBrowserFilters = filters;

    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + ":runQuery";
    request.headers["Content-Type"] = "application/json";

    // Build query
    std::ostringstream query;
    query << "{"
          << "\"structuredQuery\":{"
          << "\"from\":[{\"collectionId\":\"lobbies\"}],"
          << "\"where\":{"
          << "\"compositeFilter\":{"
          << "\"op\":\"AND\","
          << "\"filters\":["
          << "{\"fieldFilter\":{\"field\":{\"fieldPath\":\"state\"},\"op\":\"EQUAL\",\"value\":{\"stringValue\":\"InLobby\"}}},"
          << "{\"fieldFilter\":{\"field\":{\"fieldPath\":\"isPublic\"},\"op\":\"EQUAL\",\"value\":{\"booleanValue\":true}}}"
          << "]"
          << "}"
          << "},"
          << "\"orderBy\":[{\"field\":{\"fieldPath\":\"createdAt\"},\"direction\":\"DESCENDING\"}],"
          << "\"limit\":50"
          << "}"
          << "}";
    request.body = query.str();

    core.makeAuthenticatedRequest(request, [this, callback](const HttpResponse& response) {
        if (response.statusCode != 200) {
            callback({}, parseFirestoreError(response));
            return;
        }

        std::vector<LobbyBrowserEntry> entries = parseLobbyBrowserResults(response.body);
        m_cachedLobbies = entries;
        m_lastBrowserRefresh = std::chrono::steady_clock::now();

        callback(entries, FirebaseError());
    });
}

void FirebaseMatchmaking::refreshLobbyBrowser(LobbyBrowserCallback callback) {
    browsersLobbies(m_lastBrowserFilters, callback);
}

void FirebaseMatchmaking::getPlayerRating(const std::string& oderId, RatingCallback callback) {
    // Check cache first
    auto it = m_ratingCache.find(oderId);
    if (it != m_ratingCache.end()) {
        callback(it->second, FirebaseError());
        return;
    }

    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "GET";
    request.url = core.getConfig().getFirestoreUrl() + "/players/" + oderId + "/rating";

    core.makeAuthenticatedRequest(request, [this, oderId, callback](const HttpResponse& response) {
        if (response.statusCode != 200) {
            // Return default rating for new players
            if (response.statusCode == 404) {
                PlayerRating defaultRating;
                callback(defaultRating, FirebaseError());
                return;
            }
            callback(PlayerRating(), parseFirestoreError(response));
            return;
        }

        PlayerRating rating = parseRatingFromFirestore(response.body);
        m_ratingCache[oderId] = rating;
        callback(rating, FirebaseError());
    });
}

void FirebaseMatchmaking::getMyRating(RatingCallback callback) {
    auto& core = FirebaseCore::getInstance();
    getPlayerRating(core.getCurrentUser().uid, callback);
}

void FirebaseMatchmaking::submitMatchResult(const MatchResult& result, RatingChangeCallback callback) {
    auto& core = FirebaseCore::getInstance();
    std::string myId = core.getCurrentUser().uid;

    // Find my result
    const MatchResult::PlayerResult* myResult = nullptr;
    for (const auto& pr : result.results) {
        if (pr.oderId == myId) {
            myResult = &pr;
            break;
        }
    }

    if (!myResult) {
        FirebaseError error;
        error.type = FirebaseErrorType::NotFound;
        error.message = "Player not in match results";
        callback(RatingChange(), error);
        return;
    }

    // Get current rating and calculate change
    getMyRating([this, result, myResult, callback](const PlayerRating& currentRating, const FirebaseError& error) {
        if (error) {
            callback(RatingChange(), error);
            return;
        }

        RatingChange change = calculateRatingChange(currentRating, *myResult, result);

        // Update rating in Firestore
        PlayerRating newRating = currentRating;
        newRating.mmr = change.newMmr;
        newRating.elo = change.newElo;
        newRating.tier = change.newTier;
        newRating.division = change.newDivision;

        if (myResult->placement == 1) {
            newRating.wins++;
            newRating.streak = std::max(1, newRating.streak + 1);
        } else {
            newRating.losses++;
            newRating.streak = std::min(-1, newRating.streak - 1);
        }

        newRating.peakMmr = std::max(newRating.peakMmr, newRating.mmr);

        updatePlayerRating(newRating, [change, callback](const FirebaseError& updateError) {
            callback(change, updateError);
        });
    });
}

void FirebaseMatchmaking::getLeaderboard(int count, int offset,
    std::function<void(const std::vector<PlayerRating>&, const FirebaseError&)> callback) {

    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + ":runQuery";
    request.headers["Content-Type"] = "application/json";

    std::ostringstream query;
    query << "{"
          << "\"structuredQuery\":{"
          << "\"from\":[{\"collectionId\":\"players\"}],"
          << "\"orderBy\":[{\"field\":{\"fieldPath\":\"rating.mmr\"},\"direction\":\"DESCENDING\"}],"
          << "\"offset\":" << offset << ","
          << "\"limit\":" << count
          << "}"
          << "}";
    request.body = query.str();

    core.makeAuthenticatedRequest(request, [callback](const HttpResponse& response) {
        if (response.statusCode != 200) {
            callback({}, parseFirestoreError(response));
            return;
        }

        std::vector<PlayerRating> ratings = parseLeaderboardFromFirestore(response.body);
        callback(ratings, FirebaseError());
    });
}

void FirebaseMatchmaking::onLobbyUpdate(LobbyUpdateCallback callback) {
    m_lobbyUpdateCallbacks.push_back(callback);
}

void FirebaseMatchmaking::onPlayerJoin(PlayerJoinCallback callback) {
    m_playerJoinCallbacks.push_back(callback);
}

void FirebaseMatchmaking::onPlayerLeave(PlayerLeaveCallback callback) {
    m_playerLeaveCallbacks.push_back(callback);
}

void FirebaseMatchmaking::onReadyCheck(ReadyCheckCallback callback) {
    m_readyCheckCallbacks.push_back(callback);
}

void FirebaseMatchmaking::onMatchStart(MatchStartCallback callback) {
    m_matchStartCallbacks.push_back(callback);
}

void FirebaseMatchmaking::onSearchProgress(std::function<void(float, int)> callback) {
    m_searchProgressCallback = callback;
}

bool FirebaseMatchmaking::isHost() const {
    if (!m_currentLobby) return false;

    auto& core = FirebaseCore::getInstance();
    return m_currentLobby->hostId == core.getCurrentUser().uid;
}

std::string FirebaseMatchmaking::getInviteCode() const {
    if (!m_currentLobby) return "";

    auto it = m_currentLobby->settings.find("inviteCode");
    if (it != m_currentLobby->settings.end()) {
        return it->second;
    }

    return "";
}

// Private methods

void FirebaseMatchmaking::processMatchmakingQueue() {
    // This would query the matchmaking queue and find compatible players
    // Simplified implementation - in production, use Cloud Functions for this
}

void FirebaseMatchmaking::expandSearch() {
    if (m_currentTicket.expandCount >= MAX_SEARCH_EXPANDS) return;

    m_currentTicket.expandCount++;
    m_currentTicket.filters.mmrRange = static_cast<int>(
        m_currentTicket.filters.mmrRange * m_currentTicket.filters.searchTimeMultiplier);

    // Update ticket in queue
    addToMatchmakingQueue(m_currentTicket);
}

bool FirebaseMatchmaking::checkMatchCompatibility(const MatchTicket& ticket1, const MatchTicket& ticket2) {
    // Check MMR range
    int mmrDiff = std::abs(ticket1.rating.mmr - ticket2.rating.mmr);
    int maxRange = std::max(ticket1.filters.mmrRange, ticket2.filters.mmrRange);

    if (mmrDiff > maxRange) return false;

    // Check game mode compatibility
    bool modeMatch = false;
    for (auto mode1 : ticket1.filters.gameModes) {
        for (auto mode2 : ticket2.filters.gameModes) {
            if (mode1 == mode2) {
                modeMatch = true;
                break;
            }
        }
        if (modeMatch) break;
    }

    if (!modeMatch && !ticket1.filters.gameModes.empty() && !ticket2.filters.gameModes.empty()) {
        return false;
    }

    // Check region compatibility
    bool regionMatch = false;
    for (auto region1 : ticket1.filters.regions) {
        for (auto region2 : ticket2.filters.regions) {
            if (region1 == region2 || region1 == Region::Auto || region2 == Region::Auto) {
                regionMatch = true;
                break;
            }
        }
        if (regionMatch) break;
    }

    if (!regionMatch && !ticket1.filters.regions.empty() && !ticket2.filters.regions.empty()) {
        return false;
    }

    return true;
}

void FirebaseMatchmaking::createMatchFromTickets(const std::vector<MatchTicket>& tickets) {
    if (tickets.empty()) return;

    MatchLobby lobby;
    lobby.lobbyId = generateTicketId();
    lobby.hostId = tickets[0].oderId;
    lobby.gameMode = tickets[0].filters.gameModes.empty() ?
        GameMode::Deathmatch : tickets[0].filters.gameModes[0];
    lobby.maxPlayers = tickets[0].filters.maxPlayers;
    lobby.state = MatchState::Found;
    lobby.matchMode = MatchmakingMode::QuickMatch;

    for (const auto& ticket : tickets) {
        LobbyPlayer player;
        player.odId = ticket.oderId;
        player.isHost = (ticket.oderId == lobby.hostId);
        player.isReady = false;
        player.rating = ticket.rating;
        lobby.players.push_back(player);
    }

    onMatchFound(lobby);
}

void FirebaseMatchmaking::onMatchFound(const MatchLobby& lobby) {
    m_isSearching = false;
    m_currentLobby = lobby;
    m_matchState = MatchState::Found;

    if (m_matchFoundCallback) {
        m_matchFoundCallback(lobby, FirebaseError());
    }

    subscribeToLobby(lobby.lobbyId);
}

RatingChange FirebaseMatchmaking::calculateRatingChange(const PlayerRating& rating,
    const MatchResult::PlayerResult& result, const MatchResult& match) {

    RatingChange change;

    bool won = result.placement == 1;
    float performance = result.performance;

    // Calculate ELO change
    // Average opponent ELO
    int totalOpponentElo = 0;
    int opponentCount = 0;
    for (const auto& pr : match.results) {
        if (pr.oderId != result.oderId) {
            // Would need to fetch opponent ratings - simplified here
            totalOpponentElo += 1000;
            opponentCount++;
        }
    }
    int avgOpponentElo = opponentCount > 0 ? totalOpponentElo / opponentCount : 1000;

    change.eloChange = calculateEloChange(rating.elo, avgOpponentElo, won ? 1.0f : 0.0f);

    // Calculate MMR change
    change.mmrChange = calculateMmrChange(rating, performance, won);

    change.newElo = rating.elo + change.eloChange;
    change.newMmr = rating.mmr + change.mmrChange;

    // Calculate tier changes
    int oldTier = rating.tier;
    int newTier = change.newMmr / 200;  // Tier every 200 MMR
    newTier = std::clamp(newTier, 0, 10);

    change.newTier = newTier;
    change.newDivision = (change.newMmr % 200) / 50;  // 4 divisions per tier
    change.tierChanged = newTier != oldTier;
    change.promoted = newTier > oldTier;
    change.demoted = newTier < oldTier;

    return change;
}

int FirebaseMatchmaking::calculateEloChange(int playerElo, int opponentElo, float score) {
    // Standard ELO formula
    float expected = 1.0f / (1.0f + std::pow(10.0f, (opponentElo - playerElo) / 400.0f));
    int change = static_cast<int>(BASE_K_FACTOR * (score - expected));
    return change;
}

int FirebaseMatchmaking::calculateMmrChange(const PlayerRating& rating, float performance, bool won) {
    int baseChange = won ? 25 : -20;

    // Performance modifier (-10 to +10)
    int perfMod = static_cast<int>((performance - 0.5f) * 20);

    // Streak bonus/penalty
    int streakMod = 0;
    if (rating.streak > 2) streakMod = std::min(rating.streak - 2, 5);
    else if (rating.streak < -2) streakMod = std::max(rating.streak + 2, -5);

    return baseChange + perfMod + streakMod;
}

void FirebaseMatchmaking::createLobbyDocument(const MatchLobby& lobby, MatchFoundCallback callback) {
    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "POST";
    request.url = core.getConfig().getFirestoreUrl() + "/lobbies?documentId=" + lobby.lobbyId;
    request.headers["Content-Type"] = "application/json";
    request.body = serializeLobbyToFirestore(lobby);

    core.makeAuthenticatedRequest(request, [this, lobby, callback](const HttpResponse& response) {
        if (response.statusCode == 200 || response.statusCode == 201) {
            m_currentLobby = lobby;
            m_matchState = MatchState::InLobby;
            subscribeToLobby(lobby.lobbyId);
            callback(lobby, FirebaseError());
        } else {
            callback(MatchLobby(), parseFirestoreError(response));
        }
    });
}

void FirebaseMatchmaking::updateLobbyDocument(const MatchLobby& lobby) {
    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "PATCH";
    request.url = core.getConfig().getFirestoreUrl() + "/lobbies/" + lobby.lobbyId;
    request.headers["Content-Type"] = "application/json";
    request.body = serializeLobbyToFirestore(lobby);

    core.makeAuthenticatedRequest(request, [](const HttpResponse& response) {
        // Handle silently - updates are best-effort
    });
}

void FirebaseMatchmaking::deleteLobbyDocument(const std::string& lobbyId) {
    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "DELETE";
    request.url = core.getConfig().getFirestoreUrl() + "/lobbies/" + lobbyId;

    core.makeAuthenticatedRequest(request, [](const HttpResponse& response) {
        // Handle silently
    });
}

void FirebaseMatchmaking::subscribeToLobby(const std::string& lobbyId) {
    // In production, use Firestore listeners via REST long-polling or WebSocket
    // This is a simplified polling approach
}

void FirebaseMatchmaking::unsubscribeFromLobby() {
    // Stop polling/listening
}

void FirebaseMatchmaking::addToMatchmakingQueue(const MatchTicket& ticket) {
    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "PUT";
    request.url = core.getConfig().getRealtimeDbUrl() + "/matchmaking/" + ticket.ticketId + ".json";
    request.headers["Content-Type"] = "application/json";
    request.body = serializeTicketToJson(ticket);

    core.makeAuthenticatedRequest(request, [](const HttpResponse& response) {
        // Handle silently
    });
}

void FirebaseMatchmaking::removeFromMatchmakingQueue(const std::string& ticketId) {
    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "DELETE";
    request.url = core.getConfig().getRealtimeDbUrl() + "/matchmaking/" + ticketId + ".json";

    core.makeAuthenticatedRequest(request, [](const HttpResponse& response) {
        // Handle silently
    });
}

void FirebaseMatchmaking::subscribeToMatchmakingQueue() {
    // Subscribe to matchmaking queue updates
}

std::string FirebaseMatchmaking::generateLobbyCode() {
    static const char chars[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, sizeof(chars) - 2);

    std::string code;
    code.reserve(6);
    for (int i = 0; i < 6; i++) {
        code += chars[dis(gen)];
    }
    return code;
}

std::string FirebaseMatchmaking::generateTicketId() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static const char* hex = "0123456789abcdef";

    std::string id;
    id.reserve(24);
    for (int i = 0; i < 24; i++) {
        id += hex[dis(gen)];
    }
    return id;
}

void FirebaseMatchmaking::notifyLobbyUpdate() {
    if (!m_currentLobby) return;

    for (auto& callback : m_lobbyUpdateCallbacks) {
        callback(*m_currentLobby);
    }
}

// Static helper functions for serialization/parsing
namespace {

std::string serializeLobbyToFirestore(const MatchLobby& lobby) {
    std::ostringstream json;
    json << "{\"fields\":{"
         << "\"lobbyId\":{\"stringValue\":\"" << lobby.lobbyId << "\"},"
         << "\"hostId\":{\"stringValue\":\"" << lobby.hostId << "\"},"
         << "\"name\":{\"stringValue\":\"" << lobby.name << "\"},"
         << "\"gameMode\":{\"integerValue\":" << static_cast<int>(lobby.gameMode) << "},"
         << "\"state\":{\"stringValue\":\"" << static_cast<int>(lobby.state) << "\"},"
         << "\"maxPlayers\":{\"integerValue\":" << lobby.maxPlayers << "},"
         << "\"isPublic\":{\"booleanValue\":" << (lobby.isPublic() ? "true" : "false") << "},"
         << "\"playerCount\":{\"integerValue\":" << lobby.getPlayerCount() << "}"
         << "}}";
    return json.str();
}

std::string serializeTicketToJson(const MatchTicket& ticket) {
    std::ostringstream json;
    json << "{"
         << "\"ticketId\":\"" << ticket.ticketId << "\","
         << "\"playerId\":\"" << ticket.oderId << "\","
         << "\"mmr\":" << ticket.rating.mmr << ","
         << "\"mmrRange\":" << ticket.filters.mmrRange << ","
         << "\"expandCount\":" << ticket.expandCount << ","
         << "\"timestamp\":{\".sv\":\"timestamp\"}"
         << "}";
    return json.str();
}

MatchLobby parseLobbyFromFirestore(const std::string& json) {
    MatchLobby lobby;
    // Parse JSON - simplified implementation
    // In production, use proper JSON library
    return lobby;
}

std::vector<LobbyBrowserEntry> parseLobbyBrowserResults(const std::string& json) {
    std::vector<LobbyBrowserEntry> entries;
    // Parse JSON array
    return entries;
}

PlayerRating parseRatingFromFirestore(const std::string& json) {
    PlayerRating rating;
    // Parse JSON
    return rating;
}

std::vector<PlayerRating> parseLeaderboardFromFirestore(const std::string& json) {
    std::vector<PlayerRating> ratings;
    // Parse JSON array
    return ratings;
}

std::string extractLobbyIdFromQuery(const std::string& json) {
    // Extract lobby ID from query results
    return "";
}

FirebaseError parseFirestoreError(const HttpResponse& response) {
    FirebaseError error;
    error.code = response.statusCode;
    error.type = FirebaseErrorType::ServerError;
    error.message = "Firestore request failed";
    return error;
}

void updatePlayerRating(const PlayerRating& rating, std::function<void(const FirebaseError&)> callback) {
    auto& core = FirebaseCore::getInstance();

    HttpRequest request;
    request.method = "PATCH";
    request.url = core.getConfig().getFirestoreUrl() + "/players/" +
                  core.getCurrentUser().uid + "/rating";
    request.headers["Content-Type"] = "application/json";

    std::ostringstream json;
    json << "{\"fields\":{"
         << "\"mmr\":{\"integerValue\":" << rating.mmr << "},"
         << "\"elo\":{\"integerValue\":" << rating.elo << "},"
         << "\"tier\":{\"integerValue\":" << rating.tier << "},"
         << "\"division\":{\"integerValue\":" << rating.division << "},"
         << "\"wins\":{\"integerValue\":" << rating.wins << "},"
         << "\"losses\":{\"integerValue\":" << rating.losses << "},"
         << "\"peakMmr\":{\"integerValue\":" << rating.peakMmr << "}"
         << "}}";
    request.body = json.str();

    core.makeAuthenticatedRequest(request, [callback](const HttpResponse& response) {
        if (response.statusCode == 200) {
            callback(FirebaseError());
        } else {
            callback(parseFirestoreError(response));
        }
    });
}

} // anonymous namespace

} // namespace Firebase
} // namespace Network
