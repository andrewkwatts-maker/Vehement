#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace Nova {

namespace FileSystem {

std::string ReadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool WriteFile(const std::string& path, const std::string& content) {
    std::filesystem::path filePath(path);
    if (filePath.has_parent_path()) {
        std::filesystem::create_directories(filePath.parent_path());
    }

    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << content;
    return true;
}

bool FileExists(const std::string& path) {
    return std::filesystem::exists(path);
}

bool DirectoryExists(const std::string& path) {
    return std::filesystem::is_directory(path);
}

bool CreateDirectory(const std::string& path) {
    return std::filesystem::create_directories(path);
}

std::string GetFileExtension(const std::string& path) {
    return std::filesystem::path(path).extension().string();
}

std::string GetFileName(const std::string& path) {
    return std::filesystem::path(path).filename().string();
}

std::string GetDirectory(const std::string& path) {
    return std::filesystem::path(path).parent_path().string();
}

} // namespace FileSystem

} // namespace Nova
