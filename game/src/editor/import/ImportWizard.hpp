#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <glm/glm.hpp>

namespace Nova {
class AssetProcessor;
class ImportProgress;
class ImportProgressTracker;
class Texture;
}

namespace Vehement {

// Forward declarations
class TextureImportPanel;
class ModelImportPanel;
class AnimationImportPanel;

/**
 * @brief Import queue entry
 */
struct ImportQueueEntry {
    std::string filePath;
    std::string fileName;
    std::string assetType;     ///< Texture, Model, Animation
    size_t fileSize = 0;
    bool selected = true;
    bool imported = false;
    bool failed = false;
    float progress = 0.0f;
    std::string statusMessage;
};

/**
 * @brief Import wizard for drag-drop asset importing
 *
 * Features:
 * - Drag-drop import zone
 * - File type auto-detection
 * - Preview before import
 * - Per-file settings adjustment
 * - Batch import queue
 * - Progress display
 */
class ImportWizard {
public:
    /**
     * @brief Wizard configuration
     */
    struct Config {
        glm::vec2 position{100.0f, 100.0f};
        glm::vec2 size{800.0f, 600.0f};
        bool showPreview = true;
        bool showSettings = true;
        bool autoStartImport = false;
        int maxQueueDisplay = 50;
    };

    /**
     * @brief Import wizard state
     */
    enum class State {
        Idle,           ///< Waiting for files
        FilesQueued,    ///< Files added, ready to configure
        Importing,      ///< Import in progress
        Completed,      ///< Import finished
        Error           ///< Import failed
    };

public:
    ImportWizard();
    ~ImportWizard();

    // Non-copyable
    ImportWizard(const ImportWizard&) = delete;
    ImportWizard& operator=(const ImportWizard&) = delete;

    /**
     * @brief Initialize the import wizard
     */
    void Initialize(Nova::AssetProcessor* processor, const Config& config = {});

    /**
     * @brief Shutdown
     */
    void Shutdown();

    /**
     * @brief Update wizard state
     */
    void Update(float deltaTime);

    /**
     * @brief Render the wizard UI
     */
    void Render();

    // -------------------------------------------------------------------------
    // Visibility
    // -------------------------------------------------------------------------

    /**
     * @brief Show the wizard
     */
    void Show();

    /**
     * @brief Hide the wizard
     */
    void Hide();

    /**
     * @brief Toggle visibility
     */
    void Toggle();

    /**
     * @brief Check if visible
     */
    [[nodiscard]] bool IsVisible() const { return m_visible; }

    // -------------------------------------------------------------------------
    // File Handling
    // -------------------------------------------------------------------------

    /**
     * @brief Add files to import queue
     */
    void AddFiles(const std::vector<std::string>& paths);

    /**
     * @brief Add single file
     */
    void AddFile(const std::string& path);

    /**
     * @brief Clear the queue
     */
    void ClearQueue();

    /**
     * @brief Remove file from queue
     */
    void RemoveFile(size_t index);

    /**
     * @brief Get queue size
     */
    [[nodiscard]] size_t GetQueueSize() const { return m_queue.size(); }

    /**
     * @brief Get queue entries
     */
    [[nodiscard]] const std::vector<ImportQueueEntry>& GetQueue() const { return m_queue; }

    // -------------------------------------------------------------------------
    // Import Control
    // -------------------------------------------------------------------------

    /**
     * @brief Start importing queued files
     */
    void StartImport();

    /**
     * @brief Cancel ongoing import
     */
    void CancelImport();

    /**
     * @brief Check if import is in progress
     */
    [[nodiscard]] bool IsImporting() const { return m_state == State::Importing; }

    /**
     * @brief Get overall progress (0-1)
     */
    [[nodiscard]] float GetOverallProgress() const;

    /**
     * @brief Get current state
     */
    [[nodiscard]] State GetState() const { return m_state; }

    // -------------------------------------------------------------------------
    // Selection
    // -------------------------------------------------------------------------

    /**
     * @brief Select file in queue
     */
    void SelectFile(size_t index);

    /**
     * @brief Get selected file index
     */
    [[nodiscard]] int GetSelectedIndex() const { return m_selectedIndex; }

    /**
     * @brief Select all files
     */
    void SelectAll();

    /**
     * @brief Deselect all files
     */
    void DeselectAll();

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using ImportStartedCallback = std::function<void()>;
    using ImportCompletedCallback = std::function<void(size_t succeeded, size_t failed)>;
    using FileImportedCallback = std::function<void(const std::string& path, bool success)>;

    void SetImportStartedCallback(ImportStartedCallback cb) { m_onImportStarted = cb; }
    void SetImportCompletedCallback(ImportCompletedCallback cb) { m_onImportCompleted = cb; }
    void SetFileImportedCallback(FileImportedCallback cb) { m_onFileImported = cb; }

    // -------------------------------------------------------------------------
    // Input
    // -------------------------------------------------------------------------

    /**
     * @brief Handle file drop
     */
    void OnFileDrop(const std::vector<std::string>& paths);

    /**
     * @brief Handle mouse click
     */
    bool OnMouseClick(const glm::vec2& pos, int button);

    /**
     * @brief Handle mouse move
     */
    void OnMouseMove(const glm::vec2& pos);

    /**
     * @brief Check if position is over wizard
     */
    [[nodiscard]] bool IsOverWizard(const glm::vec2& pos) const;

    /**
     * @brief Check if dragging over drop zone
     */
    [[nodiscard]] bool IsOverDropZone(const glm::vec2& pos) const;

private:
    // Rendering helpers
    void RenderDropZone();
    void RenderQueueList();
    void RenderPreviewPanel();
    void RenderSettingsPanel();
    void RenderProgressBar();
    void RenderButtons();

    // Update helpers
    void UpdateImportProgress();
    void ProcessNextFile();

    // File type detection
    std::string DetectFileType(const std::string& path);
    std::string GetFileIcon(const std::string& type);
    glm::vec4 GetFileColor(const std::string& type);

    // Layout helpers
    glm::vec2 GetDropZoneBounds() const;
    glm::vec2 GetQueueListBounds() const;
    glm::vec2 GetPreviewBounds() const;
    glm::vec2 GetSettingsBounds() const;

    bool m_initialized = false;
    bool m_visible = false;
    Config m_config;
    State m_state = State::Idle;

    // Asset processor
    Nova::AssetProcessor* m_processor = nullptr;
    std::unique_ptr<Nova::ImportProgressTracker> m_progressTracker;

    // Queue
    std::vector<ImportQueueEntry> m_queue;
    int m_selectedIndex = -1;
    size_t m_currentImportIndex = 0;

    // Import panels
    std::unique_ptr<TextureImportPanel> m_texturePanel;
    std::unique_ptr<ModelImportPanel> m_modelPanel;
    std::unique_ptr<AnimationImportPanel> m_animationPanel;

    // Preview
    std::shared_ptr<Nova::Texture> m_previewTexture;
    bool m_previewDirty = true;

    // UI state
    bool m_isDraggingOver = false;
    glm::vec2 m_mousePos{0.0f};
    float m_dragHighlight = 0.0f;
    float m_progressAnimation = 0.0f;

    // Callbacks
    ImportStartedCallback m_onImportStarted;
    ImportCompletedCallback m_onImportCompleted;
    FileImportedCallback m_onFileImported;

    // Statistics
    size_t m_importedCount = 0;
    size_t m_failedCount = 0;
};

} // namespace Vehement
