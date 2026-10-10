// LA BARRE D'ONGLETS DU JEU.
//
// ⚠ L'ART SE PEINT PAR-DESSUS CEUX D'IMGUI, il ne les remplace pas. On garde la
// mécanique entière — choix, survol, défilement, ordre — et l'on efface
// seulement ses fonds ; ce qui se voit est alors le nôtre, et ce qui répond au
// clic est le sien. C'est la même division que pour la barre de titre.

#include <array>
#include <cmath>

#include <imgui_internal.h>

#include "ro/ui/skin_palette.h"
#include "ro/ui/skin_widgets.h"

namespace ro::ui {
namespace {

// Le coin d'un onglet, en pixels de l'art : au-delà, les bords s'étirent.
constexpr float kTabCorner = 3.0F;
// Le filet d'un pixel qui ferme l'onglet. ⚠ EN PIXELS DE L'ART, pas de l'écran.
constexpr float kTabEdge = 1.0F;
// L'arrondi du fond, sous l'art.
constexpr float kTabFillRounding = 2.0F;
// De combien le survol éclaircit un onglet fermé, par canal.
constexpr int kTabHoverLift = 14;

// Ce qu'ImGui ne doit plus peindre d'un onglet, parce que nous le peignons.
//
// ⚠ UNE LISTE, ET LE COMPTE S'EN DÉDUIT : le séparateur de bas de barre reprend
// `TabSelected`, il disparaît donc avec eux, et c'est notre filet qui le
// remplace. En oublier un laisse un rectangle bleu sous l'art.
constexpr auto kTabColors = std::to_array<ImGuiCol>(
    {ImGuiCol_Tab, ImGuiCol_TabHovered, ImGuiCol_TabSelected, ImGuiCol_TabDimmed,
     ImGuiCol_TabDimmedSelected, ImGuiCol_TabSelectedOverline, ImGuiCol_TabDimmedSelectedOverline});

[[nodiscard]] float Scale() { return ImGui::GetStyle().FontScaleMain; }

[[nodiscard]] ImU32 Lighter(ImU32 color, int by) {
  const auto clamp = [](int channel) { return channel > 255 ? 255 : channel; };
  return IM_COL32(clamp(static_cast<int>((color >> IM_COL32_R_SHIFT) & 0xFF) + by),
                  clamp(static_cast<int>((color >> IM_COL32_G_SHIFT) & 0xFF) + by),
                  clamp(static_cast<int>((color >> IM_COL32_B_SHIFT) & 0xFF) + by),
                  static_cast<int>((color >> IM_COL32_A_SHIFT) & 0xFF));
}

// Peint un MORCEAU d'une pièce : un rectangle de l'écran, un rectangle de la
// texture.
//
// ⚠ DEUX UNITÉS ICI, À NE JAMAIS CONFONDRE. Les coordonnées de DESTINATION sont
// des pixels d'écran, donc à l'échelle de l'interface ; celles de SOURCE sont
// des pixels de la texture et n'y touchent pas. Les mélanger échantillonne à
// côté du coin.
//
// ⚠ SOUS LA TEINTE DES IMAGES : le filet d'un onglet est de l'art, et un
// habillage sombre le veut aussi sombre que le cadre des fenêtres.
void BlitPart(ImDrawList* into, const SkinArt& art, const SkinImage& image, ImVec2 to_min,
              ImVec2 to_max, ImVec2 from_min, ImVec2 from_max) {
  if (!image.usable() || to_max.x <= to_min.x || to_max.y <= to_min.y) {
    return;
  }
  const ImVec2 uv_min(from_min.x / image.width, from_min.y / image.height);
  const ImVec2 uv_max(from_max.x / image.width, from_max.y / image.height);
  into->AddImage(image.id, to_min, to_max, uv_min, uv_max, PaintedTint(art.look().ArtTint()));
}

// Peint UN onglet : son fond, puis son contour.
void PaintTab(ImDrawList* into, const SkinArt& art, ImVec2 top_left, ImVec2 bottom_right,
              bool selected, bool hovered) {
  const SkinImage& left =
      art.Get(selected ? render::SkinPiece::kTabLeftOn : render::SkinPiece::kTabLeftOff);
  const SkinImage& middle =
      art.Get(selected ? render::SkinPiece::kTabMiddleOn : render::SkinPiece::kTabMiddleOff);
  const SkinImage& right =
      art.Get(selected ? render::SkinPiece::kTabRightOn : render::SkinPiece::kTabRightOff);

  ImU32 fill = selected ? art.look().active_tab : art.look().idle_tab;
  if (!selected && hovered) {
    fill = Lighter(fill, kTabHoverLift);
  }
  const float scale = Scale();
  const float corner = kTabCorner * scale;
  const float edge = kTabEdge * scale;
  // ⚠ L'ONGLET OUVERT DESCEND JUSQU'EN BAS : il se fond dans le contenu. Le
  // fermé s'arrête un pixel plus haut, là où son art referme le bord — c'est ce
  // seul pixel qui distingue les deux d'un coup d'œil.
  const float fill_bottom = selected ? bottom_right.y : bottom_right.y - edge;
  if (fill_bottom > top_left.y + edge) {
    into->AddRectFilled(ImVec2(top_left.x + edge, top_left.y + edge),
                        ImVec2(bottom_right.x - edge, fill_bottom), fill, kTabFillRounding * scale,
                        ImDrawFlags_RoundCornersTop);
  }
  if (!left.usable() || !middle.usable() || !right.usable()) {
    // Pièce absente de l'installation : le fond seul reste lisible.
    return;
  }

  BlitPart(into, art, left, top_left, ImVec2(top_left.x + corner, top_left.y + corner),
           ImVec2(0.0F, 0.0F), ImVec2(kTabCorner, kTabCorner));
  BlitPart(into, art, left, ImVec2(top_left.x, top_left.y + corner),
           ImVec2(top_left.x + edge, bottom_right.y), ImVec2(0.0F, kTabCorner),
           ImVec2(kTabEdge, left.height));
  BlitPart(into, art, right, ImVec2(bottom_right.x - corner, top_left.y),
           ImVec2(bottom_right.x, top_left.y + corner), ImVec2(right.width - kTabCorner, 0.0F),
           ImVec2(right.width, kTabCorner));
  BlitPart(into, art, right, ImVec2(bottom_right.x - edge, top_left.y + corner),
           ImVec2(bottom_right.x, bottom_right.y), ImVec2(right.width - kTabEdge, kTabCorner),
           ImVec2(right.width, right.height));
  BlitPart(into, art, middle, ImVec2(top_left.x + corner, top_left.y),
           ImVec2(bottom_right.x - corner, top_left.y + edge), ImVec2(0.0F, 0.0F),
           ImVec2(middle.width, kTabEdge));
  if (!selected) {
    BlitPart(into, art, middle, ImVec2(top_left.x, bottom_right.y - edge), bottom_right,
             ImVec2(0.0F, middle.height - kTabEdge), ImVec2(middle.width, middle.height));
  }
}

// Peint toute la rangée, et le trait qui la prolonge.
void PaintTabBar(const SkinArt& art) {
  ImGuiTabBar* bar = ImGui::GetCurrentTabBar();
  const ImGuiContext& context = *ImGui::GetCurrentContext();
  if (bar == nullptr || bar->BarRect.Max.x <= bar->BarRect.Min.x) {
    return;
  }
  ImDrawList* into = ImGui::GetWindowDrawList();
  into->PushClipRect(bar->BarRect.Min, bar->BarRect.Max, true);
  // ⚠ LES BORNES REVIENNENT À L'ENTIER. Un filet d'un pixel posé sur une
  // coordonnée fractionnaire ressort décalé — les textures sont échantillonnées
  // au plus proche, et une demi-abscisse tombe entre deux texels.
  const float top = std::floor(bar->BarRect.Min.y);
  const float bottom = std::floor(bar->BarRect.Max.y);
  float painted_to = std::floor(bar->BarRect.Min.x);

  for (const ImGuiTabItem& tab : bar->Tabs) {
    // ⚠ SEULEMENT CEUX SOUMIS À L'IMAGE PRÉCÉDENTE. Une entrée survit un moment à
    // l'onglet qui a cessé d'être émis, et la peindre laisserait un onglet
    // fantôme là où ImGui, lui, ne dessine plus rien.
    if (tab.LastFrameVisible + 1 < context.FrameCount) {
      continue;
    }
    if ((tab.Flags & ImGuiTabItemFlags_Invisible) != 0) {
      continue;
    }
    const float left = std::floor(bar->BarRect.Min.x + IM_TRUNC(tab.Offset - bar->ScrollingAnim));
    const float right = std::floor(left + tab.Width);
    if (right - left < kTabCorner * Scale() * 2.0F) {
      continue;  // trop étroit pour ses deux coins
    }
    // `VisibleTabId` et non `SelectedTabId` : c'est l'onglet dont le CONTENU est
    // montré, aperçu au clavier compris.
    PaintTab(into, art, ImVec2(left, top), ImVec2(right, bottom), bar->VisibleTabId == tab.ID,
             context.HoveredIdPreviousFrame == tab.ID);
    painted_to = std::max(painted_to, right);
  }

  // La rangée se prolonge jusqu'au bord du panneau, comme dans le client : les
  // onglets fermés portent déjà ce filet, il ne manque que l'après-dernier.
  const SkinImage& middle = art.Get(render::SkinPiece::kTabMiddleOff);
  const float edge = kTabEdge * Scale();
  BlitPart(into, art, middle, ImVec2(painted_to, bottom - edge), ImVec2(bar->BarRect.Max.x, bottom),
           ImVec2(0.0F, middle.height - kTabEdge), ImVec2(middle.width, middle.height));
  into->PopClipRect();
}

}  // namespace

bool BeginSkinTabBar(const char* id, const SkinArt& art, ImGuiTabBarFlags flags) {
  for (const ImGuiCol which : kTabColors) {
    ImGui::PushStyleColor(which, IM_COL32(0, 0, 0, 0));
  }
  if (!ImGui::BeginTabBar(id, flags)) {
    ImGui::PopStyleColor(static_cast<int>(kTabColors.size()));
    return false;
  }
  // ⚠ PEINT ICI, AVANT QUE LES ONGLETS NE SOIENT SOUMIS : leur libellé se pose
  // alors PAR-DESSUS l'art. Peindre à la fin le recouvrirait.
  //
  // La géométrie est donc celle de l'image précédente — c'est justement celle
  // que le joueur a sous les yeux au moment où il vise.
  PaintTabBar(art);
  return true;
}

void EndSkinTabBar() {
  ImGui::EndTabBar();
  ImGui::PopStyleColor(static_cast<int>(kTabColors.size()));
}

}  // namespace ro::ui
