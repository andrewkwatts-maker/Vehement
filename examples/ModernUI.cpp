#include "ModernUI.hpp"
#include <imgui.h>
#include <unordered_map>

// Animation state storage
static std::unordered_map<std::string, float> s_hoverAnimations;

ImVec4 ModernUI::LerpColor(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
        a.w + (b.w - a.w) * t
    );
}

float ModernUI::GetHoverAnimation(const char* id, bool isHovered) {
    float& anim = s_hoverAnimations[id];
    float target = isHovered ? 1.0f : 0.0f;
    float delta = ImGui::GetIO().DeltaTime * ANIMATION_SPEED;

    if (anim < target) {
        anim = std::min(anim + delta, target);
    } else if (anim > target) {
        anim = std::max(anim - delta, target);
    }

    return anim;
}

bool ModernUI::GlowButton(const char* label, const ImVec2& size) {
    ImGui::PushID(label);

    bool hovered = ImGui::IsItemHovered();
    float anim = GetHoverAnimation(label, hovered);

    // Interpolate colors
    ImVec4 bgColor = LerpColor(
        ImGui::GetStyleColorVec4(ImGuiCol_Button),
        ImVec4(0.45f, 0.35f, 0.65f, 1.0f),
        anim
    );

    ImGui::PushStyleColor(ImGuiCol_Button, bgColor);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.45f, 0.35f, 0.65f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.55f, 0.45f, 0.75f, 1.0f));

    // Add glow effect by drawing a larger button behind with transparency
    if (anim > 0.0f) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 btnSize = size.x > 0 ? size : ImVec2(ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2, 0);

        float glowSize = 4.0f * anim;
        ImU32 glowColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.54f, 0.50f, 1.00f, 0.3f * anim));

        drawList->AddRectFilled(
            ImVec2(pos.x - glowSize, pos.y - glowSize),
            ImVec2(pos.x + btnSize.x + glowSize, pos.y + btnSize.y + glowSize),
            glowColor,
            ImGui::GetStyle().FrameRounding + 2.0f
        );
    }

    bool result = ImGui::Button(label, size);

    ImGui::PopStyleColor(3);
    ImGui::PopID();

    return result;
}

void ModernUI::StatCard(const char* label, const char* value, const ImVec4& accentColor) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 size = ImVec2(ImGui::GetContentRegionAvail().x, 40);

    // Card background
    ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.12f, 0.12f, 0.18f, 0.8f));
    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bgColor, 4.0f);

    // Top accent bar
    ImU32 accentU32 = ImGui::ColorConvertFloat4ToU32(accentColor);
    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + 3), accentU32, 4.0f, ImDrawFlags_RoundCornersTop);

    // Border
    drawList->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y),
        ImGui::ColorConvertFloat4ToU32(ImVec4(accentColor.x * 0.5f, accentColor.y * 0.5f, accentColor.z * 0.5f, 0.4f)),
        4.0f);

    // Text
    ImGui::SetCursorScreenPos(ImVec2(pos.x + 10, pos.y + 8));
    ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "%s", label);
    ImGui::SetCursorScreenPos(ImVec2(pos.x + 10, pos.y + 22));
    ImGui::Text("%s", value);

    ImGui::SetCursorScreenPos(ImVec2(pos.x, pos.y + size.y + 4));
}

bool ModernUI::GradientHeader(const char* label, ImGuiTreeNodeFlags flags) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    ImVec2 pos = ImGui::GetCursorScreenPos();
    bool isOpen = ImGui::CollapsingHeader(label, flags);
    ImVec2 endPos = ImGui::GetCursorScreenPos();

    // Draw gradient accent line on the left
    if (isOpen) {
        drawList->AddRectFilledMultiColor(
            ImVec2(pos.x, pos.y),
            ImVec2(pos.x + 3, endPos.y),
            ImGui::ColorConvertFloat4ToU32(GradientPurple),
            ImGui::ColorConvertFloat4ToU32(GradientPink),
            ImGui::ColorConvertFloat4ToU32(GradientPink),
            ImGui::ColorConvertFloat4ToU32(GradientPurple)
        );
    }

    return isOpen;
}

void ModernUI::GradientSeparator(float alpha) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    float width = ImGui::GetContentRegionAvail().x;

    ImVec4 col1 = GradientPurple;
    col1.w = alpha;
    ImVec4 col2 = GradientPink;
    col2.w = alpha;

    drawList->AddRectFilledMultiColor(
        pos,
        ImVec2(pos.x + width, pos.y + 1),
        ImGui::ColorConvertFloat4ToU32(col1),
        ImGui::ColorConvertFloat4ToU32(col2),
        ImGui::ColorConvertFloat4ToU32(col2),
        ImGui::ColorConvertFloat4ToU32(col1)
    );

    ImGui::Dummy(ImVec2(0, 2));
}

void ModernUI::GradientText(const char* text) {
    // Note: ImGui doesn't support gradient text natively, so we'll use colored text
    // For true gradient, would need custom rendering
    ImGui::PushStyleColor(ImGuiCol_Text, LerpColor(GradientPurple, GradientPink, 0.5f));
    ImGui::Text("%s", text);
    ImGui::PopStyleColor();
}

void ModernUI::BeginGlassCard(const char* id, const ImVec2& size) {
    ImGui::PushID(id);

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 cardSize = size.x > 0 ? size : ImVec2(ImGui::GetContentRegionAvail().x, 0);

    // Glassmorphic background
    ImU32 bgColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.10f, 0.10f, 0.15f, 0.7f));
    drawList->AddRectFilled(pos, ImVec2(pos.x + cardSize.x, pos.y + cardSize.y), bgColor, 8.0f);

    // Gradient border
    ImU32 borderColor = ImGui::ColorConvertFloat4ToU32(ImVec4(0.54f, 0.50f, 1.00f, 0.3f));
    drawList->AddRect(pos, ImVec2(pos.x + cardSize.x, pos.y + cardSize.y), borderColor, 8.0f, 0, 1.5f);

    ImGui::BeginGroup();
}

void ModernUI::EndGlassCard() {
    ImGui::EndGroup();
    ImGui::PopID();
}

void ModernUI::CompactStat(const char* label, const char* value) {
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.65f, 1.0f), "%s:", label);
    ImGui::SameLine();
    ImGui::Text("%s", value);
}

void ModernUI::GradientProgressBar(float fraction, const ImVec2& size) {
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetCursorScreenPos();
    ImVec2 barSize = size.x > 0 ? size : ImVec2(ImGui::GetContentRegionAvail().x, 20);

    // Background
    drawList->AddRectFilled(pos, ImVec2(pos.x + barSize.x, pos.y + barSize.y),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.15f, 0.15f, 0.20f, 0.8f)), 4.0f);

    // Gradient fill
    if (fraction > 0.0f) {
        float fillWidth = barSize.x * fraction;
        drawList->AddRectFilledMultiColor(
            pos,
            ImVec2(pos.x + fillWidth, pos.y + barSize.y),
            ImGui::ColorConvertFloat4ToU32(GradientPurple),
            ImGui::ColorConvertFloat4ToU32(GradientPink),
            ImGui::ColorConvertFloat4ToU32(GradientPink),
            ImGui::ColorConvertFloat4ToU32(GradientPurple)
        );
    }

    // Border
    drawList->AddRect(pos, ImVec2(pos.x + barSize.x, pos.y + barSize.y),
        ImGui::ColorConvertFloat4ToU32(ImVec4(0.4f, 0.4f, 0.5f, 0.6f)), 4.0f);

    ImGui::Dummy(barSize);
}

bool ModernUI::GlowSelectable(const char* label, bool selected, ImGuiSelectableFlags flags, const ImVec2& size) {
    ImGui::PushID(label);

    bool hovered = ImGui::IsItemHovered();
    float anim = GetHoverAnimation(label, hovered || selected);

    // Glow effect
    if (anim > 0.0f) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 itemSize = size.x > 0 ? size : ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetTextLineHeightWithSpacing());

        ImU32 glowColor = ImGui::ColorConvertFloat4ToU32(LerpColor(
            ImVec4(0.0f, 0.0f, 0.0f, 0.0f),
            ImVec4(0.54f, 0.50f, 1.00f, 0.2f),
            anim
        ));

        drawList->AddRectFilled(pos, ImVec2(pos.x + itemSize.x, pos.y + itemSize.y), glowColor, 3.0f);
    }

    bool result = ImGui::Selectable(label, selected, flags, size);

    ImGui::PopID();
    return result;
}
