#include "AssetEditor.hpp"
#include <algorithm>
#include <spdlog/spdlog.h>

// AssetEditorFactory implementation
AssetEditorFactory& AssetEditorFactory::Instance() {
    static AssetEditorFactory instance;
    return instance;
}

void AssetEditorFactory::RegisterEditor(const std::string& extension, EditorCreator creator) {
    if (!creator) {
        spdlog::warn("AssetEditorFactory: Attempted to register null creator for extension '{}'", extension);
        return;
    }

    std::string ext = extension;
    // Ensure extension starts with a dot
    if (!ext.empty() && ext[0] != '.') {
        ext = "." + ext;
    }

    // Convert to lowercase for case-insensitive matching
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    m_editors[ext] = creator;
    spdlog::info("AssetEditorFactory: Registered editor for extension '{}'", ext);
}

std::unique_ptr<IAssetEditor> AssetEditorFactory::CreateEditor(const std::string& assetPath) {
    std::string ext = GetExtension(assetPath);
    if (ext.empty()) {
        spdlog::warn("AssetEditorFactory: No extension found in path '{}'", assetPath);
        return nullptr;
    }

    auto it = m_editors.find(ext);
    if (it == m_editors.end()) {
        spdlog::debug("AssetEditorFactory: No editor registered for extension '{}'", ext);
        return nullptr;
    }

    spdlog::debug("AssetEditorFactory: Creating editor for '{}'", assetPath);
    return it->second();
}

bool AssetEditorFactory::HasEditor(const std::string& extension) const {
    std::string ext = extension;
    // Ensure extension starts with a dot
    if (!ext.empty() && ext[0] != '.') {
        ext = "." + ext;
    }

    // Convert to lowercase for case-insensitive matching
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    return m_editors.find(ext) != m_editors.end();
}

std::vector<std::string> AssetEditorFactory::GetRegisteredExtensions() const {
    std::vector<std::string> extensions;
    extensions.reserve(m_editors.size());

    for (const auto& pair : m_editors) {
        extensions.push_back(pair.first);
    }

    return extensions;
}

std::string AssetEditorFactory::GetExtension(const std::string& path) {
    size_t dotPos = path.find_last_of('.');
    if (dotPos == std::string::npos || dotPos == path.length() - 1) {
        return "";
    }

    std::string ext = path.substr(dotPos);
    // Convert to lowercase for case-insensitive matching
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    return ext;
}
