#include "LocationDefinition.hpp"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cstring>

namespace Vehement {

// Static member initialization
LocationDefinition::LocationId LocationDefinition::s_nextId = 1;

// =============================================================================
// Constructor / Destructor
// =============================================================================

LocationDefinition::LocationDefinition()
    : m_id(s_nextId++) {
}

LocationDefinition::LocationDefinition(const std::string& name)
    : m_id(s_nextId++)
    , m_name(name) {
}

// =============================================================================
// Tags
// =============================================================================

void LocationDefinition::AddTag(const std::string& tag) {
    if (!HasTag(tag)) {
        m_tags.push_back(tag);
    }
}

bool LocationDefinition::RemoveTag(const std::string& tag) {
    auto it = std::find(m_tags.begin(), m_tags.end(), tag);
    if (it != m_tags.end()) {
        m_tags.erase(it);
        return true;
    }
    return false;
}

bool LocationDefinition::HasTag(const std::string& tag) const {
    return std::find(m_tags.begin(), m_tags.end(), tag) != m_tags.end();
}

// =============================================================================
// Version Tracking
// =============================================================================

void LocationDefinition::MarkEdited(const std::string& author, const std::string& description) {
    m_version.IncrementMinor();
    m_version.timestamp = static_cast<uint64_t>(
        std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
    );

    if (!author.empty()) {
        m_version.author = author;
    }
    if (!description.empty()) {
        m_version.changeDescription = description;
    }
}

// =============================================================================
// JSON Serialization - Simple implementation without external dependencies
// =============================================================================

namespace {

std::string EscapeJsonString(const std::string& str) {
    std::string result;
    result.reserve(str.size() + 10);
    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\b': result += "\\b";  break;
            case '\f': result += "\\f";  break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            default:   result += c;      break;
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
                case '"':  result += '"';  ++i; break;
                case '\\': result += '\\'; ++i; break;
                case 'b':  result += '\b'; ++i; break;
                case 'f':  result += '\f'; ++i; break;
                case 'n':  result += '\n'; ++i; break;
                case 'r':  result += '\r'; ++i; break;
                case 't':  result += '\t'; ++i; break;
                default:   result += str[i];    break;
            }
        } else {
            result += str[i];
        }
    }
    return result;
}

std::string Indent(int level, bool pretty) {
    if (!pretty) return "";
    return std::string(level * 2, ' ');
}

std::string Newline(bool pretty) {
    return pretty ? "\n" : "";
}

} // anonymous namespace

std::string LocationDefinition::ToJson(bool pretty) const {
    std::ostringstream ss;

    ss << "{" << Newline(pretty);

    // Identity
    ss << Indent(1, pretty) << "\"id\": " << m_id << "," << Newline(pretty);
    ss << Indent(1, pretty) << "\"name\": \"" << EscapeJsonString(m_name) << "\"," << Newline(pretty);
    ss << Indent(1, pretty) << "\"description\": \"" << EscapeJsonString(m_description) << "\"," << Newline(pretty);

    // World bounds
    ss << Indent(1, pretty) << "\"worldBounds\": {" << Newline(pretty);
    ss << Indent(2, pretty) << "\"minX\": " << m_worldBounds.min.x << "," << Newline(pretty);
    ss << Indent(2, pretty) << "\"minY\": " << m_worldBounds.min.y << "," << Newline(pretty);
    ss << Indent(2, pretty) << "\"minZ\": " << m_worldBounds.min.z << "," << Newline(pretty);
    ss << Indent(2, pretty) << "\"maxX\": " << m_worldBounds.max.x << "," << Newline(pretty);
    ss << Indent(2, pretty) << "\"maxY\": " << m_worldBounds.max.y << "," << Newline(pretty);
    ss << Indent(2, pretty) << "\"maxZ\": " << m_worldBounds.max.z << Newline(pretty);
    ss << Indent(1, pretty) << "}," << Newline(pretty);

    // Geo bounds
    ss << Indent(1, pretty) << "\"geoBounds\": {" << Newline(pretty);
    ss << Indent(2, pretty) << "\"minLatitude\": " << m_geoBounds.minLatitude << "," << Newline(pretty);
    ss << Indent(2, pretty) << "\"maxLatitude\": " << m_geoBounds.maxLatitude << "," << Newline(pretty);
    ss << Indent(2, pretty) << "\"minLongitude\": " << m_geoBounds.minLongitude << "," << Newline(pretty);
    ss << Indent(2, pretty) << "\"maxLongitude\": " << m_geoBounds.maxLongitude << Newline(pretty);
    ss << Indent(1, pretty) << "}," << Newline(pretty);

    // Tags
    ss << Indent(1, pretty) << "\"tags\": [";
    for (size_t i = 0; i < m_tags.size(); ++i) {
        if (i > 0) ss << ", ";
        ss << "\"" << EscapeJsonString(m_tags[i]) << "\"";
    }
    ss << "]," << Newline(pretty);

    // PCG settings
    ss << Indent(1, pretty) << "\"pcgPriority\": " << static_cast<int>(m_pcgPriority) << "," << Newline(pretty);
    ss << Indent(1, pretty) << "\"blendRadius\": " << m_blendRadius << "," << Newline(pretty);

    // Version
    ss << Indent(1, pretty) << "\"version\": {" << Newline(pretty);
    ss << Indent(2, pretty) << "\"major\": " << m_version.major << "," << Newline(pretty);
    ss << Indent(2, pretty) << "\"minor\": " << m_version.minor << "," << Newline(pretty);
    ss << Indent(2, pretty) << "\"timestamp\": " << m_version.timestamp << "," << Newline(pretty);
    ss << Indent(2, pretty) << "\"author\": \"" << EscapeJsonString(m_version.author) << "\"," << Newline(pretty);
    ss << Indent(2, pretty) << "\"changeDescription\": \"" << EscapeJsonString(m_version.changeDescription) << "\"" << Newline(pretty);
    ss << Indent(1, pretty) << "}," << Newline(pretty);

    // Metadata
    ss << Indent(1, pretty) << "\"presetName\": \"" << EscapeJsonString(m_presetName) << "\"," << Newline(pretty);
    ss << Indent(1, pretty) << "\"category\": \"" << EscapeJsonString(m_category) << "\"," << Newline(pretty);
    ss << Indent(1, pretty) << "\"enabled\": " << (m_enabled ? "true" : "false") << Newline(pretty);

    ss << "}";

    return ss.str();
}

// Simple JSON parser helpers
namespace {

void SkipWhitespace(const std::string& json, size_t& pos) {
    while (pos < json.size() && std::isspace(json[pos])) {
        ++pos;
    }
}

bool ExpectChar(const std::string& json, size_t& pos, char c) {
    SkipWhitespace(json, pos);
    if (pos < json.size() && json[pos] == c) {
        ++pos;
        return true;
    }
    return false;
}

std::string ParseString(const std::string& json, size_t& pos) {
    SkipWhitespace(json, pos);
    if (pos >= json.size() || json[pos] != '"') return "";

    ++pos;
    std::string result;
    while (pos < json.size() && json[pos] != '"') {
        if (json[pos] == '\\' && pos + 1 < json.size()) {
            result += json[pos];
            ++pos;
        }
        result += json[pos];
        ++pos;
    }
    if (pos < json.size()) ++pos; // Skip closing quote

    return UnescapeJsonString(result);
}

double ParseNumber(const std::string& json, size_t& pos) {
    SkipWhitespace(json, pos);
    size_t start = pos;
    while (pos < json.size() &&
           (std::isdigit(json[pos]) || json[pos] == '.' ||
            json[pos] == '-' || json[pos] == '+' ||
            json[pos] == 'e' || json[pos] == 'E')) {
        ++pos;
    }
    if (start == pos) return 0.0;
    return std::stod(json.substr(start, pos - start));
}

bool ParseBool(const std::string& json, size_t& pos) {
    SkipWhitespace(json, pos);
    if (json.substr(pos, 4) == "true") {
        pos += 4;
        return true;
    }
    if (json.substr(pos, 5) == "false") {
        pos += 5;
        return false;
    }
    return false;
}

std::string ParseKey(const std::string& json, size_t& pos) {
    std::string key = ParseString(json, pos);
    SkipWhitespace(json, pos);
    if (pos < json.size() && json[pos] == ':') {
        ++pos;
    }
    return key;
}

} // anonymous namespace

bool LocationDefinition::FromJson(const std::string& json) {
    size_t pos = 0;

    if (!ExpectChar(json, pos, '{')) {
        return false;
    }

    while (pos < json.size()) {
        SkipWhitespace(json, pos);
        if (json[pos] == '}') break;

        std::string key = ParseKey(json, pos);

        if (key == "id") {
            m_id = static_cast<LocationId>(ParseNumber(json, pos));
        }
        else if (key == "name") {
            m_name = ParseString(json, pos);
        }
        else if (key == "description") {
            m_description = ParseString(json, pos);
        }
        else if (key == "worldBounds") {
            ExpectChar(json, pos, '{');
            while (pos < json.size() && json[pos] != '}') {
                std::string bkey = ParseKey(json, pos);
                float val = static_cast<float>(ParseNumber(json, pos));

                if (bkey == "minX") m_worldBounds.min.x = val;
                else if (bkey == "minY") m_worldBounds.min.y = val;
                else if (bkey == "minZ") m_worldBounds.min.z = val;
                else if (bkey == "maxX") m_worldBounds.max.x = val;
                else if (bkey == "maxY") m_worldBounds.max.y = val;
                else if (bkey == "maxZ") m_worldBounds.max.z = val;

                SkipWhitespace(json, pos);
                if (json[pos] == ',') ++pos;
            }
            ExpectChar(json, pos, '}');
        }
        else if (key == "geoBounds") {
            ExpectChar(json, pos, '{');
            while (pos < json.size() && json[pos] != '}') {
                std::string bkey = ParseKey(json, pos);
                double val = ParseNumber(json, pos);

                if (bkey == "minLatitude") m_geoBounds.minLatitude = val;
                else if (bkey == "maxLatitude") m_geoBounds.maxLatitude = val;
                else if (bkey == "minLongitude") m_geoBounds.minLongitude = val;
                else if (bkey == "maxLongitude") m_geoBounds.maxLongitude = val;

                SkipWhitespace(json, pos);
                if (json[pos] == ',') ++pos;
            }
            ExpectChar(json, pos, '}');
        }
        else if (key == "tags") {
            m_tags.clear();
            ExpectChar(json, pos, '[');
            while (pos < json.size() && json[pos] != ']') {
                SkipWhitespace(json, pos);
                if (json[pos] == '"') {
                    m_tags.push_back(ParseString(json, pos));
                }
                SkipWhitespace(json, pos);
                if (json[pos] == ',') ++pos;
            }
            ExpectChar(json, pos, ']');
        }
        else if (key == "pcgPriority") {
            m_pcgPriority = static_cast<PCGPriority>(static_cast<int>(ParseNumber(json, pos)));
        }
        else if (key == "blendRadius") {
            m_blendRadius = static_cast<float>(ParseNumber(json, pos));
        }
        else if (key == "version") {
            ExpectChar(json, pos, '{');
            while (pos < json.size() && json[pos] != '}') {
                std::string vkey = ParseKey(json, pos);

                if (vkey == "major") m_version.major = static_cast<uint32_t>(ParseNumber(json, pos));
                else if (vkey == "minor") m_version.minor = static_cast<uint32_t>(ParseNumber(json, pos));
                else if (vkey == "timestamp") m_version.timestamp = static_cast<uint64_t>(ParseNumber(json, pos));
                else if (vkey == "author") m_version.author = ParseString(json, pos);
                else if (vkey == "changeDescription") m_version.changeDescription = ParseString(json, pos);

                SkipWhitespace(json, pos);
                if (json[pos] == ',') ++pos;
            }
            ExpectChar(json, pos, '}');
        }
        else if (key == "presetName") {
            m_presetName = ParseString(json, pos);
        }
        else if (key == "category") {
            m_category = ParseString(json, pos);
        }
        else if (key == "enabled") {
            m_enabled = ParseBool(json, pos);
        }

        SkipWhitespace(json, pos);
        if (json[pos] == ',') ++pos;
    }

    return true;
}

bool LocationDefinition::SaveToFile(const std::string& filePath) const {
    std::ofstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    file << ToJson(true);
    return file.good();
}

bool LocationDefinition::LoadFromFile(const std::string& filePath) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        return false;
    }

    std::ostringstream ss;
    ss << file.rdbuf();

    bool result = FromJson(ss.str());
    if (result) {
        m_filePath = filePath;
    }
    return result;
}

// =============================================================================
// Utility
// =============================================================================

std::unique_ptr<LocationDefinition> LocationDefinition::Clone() const {
    auto clone = std::make_unique<LocationDefinition>();

    // Copy all fields except ID (new ID is generated)
    clone->m_name = m_name;
    clone->m_description = m_description;
    clone->m_worldBounds = m_worldBounds;
    clone->m_geoBounds = m_geoBounds;
    clone->m_tags = m_tags;
    clone->m_pcgPriority = m_pcgPriority;
    clone->m_blendRadius = m_blendRadius;
    clone->m_version = m_version;
    clone->m_presetName = m_presetName;
    clone->m_category = m_category;
    clone->m_enabled = m_enabled;
    // Don't copy file path - clone is not saved to same file

    return clone;
}

bool LocationDefinition::IsValid() const noexcept {
    return !m_name.empty() && m_worldBounds.IsValid();
}

} // namespace Vehement
