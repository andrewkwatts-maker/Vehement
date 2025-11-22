#pragma once

/**
 * @file FileSystem.hpp
 * @brief Cross-platform file system abstraction
 *
 * Provides unified file system access across all platforms:
 * - Windows: AppData, Documents, Program Files
 * - Linux: XDG directories
 * - macOS: Application Support, Documents
 * - iOS: App sandbox (Documents, Caches)
 * - Android: Internal/external storage, APK assets
 */

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <optional>
#include <filesystem>

namespace Nova {

/**
 * @brief File access mode
 */
enum class FileAccessMode {
    Read,
    Write,
    ReadWrite,
    Append
};

/**
 * @brief File type enumeration
 */
enum class FileType {
    Unknown,
    Regular,
    Directory,
    Symlink,
    Other
};

/**
 * @brief File information structure
 */
struct FileInfo {
    std::string path;
    std::string name;
    FileType type = FileType::Unknown;
    uint64_t size = 0;
    uint64_t modifiedTime = 0;  // Unix timestamp
    uint64_t createdTime = 0;
    bool isReadOnly = false;
    bool isHidden = false;
};

/**
 * @brief Directory enumeration callback
 */
using DirectoryCallback = std::function<bool(const FileInfo& info)>;

/**
 * @brief Cross-platform file system interface
 *
 * All paths are UTF-8 encoded strings. Platform-specific path separators
 * are handled internally (use '/' for portability).
 */
class FileSystem {
public:
    // =========================================================================
    // Platform-Specific Paths
    // =========================================================================

    /**
     * @brief Get application data directory (writable)
     *
     * Platform paths:
     * - Windows: %APPDATA%/AppName/
     * - Linux: ~/.local/share/AppName/ (XDG_DATA_HOME)
     * - macOS: ~/Library/Application Support/AppName/
     * - iOS: <AppSandbox>/Documents/
     * - Android: Context.getFilesDir()
     */
    static std::string GetDataPath();

    /**
     * @brief Get cache/temporary directory
     *
     * Platform paths:
     * - Windows: %LOCALAPPDATA%/Temp/AppName/
     * - Linux: ~/.cache/AppName/ (XDG_CACHE_HOME)
     * - macOS: ~/Library/Caches/AppName/
     * - iOS: <AppSandbox>/Library/Caches/
     * - Android: Context.getCacheDir()
     */
    static std::string GetCachePath();

    /**
     * @brief Get user documents directory
     *
     * Platform paths:
     * - Windows: %USERPROFILE%/Documents/
     * - Linux: ~/Documents/ (XDG user-dirs)
     * - macOS: ~/Documents/
     * - iOS: <AppSandbox>/Documents/
     * - Android: Environment.getExternalStoragePublicDirectory(DIRECTORY_DOCUMENTS)
     */
    static std::string GetDocumentsPath();

    /**
     * @brief Get application bundle/executable directory
     *
     * Platform paths:
     * - Windows: Directory containing .exe
     * - Linux: Directory containing executable
     * - macOS: .app/Contents/MacOS/
     * - iOS: .app bundle
     * - Android: Not applicable (use GetAssetsPath)
     */
    static std::string GetBundlePath();

    /**
     * @brief Get assets/resources directory (read-only)
     *
     * Platform paths:
     * - Windows: <BundlePath>/assets/
     * - Linux: <BundlePath>/assets/ or /usr/share/AppName/
     * - macOS: .app/Contents/Resources/
     * - iOS: .app bundle root
     * - Android: APK assets folder (use AssetManager)
     */
    static std::string GetAssetsPath();

    /**
     * @brief Get save game directory
     *
     * Platform paths:
     * - Windows: %USERPROFILE%/Saved Games/AppName/
     * - Linux: ~/.local/share/AppName/saves/
     * - macOS: ~/Library/Application Support/AppName/Saves/
     * - iOS: <AppSandbox>/Documents/Saves/
     * - Android: Context.getFilesDir() + "/saves/"
     */
    static std::string GetSavePath();

    /**
     * @brief Get configuration directory
     *
     * Platform paths:
     * - Windows: %APPDATA%/AppName/
     * - Linux: ~/.config/AppName/ (XDG_CONFIG_HOME)
     * - macOS: ~/Library/Preferences/
     * - iOS: <AppSandbox>/Library/Preferences/
     * - Android: Context.getFilesDir() + "/config/"
     */
    static std::string GetConfigPath();

    /**
     * @brief Get temporary directory
     *
     * Platform paths:
     * - Windows: %TEMP%/
     * - Linux: /tmp/ or $TMPDIR
     * - macOS: $TMPDIR
     * - iOS: NSTemporaryDirectory()
     * - Android: Context.getCacheDir()
     */
    static std::string GetTempPath();

    // =========================================================================
    // File Operations
    // =========================================================================

    /**
     * @brief Read entire file into memory
     * @param path File path
     * @return File contents, empty vector on failure
     */
    static std::vector<uint8_t> ReadFile(const std::string& path);

    /**
     * @brief Read file as string
     * @param path File path
     * @return File contents as UTF-8 string, empty on failure
     */
    static std::string ReadTextFile(const std::string& path);

    /**
     * @brief Write data to file
     * @param path File path
     * @param data Data to write
     * @return true on success
     */
    static bool WriteFile(const std::string& path, const std::vector<uint8_t>& data);

    /**
     * @brief Write string to file
     * @param path File path
     * @param content UTF-8 string to write
     * @return true on success
     */
    static bool WriteTextFile(const std::string& path, const std::string& content);

    /**
     * @brief Append data to file
     * @param path File path
     * @param data Data to append
     * @return true on success
     */
    static bool AppendFile(const std::string& path, const std::vector<uint8_t>& data);

    /**
     * @brief Copy file
     * @param source Source path
     * @param destination Destination path
     * @param overwrite Overwrite if destination exists
     * @return true on success
     */
    static bool CopyFile(const std::string& source, const std::string& destination,
                         bool overwrite = false);

    /**
     * @brief Move/rename file
     * @param source Source path
     * @param destination Destination path
     * @return true on success
     */
    static bool MoveFile(const std::string& source, const std::string& destination);

    /**
     * @brief Delete file
     * @param path File path
     * @return true if file was deleted or didn't exist
     */
    static bool DeleteFile(const std::string& path);

    // =========================================================================
    // Directory Operations
    // =========================================================================

    /**
     * @brief Create directory (and parent directories)
     * @param path Directory path
     * @return true if directory exists or was created
     */
    static bool CreateDirectory(const std::string& path);

    /**
     * @brief Delete directory
     * @param path Directory path
     * @param recursive Delete contents recursively
     * @return true on success
     */
    static bool DeleteDirectory(const std::string& path, bool recursive = false);

    /**
     * @brief List files in directory
     * @param path Directory path
     * @param recursive Include subdirectories
     * @return List of file paths
     */
    static std::vector<std::string> ListFiles(const std::string& path, bool recursive = false);

    /**
     * @brief List files matching pattern
     * @param path Directory path
     * @param pattern Glob pattern (e.g., "*.png")
     * @param recursive Include subdirectories
     * @return List of matching file paths
     */
    static std::vector<std::string> ListFiles(const std::string& path,
                                               const std::string& pattern,
                                               bool recursive = false);

    /**
     * @brief Enumerate directory with callback
     * @param path Directory path
     * @param callback Called for each entry (return false to stop)
     * @param recursive Include subdirectories
     */
    static void EnumerateDirectory(const std::string& path,
                                    DirectoryCallback callback,
                                    bool recursive = false);

    // =========================================================================
    // File/Directory Queries
    // =========================================================================

    /**
     * @brief Check if path exists
     */
    static bool Exists(const std::string& path);

    /**
     * @brief Check if path is a file
     */
    static bool IsFile(const std::string& path);

    /**
     * @brief Check if path is a directory
     */
    static bool IsDirectory(const std::string& path);

    /**
     * @brief Get file information
     * @param path File path
     * @return File info, or nullopt on failure
     */
    static std::optional<FileInfo> GetFileInfo(const std::string& path);

    /**
     * @brief Get file size
     * @param path File path
     * @return File size in bytes, or 0 on failure
     */
    static uint64_t GetFileSize(const std::string& path);

    /**
     * @brief Get file modification time
     * @param path File path
     * @return Unix timestamp, or 0 on failure
     */
    static uint64_t GetModificationTime(const std::string& path);

    // =========================================================================
    // Path Manipulation
    // =========================================================================

    /**
     * @brief Join path components
     * @param base Base path
     * @param path Path to append
     * @return Combined path
     */
    static std::string Join(const std::string& base, const std::string& path);

    /**
     * @brief Get parent directory
     * @param path File/directory path
     * @return Parent path
     */
    static std::string GetParent(const std::string& path);

    /**
     * @brief Get filename from path
     * @param path File path
     * @return Filename with extension
     */
    static std::string GetFileName(const std::string& path);

    /**
     * @brief Get filename without extension
     * @param path File path
     * @return Filename without extension
     */
    static std::string GetBaseName(const std::string& path);

    /**
     * @brief Get file extension
     * @param path File path
     * @return Extension (with dot), or empty string
     */
    static std::string GetExtension(const std::string& path);

    /**
     * @brief Normalize path (resolve . and .., convert separators)
     * @param path Path to normalize
     * @return Normalized path
     */
    static std::string Normalize(const std::string& path);

    /**
     * @brief Get absolute path
     * @param path Relative or absolute path
     * @return Absolute path
     */
    static std::string GetAbsolute(const std::string& path);

    /**
     * @brief Make path relative to base
     * @param path Absolute path
     * @param base Base path
     * @return Relative path
     */
    static std::string GetRelative(const std::string& path, const std::string& base);

    /**
     * @brief Check if path is absolute
     */
    static bool IsAbsolute(const std::string& path);

    // =========================================================================
    // Platform-Specific Asset Loading (Android/iOS)
    // =========================================================================

    /**
     * @brief Read asset from APK/bundle (Android/iOS)
     * @param assetPath Relative path within assets
     * @return Asset data, empty on failure
     */
    static std::vector<uint8_t> ReadAsset(const std::string& assetPath);

    /**
     * @brief Check if asset exists in APK/bundle
     * @param assetPath Relative path within assets
     * @return true if asset exists
     */
    static bool AssetExists(const std::string& assetPath);

    /**
     * @brief List assets in directory (Android/iOS)
     * @param assetPath Directory path within assets
     * @return List of asset names
     */
    static std::vector<std::string> ListAssets(const std::string& assetPath);

    // =========================================================================
    // Utility
    // =========================================================================

    /**
     * @brief Set application name (used for path generation)
     * @param name Application name
     */
    static void SetAppName(const std::string& name);

    /**
     * @brief Get application name
     */
    static std::string GetAppName();

    /**
     * @brief Get available disk space at path
     * @param path Path to check
     * @return Available space in bytes
     */
    static uint64_t GetAvailableSpace(const std::string& path);

    /**
     * @brief Get platform-specific path separator
     */
    static char GetPathSeparator();

private:
    static std::string s_appName;
};

} // namespace Nova
