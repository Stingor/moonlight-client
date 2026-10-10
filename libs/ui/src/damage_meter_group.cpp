#include <algorithm>
#include <array>
#include <cstdio>
#include <string>

#include "ro/core/decimal_text.h"
#include "ro/core/i18n.h"
#include "ro/ui/damage_meter_window.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"

#include "damage_meter_parts.h"

namespace ro::ui {
namespace damage_meter {
namespace {

// ── LA BARRE D'OUTILS, en pixels à l'échelle 100 % ──────────────────────────
constexpr float kToolbarRoomPixels = 4.0F;
constexpr float kToolGapPixels = 8.0F;
constexpr float kCaretPixels = 5.0F;
// ── LES BARRES ──────────────────────────────────────────────────────────────
constexpr float kBarRoomPixels = 4.0F;
constexpr float kBarGapPixels = 2.0F;
constexpr float kBarRoundingPixels = 2.0F;
constexpr float kPadPixels = 4.0F;
// Notre barre porte la couleur du joueur ; celles des autres, la même éteinte.
constexpr float kSelfBarOpacity = 0.45F;
constexpr float kOtherBarOpacity = 0.22F;
constexpr ImU32 kHoveredBarColor = IM_COL32(255, 255, 255, 22);
constexpr float kPercent = 100.0F;

constexpr const char* kModePopup = "##modes_compteur";
constexpr const char* kSegmentPopup = "##segments_compteur";

[[nodiscard]] float ToolbarTextSize() { return ImGui::GetFontSize() * kLabelTextScale; }

// Un mot cliquable de la barre d'outils, suivi au besoin d'un petit triangle :
// il dit qu'il ouvre une liste. Rend vrai au clic ; avance l'abscisse.
[[nodiscard]] bool ToolWord(const char* id, const char* word, bool active, bool caret, float* x,
                            float top, float height, ImU32 accent) {
  ImDrawList* into = ImGui::GetWindowDrawList();
  ImFont* font = ImGui::GetFont();
  const float size = ToolbarTextSize();
  const float caret_width = caret ? Px(kCaretPixels) + Px(kToolGapPixels) * 0.5F : 0.0F;
  const float width = TextWidth(font, size, word) + caret_width;
  ImGui::SetCursorScreenPos(ImVec2(*x, top));
  const bool clicked = ImGui::InvisibleButton(id, ImVec2(width, height));
  const bool hovered = ImGui::IsItemHovered();
  const ImU32 color = active || hovered ? accent : kSubColor;
  const float text_y = top + (height - size) * 0.5F;
  PaintText(into, font, size, ImVec2(*x, text_y), color, word);
  if (caret) {
    const float left = *x + TextWidth(font, size, word) + Px(kToolGapPixels) * 0.25F;
    const float half = Px(kCaretPixels) * 0.5F;
    const float middle = top + height * 0.5F;
    into->AddTriangleFilled(ImVec2(left, middle - half * 0.5F),
                            ImVec2(left + Px(kCaretPixels), middle - half * 0.5F),
                            ImVec2(left + half, middle + half * 0.5F), color);
  }
  *x += width + Px(kToolGapPixels);
  return clicked;
}

}  // namespace

// ⚠ UN FORMAT LITTÉRAL PAR MODE : passé par une aide, il ne se vérifierait plus.
std::string AmountText(DamageMeterMode mode, std::int64_t amount) {
  const std::string digits = GroupDamageDigits(amount);
  std::array<char, 96> text{};
  switch (mode) {
    case DamageMeterMode::kHealingDone:
      std::snprintf(text.data(), text.size(), core::Tr("%s soignés"), digits.c_str());
      break;
    case DamageMeterMode::kDamageTaken:
      std::snprintf(text.data(), text.size(), core::Tr("%s dégâts subis"), digits.c_str());
      break;
    case DamageMeterMode::kHealingTaken:
      std::snprintf(text.data(), text.size(), core::Tr("%s soins reçus"), digits.c_str());
      break;
    case DamageMeterMode::kDamageDone:
    case DamageMeterMode::kCount:
      std::snprintf(text.data(), text.size(), core::Tr("%s dégâts"), digits.c_str());
      break;
  }
  return text.data();
}

namespace {

void DescribeBar(const DamageMeterView& view, const DamageMeterBarView& bar) {
  if (!BeginSkinTooltip()) {
    return;
  }
  ImGui::TextUnformatted(std::string(bar.name).c_str());
  ImGui::TextUnformatted(AmountText(view.mode, bar.amount).c_str());
  ImGui::Text(core::Tr("%s par seconde, %.0f %% du total"),
              FormatDamageAmount(static_cast<std::int64_t>(bar.per_second + 0.5F)).c_str(),
              static_cast<double>(bar.share * kPercent));
  ImGui::TextDisabled("%s", core::Tr("Clic : le détail par skill."));
  EndSkinTooltip();
}

void DescribeSkill(const DamageMeterView& view, const DamageMeterSkillView& skill) {
  if (!BeginSkinTooltip()) {
    return;
  }
  ImGui::TextUnformatted(std::string(skill.name).c_str());
  ImGui::TextUnformatted(AmountText(view.mode, skill.amount).c_str());
  ImGui::Text(core::Tr("%u fois, dont %u critiques"), skill.events, skill.critical);
  ImGui::Text(core::Tr("Le plus gros : %s"), GroupDamageDigits(skill.biggest).c_str());
  EndSkinTooltip();
}

// Une rangée : la barre qui mesure, le texte à gauche, le montant à droite.
// Rend vrai au clic.
[[nodiscard]] bool Row(int rank, float share_of_top, ImU32 fill, const std::string& left,
                       const std::string& right, float width) {
  ImDrawList* into = ImGui::GetWindowDrawList();
  ImFont* font = ImGui::GetFont();
  const float size = ImGui::GetFontSize();
  const float height = size + Px(kBarRoomPixels);
  const ImVec2 low = ImGui::GetCursorScreenPos();
  const ImVec2 high(low.x + width, low.y + height);
  ImGui::PushID(rank);
  const bool clicked = ImGui::InvisibleButton("##rangee", ImVec2(width, height));
  ImGui::PopID();
  into->AddRectFilled(low, ImVec2(low.x + width * std::clamp(share_of_top, 0.0F, 1.0F), high.y),
                      fill, Px(kBarRoundingPixels));
  if (ImGui::IsItemHovered()) {
    into->AddRectFilled(low, high, kHoveredBarColor, Px(kBarRoundingPixels));
  }
  const float text_y = low.y + (height - size) * 0.5F;
  const float right_width = TextWidth(font, size, right.c_str());
  PaintText(into, font, size, ImVec2(high.x - Px(kPadPixels) - right_width, text_y), IM_COL32_WHITE,
            right.c_str());
  into->PushClipRect(low, ImVec2(high.x - Px(kPadPixels) * 2.0F - right_width, high.y), true);
  PaintText(into, font, size, ImVec2(low.x + Px(kPadPixels), text_y), kSubColor, left.c_str());
  into->PopClipRect();
  ImGui::SetCursorScreenPos(ImVec2(low.x, high.y + Px(kBarGapPixels)));
  return clicked;
}

[[nodiscard]] std::string AmountAndShare(std::int64_t amount, float share) {
  std::array<char, 64> text{};
  std::snprintf(text.data(), text.size(), core::Tr("%s (%.0f %%)"),
                FormatDamageAmount(amount).c_str(), static_cast<double>(share * kPercent));
  return text.data();
}

void DrawBars(const DamageMeterView& view, float width, DamageMeterAction* asked) {
  const std::int64_t top = view.bars.empty() ? 1 : std::max<std::int64_t>(view.bars[0].amount, 1);
  for (std::size_t rank = 0; rank < view.bars.size(); ++rank) {
    const DamageMeterBarView& bar = view.bars[rank];
    // Le rang, puis « . ».
    std::array<char, ro::core::kDecimalTextBytes<std::size_t> + 2> place{};
    std::snprintf(place.data(), place.size(), "%zu. ", rank + 1);
    std::array<char, 64> right{};
    std::snprintf(right.data(), right.size(), core::Tr("%s  %s/s"),
                  FormatDamageAmount(bar.amount).c_str(),
                  FormatDamageAmount(static_cast<std::int64_t>(bar.per_second + 0.5F)).c_str());
    const ImU32 fill = Tint(view.look.plot_color, bar.self ? kSelfBarOpacity : kOtherBarOpacity);
    if (Row(static_cast<int>(rank), static_cast<float>(bar.amount) / static_cast<float>(top), fill,
            std::string(place.data()) + std::string(bar.name), right.data(), width)) {
      asked->detail = bar.id;
    }
    if (ImGui::IsItemHovered()) {
      DescribeBar(view, bar);
    }
  }
}

void DrawDetail(const DamageMeterView& view, float width, DamageMeterAction* asked) {
  // Le retour, en tête : c'est le seul chemin vers les barres.
  std::string back = "< ";
  back += view.detail_name;
  if (Row(-1, 0.0F, 0, back, std::string(core::Tr("retour")), width)) {
    asked->detail = 0U;
  }
  const std::int64_t top =
      view.detail.empty() ? 1 : std::max<std::int64_t>(view.detail[0].amount, 1);
  for (std::size_t rank = 0; rank < view.detail.size(); ++rank) {
    const DamageMeterSkillView& skill = view.detail[rank];
    static_cast<void>(Row(static_cast<int>(rank),
                          static_cast<float>(skill.amount) / static_cast<float>(top),
                          Tint(view.look.plot_color, kOtherBarOpacity), std::string(skill.name),
                          AmountAndShare(skill.amount, skill.share), width));
    if (ImGui::IsItemHovered()) {
      DescribeSkill(view, skill);
    }
  }
}

}  // namespace

float ToolbarHeight() { return ToolbarTextSize() + Px(kToolbarRoomPixels); }

void DrawToolbar(const DamageMeterView& view, float width, DamageMeterAction* asked) {
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  const float height = ToolbarHeight();
  const ImU32 accent = view.look.text_color;
  float x = origin.x;
  if (ToolWord("##mode", DamageMeterModeName(view.mode), true, true, &x, origin.y, height,
               accent)) {
    ImGui::OpenPopup(kModePopup);
  }
  if (view.pane == DamageMeterPane::kGroup &&
      ToolWord("##segment", DamageMeterSegmentName(view.segment), true, true, &x, origin.y, height,
               accent)) {
    ImGui::OpenPopup(kSegmentPopup);
  }
  // À droite, la bascule « Moi | Groupe ».
  ImFont* font = ImGui::GetFont();
  const float size = ToolbarTextSize();
  const char* self_word = core::Tr("Moi");
  const char* group_word = core::Tr("Groupe");
  float right = origin.x + width - TextWidth(font, size, self_word) -
                TextWidth(font, size, group_word) - Px(kToolGapPixels);
  if (ToolWord("##moi", self_word, view.pane == DamageMeterPane::kSelf, false, &right, origin.y,
               height, accent)) {
    asked->pane = DamageMeterPane::kSelf;
  }
  right -= Px(kToolGapPixels) * 0.5F;
  if (ToolWord("##groupe", group_word, view.pane == DamageMeterPane::kGroup, false, &right,
               origin.y, height, accent)) {
    asked->pane = DamageMeterPane::kGroup;
  }

  if (BeginSkinPopup(kModePopup)) {
    for (std::size_t rank = 0; rank < kDamageMeterModeCount; ++rank) {
      const auto mode = static_cast<DamageMeterMode>(rank);
      if (ImGui::MenuItem(DamageMeterModeName(mode), nullptr, mode == view.mode)) {
        asked->mode = mode;
      }
    }
    EndSkinPopup();
  }
  if (BeginSkinPopup(kSegmentPopup)) {
    for (std::size_t rank = 0; rank < kDamageMeterSegmentCount; ++rank) {
      const auto segment = static_cast<DamageMeterSegment>(rank);
      if (ImGui::MenuItem(DamageMeterSegmentName(segment), nullptr, segment == view.segment)) {
        asked->segment = segment;
      }
    }
    EndSkinPopup();
  }
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(width, height));
}

void DrawGroup(const DamageMeterView& view, ImVec2 size, DamageMeterAction* asked) {
  const ImVec2 origin = ImGui::GetCursorScreenPos();
  // ⚠ UN ENFANT, pour que la molette fasse défiler une longue liste — un
  // groupe de douze ne tient pas dans un compteur de deux cents pixels.
  // ⚠ VERROUILLÉ, LA LISTE EST TRAVERSANTE COMME LA FENÊTRE : un enfant
  // n'hérite pas de `NoInputs`, et avalerait les clics et la molette du monde.
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoBackground;
  if (view.look.locked && !view.docked) {
    flags |= ImGuiWindowFlags_NoInputs;
  }
  if (ImGui::BeginChild("##groupe_compteur", size, ImGuiChildFlags_None, flags)) {
    const float width = ImGui::GetContentRegionAvail().x;
    if (view.detail_id != 0) {
      DrawDetail(view, width, asked);
    } else if (view.bars.empty()) {
      const char* hint = core::Tr("Rien à montrer dans ce segment.");
      PaintText(ImGui::GetWindowDrawList(), ImGui::GetFont(), ImGui::GetFontSize(),
                ImVec2(ImGui::GetCursorScreenPos().x +
                           (width - TextWidth(ImGui::GetFont(), ImGui::GetFontSize(), hint)) * 0.5F,
                       ImGui::GetCursorScreenPos().y + size.y * 0.4F),
                kQuietColor, hint);
    } else {
      DrawBars(view, width, asked);
    }
  }
  ImGui::EndChild();
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(size);
}

}  // namespace damage_meter

const char* DamageMeterModeName(DamageMeterMode mode) {
  switch (mode) {
    case DamageMeterMode::kHealingDone:
      return core::Tr("Soins prodigués");
    case DamageMeterMode::kDamageTaken:
      return core::Tr("Dégâts subis");
    case DamageMeterMode::kHealingTaken:
      return core::Tr("Soins reçus");
    case DamageMeterMode::kDamageDone:
    case DamageMeterMode::kCount:
      break;
  }
  return core::Tr("Dégâts infligés");
}

const char* DamageMeterSegmentName(DamageMeterSegment segment) {
  switch (segment) {
    case DamageMeterSegment::kPrevious:
      return core::Tr("Précédent");
    case DamageMeterSegment::kSession:
      return core::Tr("Session");
    case DamageMeterSegment::kCurrent:
    case DamageMeterSegment::kCount:
      break;
  }
  return core::Tr("Combat");
}

const char* DamageMeterRateUnit(DamageMeterMode mode) {
  switch (mode) {
    case DamageMeterMode::kHealingDone:
      return core::Tr("HPS");
    case DamageMeterMode::kDamageTaken:
      return core::Tr("DTPS");
    case DamageMeterMode::kHealingTaken:
      return core::Tr("HRPS");
    case DamageMeterMode::kDamageDone:
    case DamageMeterMode::kCount:
      break;
  }
  return core::Tr("DPS");
}

}  // namespace ro::ui
