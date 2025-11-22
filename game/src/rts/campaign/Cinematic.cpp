#include "Cinematic.hpp"
#include <algorithm>
#include <sstream>

namespace Vehement {
namespace RTS {
namespace Campaign {

Cinematic::Cinematic(const std::string& cinematicId) : id(cinematicId) {}

void Cinematic::Initialize() {
    currentTime = 0.0f;
    currentSceneIndex = 0;
    isPlaying = false;
    isPaused = false;
    isSkipped = false;
    CalculateDuration();
}

void Cinematic::Start() {
    Initialize();
    isPlaying = true;

    if (onStart) {
        onStart(*this);
    }

    if (!scenes.empty()) {
        TriggerSceneEvents(scenes[0]);
    }
}

void Cinematic::Update(float deltaTime) {
    if (!isPlaying || isPaused) return;

    currentTime += deltaTime;

    UpdateCurrentScene(deltaTime);

    // Check for scene transitions
    if (currentSceneIndex < static_cast<int32_t>(scenes.size())) {
        const auto& scene = scenes[currentSceneIndex];
        float sceneEndTime = scene.startTime + scene.duration;

        if (currentTime >= sceneEndTime) {
            ProcessSceneTransition();
        }
    }

    // Check for completion
    if (IsComplete()) {
        Stop();
    }
}

void Cinematic::Pause() {
    isPaused = true;
}

void Cinematic::Resume() {
    isPaused = false;
}

void Cinematic::Skip() {
    if (!canSkip || !CanSkipNow()) return;

    isSkipped = true;
    isPlaying = false;

    if (onSkip) {
        onSkip(*this);
    }

    if (onEnd) {
        onEnd(*this);
    }
}

void Cinematic::Stop() {
    isPlaying = false;

    if (onEnd && !isSkipped) {
        onEnd(*this);
    }
}

void Cinematic::Reset() {
    Initialize();
}

CinematicScene* Cinematic::GetCurrentScene() {
    if (currentSceneIndex >= 0 && currentSceneIndex < static_cast<int32_t>(scenes.size())) {
        return &scenes[currentSceneIndex];
    }
    return nullptr;
}

const CinematicScene* Cinematic::GetCurrentScene() const {
    if (currentSceneIndex >= 0 && currentSceneIndex < static_cast<int32_t>(scenes.size())) {
        return &scenes[currentSceneIndex];
    }
    return nullptr;
}

CinematicScene* Cinematic::GetScene(const std::string& sceneId) {
    auto it = std::find_if(scenes.begin(), scenes.end(),
        [&sceneId](const CinematicScene& s) { return s.id == sceneId; });
    return (it != scenes.end()) ? &(*it) : nullptr;
}

void Cinematic::AdvanceToNextScene() {
    currentSceneIndex++;

    if (currentSceneIndex < static_cast<int32_t>(scenes.size())) {
        if (onSceneChange) {
            onSceneChange(*this, currentSceneIndex);
        }
        TriggerSceneEvents(scenes[currentSceneIndex]);
    }
}

void Cinematic::GoToScene(const std::string& sceneId) {
    for (size_t i = 0; i < scenes.size(); ++i) {
        if (scenes[i].id == sceneId) {
            GoToScene(static_cast<int32_t>(i));
            break;
        }
    }
}

void Cinematic::GoToScene(int32_t index) {
    if (index >= 0 && index < static_cast<int32_t>(scenes.size())) {
        currentSceneIndex = index;
        currentTime = scenes[index].startTime;

        if (onSceneChange) {
            onSceneChange(*this, currentSceneIndex);
        }
        TriggerSceneEvents(scenes[currentSceneIndex]);
    }
}

void Cinematic::AddScene(const CinematicScene& scene) {
    scenes.push_back(scene);
    CalculateDuration();
}

void Cinematic::RemoveScene(const std::string& sceneId) {
    auto it = std::find_if(scenes.begin(), scenes.end(),
        [&sceneId](const CinematicScene& s) { return s.id == sceneId; });

    if (it != scenes.end()) {
        scenes.erase(it);
        CalculateDuration();
    }
}

float Cinematic::GetProgress() const {
    if (totalDuration <= 0.0f) return 1.0f;
    return currentTime / totalDuration;
}

bool Cinematic::IsComplete() const {
    return currentTime >= totalDuration || isSkipped;
}

bool Cinematic::CanSkipNow() const {
    return canSkip && currentTime >= skipDelay;
}

void Cinematic::CalculateDuration() {
    totalDuration = 0.0f;

    for (const auto& scene : scenes) {
        float sceneEnd = scene.startTime + scene.duration;
        if (sceneEnd > totalDuration) {
            totalDuration = sceneEnd;
        }
    }

    if (hasTitleCard) {
        totalDuration += titleCard.duration;
    }
}

void Cinematic::UpdateCurrentScene(float /*deltaTime*/) {
    auto* scene = GetCurrentScene();
    if (!scene) return;

    float sceneTime = currentTime - scene->startTime;

    // Process dialogs
    for (const auto& dialog : scene->dialogs) {
        if (sceneTime >= dialog.startTime &&
            sceneTime < dialog.startTime + dialog.duration) {
            if (onDialog) {
                onDialog(*this, dialog);
            }
        }
    }

    // TODO: Process audio cues, unit animations, effects
}

void Cinematic::ProcessSceneTransition() {
    AdvanceToNextScene();
}

void Cinematic::TriggerSceneEvents(const CinematicScene& /*scene*/) {
    // TODO: Execute scene init script
    // TODO: Start scene audio
    // TODO: Configure camera
}

std::string Cinematic::Serialize() const {
    std::ostringstream oss;
    oss << "{";
    oss << "\"id\":\"" << id << "\",";
    oss << "\"title\":\"" << title << "\",";
    oss << "\"totalDuration\":" << totalDuration << ",";
    oss << "\"canSkip\":" << (canSkip ? "true" : "false") << ",";
    oss << "\"scenes\":[";
    for (size_t i = 0; i < scenes.size(); ++i) {
        if (i > 0) oss << ",";
        oss << "{";
        oss << "\"id\":\"" << scenes[i].id << "\",";
        oss << "\"name\":\"" << scenes[i].name << "\",";
        oss << "\"startTime\":" << scenes[i].startTime << ",";
        oss << "\"duration\":" << scenes[i].duration;
        oss << "}";
    }
    oss << "]}";
    return oss.str();
}

bool Cinematic::Deserialize(const std::string& /*json*/) {
    // TODO: Implement JSON parsing
    return true;
}

// CinematicFactory implementations

std::unique_ptr<Cinematic> CinematicFactory::CreateFromJson(const std::string& /*jsonPath*/) {
    // TODO: Load and parse JSON file
    return std::make_unique<Cinematic>();
}

std::unique_ptr<Cinematic> CinematicFactory::CreateSimple(
    const std::string& id,
    const std::string& title,
    float duration) {
    auto cinematic = std::make_unique<Cinematic>(id);
    cinematic->title = title;
    cinematic->totalDuration = duration;
    return cinematic;
}

CinematicScene CinematicFactory::CreateDialogScene(
    const std::string& characterId,
    const std::string& text,
    float duration) {
    CinematicScene scene;
    scene.id = "dialog_" + characterId;
    scene.duration = duration;

    CinematicDialog dialog;
    dialog.characterId = characterId;
    dialog.text = text;
    dialog.startTime = 0.0f;
    dialog.duration = duration;
    scene.dialogs.push_back(dialog);

    return scene;
}

CinematicScene CinematicFactory::CreateCameraPanScene(
    const CinematicPosition& start,
    const CinematicPosition& end,
    float duration) {
    CinematicScene scene;
    scene.id = "camera_pan";
    scene.duration = duration;

    scene.camera.type = CameraMovementType::Pan;
    scene.camera.duration = duration;

    CameraKeyframe startFrame;
    startFrame.time = 0.0f;
    startFrame.position = start;
    scene.camera.keyframes.push_back(startFrame);

    CameraKeyframe endFrame;
    endFrame.time = duration;
    endFrame.position = end;
    scene.camera.keyframes.push_back(endFrame);

    return scene;
}

} // namespace Campaign
} // namespace RTS
} // namespace Vehement
