#include "ContentBrowser.hpp"
#include "WebViewManager.hpp"
#include "JSBridge.hpp"
#include "../Editor.hpp"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <imgui.h>

namespace Vehement {
namespace WebEditor {

ContentBrowser::ContentBrowser(Editor* editor)
    : m_editor(editor)
{
}

ContentBrowser::~ContentBrowser() {
    Shutdown();
}

bool ContentBrowser::Initialize(const std::string& configsPath) {
    m_configsPath = configsPath;

    // Create web view
    WebViewConfig config;
    config.id = "content_browser";
    config.title = "Content Browser";
    config.width = 800;
    config.height = 600;
    config.debug = true;

    m_webView.reset(new WebView(config));

    // Create bridge
    m_bridge.reset(new JSBridge());
    SetupJSBridge();

    // Load HTML
    std::string htmlPath = WebViewManager::Instance().ResolvePath("editor/html/content_browser.html");
    if (!m_webView->LoadFile(htmlPath)) {
        // Try embedded HTML as fallback
        m_webView->LoadHtml(GetDefaultTemplate(ContentType::Unknown));
    }

    // Enable hot-reload for development
    m_webView->EnableHotReload({
        htmlPath,
        WebViewManager::Instance().ResolvePath("editor/html/editor.css"),
        WebViewManager::Instance().ResolvePath("editor/html/content_browser.js")
    });

    // Initial content load
    RefreshContent();

    return true;
}

void ContentBrowser::Shutdown() {
    m_webView.reset();
    m_bridge.reset();
    m_allContent.clear();
    m_contentIndex.clear();
}

void ContentBrowser::Update(float deltaTime) {
    if (m_webView) {
        m_webView->Update(deltaTime);
    }

    if (m_bridge) {
        m_bridge->ProcessPending();
    }

    if (m_needsRefresh) {
        RefreshContent();
        m_needsRefresh = false;
    }
}

void ContentBrowser::Render() {
    if (!m_webView) return;

    // Render web view in ImGui
    WebViewManager::Instance().RenderImGuiInline(m_webView->GetId());
}

void ContentBrowser::SetupJSBridge() {
    if (!m_bridge) return;

    // Connect bridge to web view
    m_bridge->SetScriptExecutor([this](const std::string& script, JSCallback callback) {
        if (m_webView) {
            m_webView->ExecuteJS(script, [callback](const std::string& result) {
                if (callback) {
                    callback(JSResult::Success(JSValue::FromJson(result)));
                }
            });
        }
    });

    // Set up message handling from webview
    m_webView->SetMessageHandler([this](const std::string& type, const std::string& payload) {
        m_bridge->HandleIncomingMessage("{\"type\":\"" + type + "\",\"payload\":" + payload + "}");
    });

    RegisterBridgeFunctions();
}

void ContentBrowser::RegisterBridgeFunctions() {
    if (!m_bridge) return;

    // Get all content
    m_bridge->RegisterFunction("contentBrowser.getContent", [this](const auto& args) {
        JSValue::Array items;
        for (const auto& item : GetFilteredContent()) {
            JSValue::Object obj;
            obj["id"] = item.id;
            obj["name"] = item.name;
            obj["description"] = item.description;
            obj["type"] = ContentTypeToString(item.type);
            obj["filePath"] = item.filePath;
            obj["thumbnailPath"] = item.thumbnailPath;
            obj["lastModified"] = item.lastModified;
            obj["isDirty"] = item.isDirty;
            obj["hasErrors"] = item.hasErrors;

            JSValue::Array tags;
            for (const auto& tag : item.tags) {
                tags.push_back(tag);
            }
            obj["tags"] = tags;

            items.push_back(obj);
        }
        return JSResult::Success(items);
    });

    // Get single item
    m_bridge->RegisterFunction("contentBrowser.getItem", [this](const auto& args) {
        if (args.empty() || !args[0].IsString()) {
            return JSResult::Error("Missing item ID");
        }

        std::string id = args[0].AsString();
        auto item = GetContentItem(id);
        if (!item) {
            return JSResult::Error("Item not found: " + id);
        }

        JSValue::Object obj;
        obj["id"] = item->id;
        obj["name"] = item->name;
        obj["description"] = item->description;
        obj["type"] = ContentTypeToString(item->type);
        obj["filePath"] = item->filePath;
        return JSResult::Success(obj);
    });

    // Get item JSON data
    m_bridge->RegisterFunction("contentBrowser.getItemData", [this](const auto& args) {
        if (args.empty() || !args[0].IsString()) {
            return JSResult::Error("Missing item ID");
        }

        std::string id = args[0].AsString();
        std::string filePath = GetFilePath(id);

        std::ifstream file(filePath);
        if (!file.is_open()) {
            return JSResult::Error("Failed to open file: " + filePath);
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        return JSResult::Success(JSValue::FromJson(buffer.str()));
    });

    // Select item
    m_bridge->RegisterFunction("contentBrowser.selectItem", [this](const auto& args) {
        if (args.empty() || !args[0].IsString()) {
            return JSResult::Error("Missing item ID");
        }

        SelectItem(args[0].AsString());
        return JSResult::Success();
    });

    // Create item
    m_bridge->RegisterFunction("contentBrowser.createItem", [this](const auto& args) {
        if (args.size() < 2) {
            return JSResult::Error("Missing type and name");
        }

        ContentType type = StringToContentType(args[0].GetString());
        std::string name = args[1].GetString();

        std::string id = CreateItem(type, name);
        if (id.empty()) {
            return JSResult::Error("Failed to create item");
        }

        return JSResult::Success(id);
    });

    // Delete item
    m_bridge->RegisterFunction("contentBrowser.deleteItem", [this](const auto& args) {
        if (args.empty() || !args[0].IsString()) {
            return JSResult::Error("Missing item ID");
        }

        bool success = DeleteItem(args[0].AsString());
        return success ? JSResult::Success() : JSResult::Error("Failed to delete item");
    });

    // Duplicate item
    m_bridge->RegisterFunction("contentBrowser.duplicateItem", [this](const auto& args) {
        if (args.empty() || !args[0].IsString()) {
            return JSResult::Error("Missing item ID");
        }

        std::string newId = DuplicateItem(args[0].AsString());
        if (newId.empty()) {
            return JSResult::Error("Failed to duplicate item");
        }

        return JSResult::Success(newId);
    });

    // Save item
    m_bridge->RegisterFunction("contentBrowser.saveItem", [this](const auto& args) {
        if (args.size() < 2) {
            return JSResult::Error("Missing item ID and data");
        }

        std::string id = args[0].GetString();
        JSValue data = args[1];

        std::string filePath = GetFilePath(id);
        std::ofstream file(filePath);
        if (!file.is_open()) {
            return JSResult::Error("Failed to open file for writing");
        }

        file << JSON::Stringify(data, true);
        return JSResult::Success();
    });

    // Set filter
    m_bridge->RegisterFunction("contentBrowser.setFilter", [this](const auto& args) {
        if (args.empty() || !args[0].IsObject()) {
            return JSResult::Error("Invalid filter object");
        }

        ContentFilter filter;
        const auto& obj = args[0].AsObject();

        auto queryIt = obj.find("searchQuery");
        if (queryIt != obj.end()) {
            filter.searchQuery = queryIt->second.GetString();
        }

        auto typesIt = obj.find("types");
        if (typesIt != obj.end() && typesIt->second.IsArray()) {
            for (const auto& t : typesIt->second.AsArray()) {
                filter.types.push_back(StringToContentType(t.GetString()));
            }
        }

        SetFilter(filter);
        return JSResult::Success();
    });

    // Refresh content
    m_bridge->RegisterFunction("contentBrowser.refresh", [this](const auto& args) {
        m_needsRefresh = true;
        return JSResult::Success();
    });

    // Open item in editor
    m_bridge->RegisterFunction("contentBrowser.openInEditor", [this](const auto& args) {
        if (args.empty() || !args[0].IsString()) {
            return JSResult::Error("Missing item ID");
        }

        if (OnItemDoubleClicked) {
            OnItemDoubleClicked(args[0].AsString());
        }
        return JSResult::Success();
    });

    // Get categories/types
    m_bridge->RegisterFunction("contentBrowser.getCategories", [this](const auto& args) {
        JSValue::Array categories;

        std::unordered_map<ContentType, int> counts;
        for (const auto& item : m_allContent) {
            counts[item.type]++;
        }

        const ContentType types[] = {
            ContentType::Spell, ContentType::Unit, ContentType::Building,
            ContentType::TechTree, ContentType::Effect, ContentType::Buff,
            ContentType::Culture, ContentType::Hero, ContentType::Ability
        };

        for (auto type : types) {
            JSValue::Object cat;
            cat["type"] = ContentTypeToString(type);
            cat["count"] = counts[type];
            categories.push_back(cat);
        }

        return JSResult::Success(categories);
    });
}

void ContentBrowser::RefreshContent() {
    m_allContent.clear();
    m_contentIndex.clear();

    // Load content from each type directory
    const ContentType types[] = {
        ContentType::Spell, ContentType::Unit, ContentType::Building,
        ContentType::TechTree, ContentType::Effect, ContentType::Buff,
        ContentType::Resource, ContentType::Culture, ContentType::Hero,
        ContentType::Ability, ContentType::Event, ContentType::Script
    };

    for (auto type : types) {
        std::string path = GetContentPath(type);
        if (std::filesystem::exists(path)) {
            LoadContentFromDirectory(path, type);
        }
    }

    // Sort by name
    std::sort(m_allContent.begin(), m_allContent.end(),
              [](const auto& a, const auto& b) { return a.name < b.name; });

    // Rebuild index
    for (size_t i = 0; i < m_allContent.size(); ++i) {
        m_contentIndex[m_allContent[i].id] = i;
    }

    // Notify web view
    if (m_bridge) {
        m_bridge->EmitEvent("contentRefreshed", {});
    }
}

void ContentBrowser::LoadContentFromDirectory(const std::string& path, ContentType type) {
    try {
        for (const auto& entry : std::filesystem::directory_iterator(path)) {
            if (entry.is_regular_file() && entry.path().extension() == ".json") {
                ContentItem item = ParseContentFile(entry.path().string(), type);
                if (!item.id.empty()) {
                    m_allContent.push_back(std::move(item));
                }
            }
        }
    } catch (const std::exception& e) {
        // Log error
    }
}

ContentItem ContentBrowser::ParseContentFile(const std::string& path, ContentType type) {
    ContentItem item;
    item.type = type;
    item.filePath = path;

    // Extract filename as ID
    std::filesystem::path fsPath(path);
    item.id = fsPath.stem().string();

    // Try to parse JSON for metadata
    std::ifstream file(path);
    if (file.is_open()) {
        std::stringstream buffer;
        buffer << file.rdbuf();

        JSValue json = JSON::Parse(buffer.str());
        if (json.IsObject()) {
            item.name = json["name"].GetString(item.id);
            item.description = json["description"].GetString();

            if (json["tags"].IsArray()) {
                for (const auto& tag : json["tags"].AsArray()) {
                    item.tags.push_back(tag.GetString());
                }
            }
        }
    }

    // Get modification time
    try {
        auto ftime = std::filesystem::last_write_time(path);
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now()
            + std::chrono::system_clock::now()
        );
        auto time = std::chrono::system_clock::to_time_t(sctp);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        item.lastModified = oss.str();
    } catch (...) {
        item.lastModified = "Unknown";
    }

    return item;
}

std::vector<ContentItem> ContentBrowser::GetFilteredContent() const {
    std::vector<ContentItem> result;

    for (const auto& item : m_allContent) {
        // Filter by type
        if (!m_filter.types.empty()) {
            bool typeMatch = false;
            for (auto type : m_filter.types) {
                if (item.type == type) {
                    typeMatch = true;
                    break;
                }
            }
            if (!typeMatch) continue;
        }

        // Filter by search query
        if (!m_filter.searchQuery.empty()) {
            std::string query = m_filter.searchQuery;
            std::string name = item.name;
            std::string desc = item.description;

            // Case-insensitive search
            std::transform(query.begin(), query.end(), query.begin(), ::tolower);
            std::transform(name.begin(), name.end(), name.begin(), ::tolower);
            std::transform(desc.begin(), desc.end(), desc.begin(), ::tolower);

            if (name.find(query) == std::string::npos &&
                desc.find(query) == std::string::npos &&
                item.id.find(query) == std::string::npos) {
                continue;
            }
        }

        // Filter by tags
        if (!m_filter.tags.empty()) {
            bool tagMatch = false;
            for (const auto& filterTag : m_filter.tags) {
                for (const auto& itemTag : item.tags) {
                    if (itemTag == filterTag) {
                        tagMatch = true;
                        break;
                    }
                }
                if (tagMatch) break;
            }
            if (!tagMatch) continue;
        }

        // Filter by state
        if (m_filter.showDirtyOnly && !item.isDirty) continue;
        if (m_filter.showErrorsOnly && !item.hasErrors) continue;

        result.push_back(item);
    }

    // Sort
    switch (m_filter.sortBy) {
        case ContentFilter::SortBy::Name:
            std::sort(result.begin(), result.end(),
                      [&](const auto& a, const auto& b) {
                          return m_filter.sortAscending ? a.name < b.name : a.name > b.name;
                      });
            break;
        case ContentFilter::SortBy::Type:
            std::sort(result.begin(), result.end(),
                      [&](const auto& a, const auto& b) {
                          return m_filter.sortAscending ? a.type < b.type : a.type > b.type;
                      });
            break;
        case ContentFilter::SortBy::Modified:
            std::sort(result.begin(), result.end(),
                      [&](const auto& a, const auto& b) {
                          return m_filter.sortAscending ?
                              a.lastModified < b.lastModified :
                              a.lastModified > b.lastModified;
                      });
            break;
        default:
            break;
    }

    return result;
}

std::optional<ContentItem> ContentBrowser::GetContentItem(const std::string& id) const {
    auto it = m_contentIndex.find(id);
    if (it != m_contentIndex.end() && it->second < m_allContent.size()) {
        return m_allContent[it->second];
    }
    return std::nullopt;
}

std::vector<ContentItem> ContentBrowser::GetContentByType(ContentType type) const {
    std::vector<ContentItem> result;
    for (const auto& item : m_allContent) {
        if (item.type == type) {
            result.push_back(item);
        }
    }
    return result;
}

std::string ContentBrowser::CreateItem(ContentType type, const std::string& name) {
    std::string id = GenerateContentId(type, name);
    std::string filePath = GetContentPath(type) + "/" + id + ".json";

    // Create directory if needed
    std::filesystem::create_directories(GetContentPath(type));

    // Create default content
    JSValue::Object content;
    content["id"] = id;
    content["name"] = name;
    content["description"] = "";
    content["tags"] = JSValue::Array{};

    std::ofstream file(filePath);
    if (!file.is_open()) {
        return "";
    }

    file << JSON::Stringify(JSValue(content), true);
    file.close();

    // Add to list
    ContentItem item;
    item.id = id;
    item.name = name;
    item.type = type;
    item.filePath = filePath;
    item.isNew = true;

    m_allContent.push_back(item);
    m_contentIndex[id] = m_allContent.size() - 1;

    if (OnItemCreated) {
        OnItemCreated(id);
    }

    return id;
}

std::string ContentBrowser::DuplicateItem(const std::string& id) {
    auto item = GetContentItem(id);
    if (!item) {
        return "";
    }

    // Read original content
    std::ifstream inFile(item->filePath);
    if (!inFile.is_open()) {
        return "";
    }

    std::stringstream buffer;
    buffer << inFile.rdbuf();
    JSValue content = JSON::Parse(buffer.str());

    // Generate new ID and name
    std::string newName = item->name + " (Copy)";
    std::string newId = GenerateContentId(item->type, newName);

    // Update content
    content["id"] = newId;
    content["name"] = newName;

    // Write new file
    std::string newPath = GetContentPath(item->type) + "/" + newId + ".json";
    std::ofstream outFile(newPath);
    if (!outFile.is_open()) {
        return "";
    }

    outFile << JSON::Stringify(content, true);
    outFile.close();

    // Add to list
    ContentItem newItem = *item;
    newItem.id = newId;
    newItem.name = newName;
    newItem.filePath = newPath;
    newItem.isNew = true;
    newItem.isDirty = false;

    m_allContent.push_back(newItem);
    m_contentIndex[newId] = m_allContent.size() - 1;

    if (OnItemCreated) {
        OnItemCreated(newId);
    }

    return newId;
}

bool ContentBrowser::DeleteItem(const std::string& id) {
    auto it = m_contentIndex.find(id);
    if (it == m_contentIndex.end()) {
        return false;
    }

    size_t index = it->second;
    const ContentItem& item = m_allContent[index];

    // Delete file
    try {
        std::filesystem::remove(item.filePath);
    } catch (...) {
        return false;
    }

    // Remove from list
    m_allContent.erase(m_allContent.begin() + index);
    m_contentIndex.erase(it);

    // Rebuild index
    m_contentIndex.clear();
    for (size_t i = 0; i < m_allContent.size(); ++i) {
        m_contentIndex[m_allContent[i].id] = i;
    }

    // Clear selection if deleted item was selected
    if (m_selectedId == id) {
        m_selectedId.clear();
    }

    if (OnItemDeleted) {
        OnItemDeleted(id);
    }

    return true;
}

bool ContentBrowser::RenameItem(const std::string& id, const std::string& newName) {
    auto it = m_contentIndex.find(id);
    if (it == m_contentIndex.end()) {
        return false;
    }

    ContentItem& item = m_allContent[it->second];

    // Update JSON file
    std::ifstream inFile(item.filePath);
    if (!inFile.is_open()) {
        return false;
    }

    std::stringstream buffer;
    buffer << inFile.rdbuf();
    inFile.close();

    JSValue content = JSON::Parse(buffer.str());
    content["name"] = newName;

    std::ofstream outFile(item.filePath);
    if (!outFile.is_open()) {
        return false;
    }

    outFile << JSON::Stringify(content, true);
    outFile.close();

    item.name = newName;
    item.isDirty = false;

    if (OnItemModified) {
        OnItemModified(id);
    }

    return true;
}

bool ContentBrowser::SaveItem(const std::string& id) {
    auto it = m_contentIndex.find(id);
    if (it == m_contentIndex.end()) {
        return false;
    }

    ContentItem& item = m_allContent[it->second];
    item.isDirty = false;

    return true;
}

bool ContentBrowser::ReloadItem(const std::string& id) {
    auto it = m_contentIndex.find(id);
    if (it == m_contentIndex.end()) {
        return false;
    }

    size_t index = it->second;
    m_allContent[index] = ParseContentFile(m_allContent[index].filePath, m_allContent[index].type);

    return true;
}

void ContentBrowser::SelectItem(const std::string& id) {
    m_selectedId = id;

    if (OnItemSelected) {
        OnItemSelected(id);
    }

    // Notify web view
    if (m_bridge) {
        JSValue::Object data;
        data["id"] = id;
        m_bridge->EmitEvent("itemSelected", data);
    }
}

std::optional<ContentItem> ContentBrowser::GetSelectedItem() const {
    if (m_selectedId.empty()) {
        return std::nullopt;
    }
    return GetContentItem(m_selectedId);
}

void ContentBrowser::SetFilter(const ContentFilter& filter) {
    m_filter = filter;

    // Notify web view
    if (m_bridge) {
        m_bridge->EmitEvent("filterChanged", {});
    }
}

void ContentBrowser::SetSearchQuery(const std::string& query) {
    m_filter.searchQuery = query;
}

void ContentBrowser::FilterByType(ContentType type) {
    m_filter.types = {type};
}

void ContentBrowser::FilterByTag(const std::string& tag) {
    m_filter.tags = {tag};
}

void ContentBrowser::ClearFilters() {
    m_filter = ContentFilter{};
}

void ContentBrowser::BeginDrag(const std::string& id) {
    m_draggedId = id;
}

void ContentBrowser::EndDrag(const std::string& targetId) {
    if (!m_draggedId.empty() && OnItemMoved) {
        OnItemMoved(m_draggedId, targetId);
    }
    m_draggedId.clear();
}

std::string ContentBrowser::GetPreviewHtml(const std::string& id) const {
    auto item = GetContentItem(id);
    if (!item) {
        return "<p>Item not found</p>";
    }

    std::ostringstream html;
    html << "<div class='preview'>";
    html << "<h2>" << item->name << "</h2>";
    html << "<p class='type'>" << ContentTypeToString(item->type) << "</p>";
    html << "<p class='description'>" << item->description << "</p>";
    html << "<p class='path'>" << item->filePath << "</p>";
    html << "<p class='modified'>Modified: " << item->lastModified << "</p>";
    html << "</div>";

    return html.str();
}

bool ContentBrowser::GenerateThumbnail(const std::string& id) {
    // TODO: Generate thumbnail based on content type
    // For units/buildings: render 3D model
    // For spells/effects: render particle preview
    // For others: generate icon
    return false;
}

std::string ContentBrowser::GenerateContentId(ContentType type, const std::string& name) {
    // Generate ID from name (lowercase, underscores for spaces)
    std::string id = name;
    std::transform(id.begin(), id.end(), id.begin(), ::tolower);
    std::replace(id.begin(), id.end(), ' ', '_');

    // Remove non-alphanumeric characters
    id.erase(std::remove_if(id.begin(), id.end(), [](char c) {
        return !std::isalnum(c) && c != '_';
    }), id.end());

    // Ensure uniqueness
    std::string baseId = id;
    int counter = 1;
    while (m_contentIndex.find(id) != m_contentIndex.end()) {
        id = baseId + "_" + std::to_string(counter++);
    }

    return id;
}

std::string ContentBrowser::GetDefaultTemplate(ContentType type) {
    // Return default HTML for fallback
    return R"(
<!DOCTYPE html>
<html>
<head>
    <style>
        body { font-family: Arial, sans-serif; background: #1e1e1e; color: #fff; margin: 0; }
        .container { display: flex; height: 100vh; }
        .sidebar { width: 200px; background: #252526; padding: 10px; }
        .content { flex: 1; padding: 10px; }
        .item { padding: 8px; margin: 4px 0; background: #333; cursor: pointer; }
        .item:hover { background: #444; }
    </style>
</head>
<body>
    <div class="container">
        <div class="sidebar">
            <input type="text" placeholder="Search..." style="width: 100%; padding: 8px;">
            <div class="category">Spells</div>
            <div class="category">Units</div>
            <div class="category">Buildings</div>
        </div>
        <div class="content">
            <p>Content Browser - Loading...</p>
        </div>
    </div>
</body>
</html>
)";
}

std::string ContentBrowser::GetContentPath(ContentType type) const {
    return m_configsPath + "/" + ContentTypeToString(type);
}

std::string ContentBrowser::GetFilePath(const std::string& id) const {
    auto item = GetContentItem(id);
    if (item) {
        return item->filePath;
    }
    return "";
}

} // namespace WebEditor
} // namespace Vehement
