// L'ascenseur habillé : la formule de sa poignée, et le geste qu'il prend.

#include <imgui.h>
#include <imgui_internal.h>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/skin_widgets.h"
#include "ro/ui/ui_context.h"

using ro::render::SkinPiece;
using ro::ui::SkinArt;
using ro::ui::SkinImage;

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

// Les mesures de l'ascenseur du client : treize pixels de large, et ses flèches
// sont carrées.
constexpr float kScrollWidthPixels = 13.0F;
constexpr float kScrollPiecePixels = 13.0F;

}  // namespace

TEST_CASE("Ascenseur : la poignée mesure ce qu'on voit", "[ui][widgets]") {
  // ⚠ CETTE FORMULE EST CELLE D'IMGUI, RECOPIÉE : l'ascenseur qu'on PEINT doit
  // tomber là où celui qu'ImGui ÉCOUTE se trouve, sinon la poignée se prend à
  // côté de là où on la voit. C'est pourquoi elle est isolée et éprouvée : une
  // montée de version n'a qu'UN endroit à revérifier.
  //
  // La moitié du contenu tient à l'écran : la poignée fait la moitié de la piste.
  const ro::ui::ScrollGrab moitie = ro::ui::VerticalScrollGrab(200.0F, 100.0F, 200.0F, 0.0F, 10.0F);
  CHECK(moitie.start == 0.0F);
  CHECK(moitie.end == 100.0F);

  // Au BOUT du défilement, elle touche le bas de la piste et ne dépasse pas.
  const ro::ui::ScrollGrab at_end =
      ro::ui::VerticalScrollGrab(200.0F, 100.0F, 200.0F, 100.0F, 10.0F);
  CHECK(at_end.end == 200.0F);
  CHECK(at_end.start == 100.0F);

  // À mi-chemin, elle est au milieu.
  const ro::ui::ScrollGrab middle =
      ro::ui::VerticalScrollGrab(200.0F, 100.0F, 200.0F, 50.0F, 10.0F);
  CHECK(middle.start == 50.0F);
  CHECK(middle.end == 150.0F);
}

namespace {

// L'art d'un ascenseur : ses six pièces, aux mesures du client.
[[nodiscard]] SkinArt ScrollArt() {
  SkinArt art;
  for (const SkinPiece piece :
       {SkinPiece::kScrollUp, SkinPiece::kScrollMiddle, SkinPiece::kScrollDown,
        SkinPiece::kScrollGrabTop, SkinPiece::kScrollGrabMiddle, SkinPiece::kScrollGrabBottom,
        SkinPiece::kResizeGrip}) {
    art.Set(piece, SkinImage{.id = 1, .width = kScrollWidthPixels, .height = kScrollPiecePixels});
  }
  return art;
}

}  // namespace

TEST_CASE("Ascenseur : la flèche du bas fait defiler", "[ui][widgets]") {
  // 🔴 IMGUI N'A PAS DE FLECHES. Sa barre est une piste et une poignee : sous les
  // deux boutons qu'on peint, personne n'ecouterait le clic. Ce serait dessiner
  // un geste qui ne repond pas -- pire qu'un bouton absent, puisque le joueur le
  // reessaie et finit par douter de sa souris.
  //
  // L'ascenseur prend donc l'interaction entiere, fleches comprises.
  ro::ui::UiContext ui;
  const SkinArt art = ScrollArt();
  ImGuiIO& io = ImGui::GetIO();
  float scroll_seen = 0.0F;
  ImVec2 arrow_at(0.0F, 0.0F);

  // ⚠ SANS GRIP : le rail en reserve la place en bas, et viser la fleche
  // demanderait de refaire ce calcul dans l'essai -- donc de recopier ce qu'il
  // verifie. Une fenetre non redimensionnable n'a pas de grip, et sa fleche du
  // bas touche le bord.
  const auto draw = [&art, &scroll_seen, &arrow_at]() {
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(200.0F, 200.0F));
    ImGui::Begin("essai", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    for (int line = 0; line < 80; ++line) {
      ImGui::Text("ligne %d", line);
    }
    const ImGuiWindow* const window = ImGui::GetCurrentWindow();
    const ImRect slot = ImGui::GetWindowScrollbarRect(ImGui::GetCurrentWindow(), ImGuiAxis_Y);
    arrow_at = ImVec2((slot.Min.x + slot.Max.x) * 0.5F, slot.Max.y - kScrollPiecePixels * 0.5F);
    scroll_seen = window->Scroll.y;
    ro::ui::PaintWindowScrollbar(art);
    ImGui::End();
  };

  // Deux images : l'ascenseur n'existe qu'a partir de ce que la precedente a
  // mesure, et c'est elle qui donne la place de la fleche.
  for (int frame = 0; frame < 2; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    draw();
    ui.EndFrame();
  }
  REQUIRE(scroll_seen == 0.0F);

  io.AddMousePosEvent(arrow_at.x, arrow_at.y);
  ui.BeginFrame(kViewport, kFrameSeconds);
  draw();
  ui.EndFrame();

  io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
  ui.BeginFrame(kViewport, kFrameSeconds);
  draw();
  ui.EndFrame();
  io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);

  // ⚠ RELU A L'IMAGE SUIVANTE : le clic ecrit le defilement pendant le dessin,
  // et `scroll_seen` de cette image-la porte encore la valeur d'avant.
  ui.BeginFrame(kViewport, kFrameSeconds);
  draw();
  ui.EndFrame();
  const float after_one_step = scroll_seen;
  CHECK(after_one_step > 0.0F);

  // 🔴 ET C'EST UN PAS, PAS UN SAUT. La barre d'ImGui est toujours la sous la
  // notre -- c'est elle qui reserve la place, et on la rend transparente --, et
  // elle ecoutait : son clic dans la piste TELEPORTE la poignee sous le curseur.
  // La fleche du bas partait donc en fin de liste. On lui retire le bouton
  // gauche sur ce rail, et ce qui reste est le pas du client : trois lignes.
  CHECK(after_one_step < ImGui::GetTextLineHeightWithSpacing() * 4.0F);
}

TEST_CASE("Ascenseur : la poignee ne passe jamais sous une fleche", "[ui][widgets]") {
  // 🔴 LA PISTE VIT ENTRE LES DEUX FLECHES. Celle d'ImGui court d'un bord a
  // l'autre du creneau -- il n'a pas de fleches a epargner -- et la poignee
  // peinte telle quelle disparaissait sous celles qu'on redessine par-dessus.
  // L'ascenseur du client ne fait jamais ca : sa poignee s'arrete au ras des
  // boutons, aux deux bouts de sa course.
  //
  // Ce que l'essai tient : au BOUT du defilement, le bas de la poignee ne
  // descend pas dans la fleche du bas.
  ro::ui::UiContext ui;
  const SkinArt art = ScrollArt();
  float lowest_bottom = 0.0F;
  float arrow_top = 0.0F;

  const auto draw = [&art, &lowest_bottom, &arrow_top]() {
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(ImVec2(200.0F, 200.0F));
    ImGui::Begin("essai", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    for (int line = 0; line < 80; ++line) {
      ImGui::Text("ligne %d", line);
    }
    ImGuiWindow* const window = ImGui::GetCurrentWindow();
    ImGui::SetScrollY(window->ScrollMax.y);
    const ImRect slot = ImGui::GetWindowScrollbarRect(window, ImGuiAxis_Y);
    arrow_top = slot.Max.y - kScrollPiecePixels;
    // La piste utile, telle que le rail la calcule : le creneau moins ses deux
    // fleches. La poignee, au bout de sa course, en occupe la fin.
    const float track = slot.GetHeight() - kScrollPiecePixels * 2.0F;
    const ro::ui::ScrollGrab grab =
        ro::ui::VerticalScrollGrab(track, window->InnerRect.GetHeight(),
                                   window->ContentSize.y + window->WindowPadding.y * 2.0F,
                                   window->ScrollMax.y, ImGui::GetStyle().GrabMinSize);
    lowest_bottom = slot.Min.y + kScrollPiecePixels + grab.end;
    ro::ui::PaintWindowScrollbar(art);
    ImGui::End();
  };

  for (int frame = 0; frame < 3; ++frame) {
    ui.BeginFrame(kViewport, kFrameSeconds);
    draw();
    ui.EndFrame();
  }
  REQUIRE(arrow_top > 0.0F);
  CHECK(lowest_bottom <= arrow_top);
}

TEST_CASE("Ascenseur : elle ne disparaît pas, et ne déborde pas", "[ui][widgets]") {
  // Un contenu énorme donnerait une poignée d'un pixel, impossible à saisir :
  // un plancher la garde attrapable, et c'est ImGui qui le fixe.
  const ro::ui::ScrollGrab minuscule =
      ro::ui::VerticalScrollGrab(200.0F, 10.0F, 100000.0F, 0.0F, 12.0F);
  CHECK(minuscule.end - minuscule.start == 12.0F);

  // Tout tient à l'écran : la poignée occupe la piste entière.
  const ro::ui::ScrollGrab entiere =
      ro::ui::VerticalScrollGrab(200.0F, 300.0F, 100.0F, 0.0F, 12.0F);
  CHECK(entiere.start == 0.0F);
  CHECK(entiere.end == 200.0F);

  // Et un défilement au-delà du possible ne la fait pas sortir.
  const ro::ui::ScrollGrab too_far =
      ro::ui::VerticalScrollGrab(200.0F, 100.0F, 200.0F, 10000.0F, 10.0F);
  CHECK(too_far.end <= 200.0F);

  // Une piste de hauteur nulle ne dessine rien.
  const ro::ui::ScrollGrab zero_extent =
      ro::ui::VerticalScrollGrab(0.0F, 10.0F, 100.0F, 0.0F, 10.0F);
  CHECK(zero_extent.start == zero_extent.end);
}
