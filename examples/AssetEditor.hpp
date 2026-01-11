#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>

/**
 * @brief Interface for all asset editors
 *
 * Each editor type implements this interface to provide:
 * - Asset loading
 * - UI rendering
 * - Save functionality
 * - Dirty state tracking
 */
class IAssetEditor {
public:
    virtual ~IAssetEditor() = default;

    /**
     * @brief Open an asset file
     * @param assetPath Absolute path to the asset
     */
    virtual void Open(const std::string& assetPath) = 0;

    /**
     * @brief Render the editor window
     * @param isOpen Pointer to open state (set to false to close window)
     */
    virtual void Render(bool* isOpen) = 0;

    /**
     * @brief Get the editor window name
     * @return Display name for the editor window
     */
    virtual std::string GetEditorName() const = 0;

    /**
     * @brief Check if the asset has unsaved changes
     * @return true if there are unsaved changes
     */
    virtual bool IsDirty() const = 0;

    /**
     * @brief Save the current asset
     */
    virtual void Save() = 0;

    /**
     * @brief Get the asset path being edited
     * @return Absolute path to the asset
     */
    virtual std::string GetAssetPath() const = 0;
};

/**
 * @brief Factory for creating asset editors based on file extension
 *
 * Singleton pattern for registering and creating editors.
 * Each file extension can be mapped to an editor creator function.
 */
class AssetEditorFactory {
public:
    using EditorCreator = std::function<std::unique_ptr<IAssetEditor>()>;

    /**
     * @brief Get the singleton instance
     */
    static AssetEditorFactory& Instance();

    /**
     * @brief Register an editor for a file extension
     * @param extension File extension (e.g., ".png", ".json")
     * @param creator Function that creates a new editor instance
     */
    void RegisterEditor(const std::string& extension, EditorCreator creator);

    /**
     * @brief Create an editor for the given asset
     * @param assetPath Path to the asset (extension determines editor type)
     * @return Editor instance or nullptr if no editor is registered
     */
    std::unique_ptr<IAssetEditor> CreateEditor(const std::string& assetPath);

    /**
     * @brief Check if an editor is registered for the extension
     * @param extension File extension to check
     * @return true if an editor is registered
     */
    bool HasEditor(const std::string& extension) const;

    /**
     * @brief Get all registered extensions
     * @return Vector of all registered file extensions
     */
    std::vector<std::string> GetRegisteredExtensions() const;

private:
    AssetEditorFactory() = default;
    std::unordered_map<std::string, EditorCreator> m_editors;

    // Helper to extract extension from path
    static std::string GetExtension(const std::string& path);
};

/**
 * @brief Helper macro to register an editor
 *
 * Usage:
 * REGISTER_ASSET_EDITOR(".png", TextureEditor);
 */
#define REGISTER_ASSET_EDITOR(ext, EditorClass) \
    AssetEditorFactory::Instance().RegisterEditor(ext, []() -> std::unique_ptr<IAssetEditor> { \
        return std::make_unique<EditorClass>(); \
    })
