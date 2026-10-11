#include "ro/ui/tracker_panel.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>

#include <imgui.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_widgets.h"

#include "achievement_paint.h"
#include "quest_paint.h"

// LE PANNEAU DE SUIVI : des sections repliables — les quêtes, les succès —, et
// sous chacune des entrées repliables. Sans fond par défaut : seul le texte se
// voit sur le jeu.
namespace ro::ui {
namespace {

using achievement::Painter;
namespace paint_kit = achievement;

// ── LA DISPOSITION, en pixels d'art depuis le coin du panneau ───────────────

// La flèche qui ouvre la fenêtre, et la place gardée à sa droite : le panneau
// découpe son dessin à son bord, et la flèche y perdrait sa pointe.
constexpr float kOpenLeft = 290.0F;
constexpr float kOpenArrowRoom = 20.0F;
constexpr float kPanelWidth = kOpenLeft + kOpenArrowRoom;
// La place au premier affichage : à cette distance du bord droit, centré en
// hauteur sur un panneau de cette hauteur.
constexpr float kTrackerRightGap = 350.0F;
constexpr float kFirstPlacementHeight = 150.0F;
// Le haut du premier en-tête, et la marge sous le dernier élément.
constexpr float kFirstEntryTop = 10.0F;
constexpr float kPanelBottomGap = 5.0F;

// L'en-tête d'une section, et l'écart qui sépare deux sections.
constexpr float kSectionLeft = 4.0F;
constexpr float kSectionHeaderHeight = 18.0F;
constexpr float kSectionGap = 6.0F;
// Le triangle de pliage de l'en-tête, et la place qu'il prend avant le libellé.
constexpr float kFoldMarkSide = 10.0F;
constexpr float kFoldMarkRoom = 13.0F;

// Le titre d'une entrée, et ce qui le sépare de ce qui le suit à droite.
constexpr float kTitleLeft = 10.0F;
constexpr float kTitleGap = 6.0F;
// Le compte des lignes d'une entrée repliée, à droite de son titre.
constexpr float kFoldedCountGap = 5.0F;
constexpr float kFoldedEntryHeight = 20.0F;
// Les lignes d'une entrée dépliée : leur retrait, leur pas, et la marge sous
// la dernière.
constexpr float kLineLeft = 20.0F;
constexpr float kLinePitch = 15.0F;
constexpr float kUnfoldedBottomGap = 5.0F;
// La barre fine sous une ligne : sa longueur, son épaisseur, et ce qu'elle
// ajoute au pas de la ligne.
constexpr float kLineBarWidth = 160.0F;
constexpr float kLineBarHeight = 2.0F;
constexpr float kLineBarGap = 2.0F;
constexpr float kLineBarRoom = kLineBarHeight + kLineBarGap;
// Le voile d'un compte qui avance déborde un peu au-dessus du titre.
constexpr float kFlashOverhang = 2.0F;
// « +2 autres », sous les entrées d'une section.
constexpr float kMoreHeight = 16.0F;

// Le bouton de guidage, à gauche de la flèche, et le badge, à gauche de lui.
constexpr float kGuideWidth = 12.0F;
constexpr float kGuideGap = 4.0F;
constexpr float kGuideLeft = kOpenLeft - kGuideGap - kGuideWidth;
constexpr float kBadgeGap = 6.0F;

// L'ÉPINGLE DE LIEU que dessine le bouton de guidage : une tête ronde, une
// pointe, un point au centre, le tout cerné de noir comme le texte.
//
// ⚠ DESSINÉE ET NON ÉCRITE : aucun glyphe de lieu n'est sûr d'exister dans la
// police du jeu, et un carré « manquant » à la place du bouton ne dirait rien.
constexpr float kPinHeadRadius = 3.5F;
constexpr float kPinHeadDrop = 4.5F;
constexpr float kPinTipDrop = 11.0F;
constexpr float kPinCoreRadius = 1.2F;
constexpr float kPinOutline = 1.0F;
constexpr int kPinSegments = 12;

// Le champ du nombre de quêtes montrées, dans les réglages : sa largeur, et le
// pas de Ctrl+clic.
constexpr float kMostQuestsFieldWidth = 96.0F;
constexpr int kMostQuestsFastStep = 5;

// ── LES COULEURS ────────────────────────────────────────────────────────────

constexpr ImU32 kLightBackground = IM_COL32(0x00, 0x00, 0x00, 0x50);
constexpr ImU32 kDarkBackground = IM_COL32(0x00, 0x00, 0x00, 0xA0);
constexpr float kBackgroundRounding = 4.0F;
// L'en-tête d'une section, un or pâle qui le distingue des titres.
constexpr ImU32 kSectionText = IM_COL32(0xFF, 0xD8, 0x6B, 0xFF);
// Ce qui se survole s'éclaire de la même teinte.
constexpr ImU32 kHoveredText = kSectionText;
// Le badge : au repos, s'il presse, s'il est échu.
constexpr ImU32 kBadgeText = IM_COL32(0xD8, 0xDE, 0xEA, 0xFF);
constexpr ImU32 kBadgeUrgentText = IM_COL32(0xFF, 0x9A, 0x2E, 0xFF);
constexpr ImU32 kBadgeExpiredText = IM_COL32(0xFF, 0x4E, 0x3E, 0xFF);
// « +2 autres ».
constexpr ImU32 kMoreText = IM_COL32(0xB8, 0xBC, 0xC6, 0xFF);
// Le voile d'un compte qui avance, à son plus fort.
constexpr ImU32 kFlashColor = IM_COL32(0xFF, 0xC8, 0x3C, 0x00);
constexpr float kFlashAlphaMost = 88.0F;
constexpr float kFlashRounding = 3.0F;
// La barre fine : en cours, atteinte, et son fond.
constexpr quest_paint::BarColors kLineBar{.fill = IM_COL32(0x8C, 0xB8, 0xFF, 0xFF),
                                          .background = IM_COL32(0x00, 0x00, 0x00, 0xA0)};
constexpr quest_paint::BarColors kLineBarReached{.fill = paint_kit::kTrackerReachedText,
                                                 .background = kLineBar.background};

// ── LES TEXTES FIXES ────────────────────────────────────────────────────────

// « ● [ », la puce du titre, et « ▶ », la flèche qui ouvre : ceux du panneau
// des succès, que la police du jeu porte.
constexpr std::string_view kTitleOpening = "\xE2\x97\x8F [";
constexpr std::string_view kTitleClosing = "]";
constexpr std::string_view kOpenArrow = "\xE2\x96\xB6";

constexpr const char* kSettingsMenuId = "##reglages_suivi";
constexpr const char* kEntryMenuId = "##entree_suivi";
// L'entrée visée par le menu, retenue d'une image à l'autre dans le rangement
// de la fenêtre.
constexpr const char* kMenuKindSlot = "##entree_menu_genre";
constexpr const char* kMenuKeySlot = "##entree_menu_cle";

// Assez pour « (+4294967295) ».
constexpr std::size_t kCountBytes = 24;

// ── L'ÉTAT DE PLIAGE ────────────────────────────────────────────────────────

[[nodiscard]] bool IsExpanded(const TrackerPanelState& state, TrackerKind kind, std::uint32_t key) {
  return std::ranges::find(state.expanded, std::pair(kind, key)) != state.expanded.end();
}

void ToggleExpanded(TrackerPanelState& state, TrackerKind kind, std::uint32_t key) {
  const auto found = std::ranges::find(state.expanded, std::pair(kind, key));
  if (found != state.expanded.end()) {
    state.expanded.erase(found);
  } else {
    state.expanded.emplace_back(kind, key);
  }
}

[[nodiscard]] bool IsFolded(const TrackerPanelState& state, TrackerKind kind) {
  return std::ranges::find(state.folded_sections, kind) != state.folded_sections.end();
}

void ToggleFolded(TrackerPanelState& state, TrackerKind kind) {
  const auto found = std::ranges::find(state.folded_sections, kind);
  if (found != state.folded_sections.end()) {
    state.folded_sections.erase(found);
  } else {
    state.folded_sections.push_back(kind);
  }
}

// ── LES HAUTEURS ────────────────────────────────────────────────────────────

[[nodiscard]] float LineHeight(const TrackerLine& line) {
  return line.progress >= 0.0F ? kLinePitch + kLineBarRoom : kLinePitch;
}

// La hauteur d'une entrée : son titre seul, ou son titre et ses lignes.
[[nodiscard]] float EntryHeight(const TrackerPanelState& state, TrackerKind kind,
                                const TrackerEntry& entry) {
  if (!IsExpanded(state, kind, entry.key)) {
    return kFoldedEntryHeight;
  }
  float height = kLinePitch + kUnfoldedBottomGap;
  for (const TrackerLine& line : entry.lines) {
    height += LineHeight(line);
  }
  return height;
}

// Une section se montre si elle a au moins une entrée.
[[nodiscard]] bool SectionShown(const TrackerSection& section) { return !section.entries.empty(); }

[[nodiscard]] float SectionHeight(const TrackerPanelState& state, const TrackerSection& section) {
  float height = section.label.empty() ? 0.0F : kSectionHeaderHeight;
  if (!section.label.empty() && IsFolded(state, section.kind)) {
    return height;
  }
  for (const TrackerEntry& entry : section.entries) {
    height += EntryHeight(state, section.kind, entry);
  }
  if (!section.more.empty()) {
    height += kMoreHeight;
  }
  return height;
}

[[nodiscard]] const char* BackgroundLabel(TrackerBackground background) {
  switch (background) {
    case TrackerBackground::kNone:
      return core::Tr("Sans fond");
    case TrackerBackground::kLight:
      return core::Tr("Fond léger");
    case TrackerBackground::kDark:
      return core::Tr("Fond sombre");
  }
  return "";
}

// ── LE DESSIN ───────────────────────────────────────────────────────────────

// Ce que le dessin d'une image apprend en passant : l'entrée sous la souris,
// pour le menu du clic droit.
struct PanelPass {
  std::optional<TrackerTarget> pointed;
};

// L'ÉPINGLE DE LIEU, dans la boîte du bouton de guidage.
void PaintGuidePin(const Painter& paint, float left, float top, bool hovered) {
  ImDrawList* const into = paint.into();
  const float scale = paint.scale();
  const float middle = left + kGuideWidth * 0.5F;
  const ImVec2 head = paint.At(middle, top + kPinHeadDrop);
  const ImVec2 tip = paint.At(middle, top + kPinTipDrop);
  const ImVec2 head_left = paint.At(middle - kPinHeadRadius, top + kPinHeadDrop);
  const ImVec2 head_right = paint.At(middle + kPinHeadRadius, top + kPinHeadDrop);
  const float outline = kPinOutline * scale;
  const ImU32 fill = hovered ? kHoveredText : paint_kit::kWhite;
  // Le contour d'abord, un pixel plus large, puis le corps.
  into->AddCircleFilled(head, (kPinHeadRadius + kPinOutline) * scale, paint_kit::kBlack,
                        kPinSegments);
  into->AddTriangleFilled(ImVec2(head_left.x - outline, head_left.y),
                          ImVec2(head_right.x + outline, head_right.y),
                          ImVec2(tip.x, tip.y + outline), paint_kit::kBlack);
  into->AddCircleFilled(head, kPinHeadRadius * scale, fill, kPinSegments);
  into->AddTriangleFilled(head_left, head_right, tip, fill);
  into->AddCircleFilled(head, kPinCoreRadius * scale, paint_kit::kBlack, kPinSegments);
}

// LA COLONNE DE DROITE d'une entrée : la flèche qui ouvre, le bouton de
// guidage, puis le badge. Rend le bord gauche de ce qu'elle occupe.
float DrawEntryControls(const Painter& paint, TrackerKind kind, const TrackerEntry& entry,
                        float top, TrackerPanelAction& action) {
  const float line_height = paint.TextHeight(paint_kit::kTextLarge);
  const float arrow_width = paint.TextWidth(kOpenArrow, paint_kit::kTextLarge, true);
  const paint_kit::HitResult open =
      paint.Hit("##ouvrir", kOpenLeft, top, arrow_width, line_height, paint_kit::kOnRelease);
  if (open.pressed) {
    action.open = TrackerTarget{.kind = kind, .key = entry.key};
  }
  paint.TextOutlined(kOpenLeft, top, kOpenArrow, paint_kit::kTextLarge,
                     open.hovered ? kHoveredText : paint_kit::kWhite, paint_kit::kBlack, true);
  float left = kOpenLeft;
  if (entry.can_guide) {
    const paint_kit::HitResult guide =
        paint.Hit("##guider", kGuideLeft, top, kGuideWidth, line_height, paint_kit::kOnRelease);
    if (guide.pressed) {
      action.guide = TrackerTarget{.kind = kind, .key = entry.key};
    }
    SkinTooltip(core::Tr("Y aller"));
    PaintGuidePin(paint, kGuideLeft, top, guide.hovered);
    left = kGuideLeft;
  }
  if (!entry.badge.empty()) {
    const float badge_width = paint.TextWidth(entry.badge, paint_kit::kTextMedium);
    left -= kBadgeGap + badge_width;
    const float drop = std::trunc((line_height - paint.TextHeight(paint_kit::kTextMedium)) * 0.5F);
    ImU32 color = kBadgeText;
    if (entry.badge_expired) {
      color = kBadgeExpiredText;
    } else if (entry.badge_urgent) {
      color = kBadgeUrgentText;
    }
    paint.TextOutlined(left, top + drop, entry.badge, paint_kit::kTextMedium, color,
                       paint_kit::kBlack);
  }
  return left;
}

// UNE ENTRÉE : son voile, son titre et sa colonne de droite, puis ses lignes ou
// leur compte. Rend la hauteur qu'elle prend.
float DrawEntry(const Painter& paint, TrackerPanelState& state, TrackerKind kind,
                const TrackerEntry& entry, float top, PanelPass& pass, TrackerPanelAction& action) {
  // ⚠ MESURÉE AVANT LE CLIC : la hauteur rendue est celle qu'on a comptée pour
  // le panneau, même si le clic replie l'entrée.
  const float height = EntryHeight(state, kind, entry);
  const bool expanded = IsExpanded(state, kind, entry.key);
  if (entry.flash > 0.0F) {
    const auto alpha = static_cast<ImU32>(std::clamp(entry.flash, 0.0F, 1.0F) * kFlashAlphaMost);
    paint.into()->AddRectFilled(
        paint.At(0.0F, top - kFlashOverhang), paint.At(kPanelWidth, top - kFlashOverhang + height),
        kFlashColor | (alpha << IM_COL32_A_SHIFT), kFlashRounding * paint.scale());
  }
  if (ImGui::IsWindowHovered() &&
      ImGui::IsMouseHoveringRect(paint.At(0.0F, top), paint.At(kPanelWidth, top + height))) {
    pass.pointed = TrackerTarget{.kind = kind, .key = entry.key};
  }
  ImGui::PushID(static_cast<int>(kind));
  ImGui::PushID(static_cast<int>(entry.key));
  const float controls_left = DrawEntryControls(paint, kind, entry, top, action);

  std::array<char, kCountBytes> count{};
  float count_room = 0.0F;
  if (!expanded && !entry.lines.empty()) {
    std::snprintf(count.data(), count.size(), "(+%zu)", entry.lines.size());
    count_room = kFoldedCountGap + paint.TextWidth(count.data(), paint_kit::kTextLarge, true);
  }
  // LE NOM SE COUPE, la puce et les crochets restent : c'est à eux qu'on
  // reconnaît une entrée.
  const float frame_width = paint.TextWidth(kTitleOpening, paint_kit::kTextLarge, true) +
                            paint.TextWidth(kTitleClosing, paint_kit::kTextLarge, true);
  const float name_room = controls_left - kTitleGap - kTitleLeft - count_room - frame_width;
  std::string title(kTitleOpening);
  title.append(quest_paint::FitToWidth(paint, entry.name, paint_kit::kTextLarge, true, name_room));
  title.append(kTitleClosing);
  const float line_height = paint.TextHeight(paint_kit::kTextLarge);
  const float title_width = paint.TextWidth(title, paint_kit::kTextLarge, true);
  if (paint.Hit("##titre", kTitleLeft, top, title_width, line_height, paint_kit::kOnRelease)
          .pressed) {
    ToggleExpanded(state, kind, entry.key);
  }
  paint.TextOutlined(kTitleLeft, top, title, paint_kit::kTextLarge,
                     entry.completed ? paint_kit::kTrackerReachedText : paint_kit::kWhite,
                     paint_kit::kBlack, true);
  ImGui::PopID();
  ImGui::PopID();

  if (!expanded) {
    if (count_room > 0.0F) {
      paint.TextShadowed(kTitleLeft + title_width + kFoldedCountGap, top, count.data(),
                         paint_kit::kTextLarge, paint_kit::kWhite, paint_kit::kBlack, true);
    }
    return height;
  }
  float line_top = top + kLinePitch;
  for (const TrackerLine& line : entry.lines) {
    const std::string shown = quest_paint::FitToWidth(paint, line.text, paint_kit::kTextLarge,
                                                      false, kOpenLeft - kLineLeft);
    paint.TextShadowed(kLineLeft, line_top, shown, paint_kit::kTextLarge,
                       line.reached ? paint_kit::kTrackerReachedText : paint_kit::kWhite,
                       paint_kit::kBlack);
    quest_paint::PaintBar(paint, kLineLeft, line_top + kLinePitch, kLineBarWidth, kLineBarHeight,
                          line.progress, line.reached ? kLineBarReached : kLineBar);
    line_top += LineHeight(line);
  }
  return height;
}

// UNE SECTION : son en-tête, qui la replie, puis ses entrées et ce qu'elle
// cache. Rend la hauteur qu'elle prend.
float DrawSection(const Painter& paint, TrackerPanelState& state, const TrackerSection& section,
                  float top, PanelPass& pass, TrackerPanelAction& action) {
  const float height = SectionHeight(state, section);
  float cursor = top;
  if (!section.label.empty()) {
    const bool folded = IsFolded(state, section.kind);
    const float line_height = paint.TextHeight(paint_kit::kTextLarge);
    const float width = kFoldMarkRoom + paint.TextWidth(section.label, paint_kit::kTextLarge, true);
    ImGui::PushID(static_cast<int>(section.kind));
    const paint_kit::HitResult hit =
        paint.Hit("##section", kSectionLeft, cursor, width, line_height, paint_kit::kOnRelease);
    ImGui::PopID();
    if (hit.pressed) {
      ToggleFolded(state, section.kind);
    }
    const ImU32 color = hit.hovered ? paint_kit::kWhite : kSectionText;
    quest_paint::PaintFoldMark(paint, kSectionLeft,
                               cursor + std::trunc((line_height - kFoldMarkSide) * 0.5F),
                               kFoldMarkSide, folded, color, paint_kit::kBlack);
    paint.TextOutlined(kSectionLeft + kFoldMarkRoom, cursor, section.label, paint_kit::kTextLarge,
                       color, paint_kit::kBlack, true);
    cursor += kSectionHeaderHeight;
    if (folded) {
      return height;
    }
  }
  for (const TrackerEntry& entry : section.entries) {
    cursor += DrawEntry(paint, state, section.kind, entry, cursor, pass, action);
  }
  if (!section.more.empty()) {
    paint.TextShadowed(kTitleLeft, cursor, section.more, paint_kit::kTextMedium, kMoreText,
                       paint_kit::kBlack);
  }
  return height;
}

// Une place ramenée dans l'écran : la définition a pu changer depuis qu'on l'a
// retenue.
[[nodiscard]] ImVec2 Inside(TrackerPlace place, ImVec2 size, ImVec2 screen) {
  return {std::clamp(place.x, 0.0F, std::max(0.0F, screen.x - size.x)),
          std::clamp(place.y, 0.0F, std::max(0.0F, screen.y - size.y))};
}

[[nodiscard]] const TrackerEntry* FindEntry(std::span<const TrackerSection> sections,
                                            TrackerKind kind, std::uint32_t key) {
  for (const TrackerSection& section : sections) {
    if (section.kind != kind) {
      continue;
    }
    for (const TrackerEntry& entry : section.entries) {
      if (entry.key == key) {
        return &entry;
      }
    }
  }
  return nullptr;
}

// LE MENU D'UNE ENTRÉE : l'ouvrir, y aller, ne plus la suivre. Une entrée
// disparue pendant que le menu est ouvert le referme.
void DrawEntryMenuItems(const TrackerEntry* entry, TrackerTarget target,
                        TrackerPanelAction& action) {
  if (entry == nullptr) {
    ImGui::CloseCurrentPopup();
    return;
  }
  const std::string name(entry->name);
  ImGui::TextDisabled("%s", name.c_str());
  ImGui::Separator();
  if (ImGui::Selectable(core::Tr("Ouvrir"))) {
    action.open = target;
  }
  if (entry->can_guide && ImGui::Selectable(core::Tr("Y aller"))) {
    action.guide = target;
  }
  if (ImGui::Selectable(core::Tr("Ne plus suivre"))) {
    action.untrack = target;
  }
}

// LES MENUS DU CLIC DROIT : celui de l'entrée visée, sinon les réglages.
// ⚠ AUCUN quand les clics traversent : la puce des fenêtres des quêtes et des
// succès reste alors le seul chemin vers les réglages.
void DrawPanelMenus(std::span<const TrackerSection> sections, const TrackerLook& look,
                    const SkinArt& skin, const PanelPass& pass, TrackerPanelAction& action) {
  ImGuiStorage* const storage = ImGui::GetStateStorage();
  const ImGuiID kind_slot = ImGui::GetID(kMenuKindSlot);
  const ImGuiID key_slot = ImGui::GetID(kMenuKeySlot);
  if (!look.click_through && ImGui::IsWindowHovered() &&
      ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
    if (pass.pointed.has_value()) {
      storage->SetInt(kind_slot, static_cast<int>(pass.pointed->kind));
      storage->SetInt(key_slot, static_cast<int>(pass.pointed->key));
      ImGui::OpenPopup(kEntryMenuId);
    } else {
      ImGui::OpenPopup(kSettingsMenuId);
    }
  }
  // ⚠ LUE AVANT D'OUVRIR LE MENU : le rangement est celui de la fenêtre
  // courante, et le menu en est une autre.
  const TrackerTarget target{.kind = static_cast<TrackerKind>(storage->GetInt(kind_slot, 0)),
                             .key = static_cast<std::uint32_t>(storage->GetInt(key_slot, 0))};
  paint_kit::PushSettingsMenuFrame();
  if (BeginSkinPopup(kSettingsMenuId)) {
    const TrackerSettingsAnswer answer = DrawTrackerSettingsItems(look, skin);
    action.look = answer.look;
    action.reset_place = answer.reset_place;
    ImGui::Separator();
    if (ImGui::Selectable(core::Tr("Masquer le suivi"))) {
      action.hide = true;
    }
    SkinTooltip(
        core::Tr("Il revient par la case « Montrer le suivi » du journal des quêtes, ou "
                 "par la page « Quêtes et suivi » des options."));
    EndSkinPopup();
  }
  if (BeginSkinPopup(kEntryMenuId)) {
    DrawEntryMenuItems(FindEntry(sections, target.kind, target.key), target, action);
    EndSkinPopup();
  }
  paint_kit::PopSettingsMenuFrame();
}

// Le menu des réglages : sa marge autour des entrées, et son liseré.
constexpr float kSettingsMenuPadding = 6.0F;
constexpr float kSettingsMenuBorder = 1.0F;

}  // namespace

namespace achievement {

void PushSettingsMenuFrame() {
  const float scale = Scale();
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                      ImVec2(kSettingsMenuPadding * scale, kSettingsMenuPadding * scale));
  ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, kSettingsMenuBorder);
  ImGui::PushStyleColor(ImGuiCol_Border, kSkinBorderColor);
}

void PopSettingsMenuFrame() {
  ImGui::PopStyleColor();
  ImGui::PopStyleVar(2);
}

}  // namespace achievement

float TrackerPanelHeight(const TrackerPanelState& state, std::span<const TrackerSection> sections) {
  float height = kFirstEntryTop + kPanelBottomGap;
  bool first = true;
  for (const TrackerSection& section : sections) {
    if (!SectionShown(section)) {
      continue;
    }
    if (!first) {
      height += kSectionGap;
    }
    first = false;
    height += SectionHeight(state, section);
  }
  return height;
}

TrackerSettingsAnswer DrawTrackerSettingsItems(const TrackerLook& look, const SkinArt& skin) {
  TrackerSettingsAnswer answer;
  TrackerLook changed = look;
  for (std::size_t rank = 0; rank < kTrackerBackgroundCount; ++rank) {
    const auto background = static_cast<TrackerBackground>(rank);
    ImGui::PushID(static_cast<int>(rank));
    if (SkinRadio(BackgroundLabel(background), look.background == background, skin)) {
      changed.background = background;
    }
    ImGui::PopID();
  }
  ImGui::Separator();
  static_cast<void>(SkinCheckBox(core::Tr("Verrouiller la position"), &changed.locked, skin));
  static_cast<void>(
      SkinCheckBox(core::Tr("Laisser passer les clics"), &changed.click_through, skin));
  SkinTooltip(
      core::Tr("Les clics vont au jeu. Le panneau ne se règle plus alors que depuis la puce "
               "des fenêtres des quêtes et des succès."));
  static_cast<void>(SkinCheckBox(core::Tr("Succès dans un panneau à part"), &changed.split, skin));
  ImGui::Separator();
  static_cast<void>(
      SkinCheckBox(core::Tr("Suivre les nouvelles quêtes"), &changed.track_new_quests, skin));
  static_cast<void>(
      SkinCheckBox(core::Tr("Quêtes de la carte d'abord"), &changed.current_map_first, skin));
  int most = std::clamp(changed.most_quests, kTrackerFewestQuests, kTrackerMostQuests);
  ImGui::SetNextItemWidth(kMostQuestsFieldWidth * paint_kit::Scale());
  if (SkinInputInt(core::Tr("Quêtes montrées"), &most, kTrackerFewestQuests, kTrackerMostQuests, 1,
                   kMostQuestsFastStep, skin)) {
    changed.most_quests = most;
  }
  ImGui::Separator();
  if (ImGui::Selectable(core::Tr("Replacer à sa place d'origine"))) {
    answer.reset_place = true;
  }
  if (changed != look) {
    answer.look = changed;
  }
  return answer;
}

TrackerOptionsAnswer DrawTrackerOptions(bool shown, const TrackerLook& look, const SkinArt& skin) {
  TrackerOptionsAnswer answer;
  if (SkinCheckBox(core::Tr("Montrer le suivi"), &shown, skin)) {
    answer.shown = shown;
  }
  SkinTooltip(core::Tr("Le panneau des quêtes et des succès suivis, sur le jeu"));
  ImGui::Separator();
  answer.settings = DrawTrackerSettingsItems(look, skin);
  return answer;
}

TrackerPanelAction DrawTrackerPanel(const char* window_id, TrackerPanelState& state,
                                    std::span<const TrackerSection> sections,
                                    const TrackerLook& look, float first_top, const SkinArt& skin) {
  TrackerPanelAction action;
  if (!state.shown || std::ranges::none_of(sections, SectionShown)) {
    return action;
  }
  const float scale = paint_kit::Scale();
  const ImVec2 screen = ImGui::GetIO().DisplaySize;
  const ImVec2 size(kPanelWidth * scale, TrackerPanelHeight(state, sections) * scale);
  const ImVec2 first_place(screen.x - kTrackerRightGap * scale,
                           (screen.y - kFirstPlacementHeight * scale) * 0.5F + first_top * scale);
  // Posé une fois par session, puis là où le joueur l'a traîné ; une place
  // demandée — celle du disque, ou l'origine — passe avant.
  if (std::exchange(state.place_reset, false)) {
    ImGui::SetNextWindowPos(first_place, ImGuiCond_Always);
    state.move_to.reset();
    // ⚠ CE RETOUR N'EST PAS UN DÉPLACEMENT : retenu, il figerait l'origine
    // d'une définition d'écran qui peut changer.
    state.drawn_at.reset();
  } else if (state.move_to.has_value()) {
    ImGui::SetNextWindowPos(Inside(*state.move_to, size, screen), ImGuiCond_Always);
    state.move_to.reset();
  } else {
    ImGui::SetNextWindowPos(first_place, ImGuiCond_Once);
  }
  ImGui::SetNextWindowSize(size, ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0F);
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
                           ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
                           ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoScrollWithMouse;
  if (look.locked) {
    flags |= ImGuiWindowFlags_NoMove;
  }
  if (look.click_through) {
    flags |= ImGuiWindowFlags_NoInputs;
  }
  if (ImGui::Begin(window_id, nullptr, flags)) {
    const ImVec2 corner = ImGui::GetWindowPos();
    const Painter paint(ImGui::GetWindowDrawList(), corner, scale);
    if (look.background != TrackerBackground::kNone) {
      paint.into()->AddRectFilled(
          corner, ImVec2(corner.x + size.x, corner.y + size.y),
          look.background == TrackerBackground::kDark ? kDarkBackground : kLightBackground,
          kBackgroundRounding * scale);
    }
    PanelPass pass;
    float top = kFirstEntryTop;
    bool first = true;
    for (const TrackerSection& section : sections) {
      if (!SectionShown(section)) {
        continue;
      }
      if (!first) {
        top += kSectionGap;
      }
      first = false;
      top += DrawSection(paint, state, section, top, pass, action);
    }
    ImGui::SetCursorScreenPos(paint.At(0.0F, 0.0F));
    ImGui::Dummy(ImVec2(0.0F, 0.0F));
    DrawPanelMenus(sections, look, skin, pass, action);

    // UN DÉPLACEMENT SE RETIENT SOURIS LÂCHÉE : pendant qu'on traîne, la
    // place d'avant reste celle qu'on compare.
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
      const TrackerPlace place{.x = corner.x, .y = corner.y};
      if (state.drawn_at.has_value() && *state.drawn_at != place) {
        action.moved = place;
      }
      state.drawn_at = place;
    }
  }
  ImGui::End();
  ImGui::PopStyleVar(2);
  return action;
}

}  // namespace ro::ui
