#include "TileMap.hpp"
#include "../../engine/pathfinding/Graph.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>

// Simple JSON serialization helpers (avoiding external dependencies)
namespace {

std::string EscapeJsonString(const std::string& str) {
    std::string result;
    result.reserve(str.size() * 2);
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

std::string UnescapeJsonString(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            switch (str[i + 1]) {
                case '"': result += '"'; ++i; break;
                case '\\': result += '\\'; ++i; break;
                case 'n': result += '\n'; ++i; break;
                case 'r': result += '\r'; ++i; break;
                case 't': result += '\t'; ++i; break;
                default: result += str[i]; break;
            }
        } else {
            result += str[i];
        }
    }
    return result;
}

// Skip whitespace in JSON parsing
size_t SkipWhitespace(const std::string& json, size_t pos) {
    while (pos < json.size() && std::isspace(json[pos])) {
        ++pos;
    }
    return pos;
}

// Parse integer from JSON
int ParseInt(const std::string& json, size_t& pos) {
    pos = SkipWhitespace(json, pos);
    int sign = 1;
    if (json[pos] == '-') {
        sign = -1;
        ++pos;
    }
    int value = 0;
    while (pos < json.size() && std::isdigit(json[pos])) {
        value = value * 10 + (json[pos] - '0');
        ++pos;
    }
    return sign * value;
}

// Parse float from JSON
float ParseFloat(const std::string& json, size_t& pos) {
    pos = SkipWhitespace(json, pos);
    std::string numStr;
    while (pos < json.size() && (std::isdigit(json[pos]) || json[pos] == '.' ||
           json[pos] == '-' || json[pos] == '+' || json[pos] == 'e' || json[pos] == 'E')) {
        numStr += json[pos++];
    }
    return std::stof(numStr);
}

// Parse boolean from JSON
bool ParseBool(const std::string& json, size_t& pos) {
    pos = SkipWhitespace(json, pos);
    if (json.compare(pos, 4, "true") == 0) {
        pos += 4;
        return true;
    } else if (json.compare(pos, 5, "false") == 0) {
        pos += 5;
        return false;
    }
    return false;
}

// Parse string from JSON
std::string ParseString(const std::string& json, size_t& pos) {
    pos = SkipWhitespace(json, pos);
    if (json[pos] != '"') return "";
    ++pos;
    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            ++pos;
            switch (json[pos]) {
                case '"': result += '"'; break;
                case '\\': result += '\\'; break;
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                default: result += json[pos]; break;
            }
        } else {
            result += json[pos];
        }
        ++pos;
    }
    if (pos < json.size()) ++pos; // Skip closing quote
    return result;
}

// Skip value in JSON (for unknown fields)
void SkipValue(const std::string& json, size_t& pos) {
    pos = SkipWhitespace(json, pos);
    if (pos >= json.size()) return;

    char c = json[pos];
    if (c == '"') {
        ParseString(json, pos);
    } else if (c == '{') {
        int depth = 1;
        ++pos;
        while (pos < json.size() && depth > 0) {
            if (json[pos] == '{') ++depth;
            else if (json[pos] == '}') --depth;
            else if (json[pos] == '"') ParseString(json, pos);
            else ++pos;
        }
    } else if (c == '[') {
        int depth = 1;
        ++pos;
        while (pos < json.size() && depth > 0) {
            if (json[pos] == '[') ++depth;
            else if (json[pos] == ']') --depth;
            else if (json[pos] == '"') ParseString(json, pos);
            else ++pos;
        }
    } else {
        // Number or boolean
        while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != ']') {
            ++pos;
        }
    }
}

} // anonymous namespace

namespace Vehement {

TileMap::TileMap() = default;

TileMap::TileMap(const TileMapConfig& config) {
    Initialize(config.width, config.height, config.tileSize);
    m_useChunks = config.useChunks;

    if (config.defaultTile != TileType::None) {
        Tile defaultTile = Tile::Ground(config.defaultTile);
        Fill(defaultTile);
    }
}

void TileMap::Initialize(int width, int height, float tileSize) {
    m_width = width;
    m_height = height;
    m_tileSize = tileSize;
    m_tiles.resize(width * height);
    m_dirty = true;
}

void TileMap::Clear() {
    m_tiles.clear();
    m_chunks.clear();
    m_width = 0;
    m_height = 0;
    m_dirty = true;
}

void TileMap::Fill(const Tile& tile) {
    for (auto& t : m_tiles) {
        t = tile;
    }
    m_dirty = true;
}

// ========== Tile Access ==========

Tile* TileMap::GetTile(int x, int y) {
    if (!IsInBounds(x, y)) return nullptr;

    if (m_useChunks) {
        TileChunk* chunk = GetChunkForTile(x, y);
        if (!chunk) return nullptr;
        int localX = x % TileChunk::CHUNK_SIZE;
        int localY = y % TileChunk::CHUNK_SIZE;
        return &chunk->GetTile(localX, localY);
    }

    return &m_tiles[y * m_width + x];
}

const Tile* TileMap::GetTile(int x, int y) const {
    if (!IsInBounds(x, y)) return nullptr;

    if (m_useChunks) {
        const TileChunk* chunk = GetChunkForTile(x, y);
        if (!chunk) return nullptr;
        int localX = x % TileChunk::CHUNK_SIZE;
        int localY = y % TileChunk::CHUNK_SIZE;
        return &chunk->GetTile(localX, localY);
    }

    return &m_tiles[y * m_width + x];
}

bool TileMap::SetTile(int x, int y, const Tile& tile) {
    if (!IsInBounds(x, y)) return false;

    if (m_useChunks) {
        TileChunk* chunk = GetChunkForTile(x, y);
        if (!chunk) return false;
        int localX = x % TileChunk::CHUNK_SIZE;
        int localY = y % TileChunk::CHUNK_SIZE;
        chunk->GetTile(localX, localY) = tile;
        chunk->dirty = true;
    } else {
        m_tiles[y * m_width + x] = tile;
    }

    m_dirty = true;
    return true;
}

Tile* TileMap::GetTileAtWorld(float worldX, float worldZ) {
    auto coord = WorldToTile(worldX, worldZ);
    return GetTile(coord.x, coord.y);
}

const Tile* TileMap::GetTileAtWorld(float worldX, float worldZ) const {
    auto coord = WorldToTile(worldX, worldZ);
    return GetTile(coord.x, coord.y);
}

bool TileMap::SetTileAtWorld(float worldX, float worldZ, const Tile& tile) {
    auto coord = WorldToTile(worldX, worldZ);
    return SetTile(coord.x, coord.y, tile);
}

// ========== Coordinate Conversion ==========

glm::ivec2 TileMap::WorldToTile(float worldX, float worldZ) const {
    return glm::ivec2(
        static_cast<int>(std::floor(worldX / m_tileSize)),
        static_cast<int>(std::floor(worldZ / m_tileSize))
    );
}

glm::ivec2 TileMap::WorldToTile(const glm::vec3& worldPos) const {
    return WorldToTile(worldPos.x, worldPos.z);
}

glm::vec3 TileMap::TileToWorld(int tileX, int tileY) const {
    return glm::vec3(
        (tileX + 0.5f) * m_tileSize,
        0.0f,
        (tileY + 0.5f) * m_tileSize
    );
}

glm::vec3 TileMap::TileToWorldCorner(int tileX, int tileY) const {
    return glm::vec3(
        tileX * m_tileSize,
        0.0f,
        tileY * m_tileSize
    );
}

bool TileMap::IsInBounds(int x, int y) const {
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

bool TileMap::IsInBoundsWorld(float worldX, float worldZ) const {
    auto coord = WorldToTile(worldX, worldZ);
    return IsInBounds(coord.x, coord.y);
}

// ========== Serialization ==========

std::string TileMap::SaveToJson() const {
    std::ostringstream json;
    json << "{\n";
    json << "  \"version\": 1,\n";
    json << "  \"width\": " << m_width << ",\n";
    json << "  \"height\": " << m_height << ",\n";
    json << "  \"tileSize\": " << m_tileSize << ",\n";
    json << "  \"tiles\": [\n";

    bool first = true;
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            const Tile& tile = m_tiles[y * m_width + x];

            if (!first) json << ",\n";
            first = false;

            json << "    {";
            json << "\"t\":" << static_cast<int>(tile.type);

            // Only write non-default values to save space
            if (tile.isWall) {
                json << ",\"w\":1";
                json << ",\"wh\":" << tile.wallHeight;
                if (tile.wallSideTexture != TileType::None) {
                    json << ",\"ws\":" << static_cast<int>(tile.wallSideTexture);
                }
                if (tile.wallTopTexture != TileType::None) {
                    json << ",\"wt\":" << static_cast<int>(tile.wallTopTexture);
                }
            }
            if (!tile.isWalkable) json << ",\"nw\":1";
            if (tile.blocksSight) json << ",\"bs\":1";
            if (tile.isDamaging) {
                json << ",\"dmg\":1";
                json << ",\"dps\":" << tile.damagePerSecond;
            }
            if (tile.movementCost != 1.0f) json << ",\"mc\":" << tile.movementCost;
            if (tile.textureVariant != 0) json << ",\"tv\":" << static_cast<int>(tile.textureVariant);
            if (tile.animation != TileAnimation::None) {
                json << ",\"an\":" << static_cast<int>(tile.animation);
            }
            if (tile.rotation != 0) json << ",\"r\":" << static_cast<int>(tile.rotation);
            if (tile.lightEmission != 0.0f) json << ",\"le\":" << tile.lightEmission;

            json << "}";
        }
    }

    json << "\n  ]\n";
    json << "}\n";

    return json.str();
}

bool TileMap::LoadFromJson(const std::string& json) {
    try {
        size_t pos = 0;
        pos = SkipWhitespace(json, pos);
        if (json[pos] != '{') return false;
        ++pos;

        int version = 0;
        int width = 0;
        int height = 0;
        float tileSize = 1.0f;
        std::vector<Tile> tiles;

        // Parse object
        while (pos < json.size()) {
            pos = SkipWhitespace(json, pos);
            if (json[pos] == '}') break;
            if (json[pos] == ',') { ++pos; continue; }

            std::string key = ParseString(json, pos);
            pos = SkipWhitespace(json, pos);
            if (json[pos] == ':') ++pos;
            pos = SkipWhitespace(json, pos);

            if (key == "version") {
                version = ParseInt(json, pos);
            } else if (key == "width") {
                width = ParseInt(json, pos);
            } else if (key == "height") {
                height = ParseInt(json, pos);
            } else if (key == "tileSize") {
                tileSize = ParseFloat(json, pos);
            } else if (key == "tiles") {
                // Parse tiles array
                if (json[pos] != '[') return false;
                ++pos;

                while (pos < json.size()) {
                    pos = SkipWhitespace(json, pos);
                    if (json[pos] == ']') { ++pos; break; }
                    if (json[pos] == ',') { ++pos; continue; }

                    // Parse tile object
                    if (json[pos] != '{') return false;
                    ++pos;

                    Tile tile;
                    while (pos < json.size()) {
                        pos = SkipWhitespace(json, pos);
                        if (json[pos] == '}') { ++pos; break; }
                        if (json[pos] == ',') { ++pos; continue; }

                        std::string tileKey = ParseString(json, pos);
                        pos = SkipWhitespace(json, pos);
                        if (json[pos] == ':') ++pos;
                        pos = SkipWhitespace(json, pos);

                        if (tileKey == "t") {
                            tile.type = static_cast<TileType>(ParseInt(json, pos));
                        } else if (tileKey == "w") {
                            tile.isWall = ParseInt(json, pos) != 0;
                        } else if (tileKey == "wh") {
                            tile.wallHeight = ParseFloat(json, pos);
                        } else if (tileKey == "ws") {
                            tile.wallSideTexture = static_cast<TileType>(ParseInt(json, pos));
                        } else if (tileKey == "wt") {
                            tile.wallTopTexture = static_cast<TileType>(ParseInt(json, pos));
                        } else if (tileKey == "nw") {
                            tile.isWalkable = ParseInt(json, pos) == 0;
                        } else if (tileKey == "bs") {
                            tile.blocksSight = ParseInt(json, pos) != 0;
                        } else if (tileKey == "dmg") {
                            tile.isDamaging = ParseInt(json, pos) != 0;
                        } else if (tileKey == "dps") {
                            tile.damagePerSecond = ParseFloat(json, pos);
                        } else if (tileKey == "mc") {
                            tile.movementCost = ParseFloat(json, pos);
                        } else if (tileKey == "tv") {
                            tile.textureVariant = static_cast<uint8_t>(ParseInt(json, pos));
                        } else if (tileKey == "an") {
                            tile.animation = static_cast<TileAnimation>(ParseInt(json, pos));
                        } else if (tileKey == "r") {
                            tile.rotation = static_cast<uint8_t>(ParseInt(json, pos));
                        } else if (tileKey == "le") {
                            tile.lightEmission = ParseFloat(json, pos);
                        } else {
                            SkipValue(json, pos);
                        }
                    }

                    tiles.push_back(tile);
                }
            } else {
                SkipValue(json, pos);
            }
        }

        // Validate and apply
        if (width <= 0 || height <= 0) return false;
        if (tiles.size() != static_cast<size_t>(width * height)) return false;

        m_width = width;
        m_height = height;
        m_tileSize = tileSize;
        m_tiles = std::move(tiles);
        m_dirty = true;

        return true;
    } catch (...) {
        return false;
    }
}

bool TileMap::SaveToFile(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file.is_open()) return false;
    file << SaveToJson();
    return file.good();
}

bool TileMap::LoadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) return false;

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return LoadFromJson(buffer.str());
}

// ========== Pathfinding Integration ==========

bool TileMap::IsWalkable(int x, int y) const {
    const Tile* tile = GetTile(x, y);
    if (!tile) return false;
    return tile->isWalkable && !tile->isWall;
}

bool TileMap::IsWalkableWorld(float worldX, float worldZ) const {
    auto coord = WorldToTile(worldX, worldZ);
    return IsWalkable(coord.x, coord.y);
}

float TileMap::GetMovementCost(int x, int y) const {
    const Tile* tile = GetTile(x, y);
    if (!tile) return std::numeric_limits<float>::infinity();
    if (!tile->isWalkable || tile->isWall) return std::numeric_limits<float>::infinity();
    return tile->movementCost;
}

void TileMap::BuildNavigationGraph(Nova::Graph& graph, bool includeDiagonals) const {
    graph.Clear();

    // Add nodes for all walkable tiles
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            if (IsWalkable(x, y)) {
                glm::vec3 worldPos = TileToWorld(x, y);
                int nodeId = graph.AddNode(worldPos, GetMovementCost(x, y));
                // Node ID should match our expected GetNodeId
                (void)nodeId; // Suppress unused warning
            }
        }
    }

    // Add edges between adjacent walkable tiles
    const int dx4[] = {1, 0, -1, 0};
    const int dy4[] = {0, 1, 0, -1};
    const int dx8[] = {1, 1, 0, -1, -1, -1, 0, 1};
    const int dy8[] = {0, 1, 1, 1, 0, -1, -1, -1};

    const int* dx = includeDiagonals ? dx8 : dx4;
    const int* dy = includeDiagonals ? dy8 : dy4;
    const int neighborCount = includeDiagonals ? 8 : 4;

    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            if (!IsWalkable(x, y)) continue;

            int nodeId = GetNodeId(x, y);

            for (int i = 0; i < neighborCount; ++i) {
                int nx = x + dx[i];
                int ny = y + dy[i];

                if (!IsInBounds(nx, ny) || !IsWalkable(nx, ny)) continue;

                // For diagonals, check that we're not cutting corners through walls
                if (includeDiagonals && i >= 4) {
                    int cornerIdx = i - 4;
                    bool blocked = false;

                    // Check adjacent cardinal directions
                    if (!IsWalkable(x + dx[cornerIdx], y) ||
                        !IsWalkable(x, y + dy[cornerIdx])) {
                        blocked = true;
                    }

                    if (blocked) continue;
                }

                int neighborId = GetNodeId(nx, ny);
                float weight = (i >= 4) ? 1.414f : 1.0f; // sqrt(2) for diagonals
                weight *= GetMovementCost(nx, ny);

                graph.AddEdge(nodeId, neighborId, weight);
            }
        }
    }
}

void TileMap::UpdateNavigationGraph(Nova::Graph& graph, int minX, int minY,
                                    int maxX, int maxY, bool includeDiagonals) const {
    // For now, just rebuild the entire graph
    // A more efficient implementation would only update the affected region
    BuildNavigationGraph(graph, includeDiagonals);
}

// ========== Chunk Management ==========

void TileMap::EnableChunks(bool enabled) {
    m_useChunks = enabled;
    if (enabled && !m_tiles.empty()) {
        // Migrate flat storage to chunks
        for (int y = 0; y < m_height; ++y) {
            for (int x = 0; x < m_width; ++x) {
                int chunkX = x / TileChunk::CHUNK_SIZE;
                int chunkY = y / TileChunk::CHUNK_SIZE;
                int64_t key = TileChunk::GetChunkKey(chunkX, chunkY);

                if (m_chunks.find(key) == m_chunks.end()) {
                    auto chunk = std::make_unique<TileChunk>();
                    chunk->chunkX = chunkX;
                    chunk->chunkY = chunkY;
                    chunk->loaded = true;
                    m_chunks[key] = std::move(chunk);
                }

                int localX = x % TileChunk::CHUNK_SIZE;
                int localY = y % TileChunk::CHUNK_SIZE;
                m_chunks[key]->GetTile(localX, localY) = m_tiles[y * m_width + x];
            }
        }
        m_tiles.clear();
    }
}

bool TileMap::LoadChunk(int chunkX, int chunkY) {
    int64_t key = TileChunk::GetChunkKey(chunkX, chunkY);

    if (m_chunks.find(key) != m_chunks.end() && m_chunks[key]->loaded) {
        return true; // Already loaded
    }

    auto chunk = std::make_unique<TileChunk>();
    chunk->chunkX = chunkX;
    chunk->chunkY = chunkY;

    if (m_chunkLoadCallback) {
        if (!m_chunkLoadCallback(chunkX, chunkY, *chunk)) {
            return false;
        }
    }

    chunk->loaded = true;
    m_chunks[key] = std::move(chunk);
    return true;
}

void TileMap::UnloadChunk(int chunkX, int chunkY) {
    int64_t key = TileChunk::GetChunkKey(chunkX, chunkY);
    auto it = m_chunks.find(key);

    if (it != m_chunks.end()) {
        if (m_chunkSaveCallback && it->second->dirty) {
            m_chunkSaveCallback(chunkX, chunkY, *it->second);
        }
        m_chunks.erase(it);
    }
}

bool TileMap::IsChunkLoaded(int chunkX, int chunkY) const {
    int64_t key = TileChunk::GetChunkKey(chunkX, chunkY);
    auto it = m_chunks.find(key);
    return it != m_chunks.end() && it->second->loaded;
}

std::vector<glm::ivec2> TileMap::GetLoadedChunks() const {
    std::vector<glm::ivec2> result;
    result.reserve(m_chunks.size());
    for (const auto& [key, chunk] : m_chunks) {
        if (chunk->loaded) {
            result.emplace_back(chunk->chunkX, chunk->chunkY);
        }
    }
    return result;
}

TileChunk* TileMap::GetChunkForTile(int x, int y) {
    int chunkX = x / TileChunk::CHUNK_SIZE;
    int chunkY = y / TileChunk::CHUNK_SIZE;
    int64_t key = TileChunk::GetChunkKey(chunkX, chunkY);

    auto it = m_chunks.find(key);
    if (it == m_chunks.end()) {
        if (!LoadChunk(chunkX, chunkY)) {
            return nullptr;
        }
        it = m_chunks.find(key);
    }

    return it->second.get();
}

const TileChunk* TileMap::GetChunkForTile(int x, int y) const {
    int chunkX = x / TileChunk::CHUNK_SIZE;
    int chunkY = y / TileChunk::CHUNK_SIZE;
    int64_t key = TileChunk::GetChunkKey(chunkX, chunkY);

    auto it = m_chunks.find(key);
    if (it == m_chunks.end()) {
        return nullptr;
    }

    return it->second.get();
}

std::vector<std::pair<glm::ivec2, const Tile*>> TileMap::GetWallTiles() const {
    std::vector<std::pair<glm::ivec2, const Tile*>> walls;

    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            const Tile* tile = GetTile(x, y);
            if (tile && tile->isWall) {
                walls.emplace_back(glm::ivec2(x, y), tile);
            }
        }
    }

    return walls;
}

void TileMap::MarkDirty(int x, int y, int width, int height) {
    m_dirty = true;

    if (m_useChunks) {
        // Mark affected chunks as dirty
        int minChunkX = x / TileChunk::CHUNK_SIZE;
        int minChunkY = y / TileChunk::CHUNK_SIZE;
        int maxChunkX = (x + width - 1) / TileChunk::CHUNK_SIZE;
        int maxChunkY = (y + height - 1) / TileChunk::CHUNK_SIZE;

        for (int cy = minChunkY; cy <= maxChunkY; ++cy) {
            for (int cx = minChunkX; cx <= maxChunkX; ++cx) {
                int64_t key = TileChunk::GetChunkKey(cx, cy);
                auto it = m_chunks.find(key);
                if (it != m_chunks.end()) {
                    it->second->dirty = true;
                }
            }
        }
    }
}

} // namespace Vehement
