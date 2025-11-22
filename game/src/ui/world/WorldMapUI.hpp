#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include "../../rts/world/WorldMap.hpp"

namespace Vehement {

/**
 * @brief Map display mode
 */
enum class MapDisplayMode : uint8_t {
    Globe3D,        // 3D globe view
    Flat2D,         // Traditional 2D map
    Satellite,      // Satellite imagery
    Political,      // Region boundaries
    Terrain,        // Terrain/elevation
    Faction,        // Faction control overlay
    Resources,      // Resource distribution
    Portals         // Portal network view
};

/**
 * @brief Map marker type
 */
enum class MapMarkerType : uint8_t {
    Player,
    Ally,
    Enemy,
    Portal,
    Quest,
    Resource,
    Battle,
    Event,
    POI,
    Custom
};

/**
 * @brief Map marker data
 */
struct MapMarker {
    std::string id;
    std::string label;
    MapMarkerType type = MapMarkerType::Custom;
    Geo::GeoCoordinate location;
    glm::vec4 color{1.0f, 1.0f, 1.0f, 1.0f};
    float size = 1.0f;
    std::string iconPath;
    bool visible = true;
    bool clickable = true;
    bool pulsing = false;
    std::string tooltip;
    nlohmann::json customData;

    [[nodiscard]] nlohmann::json ToJson() const;
    static MapMarker FromJson(const nlohmann::json& j);
};

/**
 * @brief Map region highlight
 */
struct RegionHighlight {
    std::string regionId;
    glm::vec4 fillColor{0.5f, 0.5f, 1.0f, 0.3f};
    glm::vec4 borderColor{0.5f, 0.5f, 1.0f, 1.0f};
    float borderWidth = 2.0f;
    bool pulsing = false;
    std::string label;
};

/**
 * @brief Portal connection line
 */
struct PortalLine {
    std::string portalId;
    Geo::GeoCoordinate start;
    Geo::GeoCoordinate end;
    glm::vec4 color{0.8f, 0.8f, 1.0f, 0.7f};
    float width = 2.0f;
    bool animated = true;
    bool bidirectional = true;
};

/**
 * @brief Map view state
 */
struct MapViewState {
    Geo::GeoCoordinate center;
    float zoom = 1.0f;
    float rotation = 0.0f;  // For globe view
    float tilt = 0.0f;      // For 3D views
    MapDisplayMode mode = MapDisplayMode::Political;
    bool showGrid = false;
    bool showLabels = true;
    bool showPortals = true;
    bool showPlayers = true;
    bool showFactionColors = true;
};

/**
 * @brief World map UI configuration
 */
struct WorldMapUIConfig {
    float animationSpeed = 2.0f;
    float zoomSpeed = 0.1f;
    float panSpeed = 0.01f;
    float minZoom = 0.5f;
    float maxZoom = 20.0f;
    bool enableMinimap = true;
    glm::vec2 minimapSize{200.0f, 200.0f};
    glm::vec2 minimapPosition{10.0f, 10.0f};
    bool enableSearch = true;
    bool enableFilters = true;
    bool enableLegend = true;
};

/**
 * @brief World map user interface
 */
class WorldMapUI {
public:
    using RegionClickCallback = std::function<void(const std::string& regionId)>;
    using PortalClickCallback = std::function<void(const std::string& portalId)>;
    using MarkerClickCallback = std::function<void(const MapMarker& marker)>;
    using ViewChangedCallback = std::function<void(const MapViewState& state)>;

    [[nodiscard]] static WorldMapUI& Instance();

    WorldMapUI(const WorldMapUI&) = delete;
    WorldMapUI& operator=(const WorldMapUI&) = delete;

    [[nodiscard]] bool Initialize(const WorldMapUIConfig& config = {});
    void Shutdown();
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    void Update(float deltaTime);
    void Render();

    // ==================== View Control ====================

    void SetViewState(const MapViewState& state);
    [[nodiscard]] MapViewState GetViewState() const;

    void SetDisplayMode(MapDisplayMode mode);
    [[nodiscard]] MapDisplayMode GetDisplayMode() const { return m_viewState.mode; }

    void SetCenter(const Geo::GeoCoordinate& center);
    void SetZoom(float zoom);
    void Pan(float dx, float dy);
    void ZoomIn();
    void ZoomOut();

    void FocusOnRegion(const std::string& regionId);
    void FocusOnCoordinate(const Geo::GeoCoordinate& coord);
    void FocusOnPlayer(const std::string& playerId);

    void ToggleFullscreen();
    [[nodiscard]] bool IsFullscreen() const { return m_fullscreen; }

    // ==================== Markers ====================

    void AddMarker(const MapMarker& marker);
    void RemoveMarker(const std::string& markerId);
    void UpdateMarker(const MapMarker& marker);
    void ClearMarkers();

    [[nodiscard]] std::vector<MapMarker> GetMarkers() const;
    [[nodiscard]] const MapMarker* GetMarker(const std::string& markerId) const;

    void SetMarkerVisibility(const std::string& markerId, bool visible);
    void SetMarkerTypeVisibility(MapMarkerType type, bool visible);

    // ==================== Region Highlighting ====================

    void HighlightRegion(const RegionHighlight& highlight);
    void ClearRegionHighlight(const std::string& regionId);
    void ClearAllHighlights();

    void SetSelectedRegion(const std::string& regionId);
    [[nodiscard]] std::string GetSelectedRegion() const { return m_selectedRegionId; }

    // ==================== Portal Lines ====================

    void ShowPortalLines(bool show);
    void HighlightPortalPath(const std::vector<std::string>& portalIds);
    void ClearPortalPathHighlight();

    // ==================== Filters ====================

    void SetFilter(const std::string& filterName, bool enabled);
    [[nodiscard]] bool IsFilterEnabled(const std::string& filterName) const;

    void SetMinDangerLevel(int level);
    void SetMaxDangerLevel(int level);
    void SetFactionFilter(int factionId, bool show);
    void ShowDiscoveredOnly(bool discoveredOnly);

    // ==================== Search ====================

    [[nodiscard]] std::vector<std::string> SearchRegions(const std::string& query) const;
    [[nodiscard]] std::vector<std::string> SearchPortals(const std::string& query) const;

    // ==================== Legend ====================

    void ShowLegend(bool show);
    [[nodiscard]] bool IsLegendVisible() const { return m_showLegend; }

    // ==================== Minimap ====================

    void ShowMinimap(bool show);
    [[nodiscard]] bool IsMinimapVisible() const { return m_showMinimap; }
    void SetMinimapPosition(const glm::vec2& position);
    void SetMinimapSize(const glm::vec2& size);

    // ==================== Input Handling ====================

    void OnMouseClick(float x, float y, int button);
    void OnMouseMove(float x, float y);
    void OnMouseScroll(float delta);
    void OnKeyPress(int key);

    // ==================== Callbacks ====================

    void OnRegionClicked(RegionClickCallback callback);
    void OnPortalClicked(PortalClickCallback callback);
    void OnMarkerClicked(MarkerClickCallback callback);
    void OnViewChanged(ViewChangedCallback callback);

    // ==================== JS Bridge (for HTML UI) ====================

    [[nodiscard]] nlohmann::json GetMapDataForJS() const;
    void HandleJSCommand(const std::string& command, const nlohmann::json& params);

    // ==================== Configuration ====================

    [[nodiscard]] const WorldMapUIConfig& GetConfig() const { return m_config; }

private:
    WorldMapUI() = default;
    ~WorldMapUI() = default;

    void UpdateMarkerPositions();
    void UpdatePortalLines();
    void UpdateRegionData();
    Geo::GeoCoordinate ScreenToGeo(float x, float y) const;
    glm::vec2 GeoToScreen(const Geo::GeoCoordinate& coord) const;

    bool m_initialized = false;
    WorldMapUIConfig m_config;

    MapViewState m_viewState;
    bool m_fullscreen = false;
    bool m_showLegend = true;
    bool m_showMinimap = true;

    std::string m_selectedRegionId;
    std::string m_hoveredRegionId;

    // Markers
    std::unordered_map<std::string, MapMarker> m_markers;
    std::unordered_map<MapMarkerType, bool> m_markerTypeVisibility;
    mutable std::mutex m_markersMutex;

    // Highlights
    std::unordered_map<std::string, RegionHighlight> m_regionHighlights;
    std::vector<std::string> m_highlightedPortalPath;

    // Portal lines
    std::vector<PortalLine> m_portalLines;
    bool m_showPortalLines = true;

    // Filters
    std::unordered_map<std::string, bool> m_filters;
    int m_minDangerFilter = 0;
    int m_maxDangerFilter = 10;
    std::unordered_set<int> m_hiddenFactions;
    bool m_discoveredOnly = false;

    // Callbacks
    std::vector<RegionClickCallback> m_regionClickCallbacks;
    std::vector<PortalClickCallback> m_portalClickCallbacks;
    std::vector<MarkerClickCallback> m_markerClickCallbacks;
    std::vector<ViewChangedCallback> m_viewChangedCallbacks;
    std::mutex m_callbackMutex;

    // Screen state
    glm::vec2 m_screenSize{1920.0f, 1080.0f};
    glm::vec2 m_lastMousePos{0.0f, 0.0f};
    bool m_dragging = false;
};

} // namespace Vehement
