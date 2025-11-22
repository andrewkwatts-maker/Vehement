#pragma once

/**
 * @file RTSFogOfWar.hpp
 * @brief Main include header for the Session-Based Fog of War RTS system
 *
 * This header includes all components of the RTS fog of war system:
 *
 * Core Components:
 * - SessionFogOfWar: Three-state fog of war that resets each session
 * - VisionSource: Vision providers (hero, workers, buildings, towers)
 * - Exploration: Discovery system and exploration mechanics
 * - SessionManager: Session lifecycle and statistics tracking
 * - MinimapReveal: Fog-aware minimap rendering
 *
 * Design Philosophy:
 * The session-based fog of war creates tension and replayability by:
 * - Resetting exploration progress each session
 * - Encouraging re-exploration of the map
 * - Creating risk/reward decisions for scouting
 * - Enabling fair multiplayer starts
 *
 * Integration with Radiance Cascades:
 * The fog of war system integrates with the existing RadianceCascades
 * lighting system for realistic visibility and lighting effects:
 * - Fog dims/hides areas based on exploration state
 * - Radiance cascades only active in visible areas
 * - Explored but not visible shows last-known terrain
 *
 * Example Usage:
 * @code
 * using namespace Vehement2::RTS;
 *
 * // Initialize systems
 * SessionFogOfWar fog;
 * fog.Initialize(mapWidth, mapHeight, tileSize, screenWidth, screenHeight);
 * fog.SetRadianceCascades(&radianceCascades);
 *
 * Exploration exploration;
 * exploration.Initialize(&fog, mapWidth, mapHeight, tileSize);
 * exploration.GenerateDiscoveries(seed);
 *
 * SessionManager session;
 * session.Initialize(&fog, &exploration);
 *
 * MinimapReveal minimap;
 * minimap.Initialize(&fog, mapWidth, mapHeight, tileSize);
 *
 * // Start game session
 * session.StartSession();
 *
 * // Game loop
 * void Update(float deltaTime) {
 *     // Record activity on input
 *     session.RecordActivity();
 *
 *     // Update vision sources
 *     VisionEnvironment env;
 *     env.Update(deltaTime);
 *
 *     std::vector<VisionSource> sources;
 *     sources.push_back(VisionSource::Create(VisionSourceType::Hero, heroPos, heroId, 0));
 *     for (auto& worker : workers) {
 *         sources.push_back(VisionSource::Create(VisionSourceType::Worker, worker.pos, worker.id, 0));
 *     }
 *     for (auto& building : buildings) {
 *         sources.push_back(VisionSource::Create(VisionSourceType::Building, building.pos, building.id, 0));
 *     }
 *
 *     // Update fog of war
 *     fog.UpdateVision(sources, env, deltaTime);
 *     fog.UpdateRendering(deltaTime);
 *
 *     // Update exploration
 *     exploration.Update(deltaTime);
 *
 *     // Update session
 *     session.Update(deltaTime);
 *
 *     // Update minimap
 *     minimap.Update(deltaTime);
 * }
 *
 * void Render() {
 *     // Render scene with fog of war
 *     uint32_t fogTexture = fog.GetFogTexture();
 *     // Use fogTexture in rendering pipeline...
 *
 *     // Render minimap
 *     minimap.Render();
 * }
 *
 * // Handle disconnect
 * void OnDisconnect() {
 *     session.OnPlayerDisconnect();
 * }
 *
 * // Handle reconnect
 * void OnReconnect() {
 *     session.OnPlayerReconnect();
 * }
 * @endcode
 */

// Core fog of war
#include "SessionFogOfWar.hpp"

// Vision sources and environment
#include "VisionSource.hpp"

// Exploration and discovery
#include "Exploration.hpp"

// Session management
#include "SessionManager.hpp"

// Minimap with fog integration
#include "MinimapReveal.hpp"

namespace Vehement2 {
namespace RTS {

/**
 * @brief Convenience struct bundling all RTS fog of war systems
 *
 * Use this to easily manage all fog of war related systems together.
 */
struct RTSFogOfWarSystems {
    std::unique_ptr<SessionFogOfWar> fogOfWar;
    std::unique_ptr<Exploration> exploration;
    std::unique_ptr<SessionManager> sessionManager;
    std::unique_ptr<MinimapReveal> minimap;
    std::unique_ptr<VisionSourceManager> visionSources;
    VisionEnvironment environment;

    /**
     * @brief Initialize all systems
     */
    bool Initialize(int mapWidth, int mapHeight, float tileSize,
                    int screenWidth, int screenHeight, uint32_t discoverySeed = 0) {
        // Create systems
        fogOfWar = std::make_unique<SessionFogOfWar>();
        exploration = std::make_unique<Exploration>();
        sessionManager = std::make_unique<SessionManager>();
        minimap = std::make_unique<MinimapReveal>();
        visionSources = std::make_unique<VisionSourceManager>();

        // Initialize fog of war
        if (!fogOfWar->Initialize(mapWidth, mapHeight, tileSize, screenWidth, screenHeight)) {
            return false;
        }

        // Initialize exploration
        if (!exploration->Initialize(fogOfWar.get(), mapWidth, mapHeight, tileSize)) {
            return false;
        }
        exploration->GenerateDiscoveries(discoverySeed);

        // Initialize session manager
        if (!sessionManager->Initialize(fogOfWar.get(), exploration.get())) {
            return false;
        }

        // Initialize minimap
        if (!minimap->Initialize(fogOfWar.get(), mapWidth, mapHeight, tileSize)) {
            return false;
        }

        return true;
    }

    /**
     * @brief Shutdown all systems
     */
    void Shutdown() {
        if (minimap) minimap->Shutdown();
        if (sessionManager) sessionManager->Shutdown();
        if (exploration) exploration->Shutdown();
        if (fogOfWar) fogOfWar->Shutdown();
        if (visionSources) visionSources->Clear();
    }

    /**
     * @brief Update all systems
     */
    void Update(float deltaTime) {
        // Update environment
        environment.Update(deltaTime);

        // Update vision sources
        if (visionSources) {
            visionSources->Update(deltaTime);
        }

        // Update fog of war
        if (fogOfWar && visionSources) {
            fogOfWar->UpdateVision(visionSources->GetSources(), environment, deltaTime);
            fogOfWar->UpdateRendering(deltaTime);
        }

        // Update exploration
        if (exploration) {
            exploration->Update(deltaTime);
        }

        // Update session
        if (sessionManager) {
            sessionManager->Update(deltaTime);
        }

        // Update minimap
        if (minimap) {
            minimap->Update(deltaTime);
        }
    }

    /**
     * @brief Start a new game session
     */
    void StartSession() {
        if (sessionManager) {
            sessionManager->StartSession();
        }
    }

    /**
     * @brief Record player activity
     */
    void RecordActivity() {
        if (sessionManager) {
            sessionManager->RecordActivity();
        }
    }

    /**
     * @brief Add a vision source
     */
    void AddVisionSource(const VisionSource& source) {
        if (visionSources) {
            visionSources->AddSource(source);
        }
    }

    /**
     * @brief Remove vision sources for an entity
     */
    void RemoveVisionSource(uint32_t entityId) {
        if (visionSources) {
            visionSources->RemoveByOwner(entityId);
        }
    }

    /**
     * @brief Update a vision source position
     */
    void UpdateVisionSourcePosition(uint32_t entityId, const glm::vec2& position) {
        if (visionSources) {
            visionSources->UpdatePosition(entityId, position);
        }
    }

    /**
     * @brief Render the minimap
     */
    void RenderMinimap() {
        if (minimap) {
            minimap->Render();
        }
    }

    /**
     * @brief Get fog texture for main rendering
     */
    uint32_t GetFogTexture() const {
        return fogOfWar ? fogOfWar->GetFogTexture() : 0;
    }

    /**
     * @brief Get combined fog + lighting texture
     */
    uint32_t GetCombinedTexture() const {
        return fogOfWar ? fogOfWar->GetCombinedTexture() : 0;
    }

    /**
     * @brief Get current session stats
     */
    const SessionStats* GetSessionStats() const {
        return sessionManager ? &sessionManager->GetCurrentSessionStats() : nullptr;
    }

    /**
     * @brief Get exploration percentage
     */
    float GetExplorationPercent() const {
        return exploration ? exploration->GetExplorationPercent() : 0.0f;
    }
};

} // namespace RTS
} // namespace Vehement2
