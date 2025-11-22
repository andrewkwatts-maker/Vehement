#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <deque>
#include <functional>
#include <cstdint>
#include <string>

namespace Vehement {

// Forward declarations
class TileMap;
class EntityManager;

/**
 * @brief Tile type enumeration matching Vehement2 texture categories
 */
enum class TileType : uint16_t {
    // Empty/None
    Empty = 0,

    // Ground tiles (0x0100 - 0x01FF)
    GroundDirt = 0x0100,
    GroundForrest1,
    GroundForrest2,
    GroundGrass1,
    GroundGrass2,
    GroundRocks,

    // Concrete tiles (0x0200 - 0x02FF)
    ConcreteAsphalt1 = 0x0200,
    ConcreteAsphalt2,
    ConcreteAsphalt2Steps1,
    ConcreteAsphalt2Steps2,
    ConcreteBlocks1,
    ConcreteBlocks2,
    ConcretePad,
    ConcreteTiles1,
    ConcreteTiles2,

    // Brick tiles (0x0300 - 0x03FF)
    BricksBlack = 0x0300,
    BricksGrey,
    BricksRock,
    BricksRockFrontBot,
    BricksRockFrontLhs,
    BricksRockFrontRhs,
    BricksRockFrontTop,
    BricksStacked,
    // Brick corners
    BricksCornerBL,
    BricksCornerBLRI,
    BricksCornerBLRO,
    BricksCornerBR,
    BricksCornerBRRI,
    BricksCornerBRRO,
    BricksCornerTL,
    BricksCornerTLRI,
    BricksCornerTLRO,
    BricksCornerTR,
    BricksCornerTRRI,
    BricksCornerTRRO,

    // Wood tiles (0x0400 - 0x04FF)
    Wood1 = 0x0400,
    WoodCrate1,
    WoodCrate2,
    WoodFlooring1,
    WoodFlooring2,

    // Stone tiles (0x0500 - 0x05FF)
    StoneBlack = 0x0500,
    StoneMarble1,
    StoneMarble2,
    StoneRaw,

    // Metal tiles (0x0600 - 0x06FF)
    Metal1 = 0x0600,
    Metal2,
    Metal3,
    Metal4,
    MetalTile1,
    MetalTile2,
    MetalTile3,
    MetalTile4,
    MetalShopFront,
    MetalShopFrontB,
    MetalShopFrontL,
    MetalShopFrontR,
    MetalShopFrontT,

    // Foliage tiles (0x0700 - 0x07FF)
    FoliageBonsai = 0x0700,
    FoliageBottleBrush,
    FoliageCherryTree,
    FoliagePalm1,
    FoliagePlanterBox,
    FoliagePlanterBox2,
    FoliagePlanterBox3,
    FoliagePlanterBox4,
    FoliagePotPlant,
    FoliageSilverOak,
    FoliageSilverOakBrown,
    FoliageTree1,
    FoliageTree2,
    FoliageTree3,
    FoliageYellowTree1,
    FoliageShrub1,

    // Water tiles (0x0800 - 0x08FF)
    Water1 = 0x0800,

    // Object tiles (0x0900 - 0x09FF)
    ObjectBarStool = 0x0900,
    ObjectClothesStand,
    ObjectClothesStand2,
    ObjectDeskFan,
    ObjectDeskTop,
    ObjectDeskTop0,
    ObjectDeskTop1,
    ObjectDeskTop2,
    ObjectDeskTop3,
    ObjectDeskTop4,
    ObjectGarbage1,
    ObjectGarbage2,
    ObjectGarbage3,
    ObjectGenerator,
    ObjectGenerator2,
    ObjectGenerator3,
    ObjectPiping1,
    ObjectPiping3,
    ObjectPiping4,
    ObjectShopFront,
    ObjectShopSolo,

    // Textile tiles (0x0A00 - 0x0AFF)
    TextileBasket = 0x0A00,
    TextileCarpet,
    TextileFabric1,
    TextileFabric2,
    TextileRope1,
    TextileRope2,

    // FadeOut tiles (0x0B00 - 0x0BFF)
    FadeCornerLargeBL = 0x0B00,
    FadeCornerLargeBR,
    FadeCornerLargeTL,
    FadeCornerLargeTR,
    FadeCornerSmallBL,
    FadeCornerSmallBR,
    FadeCornerSmallTL,
    FadeCornerSmallTR,
    FadeFlatB,
    FadeFlatL,
    FadeFlatR,
    FadeFlatT,
    FadeLonelyBlockB,
    FadeLonelyBlockL,
    FadeLonelyBlockR,
    FadeLonelyBlockT,

    MaxTileType
};

/**
 * @brief Structure representing a single tile change for undo/redo and network sync
 */
struct TileChange {
    int x = 0;
    int y = 0;
    TileType oldType = TileType::Empty;
    TileType newType = TileType::Empty;
    uint8_t oldVariant = 0;
    uint8_t newVariant = 0;
    bool wasWall = false;
    bool isWall = false;
    float oldWallHeight = 0.0f;
    float newWallHeight = 0.0f;
    uint64_t timestamp = 0;
};

/**
 * @brief Level Editor for Vehement2
 *
 * Allows players to modify their town when no zombies are nearby.
 * Uses existing 2D textures from the Vehement2/images/ folder to build levels.
 *
 * Features:
 * - Multiple editing tools (select, paint, erase, fill, rectangle, eyedropper)
 * - Undo/redo support with configurable history depth
 * - Zombie proximity checking to enable/disable editing
 * - Brush size control for painting
 * - Wall mode for creating 3D structures
 * - Network sync support via pending changes
 */
class LevelEditor {
public:
    /**
     * @brief Editor tool types
     */
    enum class Tool {
        Select,      ///< Select and move tiles
        Paint,       ///< Paint tiles onto the map
        Erase,       ///< Remove tiles from the map
        Fill,        ///< Fill an area with tiles (flood fill)
        Rectangle,   ///< Draw a rectangle of tiles
        Eyedropper   ///< Pick tile type from the map
    };

    /**
     * @brief Configuration for the editor
     */
    struct Config {
        float safeRadius = 20.0f;           ///< Minimum distance from zombies to edit
        int maxUndoHistory = 100;           ///< Maximum undo history entries
        int maxBrushSize = 10;              ///< Maximum brush size
        float defaultWallHeight = 2.0f;     ///< Default wall height in units
        int coinCostPerTile = 10;           ///< Base cost per tile placed
    };

public:
    LevelEditor();
    ~LevelEditor();

    // Delete copy, allow move
    LevelEditor(const LevelEditor&) = delete;
    LevelEditor& operator=(const LevelEditor&) = delete;
    LevelEditor(LevelEditor&&) noexcept = default;
    LevelEditor& operator=(LevelEditor&&) noexcept = default;

    /**
     * @brief Initialize the level editor
     * @param config Optional configuration parameters
     */
    void Initialize(const Config& config = {});

    /**
     * @brief Shutdown and cleanup resources
     */
    void Shutdown();

    /**
     * @brief Check if the editor is initialized
     */
    [[nodiscard]] bool IsInitialized() const noexcept { return m_initialized; }

    // -------------------------------------------------------------------------
    // Enable/Disable Editor Mode
    // -------------------------------------------------------------------------

    /**
     * @brief Enable or disable editor mode
     * @param enabled Whether to enable the editor
     */
    void SetEnabled(bool enabled);

    /**
     * @brief Check if the editor is currently enabled
     */
    [[nodiscard]] bool IsEnabled() const noexcept { return m_enabled; }

    /**
     * @brief Check if editing is allowed (no zombies nearby)
     * @param playerPos The player's current position
     * @param entities The entity manager to check for zombies
     * @return true if editing is allowed
     */
    [[nodiscard]] bool CanEdit(const glm::vec2& playerPos, const EntityManager& entities) const;

    // -------------------------------------------------------------------------
    // Tool Selection
    // -------------------------------------------------------------------------

    /**
     * @brief Set the current editing tool
     * @param tool The tool to use
     */
    void SetTool(Tool tool);

    /**
     * @brief Get the current editing tool
     */
    [[nodiscard]] Tool GetTool() const noexcept { return m_currentTool; }

    /**
     * @brief Get tool name as string for UI
     */
    [[nodiscard]] static const char* GetToolName(Tool tool) noexcept;

    // -------------------------------------------------------------------------
    // Tile Selection
    // -------------------------------------------------------------------------

    /**
     * @brief Set the selected tile type for painting
     * @param type The tile type to paint
     * @param variant Optional variant index (for tiles with multiple variants)
     */
    void SetSelectedTile(TileType type, uint8_t variant = 0);

    /**
     * @brief Get the currently selected tile type
     */
    [[nodiscard]] TileType GetSelectedTile() const noexcept { return m_selectedTile; }

    /**
     * @brief Get the currently selected tile variant
     */
    [[nodiscard]] uint8_t GetSelectedVariant() const noexcept { return m_selectedVariant; }

    // -------------------------------------------------------------------------
    // Brush Settings
    // -------------------------------------------------------------------------

    /**
     * @brief Set the brush size for painting
     * @param size Brush size (1-10, clamped to valid range)
     */
    void SetBrushSize(int size);

    /**
     * @brief Get the current brush size
     */
    [[nodiscard]] int GetBrushSize() const noexcept { return m_brushSize; }

    /**
     * @brief Enable or disable wall mode
     * @param isWall Whether tiles should be placed as walls
     */
    void SetWallMode(bool isWall);

    /**
     * @brief Check if wall mode is enabled
     */
    [[nodiscard]] bool IsWallMode() const noexcept { return m_wallMode; }

    /**
     * @brief Set the wall height for wall mode
     * @param height Wall height in world units
     */
    void SetWallHeight(float height);

    /**
     * @brief Get the current wall height
     */
    [[nodiscard]] float GetWallHeight() const noexcept { return m_wallHeight; }

    // -------------------------------------------------------------------------
    // Input Handling
    // -------------------------------------------------------------------------

    /**
     * @brief Handle mouse button press
     * @param worldPos Position in world coordinates
     * @param button Mouse button (0 = left, 1 = right, 2 = middle)
     */
    void OnMouseDown(const glm::vec2& worldPos, int button);

    /**
     * @brief Handle mouse button release
     * @param worldPos Position in world coordinates
     * @param button Mouse button
     */
    void OnMouseUp(const glm::vec2& worldPos, int button);

    /**
     * @brief Handle mouse movement
     * @param worldPos Position in world coordinates
     */
    void OnMouseMove(const glm::vec2& worldPos);

    /**
     * @brief Handle key press
     * @param key Key code (GLFW key codes)
     */
    void OnKeyPress(int key);

    /**
     * @brief Check if the editor is currently processing an operation
     */
    [[nodiscard]] bool IsOperationInProgress() const noexcept { return m_isDrawing; }

    // -------------------------------------------------------------------------
    // Undo/Redo
    // -------------------------------------------------------------------------

    /**
     * @brief Undo the last operation
     */
    void Undo();

    /**
     * @brief Redo the last undone operation
     */
    void Redo();

    /**
     * @brief Check if undo is available
     */
    [[nodiscard]] bool CanUndo() const noexcept { return !m_undoStack.empty(); }

    /**
     * @brief Check if redo is available
     */
    [[nodiscard]] bool CanRedo() const noexcept { return !m_redoStack.empty(); }

    /**
     * @brief Get the number of available undo operations
     */
    [[nodiscard]] size_t GetUndoCount() const noexcept { return m_undoStack.size(); }

    /**
     * @brief Get the number of available redo operations
     */
    [[nodiscard]] size_t GetRedoCount() const noexcept { return m_redoStack.size(); }

    /**
     * @brief Clear undo/redo history
     */
    void ClearHistory();

    // -------------------------------------------------------------------------
    // Apply Changes
    // -------------------------------------------------------------------------

    /**
     * @brief Apply all pending changes to the tile map
     * @param map The tile map to modify
     */
    void ApplyChanges(TileMap& map);

    /**
     * @brief Get pending changes for network synchronization
     */
    [[nodiscard]] const std::vector<TileChange>& GetPendingChanges() const noexcept {
        return m_pendingChanges;
    }

    /**
     * @brief Clear pending changes (after they've been synced)
     */
    void ClearPendingChanges();

    /**
     * @brief Get the total cost of pending changes in coins
     */
    [[nodiscard]] int GetPendingCost() const noexcept;

    // -------------------------------------------------------------------------
    // Preview
    // -------------------------------------------------------------------------

    /**
     * @brief Get the current preview position
     */
    [[nodiscard]] const glm::vec2& GetPreviewPosition() const noexcept { return m_previewPos; }

    /**
     * @brief Check if preview is valid
     */
    [[nodiscard]] bool HasValidPreview() const noexcept { return m_hasPreview; }

    /**
     * @brief Get rectangle selection start position
     */
    [[nodiscard]] const glm::vec2& GetRectangleStart() const noexcept { return m_rectStart; }

    /**
     * @brief Get rectangle selection end position
     */
    [[nodiscard]] const glm::vec2& GetRectangleEnd() const noexcept { return m_rectEnd; }

    // -------------------------------------------------------------------------
    // Callbacks
    // -------------------------------------------------------------------------

    /**
     * @brief Callback for when a tile is picked via eyedropper
     */
    std::function<void(TileType, uint8_t)> OnTilePicked;

    /**
     * @brief Callback for when the tool changes
     */
    std::function<void(Tool)> OnToolChanged;

    /**
     * @brief Callback for when changes are applied
     */
    std::function<void(const std::vector<TileChange>&)> OnChangesApplied;

private:
    // Tool operation helpers
    void BeginPaint(const glm::vec2& worldPos);
    void ContinuePaint(const glm::vec2& worldPos);
    void EndPaint();

    void BeginRectangle(const glm::vec2& worldPos);
    void UpdateRectangle(const glm::vec2& worldPos);
    void EndRectangle();

    void DoFill(const glm::vec2& worldPos);
    void DoEyedrop(const glm::vec2& worldPos);
    void DoSelect(const glm::vec2& worldPos);

    // Painting helpers
    void PaintTile(int x, int y);
    void PaintBrush(int centerX, int centerY);
    void EraseTile(int x, int y);
    std::vector<std::pair<int, int>> GetBrushTiles(int centerX, int centerY) const;

    // Flood fill helper
    void FloodFill(int startX, int startY, TileType targetType, TileType fillType);

    // Convert world position to tile coordinates
    glm::ivec2 WorldToTile(const glm::vec2& worldPos) const;

    // Add change to pending and undo stack
    void RecordChange(const TileChange& change);

    // Limit undo history size
    void TrimUndoHistory();

    // Current timestamp for changes
    uint64_t GetTimestamp() const;

private:
    bool m_initialized = false;
    bool m_enabled = false;

    // Configuration
    Config m_config;

    // Current tool state
    Tool m_currentTool = Tool::Paint;
    TileType m_selectedTile = TileType::GroundGrass1;
    uint8_t m_selectedVariant = 0;

    // Brush settings
    int m_brushSize = 1;
    bool m_wallMode = false;
    float m_wallHeight = 2.0f;

    // Drawing state
    bool m_isDrawing = false;
    glm::vec2 m_lastPaintPos{0.0f};
    glm::vec2 m_rectStart{0.0f};
    glm::vec2 m_rectEnd{0.0f};

    // Preview
    glm::vec2 m_previewPos{0.0f};
    bool m_hasPreview = false;

    // Undo/Redo stacks
    std::deque<std::vector<TileChange>> m_undoStack;
    std::deque<std::vector<TileChange>> m_redoStack;

    // Current operation changes (batched until mouse up)
    std::vector<TileChange> m_currentOperationChanges;

    // Pending changes for network sync
    std::vector<TileChange> m_pendingChanges;

    // Tile size for coordinate conversion
    float m_tileSize = 1.0f;

    // Reference to current map for eyedropper (set during operations)
    TileMap* m_currentMap = nullptr;
};

/**
 * @brief Get texture path for a tile type
 * @param type The tile type
 * @return Path relative to Vehement2/images/
 */
[[nodiscard]] const char* GetTileTexturePath(TileType type);

/**
 * @brief Get the category for a tile type
 * @param type The tile type
 * @return Category index (0-9)
 */
[[nodiscard]] int GetTileCategory(TileType type);

/**
 * @brief Get the display name for a tile type
 * @param type The tile type
 * @return Human-readable name
 */
[[nodiscard]] const char* GetTileDisplayName(TileType type);

} // namespace Vehement
