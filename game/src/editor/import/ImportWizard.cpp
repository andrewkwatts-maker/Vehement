#include "ImportWizard.hpp"
#include "TextureImportPanel.hpp"
#include "ModelImportPanel.hpp"
#include "AnimationImportPanel.hpp"
#include <engine/import/AssetProcessor.hpp>
#include <engine/import/ImportProgress.hpp>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace Vehement {

ImportWizard::ImportWizard()
    : m_texturePanel(std::make_unique<TextureImportPanel>())
    , m_modelPanel(std::make_unique<ModelImportPanel>())
    , m_animationPanel(std::make_unique<AnimationImportPanel>())
    , m_progressTracker(std::make_unique<Nova::ImportProgressTracker>()) {
}

ImportWizard::~ImportWizard() {
    Shutdown();
}

void ImportWizard::Initialize(Nova::AssetProcessor* processor, const Config& config) {
    m_processor = processor;
    m_config = config;

    m_texturePanel->Initialize();
    m_modelPanel->Initialize();
    m_animationPanel->Initialize();

    m_initialized = true;
}

void ImportWizard::Shutdown() {
    if (!m_initialized) return;

    CancelImport();
    ClearQueue();

    m_texturePanel->Shutdown();
    m_modelPanel->Shutdown();
    m_animationPanel->Shutdown();

    m_initialized = false;
}

void ImportWizard::Update(float deltaTime) {
    if (!m_initialized || !m_visible) return;

    // Update drag highlight animation
    float targetHighlight = m_isDraggingOver ? 1.0f : 0.0f;
    m_dragHighlight = m_dragHighlight + (targetHighlight - m_dragHighlight) * deltaTime * 10.0f;

    // Update import progress
    if (m_state == State::Importing) {
        UpdateImportProgress();

        m_progressAnimation += deltaTime * 2.0f;
        if (m_progressAnimation > 6.28f) m_progressAnimation -= 6.28f;
    }

    // Update sub-panels
    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_queue.size())) {
        const auto& entry = m_queue[m_selectedIndex];
        if (entry.assetType == "Texture") {
            m_texturePanel->Update(deltaTime);
        } else if (entry.assetType == "Model") {
            m_modelPanel->Update(deltaTime);
        } else if (entry.assetType == "Animation") {
            m_animationPanel->Update(deltaTime);
        }
    }
}

void ImportWizard::Render() {
    if (!m_initialized || !m_visible) return;

    // Background
    // ImGui::SetNextWindowPos(ImVec2(m_config.position.x, m_config.position.y), ImGuiCond_FirstUseEver);
    // ImGui::SetNextWindowSize(ImVec2(m_config.size.x, m_config.size.y), ImGuiCond_FirstUseEver);

    // Main window rendering would go here using ImGui or custom renderer
    // For this implementation, we define the structure

    RenderDropZone();
    RenderQueueList();

    if (m_config.showPreview) {
        RenderPreviewPanel();
    }

    if (m_config.showSettings) {
        RenderSettingsPanel();
    }

    if (m_state == State::Importing) {
        RenderProgressBar();
    }

    RenderButtons();
}

void ImportWizard::Show() {
    m_visible = true;
}

void ImportWizard::Hide() {
    m_visible = false;
}

void ImportWizard::Toggle() {
    m_visible = !m_visible;
}

// ============================================================================
// File Handling
// ============================================================================

void ImportWizard::AddFiles(const std::vector<std::string>& paths) {
    for (const auto& path : paths) {
        AddFile(path);
    }

    if (!m_queue.empty()) {
        m_state = State::FilesQueued;
        if (m_selectedIndex < 0) {
            m_selectedIndex = 0;
        }
    }
}

void ImportWizard::AddFile(const std::string& path) {
    // Check if already in queue
    for (const auto& entry : m_queue) {
        if (entry.filePath == path) {
            return;
        }
    }

    // Check if file exists and is importable
    if (!fs::exists(path)) return;

    std::string type = DetectFileType(path);
    if (type == "Unknown") return;

    ImportQueueEntry entry;
    entry.filePath = path;
    entry.fileName = fs::path(path).filename().string();
    entry.assetType = type;
    entry.fileSize = fs::file_size(path);
    entry.selected = true;

    m_queue.push_back(entry);
    m_previewDirty = true;
}

void ImportWizard::ClearQueue() {
    m_queue.clear();
    m_selectedIndex = -1;
    m_currentImportIndex = 0;
    m_importedCount = 0;
    m_failedCount = 0;
    m_state = State::Idle;
    m_previewDirty = true;
}

void ImportWizard::RemoveFile(size_t index) {
    if (index >= m_queue.size()) return;

    m_queue.erase(m_queue.begin() + index);

    if (m_selectedIndex >= static_cast<int>(m_queue.size())) {
        m_selectedIndex = static_cast<int>(m_queue.size()) - 1;
    }

    if (m_queue.empty()) {
        m_state = State::Idle;
        m_selectedIndex = -1;
    }

    m_previewDirty = true;
}

// ============================================================================
// Import Control
// ============================================================================

void ImportWizard::StartImport() {
    if (m_queue.empty() || m_state == State::Importing) return;

    m_state = State::Importing;
    m_currentImportIndex = 0;
    m_importedCount = 0;
    m_failedCount = 0;

    m_progressTracker->Clear();

    if (m_onImportStarted) {
        m_onImportStarted();
    }

    ProcessNextFile();
}

void ImportWizard::CancelImport() {
    if (m_state != State::Importing) return;

    m_progressTracker->CancelAll();
    m_state = State::FilesQueued;
}

float ImportWizard::GetOverallProgress() const {
    if (m_queue.empty()) return 0.0f;

    size_t completed = m_importedCount + m_failedCount;
    return static_cast<float>(completed) / m_queue.size();
}

// ============================================================================
// Selection
// ============================================================================

void ImportWizard::SelectFile(size_t index) {
    if (index < m_queue.size()) {
        m_selectedIndex = static_cast<int>(index);
        m_previewDirty = true;
    }
}

void ImportWizard::SelectAll() {
    for (auto& entry : m_queue) {
        entry.selected = true;
    }
}

void ImportWizard::DeselectAll() {
    for (auto& entry : m_queue) {
        entry.selected = false;
    }
}

// ============================================================================
// Input
// ============================================================================

void ImportWizard::OnFileDrop(const std::vector<std::string>& paths) {
    AddFiles(paths);
}

bool ImportWizard::OnMouseClick(const glm::vec2& pos, int button) {
    if (!m_visible || !IsOverWizard(pos)) return false;

    // Handle clicks on queue items, buttons, etc.
    // This would be implemented with actual UI coordinates

    return true;
}

void ImportWizard::OnMouseMove(const glm::vec2& pos) {
    m_mousePos = pos;
    m_isDraggingOver = IsOverDropZone(pos);
}

bool ImportWizard::IsOverWizard(const glm::vec2& pos) const {
    return pos.x >= m_config.position.x &&
           pos.x <= m_config.position.x + m_config.size.x &&
           pos.y >= m_config.position.y &&
           pos.y <= m_config.position.y + m_config.size.y;
}

bool ImportWizard::IsOverDropZone(const glm::vec2& pos) const {
    glm::vec2 bounds = GetDropZoneBounds();
    return pos.x >= m_config.position.x &&
           pos.x <= m_config.position.x + bounds.x &&
           pos.y >= m_config.position.y + 50 &&
           pos.y <= m_config.position.y + 50 + bounds.y;
}

// ============================================================================
// Rendering
// ============================================================================

void ImportWizard::RenderDropZone() {
    // Drop zone would be rendered here
    // Visual feedback for drag-over state using m_dragHighlight

    /*
    ImGui::BeginChild("DropZone", ImVec2(0, 100), true);

    // Center text
    ImVec2 textSize = ImGui::CalcTextSize("Drop files here to import");
    ImVec2 windowSize = ImGui::GetWindowSize();
    ImGui::SetCursorPos(ImVec2((windowSize.x - textSize.x) / 2, (windowSize.y - textSize.y) / 2));

    if (m_isDraggingOver) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Release to add files");
    } else if (m_queue.empty()) {
        ImGui::Text("Drop files here to import");
    } else {
        ImGui::Text("%zu files in queue", m_queue.size());
    }

    ImGui::EndChild();
    */
}

void ImportWizard::RenderQueueList() {
    // Queue list would be rendered here

    /*
    ImGui::BeginChild("QueueList", ImVec2(300, 0), true);

    for (size_t i = 0; i < m_queue.size(); ++i) {
        auto& entry = m_queue[i];

        bool isSelected = (static_cast<int>(i) == m_selectedIndex);

        // File icon based on type
        ImVec4 color = GetFileColor(entry.assetType);

        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.3f, 0.3f, 0.5f, 1.0f));
        }

        if (ImGui::Selectable(entry.fileName.c_str(), isSelected)) {
            SelectFile(i);
        }

        if (isSelected) {
            ImGui::PopStyleColor();
        }

        // Status indicator
        if (entry.imported) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "[OK]");
        } else if (entry.failed) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "[FAIL]");
        } else if (m_state == State::Importing && i == m_currentImportIndex) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.8f, 0.8f, 0.2f, 1.0f), "[%.0f%%]", entry.progress * 100);
        }
    }

    ImGui::EndChild();
    */
}

void ImportWizard::RenderPreviewPanel() {
    // Preview panel would be rendered here
    // Shows 2D preview for textures, 3D preview for models, timeline for animations

    /*
    ImGui::BeginChild("Preview", ImVec2(0, 200), true);

    if (m_selectedIndex >= 0 && m_selectedIndex < static_cast<int>(m_queue.size())) {
        const auto& entry = m_queue[m_selectedIndex];

        ImGui::Text("Preview: %s", entry.fileName.c_str());
        ImGui::Separator();

        if (entry.assetType == "Texture" && m_previewTexture) {
            // Render texture preview
            ImGui::Image((ImTextureID)m_previewTexture->GetID(), ImVec2(150, 150));
        } else if (entry.assetType == "Model") {
            ImGui::Text("3D Model Preview");
            // 3D preview would be rendered here
        } else if (entry.assetType == "Animation") {
            ImGui::Text("Animation Preview");
            // Animation timeline would be rendered here
        }
    } else {
        ImGui::Text("Select a file to preview");
    }

    ImGui::EndChild();
    */
}

void ImportWizard::RenderSettingsPanel() {
    // Settings panel - delegates to type-specific panels

    if (m_selectedIndex < 0 || m_selectedIndex >= static_cast<int>(m_queue.size())) {
        return;
    }

    const auto& entry = m_queue[m_selectedIndex];

    /*
    ImGui::BeginChild("Settings", ImVec2(0, 0), true);

    ImGui::Text("Import Settings: %s", entry.assetType.c_str());
    ImGui::Separator();

    if (entry.assetType == "Texture") {
        m_texturePanel->Render();
    } else if (entry.assetType == "Model") {
        m_modelPanel->Render();
    } else if (entry.assetType == "Animation") {
        m_animationPanel->Render();
    }

    ImGui::EndChild();
    */
}

void ImportWizard::RenderProgressBar() {
    // Progress bar during import

    /*
    float progress = GetOverallProgress();

    ImGui::ProgressBar(progress, ImVec2(-1, 0),
        (std::to_string(m_importedCount + m_failedCount) + "/" + std::to_string(m_queue.size())).c_str());

    if (m_currentImportIndex < m_queue.size()) {
        ImGui::Text("Importing: %s", m_queue[m_currentImportIndex].fileName.c_str());
    }
    */
}

void ImportWizard::RenderButtons() {
    // Action buttons

    /*
    ImGui::Separator();

    if (m_state == State::Importing) {
        if (ImGui::Button("Cancel")) {
            CancelImport();
        }
    } else {
        if (ImGui::Button("Import Selected") && !m_queue.empty()) {
            StartImport();
        }

        ImGui::SameLine();

        if (ImGui::Button("Clear Queue")) {
            ClearQueue();
        }

        ImGui::SameLine();

        if (ImGui::Button("Close")) {
            Hide();
        }
    }
    */
}

// ============================================================================
// Private Helpers
// ============================================================================

void ImportWizard::UpdateImportProgress() {
    if (m_currentImportIndex >= m_queue.size()) {
        // All done
        m_state = State::Completed;

        if (m_onImportCompleted) {
            m_onImportCompleted(m_importedCount, m_failedCount);
        }
        return;
    }

    // Check current import progress
    auto& entry = m_queue[m_currentImportIndex];
    Nova::ImportProgress* progress = m_progressTracker->GetImport(entry.filePath);

    if (progress) {
        entry.progress = progress->GetProgress();
        entry.statusMessage = progress->GetStatusMessage();

        if (progress->IsCompleted()) {
            if (progress->IsSuccessful()) {
                entry.imported = true;
                m_importedCount++;
            } else {
                entry.failed = true;
                m_failedCount++;
            }

            if (m_onFileImported) {
                m_onFileImported(entry.filePath, progress->IsSuccessful());
            }

            m_currentImportIndex++;
            ProcessNextFile();
        }
    }
}

void ImportWizard::ProcessNextFile() {
    while (m_currentImportIndex < m_queue.size()) {
        auto& entry = m_queue[m_currentImportIndex];

        if (!entry.selected) {
            m_currentImportIndex++;
            continue;
        }

        // Start import
        Nova::ImportProgress* progress = m_progressTracker->AddImport(entry.filePath);
        m_processor->QueueAsset(entry.filePath, 0, [this, &entry](bool success) {
            entry.imported = success;
            entry.failed = !success;
        });

        break;
    }
}

std::string ImportWizard::DetectFileType(const std::string& path) {
    std::string ext = fs::path(path).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

    // Textures
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" ||
        ext == ".bmp" || ext == ".dds" || ext == ".ktx" || ext == ".exr" || ext == ".hdr") {
        return "Texture";
    }

    // Models
    if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb" ||
        ext == ".dae" || ext == ".3ds") {
        return "Model";
    }

    // Animations
    if (ext == ".bvh" || ext == ".anim") {
        return "Animation";
    }

    return "Unknown";
}

std::string ImportWizard::GetFileIcon(const std::string& type) {
    if (type == "Texture") return "[TEX]";
    if (type == "Model") return "[MDL]";
    if (type == "Animation") return "[ANM]";
    return "[???]";
}

glm::vec4 ImportWizard::GetFileColor(const std::string& type) {
    if (type == "Texture") return glm::vec4(0.8f, 0.6f, 0.2f, 1.0f);
    if (type == "Model") return glm::vec4(0.2f, 0.6f, 0.8f, 1.0f);
    if (type == "Animation") return glm::vec4(0.6f, 0.2f, 0.8f, 1.0f);
    return glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
}

glm::vec2 ImportWizard::GetDropZoneBounds() const {
    return glm::vec2(m_config.size.x, 100.0f);
}

glm::vec2 ImportWizard::GetQueueListBounds() const {
    return glm::vec2(300.0f, m_config.size.y - 200.0f);
}

glm::vec2 ImportWizard::GetPreviewBounds() const {
    return glm::vec2(m_config.size.x - 320.0f, 200.0f);
}

glm::vec2 ImportWizard::GetSettingsBounds() const {
    return glm::vec2(m_config.size.x - 320.0f, m_config.size.y - 350.0f);
}

} // namespace Vehement
