#include "TickScheduler.hpp"
#include <algorithm>
#include <chrono>
#include <mutex>

namespace Vehement {
namespace Lifecycle {

// ============================================================================
// TickScheduler Implementation
// ============================================================================

TickScheduler::TickScheduler() {
    // Pre-allocate some space
    for (auto& group : m_groups) {
        group.entries.reserve(256);
    }
    m_handleToEntry.reserve(1024);
}

TickScheduler::~TickScheduler() = default;

TickHandle TickScheduler::Register(ILifecycle* object, const TickConfig& config) {
    if (!object) {
        return TickHandle::Invalid;
    }

    // Get or create index
    uint32_t index;
    if (!m_freeIndices.empty()) {
        index = m_freeIndices.back();
        m_freeIndices.pop_back();
    } else {
        index = static_cast<uint32_t>(m_handleToEntry.size());
        m_handleToEntry.push_back(nullptr);
    }

    // Create entry
    TickEntry entry;
    entry.object = object;
    entry.tickFunc = nullptr;
    entry.userData = nullptr;
    entry.config = config;
    entry.generation = m_nextGeneration++;
    entry.lastTickTime = 0.0;
    entry.pendingRemoval = false;

    // Add to appropriate group
    auto& group = m_groups[static_cast<size_t>(config.group)];
    group.entries.push_back(std::move(entry));
    group.needsSort = true;

    // Store pointer for handle lookup
    m_handleToEntry[index] = &group.entries.back();

    // Return handle
    TickHandle handle;
    handle.index = index;
    handle.generation = group.entries.back().generation;
    return handle;
}

TickHandle TickScheduler::RegisterFunction(TickConfig::TickFunction tickFunc,
                                           void* userData,
                                           const TickConfig& config) {
    if (!tickFunc) {
        return TickHandle::Invalid;
    }

    // Get or create index
    uint32_t index;
    if (!m_freeIndices.empty()) {
        index = m_freeIndices.back();
        m_freeIndices.pop_back();
    } else {
        index = static_cast<uint32_t>(m_handleToEntry.size());
        m_handleToEntry.push_back(nullptr);
    }

    // Create entry
    TickEntry entry;
    entry.object = nullptr;
    entry.tickFunc = tickFunc;
    entry.userData = userData;
    entry.config = config;
    entry.generation = m_nextGeneration++;
    entry.lastTickTime = 0.0;
    entry.pendingRemoval = false;

    // Add to appropriate group
    auto& group = m_groups[static_cast<size_t>(config.group)];
    group.entries.push_back(std::move(entry));
    group.needsSort = true;

    // Store pointer for handle lookup
    m_handleToEntry[index] = &group.entries.back();

    // Return handle
    TickHandle handle;
    handle.index = index;
    handle.generation = group.entries.back().generation;
    return handle;
}

void TickScheduler::Unregister(TickHandle handle) {
    TickEntry* entry = GetEntry(handle);
    if (!entry) return;

    entry->pendingRemoval = true;
    m_freeIndices.push_back(handle.index);
    m_handleToEntry[handle.index] = nullptr;
}

void TickScheduler::UpdateConfig(TickHandle handle, const TickConfig& config) {
    TickEntry* entry = GetEntry(handle);
    if (!entry) return;

    TickGroup oldGroup = entry->config.group;
    entry->config = config;

    // If group changed, need to move entry
    if (oldGroup != config.group) {
        // Mark for removal from old group
        entry->pendingRemoval = true;

        // Re-register in new group
        TickEntry newEntry = *entry;
        newEntry.pendingRemoval = false;
        newEntry.config.group = config.group;

        auto& group = m_groups[static_cast<size_t>(config.group)];
        group.entries.push_back(std::move(newEntry));
        group.needsSort = true;
        m_handleToEntry[handle.index] = &group.entries.back();
    } else {
        m_groups[static_cast<size_t>(config.group)].needsSort = true;
    }
}

void TickScheduler::SetEnabled(TickHandle handle, bool enabled) {
    TickEntry* entry = GetEntry(handle);
    if (entry) {
        entry->config.enabled = enabled;
    }
}

bool TickScheduler::IsValid(TickHandle handle) const {
    return GetEntry(handle) != nullptr;
}

void TickScheduler::Tick(float deltaTime) {
    auto frameStart = std::chrono::high_resolution_clock::now();

    // Update time state
    m_timeState.unscaledDeltaTime = deltaTime;
    m_timeState.deltaTime = deltaTime * m_timeState.timeScale;

    if (!m_timeState.isPaused) {
        m_timeState.totalTime += m_timeState.deltaTime;
    }

    m_timeState.frameCount++;

    // Cleanup pending removals first
    CleanupPendingRemovals();

    // Tick each group in order
    float scaledDelta = static_cast<float>(m_timeState.deltaTime);
    for (size_t i = 0; i < static_cast<size_t>(TickGroup::COUNT); ++i) {
        TickGroup(static_cast<TickGroup>(i), scaledDelta);
    }

    // Update frame timing
    auto frameEnd = std::chrono::high_resolution_clock::now();
    m_timeState.lastTickDuration = std::chrono::duration<double>(frameEnd - frameStart).count();

    // Exponential moving average for average duration
    constexpr double alpha = 0.1;
    m_timeState.averageTickDuration = alpha * m_timeState.lastTickDuration +
                                      (1.0 - alpha) * m_timeState.averageTickDuration;
}

void TickScheduler::TickGroup(TickGroup group, float deltaTime) {
    auto& groupData = m_groups[static_cast<size_t>(group)];

    // Sort if needed
    if (groupData.needsSort) {
        SortGroup(group);
    }

    auto groupStart = std::chrono::high_resolution_clock::now();

    groupData.stats.objectCount = groupData.entries.size();
    groupData.stats.tickedCount = 0;
    groupData.stats.maxDuration = 0.0;

    for (auto& entry : groupData.entries) {
        if (entry.pendingRemoval) continue;
        if (!entry.config.enabled) continue;

        // Skip if paused and not tickWhilePaused
        if (m_timeState.isPaused && !entry.config.tickWhilePaused) {
            continue;
        }

        // Check interval
        if (entry.config.interval > 0.0f) {
            double timeSinceLastTick = m_timeState.totalTime - entry.lastTickTime;
            if (timeSinceLastTick < entry.config.interval) {
                continue;
            }
        }

        // Execute tick
        auto tickStart = std::chrono::high_resolution_clock::now();

        TickEntry(entry, deltaTime);
        entry.lastTickTime = m_timeState.totalTime;
        m_timeState.tickCount++;
        groupData.stats.tickedCount++;

        if (m_profilingEnabled) {
            auto tickEnd = std::chrono::high_resolution_clock::now();
            entry.lastDuration = std::chrono::duration<double>(tickEnd - tickStart).count();
            groupData.stats.maxDuration = std::max(groupData.stats.maxDuration, entry.lastDuration);
        }
    }

    if (m_profilingEnabled) {
        auto groupEnd = std::chrono::high_resolution_clock::now();
        groupData.stats.totalDuration = std::chrono::duration<double>(groupEnd - groupStart).count();
        if (groupData.stats.tickedCount > 0) {
            groupData.stats.averageDuration = groupData.stats.totalDuration / groupData.stats.tickedCount;
        }
    }
}

uint32_t TickScheduler::TickFixed(float deltaTime) {
    m_timeState.accumulator += deltaTime;
    m_timeState.fixedStepsThisFrame = 0;

    while (m_timeState.accumulator >= m_timeState.fixedDeltaTime &&
           m_timeState.fixedStepsThisFrame < m_maxFixedStepsPerFrame) {

        // Tick physics group at fixed rate
        TickGroup(TickGroup::Physics, static_cast<float>(m_timeState.fixedDeltaTime));

        m_timeState.accumulator -= m_timeState.fixedDeltaTime;
        m_timeState.fixedStepsThisFrame++;
    }

    // Clamp accumulator to prevent spiral of death
    if (m_timeState.accumulator > m_timeState.fixedDeltaTime * 2) {
        m_timeState.accumulator = m_timeState.fixedDeltaTime * 2;
    }

    return m_timeState.fixedStepsThisFrame;
}

void TickScheduler::Pause() {
    m_timeState.isPaused = true;
}

void TickScheduler::Resume() {
    m_timeState.isPaused = false;
}

void TickScheduler::SetTimeScale(float scale) {
    m_timeState.timeScale = std::max(0.0f, scale);
}

void TickScheduler::SetFixedDeltaTime(double deltaTime) {
    m_timeState.fixedDeltaTime = std::max(0.001, deltaTime);
}

TickGroupStats TickScheduler::GetGroupStats(TickGroup group) const {
    return m_groups[static_cast<size_t>(group)].stats;
}

size_t TickScheduler::GetTotalTickCount() const {
    size_t count = 0;
    for (const auto& group : m_groups) {
        count += group.entries.size();
    }
    return count;
}

size_t TickScheduler::GetGroupTickCount(TickGroup group) const {
    return m_groups[static_cast<size_t>(group)].entries.size();
}

void TickScheduler::ResetStats() {
    for (auto& group : m_groups) {
        group.stats = TickGroupStats{};
    }
}

TickScheduler::TickEntry* TickScheduler::GetEntry(TickHandle handle) {
    if (!handle.IsValid() || handle.index >= m_handleToEntry.size()) {
        return nullptr;
    }

    TickEntry* entry = m_handleToEntry[handle.index];
    if (!entry || entry->generation != handle.generation || entry->pendingRemoval) {
        return nullptr;
    }

    return entry;
}

const TickScheduler::TickEntry* TickScheduler::GetEntry(TickHandle handle) const {
    if (!handle.IsValid() || handle.index >= m_handleToEntry.size()) {
        return nullptr;
    }

    const TickEntry* entry = m_handleToEntry[handle.index];
    if (!entry || entry->generation != handle.generation || entry->pendingRemoval) {
        return nullptr;
    }

    return entry;
}

void TickScheduler::SortGroup(TickGroup group) {
    auto& groupData = m_groups[static_cast<size_t>(group)];

    std::stable_sort(groupData.entries.begin(), groupData.entries.end(),
                     [](const TickEntry& a, const TickEntry& b) {
                         return a.config.priority > b.config.priority;
                     });

    // Update handle pointers after sort
    for (size_t i = 0; i < groupData.entries.size(); ++i) {
        // Find the handle for this entry and update pointer
        // This is O(n) but only happens when sorting
        for (size_t j = 0; j < m_handleToEntry.size(); ++j) {
            if (m_handleToEntry[j] &&
                m_handleToEntry[j]->generation == groupData.entries[i].generation) {
                m_handleToEntry[j] = &groupData.entries[i];
                break;
            }
        }
    }

    groupData.needsSort = false;
}

void TickScheduler::CleanupPendingRemovals() {
    for (auto& groupData : m_groups) {
        groupData.entries.erase(
            std::remove_if(groupData.entries.begin(), groupData.entries.end(),
                           [](const TickEntry& e) { return e.pendingRemoval; }),
            groupData.entries.end());
    }
}

void TickScheduler::TickEntry(TickEntry& entry, float deltaTime) {
    // Prefer function pointer (no vtable lookup)
    if (entry.tickFunc) {
        entry.tickFunc(entry.userData, deltaTime);
    }
    // Fall back to virtual call
    else if (entry.object) {
        entry.object->OnTick(deltaTime);
    }
}

// ============================================================================
// Global Scheduler
// ============================================================================

namespace {
    std::unique_ptr<TickScheduler> g_globalScheduler;
    std::once_flag g_schedulerInitFlag;
}

TickScheduler& GetGlobalTickScheduler() {
    std::call_once(g_schedulerInitFlag, []() {
        g_globalScheduler = std::make_unique<TickScheduler>();
    });
    return *g_globalScheduler;
}

} // namespace Lifecycle
} // namespace Vehement
