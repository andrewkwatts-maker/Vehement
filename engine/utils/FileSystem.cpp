#include <string>
#include <string_view>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <expected>
#include <vector>
#include <span>
#include <cstdint>
#include <system_error>

namespace Nova {

namespace FileSystem {

/**
 * @brief File system operation error types
 */
enum class FileError {
    NotFound,
    AccessDenied,
    AlreadyExists,
    IoError,
    InvalidPath,
    DiskFull
};

/**
 * @brief Convert system error to FileError
 */
[[nodiscard]] static FileError FromSystemError(std::error_code ec) noexcept {
    if (ec == std::errc::no_such_file_or_directory) {
        return FileError::NotFound;
    } else if (ec == std::errc::permission_denied) {
        return FileError::AccessDenied;
    } else if (ec == std::errc::file_exists) {
        return FileError::AlreadyExists;
    } else if (ec == std::errc::no_space_on_device) {
        return FileError::DiskFull;
    }
    return FileError::IoError;
}

/**
 * @brief Read entire file contents into a string
 * @param path Path to the file
 * @return File contents or error
 */
[[nodiscard]] std::expected<std::string, FileError> ReadFile(std::string_view path) {
    std::ifstream file(std::string(path), std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        if (!std::filesystem::exists(path)) {
            return std::unexpected(FileError::NotFound);
        }
        return std::unexpected(FileError::AccessDenied);
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();

    if (file.bad()) {
        return std::unexpected(FileError::IoError);
    }

    return buffer.str();
}

/**
 * @brief Read file contents as binary data
 */
[[nodiscard]] std::expected<std::vector<uint8_t>, FileError> ReadBinaryFile(std::string_view path) {
    std::ifstream file(std::string(path), std::ios::in | std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        if (!std::filesystem::exists(path)) {
            return std::unexpected(FileError::NotFound);
        }
        return std::unexpected(FileError::AccessDenied);
    }

    const auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return std::unexpected(FileError::IoError);
    }

    return buffer;
}

/**
 * @brief Write string content to a file
 * @param path Path to the file
 * @param content Content to write
 * @return Success or error
 */
[[nodiscard]] std::expected<void, FileError> WriteFile(std::string_view path, std::string_view content) {
    std::filesystem::path filePath(path);

    if (filePath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(filePath.parent_path(), ec);
        if (ec) {
            return std::unexpected(FromSystemError(ec));
        }
    }

    std::ofstream file(filePath, std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected(FileError::AccessDenied);
    }

    file.write(content.data(), static_cast<std::streamsize>(content.size()));

    if (file.bad()) {
        return std::unexpected(FileError::IoError);
    }

    return {};
}

/**
 * @brief Write binary data to a file
 */
[[nodiscard]] std::expected<void, FileError> WriteBinaryFile(std::string_view path, std::span<const uint8_t> data) {
    std::filesystem::path filePath(path);

    if (filePath.has_parent_path()) {
        std::error_code ec;
        std::filesystem::create_directories(filePath.parent_path(), ec);
        if (ec) {
            return std::unexpected(FromSystemError(ec));
        }
    }

    std::ofstream file(filePath, std::ios::out | std::ios::binary);
    if (!file.is_open()) {
        return std::unexpected(FileError::AccessDenied);
    }

    file.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));

    if (file.bad()) {
        return std::unexpected(FileError::IoError);
    }

    return {};
}

/**
 * @brief Check if a file exists
 */
[[nodiscard]] bool FileExists(std::string_view path) noexcept {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && !ec;
}

/**
 * @brief Check if a directory exists
 */
[[nodiscard]] bool DirectoryExists(std::string_view path) noexcept {
    std::error_code ec;
    return std::filesystem::is_directory(path, ec) && !ec;
}

/**
 * @brief Create a directory (and parent directories if needed)
 */
[[nodiscard]] std::expected<void, FileError> CreateDirectory(std::string_view path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);
    if (ec) {
        return std::unexpected(FromSystemError(ec));
    }
    return {};
}

/**
 * @brief Delete a file
 */
[[nodiscard]] std::expected<void, FileError> DeleteFile(std::string_view path) {
    std::error_code ec;
    if (!std::filesystem::remove(path, ec) || ec) {
        if (!std::filesystem::exists(path)) {
            return std::unexpected(FileError::NotFound);
        }
        return std::unexpected(FromSystemError(ec));
    }
    return {};
}

/**
 * @brief Copy a file
 */
[[nodiscard]] std::expected<void, FileError> CopyFile(std::string_view source, std::string_view dest) {
    std::error_code ec;
    std::filesystem::copy_file(source, dest, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        return std::unexpected(FromSystemError(ec));
    }
    return {};
}

/**
 * @brief Get file extension (including dot)
 */
[[nodiscard]] std::string GetFileExtension(std::string_view path) noexcept {
    return std::filesystem::path(path).extension().string();
}

/**
 * @brief Get file name with extension
 */
[[nodiscard]] std::string GetFileName(std::string_view path) noexcept {
    return std::filesystem::path(path).filename().string();
}

/**
 * @brief Get file name without extension
 */
[[nodiscard]] std::string GetFileStem(std::string_view path) noexcept {
    return std::filesystem::path(path).stem().string();
}

/**
 * @brief Get parent directory path
 */
[[nodiscard]] std::string GetDirectory(std::string_view path) noexcept {
    return std::filesystem::path(path).parent_path().string();
}

/**
 * @brief Get file size in bytes
 */
[[nodiscard]] std::expected<std::uintmax_t, FileError> GetFileSize(std::string_view path) {
    std::error_code ec;
    auto size = std::filesystem::file_size(path, ec);
    if (ec) {
        return std::unexpected(FromSystemError(ec));
    }
    return size;
}

/**
 * @brief Get absolute path from relative path
 */
[[nodiscard]] std::string GetAbsolutePath(std::string_view path) noexcept {
    std::error_code ec;
    auto absolute = std::filesystem::absolute(path, ec);
    return ec ? std::string(path) : absolute.string();
}

/**
 * @brief Normalize path (resolve . and .. and convert separators)
 */
[[nodiscard]] std::string NormalizePath(std::string_view path) noexcept {
    std::error_code ec;
    auto normalized = std::filesystem::weakly_canonical(path, ec);
    return ec ? std::string(path) : normalized.string();
}

} // namespace FileSystem

} // namespace Nova
