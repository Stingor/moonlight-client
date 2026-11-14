// LA BULLE DU JEU SANS SON HABILLAGE EN MAIN : elle prend celui de la fenêtre en
// cours, et hors de toute fenêtre habillée elle garde le corps clair du jeu.

#include "ro/ui/skin_tooltip.h"

#include <cmath>
#include <cstddef>

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/skin_art.h"
#include "ro/ui/skinned_window.h"
#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
constexpr float kPiecePixels = 8.0F;

// Un habillage complet. Les identifiants ne désignent aucune texture : rien
// n'est peint ici.
[[nodiscard]] ro::ui::SkinArt WholeArt() {
  ro::ui::SkinArt art;
  for (std::size_t rank = 0; rank < ro::render::kSkinPieceCount; ++rank) {
    art.Set(static_cast<ro::render::SkinPiece>(rank),
            ro::ui::SkinImage{.id = 1, .width = kPiecePixels, .height = kPiecePixels});
  }
  return art;
}

[[nodiscard]] bool SameColor(const ImVec4& color, ImU32 expected) {
  return ImGui::ColorConvertFloat4ToU32(color) == expected;
}

}  // namespace

TEST_CASE("Bulle du jeu : hors fenêtre habillée, le corps clair et le texte noir",
          "[ui][tooltip]") {
  // 🔴 C'EST LE DÉFAUT QUE LE JOUEUR A VU : une bulle ouverte hors des fenêtres
  // habillées prenait le fond SOMBRE d'ImGui, et le texte du jeu, écrit pour le
  // clair, y disparaissait.
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  CHECK(ro::ui::PaintingSkinArt() == nullptr);
  REQUIRE(ro::ui::BeginSkinTooltip());
  const ImGuiStyle& style = ImGui::GetStyle();
  CHECK(SameColor(style.Colors[ImGuiCol_PopupBg], ro::ui::kSkinnedTooltipBodyColor));
  CHECK(SameColor(style.Colors[ImGuiCol_Text], ro::ui::kSkinnedTooltipTextColor));
  ro::ui::EndSkinTooltip();
  // ⚠ ET TOUT CE QUI A ÉTÉ POSÉ EST RETIRÉ : le texte suivant n'est plus noir.
  CHECK_FALSE(SameColor(style.Colors[ImGuiCol_Text], ro::ui::kSkinnedTooltipTextColor));
  ui.EndFrame();
}

TEST_CASE("Bulle du jeu : sa marge ne vient pas de ce qui l'entoure", "[ui][tooltip]") {
  // 🔴 SIGNALÉ PAR LE JOUEUR SUR LA BARRE DE RACCOURCIS : elle pose une marge
  // nulle et un interligne serré pour ses cases, et la bulle ouverte au milieu
  // en héritait — le texte collé à ses bords.
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0F, 0.0F));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0F, 0.0F));
  REQUIRE(ro::ui::BeginSkinTooltip());
  const float scale = ImGui::GetStyle().FontScaleMain;
  CHECK(ImGui::GetStyle().WindowPadding.x == ro::ui::kBareTooltipPaddingPixels * scale);
  CHECK(ImGui::GetStyle().WindowPadding.y == ro::ui::kBareTooltipPaddingPixels * scale);
  CHECK(ImGui::GetStyle().ItemSpacing.y == ro::ui::kBareTooltipLineGapPixels * scale);
  ro::ui::EndSkinTooltip();
  // Et ce que l'appelant avait posé lui revient.
  CHECK(ImGui::GetStyle().WindowPadding.x == 0.0F);
  ImGui::PopStyleVar(2);
  ui.EndFrame();
}

TEST_CASE("Bulle du jeu : dans une fenêtre habillée, elle en prend l'habillage", "[ui][tooltip]") {
  ro::ui::UiContext ui;
  ro::ui::SkinArt art = WholeArt();
  REQUIRE(art.dresses_windows());
  // Son fond et son texte sont ceux que le joueur a réglés.
  ro::ui::SkinLook look;
  look.paper = IM_COL32(40, 42, 50, 255);
  look.paper_text = IM_COL32(232, 234, 240, 255);
  art.SetLook(look);
  ui.BeginFrame(kViewport, kFrameSeconds);
  if (ro::ui::BeginSkinnedWindow("essai", nullptr, art)) {
    CHECK(ro::ui::PaintingSkinArt() == &art);
    REQUIRE(ro::ui::BeginSkinTooltip());
    // 🔴 LE CADRE DU JEU SE PEINT SUR UN SECOND CANAL, sous le contenu : c'est
    // la marque de l'infobulle habillée, que le corps nu n'ouvre pas.
    CHECK(ImGui::GetWindowDrawList()->_Splitter._Count == 2);
    const ImGuiStyle& style = ImGui::GetStyle();
    CHECK(SameColor(style.Colors[ImGuiCol_PopupBg], look.paper));
    CHECK(SameColor(style.Colors[ImGuiCol_Text], look.paper_text));
    ro::ui::EndSkinTooltip();
    // L'habillage réglé se lit sans l'art en main.
    CHECK(ro::ui::PaintingLook().paper == look.paper);
  }
  ro::ui::EndSkinnedWindow();
  // La fenêtre fermée, plus d'habillage à prêter : celui d'origine répond.
  CHECK(ro::ui::PaintingSkinArt() == nullptr);
  CHECK(ro::ui::PaintingLook() == ro::ui::SkinLook{});
  ui.EndFrame();
}

TEST_CASE("Bulle du jeu : le texte se replie à la largeur des bulles", "[ui][tooltip]") {
  ro::ui::UiContext ui;
  // ⚠ DEUX IMAGES : une bulle d'ImGui ne connaît la taille de son contenu qu'à
  // l'image qui suit son apparition.
  for (int frame = 0; frame < 2; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::Begin("porteur");
    ro::ui::SkinTooltipFormat("%s",
                              "Une aide assez longue pour dépasser la largeur d'une bulle du jeu, "
                              "qui doit donc passer à la ligne au lieu de traverser l'écran.");
    ImGui::End();
    ui.EndFrame();
  }
  const ImGuiWindow* bubble = ImGui::FindWindowByName("##Tooltip_00");
  REQUIRE(bubble != nullptr);
  CHECK(bubble->Size.x <= ro::ui::kSkinnedTooltipWidest);
  // ⚠ LA BORNE SEULE NE SUFFIT PAS : elle rétrécit la bulle, et le texte non
  // replié déborderait au-delà, coupé. C'est la hauteur qui dit le repli.
  CHECK(bubble->ContentSize.y > 2.0F * ImGui::GetFontSize());
}

TEST_CASE("Signe d'avertissement : il tient dans son carré, l'encre dans le triangle",
          "[ui][tooltip]") {
  // 🔴 LE CÔTÉ EST CELUI D'UNE LIGNE DE TEXTE : un signe qui déborde de son
  // carré chevaucherait la ligne d'au-dessus, ou le texte posé à sa droite.
  constexpr ImVec2 kTopLeft(100.0F, 50.0F);
  constexpr float kSide = 20.0F;
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin("porteur");
  ImDrawList& into = *ImGui::GetWindowDrawList();
  const int first = into.VtxBuffer.Size;
  ro::ui::PaintWarningMark(into, kTopLeft, kSide);
  int fill = 0;
  int ink = 0;
  for (int index = first; index < into.VtxBuffer.Size; ++index) {
    const ImDrawVert& vertex = into.VtxBuffer[index];
    if (vertex.col == ro::ui::kSkinWarningFill) {
      ++fill;
      // ⚠ L'ANTICRÉNELAGE FRANGE D'UN DEMI-PIXEL : c'est la seule marge admise.
      CHECK(vertex.pos.x >= kTopLeft.x - 1.0F);
      CHECK(vertex.pos.x <= kTopLeft.x + kSide + 1.0F);
      CHECK(vertex.pos.y >= kTopLeft.y - 1.0F);
      CHECK(vertex.pos.y <= kTopLeft.y + kSide + 1.0F);
    } else if (vertex.col == ro::ui::kSkinWarningInk) {
      ++ink;
      // Le point d'exclamation, au milieu et sous la pointe.
      CHECK(std::abs(vertex.pos.x - (kTopLeft.x + kSide * 0.5F)) < kSide * 0.25F);
      CHECK(vertex.pos.y > kTopLeft.y + kSide * 0.25F);
      CHECK(vertex.pos.y < kTopLeft.y + kSide);
    }
  }
  CHECK(fill >= 3);
  CHECK(ink > 0);
  ImGui::End();
  ui.EndFrame();
}

TEST_CASE("Bulle qui met en garde : sans rien à dire, pas de bulle", "[ui][tooltip]") {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  ImGui::Begin("porteur");
  ro::ui::SkinTooltipWarned(nullptr, "");
  ImGui::End();
  CHECK(ImGui::FindWindowByName("##Tooltip_00") == nullptr);
  ro::ui::SkinTooltipWarned(nullptr, "Seule la mise en garde.");
  CHECK(ImGui::FindWindowByName("##Tooltip_00") != nullptr);
  ui.EndFrame();
}
