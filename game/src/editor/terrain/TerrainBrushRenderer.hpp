#pragma once

#include "TerrainEditor.hpp"
#include "../../../../engine/graphics/Renderer.hpp"
#include "../../../../engine/graphics/Mesh.hpp"
#include "../../../../engine/graphics/Shader.hpp"
#include <glm/glm.hpp>
#include <memory>
#include <vector>

namespace Vehement {

/**
 * @brief Visual brush preview rendering for terrain editing
 *
 * Renders a real-time preview of the terrain brush showing:
 * - Brush shape outline
 * - Affected area visualization
 * - Strength gradient
 * - Height change preview
 * - Multi-user brush cursors
 */
class TerrainBrushRenderer {
public:
    struct Config {
        bool showBrushOutline = true;
        bool showStrengthGradient = true;
        bool showHeightPreview = true;
        bool showOtherPlayers = true;
        float outlineThickness = 2.0f;
        glm::vec4 brushColor{0.2f, 0.8f, 0.2f, 0.6f};
        glm::vec4 otherPlayerColor{0.8f, 0.2f, 0.2f, 0.6f};
        int previewResolution = 32;
    };

    TerrainBrushRenderer();
    ~TerrainBrushRenderer();

    /**
     * @brief Initialize renderer
     */
    bool Initialize(Nova::Renderer& renderer);

    /**
     * @brief Shutdown renderer
     */
    void Shutdown();

    /**
     * @brief Render brush preview
     */
    void RenderBrushPreview(const TerrainEditor& editor, const glm::mat4& viewProjection);

    /**
     * @brief Render other player brushes
     */
    void RenderOtherPlayerBrushes(const std::unordered_map<uint32_t, glm::vec3>& otherEditors,
                                   const TerrainBrush& brush,
                                   const glm::mat4& viewProjection);

    /**
     * @brief Set configuration
     */
    void SetConfig(const Config& config) { m_config = config; }
    [[nodiscard]] const Config& GetConfig() const { return m_config; }

    /**
     * @brief Set brush color
     */
    void SetBrushColor(const glm::vec4& color) { m_config.brushColor = color; }

private:
    // Generate preview meshes
    void GenerateCircleOutline(float radius, std::vector<glm::vec3>& vertices);
    void GenerateSquareOutline(float size, std::vector<glm::vec3>& vertices);
    void GenerateDiamondOutline(float size, std::vector<glm::vec3>& vertices);
    void GenerateStrengthGradient(const TerrainBrush& brush, std::vector<glm::vec3>& vertices, std::vector<glm::vec4>& colors);
    void GenerateHeightPreview(const TerrainEditor& editor, std::vector<glm::vec3>& vertices, std::vector<glm::vec3>& normals);

    // Render helpers
    void RenderOutline(const std::vector<glm::vec3>& vertices, const glm::vec3& position, const glm::vec4& color, const glm::mat4& viewProjection);
    void RenderGradient(const std::vector<glm::vec3>& vertices, const std::vector<glm::vec4>& colors, const glm::vec3& position, const glm::mat4& viewProjection);
    void RenderHeightPreview(const std::vector<glm::vec3>& vertices, const std::vector<glm::vec3>& normals, const glm::vec3& position, const glm::mat4& viewProjection);

    Config m_config;
    Nova::Renderer* m_renderer = nullptr;

    // Shaders
    std::shared_ptr<Nova::Shader> m_outlineShader;
    std::shared_ptr<Nova::Shader> m_gradientShader;
    std::shared_ptr<Nova::Shader> m_previewShader;

    // Meshes for different brush shapes
    std::shared_ptr<Nova::Mesh> m_circleMesh;
    std::shared_ptr<Nova::Mesh> m_squareMesh;
    std::shared_ptr<Nova::Mesh> m_diamondMesh;
    std::shared_ptr<Nova::Mesh> m_gradientMesh;
    std::shared_ptr<Nova::Mesh> m_previewMesh;

    bool m_initialized = false;
};

/**
 * @brief Multi-user terrain editing visualization
 *
 * Shows cursors and previews for other players editing the terrain
 */
class MultiUserEditVisualization {
public:
    struct PlayerCursor {
        uint32_t playerId;
        std::string playerName;
        glm::vec3 position;
        TerrainBrush brush;
        glm::vec4 color;
        float lastUpdateTime = 0.0f;
    };

    /**
     * @brief Update player cursor
     */
    void UpdatePlayerCursor(uint32_t playerId, const std::string& name, const glm::vec3& position, const TerrainBrush& brush);

    /**
     * @brief Remove player cursor
     */
    void RemovePlayerCursor(uint32_t playerId);

    /**
     * @brief Get all active cursors
     */
    [[nodiscard]] const std::vector<PlayerCursor>& GetActiveCursors() const { return m_activeCursors; }

    /**
     * @brief Update (remove stale cursors)
     */
    void Update(float deltaTime, float timeout = 2.0f);

    /**
     * @brief Render all player cursors
     */
    void Render(TerrainBrushRenderer& renderer, const glm::mat4& viewProjection);

private:
    std::vector<PlayerCursor> m_activeCursors;
    float m_currentTime = 0.0f;
};

} // namespace Vehement
