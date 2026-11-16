#include "ro/ui/equip_window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>
#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/inventory_window.h"
#include "ro/ui/shortcut_bar.h"
#include "ro/ui/skin_popup.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/window_name.h"

#include "achievement_paint.h"
#include "equip_window_parts.h"
#include "ui_scale.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

using achievement::kBlack;
using achievement::kOnRelease;
using achievement::kWhite;
using achievement::Painter;
using render::SkinPiece;

using namespace equip_parts;

// ── LE NOM ──────────────────────────────────────────────────────────────────

// Un octet de suite d'UTF-8 : `10xxxxxx`. Rogner avant lui couperait un
// caractère en deux.
[[nodiscard]] bool IsUtf8Continuation(char byte) {
  constexpr unsigned kTopTwoBits = 0xC0U;
  constexpr unsigned kContinuationBits = 0x80U;
  return (static_cast<unsigned char>(byte) & kTopTwoBits) == kContinuationBits;
}

// LE NOM EN LIGNES, comme le jeu le coupe : au CARACTÈRE, la ligne prenant
// celui qui la fait déborder, deux lignes au plus. ⚠ QUAND IL TIENDRAIT SUR
// PLUS DE DEUX LARGEURS, la seconde se rogne et finit par trois points.
[[nodiscard]] std::vector<std::string> NameLines(const Painter& paint, std::string_view name,
                                                 float room) {
  std::vector<std::string> lines(1);
  for (std::size_t at = 0; at < name.size();) {
    const std::size_t length = achievement::Utf8Length(name[at]);
    lines.back().append(name.substr(at, length));
    at += length;
    if (paint.TextWidth(lines.back(), kTextBody) > room && at < name.size()) {
      if (lines.size() == kNameLinesMost) {
        break;
      }
      lines.emplace_back();
    }
  }
  if (paint.TextWidth(name, kTextBody) > room * static_cast<float>(kNameLinesMost)) {
    std::string& last = lines.back();
    while (!last.empty() && paint.TextWidth(last + std::string(kEllipsis), kTextBody) > room) {
      std::size_t cut = last.size() - 1;
      while (cut > 0 && IsUtf8Continuation(last[cut])) {
        --cut;
      }
      last.erase(cut);
    }
    last += kEllipsis;
  }
  return lines;
}

void PaintName(const Painter& paint, float left, float top, std::string_view name, float room,
               ImU32 color) {
  // ⚠ UN NOM VIDE NE SE MESURE PAS : sans données, sa fin vaut nul, et ImGui
  // la lirait comme une chaîne C à parcourir.
  if (name.empty()) {
    return;
  }
  const std::vector<std::string> lines = NameLines(paint, name, room);
  float y = top - kNameLineLift * static_cast<float>(lines.size() - 1);
  for (const std::string& line : lines) {
    paint.TextShadowed(left - kGlyphBearing, y, line, kTextBody, color, kNameShadow);
    y += kLineHeight;
  }
}

// ── LES PLACES ──────────────────────────────────────────────────────────────

// UNE ICÔNE, teintée quand la pièce est abîmée.
void PaintIcon(const Painter& paint, const EquipSlotView& piece, float left, float top) {
  if (!piece.icon.usable()) {
    return;
  }
  paint.into()->AddImage(piece.icon.id, paint.At(left, top),
                         paint.At(left + piece.icon.width, top + piece.icon.height), ImVec2(0, 0),
                         ImVec2(1, 1), piece.damaged ? kItemDamagedTint : IM_COL32_WHITE);
}

void PaintPlace(const Painter& paint, const EquipWindowLayout& layout, std::size_t place,
                const EquipWindowPiece& shown) {
  if (!shown.piece.filled()) {
    return;
  }
  const float icon_left = IconLeft(layout, place);
  const float icon_top = IconTop(place);
  PaintIcon(paint, shown.piece, icon_left, icon_top);
  const float name_left = IsLeftColumn(place) ? kLeftName : layout.right_name;
  PaintName(paint, name_left, icon_top + kNameBelowIcon, shown.piece.name, layout.name_room,
            EquipWindowNameColor(shown.cards_past_slots));
  if (shown.grade_badge.usable()) {
    paint.Art(shown.grade_badge, icon_left, icon_top + kGradeBelowIcon);
  }
}

// LE LISERÉ DES PLACES OÙ LA PIÈCE TRAÎNÉE PEUT ALLER.
void PaintHighlights(const Painter& paint, const EquipWindowLayout& layout,
                     const EquipWindowView& view, const SkinArt& art) {
  const SkinImage& invert = art.Get(SkinPiece::kEquipSlotHighlight);
  for (std::size_t place = 0; place < kEquipWindowPlaceCount; ++place) {
    if (!view.highlighted[place]) {
      continue;
    }
    const float left = IsLeftColumn(place) ? kLeftHighlight : layout.right_highlight;
    paint.Art(invert, left, IconTop(place) + kNameBelowIcon);
  }
}

// ── LES ONGLETS ─────────────────────────────────────────────────────────────

[[nodiscard]] const char* TabLabel(EquipWindowTab tab) {
  return tab == EquipWindowTab::kCostume ? core::Tr("Costume") : core::Tr("Général");
}

// UN ONGLET : ouvert, il touche le haut de sa bande ; fermé, il descend d'un
// pixel et se referme d'un filet. Les coins sont rognés d'un pixel.
void PaintTab(const Painter& paint, float left, float width, bool open, const char* label) {
  const float top = open ? kTabTop : kTabTop + kTabClosedDrop;
  paint.Fill(left + 1.0F, top + 1.0F, width - 2.0F, kTabHeight - 1.0F, paint.Laid(kWhite));
  const ImU32 border = paint.Laid(kTabBorder);
  paint.Fill(left + 2.0F, top, width - 4.0F, 1.0F, border);
  paint.Fill(left + 1.0F, top + 1.0F, 1.0F, 1.0F, border);
  paint.Fill(left, top + 2.0F, 1.0F, kTabHeight - 2.0F, border);
  paint.Fill(left + width - 2.0F, top + 1.0F, 1.0F, 1.0F, border);
  paint.Fill(left + width - 1.0F, top + 2.0F, 1.0F, kTabHeight - 2.0F, border);
  if (!open) {
    paint.Fill(left, kTabTop + kTabHeight - 1.0F, width, 1.0F, border);
  }
  const float text_width = std::floor(paint.TextWidth(label, kTextBody));
  const float spare = width - text_width;
  const float text_left = left + std::floor(spare / 2.0F) + std::fmod(spare, 2.0F);
  const float text_top =
      kTabTop + std::floor((kTabHeight - kLineHeight) / 2.0F) + (open ? 0.0F : kTabClosedDrop);
  paint.Text(text_left - kGlyphBearing, text_top, label, kTextBody, paint.Ink(kBlack));
}

// LA BANDE DES ONGLETS, à largeurs égales. Rend l'onglet cliqué.
[[nodiscard]] std::optional<EquipWindowTab> DrawTabs(const Painter& paint,
                                                     const EquipWindowLayout& layout,
                                                     EquipWindowTab shown) {
  constexpr std::array<EquipWindowTab, 2> kTabs = {EquipWindowTab::kGeneral,
                                                   EquipWindowTab::kCostume};
  float width = 0.0F;
  for (const EquipWindowTab tab : kTabs) {
    width = std::max(width, std::floor(paint.TextWidth(TabLabel(tab), kTextBody)) + kTabPadding);
  }
  std::optional<EquipWindowTab> asked;
  float left = 0.0F;
  for (const EquipWindowTab tab : kTabs) {
    const bool open = tab == shown;
    PaintTab(paint, left, width, open, TabLabel(tab));
    std::array<char, 16> id{};
    std::snprintf(id.data(), id.size(), "##onglet%d", static_cast<int>(tab));
    if (paint.Hit(id.data(), left, kTabTop, width, kTabHeight).pressed && !open) {
      asked = tab;
    }
    left += width - 1.0F;
  }
  paint.Fill(left + 1.0F, kTabLineTop, layout.width - left - 1.0F, 1.0F, paint.Laid(kTabBorder));
  return asked;
}

}  // namespace

EquipWindowLayout EquipWindowLayoutFor(float width) {
  const float extra = std::max(0.0F, width - kEquipWindowGameWidthPixels);
  const bool widened = extra > 0.0F;
  return EquipWindowLayout{
      .width = width,
      .right_icon = kGameRightIcon + extra,
      .right_highlight = kGameRightHighlight + extra,
      .right_name = kGameRightName + (widened ? kWideRightNameShift : 0.0F),
      .name_room = widened ? kWideNameRoom : kGameNameRoom,
  };
}

namespace {

// La place sous un point, sa colonne déjà décidée.
[[nodiscard]] std::optional<std::size_t> PlaceInColumn(ImVec2 at, bool left_column,
                                                       bool right_column,
                                                       const EquipWindowLayout& layout) {
  std::optional<std::size_t> place;
  const auto row = static_cast<std::size_t>(std::floor((at.y - kZoneTop) / kRowPitch));
  if (left_column) {
    place = row == 0 ? kFirstLeftPlace : row - 1;
  } else if (right_column) {
    place = row == 0 ? kFirstRightPlace : row + kRowsPerColumn - 1;
  }
  const float center = CenterOf(layout);
  if (center - kAmmoFromCenter < at.x && at.x < center - kAmmoZoneInset && kAmmoIconTop < at.y &&
      at.y < kAmmoIconTop + kIconPixels) {
    place = kEquipWindowAmmoPlace;
  }
  return place;
}

[[nodiscard]] bool InZone(ImVec2 at) { return at.y >= kZoneTop && at.y <= kZoneBottom; }

}  // namespace

std::optional<std::size_t> EquipWindowIconAt(const EquipWindowLayout& layout, ImVec2 at) {
  if (!InZone(at)) {
    return std::nullopt;
  }
  const bool left = at.x >= kLeftIcon && at.x < kLeftIcon + kIconPixels;
  const bool right = at.x >= layout.right_icon && at.x < layout.right_icon + kIconPixels;
  return PlaceInColumn(at, left, right, layout);
}

std::optional<std::size_t> EquipWindowEntryAt(const EquipWindowLayout& layout, ImVec2 at) {
  if (!InZone(at)) {
    return std::nullopt;
  }
  const bool left = at.x >= kLeftIcon && at.x < kLeftEntryEnd;
  const bool right = at.x >= kRightEntryStart && at.x < layout.width - kEntryEdge;
  return PlaceInColumn(at, left, right, layout);
}

ImU32 EquipWindowNameColor(std::uint8_t cards_past_slots) {
  return kNameColors[std::min<std::size_t>(cards_past_slots, kNameColors.size() - 1)];
}

float EquipWindowWidth(const SkinArt& art) {
  const SkinImage& background = art.Get(SkinPiece::kEquipBackground);
  return background.usable() ? background.width : kEquipWindowGameWidthPixels;
}

ImVec2 EquipWindowAvatarBox(const SkinArt& art) {
  return {EquipWindowWidth(art), kEquipWindowAvatarFeetPixels};
}

namespace {

// ── LE CORPS ────────────────────────────────────────────────────────────────

void PaintAvatar(const Painter& paint, const EquipWindowLayout& layout, const SkinImage& avatar) {
  if (!avatar.usable()) {
    return;
  }
  // LES PIEDS AU POINT DU JEU : la boîte se centre un pixel à droite du
  // milieu, et son bas est le sol.
  const float left = CenterOf(layout) + 1.0F - std::floor(avatar.width / 2.0F);
  const float top = kEquipWindowAvatarFeetPixels - avatar.height;
  paint.into()->AddImage(avatar.id, paint.At(left, top),
                         paint.At(left + avatar.width, top + avatar.height));
}

void DrawBody(const Painter& paint, const EquipWindowLayout& layout, EquipWindowState& state,
              const EquipWindowView& view, const SkinArt& art, EquipWindowAction& action) {
  paint.Fill(0.0F, kEquipWindowTitleBarPixels, layout.width,
             kEquipWindowHeightPixels - kEquipWindowTitleBarPixels, paint.paper());
  if (const std::optional<EquipWindowTab> asked = DrawTabs(paint, layout, state.tab)) {
    state.tab = *asked;
  }
  paint.Art(art.Get(SkinPiece::kEquipBackground), 0.0F, kBackgroundTop);
  PaintHighlights(paint, layout, view, art);
  const EquipWindowTab tab = state.tab;
  if (tab == EquipWindowTab::kCostume) {
    paint.Art(art.Get(SkinPiece::kEquipCostumeFrame), 0.0F, kCostumeFrameTop);
  }
  for (std::size_t place = 0; place < kEquipWindowPlaceCount; ++place) {
    PaintPlace(paint, layout, place, PieceAt(view, tab, place));
  }
  // ⚠ LE PERSONNAGE APRÈS LES PLACES : un nom de gauche qui déborde passe
  // derrière lui, comme dans le jeu.
  PaintAvatar(paint, layout, view.avatar);
  if (tab == EquipWindowTab::kGeneral && view.ammo.piece.filled()) {
    const float left = CenterOf(layout) - kAmmoFromCenter;
    PaintIcon(paint, view.ammo.piece, left, kAmmoIconTop);
    PaintName(paint, left, kAmmoNameTop, view.ammo.piece.name, kAmmoNameRoom,
              EquipWindowNameColor(view.ammo.cards_past_slots));
  }

  DrawIconGestures(paint, layout, view, tab, action);
  DrawEntryGestures(paint, layout, view, tab, action);
  if (tab == EquipWindowTab::kCostume) {
    DrawSettingBox(paint, art, core::Tr("Voir les costumes"), view.costumes_shown.value_or(true),
                   action.sheet.show_costumes);
  } else {
    DrawSettingBox(paint, art, core::Tr("Montrer mon équipement"),
                   view.equipment_shown.value_or(false), action.sheet.show_equipment);
  }
  DrawTakeOffAll(paint, art, view, tab, action);
  // ⚠ LA MONTURE ET LE CHARIOT NE SERVENT QUE SUR L'ONGLET GÉNÉRAL.
  if (tab == EquipWindowTab::kGeneral) {
    DrawCartButton(paint, layout, art, view, action);
    DrawRiderButton(paint, layout, art, view.riders, action);
  }
  DrawDropTarget(paint, layout, action);
}

}  // namespace

EquipWindowAction DrawEquipWindow(EquipWindowState& state, const EquipWindowView& view,
                                  const SkinArt& art) {
  EquipWindowAction action;
  if (!state.open) {
    return action;
  }
  const float scale = Scale();
  const EquipWindowLayout layout = EquipWindowLayoutFor(EquipWindowWidth(art));
  state.avatar_box = EquipWindowAvatarBox(art);
  const float body = kEquipWindowHeightPixels - kEquipWindowTitleBarPixels;
  ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver,
                          ImVec2(0.5F, 0.5F));
  // ⚠ LA LARGEUR SE POSE, LA HAUTEUR S'AJUSTE AU CORPS : la barre de titre suit
  // son art et la police, et une hauteur totale imposée rognerait le bas.
  ImGui::SetNextWindowSize(ImVec2(layout.width * scale, 0.0F), ImGuiCond_Always);
  // ⚠ LA RÉDUCTION EST CELLE D'IMGUI, reposée depuis l'état retenu à chaque
  // image puis relue : le bouton et le double-clic de la barre la basculent.
  ImGui::SetNextWindowCollapsed(state.minimized, ImGuiCond_Always);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  // 🔴 LA FENÊTRE HABILLÉE COMMUNE : sa barre, sa police de titre, la teinte du
  // thème, et le glisser d'ImGui — par lequel passent l'aimant et la grille.
  constexpr ImGuiWindowFlags kFlags =
      ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
  static WindowName name("Équipement", kEquipWindowId);
  const bool shown = BeginSkinnedWindow(name.Get(), &state.open, art, SkinnedTitle::kBar, kFlags);
  // ⚠ LE BOUTON NE RÉDUIT QU'À L'IMAGE SUIVANTE — il met la bascule en
  // attente —, le double-clic tout de suite : l'état voulu compte les deux.
  ImGuiWindow* const current = ImGui::GetCurrentWindow();
  state.minimized = current->Collapsed != current->WantCollapseToggle;
  if (shown) {
    ImGuiWindow* const window = current;
    // L'ORIGINE EST CELLE DE LA FENÊTRE DU JEU, barre comprise : le corps se
    // mesure depuis là, et la barre réelle peut être plus haute que la sienne.
    const Painter paint(window->DrawList,
                        ImVec2(window->Pos.x, window->Pos.y + window->TitleBarHeight -
                                                  kEquipWindowTitleBarPixels * scale),
                        scale, PaintingLook().paper);
    paint.PushClip(0.0F, kEquipWindowTitleBarPixels, layout.width, body);
    DrawBody(paint, layout, state, view, art, action);
    // LE CORPS ARRÊTE LES CLICS : rien ne passe au monde sous la fenêtre, et
    // elle ne s'emporte que par sa barre.
    static_cast<void>(paint.Hit("##corps", 0.0F, kEquipWindowTitleBarPixels, layout.width, body));
    paint.PopClip();
    // LE CONTENU VAUT LE CORPS, sous la barre réelle : c'est lui que la hauteur
    // ajustée mesure. ⚠ ARRONDI AU PIXEL SUPÉRIEUR, qu'ImGui tronquerait.
    ImGui::SetCursorScreenPos(ImVec2(window->Pos.x, window->Pos.y + window->TitleBarHeight));
    ImGui::Dummy(ImVec2(layout.width * scale, std::ceil(body * scale)));
  }
  EndSkinnedWindow();
  ImGui::PopStyleVar();
  return action;
}

}  // namespace ro::ui
