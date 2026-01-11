/**
 * @file Geocoding.cpp
 * @brief Geocoding services implementation
 */

#include "Geocoding.hpp"
#include <sstream>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <fstream>
#include <algorithm>

#ifdef __linux__
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#elif defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

namespace Nova {
namespace Location {

GeocodingService& GeocodingService::Instance() {
    static GeocodingService instance;
    return instance;
}

void GeocodingService::Initialize(const GeocodingConfig& config) {
    if (m_initialized) return;

    m_config = config;
    m_initialized = true;

    std::cout << "[Geocoding] Initialized with provider: " << config.provider << std::endl;
}

void GeocodingService::Shutdown() {
    m_forwardCache.clear();
    m_reverseCache.clear();
    m_initialized = false;
}

void GeocodingService::ForwardGeocode(const std::string& address, GeocodingCallback callback) {
    if (!callback) return;

    // Check cache first
    if (m_config.enableCache) {
        auto cached = GetCachedForward(address);
        if (!cached.empty()) {
            callback(cached, GeocodingError::None, "");
            return;
        }
    }

    // Use provider-specific implementation
    if (m_config.provider == "nominatim") {
        NominatimForward(address, "", callback);
    } else {
        // Default to Nominatim
        NominatimForward(address, "", callback);
    }
}

void GeocodingService::ForwardGeocodeNear(const std::string& address,
                                           const LocationCoordinate& nearLocation,
                                           GeocodingCallback callback) {
    std::ostringstream params;
    params << "&viewbox=" << (nearLocation.longitude - 0.5) << ","
           << (nearLocation.latitude + 0.5) << ","
           << (nearLocation.longitude + 0.5) << ","
           << (nearLocation.latitude - 0.5)
           << "&bounded=0";

    NominatimForward(address, params.str(), callback);
}

void GeocodingService::ForwardGeocodeWithinBounds(const std::string& address,
                                                   const LocationCoordinate& sw,
                                                   const LocationCoordinate& ne,
                                                   GeocodingCallback callback) {
    std::ostringstream params;
    params << "&viewbox=" << sw.longitude << "," << ne.latitude << ","
           << ne.longitude << "," << sw.latitude
           << "&bounded=1";

    NominatimForward(address, params.str(), callback);
}

void GeocodingService::ReverseGeocode(const LocationCoordinate& coordinate,
                                       GeocodingCallback callback) {
    ReverseGeocodeAtZoom(coordinate, 18, callback);
}

void GeocodingService::ReverseGeocodeAtZoom(const LocationCoordinate& coordinate,
                                             int zoomLevel,
                                             GeocodingCallback callback) {
    if (!callback) return;

    // Check cache first
    if (m_config.enableCache) {
        auto cached = GetCachedReverse(coordinate);
        if (!cached.empty()) {
            callback(cached, GeocodingError::None, "");
            return;
        }
    }

    NominatimReverse(coordinate, zoomLevel, callback);
}

void GeocodingService::BatchForwardGeocode(
    const std::vector<std::string>& addresses,
    std::function<void(const std::vector<std::vector<GeocodingResult>>&, GeocodingError)> callback) {

    if (!callback) return;

    std::vector<std::vector<GeocodingResult>> allResults;
    allResults.reserve(addresses.size());

    for (const auto& address : addresses) {
        bool done = false;
        std::vector<GeocodingResult> results;

        ForwardGeocode(address, [&](const std::vector<GeocodingResult>& r,
                                    GeocodingError error,
                                    const std::string& msg) {
            results = r;
            done = true;
        });

        // Simple synchronous wait (in production, use async properly)
        int timeout = 100;
        while (!done && timeout-- > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        allResults.push_back(std::move(results));
    }

    callback(allResults, GeocodingError::None);
}

void GeocodingService::BatchReverseGeocode(
    const std::vector<LocationCoordinate>& coordinates,
    std::function<void(const std::vector<std::vector<GeocodingResult>>&, GeocodingError)> callback) {

    if (!callback) return;

    std::vector<std::vector<GeocodingResult>> allResults;
    allResults.reserve(coordinates.size());

    for (const auto& coord : coordinates) {
        bool done = false;
        std::vector<GeocodingResult> results;

        ReverseGeocode(coord, [&](const std::vector<GeocodingResult>& r,
                                  GeocodingError error,
                                  const std::string& msg) {
            results = r;
            done = true;
        });

        int timeout = 100;
        while (!done && timeout-- > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        allResults.push_back(std::move(results));
    }

    callback(allResults, GeocodingError::None);
}

std::vector<GeocodingResult> GeocodingService::GetCachedForward(const std::string& address) const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    std::string key = MakeForwardCacheKey(address);
    auto it = m_forwardCache.find(key);

    if (it != m_forwardCache.end()) {
        auto age = std::chrono::system_clock::now() - it->second.timestamp;
        if (age < std::chrono::minutes(m_config.cacheTTLMinutes)) {
            return it->second.results;
        }
    }

    return {};
}

std::vector<GeocodingResult> GeocodingService::GetCachedReverse(const LocationCoordinate& coord) const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    std::string key = MakeReverseCacheKey(coord);
    auto it = m_reverseCache.find(key);

    if (it != m_reverseCache.end()) {
        auto age = std::chrono::system_clock::now() - it->second.timestamp;
        if (age < std::chrono::minutes(m_config.cacheTTLMinutes)) {
            return it->second.results;
        }
    }

    return {};
}

void GeocodingService::ClearCache() {
    std::lock_guard<std::mutex> lock(m_cacheMutex);
    m_forwardCache.clear();
    m_reverseCache.clear();
}

bool GeocodingService::SaveCache(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    std::ofstream file(filepath, std::ios::binary);
    if (!file) return false;

    // Simple serialization format
    auto writeString = [&file](const std::string& s) {
        uint32_t len = static_cast<uint32_t>(s.length());
        file.write(reinterpret_cast<char*>(&len), sizeof(len));
        file.write(s.c_str(), len);
    };

    // Write forward cache
    uint32_t forwardCount = static_cast<uint32_t>(m_forwardCache.size());
    file.write(reinterpret_cast<char*>(&forwardCount), sizeof(forwardCount));

    for (const auto& [key, entry] : m_forwardCache) {
        writeString(key);
        uint32_t resultCount = static_cast<uint32_t>(entry.results.size());
        file.write(reinterpret_cast<char*>(&resultCount), sizeof(resultCount));

        for (const auto& result : entry.results) {
            file.write(reinterpret_cast<const char*>(&result.coordinate.latitude), sizeof(double));
            file.write(reinterpret_cast<const char*>(&result.coordinate.longitude), sizeof(double));
            writeString(result.address.formattedAddress);
            writeString(result.displayName);
        }
    }

    // Write reverse cache
    uint32_t reverseCount = static_cast<uint32_t>(m_reverseCache.size());
    file.write(reinterpret_cast<char*>(&reverseCount), sizeof(reverseCount));

    for (const auto& [key, entry] : m_reverseCache) {
        writeString(key);
        uint32_t resultCount = static_cast<uint32_t>(entry.results.size());
        file.write(reinterpret_cast<char*>(&resultCount), sizeof(resultCount));

        for (const auto& result : entry.results) {
            file.write(reinterpret_cast<const char*>(&result.coordinate.latitude), sizeof(double));
            file.write(reinterpret_cast<const char*>(&result.coordinate.longitude), sizeof(double));
            writeString(result.address.formattedAddress);
            writeString(result.displayName);
        }
    }

    return true;
}

bool GeocodingService::LoadCache(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    std::ifstream file(filepath, std::ios::binary);
    if (!file) return false;

    auto readString = [&file]() -> std::string {
        uint32_t len;
        file.read(reinterpret_cast<char*>(&len), sizeof(len));
        std::string s(len, '\0');
        file.read(&s[0], len);
        return s;
    };

    try {
        // Read forward cache
        uint32_t forwardCount;
        file.read(reinterpret_cast<char*>(&forwardCount), sizeof(forwardCount));

        for (uint32_t i = 0; i < forwardCount; ++i) {
            std::string key = readString();
            uint32_t resultCount;
            file.read(reinterpret_cast<char*>(&resultCount), sizeof(resultCount));

            CacheEntry entry;
            entry.timestamp = std::chrono::system_clock::now();

            for (uint32_t j = 0; j < resultCount; ++j) {
                GeocodingResult result;
                file.read(reinterpret_cast<char*>(&result.coordinate.latitude), sizeof(double));
                file.read(reinterpret_cast<char*>(&result.coordinate.longitude), sizeof(double));
                result.address.formattedAddress = readString();
                result.displayName = readString();
                entry.results.push_back(result);
            }

            m_forwardCache[key] = entry;
        }

        // Read reverse cache
        uint32_t reverseCount;
        file.read(reinterpret_cast<char*>(&reverseCount), sizeof(reverseCount));

        for (uint32_t i = 0; i < reverseCount; ++i) {
            std::string key = readString();
            uint32_t resultCount;
            file.read(reinterpret_cast<char*>(&resultCount), sizeof(resultCount));

            CacheEntry entry;
            entry.timestamp = std::chrono::system_clock::now();

            for (uint32_t j = 0; j < resultCount; ++j) {
                GeocodingResult result;
                file.read(reinterpret_cast<char*>(&result.coordinate.latitude), sizeof(double));
                file.read(reinterpret_cast<char*>(&result.coordinate.longitude), sizeof(double));
                result.address.formattedAddress = readString();
                result.displayName = readString();
                entry.results.push_back(result);
            }

            m_reverseCache[key] = entry;
        }

        return true;
    } catch (...) {
        return false;
    }
}

GeocodingService::CacheStats GeocodingService::GetCacheStats() const {
    std::lock_guard<std::mutex> lock(m_cacheMutex);

    CacheStats stats;
    stats.forwardEntries = m_forwardCache.size();
    stats.reverseEntries = m_reverseCache.size();

    // Estimate size
    stats.totalSizeBytes = 0;
    for (const auto& [key, entry] : m_forwardCache) {
        stats.totalSizeBytes += key.size();
        for (const auto& r : entry.results) {
            stats.totalSizeBytes += sizeof(r.coordinate) + r.displayName.size() +
                                    r.address.formattedAddress.size();
        }
    }
    for (const auto& [key, entry] : m_reverseCache) {
        stats.totalSizeBytes += key.size();
        for (const auto& r : entry.results) {
            stats.totalSizeBytes += sizeof(r.coordinate) + r.displayName.size() +
                                    r.address.formattedAddress.size();
        }
    }

    return stats;
}

// === Private Implementation ===

void GeocodingService::NominatimForward(const std::string& address,
                                         const std::string& params,
                                         GeocodingCallback callback) {
    // URL encode the address
    std::ostringstream encoded;
    for (char c : address) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            encoded << c;
        } else if (c == ' ') {
            encoded << '+';
        } else {
            encoded << '%' << std::uppercase << std::hex << std::setw(2)
                    << std::setfill('0') << (int)(unsigned char)c;
        }
    }

    std::string url = m_config.apiUrl + "/search?q=" + encoded.str() +
                      "&format=json&addressdetails=1&limit=" +
                      std::to_string(m_config.maxResults) +
                      "&accept-language=" + m_config.language +
                      params;

    // Perform HTTP request
    std::string response = HttpGet(url);

    if (response.empty()) {
        callback({}, GeocodingError::NetworkError, "Failed to connect to geocoding service");
        return;
    }

    // Parse response
    auto results = ParseNominatimResponse(response);

    // Cache results
    if (m_config.enableCache && !results.empty()) {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        CacheEntry entry;
        entry.results = results;
        entry.timestamp = std::chrono::system_clock::now();
        m_forwardCache[MakeForwardCacheKey(address)] = entry;
    }

    callback(results, results.empty() ? GeocodingError::NoResults : GeocodingError::None, "");
}

void GeocodingService::NominatimReverse(const LocationCoordinate& coord,
                                         int zoom,
                                         GeocodingCallback callback) {
    std::ostringstream url;
    url << m_config.apiUrl << "/reverse?lat=" << std::fixed << std::setprecision(7)
        << coord.latitude << "&lon=" << coord.longitude
        << "&format=json&addressdetails=1&zoom=" << zoom
        << "&accept-language=" << m_config.language;

    std::string response = HttpGet(url.str());

    if (response.empty()) {
        callback({}, GeocodingError::NetworkError, "Failed to connect to geocoding service");
        return;
    }

    auto results = ParseNominatimResponse(response);

    // Cache results
    if (m_config.enableCache && !results.empty()) {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        CacheEntry entry;
        entry.results = results;
        entry.timestamp = std::chrono::system_clock::now();
        m_reverseCache[MakeReverseCacheKey(coord)] = entry;
    }

    callback(results, results.empty() ? GeocodingError::NoResults : GeocodingError::None, "");
}

std::vector<GeocodingResult> GeocodingService::ParseNominatimResponse(const std::string& json) {
    std::vector<GeocodingResult> results;

    // Simple JSON parsing for Nominatim response
    // In production, use a proper JSON library like nlohmann/json

    auto extractString = [](const std::string& json, const std::string& key) -> std::string {
        size_t pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return "";
        pos = json.find(':', pos);
        if (pos == std::string::npos) return "";
        pos = json.find('"', pos);
        if (pos == std::string::npos) return "";
        pos++;
        size_t end = json.find('"', pos);
        if (end == std::string::npos) return "";
        return json.substr(pos, end - pos);
    };

    auto extractDouble = [](const std::string& json, const std::string& key) -> double {
        size_t pos = json.find("\"" + key + "\"");
        if (pos == std::string::npos) return 0.0;
        pos = json.find(':', pos);
        if (pos == std::string::npos) return 0.0;
        pos++;
        while (pos < json.length() && (json[pos] == ' ' || json[pos] == '"')) pos++;
        size_t end = pos;
        while (end < json.length() && (std::isdigit(json[end]) || json[end] == '.' || json[end] == '-')) end++;
        if (end > pos) {
            try {
                return std::stod(json.substr(pos, end - pos));
            } catch (...) {
                return 0.0;
            }
        }
        return 0.0;
    };

    // Check if response is array or single object
    bool isArray = !json.empty() && json[0] == '[';

    if (isArray) {
        // Parse array of results
        size_t start = 0;
        while ((start = json.find('{', start)) != std::string::npos) {
            size_t end = json.find('}', start);
            if (end == std::string::npos) break;

            // Find matching brace (handle nested objects)
            int depth = 1;
            end = start + 1;
            while (end < json.length() && depth > 0) {
                if (json[end] == '{') depth++;
                else if (json[end] == '}') depth--;
                end++;
            }

            std::string objJson = json.substr(start, end - start);

            GeocodingResult result;
            result.coordinate.latitude = extractDouble(objJson, "lat");
            result.coordinate.longitude = extractDouble(objJson, "lon");
            result.displayName = extractString(objJson, "display_name");
            result.placeId = extractString(objJson, "place_id");
            result.category = extractString(objJson, "type");

            // Parse address components
            result.address.streetNumber = extractString(objJson, "house_number");
            result.address.street = extractString(objJson, "road");
            result.address.neighborhood = extractString(objJson, "suburb");
            result.address.city = extractString(objJson, "city");
            if (result.address.city.empty()) {
                result.address.city = extractString(objJson, "town");
            }
            if (result.address.city.empty()) {
                result.address.city = extractString(objJson, "village");
            }
            result.address.county = extractString(objJson, "county");
            result.address.state = extractString(objJson, "state");
            result.address.country = extractString(objJson, "country");
            result.address.countryCode = extractString(objJson, "country_code");
            result.address.postalCode = extractString(objJson, "postcode");
            result.address.formattedAddress = result.displayName;

            // Estimate confidence from importance
            double importance = extractDouble(objJson, "importance");
            result.confidence = importance > 0 ? importance : 0.5;

            if (result.coordinate.IsValid()) {
                results.push_back(result);
            }

            start = end;
        }
    } else {
        // Single object
        GeocodingResult result;
        result.coordinate.latitude = extractDouble(json, "lat");
        result.coordinate.longitude = extractDouble(json, "lon");
        result.displayName = extractString(json, "display_name");
        result.placeId = extractString(json, "place_id");
        result.category = extractString(json, "type");

        result.address.streetNumber = extractString(json, "house_number");
        result.address.street = extractString(json, "road");
        result.address.city = extractString(json, "city");
        if (result.address.city.empty()) {
            result.address.city = extractString(json, "town");
        }
        result.address.state = extractString(json, "state");
        result.address.country = extractString(json, "country");
        result.address.countryCode = extractString(json, "country_code");
        result.address.postalCode = extractString(json, "postcode");
        result.address.formattedAddress = result.displayName;

        if (result.coordinate.IsValid()) {
            results.push_back(result);
        }
    }

    return results;
}

std::string GeocodingService::HttpGet(const std::string& url) {
    // Parse URL
    std::string host, path;
    int port = 443;
    bool useHttps = url.find("https://") == 0;

    size_t hostStart = url.find("://");
    if (hostStart != std::string::npos) {
        hostStart += 3;
    } else {
        hostStart = 0;
    }

    size_t pathStart = url.find('/', hostStart);
    if (pathStart != std::string::npos) {
        host = url.substr(hostStart, pathStart - hostStart);
        path = url.substr(pathStart);
    } else {
        host = url.substr(hostStart);
        path = "/";
    }

    size_t portStart = host.find(':');
    if (portStart != std::string::npos) {
        port = std::stoi(host.substr(portStart + 1));
        host = host.substr(0, portStart);
    } else {
        port = useHttps ? 443 : 80;
    }

    // For HTTPS, we'd need OpenSSL - for simplicity, use HTTP fallback
    if (useHttps) {
        // Replace https with http for nominatim
        port = 80;
    }

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        return "";
    }
#endif

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
#ifdef _WIN32
        WSACleanup();
#endif
        return "";
    }

    struct hostent* server = gethostbyname(host.c_str());
    if (!server) {
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
        return "";
    }

    struct sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    std::memcpy(&serverAddr.sin_addr.s_addr, server->h_addr, server->h_length);
    serverAddr.sin_port = htons(port);

    // Set timeout
#ifdef _WIN32
    DWORD timeout = m_config.timeoutMs;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<char*>(&timeout), sizeof(timeout));
#else
    struct timeval timeout;
    timeout.tv_sec = m_config.timeoutMs / 1000;
    timeout.tv_usec = (m_config.timeoutMs % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
#endif

    if (connect(sock, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr)) < 0) {
#ifdef _WIN32
        closesocket(sock);
        WSACleanup();
#else
        close(sock);
#endif
        return "";
    }

    // Send HTTP request
    std::ostringstream request;
    request << "GET " << path << " HTTP/1.1\r\n";
    request << "Host: " << host << "\r\n";
    request << "User-Agent: " << m_config.userAgent << "\r\n";
    request << "Accept: application/json\r\n";
    request << "Connection: close\r\n\r\n";

    std::string requestStr = request.str();
    send(sock, requestStr.c_str(), static_cast<int>(requestStr.length()), 0);

    // Receive response
    std::string response;
    char buffer[4096];
    int bytesRead;
    while ((bytesRead = recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[bytesRead] = '\0';
        response += buffer;
    }

#ifdef _WIN32
    closesocket(sock);
    WSACleanup();
#else
    close(sock);
#endif

    // Extract body from response
    size_t bodyStart = response.find("\r\n\r\n");
    if (bodyStart != std::string::npos) {
        return response.substr(bodyStart + 4);
    }

    return response;
}

std::string GeocodingService::MakeForwardCacheKey(const std::string& address) const {
    // Normalize address for caching
    std::string key = address;
    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
    // Remove extra whitespace
    key.erase(std::unique(key.begin(), key.end(),
        [](char a, char b) { return a == ' ' && b == ' '; }), key.end());
    return key;
}

std::string GeocodingService::MakeReverseCacheKey(const LocationCoordinate& coord) const {
    // Round to ~100m precision for cache key
    std::ostringstream key;
    key << std::fixed << std::setprecision(4)
        << coord.latitude << "," << coord.longitude;
    return key.str();
}

} // namespace Location
} // namespace Nova
