#include "AnimationImportPanel.hpp"
#include <engine/import/ImportSettings.hpp>
#include <engine/import/AnimationImporter.hpp>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;

namespace Vehement {

AnimationImportPanel::AnimationImportPanel()
    : m_settings(std::make_unique<Nova::AnimationImportSettings>()) {
}

AnimationImportPanel::~AnimationImportPanel() = default;

void AnimationImportPanel::Initialize() {
    // Setup default clip markers
}

void AnimationImportPanel::Shutdown() {
    m_previewAnimation.reset();
}

void AnimationImportPanel::Update(float deltaTime) {
    if (m_previewDirty) {
        UpdatePreview();
        m_previewDirty = false;
    }

    // Update playback
    if (m_isPlaying && m_previewAnimation && !m_previewAnimation->clips.empty()) {
        m_currentTime += deltaTime * m_playbackSpeed;

        float duration = m_previewAnimation->clips[0].duration;
        if (m_currentTime > duration) {
            if (m_loopPlayback) {
                m_currentTime = fmod(m_currentTime, duration);
            } else {
                m_currentTime = duration;
                m_isPlaying = false;
            }
        }
    }
}

void AnimationImportPanel::Render() {
    RenderTimeline();
    RenderPosePreview();
    RenderClipList();
    RenderClipSplitter();
    RenderRootMotionSettings();
    RenderCompressionSettings();
    RenderRetargetingSettings();
    RenderEventEditor();
    RenderStatistics();
}

void AnimationImportPanel::SetAnimationPath(const std::string& path) {
    m_animationPath = path;
    m_settings->assetPath = path;
    m_previewDirty = true;

    m_currentTime = 0.0f;
    m_isPlaying = false;
    m_clipMarkers.clear();
    m_eventMarkers.clear();

    LoadPreviewAnimation();
}

void AnimationImportPanel::ApplyPreset(const std::string& preset) {
    if (preset == "Mobile") {
        m_settings->ApplyPreset(Nova::ImportPreset::Mobile);
    } else if (preset == "Desktop") {
        m_settings->ApplyPreset(Nova::ImportPreset::Desktop);
    } else if (preset == "HighQuality") {
        m_settings->ApplyPreset(Nova::ImportPreset::HighQuality);
    }

    m_previewDirty = true;
    if (m_onSettingsChanged) m_onSettingsChanged();
}

void AnimationImportPanel::Play() {
    m_isPlaying = true;
}

void AnimationImportPanel::Pause() {
    m_isPlaying = false;
}

void AnimationImportPanel::Stop() {
    m_isPlaying = false;
    m_currentTime = 0.0f;
}

void AnimationImportPanel::SeekTo(float time) {
    m_currentTime = time;
    if (m_previewAnimation && !m_previewAnimation->clips.empty()) {
        m_currentTime = glm::clamp(m_currentTime, 0.0f, m_previewAnimation->clips[0].duration);
    }
}

void AnimationImportPanel::RenderTimeline() {
    /*
    ImGui::Text("Timeline");
    ImGui::Separator();

    if (!m_previewAnimation || m_previewAnimation->clips.empty()) {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No animation loaded");
        return;
    }

    float duration = m_previewAnimation->clips[0].duration;

    // Playback controls
    if (m_isPlaying) {
        if (ImGui::Button("||")) Pause();
    } else {
        if (ImGui::Button(">")) Play();
    }
    ImGui::SameLine();
    if (ImGui::Button("[]")) Stop();
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &m_loopPlayback);
    ImGui::SameLine();
    ImGui::SliderFloat("Speed", &m_playbackSpeed, 0.1f, 2.0f, "%.1fx");

    // Time display
    ImGui::Text("Time: %.2f / %.2f", m_currentTime, duration);

    // Timeline scrubber
    ImVec2 timelinePos = ImGui::GetCursorScreenPos();
    ImVec2 timelineSize(ImGui::GetContentRegionAvail().x, 60);

    // Draw timeline background
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->AddRectFilled(timelinePos,
        ImVec2(timelinePos.x + timelineSize.x, timelinePos.y + timelineSize.y),
        IM_COL32(40, 40, 40, 255));

    // Draw time markers
    float pixelsPerSecond = timelineSize.x / duration * m_timelineZoom;
    for (float t = 0.0f; t <= duration; t += 0.5f) {
        float x = timelinePos.x + (t - m_timelineOffset) * pixelsPerSecond;
        if (x >= timelinePos.x && x <= timelinePos.x + timelineSize.x) {
            drawList->AddLine(
                ImVec2(x, timelinePos.y),
                ImVec2(x, timelinePos.y + timelineSize.y),
                IM_COL32(80, 80, 80, 255));

            char timeStr[16];
            snprintf(timeStr, sizeof(timeStr), "%.1fs", t);
            drawList->AddText(ImVec2(x + 2, timelinePos.y + 2), IM_COL32(150, 150, 150, 255), timeStr);
        }
    }

    // Draw clip markers
    for (size_t i = 0; i < m_clipMarkers.size(); ++i) {
        const auto& marker = m_clipMarkers[i];
        float x1 = timelinePos.x + (marker.startTime - m_timelineOffset) * pixelsPerSecond;
        float x2 = timelinePos.x + (marker.endTime - m_timelineOffset) * pixelsPerSecond;

        ImU32 color = IM_COL32(marker.color.r * 255, marker.color.g * 255,
                               marker.color.b * 255, marker.color.a * 200);

        drawList->AddRectFilled(
            ImVec2(x1, timelinePos.y + 20),
            ImVec2(x2, timelinePos.y + 45),
            color);

        // Marker name
        drawList->AddText(ImVec2(x1 + 4, timelinePos.y + 22),
                          IM_COL32(255, 255, 255, 255), marker.name.c_str());
    }

    // Draw event markers
    for (const auto& event : m_eventMarkers) {
        float x = timelinePos.x + (event.time - m_timelineOffset) * pixelsPerSecond;
        drawList->AddTriangleFilled(
            ImVec2(x, timelinePos.y + 48),
            ImVec2(x - 5, timelinePos.y + 58),
            ImVec2(x + 5, timelinePos.y + 58),
            IM_COL32(event.color.r * 255, event.color.g * 255, event.color.b * 255, 255));
    }

    // Draw playhead
    float playheadX = timelinePos.x + (m_currentTime - m_timelineOffset) * pixelsPerSecond;
    drawList->AddLine(
        ImVec2(playheadX, timelinePos.y),
        ImVec2(playheadX, timelinePos.y + timelineSize.y),
        IM_COL32(255, 100, 100, 255), 2.0f);

    // Invisible button for interaction
    ImGui::InvisibleButton("##timeline", timelineSize);

    if (ImGui::IsItemHovered() || m_isDraggingPlayhead) {
        if (ImGui::IsMouseClicked(0) || (ImGui::IsMouseDown(0) && m_isDraggingPlayhead)) {
            m_isDraggingPlayhead = true;
            float mouseX = ImGui::GetMousePos().x - timelinePos.x;
            float newTime = mouseX / pixelsPerSecond + m_timelineOffset;
            SeekTo(newTime);
        }

        if (ImGui::IsMouseReleased(0)) {
            m_isDraggingPlayhead = false;
        }

        // Zoom with scroll
        float wheel = ImGui::GetIO().MouseWheel;
        if (wheel != 0.0f) {
            m_timelineZoom *= (1.0f + wheel * 0.1f);
            m_timelineZoom = glm::clamp(m_timelineZoom, 0.1f, 10.0f);
        }
    }
    */
}

void AnimationImportPanel::RenderPosePreview() {
    /*
    ImGui::Text("Pose Preview");
    ImGui::Separator();

    // 3D pose preview would be rendered here
    ImVec2 previewSize(200, 200);
    ImGui::Dummy(previewSize);

    ImGui::Checkbox("Show Bone Names", &m_showBoneNames);
    ImGui::Checkbox("Show Root Motion Path", &m_showRootMotion);
    */
}

void AnimationImportPanel::RenderClipList() {
    /*
    if (!m_previewAnimation) return;

    ImGui::Text("Clips (%zu)", m_previewAnimation->clips.size());
    ImGui::Separator();

    for (size_t i = 0; i < m_previewAnimation->clips.size(); ++i) {
        const auto& clip = m_previewAnimation->clips[i];

        ImGui::PushID(static_cast<int>(i));

        bool selected = false;
        ImGui::Selectable(clip.name.c_str(), &selected);

        ImGui::SameLine(150);
        ImGui::Text("%.2fs", clip.duration);

        ImGui::SameLine(200);
        ImGui::Text(clip.looping ? "[Loop]" : "[Once]");

        ImGui::PopID();
    }
    */
}

void AnimationImportPanel::RenderClipSplitter() {
    /*
    ImGui::Text("Clip Splitting");
    ImGui::Separator();

    if (ImGui::Checkbox("Split by Markers", &m_settings->splitByMarkers)) {
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    if (ImGui::Checkbox("Split by Takes", &m_settings->splitByTakes)) {
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    // Clip marker editing
    ImGui::Text("Clip Markers:");

    if (ImGui::Button("Add Marker")) {
        ClipMarker marker;
        marker.name = "Clip_" + std::to_string(m_clipMarkers.size());
        marker.startTime = m_currentTime;
        marker.endTime = m_currentTime + 1.0f;
        marker.color = glm::vec4(0.3f, 0.6f, 0.9f, 0.8f);
        marker.selected = false;
        m_clipMarkers.push_back(marker);
    }

    for (size_t i = 0; i < m_clipMarkers.size(); ++i) {
        auto& marker = m_clipMarkers[i];

        ImGui::PushID(static_cast<int>(i));

        char nameBuf[64];
        strncpy(nameBuf, marker.name.c_str(), sizeof(nameBuf));
        if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) {
            marker.name = nameBuf;
        }

        ImGui::SameLine();
        ImGui::DragFloatRange2("##range", &marker.startTime, &marker.endTime,
                               0.01f, 0.0f, m_previewAnimation ? m_previewAnimation->originalDuration : 10.0f);

        ImGui::SameLine();
        if (ImGui::Button("X")) {
            m_clipMarkers.erase(m_clipMarkers.begin() + i);
            --i;
        }

        ImGui::PopID();
    }
    */
}

void AnimationImportPanel::RenderRootMotionSettings() {
    /*
    ImGui::Text("Root Motion");
    ImGui::Separator();

    if (ImGui::Checkbox("Extract Root Motion", &m_settings->extractRootMotion)) {
        m_previewDirty = true;
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    if (m_settings->extractRootMotion) {
        char rootBoneBuf[64];
        strncpy(rootBoneBuf, m_settings->rootBoneName.c_str(), sizeof(rootBoneBuf));
        if (ImGui::InputText("Root Bone", rootBoneBuf, sizeof(rootBoneBuf))) {
            m_settings->rootBoneName = rootBoneBuf;
            m_previewDirty = true;
        }

        ImGui::Checkbox("Lock Root X/Z", &m_settings->lockRootPositionXZ);
        ImGui::Checkbox("Lock Root Rotation Y", &m_settings->lockRootRotationY);
        ImGui::Checkbox("Lock Root Height", &m_settings->lockRootHeight);

        // Root motion graph preview
        ImGui::Text("Root Motion Path:");
        ImVec2 graphSize(150, 150);
        // Would draw 2D path of root motion here
        ImGui::Dummy(graphSize);
    }
    */
}

void AnimationImportPanel::RenderCompressionSettings() {
    /*
    ImGui::Text("Compression");
    ImGui::Separator();

    const char* compressionModes[] = {"None", "Lossy", "High Quality", "Aggressive"};
    int compression = static_cast<int>(m_settings->compression);
    if (ImGui::Combo("Compression", &compression, compressionModes, IM_ARRAYSIZE(compressionModes))) {
        m_settings->compression = static_cast<Nova::AnimationCompression>(compression);
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    if (m_settings->compression != Nova::AnimationCompression::None) {
        ImGui::DragFloat("Position Tolerance", &m_settings->positionTolerance,
                         0.0001f, 0.0001f, 0.1f, "%.4f");
        ImGui::DragFloat("Rotation Tolerance", &m_settings->rotationTolerance,
                         0.00001f, 0.00001f, 0.01f, "%.5f");
        ImGui::DragFloat("Scale Tolerance", &m_settings->scaleTolerance,
                         0.0001f, 0.0001f, 0.1f, "%.4f");
    }

    if (ImGui::Checkbox("Resample", &m_settings->resample)) {
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    if (m_settings->resample) {
        ImGui::DragFloat("Target FPS", &m_settings->targetSampleRate, 1.0f, 10.0f, 120.0f, "%.0f");
    }
    */
}

void AnimationImportPanel::RenderRetargetingSettings() {
    /*
    ImGui::Text("Retargeting");
    ImGui::Separator();

    if (ImGui::Checkbox("Enable Retargeting", &m_settings->enableRetargeting)) {
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    if (m_settings->enableRetargeting) {
        // Source skeleton selection
        ImGui::InputText("Source Skeleton", &m_settings->sourceSkeletonPath[0],
                         m_settings->sourceSkeletonPath.capacity());

        // Target skeleton selection
        ImGui::InputText("Target Skeleton", &m_settings->targetSkeletonPath[0],
                         m_settings->targetSkeletonPath.capacity());

        if (ImGui::Button("Auto-Generate Mapping")) {
            // Would call AnimationImporter::AutoGenerateBoneMapping
        }

        // Bone mapping table
        if (ImGui::TreeNode("Bone Mapping")) {
            for (auto& [src, tgt] : m_settings->boneMapping) {
                ImGui::Text("%s -> %s", src.c_str(), tgt.c_str());
            }
            ImGui::TreePop();
        }
    }
    */
}

void AnimationImportPanel::RenderEventEditor() {
    /*
    ImGui::Text("Events");
    ImGui::Separator();

    if (ImGui::Checkbox("Import Events", &m_settings->importEvents)) {
        if (m_onSettingsChanged) m_onSettingsChanged();
    }

    if (ImGui::Button("Add Event")) {
        EventMarker event;
        event.name = "Event_" + std::to_string(m_eventMarkers.size());
        event.time = m_currentTime;
        event.color = glm::vec4(0.9f, 0.6f, 0.2f, 1.0f);
        m_eventMarkers.push_back(event);
    }

    for (size_t i = 0; i < m_eventMarkers.size(); ++i) {
        auto& event = m_eventMarkers[i];

        ImGui::PushID(static_cast<int>(i));

        char nameBuf[64];
        strncpy(nameBuf, event.name.c_str(), sizeof(nameBuf));
        if (ImGui::InputText("##evtname", nameBuf, sizeof(nameBuf))) {
            event.name = nameBuf;
        }

        ImGui::SameLine();
        ImGui::DragFloat("##evttime", &event.time, 0.01f, 0.0f,
                         m_previewAnimation ? m_previewAnimation->originalDuration : 10.0f);

        ImGui::SameLine();
        if (ImGui::Button("X")) {
            m_eventMarkers.erase(m_eventMarkers.begin() + i);
            --i;
        }

        ImGui::PopID();
    }
    */
}

void AnimationImportPanel::RenderStatistics() {
    /*
    ImGui::Text("Statistics");
    ImGui::Separator();

    if (m_previewAnimation) {
        ImGui::Text("Duration: %.2f s", m_previewAnimation->originalDuration);
        ImGui::Text("Sample Rate: %.0f fps", m_previewAnimation->originalSampleRate);
        ImGui::Text("Total Clips: %u", m_previewAnimation->totalClips);
        ImGui::Text("Total Channels: %u", m_previewAnimation->totalChannels);
        ImGui::Text("Total Keyframes: %u", m_previewAnimation->totalKeyframes);

        ImGui::Separator();
        ImGui::Text("Bones: %zu", m_previewAnimation->boneNames.size());

        // Compression stats
        if (m_previewAnimation->compressionRatio < 1.0f) {
            ImGui::Text("Compression: %.1f%%",
                        (1.0f - m_previewAnimation->compressionRatio) * 100.0f);
        }
    }
    */
}

void AnimationImportPanel::UpdatePreview() {
    // Regenerate preview data based on current settings
}

void AnimationImportPanel::LoadPreviewAnimation() {
    if (m_animationPath.empty()) return;

    Nova::AnimationImporter importer;
    auto result = importer.Import(m_animationPath);

    if (result.success) {
        m_previewAnimation = std::make_unique<Nova::ImportedAnimation>(std::move(result));

        // Setup initial clip markers
        m_clipMarkers.clear();
        for (const auto& clip : m_previewAnimation->clips) {
            ClipMarker marker;
            marker.name = clip.name;
            marker.startTime = clip.startTime;
            marker.endTime = clip.endTime;
            marker.color = glm::vec4(0.3f, 0.6f, 0.9f, 0.8f);
            marker.selected = false;
            m_clipMarkers.push_back(marker);
        }

        // Extract events
        m_eventMarkers.clear();
        for (const auto& clip : m_previewAnimation->clips) {
            for (const auto& event : clip.events) {
                EventMarker em;
                em.name = event.name;
                em.time = event.time;
                em.color = glm::vec4(0.9f, 0.6f, 0.2f, 1.0f);
                m_eventMarkers.push_back(em);
            }
        }
    }
}

} // namespace Vehement
