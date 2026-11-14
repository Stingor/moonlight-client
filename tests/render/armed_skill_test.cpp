// CE QU'UN SORT EN MAIN MONTRE : la place de ses chiffres au curseur, et celle
// de son étiquette. Les défauts sont ceux du client, et c'est ce que ces essais
// gardent — un réglage remis à zéro doit rendre le jeu tel qu'il est.

#include "ro/render/armed_skill.h"

#include <catch2/catch_test_macros.hpp>

namespace {

using ro::render::ArmedBannerBaseline;
using ro::render::ArmedBannerColorBytes;
using ro::render::ArmedLevelDigitAt;
using ro::render::ArmedSkillLook;

}  // namespace

TEST_CASE("Sort en main : les défauts sont ceux du client", "[render][armed]") {
  const ArmedSkillLook look;
  CHECK(look.level_shown);
  CHECK(look.level_scale == 2.0F);
  CHECK(look.level_offset_x_pixels == 24.0F);
}

TEST_CASE("Sort en main : un seul chiffre se pose à l'écart réglé", "[render][armed]") {
  const ArmedSkillLook look;
  const glm::vec2 at = ArmedLevelDigitAt(look, {100.0F, 200.0F}, 1, 0);
  CHECK(at.x == 124.0F);
  // ⚠ LA MÊME ORDONNÉE QUE LA POINTE : le client ne décale son nombre qu'en
  // abscisse, et un chiffre remonté se lirait comme une infobulle.
  CHECK(at.y == 200.0F);
}

TEST_CASE("Sort en main : les unités sont à DROITE", "[render][armed]") {
  // 🔴 C'EST LA PANNE QUE CET ESSAI GARDE. Poser l'indice zéro à gauche écrirait
  // le nombre à l'envers dès la dizaine — un rang 10 se lirait « 01 ».
  const ArmedSkillLook look;
  const glm::vec2 tens = ArmedLevelDigitAt(look, {0.0F, 0.0F}, 2, 1);
  const glm::vec2 units = ArmedLevelDigitAt(look, {0.0F, 0.0F}, 2, 0);
  CHECK(tens.x < units.x);
  // Le chiffre de poids fort à l'écart réglé, l'autre un pas plus loin : seize
  // pixels à l'échelle deux, comme le client.
  CHECK(tens.x == 24.0F);
  CHECK(units.x == 40.0F);
}

TEST_CASE("Sort en main : le pas suit l'échelle", "[render][armed]") {
  // L'écart d'un chiffre au suivant est un écart de SPRITE : doubler l'échelle
  // sans doubler le pas ferait des chiffres qui se chevauchent.
  ArmedSkillLook look;
  look.level_scale = 1.0F;
  const glm::vec2 units = ArmedLevelDigitAt(look, {0.0F, 0.0F}, 2, 0);
  CHECK(units.x == 32.0F);
}

TEST_CASE("Sort en main : l'étiquette est centrée, et haute", "[render][armed]") {
  const ArmedSkillLook look;
  const glm::vec2 at = ArmedBannerBaseline(look, 1280.0F, 960.0F);
  CHECK(at.x == 640.0F);
  // 46/480 de la hauteur, la place du client.
  CHECK(at.y == 92.0F);
}

TEST_CASE("Sort en main : les réglages sont bornés au DESSIN", "[render][armed]") {
  // ⚠ LES RÉGLAGES VIENNENT AUSSI D'UN FICHIER que le joueur écrit à la main, et
  // cette voie-là ne passe par aucun curseur.
  ArmedSkillLook look;
  look.banner_height_percent = 900.0F;
  CHECK(ArmedBannerBaseline(look, 100.0F, 1000.0F).y == 900.0F);
  look.banner_height_percent = -50.0F;
  CHECK(ArmedBannerBaseline(look, 100.0F, 1000.0F).y == 0.0F);
}

TEST_CASE("Sort en main : la couleur se lit comme celle de la zone", "[render][armed]") {
  // Alpha, bleu, vert, rouge du poids fort au faible — et quatre octets rendus
  // dans l'ordre rouge, vert, bleu, alpha.
  const std::array<std::uint8_t, 4> bytes = ArmedBannerColorBytes(0xFF32FF32U);
  CHECK(bytes[0] == 0x32);
  CHECK(bytes[1] == 0xFF);
  CHECK(bytes[2] == 0x32);
  CHECK(bytes[3] == 0xFF);
}
