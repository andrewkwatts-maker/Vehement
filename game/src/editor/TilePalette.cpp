#include "TilePalette.hpp"
#include <algorithm>
#include <cctype>

// Forward declare ImGui functions that would be used
// In actual implementation, include imgui.h
namespace ImGui {
    bool Begin(const char*, bool* = nullptr, int = 0) { return true; }
    void End() {}
    bool BeginTabBar(const char*, int = 0) { return true; }
    void EndTabBar() {}
    bool BeginTabItem(const char*, bool* = nullptr, int = 0) { return true; }
    void EndTabItem() {}
    bool InputText(const char*, char*, size_t, int = 0) { return false; }
    void Image(void*, const glm::vec2&) {}
    bool ImageButton(const char*, void*, const glm::vec2&) { return false; }
    void Text(const char*, ...) {}
    void TextWrapped(const char*, ...) {}
    void Separator() {}
    void SameLine(float = 0, float = -1) {}
    void BeginChild(const char*, const glm::vec2& = {}, bool = false, int = 0) {}
    void EndChild() {}
    void SetScrollY(float) {}
    float GetScrollY() { return 0; }
    float GetScrollMaxY() { return 0; }
    bool IsItemHovered() { return false; }
    bool IsItemClicked(int = 0) { return false; }
    void BeginTooltip() {}
    void EndTooltip() {}
    void PushStyleColor(int, const glm::vec4&) {}
    void PopStyleColor(int = 1) {}
    void PushID(int) {}
    void PopID() {}
    glm::vec2 GetCursorScreenPos() { return {}; }
    glm::vec2 GetContentRegionAvail() { return {}; }
    void Dummy(const glm::vec2&) {}
    void SetCursorScreenPos(const glm::vec2&) {}
    bool Button(const char*, const glm::vec2& = {}) { return false; }
    void Columns(int = 1, const char* = nullptr, bool = true) {}
    void NextColumn() {}
}

namespace Vehement {

// ============================================================================
// TileAtlas Implementation
// ============================================================================

void TileAtlas::Initialize(Nova::TextureManager& textureManager, const std::string& basePath) {
    if (m_initialized) {
        return;
    }

    m_textureManager = &textureManager;
    m_basePath = basePath;

    // Ensure base path ends with /
    if (!m_basePath.empty() && m_basePath.back() != '/') {
        m_basePath += '/';
    }

    LoadAllTiles();
    m_initialized = true;
}

std::shared_ptr<Nova::Texture> TileAtlas::GetTexture(TileType type) const {
    auto it = m_tileIndexMap.find(type);
    if (it != m_tileIndexMap.end()) {
        return m_tiles[it->second].thumbnail;
    }
    return nullptr;
}

std::vector<const TileEntry*> TileAtlas::GetTilesByCategory(int category) const {
    std::vector<const TileEntry*> result;
    for (const auto& entry : m_tiles) {
        if (GetTileCategory(entry.type) == category) {
            result.push_back(&entry);
        }
    }
    return result;
}

const TileEntry* TileAtlas::GetTileEntry(TileType type) const {
    auto it = m_tileIndexMap.find(type);
    if (it != m_tileIndexMap.end()) {
        return &m_tiles[it->second];
    }
    return nullptr;
}

void TileAtlas::LoadAllTiles() {
    // Load all tile types
    // Ground tiles
    LoadTile(TileType::GroundDirt);
    LoadTile(TileType::GroundForrest1);
    LoadTile(TileType::GroundForrest2);
    LoadTile(TileType::GroundGrass1);
    LoadTile(TileType::GroundGrass2);
    LoadTile(TileType::GroundRocks);

    // Concrete tiles
    LoadTile(TileType::ConcreteAsphalt1);
    LoadTile(TileType::ConcreteAsphalt2);
    LoadTile(TileType::ConcreteAsphalt2Steps1);
    LoadTile(TileType::ConcreteAsphalt2Steps2);
    LoadTile(TileType::ConcreteBlocks1);
    LoadTile(TileType::ConcreteBlocks2);
    LoadTile(TileType::ConcretePad);
    LoadTile(TileType::ConcreteTiles1);
    LoadTile(TileType::ConcreteTiles2);

    // Brick tiles
    LoadTile(TileType::BricksBlack);
    LoadTile(TileType::BricksGrey);
    LoadTile(TileType::BricksRock);
    LoadTile(TileType::BricksRockFrontBot);
    LoadTile(TileType::BricksRockFrontLhs);
    LoadTile(TileType::BricksRockFrontRhs);
    LoadTile(TileType::BricksRockFrontTop);
    LoadTile(TileType::BricksStacked);
    LoadTile(TileType::BricksCornerBL);
    LoadTile(TileType::BricksCornerBLRI);
    LoadTile(TileType::BricksCornerBLRO);
    LoadTile(TileType::BricksCornerBR);
    LoadTile(TileType::BricksCornerBRRI);
    LoadTile(TileType::BricksCornerBRRO);
    LoadTile(TileType::BricksCornerTL);
    LoadTile(TileType::BricksCornerTLRI);
    LoadTile(TileType::BricksCornerTLRO);
    LoadTile(TileType::BricksCornerTR);
    LoadTile(TileType::BricksCornerTRRI);
    LoadTile(TileType::BricksCornerTRRO);

    // Wood tiles
    LoadTile(TileType::Wood1);
    LoadTile(TileType::WoodCrate1);
    LoadTile(TileType::WoodCrate2);
    LoadTile(TileType::WoodFlooring1);
    LoadTile(TileType::WoodFlooring2);

    // Stone tiles
    LoadTile(TileType::StoneBlack);
    LoadTile(TileType::StoneMarble1);
    LoadTile(TileType::StoneMarble2);
    LoadTile(TileType::StoneRaw);

    // Metal tiles
    LoadTile(TileType::Metal1);
    LoadTile(TileType::Metal2);
    LoadTile(TileType::Metal3);
    LoadTile(TileType::Metal4);
    LoadTile(TileType::MetalTile1);
    LoadTile(TileType::MetalTile2);
    LoadTile(TileType::MetalTile3);
    LoadTile(TileType::MetalTile4);
    LoadTile(TileType::MetalShopFront);
    LoadTile(TileType::MetalShopFrontB);
    LoadTile(TileType::MetalShopFrontL);
    LoadTile(TileType::MetalShopFrontR);
    LoadTile(TileType::MetalShopFrontT);

    // Foliage tiles
    LoadTile(TileType::FoliageBonsai);
    LoadTile(TileType::FoliageBottleBrush);
    LoadTile(TileType::FoliageCherryTree);
    LoadTile(TileType::FoliagePalm1);
    LoadTile(TileType::FoliagePlanterBox);
    LoadTile(TileType::FoliagePlanterBox2);
    LoadTile(TileType::FoliagePlanterBox3);
    LoadTile(TileType::FoliagePlanterBox4);
    LoadTile(TileType::FoliagePotPlant);
    LoadTile(TileType::FoliageSilverOak);
    LoadTile(TileType::FoliageSilverOakBrown);
    LoadTile(TileType::FoliageTree1);
    LoadTile(TileType::FoliageTree2);
    LoadTile(TileType::FoliageTree3);
    LoadTile(TileType::FoliageYellowTree1);
    LoadTile(TileType::FoliageShrub1);

    // Water tiles
    LoadTile(TileType::Water1);

    // Object tiles
    LoadTile(TileType::ObjectBarStool);
    LoadTile(TileType::ObjectClothesStand);
    LoadTile(TileType::ObjectClothesStand2);
    LoadTile(TileType::ObjectDeskFan);
    LoadTile(TileType::ObjectDeskTop);
    LoadTile(TileType::ObjectDeskTop0);
    LoadTile(TileType::ObjectDeskTop1);
    LoadTile(TileType::ObjectDeskTop2);
    LoadTile(TileType::ObjectDeskTop3);
    LoadTile(TileType::ObjectDeskTop4);
    LoadTile(TileType::ObjectGarbage1);
    LoadTile(TileType::ObjectGarbage2);
    LoadTile(TileType::ObjectGarbage3);
    LoadTile(TileType::ObjectGenerator);
    LoadTile(TileType::ObjectGenerator2);
    LoadTile(TileType::ObjectGenerator3);
    LoadTile(TileType::ObjectPiping1);
    LoadTile(TileType::ObjectPiping3);
    LoadTile(TileType::ObjectPiping4);
    LoadTile(TileType::ObjectShopFront);
    LoadTile(TileType::ObjectShopSolo);

    // Textile tiles
    LoadTile(TileType::TextileBasket);
    LoadTile(TileType::TextileCarpet);
    LoadTile(TileType::TextileFabric1);
    LoadTile(TileType::TextileFabric2);
    LoadTile(TileType::TextileRope1);
    LoadTile(TileType::TextileRope2);

    // FadeOut tiles
    LoadTile(TileType::FadeCornerLargeBL);
    LoadTile(TileType::FadeCornerLargeBR);
    LoadTile(TileType::FadeCornerLargeTL);
    LoadTile(TileType::FadeCornerLargeTR);
    LoadTile(TileType::FadeCornerSmallBL);
    LoadTile(TileType::FadeCornerSmallBR);
    LoadTile(TileType::FadeCornerSmallTL);
    LoadTile(TileType::FadeCornerSmallTR);
    LoadTile(TileType::FadeFlatB);
    LoadTile(TileType::FadeFlatL);
    LoadTile(TileType::FadeFlatR);
    LoadTile(TileType::FadeFlatT);
    LoadTile(TileType::FadeLonelyBlockB);
    LoadTile(TileType::FadeLonelyBlockL);
    LoadTile(TileType::FadeLonelyBlockR);
    LoadTile(TileType::FadeLonelyBlockT);
}

void TileAtlas::LoadTile(TileType type) {
    const char* path = GetTileTexturePath(type);
    if (!path || path[0] == '\0') {
        return;
    }

    TileEntry entry;
    entry.type = type;
    entry.variant = 0;
    entry.name = GetTileDisplayName(type);
    entry.texturePath = m_basePath + path;

    // Load texture
    if (m_textureManager) {
        entry.thumbnail = m_textureManager->Load(entry.texturePath);
    }

    m_tileIndexMap[type] = m_tiles.size();
    m_tiles.push_back(std::move(entry));
}

// ============================================================================
// TilePalette Implementation
// ============================================================================

TilePalette::TilePalette() = default;
TilePalette::~TilePalette() = default;

void TilePalette::Initialize(const TileAtlas& atlas, const Config& config) {
    if (m_initialized) {
        return;
    }

    m_atlas = &atlas;
    m_config = config;
    m_filterDirty = true;
    m_initialized = true;
}

void TilePalette::Render() {
    if (!m_initialized || !m_atlas) {
        return;
    }

    ImGui::SetCursorScreenPos(m_position);

    // Category tabs
    RenderCategoryTabs();

    // Search bar
    RenderSearchBar();

    // Tile grid with scrolling
    RenderTileGrid();

    // Preview panel for selected tile
    RenderPreviewPanel();
}

void TilePalette::Update(float deltaTime) {
    // Update animations
    if (m_hoveredTile != TileType::Empty) {
        m_hoverAnimTime = std::min(m_hoverAnimTime + deltaTime * 4.0f, 1.0f);
    } else {
        m_hoverAnimTime = std::max(m_hoverAnimTime - deltaTime * 4.0f, 0.0f);
    }

    // Update filter if dirty
    if (m_filterDirty) {
        m_filteredTiles = GetVisibleTiles();
        m_filterDirty = false;

        // Calculate max scroll
        int numRows = (static_cast<int>(m_filteredTiles.size()) + m_config.tilesPerRow - 1) / m_config.tilesPerRow;
        float contentHeight = numRows * (m_config.thumbnailSize + m_config.padding);
        float viewHeight = m_size.y - 100.0f; // Account for tabs and search
        m_maxScroll = std::max(0.0f, contentHeight - viewHeight);
    }
}

void TilePalette::SetCategory(Category cat) {
    if (m_currentCategory != cat) {
        m_currentCategory = cat;
        m_filterDirty = true;
        m_scrollOffset = 0.0f;

        if (OnCategoryChanged) {
            OnCategoryChanged(cat);
        }
    }
}

const char* TilePalette::GetCategoryName(Category cat) noexcept {
    switch (cat) {
        case Category::All: return "All";
        case Category::Ground: return "Ground";
        case Category::Concrete: return "Concrete";
        case Category::Bricks: return "Bricks";
        case Category::Wood: return "Wood";
        case Category::Stone: return "Stone";
        case Category::Metal: return "Metal";
        case Category::Foliage: return "Foliage";
        case Category::Water: return "Water";
        case Category::Objects: return "Objects";
        case Category::Textiles: return "Textiles";
        case Category::FadeOut: return "Transitions";
        case Category::Favorites: return "Favorites";
        case Category::Recent: return "Recent";
        default: return "Unknown";
    }
}

int TilePalette::GetTileCount(Category cat) const {
    if (!m_atlas) return 0;

    if (cat == Category::Favorites) {
        return static_cast<int>(m_favorites.size());
    }
    if (cat == Category::Recent) {
        return static_cast<int>(m_recentTiles.size());
    }
    if (cat == Category::All) {
        return static_cast<int>(m_atlas->GetAllTiles().size());
    }

    int catIndex = static_cast<int>(cat);
    return static_cast<int>(m_atlas->GetTilesByCategory(catIndex).size());
}

void TilePalette::SetSelectedTile(TileType type, uint8_t variant) {
    m_selectedTile = type;
    m_selectedVariant = variant;
    m_selectAnimTime = 0.0f;

    // Add to recent
    AddToRecent(type);
}

const TileEntry* TilePalette::GetSelectedEntry() const {
    if (!m_atlas) return nullptr;
    return m_atlas->GetTileEntry(m_selectedTile);
}

bool TilePalette::OnClick(const glm::vec2& screenPos) {
    if (!m_initialized) {
        return false;
    }

    // Check if click is within palette bounds
    if (screenPos.x < m_position.x || screenPos.x > m_position.x + m_size.x ||
        screenPos.y < m_position.y || screenPos.y > m_position.y + m_size.y) {
        return false;
    }

    int index = GetTileIndexAtPosition(screenPos);
    if (index >= 0 && index < static_cast<int>(m_filteredTiles.size())) {
        const auto* entry = m_filteredTiles[index];
        SetSelectedTile(entry->type, entry->variant);

        if (OnTileSelected) {
            OnTileSelected(entry->type, entry->variant);
        }
        return true;
    }

    return false;
}

void TilePalette::OnMouseMove(const glm::vec2& screenPos) {
    if (!m_initialized) {
        return;
    }

    int index = GetTileIndexAtPosition(screenPos);
    if (index >= 0 && index < static_cast<int>(m_filteredTiles.size())) {
        m_hoveredTile = m_filteredTiles[index]->type;
    } else {
        m_hoveredTile = TileType::Empty;
    }
}

void TilePalette::SetFilter(const std::string& filter) {
    if (m_filterText != filter) {
        m_filterText = filter;
        m_filterDirty = true;
        m_scrollOffset = 0.0f;
    }
}

void TilePalette::ClearFilter() {
    SetFilter("");
}

void TilePalette::ToggleFavorite(TileType type) {
    auto it = std::find(m_favorites.begin(), m_favorites.end(), type);
    if (it != m_favorites.end()) {
        m_favorites.erase(it);
    } else {
        m_favorites.push_back(type);
    }

    // Update entries
    if (m_atlas) {
        // Note: In a real implementation, we'd modify the entry's isFavorite flag
    }

    m_filterDirty = true;
}

bool TilePalette::IsFavorite(TileType type) const {
    return std::find(m_favorites.begin(), m_favorites.end(), type) != m_favorites.end();
}

std::vector<TileType> TilePalette::GetFavorites() const {
    return m_favorites;
}

void TilePalette::ClearFavorites() {
    m_favorites.clear();
    m_filterDirty = true;
}

void TilePalette::AddToRecent(TileType type) {
    UpdateRecentTile(type);
}

void TilePalette::ClearRecent() {
    m_recentTiles.clear();
    m_filterDirty = true;
}

void TilePalette::SetBounds(const glm::vec2& position, const glm::vec2& size) {
    m_position = position;
    m_size = size;
    m_filterDirty = true;
}

void TilePalette::SetScrollOffset(float offset) {
    m_scrollOffset = std::clamp(offset, 0.0f, m_maxScroll);
}

// ============================================================================
// Private Implementation
// ============================================================================

void TilePalette::RenderCategoryTabs() {
    if (ImGui::BeginTabBar("TileCategoryTabs")) {
        // Main categories
        const Category categories[] = {
            Category::Ground, Category::Concrete, Category::Bricks,
            Category::Wood, Category::Stone, Category::Metal,
            Category::Foliage, Category::Water, Category::Objects
        };

        for (auto cat : categories) {
            if (ImGui::BeginTabItem(GetCategoryName(cat))) {
                SetCategory(cat);
                ImGui::EndTabItem();
            }
        }

        // Additional tabs
        if (ImGui::BeginTabItem("Favorites")) {
            SetCategory(Category::Favorites);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Recent")) {
            SetCategory(Category::Recent);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("All")) {
            SetCategory(Category::All);
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }
}

void TilePalette::RenderSearchBar() {
    static char searchBuffer[256] = "";

    ImGui::Text("Search:");
    ImGui::SameLine();

    if (ImGui::InputText("##TileSearch", searchBuffer, sizeof(searchBuffer))) {
        SetFilter(searchBuffer);
    }

    if (ImGui::Button("Clear")) {
        searchBuffer[0] = '\0';
        ClearFilter();
    }

    ImGui::Separator();
}

void TilePalette::RenderTileGrid() {
    // Calculate grid area
    glm::vec2 gridSize = {
        m_size.x,
        m_size.y - 120.0f // Account for tabs, search, and preview
    };

    ImGui::BeginChild("TileGrid", gridSize, true);

    float tileFullSize = m_config.thumbnailSize + m_config.padding;
    int col = 0;

    for (size_t i = 0; i < m_filteredTiles.size(); ++i) {
        const auto* entry = m_filteredTiles[i];
        if (!entry) continue;

        ImGui::PushID(static_cast<int>(i));

        // Determine if this tile is selected or hovered
        bool isSelected = (entry->type == m_selectedTile);
        bool isHovered = (entry->type == m_hoveredTile);

        // Apply highlight color
        if (isSelected) {
            ImGui::PushStyleColor(0, m_config.selectedColor); // ImGuiCol_Button
        } else if (isHovered) {
            ImGui::PushStyleColor(0, m_config.hoverColor);
        }

        // Render tile button
        void* textureId = entry->thumbnail ? reinterpret_cast<void*>(static_cast<uintptr_t>(entry->thumbnail->GetID())) : nullptr;
        glm::vec2 thumbSize{static_cast<float>(m_config.thumbnailSize), static_cast<float>(m_config.thumbnailSize)};

        if (ImGui::ImageButton("##tile", textureId, thumbSize)) {
            SetSelectedTile(entry->type, entry->variant);
            if (OnTileSelected) {
                OnTileSelected(entry->type, entry->variant);
            }
        }

        // Tooltip on hover
        if (ImGui::IsItemHovered()) {
            RenderTileTooltip(*entry);
        }

        // Right-click for favorite toggle
        if (ImGui::IsItemClicked(1)) {
            ToggleFavorite(entry->type);
        }

        if (isSelected || isHovered) {
            ImGui::PopStyleColor();
        }

        ImGui::PopID();

        // Layout
        ++col;
        if (col < m_config.tilesPerRow) {
            ImGui::SameLine();
        } else {
            col = 0;
        }
    }

    // Update scroll
    m_scrollOffset = ImGui::GetScrollY();
    m_maxScroll = ImGui::GetScrollMaxY();

    ImGui::EndChild();
}

void TilePalette::RenderTileTooltip(const TileEntry& entry) {
    ImGui::BeginTooltip();

    // Show larger preview
    void* textureId = entry.thumbnail ? reinterpret_cast<void*>(static_cast<uintptr_t>(entry.thumbnail->GetID())) : nullptr;
    ImGui::Image(textureId, glm::vec2{128.0f, 128.0f});

    ImGui::Text("%s", entry.name.c_str());

    // Show favorite status
    if (IsFavorite(entry.type)) {
        ImGui::Text("[Favorite]");
    }

    ImGui::Text("Right-click to toggle favorite");

    ImGui::EndTooltip();
}

void TilePalette::RenderPreviewPanel() {
    ImGui::Separator();
    ImGui::Text("Selected: %s", GetTileDisplayName(m_selectedTile));

    const auto* entry = GetSelectedEntry();
    if (entry && entry->thumbnail) {
        void* textureId = reinterpret_cast<void*>(static_cast<uintptr_t>(entry->thumbnail->GetID()));
        ImGui::Image(textureId, glm::vec2{48.0f, 48.0f});
    }
}

std::vector<const TileEntry*> TilePalette::GetVisibleTiles() const {
    std::vector<const TileEntry*> result;

    if (!m_atlas) {
        return result;
    }

    // Get base tile list based on category
    std::vector<const TileEntry*> baseTiles;

    if (m_currentCategory == Category::Favorites) {
        for (TileType type : m_favorites) {
            if (const auto* entry = m_atlas->GetTileEntry(type)) {
                baseTiles.push_back(entry);
            }
        }
    } else if (m_currentCategory == Category::Recent) {
        for (TileType type : m_recentTiles) {
            if (const auto* entry = m_atlas->GetTileEntry(type)) {
                baseTiles.push_back(entry);
            }
        }
    } else if (m_currentCategory == Category::All) {
        for (const auto& entry : m_atlas->GetAllTiles()) {
            baseTiles.push_back(&entry);
        }
    } else {
        int catIndex = static_cast<int>(m_currentCategory);
        baseTiles = m_atlas->GetTilesByCategory(catIndex);
    }

    // Apply filter
    if (m_filterText.empty()) {
        return baseTiles;
    }

    // Case-insensitive filter
    std::string filterLower = m_filterText;
    std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    for (const auto* entry : baseTiles) {
        std::string nameLower = entry->name;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (nameLower.find(filterLower) != std::string::npos) {
            result.push_back(entry);
        }
    }

    return result;
}

int TilePalette::GetTileIndexAtPosition(const glm::vec2& screenPos) const {
    // Calculate grid area start
    glm::vec2 gridStart = m_position + glm::vec2{0.0f, 80.0f}; // After tabs and search

    // Check if in grid area
    if (screenPos.x < gridStart.x || screenPos.y < gridStart.y) {
        return -1;
    }

    float tileFullSize = m_config.thumbnailSize + m_config.padding;
    float relX = screenPos.x - gridStart.x;
    float relY = screenPos.y - gridStart.y + m_scrollOffset;

    int col = static_cast<int>(relX / tileFullSize);
    int row = static_cast<int>(relY / tileFullSize);

    if (col < 0 || col >= m_config.tilesPerRow) {
        return -1;
    }

    int index = row * m_config.tilesPerRow + col;
    return index;
}

glm::vec2 TilePalette::GetTilePosition(int index) const {
    float tileFullSize = m_config.thumbnailSize + m_config.padding;
    int col = index % m_config.tilesPerRow;
    int row = index / m_config.tilesPerRow;

    glm::vec2 gridStart = m_position + glm::vec2{0.0f, 80.0f};
    return gridStart + glm::vec2{col * tileFullSize, row * tileFullSize - m_scrollOffset};
}

void TilePalette::UpdateRecentTile(TileType type) {
    // Remove if already in list
    auto it = std::find(m_recentTiles.begin(), m_recentTiles.end(), type);
    if (it != m_recentTiles.end()) {
        m_recentTiles.erase(it);
    }

    // Add to front
    m_recentTiles.insert(m_recentTiles.begin(), type);

    // Trim to max size
    if (m_recentTiles.size() > static_cast<size_t>(m_config.maxRecentTiles)) {
        m_recentTiles.resize(m_config.maxRecentTiles);
    }

    m_filterDirty = true;
}

} // namespace Vehement
