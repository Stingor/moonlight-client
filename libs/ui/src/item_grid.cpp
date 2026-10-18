#include "item_grid.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <optional>
#include <string>

#include <imgui_internal.h>

#include "ro/core/i18n.h"
#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_tooltip.h"
#include "ro/ui/skin_widgets.h"

#include "item_drag.h"
#include "ui_text.h"

namespace ro::ui {
namespace {

using render::SkinPiece;

// La marge d'un badge dans sa case : à droite et en bas.
constexpr float kBadgePadXPixels = 2.0F;
constexpr float kBadgePadYPixels = 1.0F;

// Les onglets se CHEVAUCHENT d'un pixel. Ce n'est pas un tassement approximatif
// mais le dessin du client : le filet du bas de l'un est celui du haut du
// suivant, et un espacement positif -- le defaut d'ImGui en pose quatre --
// ouvrirait une rainure entre chaque.
constexpr float kTabOverlapPixels = -1.0F;

// LE PONT : ce qu'il laisse de chaque bout de l'onglet, et de combien il mord
// sur ce qu'il ouvre.
constexpr float kTabBridgeInsetPixels = 1.0F;
constexpr float kTabBridgeOverlapPixels = 2.0F;
// Le corps des images d'onglet, relevé dedans : blanc pour six d'entre elles,
// gris-bleu pour celle des favoris.
constexpr ImU32 kTabBodyColor = IM_COL32(255, 255, 255, 255);
constexpr ImU32 kFavoriteTabBodyColor = IM_COL32(0xD1, 0xDC, 0xE8, 255);

// Ce qu'on affiche à la place d'une icône absente.
constexpr const char* kMissingIcon = "?";

struct TabLook {
  SkinPiece on;
  SkinPiece off;
};

const std::array<TabLook, kItemTabCount>& TabLooks() {
  static const std::array<TabLook, kItemTabCount> looks = {
      TabLook{SkinPiece::kTabAllOn, SkinPiece::kTabAllOff},
      TabLook{SkinPiece::kTabConsumableOn, SkinPiece::kTabConsumableOff},
      TabLook{SkinPiece::kTabEquipmentOn, SkinPiece::kTabEquipmentOff},
      TabLook{SkinPiece::kTabAmmoOn, SkinPiece::kTabAmmoOff},
      TabLook{SkinPiece::kTabEtcOn, SkinPiece::kTabEtcOff},
      TabLook{SkinPiece::kTabCardOn, SkinPiece::kTabCardOff},
      TabLook{SkinPiece::kTabFavoriteOn, SkinPiece::kTabFavoriteOff},
      TabLook{SkinPiece::kTabWeaponOn, SkinPiece::kTabWeaponOff},
      TabLook{SkinPiece::kTabArmorOn, SkinPiece::kTabArmorOff},
      // ⚠ L'APPARAT REPREND LA TUILE DE L'ÉQUIPEMENT, et ce n'est pas un repli :
      // `tab_cos` EST la tuile d'apparat, que le client donne aussi à l'onglet
      // « Equip » du sac. Deux onglets, une image — et deux fenêtres qui ne les
      // montrent jamais ensemble.
      TabLook{SkinPiece::kTabEquipmentOn, SkinPiece::kTabEquipmentOff},
      TabLook{SkinPiece::kTabCashOn, SkinPiece::kTabCashOff},
  };
  return looks;
}

// Les mêmes, couchés. ⚠ L'ORDRE EST CELUI DE `ItemTab`, comme au-dessus : les
// deux tables s'indexent par le même rang, et les désaccorder donnerait à un
// onglet l'image de son voisin dans une seule des deux dispositions.
const std::array<TabLook, kItemTabCount>& LyingTabLooks() {
  static const std::array<TabLook, kItemTabCount> looks = {
      TabLook{SkinPiece::kTabHAllOn, SkinPiece::kTabHAllOff},
      TabLook{SkinPiece::kTabHConsumableOn, SkinPiece::kTabHConsumableOff},
      TabLook{SkinPiece::kTabHEquipmentOn, SkinPiece::kTabHEquipmentOff},
      TabLook{SkinPiece::kTabHAmmoOn, SkinPiece::kTabHAmmoOff},
      TabLook{SkinPiece::kTabHEtcOn, SkinPiece::kTabHEtcOff},
      TabLook{SkinPiece::kTabHCardOn, SkinPiece::kTabHCardOff},
      TabLook{SkinPiece::kTabHFavoriteOn, SkinPiece::kTabHFavoriteOff},
      TabLook{SkinPiece::kTabHWeaponOn, SkinPiece::kTabHWeaponOff},
      TabLook{SkinPiece::kTabHArmorOn, SkinPiece::kTabHArmorOff},
      TabLook{SkinPiece::kTabHEquipmentOn, SkinPiece::kTabHEquipmentOff},
      TabLook{SkinPiece::kTabHCashOn, SkinPiece::kTabHCashOff},
  };
  return looks;
}

// LA TABLE DE LA DISPOSITION DEMANDÉE. ⚠ LES DEUX S'INDEXENT PAR LE MÊME RANG —
// celui de `ItemTab` —, et c'est ce qui permet de choisir la table sans toucher
// au reste du dessin.
const std::array<TabLook, kItemTabCount>& TabLooksFor(bool lying) {
  return lying ? LyingTabLooks() : TabLooks();
}

// LES DEUX TUILES D'UN ONGLET, une fois la disposition ET le fond des favoris
// tranchés.
//
// 🔴 UN SEUL ENDROIT LE FAIT, et les deux passes le partagent : le dessin et la
// MESURE de la bande. Mesurer avec une tuile et dessiner avec une autre donnerait
// une bande taillée pour ce qu'elle ne montre pas.
[[nodiscard]] TabLook PiecesFor(ItemTab tab, TabStripLook look) {
  if (tab == ItemTab::kFavorite && !look.favorite_tinted) {
    return look.lying ? TabLook{SkinPiece::kTabHFavoritePlainOn, SkinPiece::kTabHFavoritePlainOff}
                      : TabLook{SkinPiece::kTabFavoritePlainOn, SkinPiece::kTabFavoritePlainOff};
  }
  return TabLooksFor(look.lying)[static_cast<std::size_t>(tab)];
}

// CE QU'UN ONGLET A PRIS, et ce qui s'est passé dessus.
struct TabDrawn {
  ImVec2 min{};
  ImVec2 max{};
  bool clicked = false;
  std::uint16_t dropped_slot = 0;
};

// UN ONGLET, POSÉ AU CURSEUR — tuile ou libellé, debout ou couché.
//
// 🔴 LE DESSIN, LE SURVOL, LE CLIC ET LE LÂCHER VIVENT ICI, une fois pour les
// deux dispositions. Recopiés d'une boucle à l'autre, ils divergeraient : un
// lâcher sur l'onglet des favoris, par exemple, marcherait debout et pas
// couché.
//
// ⚠ `thickness` EST L'AXE IMPOSÉ — la largeur d'un onglet debout, la hauteur
// d'un onglet couché. L'autre côté se déduit du rapport de l'image : garder sa
// taille d'origine étirerait la tuile de tout ce que le premier a dû bouger.
TabDrawn DrawOneTab(ItemTab tab, bool active, const SkinArt& art, TabStripLook look,
                    float thickness) {
  const auto rank = static_cast<std::size_t>(tab);
  const TabLook pieces = PiecesFor(tab, look);
  const SkinImage& image = art.Get(active ? pieces.on : pieces.off);
  // ⚠ L'ART PEUT MANQUER même quand on le demande : une installation incomplète
  // n'a pas de tuile. On retombe alors sur le libellé, qui se lit toujours.
  const bool tile = look.images && image.usable();
  const char* const label = ItemTabLabel(tab);

  float length = ImGui::GetFrameHeight();
  if (tile) {
    length = look.lying ? thickness * image.width / image.height
                        : thickness * image.height / image.width;
  } else if (look.lying) {
    length = ImGui::CalcTextSize(label).x + 2.0F * ImGui::GetStyle().FramePadding.x;
  }
  const ImVec2 size = look.lying ? ImVec2(length, thickness) : ImVec2(thickness, length);

  TabDrawn drawn;
  drawn.min = ImGui::GetCursorScreenPos();
  drawn.max = ImVec2(drawn.min.x + size.x, drawn.min.y + size.y);
  ImGui::PushID(static_cast<int>(rank));
  ImGui::InvisibleButton("onglet", size);
  drawn.clicked = ImGui::IsItemClicked();
  const bool hovered = ImGui::IsItemHovered();
  ImDrawList* const into = ImGui::GetWindowDrawList();
  if (tile) {
    into->AddImage(image.id, drawn.min, drawn.max, ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F),
                   PaintedTint(art.look().item_tab_tint));
  } else {
    into->AddRectFilled(drawn.min, drawn.max, active ? art.look().active_tab : art.look().idle_tab);
    // Le libellé est centré sur les deux axes. Debout, la bande a été taillée
    // pour le plus long d'entre eux : aucun ne se rogne.
    const ImVec2 text = ImGui::CalcTextSize(label);
    into->AddText(
        ImVec2(drawn.min.x + (size.x - text.x) * 0.5F, drawn.min.y + (size.y - text.y) * 0.5F),
        ImGui::GetColorU32(ImGuiCol_Text), label);
  }
  // Le survol éclaircit, comme la case d'un objet — jamais le bleu du thème.
  if (hovered && !active) {
    into->AddRectFilled(drawn.min, drawn.max, kItemHoverVeil);
  }
  // ⚠ L'INFOBULLE NE DIT QUE CE QUE LA TUILE TAIT : en mode texte, le nom est
  // déjà écrit sur l'onglet, et la répéter au survol serait du bruit.
  if (hovered && tile) {
    SkinTooltip(label);
  }
  // ⚠ LA CIBLE SE POSE SUR LE DERNIER ÉLÉMENT SOUMIS, donc juste après le bouton
  // invisible de l'onglet — et avant le `PopID`, sinon elle porterait une
  // identité que rien ne distingue de celle de sa voisine.
  if (look.takes_drops && ImGui::BeginDragDropTarget()) {
    // ⚠ LA MÊME CHARGE QUE LA BARRE DE RACCOURCIS, et c'est voulu : un objet
    // qu'on tire est un objet qu'on tire, et deux types de charge obligeraient
    // chaque case à choisir lequel émettre avant de savoir où on va le lâcher.
    if (const std::optional<ItemDrag> dropped = AcceptItemDrag()) {
      const ItemDrag& carried = *dropped;
      drawn.dropped_slot = carried.client_slot;
    }
    ImGui::EndDragDropTarget();
  }
  ImGui::PopID();
  return drawn;
}

// Ce qu'un onglet dessiné apprend au résultat de la bande.
void NoteTab(const TabDrawn& drawn, ItemTab tab, bool active, TabStripResult& result) {
  if (drawn.clicked) {
    result.chosen = tab;
  }
  if (drawn.dropped_slot != 0) {
    result.dropped_slot = drawn.dropped_slot;
    result.dropped_on = tab;
  }
  if (active) {
    result.active_min = drawn.min;
    result.active_max = drawn.max;
    result.active_shown = true;
  }
}

}  // namespace

render::SkinPiece ItemTabPiece(ItemTab tab, bool active) {
  const TabLook& look = TabLooks()[static_cast<std::size_t>(tab)];
  return active ? look.on : look.off;
}

render::SkinPiece ItemTabPieceLying(ItemTab tab, bool active) {
  const TabLook& look = LyingTabLooks()[static_cast<std::size_t>(tab)];
  return active ? look.on : look.off;
}

// L'identité ImGui de l'invite de quantité. ⚠ JAMAIS TRADUITE : c'est une clé,
// pas un texte, et la changer de langue en ferait un autre surgissant.
constexpr const char* kQuantityPromptId = "##quantite";

// La largeur du champ de saisie, en cadratins de la police courante : un nombre
// à cinq chiffres y tient, et il suit l'échelle de l'interface.
constexpr float kQuantityFieldEms = 5.0F;
// Ce qui sépare le champ de ses deux boutons de pas. Serré : les trois ne
// forment qu'un seul objet à l'œil.
constexpr float kQuantityStepGapPixels = 2.0F;
// Le marqueur d'aide, tel que le client l'écrit. ⚠ PAS TRADUIT : ce sont trois
// signes de ponctuation, et les traduire n'aurait aucun sens.
constexpr const char* kHelpMarkerText = "(?)";
// L'icône du fantôme d'un glisser. ⚠ PLUS PETITE QU'UNE CASE : le fantôme suit
// le curseur, et une icône pleine taille cacherait ce qu'on vise.
constexpr float kDragGhostIconPixels = 24.0F;

// Le bandeau en trois morceaux, ou un aplat quand l'installation ne les a pas.
void DrawFooterBar(ImDrawList* into, const SkinArt& art, ImVec2 top_left, ImVec2 bottom_right) {
  const SkinImage& left = art.Get(SkinPiece::kItemBarLeft);
  const SkinImage& middle = art.Get(SkinPiece::kItemBarMiddle);
  const SkinImage& right = art.Get(SkinPiece::kItemBarRight);
  if (!left.usable() || !middle.usable() || !right.usable()) {
    into->AddRectFilled(top_left, bottom_right, art.look().input);
    return;
  }
  const StripRects strip = HorizontalStrip(top_left, bottom_right, left.width, right.width);
  PaintArt(into, art, left.id, ImVec2(strip.left.x, strip.left.y),
           ImVec2(strip.left.z, strip.left.w));
  if (strip.middle.z > strip.middle.x) {
    PaintArt(into, art, middle.id, ImVec2(strip.middle.x, strip.middle.y),
             ImVec2(strip.middle.z, strip.middle.w));
  }
  PaintArt(into, art, right.id, ImVec2(strip.right.x, strip.right.y),
           ImVec2(strip.right.z, strip.right.w));
}

// Une icône du bandeau, centrée verticalement. Rend sa largeur — zéro quand elle
// manque —, dont l'appelant se sert pour avancer.
float DrawFooterIcon(ImDrawList* into, const SkinImage& icon, float x, float center_y) {
  if (!icon.usable()) {
    return 0.0F;
  }
  const ImVec2 top(x, center_y - icon.height * 0.5F);
  into->AddImage(icon.id, top, ImVec2(top.x + icon.width, top.y + icon.height), ImVec2(0.0F, 0.0F),
                 ImVec2(1.0F, 1.0F), PaintedTint(IM_COL32_WHITE));
  return icon.width;
}

bool FooterToggle(const char* id, const SkinImage& on, const SkinImage& off, bool active,
                  ImVec2 left_middle, const char* tooltip, float* width) {
  const SkinImage& shown = active ? on : off;
  if (width != nullptr) {
    *width = shown.usable() ? shown.width : 0.0F;
  }
  if (!shown.usable()) {
    return false;
  }
  ImGuiWindow* const window = ImGui::GetCurrentWindow();
  const ImVec2 cursor = window->DC.CursorPos;
  const ImVec2 cursor_max = window->DC.CursorMaxPos;

  const ImVec2 at(left_middle.x, left_middle.y - shown.height * 0.5F);
  ImGui::SetCursorScreenPos(at);
  const bool clicked = ImGui::InvisibleButton(id, ImVec2(shown.width, shown.height));
  window->DrawList->AddImage(shown.id, at, ImVec2(at.x + shown.width, at.y + shown.height),
                             ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F), PaintedTint(IM_COL32_WHITE));
  SkinTooltip(tooltip);

  window->DC.CursorPos = cursor;
  window->DC.CursorMaxPos = cursor_max;
  return clicked;
}

bool FooterImageButton(const char* id, const SkinImage& out, const SkinImage& over,
                       const SkinImage& down, ImVec2 left_middle, const char* tooltip, float* width,
                       float art_scale) {
  // ⚠ LA TAILLE DESSINÉE, ET NON CELLE DU BITMAP : c'est elle que l'appelant a
  // retranchée du bord pour trouver le point de pose.
  const ImVec2 side(out.width * art_scale, out.height * art_scale);
  if (width != nullptr) {
    *width = out.usable() ? side.x : 0.0F;
  }
  if (!out.usable()) {
    return false;
  }
  ImGuiWindow* const window = ImGui::GetCurrentWindow();
  const ImVec2 cursor = window->DC.CursorPos;
  const ImVec2 cursor_max = window->DC.CursorMaxPos;

  // ⚠ LA TAILLE EST CELLE DU REPOS, quel que soit l'état montré : les trois
  // images font la même taille dans l'art du jeu, et prendre celle de l'état
  // courant ferait sauter le bouton d'un pixel au survol si elles divergeaient.
  const ImVec2 at(left_middle.x, left_middle.y - side.y * 0.5F);
  ImGui::SetCursorScreenPos(at);
  const bool clicked = ImGui::InvisibleButton(id, side);
  const SkinImage& shown = ImGui::IsItemActive() && down.usable()    ? down
                           : ImGui::IsItemHovered() && over.usable() ? over
                                                                     : out;
  window->DrawList->AddImage(shown.id, at, ImVec2(at.x + side.x, at.y + side.y), ImVec2(0.0F, 0.0F),
                             ImVec2(1.0F, 1.0F), PaintedTint(IM_COL32_WHITE));
  SkinTooltip(tooltip);

  window->DC.CursorPos = cursor;
  window->DC.CursorMaxPos = cursor_max;
  return clicked;
}

bool FooterButton(const char* label, const SkinArt& art, ImVec2 left_middle, float width) {
  ImGuiWindow* const window = ImGui::GetCurrentWindow();
  const ImVec2 cursor = window->DC.CursorPos;
  const ImVec2 cursor_max = window->DC.CursorMaxPos;

  ImGui::SetCursorScreenPos(
      ImVec2(left_middle.x, SkinSmallButtonTopCenteredOn(art, left_middle.y)));
  const bool clicked = SkinSmallButton(label, art, ImVec2(width, 0.0F));

  window->DC.CursorPos = cursor;
  window->DC.CursorMaxPos = cursor_max;
  return clicked;
}

float FooterHelpWidth() { return ImGui::CalcTextSize(kHelpMarkerText).x; }

void FooterHelp(const char* title, std::span<const char* const> lines, ImVec2 left_middle) {
  ImGuiWindow* const window = ImGui::GetCurrentWindow();
  const ImVec2 cursor = window->DC.CursorPos;
  const ImVec2 cursor_max = window->DC.CursorMaxPos;
  ImGui::SetCursorScreenPos(
      ImVec2(left_middle.x, left_middle.y - ImGui::GetTextLineHeight() * 0.5F));
  SkinHelpMarker(title, lines);
  window->DC.CursorPos = cursor;
  window->DC.CursorMaxPos = cursor_max;
}

// LE FOND PAVÉ, aligné sur la PREMIÈRE CASE et non sur le bord de la zone.
//
// ⚠ C'EST CET ALIGNEMENT QUI FAIT TENIR LE DESSIN : le fond et les objets
// partagent alors la même marge, et le joint de deux tuiles tombe sur le joint
// de deux cases. Aligné sur le bord, le pavage glisserait d'un ou deux pixels et
// la grille paraîtrait de travers.
void DrawTiledBackground(ImDrawList* into, const SkinImage& tile, ImVec2 origin, ImVec2 top_left,
                         ImVec2 bottom_right, const SkinLook& look) {
  if (!tile.usable()) {
    into->AddRectFilled(top_left, bottom_right, ImGui::GetColorU32(look.body));
    return;
  }
  const ImU32 tint = PaintedTint(look.item_grid_tint);
  into->PushClipRect(top_left, bottom_right, true);
  const float start_x = origin.x - std::ceil((origin.x - top_left.x) / tile.width) * tile.width;
  const float start_y = origin.y - std::ceil((origin.y - top_left.y) / tile.height) * tile.height;
  for (float y = start_y; y < bottom_right.y; y += tile.height) {
    for (float x = start_x; x < bottom_right.x; x += tile.width) {
      into->AddImage(tile.id, ImVec2(x, y), ImVec2(x + tile.width, y + tile.height),
                     ImVec2(0.0F, 0.0F), ImVec2(1.0F, 1.0F), tint);
    }
  }
  into->PopClipRect();
}

// Le badge d'une case : le raffinement, SINON la quantité, et jamais les deux.
void DrawBadge(ImDrawList* into, const InventoryCellView& cell, ImVec2 top_left,
               ImVec2 bottom_right) {
  std::array<char, 16> badge = {};
  if (cell.refine_level > 0) {
    std::snprintf(badge.data(), badge.size(), "+%d", cell.refine_level);
  } else if (cell.amount > 1) {
    std::snprintf(badge.data(), badge.size(), "%d", cell.amount);
  }
  if (badge[0] == '\0') {
    return;
  }

  ImFont* const font = ImGui::GetFont();
  const float base = ImGui::GetFontSize() * kItemBadgeScale;
  const float available = (bottom_right.x - top_left.x) - kBadgePadXPixels;
  ImVec2 size = font->CalcTextSizeA(base, FLT_MAX, 0.0F, badge.data());
  const float font_size = ItemBadgeFontSize(base, size.x, available);
  size = font->CalcTextSizeA(font_size, FLT_MAX, 0.0F, badge.data());

  // Aligné à droite, mais JAMAIS avant le bord gauche de sa case, et coupé à la
  // case : un nombre démesuré reste chez lui au lieu de brouiller son voisin.
  const ImVec2 at(std::max(top_left.x, bottom_right.x - size.x - kBadgePadXPixels),
                  bottom_right.y - size.y - kBadgePadYPixels);
  into->PushClipRect(top_left, bottom_right, true);
  // Cerne blanc, texte noir dessus : lisible sur n'importe quelle icône.
  for (int dx = -1; dx <= 1; ++dx) {
    for (int dy = -1; dy <= 1; ++dy) {
      if (dx == 0 && dy == 0) {
        continue;
      }
      into->AddText(font, font_size,
                    ImVec2(at.x + static_cast<float>(dx), at.y + static_cast<float>(dy)),
                    IM_COL32_WHITE, badge.data());
    }
  }
  into->AddText(font, font_size, at, IM_COL32_BLACK, badge.data());
  into->PopClipRect();
}

// L'icône, à sa taille d'art, centrée, RÉDUITE seulement si elle déborde.
//
// ⚠ JAMAIS AGRANDIE : une icône de 24 px étirée sur 32 devient floue, et le
// client ne le fait pas non plus.
void DrawIcon(ImDrawList* into, const InventoryCellView& cell, ImVec2 top_left, float cell_side) {
  if (!cell.icon.usable()) {
    const ImVec2 middle(top_left.x + cell_side * 0.5F, top_left.y + cell_side * 0.5F);
    const ImVec2 size = ImGui::CalcTextSize(kMissingIcon);
    into->AddText(ImVec2(middle.x - size.x * 0.5F, middle.y - size.y * 0.5F),
                  ImGui::GetColorU32(ImGuiCol_TextDisabled), kMissingIcon);
    return;
  }
  float width = cell.icon.width;
  float height = cell.icon.height;
  if (width > cell_side || height > cell_side) {
    const float shrink = cell_side / std::max(width, height);
    width *= shrink;
    height *= shrink;
  }
  const ImVec2 at(top_left.x + (cell_side - width) * 0.5F,
                  top_left.y + (cell_side - height) * 0.5F);
  into->AddImage(cell.icon.id, at, ImVec2(at.x + width, at.y + height), ImVec2(0, 0), ImVec2(1, 1),
                 PaintedTint(cell.damaged ? kItemDamagedTint : IM_COL32_WHITE));
}

// La largeur de la bande : la plus large des quatorze images.
//
// ⚠ PAS CELLE D'UNE SEULE, prise pour toutes. Les onglets n'ont aucune raison
// d'avoir la même largeur, et en choisir une au hasard rognerait les autres.
float ItemTabsThickness(const SkinArt& art, std::span<const ItemTab> tabs, TabStripLook look) {
  if (!look.images) {
    // COUCHÉE, le texte tient sur une ligne ; DEBOUT, la bande prend la largeur
    // du plus long libellé — c'est ce qui garantit qu'aucun ne se rogne.
    if (look.lying) {
      return ImGui::GetFrameHeight();
    }
    float widest = 0.0F;
    for (const ItemTab tab : tabs) {
      widest = std::max(widest, ImGui::CalcTextSize(ItemTabLabel(tab)).x);
    }
    return widest + 2.0F * ImGui::GetStyle().FramePadding.x;
  }
  float thickest = 0.0F;
  for (const ItemTab tab : tabs) {
    const TabLook pieces = PiecesFor(tab, look);
    const SkinImage& on = art.Get(pieces.on);
    const SkinImage& off = art.Get(pieces.off);
    thickest = std::max(thickest, look.lying ? on.height : on.width);
    thickest = std::max(thickest, look.lying ? off.height : off.width);
  }
  // Sans art, les onglets retombent sur le libellé : c'est sa hauteur de ligne
  // qui donne alors l'épaisseur.
  return thickest > 0.0F ? thickest : ImGui::GetFrameHeight();
}

// LA BANDE D'ONGLETS, dans l'une ou l'autre disposition.
//
// 🔴 DEBOUT, ELLE EST SON PROPRE ENFANT, DE LA MÊME HAUTEUR QUE LA GRILLE. Posée
// dans le flux de la fenêtre, elle commencerait à la hauteur du champ de filtre
// et la grille descendrait sans elle : les deux ne partageraient plus de bord,
// et l'onglet actif ne pourrait plus toucher ce qu'il ouvre.
//
// ⚠ COUCHÉE, ELLE N'EST PAS UN ENFANT : elle tient sur une ligne du flux et
// laisse toute la largeur à ce qui suit. Son espacement vertical est NUL, ce qui
// colle la grille sous elle — sans quoi le pont de l'onglet actif n'aurait rien
// à manger.
TabStripResult DrawItemTabs(std::span<const ItemTab> tabs, ItemTab current, const SkinArt& art,
                            TabStripLook look, float thickness, float length) {
  TabStripResult result;
  result.chosen = current;
  result.lying = look.lying;
  result.favorite_tinted = look.favorite_tinted;

  if (look.lying) {
    // ⚠ LE MÊME RECOUVREMENT D'UN PIXEL QUE LA BANDE DEBOUT, mais sur l'autre
    // axe : les onglets sont jointifs, et un espacement les ferait flotter.
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(kTabOverlapPixels, 0.0F));
    bool first = true;
    for (const ItemTab tab : tabs) {
      if (!first) {
        ImGui::SameLine();
      }
      first = false;
      const bool active = tab == current;
      NoteTab(DrawOneTab(tab, active, art, look, thickness), tab, active, result);
    }
    ImGui::PopStyleVar();
    return result;
  }

  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  const bool open =
      ImGui::BeginChild("onglets", ImVec2(thickness, length), ImGuiChildFlags_None,
                        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  if (open) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0F, kTabOverlapPixels));
    for (const ItemTab tab : tabs) {
      const bool active = tab == current;
      NoteTab(DrawOneTab(tab, active, art, look, thickness), tab, active, result);
    }
    ImGui::PopStyleVar();
  }
  ImGui::EndChild();
  ImGui::PopStyleVar();
  return result;
}

void DrawActiveTabBridge(ImDrawList* into, const TabStripResult& strip, const SkinLook& look) {
  if (!strip.active_shown) {
    return;
  }
  // ⚠ LE CORPS DES FAVORIS N'EST PAS TOUJOURS BLANC. `tab_fav` est gris-bleu, et
  // un pont blanc y trancherait au lieu de s'y fondre — le seul onglet dont le
  // pont se verrait comme un défaut. `tab_favs`, celui du coffre, est blanc comme
  // les autres : voir `TabStripLook::favorite_tinted`.
  const bool tinted = strip.chosen == ItemTab::kFavorite && strip.favorite_tinted;
  // ⚠ SOUS LA TEINTE DES ONGLETS, comme l'image dont il prolonge le corps.
  const ImU32 body =
      PaintedTint(MultiplyTint(tinted ? kFavoriteTabBodyColor : kTabBodyColor, look.item_tab_tint));
  if (strip.lying) {
    // COUCHÉE : le bord à manger est celui du BAS, sur la largeur de l'onglet
    // moins un pixel à chaque bout — sinon le pont dépasse et le liseré de
    // l'onglet paraît rogné.
    into->AddRectFilled(ImVec2(strip.active_min.x + kTabBridgeInsetPixels,
                               strip.active_max.y - kTabBridgeInsetPixels),
                        ImVec2(strip.active_max.x - kTabBridgeInsetPixels,
                               strip.active_max.y + kTabBridgeOverlapPixels),
                        body);
    return;
  }
  // DEBOUT : le bord à manger est celui de DROITE, sur la hauteur de l'onglet
  // moins un pixel à chaque bout.
  into->AddRectFilled(ImVec2(strip.active_max.x - kTabBridgeInsetPixels,
                             strip.active_min.y + kTabBridgeInsetPixels),
                      ImVec2(strip.active_max.x + kTabBridgeOverlapPixels,
                             strip.active_max.y - kTabBridgeInsetPixels),
                      body);
}

void StorageBlocksTransferTooltip() {
  SkinTooltip(
      core::Tr("Impossible tant que le storage est ouvert (règle du "
               "serveur).\nFermez le storage, ou faites transiter "
               "l'item par le storage."));
}

bool MouseOverWindow(const char* window_id) {
  const ImGuiWindow* const window = ImGui::FindWindowByName(window_id);
  // ⚠ `WasActive` ET NON `Active` : une fenêtre n'est « active » qu'entre son
  // `Begin` et son `End`, et la question se pose ailleurs dans l'image.
  return window != nullptr && window->WasActive && window->Rect().Contains(ImGui::GetMousePos());
}

void DragItemFrom(const InventoryCellView& cell, ItemHolder mine, ImVec2 padding,
                  const char* refusal) {
  // ⚠ LA MARGE SE POUSSE AVANT LE `Begin` : le fantôme est une infobulle, et il
  // hérite du `WindowPadding` en vigueur AU MOMENT OÙ IL S'OUVRE — celui, nul,
  // qui rend les tuiles de la grille jointives.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
  if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
    ImGui::PopStyleVar();
    return;
  }
  const ItemDrag carried{.item_id = cell.item_id,
                         .client_slot = cell.client_slot,
                         .from = mine,
                         .worn = cell.worn,
                         .card = cell.card};
  ImGui::SetDragDropPayload(kItemDragPayload, &carried, sizeof(carried));
  // L'ICÔNE PUIS LE NOM, comme au client : c'est l'objet qu'on TIENT qu'il faut
  // reconnaître, et un nom seul ne se reconnaît pas d'un coup d'œil.
  if (cell.icon.usable()) {
    const ImVec2 at = ImGui::GetCursorScreenPos();
    ImGui::Dummy(ImVec2(kDragGhostIconPixels, kDragGhostIconPixels));
    DrawIcon(ImGui::GetWindowDrawList(), cell, at, kDragGhostIconPixels);
    ImGui::SameLine();
  }
  Unformatted(cell.name);
  // 🔴 LE REFUS SE DIT PENDANT LE GLISSER, seul moment où l'on peut encore
  // renoncer. Après le lâcher, il ne reste qu'un geste qui n'a rien fait.
  if (refusal != nullptr) {
    ImGui::TextColored(kDragRefusalColor, "%s", refusal);
  }
  ImGui::EndDragDropSource();
  ImGui::PopStyleVar();
}

ItemTransferAsk AcceptItemDrop(ItemHolder mine, bool same) {
  ItemTransferAsk asked;
  if (!ImGui::BeginDragDropTarget()) {
    return asked;
  }
  if (const ImGuiPayload* dropped = ImGui::AcceptDragDropPayload(kItemDragPayload);
      dropped != nullptr && dropped->DataSize == static_cast<int>(sizeof(ItemDrag))) {
    if (const auto& carried = *static_cast<const ItemDrag*>(dropped->Data);
        same || carried.from != mine) {
      asked.from = carried.from;
      asked.to = mine;
      asked.client_slot = carried.client_slot;
    }
  }
  ImGui::EndDragDropTarget();
  return asked;
}

void DrawCellTooltip(const InventoryCellView& cell, ImVec2 padding) {
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
  if (BeginSkinTooltip()) {
    Unformatted(cell.name);
    if (cell.amount > 1) {
      std::array<char, 64> quantity = {};
      std::snprintf(quantity.data(), quantity.size(), core::Tr("Quantité : %d"), cell.amount);
      ImGui::TextDisabled("%s", quantity.data());
    }
    EndSkinTooltip();
  }
  ImGui::PopStyleVar();
}

// Cale la taille demandée sur un nombre ENTIER de cases, dans les deux sens, et
// jamais sous le plancher de cinq.
//
// ⚠ LA MESURE VIENT DE L'IMAGE PRÉCÉDENTE, portée par l'état que l'appelant nous
// passe. C'est le fonctionnement normal d'une interface en mode immédiat : au
// tout premier redimensionnement le chrome vaut zéro, et la fenêtre se cale une
// image plus tard — ce qui ne se voit pas.
QuantityAnswer DrawQuantityPrompt(std::span<const InventoryCellView> cells, ItemQuantityAsk& ask,
                                  const char* confirm, const SkinArt& art) {
  QuantityAnswer answer;
  if (ask.slot == 0) {
    return answer;
  }
  // La place visée existe-t-elle encore ? Le temps de répondre, l'objet a pu
  // partir — vendu, consommé, échangé.
  const InventoryCellView* target = nullptr;
  for (const InventoryCellView& cell : cells) {
    if (cell.client_slot == ask.slot) {
      target = &cell;
      break;
    }
  }
  if (target == nullptr) {
    ask.slot = 0;
    return answer;
  }
  const AmountAnswer given = DrawCountPrompt(ask.amount, target->amount, confirm, art);
  if (given.chosen) {
    answer.slot = ask.slot;
    answer.amount = given.amount;
  }
  if (given.closed) {
    ask.slot = 0;
  }
  return answer;
}

AmountAnswer DrawCountPrompt(int& amount, int most, const char* confirm, const SkinArt& art) {
  AmountAnswer answer;
  ImGui::OpenPopup(kQuantityPromptId);
  // 🔴 SOUS LE CURSEUR, ET NON AU CENTRE DE L'ÉCRAN : le prompt suit le clic qui
  // l'a déclenché. Au centre, il oblige l'œil et la souris à traverser l'écran
  // pour saisir un nombre, puis à revenir.
  ImGui::SetNextWindowPos(ImGui::GetMousePos(), ImGuiCond_Appearing);
  // 🔴 MODALE SANS VOILE. `BeginPopupModal` assombrit tout l'écran derrière lui,
  // et c'est disproportionné pour saisir un nombre : la fenêtre d'où l'on vient
  // disparaît sous un gris, alors qu'on lui prend une pile. On garde le BLOCAGE
  // — un clic à côté ne doit rien déclencher ailleurs — et l'on efface le voile.
  ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, IM_COL32(0, 0, 0, 0));
  const bool prompt_open = ImGui::BeginPopupModal(
      kQuantityPromptId, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar);
  ImGui::PopStyleColor();
  if (!prompt_open) {
    return answer;
  }
  // 🔴 LE DÉFAUT EST LA PILE ENTIÈRE, et c'est le cas courant : on déplace une
  // pile, on n'en prélève un morceau que par exception. Le champ s'ouvre
  // SÉLECTIONNÉ — taper un nombre le remplace —, si bien que le cas rare coûte
  // une saisie et que le cas courant n'en coûte aucune.
  //
  // ⛔ LE JET AUSSI, bien que « jeter tout par mégarde ne se rattrape pas » :
  // le nombre est SOUS LES YEUX et il faut encore valider. Ce qui ne se
  // rattrape pas se protège par le verrou de jet, pas par un défaut qui gêne
  // les trois autres verbes.
  //
  // ⚠ C'EST LE PROMPT QUI POSE CE DÉFAUT, et non l'appelant : lui ne connaît pas
  // toujours la taille de la pile au moment où il arme la question.
  const bool appearing = ImGui::IsWindowAppearing();
  if (appearing) {
    amount = most;
  }
  ImGui::Text(core::Tr("%s combien ? (max %d)"), confirm, most);

  // ⚠ `InputInt` SANS PAS, et les deux boutons posés à la main : les +/- qu'il
  // dessine tout seul sont ceux d'ImGui, que l'habillage ne touche pas — deux
  // carrés gris au milieu d'une fenêtre du jeu.
  if (SkinButton("-", art)) {
    amount -= 1;
  }
  ImGui::SameLine(0.0F, kQuantityStepGapPixels);
  if (appearing) {
    // ⚠ VISE L'ÉLÉMENT SUIVANT, donc posé AVANT lui.
    ImGui::SetKeyboardFocusHere();
  }
  ImGui::SetNextItemWidth(ImGui::GetFontSize() * kQuantityFieldEms);
  ImGui::InputInt("##combien", &amount, 0, 0);
  ImGui::SameLine(0.0F, kQuantityStepGapPixels);
  if (SkinButton("+", art)) {
    amount += 1;
  }
  amount = std::clamp(amount, 1, std::max(1, most));

  if (SkinButton(confirm, art) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) ||
      ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
    answer.chosen = true;
    answer.amount = amount;
    answer.closed = true;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  // ⚠ « TOUT » EXISTE MÊME AVEC LA PILE ENTIÈRE POUR DÉFAUT : on y revient après
  // avoir tapé un nombre, et c'est le geste qu'il évite de refaire à l'envers.
  if (SkinButton(core::Tr("Tout"), art)) {
    answer.chosen = true;
    answer.amount = most;
    answer.closed = true;
    ImGui::CloseCurrentPopup();
  }
  ImGui::SameLine();
  if (SkinButton(core::Tr("Annuler"), art) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
    answer.closed = true;
    ImGui::CloseCurrentPopup();
  }
  ImGui::EndPopup();
  return answer;
}

float GridScrollbarGutter() {
  // ⚠ IL N'Y A PAS D'AUTRE FAÇON DE LE DEMANDER : `GetStyle().ScrollbarSize`
  // donne la largeur d'un ascenseur, jamais s'il en paraît un, et
  // `GetContentRegionAvail` dépend d'où le curseur en est.
  const ImGuiWindow* const here = ImGui::GetCurrentWindow();
  return here == nullptr ? 0.0F : here->ScrollbarSizes.x;
}

ImGuiWindowFlags GridScrollFlags(bool size_locked) {
  return size_locked ? ImGuiWindowFlags_AlwaysVerticalScrollbar : ImGuiWindowFlags_None;
}

void SnapToCells(ImGuiSizeCallbackData* asked) {
  const auto* chrome = static_cast<const ImVec2*>(asked->UserData);
  if (chrome == nullptr) {
    return;
  }
  const float step = kItemCellPixels + kCellGapPixels;
  const auto cages = [step](float room) {
    const int count = static_cast<int>((room + kCellGapPixels) / step + 0.5F);
    return count < kItemGridMinimumColumns ? kItemGridMinimumColumns : count;
  };
  const int columns = cages(asked->DesiredSize.x - chrome->x);
  const int rows = cages(asked->DesiredSize.y - chrome->y);
  asked->DesiredSize.x = chrome->x + static_cast<float>(columns) * step - kCellGapPixels;
  asked->DesiredSize.y = chrome->y + static_cast<float>(rows) * step - kCellGapPixels;
}

}  // namespace ro::ui
