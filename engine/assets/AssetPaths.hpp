#pragma once

/**
 * @file AssetPaths.hpp
 * @brief Platform-specific asset path resolution
 *
 * Provides unified asset loading across all platforms:
 * - Android: APK assets folder via AAssetManager
 * - iOS: App bundle resources
 * - Linux/Windows/macOS: Relative filesystem paths
 *
 * Supports asset packs for downloadable content and expansion files.
 */

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cstdint>
#include <unordered_map>

namespace Nova {

// ============================================================================
// Asset Types
// ============================================================================

/**
 * @brief Asset category for organized storage
 */
enum class AssetCategory {
    General,        // Generic assets
    Textures,       // Image files
    Models,         // 3D models
    Shaders,        // Shader programs
    Audio,          // Sound effects and music
    Fonts,          // Font files
    Scripts,        // Game scripts
    Levels,         // Level/scene data
    Localization,   // Language files
    Config,         // Configuration files
    UserData        // User-generated content
};

/**
 * @brief Asset source/location
 */
enum class AssetSource {
    Bundle,         // Main application bundle (APK/App/executable)
    Expansion,      // Expansion pack (Android OBB, iOS On-Demand Resources)
    Downloaded,     // Downloaded at runtime
    UserCreated,    // User-created content
    Cache           // Cached/processed assets
};

/**
 * @brief Asset pack information
 */
struct AssetPack {
    std::string name;
    std::string path;
    AssetSource source = AssetSource::Bundle;
    uint64_t size = 0;
    uint32_t version = 0;
    bool isLoaded = false;
    bool isRequired = true;
};

/**
 * @brief Asset loading progress callback
 */
using AssetProgressCallback = std::function<void(const std::string& assetPath,
                                                   float progress,
                                                   uint64_t bytesLoaded,
                                                   uint64_t totalBytes)>;

// ============================================================================
// Asset Path Manager
// ============================================================================

/**
 * @brief Cross-platform asset path resolution and loading
 *
 * Handles platform differences in asset storage:
 *
 * Android:
 *   - Main assets: APK assets/ folder (read via AAssetManager)
 *   - Expansion: OBB files in external storage
 *   - Cache: Internal cache directory
 *
 * iOS:
 *   - Main assets: App bundle root
 *   - Expansion: On-Demand Resources
 *   - Cache: Library/Caches
 *
 * Desktop (Windows/Linux/macOS):
 *   - Main assets: <executable>/assets/
 *   - Expansion: <executable>/dlc/
 *   - Cache: Platform-specific cache directory
 */
class AssetPaths {
public:
    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize asset system
     *
     * On Android, this requires the AAssetManager from JNI.
     * On iOS, this configures bundle resource access.
     */
    static bool Initialize();

    /**
     * @brief Initialize with Android asset manager
     * @param assetManager AAssetManager pointer from JNI
     */
    static bool Initialize(void* assetManager);

    /**
     * @brief Shutdown asset system
     */
    static void Shutdown();

    /**
     * @brief Check if initialized
     */
    static bool IsInitialized();

    // =========================================================================
    // Path Resolution
    // =========================================================================

    /**
     * @brief Resolve asset path to platform-specific path
     *
     * @param relativePath Relative path (e.g., "textures/player.png")
     * @param category Asset category (affects search paths)
     * @return Platform-specific path, or empty if not found
     *
     * Search order:
     * 1. User data directory (for modding support)
     * 2. Downloaded content
     * 3. Expansion packs
     * 4. Main bundle
     */
    static std::string Resolve(const std::string& relativePath,
                                AssetCategory category = AssetCategory::General);

    /**
     * @brief Get base path for asset category
     *
     * @param category Asset category
     * @return Base directory path
     */
    static std::string GetCategoryPath(AssetCategory category);

    /**
     * @brief Get full path for an asset
     *
     * @param category Asset category
     * @param filename Asset filename
     * @return Full resolved path
     */
    static std::string GetAssetPath(AssetCategory category, const std::string& filename);

    // =========================================================================
    // Platform-Specific Paths
    // =========================================================================

    /**
     * @brief Get main assets directory
     *
     * Platform paths:
     * - Android: "assets://" (virtual, use AssetManager)
     * - iOS: [[NSBundle mainBundle] resourcePath]
     * - Desktop: <executable>/assets/
     */
    static std::string GetAssetsRoot();

    /**
     * @brief Get expansion/DLC assets directory
     *
     * Platform paths:
     * - Android: External storage OBB path
     * - iOS: On-Demand Resources path
     * - Desktop: <executable>/dlc/
     */
    static std::string GetExpansionRoot();

    /**
     * @brief Get writable data directory
     *
     * Platform paths:
     * - Android: Internal files directory
     * - iOS: Documents directory
     * - Desktop: AppData/local share
     */
    static std::string GetDataRoot();

    /**
     * @brief Get cache directory
     */
    static std::string GetCacheRoot();

    // =========================================================================
    // Asset Loading
    // =========================================================================

    /**
     * @brief Load asset data
     *
     * Handles platform-specific loading:
     * - Android: AAsset_read
     * - iOS: NSBundle/NSData
     * - Desktop: Standard file I/O
     *
     * @param path Relative asset path
     * @return Asset data, empty on failure
     */
    static std::vector<uint8_t> LoadAsset(const std::string& path);

    /**
     * @brief Load asset as string
     *
     * @param path Relative asset path
     * @return Asset contents as UTF-8 string
     */
    static std::string LoadAssetText(const std::string& path);

    /**
     * @brief Check if asset exists
     *
     * @param path Relative asset path
     * @return true if asset exists
     */
    static bool AssetExists(const std::string& path);

    /**
     * @brief Get asset size
     *
     * @param path Relative asset path
     * @return Asset size in bytes, 0 if not found
     */
    static uint64_t GetAssetSize(const std::string& path);

    /**
     * @brief List assets in directory
     *
     * @param directory Relative directory path
     * @param recursive Include subdirectories
     * @return List of asset paths
     */
    static std::vector<std::string> ListAssets(const std::string& directory,
                                                 bool recursive = false);

    /**
     * @brief List assets matching pattern
     *
     * @param directory Directory to search
     * @param pattern Glob pattern (e.g., "*.png")
     * @return List of matching asset paths
     */
    static std::vector<std::string> ListAssets(const std::string& directory,
                                                 const std::string& pattern);

    // =========================================================================
    // Asset Packs
    // =========================================================================

    /**
     * @brief Register an asset pack
     *
     * @param pack Asset pack information
     */
    static void RegisterAssetPack(const AssetPack& pack);

    /**
     * @brief Load an asset pack
     *
     * For Android OBB files or iOS On-Demand Resources.
     *
     * @param packName Pack name
     * @param callback Progress callback
     * @return true if pack loaded successfully
     */
    static bool LoadAssetPack(const std::string& packName,
                               AssetProgressCallback callback = nullptr);

    /**
     * @brief Unload an asset pack
     *
     * @param packName Pack name
     */
    static void UnloadAssetPack(const std::string& packName);

    /**
     * @brief Check if asset pack is loaded
     *
     * @param packName Pack name
     * @return true if pack is loaded
     */
    static bool IsAssetPackLoaded(const std::string& packName);

    /**
     * @brief Get all registered asset packs
     */
    static std::vector<AssetPack> GetAssetPacks();

    // =========================================================================
    // Android-Specific
    // =========================================================================

#ifdef NOVA_PLATFORM_ANDROID
    /**
     * @brief Set Android asset manager (from JNI)
     *
     * @param assetManager AAssetManager pointer
     */
    static void SetAssetManager(void* assetManager);

    /**
     * @brief Get Android asset manager
     *
     * @return AAssetManager pointer
     */
    static void* GetAssetManager();

    /**
     * @brief Set OBB file path (for expansion files)
     *
     * @param mainObbPath Path to main.obb
     * @param patchObbPath Path to patch.obb (optional)
     */
    static void SetObbPaths(const std::string& mainObbPath,
                             const std::string& patchObbPath = "");

    /**
     * @brief Open asset from APK
     *
     * @param path Asset path within APK
     * @return AAsset pointer (must be closed with AAsset_close)
     */
    static void* OpenAsset(const std::string& path);
#endif

    // =========================================================================
    // iOS-Specific
    // =========================================================================

#ifdef NOVA_PLATFORM_IOS
    /**
     * @brief Request On-Demand Resource
     *
     * @param resourceTag Resource tag
     * @param callback Completion callback
     */
    static void RequestOnDemandResource(const std::string& resourceTag,
                                         std::function<void(bool success)> callback);

    /**
     * @brief Check if On-Demand Resource is available
     *
     * @param resourceTag Resource tag
     * @return true if resource is downloaded
     */
    static bool IsOnDemandResourceAvailable(const std::string& resourceTag);
#endif

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Get category folder name
     *
     * @param category Asset category
     * @return Folder name (e.g., "textures", "models")
     */
    static std::string GetCategoryFolder(AssetCategory category);

    /**
     * @brief Normalize asset path (convert separators, remove ..)
     *
     * @param path Path to normalize
     * @return Normalized path
     */
    static std::string NormalizePath(const std::string& path);

    /**
     * @brief Join path components
     *
     * @param base Base path
     * @param path Path to append
     * @return Combined path
     */
    static std::string JoinPath(const std::string& base, const std::string& path);

    /**
     * @brief Get file extension from path
     *
     * @param path File path
     * @return Extension with dot (e.g., ".png")
     */
    static std::string GetExtension(const std::string& path);

    /**
     * @brief Check if path is absolute
     *
     * @param path Path to check
     * @return true if path is absolute
     */
    static bool IsAbsolutePath(const std::string& path);

private:
    static bool s_initialized;
    static std::string s_assetsRoot;
    static std::string s_expansionRoot;
    static std::string s_dataRoot;
    static std::string s_cacheRoot;
    static std::unordered_map<std::string, AssetPack> s_assetPacks;

#ifdef NOVA_PLATFORM_ANDROID
    static void* s_assetManager;
    static std::string s_mainObbPath;
    static std::string s_patchObbPath;
#endif
};

// ============================================================================
// Inline Category Folder Names
// ============================================================================

inline std::string AssetPaths::GetCategoryFolder(AssetCategory category) {
    switch (category) {
        case AssetCategory::General:      return "";
        case AssetCategory::Textures:     return "textures";
        case AssetCategory::Models:       return "models";
        case AssetCategory::Shaders:      return "shaders";
        case AssetCategory::Audio:        return "audio";
        case AssetCategory::Fonts:        return "fonts";
        case AssetCategory::Scripts:      return "scripts";
        case AssetCategory::Levels:       return "levels";
        case AssetCategory::Localization: return "localization";
        case AssetCategory::Config:       return "config";
        case AssetCategory::UserData:     return "userdata";
        default:                          return "";
    }
}

} // namespace Nova
