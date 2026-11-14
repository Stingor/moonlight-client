// UN MODÈLE AU PRESSE-PAPIERS : écrit, relu, refusé.

#include "ro/ui/model_clipboard.h"

#include <catch2/catch_test_macros.hpp>

using ro::ui::ClipboardModel;

TEST_CASE("Presse-papiers : un modèle s'écrit et se relit à l'identique", "[ui][mapedit]") {
  const ClipboardModel lantern{.model_file = "대만\\대만_등.rsm",
                               .rotation = {0.0F, 270.0F, 0.0F},
                               .scale = {0.22F, 0.22F, 0.22F},
                               .opacity = 0.5F};
  const std::string text = ro::ui::ModelClipboardText(lantern);
  CHECK(text.starts_with("moonclient-modèle|"));
  CHECK(ro::ui::ParseModelClipboard(text) == lantern);
  // Un retour à la ligne collé avec : relu quand même.
  CHECK(ro::ui::ParseModelClipboard(text + "\r\n") == lantern);
  // Plein : l'opacité reste vide.
  const ClipboardModel plain{.model_file = "a.rsm"};
  CHECK(ro::ui::ParseModelClipboard(ro::ui::ModelClipboardText(plain)) == plain);
}

TEST_CASE("Presse-papiers : le modèle emporte sa façon de jouer", "[ui][mapedit]") {
  const ClipboardModel mill{
      .model_file = "a.rsm", .animation_type = ro::render::kModelPlayOnce, .animation_speed = 0.5F};
  CHECK(ro::ui::ParseModelClipboard(ro::ui::ModelClipboardText(mill)) == mill);
  // Une valeur que le .rsw ne joue pas fige, et s'écrit comme telle.
  const ClipboardModel odd{.model_file = "a.rsm", .animation_type = 7};
  CHECK(ro::ui::ParseModelClipboard(ro::ui::ModelClipboardText(odd))->animation_type ==
        ro::render::kModelStill);
  // Une ligne sans le dernier champ joue en boucle, à vitesse 1.
  const auto older = ro::ui::ParseModelClipboard("moonclient-modèle|a.rsm|0 0 0|1 1 1|");
  REQUIRE(older.has_value());
  CHECK(older->animation_type == ro::render::kModelPlayLooped);
  CHECK(older->animation_speed == 1.0F);
  CHECK_FALSE(ro::ui::ParseModelClipboard("moonclient-modèle|a.rsm|0 0 0|1 1 1||2 0").has_value());
  CHECK_FALSE(ro::ui::ParseModelClipboard("moonclient-modèle|a.rsm|0 0 0|1 1 1||3 1").has_value());
  CHECK_FALSE(
      ro::ui::ParseModelClipboard("moonclient-modèle|a.rsm|0 0 0|1 1 1||1.5 1").has_value());
}

TEST_CASE("Presse-papiers : ce qui n'est pas un modèle est refusé", "[ui][mapedit]") {
  CHECK_FALSE(ro::ui::ParseModelClipboard("").has_value());
  CHECK_FALSE(ro::ui::ParseModelClipboard("150.10 113.73 71.46").has_value());
  CHECK_FALSE(ro::ui::ParseModelClipboard("moonclient-modèle||0 0 0|1 1 1|").has_value());
  CHECK_FALSE(ro::ui::ParseModelClipboard("moonclient-modèle|a.rsm|0 0|1 1 1|").has_value());
  CHECK_FALSE(ro::ui::ParseModelClipboard("moonclient-modèle|a.rsm|0 0 0|1 1 1|2").has_value());
  CHECK_FALSE(ro::ui::ParseModelClipboard("autre|a.rsm|0 0 0|1 1 1|").has_value());
}
