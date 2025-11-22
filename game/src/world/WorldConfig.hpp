#pragma once

#include <string>
#include <glm/glm.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

namespace Vehement {

/**
 * @brief Hexagonal grid orientation
 */
enum class HexOrientation : uint8_t {
    PointyTop,   // Pointy side up (default)
    FlatTop      // Flat side up
};

/**
 * @brief Extended world configuration for 3D voxel and hex grid support
 *
 * This configuration allows switching between hex and rectangular grids,
 * configuring voxel sizes for multi-story buildings, and setting world bounds.
 */
struct WorldConfig {
    // ========== Grid Type ==========

    bool useHexGrid = true;                                    // Use hex grid (true) or rectangular grid (false)
    HexOrientation hexOrientation = HexOrientation::PointyTop; // Hex orientation when using hex grid

    // ========== Tile/Voxel Sizes ==========

    float tileSizeXY = 1.0f;       // World meters per tile in X/Y directions
    float tileSizeZ = 0.333f;      // World meters per voxel in Z direction (1/3 of XY by default)
    float hexOuterRadius = 1.0f;   // Outer radius for hex tiles (distance from center to corner)

    // Derived hex dimensions (calculated from hexOuterRadius)
    float GetHexInnerRadius() const { return hexOuterRadius * 0.866025403784f; } // sqrt(3)/2
    float GetHexWidth() const {
        return hexOrientation == HexOrientation::PointyTop
            ? GetHexInnerRadius() * 2.0f
            : hexOuterRadius * 2.0f;
    }
    float GetHexHeight() const {
        return hexOrientation == HexOrientation::PointyTop
            ? hexOuterRadius * 2.0f
            : GetHexInnerRadius() * 2.0f;
    }

    // ========== World Bounds ==========

    int mapWidth = 256;            // Map width in tiles (X direction)
    int mapHeight = 256;           // Map height in tiles (Y direction for 2D, XY plane)
    int maxZLevels = 32;           // Maximum Z levels (vertical voxel layers)

    // ========== Rendering ==========

    int renderDistance = 64;       // Horizontal render distance in tiles
    int verticalRenderDistance = 8; // Vertical render distance (Z levels above/below camera)
    bool enableFrustumCulling = true;
    bool enableOcclusionCulling = true;
    float lodDistance1 = 32.0f;    // Distance for first LOD level
    float lodDistance2 = 64.0f;    // Distance for second LOD level

    // ========== Large Objects ==========

    glm::ivec3 maxObjectSize = {4, 4, 8}; // Maximum tiles an object can span (X, Y, Z)

    // ========== Legacy Compatibility ==========

    // These are kept for compatibility with the original World class
    float tileSize = 1.0f;         // Alias for tileSizeXY
    std::string textureBasePath = "Vehement2/images/";
    bool enableChunks = false;

    // ========== Serialization ==========

    /**
     * @brief Load configuration from a JSON file
     * @param path Path to the configuration file
     * @return Loaded configuration (defaults if file not found)
     */
    static WorldConfig LoadFromFile(const std::string& path) {
        WorldConfig config;

        std::ifstream file(path);
        if (!file.is_open()) {
            return config; // Return defaults
        }

        try {
            nlohmann::json json;
            file >> json;

            // Grid type
            if (json.contains("useHexGrid")) config.useHexGrid = json["useHexGrid"];
            if (json.contains("hexOrientation")) {
                std::string orient = json["hexOrientation"];
                config.hexOrientation = (orient == "FlatTop")
                    ? HexOrientation::FlatTop
                    : HexOrientation::PointyTop;
            }

            // Tile/voxel sizes
            if (json.contains("tileSizeXY")) config.tileSizeXY = json["tileSizeXY"];
            if (json.contains("tileSizeZ")) config.tileSizeZ = json["tileSizeZ"];
            if (json.contains("hexOuterRadius")) config.hexOuterRadius = json["hexOuterRadius"];

            // World bounds
            if (json.contains("mapWidth")) config.mapWidth = json["mapWidth"];
            if (json.contains("mapHeight")) config.mapHeight = json["mapHeight"];
            if (json.contains("maxZLevels")) config.maxZLevels = json["maxZLevels"];

            // Rendering
            if (json.contains("renderDistance")) config.renderDistance = json["renderDistance"];
            if (json.contains("verticalRenderDistance")) config.verticalRenderDistance = json["verticalRenderDistance"];
            if (json.contains("enableFrustumCulling")) config.enableFrustumCulling = json["enableFrustumCulling"];
            if (json.contains("enableOcclusionCulling")) config.enableOcclusionCulling = json["enableOcclusionCulling"];
            if (json.contains("lodDistance1")) config.lodDistance1 = json["lodDistance1"];
            if (json.contains("lodDistance2")) config.lodDistance2 = json["lodDistance2"];

            // Large objects
            if (json.contains("maxObjectSize")) {
                auto& arr = json["maxObjectSize"];
                if (arr.is_array() && arr.size() >= 3) {
                    config.maxObjectSize = glm::ivec3(arr[0], arr[1], arr[2]);
                }
            }

            // Legacy
            if (json.contains("tileSize")) config.tileSize = json["tileSize"];
            if (json.contains("textureBasePath")) config.textureBasePath = json["textureBasePath"];
            if (json.contains("enableChunks")) config.enableChunks = json["enableChunks"];

            // Sync tileSize with tileSizeXY
            config.tileSize = config.tileSizeXY;

        } catch (const std::exception& e) {
            // Return defaults on parse error
            return WorldConfig();
        }

        return config;
    }

    /**
     * @brief Save configuration to a JSON file
     * @param path Path to save the configuration
     */
    void SaveToFile(const std::string& path) const {
        nlohmann::json json;

        // Grid type
        json["useHexGrid"] = useHexGrid;
        json["hexOrientation"] = (hexOrientation == HexOrientation::FlatTop) ? "FlatTop" : "PointyTop";

        // Tile/voxel sizes
        json["tileSizeXY"] = tileSizeXY;
        json["tileSizeZ"] = tileSizeZ;
        json["hexOuterRadius"] = hexOuterRadius;

        // World bounds
        json["mapWidth"] = mapWidth;
        json["mapHeight"] = mapHeight;
        json["maxZLevels"] = maxZLevels;

        // Rendering
        json["renderDistance"] = renderDistance;
        json["verticalRenderDistance"] = verticalRenderDistance;
        json["enableFrustumCulling"] = enableFrustumCulling;
        json["enableOcclusionCulling"] = enableOcclusionCulling;
        json["lodDistance1"] = lodDistance1;
        json["lodDistance2"] = lodDistance2;

        // Large objects
        json["maxObjectSize"] = {maxObjectSize.x, maxObjectSize.y, maxObjectSize.z};

        // Legacy
        json["tileSize"] = tileSize;
        json["textureBasePath"] = textureBasePath;
        json["enableChunks"] = enableChunks;

        std::ofstream file(path);
        if (file.is_open()) {
            file << json.dump(2);
        }
    }

    /**
     * @brief Convert to JSON object
     */
    nlohmann::json ToJson() const {
        nlohmann::json json;

        json["useHexGrid"] = useHexGrid;
        json["hexOrientation"] = (hexOrientation == HexOrientation::FlatTop) ? "FlatTop" : "PointyTop";
        json["tileSizeXY"] = tileSizeXY;
        json["tileSizeZ"] = tileSizeZ;
        json["hexOuterRadius"] = hexOuterRadius;
        json["mapWidth"] = mapWidth;
        json["mapHeight"] = mapHeight;
        json["maxZLevels"] = maxZLevels;
        json["renderDistance"] = renderDistance;
        json["verticalRenderDistance"] = verticalRenderDistance;
        json["enableFrustumCulling"] = enableFrustumCulling;
        json["enableOcclusionCulling"] = enableOcclusionCulling;
        json["maxObjectSize"] = {maxObjectSize.x, maxObjectSize.y, maxObjectSize.z};

        return json;
    }

    /**
     * @brief Load from JSON object
     */
    void FromJson(const nlohmann::json& json) {
        *this = WorldConfig(); // Reset to defaults

        if (json.contains("useHexGrid")) useHexGrid = json["useHexGrid"];
        if (json.contains("hexOrientation")) {
            std::string orient = json["hexOrientation"];
            hexOrientation = (orient == "FlatTop") ? HexOrientation::FlatTop : HexOrientation::PointyTop;
        }
        if (json.contains("tileSizeXY")) tileSizeXY = json["tileSizeXY"];
        if (json.contains("tileSizeZ")) tileSizeZ = json["tileSizeZ"];
        if (json.contains("hexOuterRadius")) hexOuterRadius = json["hexOuterRadius"];
        if (json.contains("mapWidth")) mapWidth = json["mapWidth"];
        if (json.contains("mapHeight")) mapHeight = json["mapHeight"];
        if (json.contains("maxZLevels")) maxZLevels = json["maxZLevels"];
        if (json.contains("renderDistance")) renderDistance = json["renderDistance"];
        if (json.contains("verticalRenderDistance")) verticalRenderDistance = json["verticalRenderDistance"];
        if (json.contains("enableFrustumCulling")) enableFrustumCulling = json["enableFrustumCulling"];
        if (json.contains("enableOcclusionCulling")) enableOcclusionCulling = json["enableOcclusionCulling"];
        if (json.contains("maxObjectSize")) {
            auto& arr = json["maxObjectSize"];
            if (arr.is_array() && arr.size() >= 3) {
                maxObjectSize = glm::ivec3(arr[0], arr[1], arr[2]);
            }
        }

        tileSize = tileSizeXY;
    }
};

} // namespace Vehement
