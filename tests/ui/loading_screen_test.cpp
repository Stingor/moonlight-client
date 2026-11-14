// L'écran de chargement : un fond qui couvre tout, l'illustration au-dessus de
// la bande, et la bande qui dit où en est le chargement.

#include "ro/ui/loading_screen.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;
// Les illustrations de l'installation : 640 × 480.
constexpr float kArtWidth = 640.0F;
constexpr float kArtHeight = 480.0F;
// Un identifiant de texture quelconque : la liste de dessin ne le lit pas.
constexpr ImTextureID kSomeTexture = 7;
// Le sablier d'attente de l'installation.
constexpr float kSpinnerWidth = 57.0F;
constexpr float kSpinnerHeight = 80.0F;

// Les sommets qu'ajoute un dessin de l'écran.
[[nodiscard]] int VerticesOf(const ro::ui::LoadingScreenView& view) {
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const ImDrawList* list = ImGui::GetBackgroundDrawList();
  const int before = list->VtxBuffer.Size;
  ro::ui::DrawLoadingScreen(view);
  const int after = list->VtxBuffer.Size;
  ui.EndFrame();
  return after - before;
}

}  // namespace

TEST_CASE("Chargement : le fond couvre l'écran, et le texte s'y ajoute", "[ui][loading]") {
  // 🔴 IL PASSE AVANT CE QUI BLOQUE : s'il ne couvrait pas tout le cadre, la
  // scène d'avant resterait visible par morceaux pendant le figement.
  ro::ui::UiContext ui;
  ui.BeginFrame(kViewport, kFrameSeconds);
  const ImDrawList* list = ImGui::GetBackgroundDrawList();
  const int before = list->VtxBuffer.Size;
  ro::ui::DrawLoadingScreen("");
  const int with_background = list->VtxBuffer.Size;
  ro::ui::DrawLoadingScreen("Chargement...");
  const int with_text = list->VtxBuffer.Size;
  ui.EndFrame();

  CHECK(with_background > before);
  // Le texte ajoute des glyphes par-dessus un second fond.
  CHECK(with_text - with_background > with_background - before);
}

TEST_CASE("Chargement : l'illustration garde ses proportions au-dessus de la bande",
          "[ui][loading]") {
  const ImVec2 origin(0.0F, 0.0F);
  const ImVec2 wide(kViewport.width_px, kViewport.height_px);
  const ro::ui::LoadingLayout layout =
      ro::ui::LayoutLoadingScreen(origin, wide, kArtWidth, kArtHeight);
  // La bande au pied, de toute la largeur.
  CHECK(layout.band_max.y == kViewport.height_px);
  CHECK(layout.band_min.y == Catch::Approx(kViewport.height_px - ro::ui::kLoadingBandHeightPx));
  CHECK(layout.band_max.x - layout.band_min.x == kViewport.width_px);
  // L'illustration tient toute la hauteur au-dessus de la bande, centrée, sans
  // rien couper : des bandes noires sur les côtés.
  const float shown_width = layout.art_max.x - layout.art_min.x;
  const float shown_height = layout.art_max.y - layout.art_min.y;
  CHECK(shown_height == Catch::Approx(layout.band_min.y));
  CHECK(shown_width / shown_height == Catch::Approx(kArtWidth / kArtHeight));
  CHECK(layout.art_min.x == Catch::Approx(kViewport.width_px - layout.art_max.x));
  CHECK(layout.art_max.y <= layout.band_min.y);
}

TEST_CASE("Chargement : sur un écran haut, les bandes noires passent dessus et dessous",
          "[ui][loading]") {
  constexpr float kTallWidth = 600.0F;
  constexpr float kTallHeight = 1200.0F;
  const ro::ui::LoadingLayout layout = ro::ui::LayoutLoadingScreen(
      ImVec2(0.0F, 0.0F), ImVec2(kTallWidth, kTallHeight), kArtWidth, kArtHeight);
  CHECK(layout.art_max.x - layout.art_min.x == Catch::Approx(kTallWidth));
  CHECK(layout.art_min.y > 0.0F);
  CHECK(layout.art_max.y < layout.band_min.y);
}

TEST_CASE("Chargement : sans illustration, aucune place ne lui revient", "[ui][loading]") {
  const ro::ui::LoadingLayout layout = ro::ui::LayoutLoadingScreen(
      ImVec2(0.0F, 0.0F), ImVec2(kViewport.width_px, kViewport.height_px), 0.0F, 0.0F);
  CHECK(layout.art_max.x == layout.art_min.x);
  CHECK(layout.art_max.y == layout.art_min.y);
}

TEST_CASE("Chargement : l'écran complet peint la barre, les textes et l'animation",
          "[ui][loading]") {
  ro::ui::LoadingScreenView bare;
  const int bare_vertices = VerticesOf(bare);
  CHECK(bare_vertices > 0);

  ro::ui::LoadingScreenView full;
  full.backdrop = ro::ui::SkinImage{.id = kSomeTexture, .width = kArtWidth, .height = kArtHeight};
  full.step_text = "Modèles 312 / 540";
  full.percent_text = "57 %";
  full.fraction = 0.57F;
  CHECK(VerticesOf(full) > bare_vertices);

  // L'image de l'animation remplace les points dessinés.
  ro::ui::LoadingScreenView with_sprite = bare;
  with_sprite.spinner =
      ro::ui::SkinImage{.id = kSomeTexture, .width = kSpinnerWidth, .height = kSpinnerHeight};
  CHECK(VerticesOf(with_sprite) < bare_vertices);
}

TEST_CASE("Chargement : une barre indéterminée et un fondu se peignent sans erreur",
          "[ui][loading]") {
  ro::ui::LoadingScreenView view;
  view.fraction = -1.0F;
  view.opacity = 0.0F;
  for (const float clock_s : {0.0F, 0.4F, 1.2F, 7.9F}) {
    view.clock_s = clock_s;
    CHECK(VerticesOf(view) > 0);
  }
}
