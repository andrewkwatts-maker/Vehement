#include "ImportProgress.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <algorithm>

namespace Nova {

// ============================================================================
// ImportProgress Implementation
// ============================================================================

ImportProgress::ImportProgress() {
    m_startTime = std::chrono::steady_clock::now();
}

ImportProgress::ImportProgress(const std::string& assetPath)
    : m_assetPath(assetPath) {
    m_startTime = std::chrono::steady_clock::now();
}

void ImportProgress::SetProgress(float progress) {
    m_progress.store(std::clamp(progress, 0.0f, 1.0f));
    NotifyProgress();
}

float ImportProgress::GetProgress() const {
    if (!m_stages.empty()) {
        return CalculateTotalProgress();
    }
    return m_progress.load();
}

void ImportProgress::IncrementProgress(float amount) {
    float current = m_progress.load();
    SetProgress(current + amount);
}

void ImportProgress::SetStatusMessage(const std::string& message) {
    std::lock_guard<std::mutex> lock(m_messageMutex);
    m_statusMessage = message;
}

std::string ImportProgress::GetStatusMessage() const {
    std::lock_guard<std::mutex> lock(m_messageMutex);
    return m_statusMessage;
}

void ImportProgress::AddStage(const std::string& name, const std::string& description, float weight) {
    ImportStage stage;
    stage.name = name;
    stage.description = description;
    stage.weight = weight;
    m_stages.push_back(stage);
}

void ImportProgress::BeginStage(const std::string& name) {
    for (size_t i = 0; i < m_stages.size(); ++i) {
        if (m_stages[i].name == name) {
            m_currentStageIndex = static_cast<int>(i);
            m_stages[i].progress = 0.0f;
            SetStatusMessage(m_stages[i].description);
            return;
        }
    }
    // Stage not found, add it
    AddStage(name, name, 1.0f);
    m_currentStageIndex = static_cast<int>(m_stages.size() - 1);
}

void ImportProgress::EndStage() {
    if (m_currentStageIndex >= 0 && m_currentStageIndex < static_cast<int>(m_stages.size())) {
        m_stages[m_currentStageIndex].progress = 1.0f;
        m_stages[m_currentStageIndex].completed = true;
    }
    NotifyProgress();
}

void ImportProgress::UpdateStageProgress(float progress) {
    if (m_currentStageIndex >= 0 && m_currentStageIndex < static_cast<int>(m_stages.size())) {
        m_stages[m_currentStageIndex].progress = std::clamp(progress, 0.0f, 1.0f);
    }
    NotifyProgress();
}

std::string ImportProgress::GetCurrentStageName() const {
    if (m_currentStageIndex >= 0 && m_currentStageIndex < static_cast<int>(m_stages.size())) {
        return m_stages[m_currentStageIndex].name;
    }
    return "";
}

void ImportProgress::AddMessage(ImportMessageSeverity severity, const std::string& message,
                                 const std::string& details) {
    ImportMessage msg(severity, message, m_assetPath);
    msg.details = details;

    {
        std::lock_guard<std::mutex> lock(m_messageMutex);
        m_messages.push_back(msg);
    }

    if (severity == ImportMessageSeverity::Error || severity == ImportMessageSeverity::Fatal) {
        m_errorCount++;
    } else if (severity == ImportMessageSeverity::Warning) {
        m_warningCount++;
    }

    if (m_messageCallback) {
        m_messageCallback(msg);
    }
}

void ImportProgress::Info(const std::string& message, const std::string& details) {
    AddMessage(ImportMessageSeverity::Info, message, details);
}

void ImportProgress::Warning(const std::string& message, const std::string& details) {
    AddMessage(ImportMessageSeverity::Warning, message, details);
}

void ImportProgress::Error(const std::string& message, const std::string& details) {
    AddMessage(ImportMessageSeverity::Error, message, details);
}

void ImportProgress::Fatal(const std::string& message, const std::string& details) {
    AddMessage(ImportMessageSeverity::Fatal, message, details);
    SetStatus(ImportStatus::Failed);
}

std::vector<ImportMessage> ImportProgress::GetMessages() const {
    std::lock_guard<std::mutex> lock(m_messageMutex);
    return m_messages;
}

std::vector<ImportMessage> ImportProgress::GetMessages(ImportMessageSeverity severity) const {
    std::lock_guard<std::mutex> lock(m_messageMutex);
    std::vector<ImportMessage> filtered;
    for (const auto& msg : m_messages) {
        if (msg.severity == severity) {
            filtered.push_back(msg);
        }
    }
    return filtered;
}

bool ImportProgress::HasErrors() const {
    return m_errorCount.load() > 0;
}

bool ImportProgress::HasWarnings() const {
    return m_warningCount.load() > 0;
}

size_t ImportProgress::GetErrorCount() const {
    return m_errorCount.load();
}

size_t ImportProgress::GetWarningCount() const {
    return m_warningCount.load();
}

void ImportProgress::SetStatus(ImportStatus status) {
    ImportStatus oldStatus = m_status.exchange(status);

    if (status == ImportStatus::Completed || status == ImportStatus::Failed ||
        status == ImportStatus::Cancelled || status == ImportStatus::CompletedWithWarnings) {
        StopTiming();

        if (m_completionCallback && oldStatus != status) {
            m_completionCallback(status);
        }
    }
}

ImportStatus ImportProgress::GetStatus() const {
    return m_status.load();
}

bool ImportProgress::IsCompleted() const {
    ImportStatus status = m_status.load();
    return status == ImportStatus::Completed ||
           status == ImportStatus::CompletedWithWarnings ||
           status == ImportStatus::Failed ||
           status == ImportStatus::Cancelled;
}

bool ImportProgress::IsSuccessful() const {
    ImportStatus status = m_status.load();
    return status == ImportStatus::Completed ||
           status == ImportStatus::CompletedWithWarnings;
}

bool ImportProgress::IsFailed() const {
    return m_status.load() == ImportStatus::Failed;
}

void ImportProgress::RequestCancel() {
    m_cancelRequested.store(true);
}

bool ImportProgress::IsCancellationRequested() const {
    return m_cancelRequested.load();
}

void ImportProgress::MarkCancelled() {
    SetStatus(ImportStatus::Cancelled);
}

void ImportProgress::StartTiming() {
    m_startTime = std::chrono::steady_clock::now();
    m_timingStarted = true;
    m_timingStopped = false;
}

void ImportProgress::StopTiming() {
    if (m_timingStarted && !m_timingStopped) {
        m_endTime = std::chrono::steady_clock::now();
        m_timingStopped = true;
    }
}

int64_t ImportProgress::GetElapsedMs() const {
    auto end = m_timingStopped ? m_endTime : std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - m_startTime).count();
}

int64_t ImportProgress::GetEstimatedRemainingMs() const {
    float progress = GetProgress();
    if (progress <= 0.0f || progress >= 1.0f) {
        return 0;
    }

    int64_t elapsed = GetElapsedMs();
    float remaining = 1.0f - progress;
    return static_cast<int64_t>((elapsed / progress) * remaining);
}

void ImportProgress::SetProgressCallback(ProgressCallback callback) {
    m_progressCallback = callback;
}

void ImportProgress::SetCompletionCallback(CompletionCallback callback) {
    m_completionCallback = callback;
}

void ImportProgress::SetMessageCallback(MessageCallback callback) {
    m_messageCallback = callback;
}

void ImportProgress::NotifyProgress() {
    if (m_progressCallback) {
        m_progressCallback(GetProgress(), GetStatusMessage());
    }
}

float ImportProgress::CalculateTotalProgress() const {
    if (m_stages.empty()) {
        return m_progress.load();
    }

    float totalWeight = 0.0f;
    float weightedProgress = 0.0f;

    for (const auto& stage : m_stages) {
        totalWeight += stage.weight;
        weightedProgress += stage.progress * stage.weight;
    }

    return totalWeight > 0.0f ? (weightedProgress / totalWeight) : 0.0f;
}

std::string ImportProgress::GenerateLog() const {
    std::ostringstream ss;

    ss << "=== Import Log ===" << std::endl;
    ss << "Asset: " << m_assetPath << std::endl;
    ss << "Output: " << m_outputPath << std::endl;
    ss << "Status: ";

    switch (m_status.load()) {
        case ImportStatus::Pending: ss << "Pending"; break;
        case ImportStatus::InProgress: ss << "In Progress"; break;
        case ImportStatus::Completed: ss << "Completed"; break;
        case ImportStatus::CompletedWithWarnings: ss << "Completed with Warnings"; break;
        case ImportStatus::Failed: ss << "Failed"; break;
        case ImportStatus::Cancelled: ss << "Cancelled"; break;
    }
    ss << std::endl;

    ss << "Duration: " << GetElapsedMs() << " ms" << std::endl;
    ss << "Warnings: " << m_warningCount.load() << std::endl;
    ss << "Errors: " << m_errorCount.load() << std::endl;
    ss << std::endl;

    // Stages
    if (!m_stages.empty()) {
        ss << "--- Stages ---" << std::endl;
        for (const auto& stage : m_stages) {
            ss << "  [" << (stage.completed ? "X" : " ") << "] "
               << stage.name << " - " << static_cast<int>(stage.progress * 100) << "%"
               << std::endl;
        }
        ss << std::endl;
    }

    // Messages
    auto messages = GetMessages();
    if (!messages.empty()) {
        ss << "--- Messages ---" << std::endl;
        for (const auto& msg : messages) {
            switch (msg.severity) {
                case ImportMessageSeverity::Info: ss << "[INFO] "; break;
                case ImportMessageSeverity::Warning: ss << "[WARN] "; break;
                case ImportMessageSeverity::Error: ss << "[ERROR] "; break;
                case ImportMessageSeverity::Fatal: ss << "[FATAL] "; break;
            }
            ss << msg.message;
            if (!msg.details.empty()) {
                ss << " - " << msg.details;
            }
            ss << std::endl;
        }
    }

    return ss.str();
}

bool ImportProgress::SaveLog(const std::string& path) const {
    std::ofstream file(path);
    if (!file.is_open()) {
        return false;
    }
    file << GenerateLog();
    return true;
}

// ============================================================================
// ImportProgressTracker Implementation
// ============================================================================

ImportProgress* ImportProgressTracker::AddImport(const std::string& assetPath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_pathToIndex.find(assetPath);
    if (it != m_pathToIndex.end()) {
        return m_imports[it->second].get();
    }

    auto progress = std::make_unique<ImportProgress>(assetPath);

    // Set up callback to track completion
    progress->SetCompletionCallback([this](ImportStatus status) {
        CheckCompletion();
        if (m_overallProgressCallback) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_overallProgressCallback(GetOverallProgress(), GetCompletedCount(), GetTotalCount());
        }
    });

    ImportProgress* ptr = progress.get();
    size_t index = m_imports.size();
    m_imports.push_back(std::move(progress));
    m_pathToIndex[assetPath] = index;

    return ptr;
}

ImportProgress* ImportProgressTracker::GetImport(const std::string& assetPath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_pathToIndex.find(assetPath);
    if (it != m_pathToIndex.end()) {
        return m_imports[it->second].get();
    }
    return nullptr;
}

std::vector<ImportProgress*> ImportProgressTracker::GetAllImports() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ImportProgress*> result;
    result.reserve(m_imports.size());
    for (auto& import : m_imports) {
        result.push_back(import.get());
    }
    return result;
}

std::vector<ImportProgress*> ImportProgressTracker::GetPendingImports() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ImportProgress*> result;
    for (auto& import : m_imports) {
        if (import->GetStatus() == ImportStatus::Pending ||
            import->GetStatus() == ImportStatus::InProgress) {
            result.push_back(import.get());
        }
    }
    return result;
}

std::vector<ImportProgress*> ImportProgressTracker::GetCompletedImports() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ImportProgress*> result;
    for (auto& import : m_imports) {
        if (import->IsCompleted() && !import->IsFailed()) {
            result.push_back(import.get());
        }
    }
    return result;
}

std::vector<ImportProgress*> ImportProgressTracker::GetFailedImports() {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ImportProgress*> result;
    for (auto& import : m_imports) {
        if (import->IsFailed()) {
            result.push_back(import.get());
        }
    }
    return result;
}

float ImportProgressTracker::GetOverallProgress() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_imports.empty()) return 0.0f;

    float total = 0.0f;
    for (const auto& import : m_imports) {
        total += import->GetProgress();
    }
    return total / static_cast<float>(m_imports.size());
}

size_t ImportProgressTracker::GetTotalCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_imports.size();
}

size_t ImportProgressTracker::GetCompletedCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& import : m_imports) {
        if (import->IsCompleted()) count++;
    }
    return count;
}

size_t ImportProgressTracker::GetFailedCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t count = 0;
    for (const auto& import : m_imports) {
        if (import->IsFailed()) count++;
    }
    return count;
}

bool ImportProgressTracker::IsAllCompleted() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& import : m_imports) {
        if (!import->IsCompleted()) return false;
    }
    return true;
}

void ImportProgressTracker::CancelAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto& import : m_imports) {
        import->RequestCancel();
    }
}

void ImportProgressTracker::Clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_imports.clear();
    m_pathToIndex.clear();
}

void ImportProgressTracker::ClearCompleted() {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::unique_ptr<ImportProgress>> pending;
    m_pathToIndex.clear();

    for (auto& import : m_imports) {
        if (!import->IsCompleted()) {
            m_pathToIndex[import->GetAssetPath()] = pending.size();
            pending.push_back(std::move(import));
        }
    }

    m_imports = std::move(pending);
}

void ImportProgressTracker::SetOverallProgressCallback(OverallProgressCallback callback) {
    m_overallProgressCallback = callback;
}

void ImportProgressTracker::SetAllCompletedCallback(AllCompletedCallback callback) {
    m_allCompletedCallback = callback;
}

void ImportProgressTracker::CheckCompletion() {
    if (IsAllCompleted() && m_allCompletedCallback) {
        size_t succeeded = 0;
        size_t failed = 0;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            for (const auto& import : m_imports) {
                if (import->IsSuccessful()) succeeded++;
                else if (import->IsFailed()) failed++;
            }
        }
        m_allCompletedCallback(succeeded, failed);
    }
}

std::string ImportProgressTracker::GenerateCombinedLog() const {
    std::ostringstream ss;

    ss << "=== Batch Import Log ===" << std::endl;
    ss << "Total: " << GetTotalCount() << std::endl;
    ss << "Completed: " << GetCompletedCount() << std::endl;
    ss << "Failed: " << GetFailedCount() << std::endl;
    ss << std::endl;

    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& import : m_imports) {
        ss << "---" << std::endl;
        ss << import->GenerateLog();
        ss << std::endl;
    }

    return ss.str();
}

// ============================================================================
// ConsoleProgressReporter Implementation
// ============================================================================

void ConsoleProgressReporter::ReportProgress(float progress, const std::string& message) {
    if (m_verbose) {
        std::cout << "[" << static_cast<int>(progress * 100) << "%] " << message << std::endl;
    } else {
        // Simple progress bar
        int barWidth = 40;
        int pos = static_cast<int>(barWidth * progress);

        std::cout << "\r[";
        for (int i = 0; i < barWidth; ++i) {
            if (i < pos) std::cout << "=";
            else if (i == pos) std::cout << ">";
            else std::cout << " ";
        }
        std::cout << "] " << static_cast<int>(progress * 100) << "% " << message;
        std::cout.flush();

        if (progress >= 1.0f) {
            std::cout << std::endl;
        }
    }
}

void ConsoleProgressReporter::ReportStageBegin(const std::string& stageName) {
    if (m_verbose) {
        std::cout << "==> Starting: " << stageName << std::endl;
    }
}

void ConsoleProgressReporter::ReportStageEnd(const std::string& stageName, bool success) {
    if (m_verbose) {
        std::cout << "<== " << (success ? "Completed" : "Failed") << ": " << stageName << std::endl;
    }
}

void ConsoleProgressReporter::ReportMessage(const ImportMessage& message) {
    switch (message.severity) {
        case ImportMessageSeverity::Info:
            if (m_verbose) {
                std::cout << "[INFO] " << message.message << std::endl;
            }
            break;
        case ImportMessageSeverity::Warning:
            std::cout << "[WARNING] " << message.message << std::endl;
            break;
        case ImportMessageSeverity::Error:
        case ImportMessageSeverity::Fatal:
            std::cerr << "[ERROR] " << message.message << std::endl;
            break;
    }
}

// ============================================================================
// ImportProgressReporter Implementation
// ============================================================================

void ImportProgressReporter::ReportProgress(float progress, const std::string& message) {
    m_progress.SetProgress(progress);
    m_progress.SetStatusMessage(message);
}

void ImportProgressReporter::ReportStageBegin(const std::string& stageName) {
    m_progress.BeginStage(stageName);
}

void ImportProgressReporter::ReportStageEnd(const std::string& stageName, bool success) {
    m_progress.EndStage();
    if (!success) {
        m_progress.Error("Stage failed: " + stageName);
    }
}

void ImportProgressReporter::ReportMessage(const ImportMessage& message) {
    switch (message.severity) {
        case ImportMessageSeverity::Info:
            m_progress.Info(message.message, message.details);
            break;
        case ImportMessageSeverity::Warning:
            m_progress.Warning(message.message, message.details);
            break;
        case ImportMessageSeverity::Error:
            m_progress.Error(message.message, message.details);
            break;
        case ImportMessageSeverity::Fatal:
            m_progress.Fatal(message.message, message.details);
            break;
    }
}

bool ImportProgressReporter::IsCancellationRequested() const {
    return m_progress.IsCancellationRequested();
}

} // namespace Nova
