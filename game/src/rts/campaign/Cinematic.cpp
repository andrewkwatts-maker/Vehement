#include "Cinematic.hpp"
#include "engine/core/json_wrapper.hpp"
#include <algorithm>
#include <sstream>
#include <fstream>

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

    // Process audio cues
    for (const auto& cue : scene->audioCues) {
        if (sceneTime >= cue.startTime &&
            sceneTime < cue.startTime + 0.1f) {  // Small window to avoid re-triggering
            // Audio cue processing is handled by the CinematicPlayer
        }
    }

    // Process unit animations
    for (const auto& anim : scene->unitAnimations) {
        if (sceneTime >= anim.startTime &&
            sceneTime < anim.startTime + anim.duration) {
            // Unit animation processing - the CinematicPlayer will handle the actual animation
        }
    }

    // Process visual effects
    for (const auto& effect : scene->effects) {
        if (sceneTime >= effect.startTime &&
            sceneTime < effect.startTime + effect.duration) {
            // Visual effect processing - the CinematicPlayer will handle the actual effects
        }
    }
}

void Cinematic::ProcessSceneTransition() {
    AdvanceToNextScene();
}

void Cinematic::TriggerSceneEvents(const CinematicScene& scene) {
    // Execute scene init script
    if (!scene.initScript.empty()) {
        // Script execution would be handled by the scripting system
        // For now, we note that a script is ready to execute
    }

    // Start scene audio (background music if specified)
    for (const auto& cue : scene.audioCues) {
        if (cue.startTime <= 0.0f) {
            // Audio cues at scene start are triggered immediately
            // Actual playback is handled by CinematicPlayer
        }
    }

    // Configure camera based on scene camera movement type
    if (!scene.camera.keyframes.empty()) {
        // Camera configuration is handled by the CinematicPlayer
        // through the InterpolateCamera method
    }
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

bool Cinematic::Deserialize(const std::string& jsonStr) {
    try {
        auto doc = Nova::Json::Parse(jsonStr);

        id = Nova::Json::Get<std::string>(doc, "id", id);
        title = Nova::Json::Get<std::string>(doc, "title", "");
        description = Nova::Json::Get<std::string>(doc, "description", "");
        canSkip = Nova::Json::Get<bool>(doc, "canSkip", true);
        skipDelay = Nova::Json::Get<float>(doc, "skipDelay", 2.0f);
        backgroundMusic = Nova::Json::Get<std::string>(doc, "backgroundMusic", "");
        musicVolume = Nova::Json::Get<float>(doc, "musicVolume", 0.8f);
        fadeInMusic = Nova::Json::Get<bool>(doc, "fadeInMusic", true);
        fadeOutMusic = Nova::Json::Get<bool>(doc, "fadeOutMusic", true);
        mapFile = Nova::Json::Get<std::string>(doc, "mapFile", "");
        useGameWorld = Nova::Json::Get<bool>(doc, "useGameWorld", true);

        // Parse scenes
        if (doc.contains("scenes") && doc["scenes"].is_array()) {
            scenes.clear();
            for (const auto& sceneJson : doc["scenes"]) {
                CinematicScene scene;
                scene.id = Nova::Json::Get<std::string>(sceneJson, "id", "");
                scene.name = Nova::Json::Get<std::string>(sceneJson, "name", "");
                scene.startTime = Nova::Json::Get<float>(sceneJson, "startTime", 0.0f);
                scene.duration = Nova::Json::Get<float>(sceneJson, "duration", 0.0f);
                scene.initScript = Nova::Json::Get<std::string>(sceneJson, "initScript", "");
                scene.updateScript = Nova::Json::Get<std::string>(sceneJson, "updateScript", "");
                scene.endScript = Nova::Json::Get<std::string>(sceneJson, "endScript", "");
                scene.skybox = Nova::Json::Get<std::string>(sceneJson, "skybox", "");
                scene.lighting = Nova::Json::Get<std::string>(sceneJson, "lighting", "");
                scene.timeOfDay = Nova::Json::Get<float>(sceneJson, "timeOfDay", 12.0f);
                scene.weather = Nova::Json::Get<std::string>(sceneJson, "weather", "");
                scene.transitionDuration = Nova::Json::Get<float>(sceneJson, "transitionDuration", 0.5f);

                // Parse dialogs
                if (sceneJson.contains("dialogs") && sceneJson["dialogs"].is_array()) {
                    for (const auto& dialogJson : sceneJson["dialogs"]) {
                        CinematicDialog dialog;
                        dialog.characterId = Nova::Json::Get<std::string>(dialogJson, "characterId", "");
                        dialog.characterName = Nova::Json::Get<std::string>(dialogJson, "characterName", "");
                        dialog.text = Nova::Json::Get<std::string>(dialogJson, "text", "");
                        dialog.voiceoverFile = Nova::Json::Get<std::string>(dialogJson, "voiceoverFile", "");
                        dialog.startTime = Nova::Json::Get<float>(dialogJson, "startTime", 0.0f);
                        dialog.duration = Nova::Json::Get<float>(dialogJson, "duration", 0.0f);
                        dialog.portraitImage = Nova::Json::Get<std::string>(dialogJson, "portraitImage", "");
                        dialog.emotion = Nova::Json::Get<std::string>(dialogJson, "emotion", "neutral");
                        dialog.showSubtitle = Nova::Json::Get<bool>(dialogJson, "showSubtitle", true);
                        dialog.position = Nova::Json::Get<std::string>(dialogJson, "position", "center");
                        scene.dialogs.push_back(dialog);
                    }
                }

                // Parse audio cues
                if (sceneJson.contains("audioCues") && sceneJson["audioCues"].is_array()) {
                    for (const auto& cueJson : sceneJson["audioCues"]) {
                        AudioCue cue;
                        cue.audioFile = Nova::Json::Get<std::string>(cueJson, "audioFile", "");
                        cue.startTime = Nova::Json::Get<float>(cueJson, "startTime", 0.0f);
                        cue.volume = Nova::Json::Get<float>(cueJson, "volume", 1.0f);
                        cue.fadeIn = Nova::Json::Get<float>(cueJson, "fadeIn", 0.0f);
                        cue.fadeOut = Nova::Json::Get<float>(cueJson, "fadeOut", 0.0f);
                        cue.loop = Nova::Json::Get<bool>(cueJson, "loop", false);
                        cue.isMusic = Nova::Json::Get<bool>(cueJson, "isMusic", false);
                        cue.channel = Nova::Json::Get<std::string>(cueJson, "channel", "");
                        scene.audioCues.push_back(cue);
                    }
                }

                // Parse camera movement
                if (sceneJson.contains("camera") && sceneJson["camera"].is_object()) {
                    const auto& camJson = sceneJson["camera"];
                    scene.camera.duration = Nova::Json::Get<float>(camJson, "duration", 0.0f);
                    scene.camera.followTargetId = Nova::Json::Get<std::string>(camJson, "followTargetId", "");
                    scene.camera.followDistance = Nova::Json::Get<float>(camJson, "followDistance", 10.0f);
                    scene.camera.shakeIntensity = Nova::Json::Get<float>(camJson, "shakeIntensity", 0.0f);
                    scene.camera.shakeDuration = Nova::Json::Get<float>(camJson, "shakeDuration", 0.0f);

                    std::string typeStr = Nova::Json::Get<std::string>(camJson, "type", "static");
                    if (typeStr == "pan") scene.camera.type = CameraMovementType::Pan;
                    else if (typeStr == "zoom") scene.camera.type = CameraMovementType::Zoom;
                    else if (typeStr == "orbit") scene.camera.type = CameraMovementType::Orbit;
                    else if (typeStr == "shake") scene.camera.type = CameraMovementType::Shake;
                    else if (typeStr == "follow") scene.camera.type = CameraMovementType::Follow;
                    else if (typeStr == "bezier") scene.camera.type = CameraMovementType::Bezier;
                    else if (typeStr == "custom") scene.camera.type = CameraMovementType::Custom;
                    else scene.camera.type = CameraMovementType::Static;

                    // Parse keyframes
                    if (camJson.contains("keyframes") && camJson["keyframes"].is_array()) {
                        for (const auto& kfJson : camJson["keyframes"]) {
                            CameraKeyframe kf;
                            kf.time = Nova::Json::Get<float>(kfJson, "time", 0.0f);
                            kf.easing = Nova::Json::Get<float>(kfJson, "easing", 1.0f);
                            kf.easingType = Nova::Json::Get<std::string>(kfJson, "easingType", "linear");

                            if (kfJson.contains("position") && kfJson["position"].is_object()) {
                                const auto& posJson = kfJson["position"];
                                kf.position.x = Nova::Json::Get<float>(posJson, "x", 0.0f);
                                kf.position.y = Nova::Json::Get<float>(posJson, "y", 0.0f);
                                kf.position.z = Nova::Json::Get<float>(posJson, "z", 0.0f);
                                kf.position.pitch = Nova::Json::Get<float>(posJson, "pitch", 0.0f);
                                kf.position.yaw = Nova::Json::Get<float>(posJson, "yaw", 0.0f);
                                kf.position.roll = Nova::Json::Get<float>(posJson, "roll", 0.0f);
                                kf.position.fov = Nova::Json::Get<float>(posJson, "fov", 60.0f);
                            }

                            scene.camera.keyframes.push_back(kf);
                        }
                    }
                }

                scenes.push_back(scene);
            }
        }

        // Parse title card
        if (doc.contains("titleCard") && doc["titleCard"].is_object()) {
            hasTitleCard = true;
            const auto& tcJson = doc["titleCard"];
            titleCard.title = Nova::Json::Get<std::string>(tcJson, "title", "");
            titleCard.subtitle = Nova::Json::Get<std::string>(tcJson, "subtitle", "");
            titleCard.backgroundImage = Nova::Json::Get<std::string>(tcJson, "backgroundImage", "");
            titleCard.duration = Nova::Json::Get<float>(tcJson, "duration", 5.0f);
            titleCard.font = Nova::Json::Get<std::string>(tcJson, "font", "");
            titleCard.textColor = Nova::Json::Get<std::string>(tcJson, "textColor", "");
            titleCard.animationType = Nova::Json::Get<std::string>(tcJson, "animationType", "fade-in");
        }

        CalculateDuration();
        return true;
    } catch (const Nova::Json::JsonException& /*e*/) {
        return false;
    }
}

// CinematicFactory implementations

std::unique_ptr<Cinematic> CinematicFactory::CreateFromJson(const std::string& jsonPath) {
    auto jsonOpt = Nova::Json::TryParseFile(jsonPath);
    if (!jsonOpt.has_value()) {
        return nullptr;
    }

    auto cinematic = std::make_unique<Cinematic>();
    std::string jsonStr = Nova::Json::Stringify(jsonOpt.value());
    if (!cinematic->Deserialize(jsonStr)) {
        return nullptr;
    }

    return cinematic;
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
