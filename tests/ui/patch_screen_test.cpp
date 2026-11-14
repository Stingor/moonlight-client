// L'écran de mise à jour : il dessine sans rien savoir du patcher, et ne rend
// d'intention que sur un geste du joueur.

#include "ro/ui/patch_screen.h"

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/ui_context.h"

namespace {

constexpr ro::ui::Viewport kViewport{.width_px = 1280.0F, .height_px = 720.0F};
constexpr float kFrameSeconds = 1.0F / 60.0F;

}  // namespace

TEST_CASE("Mise à jour : en cours ou en échec, l'écran se dessine sans rien demander",
          "[ui][patch]") {
  ro::ui::UiContext ui;
  for (const bool failed : {false, true}) {
    for (const float fraction : {-1.0F, 0.0F, 0.5F, 1.0F}) {
      ui.BeginFrame(kViewport, kFrameSeconds);
      ro::ui::PatchScreenView view;
      view.headline = "Mise à jour 1 sur 2";
      view.detail = "2026-09-24-grf-1.thor";
      view.fraction = fraction;
      view.failed = failed;
      view.retry_label = "Réessayer";
      view.play_label = "Jouer quand même";
      view.quit_label = "Quitter";
      const ro::ui::PatchScreenAnswers answers = ro::ui::DrawPatchScreen(view);
      ui.EndFrame();
      // Aucun clic : aucune intention.
      CHECK_FALSE(answers.retry);
      CHECK_FALSE(answers.play_anyway);
      CHECK_FALSE(answers.quit);
    }
  }
}
