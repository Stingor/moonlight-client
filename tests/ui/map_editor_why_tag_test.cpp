// LA RAISON AU BOUT D'UNE LIGNE DE LISTE de l'éditeur de carte, et
// l'intensité de la couleur d'une lumière.

#include <array>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/map_editor_effects_panel.h"
#include "ro/ui/triple_clipboard.h"

TEST_CASE("Éditeur : la raison suit la ligne, et ne coupe pas son libellé", "[ui][mapedit]") {
  CHECK(ro::ui::WhyTag("").empty());
  CHECK(ro::ui::WhyTag("brasero de gauche") == " — brasero de gauche");
  // « ## » terminerait le libellé visible chez ImGui : il se rompt.
  CHECK(ro::ui::WhyTag("lampe ##2") == " — lampe # #2");
  CHECK(ro::ui::WhyTag("###").find("##") == std::string::npos);
}

TEST_CASE("Éditeur : trois valeurs s'écrivent et se relisent au presse-papiers", "[ui][mapedit]") {
  CHECK(ro::ui::TripleText({150.1F, 113.734F, -4.0F}) == "150.10 113.73 -4.00");
  const auto read = ro::ui::ParseTriple("150.10 113.73 -4.00");
  REQUIRE(read.has_value());
  CHECK((*read)[0] == 150.1F);
  CHECK((*read)[2] == -4.0F);
  // Virgules, points-virgules, espaces mêlés.
  CHECK(ro::ui::ParseTriple(" 1, 2;3 ").has_value());
  // Ni deux, ni quatre, ni un mot.
  CHECK_FALSE(ro::ui::ParseTriple("1 2").has_value());
  CHECK_FALSE(ro::ui::ParseTriple("1 2 3 4").has_value());
  CHECK_FALSE(ro::ui::ParseTriple("1 deux 3").has_value());
  CHECK_FALSE(ro::ui::ParseTriple("").has_value());
}

TEST_CASE("Éditeur : l'intensité d'une lumière change sa force, pas sa teinte", "[ui][mapedit]") {
  // La couleur d'un brasier : son intensité est sa plus forte composante.
  const std::array<float, 3> brazier{2.0F, 1.4F, 0.7F};
  CHECK(ro::ui::LightIntensityOf(brazier) == 2.0F);
  const std::array<float, 3> halved = ro::ui::WithLightIntensity(brazier, 1.0F);
  CHECK(halved[0] == 1.0F);
  CHECK(halved[1] == 0.7F);
  CHECK(halved[2] == 0.35F);
  CHECK(ro::ui::LightIntensityOf(ro::ui::WithLightIntensity(brazier, 3.5F)) == 3.5F);
  // Une couleur noire n'a plus de teinte : elle devient un blanc.
  const std::array<float, 3> black{0.0F, 0.0F, 0.0F};
  CHECK(ro::ui::WithLightIntensity(black, 1.5F) == std::array<float, 3>{1.5F, 1.5F, 1.5F});
}
