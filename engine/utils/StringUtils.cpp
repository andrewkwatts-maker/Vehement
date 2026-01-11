#include <string>
#include <string_view>
#include <vector>
#include <sstream>
#include <algorithm>
#include <charconv>
#include <optional>
#include <cctype>
#include <numeric>

namespace Nova {

namespace StringUtils {

// Whitespace characters for trimming
constexpr std::string_view kWhitespace = " \t\n\r\f\v";

/**
 * @brief Split a string by delimiter
 * @param str The string to split
 * @param delimiter The character to split on
 * @return Vector of string parts
 */
[[nodiscard]] std::vector<std::string> Split(std::string_view str, char delimiter) {
    std::vector<std::string> tokens;
    size_t start = 0;
    size_t end = 0;

    while ((end = str.find(delimiter, start)) != std::string_view::npos) {
        tokens.emplace_back(str.substr(start, end - start));
        start = end + 1;
    }
    tokens.emplace_back(str.substr(start));

    return tokens;
}

/**
 * @brief Split a string by any character in delimiters
 */
[[nodiscard]] std::vector<std::string> SplitAny(std::string_view str, std::string_view delimiters) {
    std::vector<std::string> tokens;
    size_t start = str.find_first_not_of(delimiters);

    while (start != std::string_view::npos) {
        size_t end = str.find_first_of(delimiters, start);
        tokens.emplace_back(str.substr(start, end - start));
        start = str.find_first_not_of(delimiters, end);
    }

    return tokens;
}

/**
 * @brief Join strings with a delimiter
 */
[[nodiscard]] std::string Join(const std::vector<std::string>& parts, std::string_view delimiter) {
    if (parts.empty()) return "";

    size_t totalSize = delimiter.size() * (parts.size() - 1);
    for (const auto& part : parts) {
        totalSize += part.size();
    }

    std::string result;
    result.reserve(totalSize);

    result = parts[0];
    for (size_t i = 1; i < parts.size(); ++i) {
        result.push_back(delimiter);
        result.push_back(parts[i]);
    }

    return result;
}

/**
 * @brief Trim whitespace from both ends of string
 */
[[nodiscard]] std::string Trim(std::string_view str) {
    const auto start = str.find_first_not_of(kWhitespace);
    if (start == std::string_view::npos) {
        return "";
    }
    const auto end = str.find_last_not_of(kWhitespace);
    return std::string(str.substr(start, end - start + 1));
}

/**
 * @brief Trim whitespace from left side
 */
[[nodiscard]] std::string TrimLeft(std::string_view str) {
    const auto start = str.find_first_not_of(kWhitespace);
    return start == std::string_view::npos ? "" : std::string(str.substr(start));
}

/**
 * @brief Trim whitespace from right side
 */
[[nodiscard]] std::string TrimRight(std::string_view str) {
    const auto end = str.find_last_not_of(kWhitespace);
    return end == std::string_view::npos ? "" : std::string(str.substr(0, end + 1));
}

/**
 * @brief Convert string to lowercase
 */
[[nodiscard]] std::string ToLower(std::string_view str) {
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

/**
 * @brief Convert string to uppercase
 */
[[nodiscard]] std::string ToUpper(std::string_view str) {
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return result;
}

/**
 * @brief Check if string starts with prefix
 */
[[nodiscard]] constexpr bool StartsWith(std::string_view str, std::string_view prefix) noexcept {
    return str.size() >= prefix.size() && str.substr(0, prefix.size()) == prefix;
}

/**
 * @brief Check if string ends with suffix
 */
[[nodiscard]] constexpr bool EndsWith(std::string_view str, std::string_view suffix) noexcept {
    return str.size() >= suffix.size() && str.substr(str.size() - suffix.size()) == suffix;
}

/**
 * @brief Check if string contains substring
 */
[[nodiscard]] constexpr bool Contains(std::string_view str, std::string_view substr) noexcept {
    return str.find(substr) != std::string_view::npos;
}

/**
 * @brief Replace all occurrences of 'from' with 'to'
 */
[[nodiscard]] std::string Replace(std::string_view str, std::string_view from, std::string_view to) {
    if (from.empty()) {
        return std::string(str);
    }

    std::string result;
    result.reserve(str.size());

    size_t pos = 0;
    size_t prevPos = 0;

    while ((pos = str.find(from, prevPos)) != std::string_view::npos) {
        result.push_back(str.substr(prevPos, pos - prevPos));
        result.push_back(to);
        prevPos = pos + from.size();
    }
    result.push_back(str.substr(prevPos));

    return result;
}

/**
 * @brief Replace first occurrence of 'from' with 'to'
 */
[[nodiscard]] std::string ReplaceFirst(std::string_view str, std::string_view from, std::string_view to) {
    const size_t pos = str.find(from);
    if (pos == std::string_view::npos) {
        return std::string(str);
    }

    std::string result;
    result.reserve(str.size() - from.size() + to.size());
    result.push_back(str.substr(0, pos));
    result.push_back(to);
    result.push_back(str.substr(pos + from.size()));
    return result;
}

/**
 * @brief Parse string to integer
 */
[[nodiscard]] std::optional<int> ParseInt(std::string_view str) noexcept {
    str = std::string_view(str.data() + str.find_first_not_of(kWhitespace),
                           str.find_last_not_of(kWhitespace) + 1 - str.find_first_not_of(kWhitespace));
    int value;
    auto [ptr, ec] = std::from_chars(str.data(), str.data() + str.size(), value);
    if (ec == std::errc{} && ptr == str.data() + str.size()) {
        return value;
    }
    return std::nullopt;
}

/**
 * @brief Parse string to float
 */
[[nodiscard]] std::optional<float> ParseFloat(std::string_view str) noexcept {
    // Note: std::from_chars for float may not be available on all platforms
    // Fall back to strtof
    std::string s(str);
    char* end;
    float value = std::strtof(s.c_str(), &end);
    if (end == s.c_str() + s.size()) {
        return value;
    }
    return std::nullopt;
}

/**
 * @brief Parse string to bool (accepts true/false, yes/no, 1/0)
 */
[[nodiscard]] std::optional<bool> ParseBool(std::string_view str) noexcept {
    std::string lower(str);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (lower == "true" || lower == "yes" || lower == "1" || lower == "on") {
        return true;
    }
    if (lower == "false" || lower == "no" || lower == "0" || lower == "off") {
        return false;
    }
    return std::nullopt;
}

/**
 * @brief Check if string is empty or contains only whitespace
 */
[[nodiscard]] bool IsBlank(std::string_view str) noexcept {
    return str.find_first_not_of(kWhitespace) == std::string_view::npos;
}

/**
 * @brief Check if string contains only digits
 */
[[nodiscard]] bool IsNumeric(std::string_view str) noexcept {
    if (str.empty()) return false;
    return std::all_of(str.begin(), str.end(), [](unsigned char c) { return std::isdigit(c); });
}

/**
 * @brief Check if string contains only alphanumeric characters
 */
[[nodiscard]] bool IsAlphanumeric(std::string_view str) noexcept {
    if (str.empty()) return false;
    return std::all_of(str.begin(), str.end(), [](unsigned char c) { return std::isalnum(c); });
}

/**
 * @brief Repeat a string n times
 */
[[nodiscard]] std::string Repeat(std::string_view str, size_t count) {
    std::string result;
    result.reserve(str.size() * count);
    for (size_t i = 0; i < count; ++i) {
        result.push_back(str);
    }
    return result;
}

/**
 * @brief Pad string on the left to reach specified width
 */
[[nodiscard]] std::string PadLeft(std::string_view str, size_t width, char padChar = ' ') {
    if (str.size() >= width) {
        return std::string(str);
    }
    return std::string(width - str.size(), padChar) + std::string(str);
}

/**
 * @brief Pad string on the right to reach specified width
 */
[[nodiscard]] std::string PadRight(std::string_view str, size_t width, char padChar = ' ') {
    if (str.size() >= width) {
        return std::string(str);
    }
    return std::string(str) + std::string(width - str.size(), padChar);
}

} // namespace StringUtils

} // namespace Nova
