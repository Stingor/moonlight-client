#include "ro/ui/skin_palette.h"

#include <algorithm>
#include <array>
#include <tuple>
#include <utility>

#include "ui_color.h"

namespace ro::ui {
namespace {

constexpr int kHoverLift = 10;
constexpr int kActiveLift = 22;
constexpr int kTabHoverLift = 14;

// Ce que rien ne doit peindre, parce que NOUS le peignons : la barre de titre,
// l'ascenseur, la poignée de redimensionnement.
constexpr ImU32 kPaintedByUs = IM_COL32(0, 0, 0, 0);

// ⚠ UNE LISTE, ET LE COMPTE S'EN DÉDUIT. Écrire « 24 » à l'autre bout du fichier
// est ce qui laisse une couleur posée pour toute la suite de l'image le jour où
// l'on en ajoute une vingt-cinquième — les fenêtres d'après perdent alors leur
// aspect, et rien ne dit pourquoi.
[[nodiscard]] auto PaletteOf(const SkinLook& look) {
  return std::to_array<std::pair<ImGuiCol, ImU32>>({
      {ImGuiCol_WindowBg, look.body},
      {ImGuiCol_ChildBg, kPaintedByUs},
      {ImGuiCol_Border, look.border},
      {ImGuiCol_Text, look.text},
      // LE TEXTE SECONDAIRE ET LES LIENS : ce qui les écrit les lit ici, et non
      // dans une constante — c'est ce qui les fait suivre l'habillage.
      {ImGuiCol_TextDisabled, look.faint_text},
      {ImGuiCol_TextLink, look.link},
      // La barre de titre d'ImGui s'efface : l'art prend sa place.
      {ImGuiCol_TitleBg, kPaintedByUs},
      {ImGuiCol_TitleBgActive, kPaintedByUs},
      {ImGuiCol_TitleBgCollapsed, kPaintedByUs},
      // ⚠ EFFACÉ NE VEUT PAS DIRE ÉTEINT : l'ascenseur et la poignée restent
      // vivants dessous, avec leur clic, leur glisser et leur roulette. C'est leur
      // IMAGE qu'on remplace.
      {ImGuiCol_ScrollbarBg, kPaintedByUs},
      {ImGuiCol_ScrollbarGrab, kPaintedByUs},
      {ImGuiCol_ScrollbarGrabHovered, kPaintedByUs},
      {ImGuiCol_ScrollbarGrabActive, kPaintedByUs},
      {ImGuiCol_ResizeGrip, kPaintedByUs},
      {ImGuiCol_ResizeGripHovered, kPaintedByUs},
      {ImGuiCol_ResizeGripActive, kPaintedByUs},
      {ImGuiCol_FrameBg, look.input},
      {ImGuiCol_FrameBgHovered, Lighter(look.input, kHoverLift)},
      {ImGuiCol_FrameBgActive, Lighter(look.input, kActiveLift)},
      {ImGuiCol_Tab, look.idle_tab},
      {ImGuiCol_TabHovered, Lighter(look.idle_tab, kTabHoverLift)},
      {ImGuiCol_TabSelected, look.active_tab},
      {ImGuiCol_TableHeaderBg, look.table_header},
      {ImGuiCol_Header, look.selection},
      {ImGuiCol_HeaderHovered, look.SelectionHover()},
      {ImGuiCol_HeaderActive, look.SelectionActive()},
      {ImGuiCol_PopupBg, look.popup},
      // LE REMPLISSAGE D'UNE JAUGE. ⚠ SANS LUI, ImGui la peint en JAUNE VIF —
      // sa couleur d'histogramme —, ce qui sur le corps presque blanc d'une
      // fenêtre du jeu se voit à l'autre bout de l'écran. On reprend le bleu de
      // l'onglet actif, qui est celui du client.
      {ImGuiCol_PlotHistogram, look.active_tab},
  });
}

// Les mesures. Toutes suivent l'échelle de l'interface SAUF l'épaisseur du
// cadre — voir plus bas.
[[nodiscard]] auto MeasuresOf(const SkinLook& look) {
  return std::to_array<std::pair<ImGuiStyleVar, float>>({
      {ImGuiStyleVar_WindowRounding, look.rounding},
      {ImGuiStyleVar_FrameRounding, look.rounding},
      {ImGuiStyleVar_PopupRounding, kSkinPopupRounding},
      {ImGuiStyleVar_GrabRounding, kSkinGrabRounding},
      {ImGuiStyleVar_ScrollbarSize, kSkinScrollbarWidth},
      // ⚠ L'ASCENSEUR N'EST PAS ARRONDI : ses pièces sont des bitmaps à angles
      // droits, et un arrondi les ferait dépasser de leur rail.
      {ImGuiStyleVar_ScrollbarRounding, 0.0F},
  });
}

// L'opacité la plus basse : en dessous, une fenêtre ne se lit plus.
constexpr float kLeastOpacity = 0.3F;

}  // namespace

float SkinFramePaddingY(const SkinArt& art) {
  const float bar = art.Get(render::SkinPiece::kTitleBarLeft).height;
  if (bar <= 0.0F) {
    // Sans art, on ne touche à rien : l'interface est déjà celle d'ImGui.
    return ImGui::GetStyle().FramePadding.y;
  }
  // ⚠ L'ART PASSE PAR L'ÉCHELLE, LA POLICE NON : elle suit déjà `FontScaleMain`.
  // Les deux grandissent donc du même facteur, et la densité tient à toute
  // échelle. Jamais négatif : une police plus haute que la barre donnerait un
  // remplissage inversé, et ImGui n'en veut pas.
  const float scale = ImGui::GetStyle().FontScaleMain;
  return std::max(0.0F, (bar * scale - ImGui::GetFontSize()) * 0.5F);
}

SkinStylePushed PushSkinStyle(const SkinArt& art) {
  const SkinLook& look = art.look();
  const auto palette = PaletteOf(look);
  for (const auto& [which, color] : palette) {
    ImGui::PushStyleColor(which, color);
  }
  const float scale = ImGui::GetStyle().FontScaleMain;
  const auto measures = MeasuresOf(look);
  for (const auto& [which, measure] : measures) {
    ImGui::PushStyleVar(which, measure * scale);
  }
  // L'OPACITÉ DE L'HABILLAGE, par-dessus celle de l'endroit : une zone grisée
  // reste grisée.
  ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                      ImGui::GetStyle().Alpha * std::clamp(look.opacity, kLeastOpacity, 1.0F));
  // LA DENSITÉ DU JEU : voir `SkinFramePaddingY`. ⚠ SEUL LE VERTICAL CHANGE —
  // l'horizontal dit ce qui sépare un texte du bord de son cadre, et le jeu
  // n'y serre rien.
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                      ImVec2(ImGui::GetStyle().FramePadding.x, SkinFramePaddingY(art)));
  // ⚠ LE CADRE RESTE À UN PIXEL À TOUTE ÉCHELLE, seule exception. Les autres
  // mesures bornent des surfaces d'art ; celle-ci est un TRAIT, et l'interface
  // le dessine adouci : à cent cinquante pour cent, un pixel et demi donne un
  // liseré baveux là où un pixel reste net. Un cadre fin sur une grande fenêtre
  // ne choque pas.
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0F);
  return SkinStylePushed{.colors = static_cast<int>(palette.size()),
                         .vars = static_cast<int>(measures.size()) + 3};
}

void PushPaperStyle(const SkinLook& look) {
  const auto paper = std::to_array<std::pair<ImGuiCol, ImU32>>({
      {ImGuiCol_WindowBg, look.paper},
      {ImGuiCol_PopupBg, look.paper},
      {ImGuiCol_Text, look.paper_text},
      {ImGuiCol_TextDisabled, ReadableOn(look.faint_text, look.paper)},
      {ImGuiCol_TextLink, ReadableOn(look.link, look.paper)},
  });
  static_assert(std::tuple_size_v<decltype(paper)> == kPaperStyleColors);
  for (const auto& [which, color] : paper) {
    ImGui::PushStyleColor(which, color);
  }
}

void PopSkinStyle(SkinStylePushed pushed) {
  if (pushed.vars > 0) {
    ImGui::PopStyleVar(pushed.vars);
  }
  if (pushed.colors > 0) {
    ImGui::PopStyleColor(pushed.colors);
  }
}

}  // namespace ro::ui
