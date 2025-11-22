#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <functional>
#include <memory>
#include <fstream>
#include <nlohmann/json.hpp>

namespace Nova {

// ============================================================================
// Language/Locale Types
// ============================================================================

/**
 * @brief Language identifier following BCP 47 format
 */
struct LanguageCode {
    std::string code;           ///< e.g., "en", "en-US", "zh-Hans"
    std::string displayName;    ///< e.g., "English", "English (US)"
    std::string nativeName;     ///< e.g., "English", "Deutsch"
    bool rtl = false;           ///< Right-to-left text direction

    bool operator==(const LanguageCode& other) const { return code == other.code; }
};

/**
 * @brief Text direction for layout
 */
enum class TextDirection : uint8_t {
    LeftToRight,
    RightToLeft
};

/**
 * @brief Plural form categories (CLDR)
 */
enum class PluralCategory : uint8_t {
    Zero,
    One,
    Two,
    Few,
    Many,
    Other
};

// ============================================================================
// Plural Rules
// ============================================================================

/**
 * @brief Plural rule function type
 * @param n The number to get plural form for
 * @return The plural category
 */
using PluralRuleFunc = std::function<PluralCategory(int n)>;

/**
 * @brief Common plural rules for different languages
 */
namespace PluralRules {

/**
 * @brief English plural rules (also works for many Germanic languages)
 * one: n = 1
 * other: everything else
 */
inline PluralCategory English(int n) {
    return (n == 1) ? PluralCategory::One : PluralCategory::Other;
}

/**
 * @brief French plural rules
 * one: n = 0 or n = 1
 * other: everything else
 */
inline PluralCategory French(int n) {
    return (n == 0 || n == 1) ? PluralCategory::One : PluralCategory::Other;
}

/**
 * @brief Russian plural rules
 * one: n mod 10 = 1 and n mod 100 != 11
 * few: n mod 10 in 2..4 and n mod 100 not in 12..14
 * many: n mod 10 = 0 or n mod 10 in 5..9 or n mod 100 in 11..14
 * other: everything else
 */
inline PluralCategory Russian(int n) {
    const int mod10 = n % 10;
    const int mod100 = n % 100;

    if (mod10 == 1 && mod100 != 11) return PluralCategory::One;
    if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14)) return PluralCategory::Few;
    if (mod10 == 0 || (mod10 >= 5 && mod10 <= 9) || (mod100 >= 11 && mod100 <= 14)) return PluralCategory::Many;
    return PluralCategory::Other;
}

/**
 * @brief Arabic plural rules
 */
inline PluralCategory Arabic(int n) {
    if (n == 0) return PluralCategory::Zero;
    if (n == 1) return PluralCategory::One;
    if (n == 2) return PluralCategory::Two;
    const int mod100 = n % 100;
    if (mod100 >= 3 && mod100 <= 10) return PluralCategory::Few;
    if (mod100 >= 11) return PluralCategory::Many;
    return PluralCategory::Other;
}

/**
 * @brief Japanese/Chinese/Korean (no plural forms)
 */
inline PluralCategory CJK(int /*n*/) {
    return PluralCategory::Other;
}

/**
 * @brief Polish plural rules
 */
inline PluralCategory Polish(int n) {
    const int mod10 = n % 10;
    const int mod100 = n % 100;

    if (n == 1) return PluralCategory::One;
    if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14)) return PluralCategory::Few;
    if (mod10 == 0 || mod10 == 1 || (mod10 >= 5 && mod10 <= 9) || (mod100 >= 12 && mod100 <= 14)) return PluralCategory::Many;
    return PluralCategory::Other;
}

} // namespace PluralRules

// ============================================================================
// String Entry with Plural Forms
// ============================================================================

/**
 * @brief A localized string entry with optional plural forms
 */
struct LocalizedString {
    std::string key;
    std::string value;                              ///< Default/singular form
    std::unordered_map<PluralCategory, std::string> plurals;  ///< Plural forms
    std::string context;                            ///< Disambiguation context

    /**
     * @brief Get the appropriate form for a count
     */
    [[nodiscard]] const std::string& GetForCount(int count, PluralRuleFunc rule) const {
        if (plurals.empty()) return value;
        PluralCategory cat = rule(count);
        auto it = plurals.find(cat);
        if (it != plurals.end()) return it->second;
        // Fallback to Other, then to base value
        it = plurals.find(PluralCategory::Other);
        return (it != plurals.end()) ? it->second : value;
    }
};

// ============================================================================
// Font Fallback Configuration
// ============================================================================

/**
 * @brief Font fallback configuration for different scripts
 */
struct FontFallback {
    std::string script;         ///< Script name (e.g., "CJK", "Arabic", "Cyrillic")
    std::string fontPath;       ///< Path to fallback font
    float sizeMultiplier = 1.0f; ///< Size adjustment for this font
};

// ============================================================================
// Localization Manager
// ============================================================================

/**
 * @brief Complete localization system
 *
 * Features:
 * - String tables with key-value lookup
 * - Pluralization rules (CLDR-based)
 * - String interpolation with named parameters
 * - RTL text support
 * - Font fallbacks for different scripts
 * - Language detection and switching
 * - JSON-based string tables
 *
 * Usage:
 * @code
 * auto& loc = Localization::Instance();
 * loc.Initialize("assets/localization");
 * loc.SetLanguage("en-US");
 *
 * // Simple string
 * std::string greeting = loc.Get("ui.greeting");
 *
 * // With parameters
 * std::string msg = loc.Format("game.score", {{"score", "1000"}});
 *
 * // Plural form
 * std::string items = loc.Plural("inventory.items", itemCount);
 * @endcode
 */
class Localization {
public:
    /**
     * @brief Get singleton instance
     */
    static Localization& Instance();

    // Non-copyable
    Localization(const Localization&) = delete;
    Localization& operator=(const Localization&) = delete;

    /**
     * @brief Initialize with base path to localization files
     * @param basePath Directory containing language JSON files
     */
    bool Initialize(const std::string& basePath);

    /**
     * @brief Shutdown and cleanup
     */
    void Shutdown();

    // =========== Language Management ===========

    /**
     * @brief Set current language
     * @param languageCode Language code (e.g., "en", "en-US", "ja")
     * @return true if language was loaded successfully
     */
    bool SetLanguage(const std::string& languageCode);

    /**
     * @brief Get current language code
     */
    [[nodiscard]] const std::string& GetCurrentLanguage() const { return m_currentLanguage; }

    /**
     * @brief Get current language info
     */
    [[nodiscard]] const LanguageCode* GetCurrentLanguageInfo() const;

    /**
     * @brief Get list of available languages
     */
    [[nodiscard]] const std::vector<LanguageCode>& GetAvailableLanguages() const { return m_availableLanguages; }

    /**
     * @brief Check if language is available
     */
    [[nodiscard]] bool HasLanguage(const std::string& code) const;

    /**
     * @brief Get text direction for current language
     */
    [[nodiscard]] TextDirection GetTextDirection() const;

    /**
     * @brief Check if current language is RTL
     */
    [[nodiscard]] bool IsRTL() const;

    // =========== String Lookup ===========

    /**
     * @brief Get a localized string by key
     * @param key String key (e.g., "menu.play", "dialog.greeting")
     * @return Localized string, or key if not found
     */
    [[nodiscard]] const std::string& Get(const std::string& key) const;

    /**
     * @brief Get a localized string with context
     * @param key String key
     * @param context Context for disambiguation
     */
    [[nodiscard]] const std::string& GetWithContext(const std::string& key,
                                                     const std::string& context) const;

    /**
     * @brief Check if a key exists
     */
    [[nodiscard]] bool Has(const std::string& key) const;

    // =========== Formatting ===========

    /**
     * @brief Format a string with named parameters
     * @param key String key
     * @param params Named parameters (e.g., {{"name", "Player"}, {"score", "100"}})
     * @return Formatted string
     *
     * String format: "Hello, {name}! Your score is {score}."
     */
    [[nodiscard]] std::string Format(const std::string& key,
                                     const std::unordered_map<std::string, std::string>& params) const;

    /**
     * @brief Format with a single parameter
     */
    [[nodiscard]] std::string Format(const std::string& key,
                                     const std::string& paramName,
                                     const std::string& paramValue) const;

    // =========== Pluralization ===========

    /**
     * @brief Get plural form of a string
     * @param key String key
     * @param count Number for plural selection
     * @return Appropriate plural form
     */
    [[nodiscard]] std::string Plural(const std::string& key, int count) const;

    /**
     * @brief Get plural form with formatting
     * @param key String key
     * @param count Number for plural selection (also used as {count} parameter)
     * @param extraParams Additional parameters
     */
    [[nodiscard]] std::string PluralFormat(const std::string& key, int count,
                                           const std::unordered_map<std::string, std::string>& extraParams = {}) const;

    /**
     * @brief Set plural rule for a language
     */
    void SetPluralRule(const std::string& languageCode, PluralRuleFunc rule);

    // =========== Font Fallbacks ===========

    /**
     * @brief Add a font fallback for a script
     */
    void AddFontFallback(const FontFallback& fallback);

    /**
     * @brief Get font fallbacks
     */
    [[nodiscard]] const std::vector<FontFallback>& GetFontFallbacks() const { return m_fontFallbacks; }

    /**
     * @brief Get font path for a character
     * @param codepoint Unicode codepoint
     * @return Font path or empty string for default font
     */
    [[nodiscard]] std::string GetFontForCodepoint(uint32_t codepoint) const;

    // =========== Callbacks ===========

    using LanguageChangeCallback = std::function<void(const std::string& newLanguage)>;

    /**
     * @brief Register callback for language changes
     * @return Callback ID for unregistration
     */
    uint32_t OnLanguageChanged(LanguageChangeCallback callback);

    /**
     * @brief Unregister language change callback
     */
    void RemoveLanguageChangeCallback(uint32_t id);

    // =========== Utility ===========

    /**
     * @brief Reload current language strings
     */
    bool ReloadCurrentLanguage();

    /**
     * @brief Get all keys in current language
     */
    [[nodiscard]] std::vector<std::string> GetAllKeys() const;

    /**
     * @brief Get missing keys (keys in default language not in current)
     */
    [[nodiscard]] std::vector<std::string> GetMissingKeys() const;

    /**
     * @brief Export current language to JSON
     */
    bool ExportToFile(const std::string& path) const;

    // =========== Number/Date Formatting ===========

    /**
     * @brief Format a number according to current locale
     */
    [[nodiscard]] std::string FormatNumber(double value, int decimals = 2) const;

    /**
     * @brief Format a percentage
     */
    [[nodiscard]] std::string FormatPercent(double value, int decimals = 0) const;

    /**
     * @brief Format currency
     */
    [[nodiscard]] std::string FormatCurrency(double value, const std::string& currencyCode = "USD") const;

private:
    Localization() = default;
    ~Localization() = default;

    bool LoadLanguage(const std::string& code);
    void ParseStringTable(const nlohmann::json& json, const std::string& prefix = "");
    std::string ApplyParameters(const std::string& text,
                                const std::unordered_map<std::string, std::string>& params) const;
    void DetectAvailableLanguages();
    PluralRuleFunc GetPluralRule(const std::string& code) const;

    std::string m_basePath;
    std::string m_currentLanguage;
    std::string m_defaultLanguage = "en";

    // String tables
    std::unordered_map<std::string, LocalizedString> m_strings;
    std::unordered_map<std::string, LocalizedString> m_defaultStrings;

    // Available languages
    std::vector<LanguageCode> m_availableLanguages;

    // Plural rules per language
    std::unordered_map<std::string, PluralRuleFunc> m_pluralRules;

    // Font fallbacks
    std::vector<FontFallback> m_fontFallbacks;

    // Callbacks
    std::unordered_map<uint32_t, LanguageChangeCallback> m_languageCallbacks;
    uint32_t m_nextCallbackId = 1;

    // Locale-specific settings
    char m_decimalSeparator = '.';
    char m_thousandsSeparator = ',';

    bool m_initialized = false;
};

// ============================================================================
// Localization Implementation (inline for header-only option)
// ============================================================================

inline Localization& Localization::Instance() {
    static Localization instance;
    return instance;
}

inline bool Localization::Initialize(const std::string& basePath) {
    m_basePath = basePath;

    // Initialize default plural rules
    m_pluralRules["en"] = PluralRules::English;
    m_pluralRules["en-US"] = PluralRules::English;
    m_pluralRules["en-GB"] = PluralRules::English;
    m_pluralRules["de"] = PluralRules::English;
    m_pluralRules["es"] = PluralRules::English;
    m_pluralRules["it"] = PluralRules::English;
    m_pluralRules["pt"] = PluralRules::English;
    m_pluralRules["fr"] = PluralRules::French;
    m_pluralRules["ru"] = PluralRules::Russian;
    m_pluralRules["uk"] = PluralRules::Russian;
    m_pluralRules["pl"] = PluralRules::Polish;
    m_pluralRules["ar"] = PluralRules::Arabic;
    m_pluralRules["ja"] = PluralRules::CJK;
    m_pluralRules["zh"] = PluralRules::CJK;
    m_pluralRules["zh-Hans"] = PluralRules::CJK;
    m_pluralRules["zh-Hant"] = PluralRules::CJK;
    m_pluralRules["ko"] = PluralRules::CJK;

    DetectAvailableLanguages();

    // Load default language
    if (!m_availableLanguages.empty()) {
        LoadLanguage(m_defaultLanguage);
        m_defaultStrings = m_strings;
    }

    m_initialized = true;
    return true;
}

inline void Localization::Shutdown() {
    m_strings.clear();
    m_defaultStrings.clear();
    m_availableLanguages.clear();
    m_languageCallbacks.clear();
    m_initialized = false;
}

inline bool Localization::SetLanguage(const std::string& languageCode) {
    if (languageCode == m_currentLanguage) return true;

    if (!LoadLanguage(languageCode)) {
        return false;
    }

    m_currentLanguage = languageCode;

    // Update locale settings
    const auto* info = GetCurrentLanguageInfo();
    if (info && info->rtl) {
        // RTL-specific settings if needed
    }

    // Notify callbacks
    for (const auto& [id, callback] : m_languageCallbacks) {
        callback(m_currentLanguage);
    }

    return true;
}

inline const LanguageCode* Localization::GetCurrentLanguageInfo() const {
    for (const auto& lang : m_availableLanguages) {
        if (lang.code == m_currentLanguage) {
            return &lang;
        }
    }
    return nullptr;
}

inline bool Localization::HasLanguage(const std::string& code) const {
    for (const auto& lang : m_availableLanguages) {
        if (lang.code == code) return true;
    }
    return false;
}

inline TextDirection Localization::GetTextDirection() const {
    const auto* info = GetCurrentLanguageInfo();
    return (info && info->rtl) ? TextDirection::RightToLeft : TextDirection::LeftToRight;
}

inline bool Localization::IsRTL() const {
    return GetTextDirection() == TextDirection::RightToLeft;
}

inline const std::string& Localization::Get(const std::string& key) const {
    auto it = m_strings.find(key);
    if (it != m_strings.end()) {
        return it->second.value;
    }

    // Fallback to default language
    auto dit = m_defaultStrings.find(key);
    if (dit != m_defaultStrings.end()) {
        return dit->second.value;
    }

    // Return key as last resort
    static std::string empty;
    auto& cached = const_cast<std::unordered_map<std::string, LocalizedString>&>(m_strings);
    cached[key].value = key;
    return cached[key].value;
}

inline const std::string& Localization::GetWithContext(const std::string& key,
                                                        const std::string& context) const {
    std::string contextKey = key + "##" + context;
    auto it = m_strings.find(contextKey);
    if (it != m_strings.end()) {
        return it->second.value;
    }
    return Get(key);
}

inline bool Localization::Has(const std::string& key) const {
    return m_strings.count(key) > 0 || m_defaultStrings.count(key) > 0;
}

inline std::string Localization::Format(const std::string& key,
                                         const std::unordered_map<std::string, std::string>& params) const {
    return ApplyParameters(Get(key), params);
}

inline std::string Localization::Format(const std::string& key,
                                         const std::string& paramName,
                                         const std::string& paramValue) const {
    return Format(key, {{paramName, paramValue}});
}

inline std::string Localization::Plural(const std::string& key, int count) const {
    auto it = m_strings.find(key);
    if (it != m_strings.end()) {
        auto rule = GetPluralRule(m_currentLanguage);
        return it->second.GetForCount(count, rule);
    }

    // Fallback
    auto dit = m_defaultStrings.find(key);
    if (dit != m_defaultStrings.end()) {
        return dit->second.GetForCount(count, PluralRules::English);
    }

    return key;
}

inline std::string Localization::PluralFormat(const std::string& key, int count,
                                               const std::unordered_map<std::string, std::string>& extraParams) const {
    std::string text = Plural(key, count);
    auto params = extraParams;
    params["count"] = std::to_string(count);
    return ApplyParameters(text, params);
}

inline void Localization::SetPluralRule(const std::string& languageCode, PluralRuleFunc rule) {
    m_pluralRules[languageCode] = std::move(rule);
}

inline void Localization::AddFontFallback(const FontFallback& fallback) {
    m_fontFallbacks.push_back(fallback);
}

inline std::string Localization::GetFontForCodepoint(uint32_t codepoint) const {
    // CJK ranges
    if ((codepoint >= 0x4E00 && codepoint <= 0x9FFF) ||   // CJK Unified
        (codepoint >= 0x3400 && codepoint <= 0x4DBF) ||   // CJK Extension A
        (codepoint >= 0x3000 && codepoint <= 0x303F)) {   // CJK Symbols
        for (const auto& fb : m_fontFallbacks) {
            if (fb.script == "CJK") return fb.fontPath;
        }
    }

    // Arabic
    if ((codepoint >= 0x0600 && codepoint <= 0x06FF) ||
        (codepoint >= 0x0750 && codepoint <= 0x077F)) {
        for (const auto& fb : m_fontFallbacks) {
            if (fb.script == "Arabic") return fb.fontPath;
        }
    }

    // Cyrillic
    if (codepoint >= 0x0400 && codepoint <= 0x04FF) {
        for (const auto& fb : m_fontFallbacks) {
            if (fb.script == "Cyrillic") return fb.fontPath;
        }
    }

    return "";
}

inline uint32_t Localization::OnLanguageChanged(LanguageChangeCallback callback) {
    uint32_t id = m_nextCallbackId++;
    m_languageCallbacks[id] = std::move(callback);
    return id;
}

inline void Localization::RemoveLanguageChangeCallback(uint32_t id) {
    m_languageCallbacks.erase(id);
}

inline bool Localization::ReloadCurrentLanguage() {
    return LoadLanguage(m_currentLanguage);
}

inline std::vector<std::string> Localization::GetAllKeys() const {
    std::vector<std::string> keys;
    keys.reserve(m_strings.size());
    for (const auto& [key, _] : m_strings) {
        keys.push_back(key);
    }
    return keys;
}

inline std::vector<std::string> Localization::GetMissingKeys() const {
    std::vector<std::string> missing;
    for (const auto& [key, _] : m_defaultStrings) {
        if (m_strings.find(key) == m_strings.end()) {
            missing.push_back(key);
        }
    }
    return missing;
}

inline bool Localization::ExportToFile(const std::string& path) const {
    nlohmann::json json;
    for (const auto& [key, str] : m_strings) {
        if (str.plurals.empty()) {
            json[key] = str.value;
        } else {
            json[key]["value"] = str.value;
            for (const auto& [cat, text] : str.plurals) {
                std::string catName;
                switch (cat) {
                    case PluralCategory::Zero: catName = "zero"; break;
                    case PluralCategory::One: catName = "one"; break;
                    case PluralCategory::Two: catName = "two"; break;
                    case PluralCategory::Few: catName = "few"; break;
                    case PluralCategory::Many: catName = "many"; break;
                    case PluralCategory::Other: catName = "other"; break;
                }
                json[key]["plural"][catName] = text;
            }
        }
    }

    std::ofstream file(path);
    if (!file) return false;
    file << json.dump(2);
    return true;
}

inline std::string Localization::FormatNumber(double value, int decimals) const {
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);

    std::string result = buffer;

    // Replace decimal separator
    size_t dotPos = result.find('.');
    if (dotPos != std::string::npos && m_decimalSeparator != '.') {
        result[dotPos] = m_decimalSeparator;
    }

    // Add thousands separators
    if (m_thousandsSeparator != '\0') {
        size_t insertPos = (dotPos != std::string::npos) ? dotPos : result.length();
        int count = 0;
        for (size_t i = insertPos; i > 0; --i) {
            if (result[i-1] == '-') break;
            if (++count == 3 && i > 1 && result[i-2] != '-') {
                result.insert(i-1, 1, m_thousandsSeparator);
                count = 0;
            }
        }
    }

    return result;
}

inline std::string Localization::FormatPercent(double value, int decimals) const {
    return FormatNumber(value * 100.0, decimals) + "%";
}

inline std::string Localization::FormatCurrency(double value, const std::string& currencyCode) const {
    std::string formatted = FormatNumber(value, 2);

    if (currencyCode == "USD") return "$" + formatted;
    if (currencyCode == "EUR") return formatted + " EUR";
    if (currencyCode == "GBP") return "GBP " + formatted;
    if (currencyCode == "JPY") return "JPY " + FormatNumber(value, 0);

    return formatted + " " + currencyCode;
}

inline bool Localization::LoadLanguage(const std::string& code) {
    std::string path = m_basePath + "/" + code + ".json";
    std::ifstream file(path);
    if (!file) {
        // Try without region code
        size_t dash = code.find('-');
        if (dash != std::string::npos) {
            path = m_basePath + "/" + code.substr(0, dash) + ".json";
            file.open(path);
        }
        if (!file) return false;
    }

    try {
        nlohmann::json json;
        file >> json;
        m_strings.clear();
        ParseStringTable(json);

        // Load locale settings
        if (json.contains("_locale")) {
            const auto& locale = json["_locale"];
            if (locale.contains("decimalSeparator")) {
                m_decimalSeparator = locale["decimalSeparator"].get<std::string>()[0];
            }
            if (locale.contains("thousandsSeparator")) {
                m_thousandsSeparator = locale["thousandsSeparator"].get<std::string>()[0];
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

inline void Localization::ParseStringTable(const nlohmann::json& json, const std::string& prefix) {
    for (auto& [key, value] : json.items()) {
        if (key.empty() || key[0] == '_') continue;

        std::string fullKey = prefix.empty() ? key : prefix + "." + key;

        if (value.is_string()) {
            LocalizedString str;
            str.key = fullKey;
            str.value = value.get<std::string>();
            m_strings[fullKey] = std::move(str);
        } else if (value.is_object()) {
            if (value.contains("value") || value.contains("plural")) {
                LocalizedString str;
                str.key = fullKey;
                if (value.contains("value")) {
                    str.value = value["value"].get<std::string>();
                }
                if (value.contains("plural")) {
                    for (auto& [cat, text] : value["plural"].items()) {
                        PluralCategory category = PluralCategory::Other;
                        if (cat == "zero") category = PluralCategory::Zero;
                        else if (cat == "one") category = PluralCategory::One;
                        else if (cat == "two") category = PluralCategory::Two;
                        else if (cat == "few") category = PluralCategory::Few;
                        else if (cat == "many") category = PluralCategory::Many;
                        str.plurals[category] = text.get<std::string>();
                    }
                }
                if (value.contains("context")) {
                    str.context = value["context"].get<std::string>();
                }
                m_strings[fullKey] = std::move(str);
            } else {
                // Nested object
                ParseStringTable(value, fullKey);
            }
        }
    }
}

inline std::string Localization::ApplyParameters(const std::string& text,
                                                  const std::unordered_map<std::string, std::string>& params) const {
    if (params.empty()) return text;

    std::string result = text;
    for (const auto& [name, value] : params) {
        std::string placeholder = "{" + name + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }
    return result;
}

inline void Localization::DetectAvailableLanguages() {
    m_availableLanguages.clear();

    // Define known languages
    std::vector<LanguageCode> knownLanguages = {
        {"en", "English", "English", false},
        {"en-US", "English (US)", "English (US)", false},
        {"en-GB", "English (UK)", "English (UK)", false},
        {"de", "German", "Deutsch", false},
        {"fr", "French", "Fran\xC3\xA7ais", false},
        {"es", "Spanish", "Espa\xC3\xB1ol", false},
        {"it", "Italian", "Italiano", false},
        {"pt", "Portuguese", "Portugu\xC3\xAAs", false},
        {"pt-BR", "Portuguese (Brazil)", "Portugu\xC3\xAAs (Brasil)", false},
        {"ru", "Russian", "\xD0\xA0\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9", false},
        {"pl", "Polish", "Polski", false},
        {"ja", "Japanese", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", false},
        {"ko", "Korean", "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4", false},
        {"zh-Hans", "Chinese (Simplified)", "\xE7\xAE\x80\xE4\xBD\x93\xE4\xB8\xAD\xE6\x96\x87", false},
        {"zh-Hant", "Chinese (Traditional)", "\xE7\xB9\x81\xE9\xAB\x94\xE4\xB8\xAD\xE6\x96\x87", false},
        {"ar", "Arabic", "\xD8\xA7\xD9\x84\xD8\xB9\xD8\xB1\xD8\xA8\xD9\x8A\xD8\xA9", true},
        {"he", "Hebrew", "\xD7\xA2\xD7\x91\xD7\xA8\xD7\x99\xD7\xAA", true},
    };

    // Check which language files exist
    for (const auto& lang : knownLanguages) {
        std::string path = m_basePath + "/" + lang.code + ".json";
        std::ifstream file(path);
        if (file) {
            m_availableLanguages.push_back(lang);
        }
    }
}

inline PluralRuleFunc Localization::GetPluralRule(const std::string& code) const {
    auto it = m_pluralRules.find(code);
    if (it != m_pluralRules.end()) {
        return it->second;
    }

    // Try base language code
    size_t dash = code.find('-');
    if (dash != std::string::npos) {
        auto baseIt = m_pluralRules.find(code.substr(0, dash));
        if (baseIt != m_pluralRules.end()) {
            return baseIt->second;
        }
    }

    return PluralRules::English;
}

} // namespace Nova
