#include "WorkshopIntegration.hpp"
#include <fstream>
#include <filesystem>
#include <algorithm>

namespace Vehement {

WorkshopIntegration& WorkshopIntegration::Instance() {
    static WorkshopIntegration instance;
    return instance;
}

bool WorkshopIntegration::Initialize() {
    if (m_isInitialized) return true;

    // Initialize platform-specific workshop API (Steam, etc.)
    // This would connect to Steam Workshop or similar service

    m_isInitialized = true;
    return true;
}

void WorkshopIntegration::Shutdown() {
    m_isInitialized = false;
    m_itemCache.clear();
    m_subscribedItems.clear();
    m_favoriteItems.clear();
}

bool WorkshopIntegration::CreateItem(WorkshopItemType type,
                                      std::function<void(uint64_t itemId, bool success)> callback) {
    if (!m_isInitialized) {
        if (callback) callback(0, false);
        return false;
    }

    // Platform-specific item creation
    // For now, simulate with a local ID
    uint64_t newItemId = std::hash<std::string>{}(std::to_string(std::time(nullptr)));

    if (OnItemCreated) {
        OnItemCreated(newItemId, true);
    }

    if (callback) {
        callback(newItemId, true);
    }

    return true;
}

bool WorkshopIntegration::UpdateItem(uint64_t itemId, const WorkshopItemInfo& info,
                                      std::function<void(bool success, const std::string& error)> callback) {
    if (!m_isInitialized) {
        if (callback) callback(false, "Workshop not initialized");
        return false;
    }

    // Cache the item info
    m_itemCache[itemId] = info;

    if (callback) {
        callback(true, "");
    }

    return true;
}

bool WorkshopIntegration::DeleteItem(uint64_t itemId,
                                      std::function<void(bool success)> callback) {
    if (!m_isInitialized) {
        if (callback) callback(false);
        return false;
    }

    m_itemCache.erase(itemId);

    if (callback) {
        callback(true);
    }

    return true;
}

bool WorkshopIntegration::SetItemContent(uint64_t itemId, const std::string& contentPath) {
    if (!m_isInitialized) return false;

    auto it = m_itemCache.find(itemId);
    if (it != m_itemCache.end()) {
        it->second.contentPath = contentPath;
        return true;
    }

    return false;
}

bool WorkshopIntegration::SetItemThumbnail(uint64_t itemId, const std::string& thumbnailPath) {
    if (!m_isInitialized) return false;

    auto it = m_itemCache.find(itemId);
    if (it != m_itemCache.end()) {
        it->second.thumbnailPath = thumbnailPath;
        return true;
    }

    return false;
}

bool WorkshopIntegration::SubmitUpdate(uint64_t itemId, const std::string& changeNotes,
                                        std::function<void(bool success, const std::string& error)> callback) {
    if (!m_isInitialized) {
        if (callback) callback(false, "Workshop not initialized");
        return false;
    }

    auto it = m_itemCache.find(itemId);
    if (it == m_itemCache.end()) {
        if (callback) callback(false, "Item not found");
        return false;
    }

    // Simulate upload progress
    m_uploadProgress.bytesUploaded = 0;
    m_uploadProgress.totalBytes = 1000000;  // Simulated
    m_uploadProgress.percentage = 0.0f;
    m_uploadProgress.status = "Preparing upload...";

    // In real implementation, this would upload to the workshop
    it->second.changeNotes = changeNotes;
    it->second.status = WorkshopItemStatus::Published;
    it->second.updatedTime = std::time(nullptr);

    m_uploadProgress.percentage = 100.0f;
    m_uploadProgress.status = "Complete";

    if (OnItemUpdated) {
        OnItemUpdated(itemId, true);
    }

    if (callback) {
        callback(true, "");
    }

    return true;
}

bool WorkshopIntegration::Subscribe(uint64_t itemId, std::function<void(bool success)> callback) {
    if (!m_isInitialized) {
        if (callback) callback(false);
        return false;
    }

    if (!IsSubscribed(itemId)) {
        m_subscribedItems.push_back(itemId);

        if (OnItemSubscribed) {
            OnItemSubscribed(itemId);
        }
    }

    if (callback) {
        callback(true);
    }

    return true;
}

bool WorkshopIntegration::Unsubscribe(uint64_t itemId, std::function<void(bool success)> callback) {
    if (!m_isInitialized) {
        if (callback) callback(false);
        return false;
    }

    m_subscribedItems.erase(
        std::remove(m_subscribedItems.begin(), m_subscribedItems.end(), itemId),
        m_subscribedItems.end());

    if (OnItemUnsubscribed) {
        OnItemUnsubscribed(itemId);
    }

    if (callback) {
        callback(true);
    }

    return true;
}

std::vector<uint64_t> WorkshopIntegration::GetSubscribedItems() {
    return m_subscribedItems;
}

bool WorkshopIntegration::IsSubscribed(uint64_t itemId) const {
    return std::find(m_subscribedItems.begin(), m_subscribedItems.end(), itemId)
           != m_subscribedItems.end();
}

bool WorkshopIntegration::DownloadItem(uint64_t itemId,
                                        std::function<void(bool success, const std::string& path)> callback) {
    if (!m_isInitialized) {
        if (callback) callback(false, "");
        return false;
    }

    // Simulate download
    DownloadProgress progress;
    progress.itemId = itemId;
    progress.bytesDownloaded = 0;
    progress.totalBytes = 500000;
    progress.percentage = 0.0f;
    progress.status = "Downloading...";
    m_downloadProgress[itemId] = progress;

    std::string downloadPath = "workshop/" + std::to_string(itemId);

    // Complete download
    m_downloadProgress[itemId].percentage = 100.0f;
    m_downloadProgress[itemId].status = "Complete";

    if (OnItemDownloaded) {
        OnItemDownloaded(itemId);
    }

    if (callback) {
        callback(true, downloadPath);
    }

    return true;
}

bool WorkshopIntegration::IsItemInstalled(uint64_t itemId) const {
    // Check if item is downloaded locally
    std::string path = "workshop/" + std::to_string(itemId);
    return std::filesystem::exists(path);
}

std::string WorkshopIntegration::GetInstalledItemPath(uint64_t itemId) const {
    return "workshop/" + std::to_string(itemId);
}

bool WorkshopIntegration::CheckForUpdates(
    std::function<void(const std::vector<uint64_t>& itemsNeedingUpdate)> callback) {
    if (!m_isInitialized) {
        if (callback) callback({});
        return false;
    }

    std::vector<uint64_t> needsUpdate;
    // Check subscribed items for updates
    // This would compare local version with remote version

    if (callback) {
        callback(needsUpdate);
    }

    return true;
}

bool WorkshopIntegration::QueryItems(const WorkshopQuery& query,
                                      std::function<void(const WorkshopQueryResult& result)> callback) {
    if (!m_isInitialized) {
        if (callback) {
            WorkshopQueryResult empty;
            empty.errorMessage = "Workshop not initialized";
            callback(empty);
        }
        return false;
    }

    // Simulate query results
    WorkshopQueryResult result;
    result.totalResults = 0;
    result.currentPage = query.pageIndex;
    result.totalPages = 0;
    result.hasMore = false;

    // In real implementation, query the workshop service

    if (callback) {
        callback(result);
    }

    return true;
}

bool WorkshopIntegration::GetItemInfo(uint64_t itemId,
                                       std::function<void(const WorkshopItemInfo& info)> callback) {
    if (!m_isInitialized) {
        if (callback) {
            WorkshopItemInfo empty;
            callback(empty);
        }
        return false;
    }

    auto it = m_itemCache.find(itemId);
    if (it != m_itemCache.end()) {
        if (callback) {
            callback(it->second);
        }
        return true;
    }

    // Query the workshop for item info
    WorkshopItemInfo info;
    info.itemId = itemId;

    if (callback) {
        callback(info);
    }

    return true;
}

bool WorkshopIntegration::GetUserItems(std::function<void(const WorkshopQueryResult& result)> callback) {
    WorkshopQuery query;
    query.publishedByMe = true;
    return QueryItems(query, callback);
}

bool WorkshopIntegration::SetRating(uint64_t itemId, bool voteUp) {
    if (!m_isInitialized) return false;
    // Submit rating to workshop
    return true;
}

bool WorkshopIntegration::AddToFavorites(uint64_t itemId) {
    if (!m_isInitialized) return false;

    if (!IsFavorite(itemId)) {
        m_favoriteItems.push_back(itemId);
    }
    return true;
}

bool WorkshopIntegration::RemoveFromFavorites(uint64_t itemId) {
    if (!m_isInitialized) return false;

    m_favoriteItems.erase(
        std::remove(m_favoriteItems.begin(), m_favoriteItems.end(), itemId),
        m_favoriteItems.end());
    return true;
}

bool WorkshopIntegration::IsFavorite(uint64_t itemId) const {
    return std::find(m_favoriteItems.begin(), m_favoriteItems.end(), itemId)
           != m_favoriteItems.end();
}

bool WorkshopIntegration::CreateCollection(const std::string& name, const std::vector<uint64_t>& items,
                                            std::function<void(uint64_t collectionId)> callback) {
    if (!m_isInitialized) {
        if (callback) callback(0);
        return false;
    }

    uint64_t collectionId = std::hash<std::string>{}(name + std::to_string(std::time(nullptr)));

    if (callback) {
        callback(collectionId);
    }

    return true;
}

bool WorkshopIntegration::AddToCollection(uint64_t collectionId, uint64_t itemId) {
    if (!m_isInitialized) return false;
    return true;
}

bool WorkshopIntegration::RemoveFromCollection(uint64_t collectionId, uint64_t itemId) {
    if (!m_isInitialized) return false;
    return true;
}

DownloadProgress WorkshopIntegration::GetDownloadProgress(uint64_t itemId) const {
    auto it = m_downloadProgress.find(itemId);
    if (it != m_downloadProgress.end()) {
        return it->second;
    }
    return DownloadProgress{};
}

std::string WorkshopIntegration::GetWorkshopItemUrl(uint64_t itemId) {
    // Steam Workshop URL format
    return "https://steamcommunity.com/sharedfiles/filedetails/?id=" + std::to_string(itemId);
}

std::string WorkshopIntegration::TypeToString(WorkshopItemType type) {
    switch (type) {
        case WorkshopItemType::Map: return "Map";
        case WorkshopItemType::Campaign: return "Campaign";
        case WorkshopItemType::GameMode: return "GameMode";
        case WorkshopItemType::Mod: return "Mod";
        case WorkshopItemType::Asset: return "Asset";
        case WorkshopItemType::Script: return "Script";
        case WorkshopItemType::Collection: return "Collection";
        default: return "Unknown";
    }
}

WorkshopItemType WorkshopIntegration::StringToType(const std::string& str) {
    if (str == "Map") return WorkshopItemType::Map;
    if (str == "Campaign") return WorkshopItemType::Campaign;
    if (str == "GameMode") return WorkshopItemType::GameMode;
    if (str == "Mod") return WorkshopItemType::Mod;
    if (str == "Asset") return WorkshopItemType::Asset;
    if (str == "Script") return WorkshopItemType::Script;
    if (str == "Collection") return WorkshopItemType::Collection;
    return WorkshopItemType::Map;
}

std::vector<std::string> WorkshopIntegration::GetTagsForType(WorkshopItemType type) {
    switch (type) {
        case WorkshopItemType::Map:
            return {"1v1", "2v2", "3v3", "4v4", "FFA", "Melee", "Custom", "Balanced", "Competitive"};
        case WorkshopItemType::Campaign:
            return {"Story", "Tutorial", "Co-op", "Short", "Long", "Beginner", "Expert"};
        case WorkshopItemType::GameMode:
            return {"PvP", "PvE", "Co-op", "Competitive", "Casual", "Tower Defense", "Survival"};
        case WorkshopItemType::Mod:
            return {"Total Conversion", "Balance", "Visual", "Audio", "Gameplay", "UI"};
        default:
            return {};
    }
}

// WorkshopPackager

bool WorkshopPackager::PackageMap(const std::string& mapPath, const std::string& outputPath,
                                   std::vector<std::string>& errors) {
    if (!std::filesystem::exists(mapPath)) {
        errors.push_back("Map file not found: " + mapPath);
        return false;
    }

    // Create output directory
    std::filesystem::create_directories(std::filesystem::path(outputPath).parent_path());

    // Copy map and related files
    try {
        std::filesystem::copy(mapPath, outputPath, std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (const std::exception& e) {
        errors.push_back(std::string("Failed to package map: ") + e.what());
        return false;
    }
}

bool WorkshopPackager::PackageCampaign(const std::string& campaignPath, const std::string& outputPath,
                                        std::vector<std::string>& errors) {
    if (!std::filesystem::exists(campaignPath)) {
        errors.push_back("Campaign file not found: " + campaignPath);
        return false;
    }

    // Package campaign and all referenced maps
    try {
        std::filesystem::create_directories(outputPath);
        std::filesystem::copy(campaignPath,
                              outputPath + "/campaign.vcampaign",
                              std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (const std::exception& e) {
        errors.push_back(std::string("Failed to package campaign: ") + e.what());
        return false;
    }
}

bool WorkshopPackager::PackageGameMode(const std::string& modePath, const std::string& outputPath,
                                        std::vector<std::string>& errors) {
    if (!std::filesystem::exists(modePath)) {
        errors.push_back("Game mode file not found: " + modePath);
        return false;
    }

    try {
        std::filesystem::copy(modePath, outputPath, std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (const std::exception& e) {
        errors.push_back(std::string("Failed to package game mode: ") + e.what());
        return false;
    }
}

bool WorkshopPackager::ValidateForUpload(const std::string& contentPath, WorkshopItemType type,
                                          std::vector<std::string>& errors) {
    if (!std::filesystem::exists(contentPath)) {
        errors.push_back("Content path does not exist");
        return false;
    }

    uint64_t size = GetPackageSize(contentPath);
    const uint64_t MAX_SIZE = 1024ULL * 1024 * 1024;  // 1 GB

    if (size > MAX_SIZE) {
        errors.push_back("Content exceeds maximum upload size (1 GB)");
        return false;
    }

    return errors.empty();
}

bool WorkshopPackager::GenerateThumbnail(const std::string& contentPath, const std::string& outputPath,
                                          int width, int height) {
    // Generate thumbnail image from map preview
    // This would render a preview of the map to an image
    return true;
}

uint64_t WorkshopPackager::GetPackageSize(const std::string& contentPath) {
    uint64_t size = 0;

    if (std::filesystem::is_directory(contentPath)) {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(contentPath)) {
            if (entry.is_regular_file()) {
                size += entry.file_size();
            }
        }
    } else if (std::filesystem::is_regular_file(contentPath)) {
        size = std::filesystem::file_size(contentPath);
    }

    return size;
}

bool WorkshopPackager::ExtractItem(const std::string& packagePath, const std::string& outputPath) {
    try {
        std::filesystem::create_directories(outputPath);
        std::filesystem::copy(packagePath, outputPath,
                              std::filesystem::copy_options::recursive |
                              std::filesystem::copy_options::overwrite_existing);
        return true;
    } catch (...) {
        return false;
    }
}

// WorkshopBrowser

WorkshopBrowser::WorkshopBrowser() {
    m_currentQuery.type = WorkshopItemType::Map;
    m_currentQuery.sortBy = "popular";
    m_currentQuery.pageSize = 20;
}

WorkshopBrowser::~WorkshopBrowser() = default;

void WorkshopBrowser::SetSearchFilter(const std::string& search) {
    m_currentQuery.searchText = search;
}

void WorkshopBrowser::SetTypeFilter(WorkshopItemType type) {
    m_currentQuery.type = type;
}

void WorkshopBrowser::SetTagFilter(const std::vector<std::string>& tags) {
    m_currentQuery.tags = tags;
}

void WorkshopBrowser::SetSortOrder(const std::string& sortBy, bool descending) {
    m_currentQuery.sortBy = sortBy;
    m_currentQuery.descendingOrder = descending;
}

void WorkshopBrowser::RefreshResults() {
    m_currentQuery.pageIndex = 0;
    m_isLoading = true;

    WorkshopIntegration::Instance().QueryItems(m_currentQuery, [this](const WorkshopQueryResult& result) {
        m_currentResults = result;
        m_isLoading = false;

        if (OnResultsUpdated) {
            OnResultsUpdated();
        }
    });
}

void WorkshopBrowser::NextPage() {
    if (m_currentResults.hasMore) {
        m_currentQuery.pageIndex++;
        RefreshResults();
    }
}

void WorkshopBrowser::PreviousPage() {
    if (m_currentQuery.pageIndex > 0) {
        m_currentQuery.pageIndex--;
        RefreshResults();
    }
}

void WorkshopBrowser::GoToPage(int page) {
    if (page >= 0 && page < m_currentResults.totalPages) {
        m_currentQuery.pageIndex = page;
        RefreshResults();
    }
}

void WorkshopBrowser::SelectItem(uint64_t itemId) {
    for (auto& item : m_currentResults.items) {
        if (item.itemId == itemId) {
            m_selectedItem = &item;

            if (OnItemSelected) {
                OnItemSelected(item);
            }
            return;
        }
    }
    m_selectedItem = nullptr;
}

void WorkshopBrowser::SubscribeSelected() {
    if (m_selectedItem) {
        WorkshopIntegration::Instance().Subscribe(m_selectedItem->itemId, nullptr);
    }
}

void WorkshopBrowser::UnsubscribeSelected() {
    if (m_selectedItem) {
        WorkshopIntegration::Instance().Unsubscribe(m_selectedItem->itemId, nullptr);
    }
}

void WorkshopBrowser::DownloadSelected() {
    if (m_selectedItem) {
        WorkshopIntegration::Instance().DownloadItem(m_selectedItem->itemId, nullptr);
    }
}

void WorkshopBrowser::RateSelected(bool positive) {
    if (m_selectedItem) {
        WorkshopIntegration::Instance().SetRating(m_selectedItem->itemId, positive);
    }
}

void WorkshopBrowser::FavoriteSelected() {
    if (m_selectedItem) {
        if (WorkshopIntegration::Instance().IsFavorite(m_selectedItem->itemId)) {
            WorkshopIntegration::Instance().RemoveFromFavorites(m_selectedItem->itemId);
        } else {
            WorkshopIntegration::Instance().AddToFavorites(m_selectedItem->itemId);
        }
    }
}

void WorkshopBrowser::OpenInBrowser() {
    if (m_selectedItem) {
        std::string url = WorkshopIntegration::GetWorkshopItemUrl(m_selectedItem->itemId);
        // Open URL in system browser
        #ifdef _WIN32
            system(("start " + url).c_str());
        #elif __APPLE__
            system(("open " + url).c_str());
        #else
            system(("xdg-open " + url).c_str());
        #endif
    }
}

} // namespace Vehement
