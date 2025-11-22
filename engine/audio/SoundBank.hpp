#pragma once

#include "audio/AudioEngine.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <random>
#include <nlohmann/json.hpp>

namespace Nova {

// ============================================================================
// Sound Variation
// ============================================================================

/**
 * @brief Parameters for sound variation/randomization
 */
struct SoundVariation {
    float volumeMin = 1.0f;      ///< Minimum volume multiplier
    float volumeMax = 1.0f;      ///< Maximum volume multiplier
    float pitchMin = 1.0f;       ///< Minimum pitch multiplier
    float pitchMax = 1.0f;       ///< Maximum pitch multiplier

    /**
     * @brief Get randomized volume
     */
    [[nodiscard]] float GetRandomVolume() const {
        if (volumeMin == volumeMax) return volumeMin;
        static thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<float> dist(volumeMin, volumeMax);
        return dist(gen);
    }

    /**
     * @brief Get randomized pitch
     */
    [[nodiscard]] float GetRandomPitch() const {
        if (pitchMin == pitchMax) return pitchMin;
        static thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<float> dist(pitchMin, pitchMax);
        return dist(gen);
    }
};

// ============================================================================
// Sound Entry
// ============================================================================

/**
 * @brief A single sound entry with multiple variations
 */
struct SoundEntry {
    std::string name;                              ///< Sound identifier
    std::vector<std::string> variations;           ///< Paths to variation files
    SoundVariation params;                         ///< Randomization parameters
    std::string outputBus = "sfx";                 ///< Output bus name
    float cooldown = 0.0f;                         ///< Minimum time between plays
    int maxInstances = 0;                          ///< Max simultaneous instances (0 = unlimited)
    bool loop = false;                             ///< Loop by default
    bool is3D = true;                              ///< 3D spatialization

    // Runtime state
    mutable size_t lastVariationIndex = 0;
    mutable float lastPlayTime = -1000.0f;
    mutable int currentInstances = 0;

    /**
     * @brief Get the next variation to play (round-robin or random)
     */
    [[nodiscard]] size_t GetNextVariation(bool sequential = false) const {
        if (variations.empty()) return 0;

        if (sequential) {
            lastVariationIndex = (lastVariationIndex + 1) % variations.size();
            return lastVariationIndex;
        } else {
            static thread_local std::mt19937 gen(std::random_device{}());
            std::uniform_int_distribution<size_t> dist(0, variations.size() - 1);
            // Avoid repeating the same variation
            size_t index = dist(gen);
            if (variations.size() > 1 && index == lastVariationIndex) {
                index = (index + 1) % variations.size();
            }
            lastVariationIndex = index;
            return index;
        }
    }

    /**
     * @brief Check if the sound can be played (cooldown, max instances)
     */
    [[nodiscard]] bool CanPlay(float currentTime) const {
        if (cooldown > 0.0f && (currentTime - lastPlayTime) < cooldown) {
            return false;
        }
        if (maxInstances > 0 && currentInstances >= maxInstances) {
            return false;
        }
        return true;
    }
};

// ============================================================================
// Sound Group
// ============================================================================

/**
 * @brief A group of related sounds with shared settings
 */
struct SoundGroup {
    std::string name;
    std::vector<std::string> soundNames;  ///< Names of sounds in this group
    float volumeMultiplier = 1.0f;        ///< Volume multiplier for all sounds
    bool muted = false;                   ///< Mute all sounds in group
    std::string outputBus = "sfx";        ///< Default output bus for group

    /**
     * @brief Get a random sound from the group
     */
    [[nodiscard]] const std::string& GetRandomSound() const {
        if (soundNames.empty()) {
            static std::string empty;
            return empty;
        }
        static thread_local std::mt19937 gen(std::random_device{}());
        std::uniform_int_distribution<size_t> dist(0, soundNames.size() - 1);
        return soundNames[dist(gen)];
    }
};

// ============================================================================
// Sound Bank
// ============================================================================

/**
 * @brief Sound asset management with variation support
 *
 * SoundBank provides high-level sound management including:
 * - Sound variations (multiple files for the same logical sound)
 * - Randomization (volume, pitch)
 * - Sound groups for bulk control
 * - JSON configuration loading
 * - Cooldown and instance limiting
 *
 * Usage:
 * @code
 * SoundBank bank;
 * bank.LoadFromFile("sounds.json");
 *
 * // Play a sound with automatic variation selection
 * bank.Play("explosion");
 *
 * // Play 3D sound
 * bank.Play3D("footstep", position);
 *
 * // Mute a group
 * bank.SetGroupMuted("combat", true);
 * @endcode
 */
class SoundBank {
public:
    SoundBank() = default;
    ~SoundBank() = default;

    /**
     * @brief Load sound bank from JSON file
     *
     * Expected format:
     * @code
     * {
     *   "sounds": {
     *     "explosion": {
     *       "variations": ["explosion1.ogg", "explosion2.ogg"],
     *       "volume": [0.8, 1.0],
     *       "pitch": [0.9, 1.1],
     *       "bus": "sfx",
     *       "cooldown": 0.1
     *     }
     *   },
     *   "groups": {
     *     "combat": {
     *       "sounds": ["explosion", "gunshot"],
     *       "volume": 1.0
     *     }
     *   }
     * }
     * @endcode
     */
    bool LoadFromFile(const std::string& path);

    /**
     * @brief Load from JSON object
     */
    bool LoadFromJson(const nlohmann::json& json);

    /**
     * @brief Save current configuration to JSON file
     */
    bool SaveToFile(const std::string& path) const;

    /**
     * @brief Preload all sounds in the bank
     */
    void PreloadAll();

    /**
     * @brief Unload all sounds
     */
    void UnloadAll();

    // =========== Sound Registration ===========

    /**
     * @brief Add a sound entry programmatically
     */
    void AddSound(const std::string& name, const SoundEntry& entry);

    /**
     * @brief Add a simple sound (single file, no variations)
     */
    void AddSimpleSound(const std::string& name, const std::string& path,
                        const std::string& bus = "sfx");

    /**
     * @brief Remove a sound entry
     */
    void RemoveSound(const std::string& name);

    /**
     * @brief Check if a sound exists
     */
    [[nodiscard]] bool HasSound(const std::string& name) const;

    /**
     * @brief Get a sound entry
     */
    [[nodiscard]] const SoundEntry* GetSound(const std::string& name) const;

    // =========== Group Management ===========

    /**
     * @brief Add a sound group
     */
    void AddGroup(const std::string& name, const SoundGroup& group);

    /**
     * @brief Remove a group
     */
    void RemoveGroup(const std::string& name);

    /**
     * @brief Get a group
     */
    [[nodiscard]] const SoundGroup* GetGroup(const std::string& name) const;

    /**
     * @brief Set group volume
     */
    void SetGroupVolume(const std::string& name, float volume);

    /**
     * @brief Set group muted state
     */
    void SetGroupMuted(const std::string& name, bool muted);

    // =========== Playback ===========

    /**
     * @brief Play a 2D sound by name
     * @param name Sound name
     * @param volumeOverride Optional volume override (-1 for default)
     * @return Audio source handle, nullptr if failed
     */
    std::shared_ptr<AudioSource> Play(const std::string& name,
                                       float volumeOverride = -1.0f);

    /**
     * @brief Play a 3D sound at position
     */
    std::shared_ptr<AudioSource> Play3D(const std::string& name,
                                         const glm::vec3& position,
                                         float volumeOverride = -1.0f);

    /**
     * @brief Play a random sound from a group
     */
    std::shared_ptr<AudioSource> PlayFromGroup(const std::string& groupName,
                                                float volumeOverride = -1.0f);

    /**
     * @brief Play a random sound from a group at position
     */
    std::shared_ptr<AudioSource> PlayFromGroup3D(const std::string& groupName,
                                                  const glm::vec3& position,
                                                  float volumeOverride = -1.0f);

    /**
     * @brief Stop all playing sounds from an entry
     */
    void StopSound(const std::string& name);

    /**
     * @brief Stop all sounds in a group
     */
    void StopGroup(const std::string& groupName);

    // =========== Update ===========

    /**
     * @brief Update sound bank (call each frame)
     * @param currentTime Current game time for cooldown tracking
     */
    void Update(float currentTime);

    // =========== Statistics ===========

    /**
     * @brief Get number of registered sounds
     */
    [[nodiscard]] size_t GetSoundCount() const { return m_sounds.size(); }

    /**
     * @brief Get number of groups
     */
    [[nodiscard]] size_t GetGroupCount() const { return m_groups.size(); }

    /**
     * @brief Get all sound names
     */
    [[nodiscard]] std::vector<std::string> GetSoundNames() const;

    /**
     * @brief Get all group names
     */
    [[nodiscard]] std::vector<std::string> GetGroupNames() const;

private:
    std::shared_ptr<AudioSource> PlayInternal(const SoundEntry& entry,
                                               const glm::vec3* position,
                                               float volumeOverride);

    std::unordered_map<std::string, SoundEntry> m_sounds;
    std::unordered_map<std::string, SoundGroup> m_groups;

    // Loaded buffers for each variation
    std::unordered_map<std::string, std::vector<std::shared_ptr<AudioBuffer>>> m_loadedBuffers;

    // Active sources per sound for instance tracking
    std::unordered_map<std::string, std::vector<std::weak_ptr<AudioSource>>> m_activeSources;

    float m_currentTime = 0.0f;
    std::string m_basePath;
};

// ============================================================================
// SoundBank Implementation
// ============================================================================

inline bool SoundBank::LoadFromFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }

    try {
        nlohmann::json json;
        file >> json;

        // Store base path for relative paths
        size_t lastSlash = path.find_last_of("/\\");
        m_basePath = (lastSlash != std::string::npos) ? path.substr(0, lastSlash + 1) : "";

        return LoadFromJson(json);
    } catch (const std::exception& e) {
        return false;
    }
}

inline bool SoundBank::LoadFromJson(const nlohmann::json& json) {
    // Clear existing data
    m_sounds.clear();
    m_groups.clear();

    // Load sounds
    if (json.contains("sounds")) {
        for (auto& [name, soundJson] : json["sounds"].items()) {
            SoundEntry entry;
            entry.name = name;

            // Load variations
            if (soundJson.contains("variations")) {
                for (const auto& var : soundJson["variations"]) {
                    entry.variations.push_back(m_basePath + var.get<std::string>());
                }
            } else if (soundJson.contains("file")) {
                entry.variations.push_back(m_basePath + soundJson["file"].get<std::string>());
            }

            // Load volume range
            if (soundJson.contains("volume")) {
                const auto& vol = soundJson["volume"];
                if (vol.is_array()) {
                    entry.params.volumeMin = vol[0].get<float>();
                    entry.params.volumeMax = vol[1].get<float>();
                } else {
                    entry.params.volumeMin = entry.params.volumeMax = vol.get<float>();
                }
            }

            // Load pitch range
            if (soundJson.contains("pitch")) {
                const auto& pitch = soundJson["pitch"];
                if (pitch.is_array()) {
                    entry.params.pitchMin = pitch[0].get<float>();
                    entry.params.pitchMax = pitch[1].get<float>();
                } else {
                    entry.params.pitchMin = entry.params.pitchMax = pitch.get<float>();
                }
            }

            // Load other properties
            if (soundJson.contains("bus")) {
                entry.outputBus = soundJson["bus"].get<std::string>();
            }
            if (soundJson.contains("cooldown")) {
                entry.cooldown = soundJson["cooldown"].get<float>();
            }
            if (soundJson.contains("maxInstances")) {
                entry.maxInstances = soundJson["maxInstances"].get<int>();
            }
            if (soundJson.contains("loop")) {
                entry.loop = soundJson["loop"].get<bool>();
            }
            if (soundJson.contains("is3D")) {
                entry.is3D = soundJson["is3D"].get<bool>();
            }

            m_sounds[name] = std::move(entry);
        }
    }

    // Load groups
    if (json.contains("groups")) {
        for (auto& [name, groupJson] : json["groups"].items()) {
            SoundGroup group;
            group.name = name;

            if (groupJson.contains("sounds")) {
                for (const auto& sound : groupJson["sounds"]) {
                    group.soundNames.push_back(sound.get<std::string>());
                }
            }
            if (groupJson.contains("volume")) {
                group.volumeMultiplier = groupJson["volume"].get<float>();
            }
            if (groupJson.contains("bus")) {
                group.outputBus = groupJson["bus"].get<std::string>();
            }

            m_groups[name] = std::move(group);
        }
    }

    return true;
}

inline bool SoundBank::SaveToFile(const std::string& path) const {
    nlohmann::json json;

    // Save sounds
    json["sounds"] = nlohmann::json::object();
    for (const auto& [name, entry] : m_sounds) {
        nlohmann::json soundJson;
        soundJson["variations"] = entry.variations;
        soundJson["volume"] = {entry.params.volumeMin, entry.params.volumeMax};
        soundJson["pitch"] = {entry.params.pitchMin, entry.params.pitchMax};
        soundJson["bus"] = entry.outputBus;
        soundJson["cooldown"] = entry.cooldown;
        soundJson["maxInstances"] = entry.maxInstances;
        soundJson["loop"] = entry.loop;
        soundJson["is3D"] = entry.is3D;
        json["sounds"][name] = soundJson;
    }

    // Save groups
    json["groups"] = nlohmann::json::object();
    for (const auto& [name, group] : m_groups) {
        nlohmann::json groupJson;
        groupJson["sounds"] = group.soundNames;
        groupJson["volume"] = group.volumeMultiplier;
        groupJson["bus"] = group.outputBus;
        json["groups"][name] = groupJson;
    }

    std::ofstream file(path);
    if (!file) return false;
    file << json.dump(2);
    return true;
}

inline void SoundBank::PreloadAll() {
    auto& audio = AudioEngine::Instance();

    for (auto& [name, entry] : m_sounds) {
        auto& buffers = m_loadedBuffers[name];
        buffers.clear();

        for (const auto& path : entry.variations) {
            auto buffer = audio.LoadSound(path);
            if (buffer) {
                buffers.push_back(buffer);
            }
        }
    }
}

inline void SoundBank::UnloadAll() {
    m_loadedBuffers.clear();
}

inline void SoundBank::AddSound(const std::string& name, const SoundEntry& entry) {
    m_sounds[name] = entry;
}

inline void SoundBank::AddSimpleSound(const std::string& name, const std::string& path,
                                       const std::string& bus) {
    SoundEntry entry;
    entry.name = name;
    entry.variations.push_back(path);
    entry.outputBus = bus;
    m_sounds[name] = entry;
}

inline void SoundBank::RemoveSound(const std::string& name) {
    m_sounds.erase(name);
    m_loadedBuffers.erase(name);
}

inline bool SoundBank::HasSound(const std::string& name) const {
    return m_sounds.count(name) > 0;
}

inline const SoundEntry* SoundBank::GetSound(const std::string& name) const {
    auto it = m_sounds.find(name);
    return (it != m_sounds.end()) ? &it->second : nullptr;
}

inline void SoundBank::AddGroup(const std::string& name, const SoundGroup& group) {
    m_groups[name] = group;
}

inline void SoundBank::RemoveGroup(const std::string& name) {
    m_groups.erase(name);
}

inline const SoundGroup* SoundBank::GetGroup(const std::string& name) const {
    auto it = m_groups.find(name);
    return (it != m_groups.end()) ? &it->second : nullptr;
}

inline void SoundBank::SetGroupVolume(const std::string& name, float volume) {
    auto it = m_groups.find(name);
    if (it != m_groups.end()) {
        it->second.volumeMultiplier = std::clamp(volume, 0.0f, 2.0f);
    }
}

inline void SoundBank::SetGroupMuted(const std::string& name, bool muted) {
    auto it = m_groups.find(name);
    if (it != m_groups.end()) {
        it->second.muted = muted;
    }
}

inline std::shared_ptr<AudioSource> SoundBank::Play(const std::string& name,
                                                      float volumeOverride) {
    auto it = m_sounds.find(name);
    if (it == m_sounds.end()) return nullptr;
    return PlayInternal(it->second, nullptr, volumeOverride);
}

inline std::shared_ptr<AudioSource> SoundBank::Play3D(const std::string& name,
                                                        const glm::vec3& position,
                                                        float volumeOverride) {
    auto it = m_sounds.find(name);
    if (it == m_sounds.end()) return nullptr;
    return PlayInternal(it->second, &position, volumeOverride);
}

inline std::shared_ptr<AudioSource> SoundBank::PlayFromGroup(const std::string& groupName,
                                                               float volumeOverride) {
    auto groupIt = m_groups.find(groupName);
    if (groupIt == m_groups.end()) return nullptr;

    const auto& soundName = groupIt->second.GetRandomSound();
    if (soundName.empty()) return nullptr;

    float effectiveVolume = (volumeOverride >= 0.0f) ? volumeOverride : 1.0f;
    effectiveVolume *= groupIt->second.volumeMultiplier;

    return Play(soundName, effectiveVolume);
}

inline std::shared_ptr<AudioSource> SoundBank::PlayFromGroup3D(const std::string& groupName,
                                                                 const glm::vec3& position,
                                                                 float volumeOverride) {
    auto groupIt = m_groups.find(groupName);
    if (groupIt == m_groups.end()) return nullptr;

    const auto& soundName = groupIt->second.GetRandomSound();
    if (soundName.empty()) return nullptr;

    float effectiveVolume = (volumeOverride >= 0.0f) ? volumeOverride : 1.0f;
    effectiveVolume *= groupIt->second.volumeMultiplier;

    return Play3D(soundName, position, effectiveVolume);
}

inline void SoundBank::StopSound(const std::string& name) {
    auto it = m_activeSources.find(name);
    if (it != m_activeSources.end()) {
        for (auto& weakSource : it->second) {
            if (auto source = weakSource.lock()) {
                source->Stop();
            }
        }
        it->second.clear();
    }
}

inline void SoundBank::StopGroup(const std::string& groupName) {
    auto groupIt = m_groups.find(groupName);
    if (groupIt == m_groups.end()) return;

    for (const auto& soundName : groupIt->second.soundNames) {
        StopSound(soundName);
    }
}

inline void SoundBank::Update(float currentTime) {
    m_currentTime = currentTime;

    // Clean up expired sources and update instance counts
    for (auto& [name, sources] : m_activeSources) {
        auto it = m_sounds.find(name);
        if (it != m_sounds.end()) {
            int activeCount = 0;
            sources.erase(
                std::remove_if(sources.begin(), sources.end(),
                    [&activeCount](const std::weak_ptr<AudioSource>& weak) {
                        if (auto source = weak.lock()) {
                            if (source->IsPlaying()) {
                                ++activeCount;
                                return false;
                            }
                        }
                        return true;
                    }),
                sources.end());
            it->second.currentInstances = activeCount;
        }
    }
}

inline std::vector<std::string> SoundBank::GetSoundNames() const {
    std::vector<std::string> names;
    names.reserve(m_sounds.size());
    for (const auto& [name, _] : m_sounds) {
        names.push_back(name);
    }
    return names;
}

inline std::vector<std::string> SoundBank::GetGroupNames() const {
    std::vector<std::string> names;
    names.reserve(m_groups.size());
    for (const auto& [name, _] : m_groups) {
        names.push_back(name);
    }
    return names;
}

inline std::shared_ptr<AudioSource> SoundBank::PlayInternal(const SoundEntry& entry,
                                                             const glm::vec3* position,
                                                             float volumeOverride) {
    // Check cooldown and max instances
    if (!entry.CanPlay(m_currentTime)) {
        return nullptr;
    }

    // Get buffer (load if needed)
    auto& buffers = m_loadedBuffers[entry.name];
    if (buffers.empty()) {
        auto& audio = AudioEngine::Instance();
        for (const auto& path : entry.variations) {
            auto buffer = audio.LoadSound(path);
            if (buffer) {
                buffers.push_back(buffer);
            }
        }
    }

    if (buffers.empty()) {
        return nullptr;
    }

    // Select variation
    size_t varIndex = entry.GetNextVariation();
    if (varIndex >= buffers.size()) varIndex = 0;

    // Calculate volume and pitch
    float volume = (volumeOverride >= 0.0f) ? volumeOverride : entry.params.GetRandomVolume();
    float pitch = entry.params.GetRandomPitch();

    // Play the sound
    auto& audio = AudioEngine::Instance();
    std::shared_ptr<AudioSource> source;

    if (position && entry.is3D) {
        source = audio.PlayConfigured(buffers[varIndex], *position, volume, pitch,
                                       entry.loop, entry.outputBus);
    } else {
        source = audio.Play2D(buffers[varIndex], volume, pitch);
        if (source) {
            source->SetLooping(entry.loop);
            source->SetOutputBus(entry.outputBus);
        }
    }

    if (source) {
        // Track for instance counting
        entry.lastPlayTime = m_currentTime;
        m_activeSources[entry.name].push_back(source);
    }

    return source;
}

} // namespace Nova
