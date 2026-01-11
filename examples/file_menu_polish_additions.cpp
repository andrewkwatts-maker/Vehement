// ========================================
// File Menu Polish - New Implementations
// To be added to StandaloneEditor.cpp
// ========================================

// Add to includes at the top:
// #include "ModernUI.hpp"
// #include <fstream>
// #ifdef _WIN32
// #include <Windows.h>
// #include <commdlg.h>
// #endif

// ========================================
// Windows Native File Dialog Functions  
// ========================================

#ifdef _WIN32
std::string StandaloneEditor::OpenNativeFileDialog(const char* filter, const char* title) {
    char filename[MAX_PATH] = {0};
    
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_DONTADDTORECENT | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
    
    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }
    
    return "";
}

std::string StandaloneEditor::SaveNativeFileDialog(const char* filter, const char* title, const char* defaultExt) {
    char filename[MAX_PATH] = {0};
    
    OPENFILENAMEA ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.lpstrDefExt = defaultExt;
    ofn.Flags = OFN_DONTADDTORECENT | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
    
    if (GetSaveFileNameA(&ofn)) {
        return std::string(filename);
    }
    
    return "";
}
#else
// Fallback for non-Windows platforms
std::string StandaloneEditor::OpenNativeFileDialog(const char* filter, const char* title) {
    spdlog::warn("Native file dialog not implemented for this platform");
    return "";
}

std::string StandaloneEditor::SaveNativeFileDialog(const char* filter, const char* title, const char* defaultExt) {
    spdlog::warn("Native file dialog not implemented for this platform");
    return "";
}
#endif

// ========================================
// Recent Files Management
// ========================================

void StandaloneEditor::LoadRecentFiles() {
    std::string configPath = "assets/config/editor.json";
    std::ifstream file(configPath);
    
    if (!file.is_open()) {
        spdlog::info("No editor config found, starting with empty recent files");
        return;
    }
    
    m_recentFiles.clear();
    std::string line;
    bool inRecentFiles = false;
    
    while (std::getline(file, line)) {
        // Simple JSON parsing - look for "recentFiles" array
        if (line.find("\"recentFiles\"") != std::string::npos) {
            inRecentFiles = true;
            continue;
        }
        
        if (inRecentFiles) {
            if (line.find("]") != std::string::npos) {
                break;
            }
            
            // Extract file path from JSON string
            size_t start = line.find('"');
            size_t end = line.rfind('"');
            if (start != std::string::npos && end != std::string::npos && start != end) {
                std::string path = line.substr(start + 1, end - start - 1);
                if (!path.empty()) {
                    m_recentFiles.push_back(path);
                }
            }
        }
    }
    
    file.close();
    spdlog::info("Loaded {} recent files", m_recentFiles.size());
}

void StandaloneEditor::SaveRecentFiles() {
    std::string configPath = "assets/config/editor.json";
    std::ofstream file(configPath);
    
    if (!file.is_open()) {
        spdlog::error("Failed to save recent files to {}", configPath);
        return;
    }
    
    // Write simple JSON structure
    file << "{\n";
    file << "  \"recentFiles\": [\n";
    
    for (size_t i = 0; i < m_recentFiles.size(); ++i) {
        file << "    \"" << m_recentFiles[i] << "\"";
        if (i < m_recentFiles.size() - 1) {
            file << ",";
        }
        file << "\n";
    }
    
    file << "  ]\n";
    file << "}\n";
    
    file.close();
    spdlog::info("Saved {} recent files", m_recentFiles.size());
}

void StandaloneEditor::AddToRecentFiles(const std::string& path) {
    // Remove if already exists
    auto it = std::find(m_recentFiles.begin(), m_recentFiles.end(), path);
    if (it != m_recentFiles.end()) {
        m_recentFiles.erase(it);
    }
    
    // Add to front
    m_recentFiles.insert(m_recentFiles.begin(), path);
    
    // Keep only last 10
    if (m_recentFiles.size() > 10) {
        m_recentFiles.resize(10);
    }
    
    SaveRecentFiles();
}

void StandaloneEditor::ClearRecentFiles() {
    m_recentFiles.clear();
    SaveRecentFiles();
}

// ========================================
// Import/Export Dialog Implementations
// ========================================

void StandaloneEditor::ShowImportDialog() {
    // This will be shown as a popup menu in the File menu
    // Implementations handled in RenderMenuBar
}

void StandaloneEditor::ShowExportDialog() {
    // This will be shown as a popup menu in the File menu
    // Implementations handled in RenderMenuBar
}

