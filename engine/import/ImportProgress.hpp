#pragma once

#include <string>
#include <vector>
#include <functional>
#include <atomic>
#include <mutex>
#include <chrono>
#include <memory>

namespace Nova {

// ============================================================================
// Forward Declarations
// ============================================================================

class ImportProgress;
class ImportProgressTracker;

// ============================================================================
// Import Error/Warning Types
// ============================================================================

/**
 * @brief Severity level for import messages
 */
enum class ImportMessageSeverity : uint8_t {
    Info,
    Warning,
    Error,
    Fatal
};

/**
 * @brief Import status
 */
enum class ImportStatus : uint8_t {
    Pending,        ///< Not started
    InProgress,     ///< Currently importing
    Completed,      ///< Finished successfully
    CompletedWithWarnings,
    Failed,         ///< Import failed
    Cancelled       ///< User cancelled
};

/**
 * @brief Single import message (error/warning/info)
 */
struct ImportMessage {
    ImportMessageSeverity severity = ImportMessageSeverity::Info;
    std::string message;
    std::string details;
    std::string assetPath;
    int line = 0;           ///< Line number if applicable
    int column = 0;         ///< Column number if applicable
    std::chrono::system_clock::time_point timestamp;

    ImportMessage() : timestamp(std::chrono::system_clock::now()) {}

    ImportMessage(ImportMessageSeverity sev, const std::string& msg,
                  const std::string& path = "")
        : severity(sev), message(msg), assetPath(path),
          timestamp(std::chrono::system_clock::now()) {}
};

/**
 * @brief Import stage information
 */
struct ImportStage {
    std::string name;
    std::string description;
    float weight = 1.0f;        ///< Relative weight for progress
    float progress = 0.0f;      ///< 0.0 - 1.0
    bool completed = false;
    std::chrono::milliseconds duration{0};
};

// ============================================================================
// Import Progress
// ============================================================================

/**
 * @brief Progress tracker for a single import operation
 */
class ImportProgress {
public:
    ImportProgress();
    explicit ImportProgress(const std::string& assetPath);
    ~ImportProgress() = default;

    // Non-copyable but movable
    ImportProgress(const ImportProgress&) = delete;
    ImportProgress& operator=(const ImportProgress&) = delete;
    ImportProgress(ImportProgress&&) noexcept = default;
    ImportProgress& operator=(ImportProgress&&) noexcept = default;

    // -------------------------------------------------------------------------
    // Progress Tracking
    // -------------------------------------------------------------------------

    /**
     * @brief Set overall progress (0.0 - 1.0)
     */
    void SetProgress(float progress);

    /**
     * @brief Get overall progress
     */
    [[nodiscard]] float GetProgress() const;

    /**
     * @brief Increment progress by amount
     */
    void IncrementProgress(float amount);

    /**
     * @brief Set status message
     */
    void SetStatusMessage(const std::string& message);

    /**
     * @brief Get current status message
     */
    [[nodiscard]] std::string GetStatusMessage() const;

    // -------------------------------------------------------------------------
    // Multi-Stage Progress
    // -------------------------------------------------------------------------

    /**
     * @brief Add a stage to the import process
     */
    void AddStage(const std::string& name, const std::string& description, float weight = 1.0f);

    /**
     * @brief Begin a stage
     */
    void BeginStage(const std::string& name);

    /**
     * @brief End current stage
     */
    void EndStage();

    /**
     * @brief Update current stage progress
     */
    void UpdateStageProgress(float progress);

    /**
     * @brief Get current stage name
     */
    [[nodiscard]] std::string GetCurrentStageName() const;

    /**
     * @brief Get all stages
     */
    [[nodiscard]] const std::vector<ImportStage>& GetStages() const { return m_stages; }

    // -------------------------------------------------------------------------
    // Messages
    // -------------------------------------------------------------------------

    /**
     * @brief Add an info message
     */
    void Info(const std::string& message, const std::string& details = "");

    /**
     * @brief Add a warning
     */
    void Warning(const std::string& message, const std::string& details = "");

    /**
     * @brief Add an error
     */
    void Error(const std::string& message, const std::string& details = "");

    /**
     * @brief Add a fatal error
     */
    void Fatal(const std::string& message, const std::string& details = "");

    /**
     * @brief Get all messages
     */
    [[nodiscard]] std::vector<ImportMessage> GetMessages() const;

    /**
     * @brief Get messages by severity
     */
    [[nodiscard]] std::vector<ImportMessage> GetMessages(ImportMessageSeverity severity) const;

    /**
     * @brief Check if has errors
     */
    [[nodiscard]] bool HasErrors() const;

    /**
     * @brief Check if has warnings
     */
    [[nodiscard]] bool HasWarnings() const;

    /**
     * @brief Get error count
     */
    [[nodiscard]] size_t GetErrorCount() const;

    /**
     * @brief Get warning count
     */
    [[nodiscard]] size_t GetWarningCount() const;

    // -------------------------------------------------------------------------
    // Status
    // -------------------------------------------------------------------------

    /**
     * @brief Set import status
     */
    void SetStatus(ImportStatus status);

    /**
     * @brief Get import status
     */
    [[nodiscard]] ImportStatus GetStatus() const;

    /**
     * @brief Check if completed (success or failure)
     */
    [[nodiscard]] bool IsCompleted() const;

    /**
     * @brief Check if successful
     */
    [[nodiscard]] bool IsSuccessful() const;

    /**
     * @brief Check if failed
     */
    [[nodiscard]] bool IsFailed() const;

    // -------------------------------------------------------------------------
    // Cancellation
    // -------------------------------------------------------------------------

    /**
     * @brief Request cancellation
     */
    void RequestCancel();

    /**
     * @brief Check if cancellation requested
     */
    [[nodiscard]] bool IsCancellationRequested() const;

    /**
     * @brief Mark as cancelled
     */
    void MarkCancelled();

    // -------------------------------------------------------------------------
    // Timing
    // -------------------------------------------------------------------------

    /**
     * @brief Start timing
     */
    void StartTiming();

    /**
     * @brief Stop timing
     */
    void StopTiming();

    /**
     * @brief Get elapsed time in milliseconds
     */
    [[nodiscard]] int64_t GetElapsedMs() const;

    /**
     * @brief Get estimated remaining time
     */
    [[nodiscard]] int64_t GetEstimatedRemainingMs() const;

    // -------------------------------------------------------------------------
    // Asset Info
    // -------------------------------------------------------------------------

    /**
     * @brief Get asset path
     */
    [[nodiscard]] const std::string& GetAssetPath() const { return m_assetPath; }

    /**
     * @brief Set asset path
     */
    void SetAssetPath(const std::string& path) { m_assetPath = path; }

    /**
     * @brief Set output path
     */
    void SetOutputPath(const std::string& path) { m_outputPath = path; }

    /**
     * @brief Get output path
     */
    [[nodiscard]] const std::string& GetOutputPath() const { return m_outputPath; }

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    using ProgressCallback = std::function<void(float progress, const std::string& message)>;
    using CompletionCallback = std::function<void(ImportStatus status)>;
    using MessageCallback = std::function<void(const ImportMessage& message)>;

    /**
     * @brief Set progress callback
     */
    void SetProgressCallback(ProgressCallback callback);

    /**
     * @brief Set completion callback
     */
    void SetCompletionCallback(CompletionCallback callback);

    /**
     * @brief Set message callback
     */
    void SetMessageCallback(MessageCallback callback);

    // -------------------------------------------------------------------------
    // Log Generation
    // -------------------------------------------------------------------------

    /**
     * @brief Generate import log
     */
    [[nodiscard]] std::string GenerateLog() const;

    /**
     * @brief Save log to file
     */
    bool SaveLog(const std::string& path) const;

private:
    void AddMessage(ImportMessageSeverity severity, const std::string& message,
                    const std::string& details);
    void NotifyProgress();
    float CalculateTotalProgress() const;

    std::string m_assetPath;
    std::string m_outputPath;
    std::string m_statusMessage;

    std::atomic<float> m_progress{0.0f};
    std::atomic<ImportStatus> m_status{ImportStatus::Pending};
    std::atomic<bool> m_cancelRequested{false};

    std::vector<ImportStage> m_stages;
    int m_currentStageIndex = -1;

    mutable std::mutex m_messageMutex;
    std::vector<ImportMessage> m_messages;
    std::atomic<size_t> m_errorCount{0};
    std::atomic<size_t> m_warningCount{0};

    std::chrono::steady_clock::time_point m_startTime;
    std::chrono::steady_clock::time_point m_endTime;
    bool m_timingStarted = false;
    bool m_timingStopped = false;

    ProgressCallback m_progressCallback;
    CompletionCallback m_completionCallback;
    MessageCallback m_messageCallback;
};

// ============================================================================
// Batch Import Progress Tracker
// ============================================================================

/**
 * @brief Tracks progress for multiple imports
 */
class ImportProgressTracker {
public:
    ImportProgressTracker() = default;
    ~ImportProgressTracker() = default;

    /**
     * @brief Add an import to track
     */
    ImportProgress* AddImport(const std::string& assetPath);

    /**
     * @brief Get import progress by path
     */
    ImportProgress* GetImport(const std::string& assetPath);

    /**
     * @brief Get all imports
     */
    [[nodiscard]] std::vector<ImportProgress*> GetAllImports();

    /**
     * @brief Get pending imports
     */
    [[nodiscard]] std::vector<ImportProgress*> GetPendingImports();

    /**
     * @brief Get completed imports
     */
    [[nodiscard]] std::vector<ImportProgress*> GetCompletedImports();

    /**
     * @brief Get failed imports
     */
    [[nodiscard]] std::vector<ImportProgress*> GetFailedImports();

    /**
     * @brief Get overall progress (0.0 - 1.0)
     */
    [[nodiscard]] float GetOverallProgress() const;

    /**
     * @brief Get total import count
     */
    [[nodiscard]] size_t GetTotalCount() const;

    /**
     * @brief Get completed count
     */
    [[nodiscard]] size_t GetCompletedCount() const;

    /**
     * @brief Get failed count
     */
    [[nodiscard]] size_t GetFailedCount() const;

    /**
     * @brief Check if all completed
     */
    [[nodiscard]] bool IsAllCompleted() const;

    /**
     * @brief Request cancellation for all
     */
    void CancelAll();

    /**
     * @brief Clear all imports
     */
    void Clear();

    /**
     * @brief Remove completed imports
     */
    void ClearCompleted();

    /**
     * @brief Set overall progress callback
     */
    using OverallProgressCallback = std::function<void(float progress, size_t completed, size_t total)>;
    void SetOverallProgressCallback(OverallProgressCallback callback);

    /**
     * @brief Set completion callback
     */
    using AllCompletedCallback = std::function<void(size_t succeeded, size_t failed)>;
    void SetAllCompletedCallback(AllCompletedCallback callback);

    /**
     * @brief Generate combined log
     */
    [[nodiscard]] std::string GenerateCombinedLog() const;

private:
    void CheckCompletion();

    mutable std::mutex m_mutex;
    std::vector<std::unique_ptr<ImportProgress>> m_imports;
    std::unordered_map<std::string, size_t> m_pathToIndex;

    OverallProgressCallback m_overallProgressCallback;
    AllCompletedCallback m_allCompletedCallback;
};

// ============================================================================
// Scoped Progress Helper
// ============================================================================

/**
 * @brief RAII helper for stage progress
 */
class ScopedStageProgress {
public:
    ScopedStageProgress(ImportProgress& progress, const std::string& stageName)
        : m_progress(progress) {
        m_progress.BeginStage(stageName);
    }

    ~ScopedStageProgress() {
        m_progress.EndStage();
    }

    void SetProgress(float progress) {
        m_progress.UpdateStageProgress(progress);
    }

private:
    ImportProgress& m_progress;
};

// ============================================================================
// Progress Reporter Interface
// ============================================================================

/**
 * @brief Interface for progress reporting
 */
class IProgressReporter {
public:
    virtual ~IProgressReporter() = default;

    virtual void ReportProgress(float progress, const std::string& message) = 0;
    virtual void ReportStageBegin(const std::string& stageName) = 0;
    virtual void ReportStageEnd(const std::string& stageName, bool success) = 0;
    virtual void ReportMessage(const ImportMessage& message) = 0;
    virtual bool IsCancellationRequested() const = 0;
};

/**
 * @brief Console progress reporter
 */
class ConsoleProgressReporter : public IProgressReporter {
public:
    ConsoleProgressReporter(bool verbose = false) : m_verbose(verbose) {}

    void ReportProgress(float progress, const std::string& message) override;
    void ReportStageBegin(const std::string& stageName) override;
    void ReportStageEnd(const std::string& stageName, bool success) override;
    void ReportMessage(const ImportMessage& message) override;
    bool IsCancellationRequested() const override { return m_cancelRequested; }

    void RequestCancel() { m_cancelRequested = true; }

private:
    bool m_verbose;
    std::atomic<bool> m_cancelRequested{false};
};

/**
 * @brief Progress reporter that wraps ImportProgress
 */
class ImportProgressReporter : public IProgressReporter {
public:
    explicit ImportProgressReporter(ImportProgress& progress) : m_progress(progress) {}

    void ReportProgress(float progress, const std::string& message) override;
    void ReportStageBegin(const std::string& stageName) override;
    void ReportStageEnd(const std::string& stageName, bool success) override;
    void ReportMessage(const ImportMessage& message) override;
    bool IsCancellationRequested() const override;

private:
    ImportProgress& m_progress;
};

} // namespace Nova
