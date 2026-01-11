#pragma once

#include <imgui.h>
#include <string>

/**
 * @brief Modern UI widget helpers with glow effects and animations
 * Inspired by glassmorphic design with gradient accents
 */
class ModernUI {
public:
    // Color scheme based on mystical theme
    static constexpr ImVec4 GradientPurple = ImVec4(0.54f, 0.50f, 1.00f, 1.0f);  // (139, 127, 255)
    static constexpr ImVec4 GradientPink = ImVec4(1.00f, 0.49f, 0.71f, 1.0f);    // (255, 126, 182)
    static constexpr ImVec4 DeepBlue = ImVec4(0.04f, 0.05f, 0.15f, 1.0f);        // (10, 14, 39)
    static constexpr ImVec4 Gold = ImVec4(0.78f, 0.63f, 0.29f, 1.0f);            // Gold accent
    static constexpr ImVec4 Cyan = ImVec4(0.00f, 0.80f, 0.82f, 1.0f);            // Cyan accent

    /**
     * @brief Renders a button with gradient background and glow effect on hover
     */
    static bool GlowButton(const char* label, const ImVec2& size = ImVec2(0, 0));

    /**
     * @brief Renders a stat card with glassmorphic background
     */
    static void StatCard(const char* label, const char* value, const ImVec4& accentColor = Gold);

    /**
     * @brief Renders a collapsing header with gradient accent bar
     */
    static bool GradientHeader(const char* label, ImGuiTreeNodeFlags flags = 0);

    /**
     * @brief Renders a separator with gradient
     */
    static void GradientSeparator(float alpha = 0.5f);

    /**
     * @brief Renders text with gradient (purple to pink)
     */
    static void GradientText(const char* text);

    /**
     * @brief Creates a glassmorphic card background
     * Call BeginGlassCard() / EndGlassCard() around content
     */
    static void BeginGlassCard(const char* id, const ImVec2& size = ImVec2(0, 0));
    static void EndGlassCard();

    /**
     * @brief Renders a compact stat line (label: value)
     */
    static void CompactStat(const char* label, const char* value);

    /**
     * @brief Renders a progress bar with gradient fill
     */
    static void GradientProgressBar(float fraction, const ImVec2& size = ImVec2(-1, 0));

    /**
     * @brief Renders a selectable item with hover glow
     */
    static bool GlowSelectable(const char* label, bool selected = false, ImGuiSelectableFlags flags = 0, const ImVec2& size = ImVec2(0, 0));

    /**
     * @brief Helper to interpolate between two colors
     */
    static ImVec4 LerpColor(const ImVec4& a, const ImVec4& b, float t);

    /**
     * @brief Get hover animation value (0 to 1) with smooth transition
     */
    static float GetHoverAnimation(const char* id, bool isHovered);

private:
    static constexpr float ANIMATION_SPEED = 6.0f;
};
