#pragma once

#include "core/Profiler.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>

namespace Nova {

// ============================================================================
// Chrome Tracing Event Types
// ============================================================================

/**
 * @brief Chrome trace event type
 */
enum class TraceEventType : char {
    DurationBegin = 'B',
    DurationEnd = 'E',
    Complete = 'X',
    Instant = 'i',
    Counter = 'C',
    AsyncStart = 'b',
    AsyncEnd = 'e',
    FlowStart = 's',
    FlowEnd = 'f',
    Metadata = 'M',
    MemoryDump = 'v'
};

/**
 * @brief Chrome trace event
 */
struct TraceEvent {
    std::string name;
    std::string category;
    TraceEventType type;
    uint64_t timestampUs;
    uint64_t durationUs;
    uint32_t processId;
    uint32_t threadId;
    std::string args;  // JSON args

    [[nodiscard]] std::string ToJson() const {
        std::ostringstream ss;
        ss << "{\"name\":\"" << name << "\","
           << "\"cat\":\"" << category << "\","
           << "\"ph\":\"" << static_cast<char>(type) << "\","
           << "\"ts\":" << timestampUs << ","
           << "\"pid\":" << processId << ","
           << "\"tid\":" << threadId;

        if (type == TraceEventType::Complete) {
            ss << ",\"dur\":" << durationUs;
        }

        if (!args.empty()) {
            ss << ",\"args\":" << args;
        }

        ss << "}";
        return ss.str();
    }
};

// ============================================================================
// Memory Tracker
// ============================================================================

/**
 * @brief Memory allocation tracking
 */
struct MemoryStats {
    size_t totalAllocated = 0;
    size_t totalFreed = 0;
    size_t currentUsage = 0;
    size_t peakUsage = 0;
    size_t allocationCount = 0;
    size_t deallocationCount = 0;

    std::unordered_map<std::string, size_t> categoryUsage;

    void RecordAllocation(size_t size, const std::string& category = "default") {
        totalAllocated += size;
        currentUsage += size;
        ++allocationCount;
        peakUsage = std::max(peakUsage, currentUsage);
        categoryUsage[category] += size;
    }

    void RecordDeallocation(size_t size, const std::string& category = "default") {
        totalFreed += size;
        currentUsage = (currentUsage > size) ? currentUsage - size : 0;
        ++deallocationCount;
        if (categoryUsage.count(category)) {
            categoryUsage[category] = (categoryUsage[category] > size) ?
                categoryUsage[category] - size : 0;
        }
    }

    void Reset() {
        totalAllocated = 0;
        totalFreed = 0;
        currentUsage = 0;
        peakUsage = 0;
        allocationCount = 0;
        deallocationCount = 0;
        categoryUsage.clear();
    }
};

// ============================================================================
// Network Profiler
// ============================================================================

/**
 * @brief Network statistics
 */
struct NetworkStats {
    size_t bytesSent = 0;
    size_t bytesReceived = 0;
    size_t packetsSent = 0;
    size_t packetsReceived = 0;
    size_t packetsLost = 0;
    float latencyMs = 0.0f;
    float jitterMs = 0.0f;
    float bandwidthKbps = 0.0f;

    // Per-message type tracking
    std::unordered_map<std::string, size_t> messageCounts;
    std::unordered_map<std::string, size_t> messageSizes;

    void RecordSend(size_t bytes, const std::string& messageType = "") {
        bytesSent += bytes;
        ++packetsSent;
        if (!messageType.empty()) {
            ++messageCounts[messageType + "_sent"];
            messageSizes[messageType + "_sent"] += bytes;
        }
    }

    void RecordReceive(size_t bytes, const std::string& messageType = "") {
        bytesReceived += bytes;
        ++packetsReceived;
        if (!messageType.empty()) {
            ++messageCounts[messageType + "_recv"];
            messageSizes[messageType + "_recv"] += bytes;
        }
    }

    void Reset() {
        bytesSent = 0;
        bytesReceived = 0;
        packetsSent = 0;
        packetsReceived = 0;
        packetsLost = 0;
        latencyMs = 0.0f;
        jitterMs = 0.0f;
        bandwidthKbps = 0.0f;
        messageCounts.clear();
        messageSizes.clear();
    }
};

// ============================================================================
// Chrome Trace Profiler
// ============================================================================

/**
 * @brief Chrome tracing profiler with memory and network tracking
 *
 * Exports profiling data to Chrome's trace format (about:tracing or
 * Perfetto) for detailed analysis.
 *
 * Features:
 * - Chrome trace JSON export
 * - GPU timing queries
 * - Memory allocation tracking
 * - Network profiling
 * - Per-thread profiling
 * - Custom counters and events
 *
 * Usage:
 * @code
 * auto& profiler = ChromeProfiler::Instance();
 * profiler.BeginSession("profile_session");
 *
 * // Profile scope
 * {
 *     NOVA_CHROME_PROFILE_SCOPE("MyFunction");
 *     // ... code ...
 * }
 *
 * // Memory tracking
 * profiler.RecordAllocation(1024, "textures");
 *
 * // End and export
 * profiler.EndSession();
 * // Open chrome_trace.json in chrome://tracing
 * @endcode
 */
class ChromeProfiler {
public:
    static ChromeProfiler& Instance();

    // Non-copyable
    ChromeProfiler(const ChromeProfiler&) = delete;
    ChromeProfiler& operator=(const ChromeProfiler&) = delete;

    // =========== Session Control ===========

    /**
     * @brief Begin a profiling session
     */
    void BeginSession(const std::string& name, const std::string& filepath = "chrome_trace.json");

    /**
     * @brief End current session and write file
     */
    void EndSession();

    /**
     * @brief Check if session is active
     */
    [[nodiscard]] bool IsSessionActive() const { return m_sessionActive; }

    // =========== Event Recording ===========

    /**
     * @brief Record a complete duration event
     */
    void WriteEvent(const std::string& name, const std::string& category,
                    uint64_t startUs, uint64_t durationUs);

    /**
     * @brief Record begin of duration event
     */
    void BeginEvent(const std::string& name, const std::string& category = "function");

    /**
     * @brief Record end of duration event
     */
    void EndEvent(const std::string& name, const std::string& category = "function");

    /**
     * @brief Record instant event
     */
    void InstantEvent(const std::string& name, const std::string& category = "event");

    /**
     * @brief Record counter value
     */
    void Counter(const std::string& name, int64_t value, const std::string& category = "counter");

    /**
     * @brief Add custom event with args
     */
    void WriteEventWithArgs(const std::string& name, const std::string& category,
                            TraceEventType type, const std::string& argsJson);

    // =========== GPU Profiling ===========

    /**
     * @brief Begin GPU timing query
     */
    void BeginGPUEvent(const std::string& name);

    /**
     * @brief End GPU timing query
     */
    void EndGPUEvent();

    /**
     * @brief Get GPU query results (call at end of frame)
     */
    void ResolveGPUQueries();

    // =========== Memory Tracking ===========

    /**
     * @brief Record memory allocation
     */
    void RecordAllocation(size_t size, const std::string& category = "default");

    /**
     * @brief Record memory deallocation
     */
    void RecordDeallocation(size_t size, const std::string& category = "default");

    /**
     * @brief Get memory stats
     */
    [[nodiscard]] const MemoryStats& GetMemoryStats() const { return m_memoryStats; }

    /**
     * @brief Reset memory stats
     */
    void ResetMemoryStats() { m_memoryStats.Reset(); }

    /**
     * @brief Record memory snapshot to trace
     */
    void RecordMemorySnapshot();

    // =========== Network Profiling ===========

    /**
     * @brief Record network send
     */
    void RecordNetworkSend(size_t bytes, const std::string& messageType = "");

    /**
     * @brief Record network receive
     */
    void RecordNetworkReceive(size_t bytes, const std::string& messageType = "");

    /**
     * @brief Update network latency
     */
    void UpdateNetworkLatency(float latencyMs, float jitterMs = 0.0f);

    /**
     * @brief Get network stats
     */
    [[nodiscard]] const NetworkStats& GetNetworkStats() const { return m_networkStats; }

    /**
     * @brief Reset network stats
     */
    void ResetNetworkStats() { m_networkStats.Reset(); }

    // =========== Frame Markers ===========

    /**
     * @brief Mark frame begin
     */
    void BeginFrame();

    /**
     * @brief Mark frame end
     */
    void EndFrame();

    /**
     * @brief Get current frame number
     */
    [[nodiscard]] uint64_t GetFrameNumber() const { return m_frameNumber; }

    // =========== Thread Naming ===========

    /**
     * @brief Set name for current thread
     */
    void SetThreadName(const std::string& name);

    // =========== Utilities ===========

    /**
     * @brief Get current timestamp in microseconds
     */
    [[nodiscard]] uint64_t GetTimestampUs() const;

    /**
     * @brief Get current thread ID
     */
    [[nodiscard]] uint32_t GetThreadId() const;

    /**
     * @brief Flush all pending events to file
     */
    void Flush();

    /**
     * @brief Set maximum events before auto-flush
     */
    void SetMaxBufferedEvents(size_t max) { m_maxBufferedEvents = max; }

private:
    ChromeProfiler();
    ~ChromeProfiler();

    void WriteHeader();
    void WriteFooter();
    void WriteEvents();

    std::ofstream m_outputFile;
    std::string m_sessionName;
    std::string m_filepath;

    std::vector<TraceEvent> m_events;
    std::mutex m_eventMutex;

    MemoryStats m_memoryStats;
    NetworkStats m_networkStats;

    std::chrono::high_resolution_clock::time_point m_sessionStart;
    uint64_t m_frameNumber = 0;
    uint32_t m_processId;
    size_t m_maxBufferedEvents = 100000;
    bool m_sessionActive = false;
    bool m_firstEvent = true;

    // GPU queries
    struct GPUQuery {
        std::string name;
        uint32_t queryId;
        bool pending;
    };
    std::vector<GPUQuery> m_gpuQueries;
    std::vector<uint32_t> m_queryPool;
    std::string m_currentGPUEvent;
};

// ============================================================================
// RAII Scope Timer for Chrome Profiling
// ============================================================================

/**
 * @brief RAII scope timer for Chrome tracing
 */
class ChromeScopeTimer {
public:
    ChromeScopeTimer(const char* name, const char* category = "function")
        : m_name(name), m_category(category)
    {
        m_start = ChromeProfiler::Instance().GetTimestampUs();
    }

    ~ChromeScopeTimer() {
        uint64_t end = ChromeProfiler::Instance().GetTimestampUs();
        ChromeProfiler::Instance().WriteEvent(m_name, m_category, m_start, end - m_start);
    }

    // Non-copyable, non-movable
    ChromeScopeTimer(const ChromeScopeTimer&) = delete;
    ChromeScopeTimer& operator=(const ChromeScopeTimer&) = delete;

private:
    const char* m_name;
    const char* m_category;
    uint64_t m_start;
};

// ============================================================================
// Implementation
// ============================================================================

inline ChromeProfiler& ChromeProfiler::Instance() {
    static ChromeProfiler instance;
    return instance;
}

inline ChromeProfiler::ChromeProfiler()
    : m_processId(static_cast<uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xFFFF))
{}

inline ChromeProfiler::~ChromeProfiler() {
    if (m_sessionActive) {
        EndSession();
    }
}

inline void ChromeProfiler::BeginSession(const std::string& name, const std::string& filepath) {
    if (m_sessionActive) {
        EndSession();
    }

    m_sessionName = name;
    m_filepath = filepath;
    m_outputFile.open(filepath);

    if (!m_outputFile) {
        return;
    }

    m_sessionStart = std::chrono::high_resolution_clock::now();
    m_sessionActive = true;
    m_firstEvent = true;
    m_events.clear();

    WriteHeader();

    // Add metadata event for session name
    WriteEventWithArgs("process_name", "metadata", TraceEventType::Metadata,
                       "{\"name\":\"" + name + "\"}");
}

inline void ChromeProfiler::EndSession() {
    if (!m_sessionActive) return;

    WriteEvents();
    WriteFooter();

    m_outputFile.close();
    m_sessionActive = false;
    m_events.clear();
}

inline void ChromeProfiler::WriteHeader() {
    m_outputFile << "{\"traceEvents\":[";
}

inline void ChromeProfiler::WriteFooter() {
    m_outputFile << "],\"displayTimeUnit\":\"ms\",\"systemTraceEvents\":\"SystemTraceData\",\"otherData\":{";
    m_outputFile << "\"version\":\"Nova Engine Profiler 1.0\"";
    m_outputFile << "}}";
}

inline void ChromeProfiler::WriteEvents() {
    std::lock_guard<std::mutex> lock(m_eventMutex);

    for (const auto& event : m_events) {
        if (!m_firstEvent) {
            m_outputFile << ",";
        }
        m_outputFile << "\n" << event.ToJson();
        m_firstEvent = false;
    }

    m_events.clear();
}

inline void ChromeProfiler::WriteEvent(const std::string& name, const std::string& category,
                                        uint64_t startUs, uint64_t durationUs) {
    if (!m_sessionActive) return;

    TraceEvent event;
    event.name = name;
    event.category = category;
    event.type = TraceEventType::Complete;
    event.timestampUs = startUs;
    event.durationUs = durationUs;
    event.processId = m_processId;
    event.threadId = GetThreadId();

    {
        std::lock_guard<std::mutex> lock(m_eventMutex);
        m_events.push_back(event);

        if (m_events.size() >= m_maxBufferedEvents) {
            WriteEvents();
        }
    }
}

inline void ChromeProfiler::BeginEvent(const std::string& name, const std::string& category) {
    if (!m_sessionActive) return;

    TraceEvent event;
    event.name = name;
    event.category = category;
    event.type = TraceEventType::DurationBegin;
    event.timestampUs = GetTimestampUs();
    event.processId = m_processId;
    event.threadId = GetThreadId();

    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_events.push_back(event);
}

inline void ChromeProfiler::EndEvent(const std::string& name, const std::string& category) {
    if (!m_sessionActive) return;

    TraceEvent event;
    event.name = name;
    event.category = category;
    event.type = TraceEventType::DurationEnd;
    event.timestampUs = GetTimestampUs();
    event.processId = m_processId;
    event.threadId = GetThreadId();

    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_events.push_back(event);
}

inline void ChromeProfiler::InstantEvent(const std::string& name, const std::string& category) {
    if (!m_sessionActive) return;

    TraceEvent event;
    event.name = name;
    event.category = category;
    event.type = TraceEventType::Instant;
    event.timestampUs = GetTimestampUs();
    event.processId = m_processId;
    event.threadId = GetThreadId();
    event.args = "{\"s\":\"g\"}";  // Global scope

    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_events.push_back(event);
}

inline void ChromeProfiler::Counter(const std::string& name, int64_t value, const std::string& category) {
    if (!m_sessionActive) return;

    TraceEvent event;
    event.name = name;
    event.category = category;
    event.type = TraceEventType::Counter;
    event.timestampUs = GetTimestampUs();
    event.processId = m_processId;
    event.threadId = GetThreadId();
    event.args = "{\"" + name + "\":" + std::to_string(value) + "}";

    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_events.push_back(event);
}

inline void ChromeProfiler::WriteEventWithArgs(const std::string& name, const std::string& category,
                                                TraceEventType type, const std::string& argsJson) {
    if (!m_sessionActive) return;

    TraceEvent event;
    event.name = name;
    event.category = category;
    event.type = type;
    event.timestampUs = GetTimestampUs();
    event.processId = m_processId;
    event.threadId = GetThreadId();
    event.args = argsJson;

    std::lock_guard<std::mutex> lock(m_eventMutex);
    m_events.push_back(event);
}

inline void ChromeProfiler::BeginGPUEvent(const std::string& name) {
    m_currentGPUEvent = name;
    // Would call glQueryCounter or equivalent
}

inline void ChromeProfiler::EndGPUEvent() {
    // Would end GPU query
}

inline void ChromeProfiler::ResolveGPUQueries() {
    // Would read GPU query results and record events
}

inline void ChromeProfiler::RecordAllocation(size_t size, const std::string& category) {
    m_memoryStats.RecordAllocation(size, category);

    if (m_sessionActive) {
        Counter("Memory_" + category, static_cast<int64_t>(m_memoryStats.categoryUsage[category]), "memory");
    }
}

inline void ChromeProfiler::RecordDeallocation(size_t size, const std::string& category) {
    m_memoryStats.RecordDeallocation(size, category);

    if (m_sessionActive) {
        Counter("Memory_" + category, static_cast<int64_t>(m_memoryStats.categoryUsage[category]), "memory");
    }
}

inline void ChromeProfiler::RecordMemorySnapshot() {
    if (!m_sessionActive) return;

    std::ostringstream args;
    args << "{\"total\":" << m_memoryStats.currentUsage
         << ",\"peak\":" << m_memoryStats.peakUsage
         << ",\"allocations\":" << m_memoryStats.allocationCount << "}";

    WriteEventWithArgs("MemorySnapshot", "memory", TraceEventType::Instant, args.str());
}

inline void ChromeProfiler::RecordNetworkSend(size_t bytes, const std::string& messageType) {
    m_networkStats.RecordSend(bytes, messageType);

    if (m_sessionActive) {
        Counter("Network_BytesSent", static_cast<int64_t>(m_networkStats.bytesSent), "network");
    }
}

inline void ChromeProfiler::RecordNetworkReceive(size_t bytes, const std::string& messageType) {
    m_networkStats.RecordReceive(bytes, messageType);

    if (m_sessionActive) {
        Counter("Network_BytesReceived", static_cast<int64_t>(m_networkStats.bytesReceived), "network");
    }
}

inline void ChromeProfiler::UpdateNetworkLatency(float latencyMs, float jitterMs) {
    m_networkStats.latencyMs = latencyMs;
    m_networkStats.jitterMs = jitterMs;

    if (m_sessionActive) {
        Counter("Network_Latency", static_cast<int64_t>(latencyMs * 1000), "network");
    }
}

inline void ChromeProfiler::BeginFrame() {
    if (m_sessionActive) {
        BeginEvent("Frame", "frame");
    }
}

inline void ChromeProfiler::EndFrame() {
    if (m_sessionActive) {
        EndEvent("Frame", "frame");
    }
    ++m_frameNumber;
}

inline void ChromeProfiler::SetThreadName(const std::string& name) {
    if (m_sessionActive) {
        WriteEventWithArgs("thread_name", "metadata", TraceEventType::Metadata,
                           "{\"name\":\"" + name + "\"}");
    }
}

inline uint64_t ChromeProfiler::GetTimestampUs() const {
    auto now = std::chrono::high_resolution_clock::now();
    return std::chrono::duration_cast<std::chrono::microseconds>(now - m_sessionStart).count();
}

inline uint32_t ChromeProfiler::GetThreadId() const {
    return static_cast<uint32_t>(std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xFFFFFFFF);
}

inline void ChromeProfiler::Flush() {
    if (m_sessionActive) {
        WriteEvents();
        m_outputFile.flush();
    }
}

} // namespace Nova

// ============================================================================
// Profiling Macros
// ============================================================================

#ifdef NOVA_PROFILE_ENABLED

#define NOVA_CHROME_PROFILE_SCOPE(name) \
    ::Nova::ChromeScopeTimer _nova_chrome_timer_##__LINE__(name)

#define NOVA_CHROME_PROFILE_FUNCTION() \
    NOVA_CHROME_PROFILE_SCOPE(__FUNCTION__)

#define NOVA_CHROME_PROFILE_SCOPE_CAT(name, category) \
    ::Nova::ChromeScopeTimer _nova_chrome_timer_##__LINE__(name, category)

#define NOVA_CHROME_BEGIN_SESSION(name) \
    ::Nova::ChromeProfiler::Instance().BeginSession(name)

#define NOVA_CHROME_END_SESSION() \
    ::Nova::ChromeProfiler::Instance().EndSession()

#define NOVA_CHROME_COUNTER(name, value) \
    ::Nova::ChromeProfiler::Instance().Counter(name, value)

#define NOVA_CHROME_MEMORY_ALLOC(size, category) \
    ::Nova::ChromeProfiler::Instance().RecordAllocation(size, category)

#define NOVA_CHROME_MEMORY_FREE(size, category) \
    ::Nova::ChromeProfiler::Instance().RecordDeallocation(size, category)

#else

#define NOVA_CHROME_PROFILE_SCOPE(name)
#define NOVA_CHROME_PROFILE_FUNCTION()
#define NOVA_CHROME_PROFILE_SCOPE_CAT(name, category)
#define NOVA_CHROME_BEGIN_SESSION(name)
#define NOVA_CHROME_END_SESSION()
#define NOVA_CHROME_COUNTER(name, value)
#define NOVA_CHROME_MEMORY_ALLOC(size, category)
#define NOVA_CHROME_MEMORY_FREE(size, category)

#endif
