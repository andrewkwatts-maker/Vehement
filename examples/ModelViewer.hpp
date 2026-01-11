#pragma once

#include "AssetEditor.hpp"
#include <imgui.h>
#include <glm/glm.hpp>
#include <vector>

/**
 * @brief 3D Model viewer and editor
 *
 * Features:
 * - 3D preview with orbit camera controls
 * - Display model statistics (vertices, triangles, materials)
 * - Material override and visualization
 * - Skeleton/bone visualization
 * - LOD level switching
 * - Bounding box display
 * - Wireframe/solid toggle
 * - Normal visualization
 */
class ModelViewer : public IAssetEditor {
public:
    ModelViewer();
    ~ModelViewer() override;

    void Open(const std::string& assetPath) override;
    void Render(bool* isOpen) override;
    std::string GetEditorName() const override;
    bool IsDirty() const override;
    void Save() override;
    std::string GetAssetPath() const override;

private:
    void RenderViewport();
    void RenderToolbar();
    void RenderProperties();
    void RenderMaterialList();
    void LoadModel();
    void UpdateCamera();

    std::string m_assetPath;
    std::string m_modelName;
    bool m_isDirty = false;
    bool m_isLoaded = false;

    // Model statistics
    int m_vertexCount = 0;
    int m_triangleCount = 0;
    int m_materialCount = 0;
    int m_boneCount = 0;
    int m_lodCount = 1;

    // Display options
    bool m_showWireframe = false;
    bool m_showNormals = false;
    bool m_showBounds = true;
    bool m_showSkeleton = false;
    bool m_showGrid = true;
    int m_currentLod = 0;

    // Camera controls
    glm::vec3 m_cameraPosition{5.0f, 3.0f, 5.0f};
    glm::vec3 m_cameraTarget{0.0f, 0.0f, 0.0f};
    float m_cameraDistance = 10.0f;
    float m_cameraYaw = 45.0f;
    float m_cameraPitch = 30.0f;
    float m_cameraFov = 60.0f;

    // Model transform
    glm::vec3 m_modelRotation{0.0f, 0.0f, 0.0f};
    bool m_autoRotate = false;
    float m_autoRotateSpeed = 30.0f; // degrees per second

    // Bounds
    glm::vec3 m_boundsMin{-1.0f, -1.0f, -1.0f};
    glm::vec3 m_boundsMax{1.0f, 1.0f, 1.0f};

    // Materials
    struct MaterialInfo {
        std::string name;
        std::string diffuseTexture;
        std::string normalTexture;
        glm::vec3 color{1.0f, 1.0f, 1.0f};
    };
    std::vector<MaterialInfo> m_materials;

    // UI state
    bool m_isDraggingCamera = false;
    ImVec2 m_lastMousePos{0.0f, 0.0f};
};
