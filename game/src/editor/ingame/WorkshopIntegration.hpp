#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <unordered_map>

namespace Vehement {

/**
 * @brief Workshop item types
 */
enum class WorkshopItemType {
    Map,
    Campaign,
    GameMode,
    Mod,
    Asset,
    Script,
    Collection
};

/**
 * @brief Workshop item visibility
 */
enum class WorkshopVisibility {
    Public,
    FriendsOnly,
    Private,
    Unlisted
};

/**
 * @brief Workshop item status
 */
enum class WorkshopItemStatus {
    None,
    Creating,
    Uploading,
    Published,
    Updating,
    Error,
    Banned
};

/**
 * @brief Workshop item metadata
 */
struct WorkshopItemInfo {
    uint64_t itemId = 0;
    std::string title;
    std::string description;
    WorkshopItemType type;
    WorkshopVisibility visibility;
    WorkshopItemStatus status;
    std::string thumbnailPath;
    std::string contentPath;
    std::string changeNotes;
    std::vector<std::string> tags;
    std::string version;
    uint64_t createdTime;
    uint64_t updatedTime;
    uint64_t fileSize;
    int subscriptionCount;
    int favoriteCount;
    int downloadCount;
    float rating;
    int ratingCount;
    std::string authorId;
    std::string authorName;
    bool isSubscribed;
    bool isInstalled;
    bool needsUpdate;
};

/**
 * @brief Workshop query parameters
 */
struct WorkshopQuery {
    std::string searchText;
    WorkshopItemType type = WorkshopItemType::Map;
    std::vector<std::string> tags;
    std::string sortBy = "popular";  // popular, recent, rating, subscriptions
    bool descendingOrder = true;
    int pageSize = 20;
    int pageIndex = 0;
    std::string authorId;
    bool subscribedOnly = false;
    bool publishedByMe = false;
};

/**
 * @brief Workshop query result
 */
struct WorkshopQueryResult {
    std::vector<WorkshopItemInfo> items;
    int totalResults;
    int currentPage;
    int totalPages;
    bool hasMore;
    std::string errorMessage;
};

/**
 * @brief Upload progress info
 */
struct UploadProgress {
    uint64_t bytesUploaded;
    uint64_t totalBytes;
    float percentage;
    std::string currentFile;
    std::string status;
};

/**
 * @brief Download progress info
 */
struct DownloadProgress {
    uint64_t itemId;
    uint64_t bytesDownloaded;
    uint64_t totalBytes;
    float percentage;
    std::string status;
};

/**
 * @brief Workshop Integration - Steam/platform workshop support
 *
 * Features:
 * - Publish maps, campaigns, mods
 * - Subscribe to content
 * - Update published items
 * - Browse and search
 * - Ratings and favorites
 */
class WorkshopIntegration {
public:
    static WorkshopIntegration& Instance();

    // Initialization
    bool Initialize();
    void Shutdown();
    bool IsAvailable() const { return m_isInitialized; }

    // Publishing
    bool CreateItem(WorkshopItemType type, std::function<void(uint64_t itemId, bool success)> callback);
    bool UpdateItem(uint64_t itemId, const WorkshopItemInfo& info,
                    std::function<void(bool success, const std::string& error)> callback);
    bool DeleteItem(uint64_t itemId, std::function<void(bool success)> callback);
    bool SetItemContent(uint64_t itemId, const std::string& contentPath);
    bool SetItemThumbnail(uint64_t itemId, const std::string& thumbnailPath);
    bool SubmitUpdate(uint64_t itemId, const std::string& changeNotes,
                      std::function<void(bool success, const std::string& error)> callback);

    // Subscription management
    bool Subscribe(uint64_t itemId, std::function<void(bool success)> callback);
    bool Unsubscribe(uint64_t itemId, std::function<void(bool success)> callback);
    std::vector<uint64_t> GetSubscribedItems();
    bool IsSubscribed(uint64_t itemId) const;

    // Downloading
    bool DownloadItem(uint64_t itemId, std::function<void(bool success, const std::string& path)> callback);
    bool IsItemInstalled(uint64_t itemId) const;
    std::string GetInstalledItemPath(uint64_t itemId) const;
    bool CheckForUpdates(std::function<void(const std::vector<uint64_t>& itemsNeedingUpdate)> callback);

    // Queries
    bool QueryItems(const WorkshopQuery& query, std::function<void(const WorkshopQueryResult& result)> callback);
    bool GetItemInfo(uint64_t itemId, std::function<void(const WorkshopItemInfo& info)> callback);
    bool GetUserItems(std::function<void(const WorkshopQueryResult& result)> callback);

    // Ratings and favorites
    bool SetRating(uint64_t itemId, bool voteUp);
    bool AddToFavorites(uint64_t itemId);
    bool RemoveFromFavorites(uint64_t itemId);
    bool IsFavorite(uint64_t itemId) const;

    // Collections
    bool CreateCollection(const std::string& name, const std::vector<uint64_t>& items,
                          std::function<void(uint64_t collectionId)> callback);
    bool AddToCollection(uint64_t collectionId, uint64_t itemId);
    bool RemoveFromCollection(uint64_t collectionId, uint64_t itemId);

    // Progress tracking
    UploadProgress GetUploadProgress() const { return m_uploadProgress; }
    DownloadProgress GetDownloadProgress(uint64_t itemId) const;

    // Events
    std::function<void(uint64_t itemId, bool success)> OnItemCreated;
    std::function<void(uint64_t itemId, bool success)> OnItemUpdated;
    std::function<void(uint64_t itemId)> OnItemDownloaded;
    std::function<void(uint64_t itemId)> OnItemSubscribed;
    std::function<void(uint64_t itemId)> OnItemUnsubscribed;
    std::function<void(const UploadProgress&)> OnUploadProgress;
    std::function<void(const DownloadProgress&)> OnDownloadProgress;

    // Utilities
    static std::string GetWorkshopItemUrl(uint64_t itemId);
    static std::string TypeToString(WorkshopItemType type);
    static WorkshopItemType StringToType(const std::string& str);
    static std::vector<std::string> GetTagsForType(WorkshopItemType type);

private:
    WorkshopIntegration() = default;
    ~WorkshopIntegration() = default;
    WorkshopIntegration(const WorkshopIntegration&) = delete;
    WorkshopIntegration& operator=(const WorkshopIntegration&) = delete;

    void UpdateCallbacks();
    bool PackageContent(const std::string& sourcePath, const std::string& outputPath);
    bool ExtractContent(const std::string& packagePath, const std::string& outputPath);

    bool m_isInitialized = false;
    UploadProgress m_uploadProgress;
    std::unordered_map<uint64_t, DownloadProgress> m_downloadProgress;
    std::unordered_map<uint64_t, WorkshopItemInfo> m_itemCache;
    std::vector<uint64_t> m_subscribedItems;
    std::vector<uint64_t> m_favoriteItems;

    // Platform-specific handle
    void* m_platformHandle = nullptr;
};

/**
 * @brief Workshop content packager
 */
class WorkshopPackager {
public:
    /**
     * @brief Package a map for workshop upload
     */
    static bool PackageMap(const std::string& mapPath, const std::string& outputPath,
                           std::vector<std::string>& errors);

    /**
     * @brief Package a campaign for workshop upload
     */
    static bool PackageCampaign(const std::string& campaignPath, const std::string& outputPath,
                                std::vector<std::string>& errors);

    /**
     * @brief Package a game mode for workshop upload
     */
    static bool PackageGameMode(const std::string& modePath, const std::string& outputPath,
                                std::vector<std::string>& errors);

    /**
     * @brief Validate content before upload
     */
    static bool ValidateForUpload(const std::string& contentPath, WorkshopItemType type,
                                  std::vector<std::string>& errors);

    /**
     * @brief Generate thumbnail from map/campaign
     */
    static bool GenerateThumbnail(const std::string& contentPath, const std::string& outputPath,
                                  int width = 512, int height = 512);

    /**
     * @brief Get estimated upload size
     */
    static uint64_t GetPackageSize(const std::string& contentPath);

    /**
     * @brief Extract workshop item to local directory
     */
    static bool ExtractItem(const std::string& packagePath, const std::string& outputPath);
};

/**
 * @brief Workshop item browser/manager
 */
class WorkshopBrowser {
public:
    WorkshopBrowser();
    ~WorkshopBrowser();

    // UI state
    void SetSearchFilter(const std::string& search);
    void SetTypeFilter(WorkshopItemType type);
    void SetTagFilter(const std::vector<std::string>& tags);
    void SetSortOrder(const std::string& sortBy, bool descending = true);

    // Navigation
    void RefreshResults();
    void NextPage();
    void PreviousPage();
    void GoToPage(int page);

    // Current results
    const WorkshopQueryResult& GetCurrentResults() const { return m_currentResults; }
    const WorkshopItemInfo* GetSelectedItem() const { return m_selectedItem; }
    void SelectItem(uint64_t itemId);

    // Actions
    void SubscribeSelected();
    void UnsubscribeSelected();
    void DownloadSelected();
    void RateSelected(bool positive);
    void FavoriteSelected();
    void OpenInBrowser();

    // Events
    std::function<void()> OnResultsUpdated;
    std::function<void(const WorkshopItemInfo&)> OnItemSelected;

private:
    WorkshopQuery m_currentQuery;
    WorkshopQueryResult m_currentResults;
    WorkshopItemInfo* m_selectedItem = nullptr;
    bool m_isLoading = false;
};

} // namespace Vehement
