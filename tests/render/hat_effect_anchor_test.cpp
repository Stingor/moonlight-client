#include "ro/render/hat_effect_anchor.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_scale.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/sprite_billboard.h"

using Catch::Approx;
using ro::render::HatConcreteStrAnchor;
using ro::render::HatEffect;
using ro::render::HatEffectAnchor;
using ro::render::HatEffectBearer;
using ro::render::HatStrAnchor;

namespace {

// Un personnage à pied, adulte, d'un métier quelconque.
constexpr std::uint32_t kSwordman = 1;
// Un chevalier sur son Peco, et un bébé.
constexpr std::uint32_t kKnightOnPeco = 13;
constexpr std::uint32_t kBabyNovice = 4023;
// Un Doram.
constexpr std::uint32_t kSummoner = 4218;

// Où tombe l'origine du fichier, en pixels de sprite au-dessus des pieds : ce
// que le joueur compare à l'œil entre les deux clients.
[[nodiscard]] float PixelsAboveFeet(const HatEffectAnchor& anchor) {
  return -anchor.view_offset.y * ro::render::kSpritePixelsPerWorldUnit + anchor.lift_pixels;
}

}  // namespace

TEST_CASE("Ancre d'un chapeau : une fiche à -11 pose son fichier AUX PIEDS",
          "[render][hateffect]") {
  // Cosmic Connection (rang 222) : décalage -11, rien d'autre. Son origine est
  // aux pieds, et non quatre-vingts pixels plus haut comme un `.str` ordinaire —
  // c'est ce qui mettait sa sphère au-dessus de la tête.
  HatEffect cosmic;
  cosmic.file = "efst_Cosmic_Connection\\strbright.str";
  cosmic.offset = -11.0F;
  const HatEffectAnchor anchor = HatStrAnchor(cosmic, cosmic.file, {.job = kSwordman});
  CHECK(anchor.lift_pixels == 0.0F);
  CHECK(anchor.view_offset.x == 0.0F);
  CHECK(anchor.view_offset.y == 0.0F);
  CHECK(PixelsAboveFeet(anchor) == 0.0F);
}

TEST_CASE("Ancre d'un chapeau : le décalage à droite, en unités du monde", "[render][hateffect]") {
  // Dark Lord Cloak (rang 185) : -11 et -0,5, monture ignorée. La hauteur est
  // nulle, donc positive ou nulle : l'ancre tient, et le cercle est aux pieds,
  // une demi-unité à gauche.
  HatEffect cloak;
  cloak.file = "efst_C_Dark_Lord_Cloak\\darklordcloak.str";
  cloak.offset = -11.0F;
  cloak.offset_x = -0.5F;
  cloak.ignore_riding = true;
  const HatEffectAnchor anchor = HatStrAnchor(cloak, cloak.file, {.job = kKnightOnPeco});
  CHECK(anchor.lift_pixels == 0.0F);
  CHECK(anchor.view_offset.x == Approx(-0.5F));
  CHECK(anchor.view_offset.y == 0.0F);
}

TEST_CASE("Ancre d'un chapeau : une fiche à zéro monte de onze unités, la monture en plus",
          "[render][hateffect]") {
  HatEffect plain;
  plain.file = "efst_firework\\firework.str";
  // 18 + 0 - 7 : onze unités, soit soixante-dix-sept pixels de sprite — à trois
  // pixels du relèvement ordinaire.
  CHECK(HatStrAnchor(plain, plain.file, {.job = kSwordman}).view_offset.y == -11.0F);
  CHECK(PixelsAboveFeet(HatStrAnchor(plain, plain.file, {.job = kSwordman})) == 77.0F);
  // Un chevalier monté est plus haut de trois : sa classe de taille.
  CHECK(HatStrAnchor(plain, plain.file, {.job = kKnightOnPeco}).view_offset.y == -14.0F);
  // Ignorer la monture fixe la base.
  plain.ignore_riding = true;
  CHECK(HatStrAnchor(plain, plain.file, {.job = kKnightOnPeco}).view_offset.y == -11.0F);
}

TEST_CASE("Ancre d'un chapeau : l'échelle du corps et sa réduction", "[render][hateffect]") {
  HatEffect record;
  record.file = "x.str";
  // Un bébé de première classe : trois quarts de onze.
  CHECK(HatStrAnchor(record, record.file, {.job = kBabyNovice}).view_offset.y ==
        Approx(-11.0F * 0.75F));
  // La réduction ne compte que pour les fiches qui la demandent.
  const HatEffectBearer shrunk{.job = kSwordman, .shrink_scale = 0.5F};
  CHECK(HatStrAnchor(record, record.file, shrunk).view_offset.y == -11.0F);
  CHECK(HatStrAnchor(record, record.file, shrunk).scale == 1.0F);
  record.adjust_position_when_shrunk = true;
  record.adjust_size_when_shrunk = true;
  CHECK(HatStrAnchor(record, record.file, shrunk).view_offset.y == Approx(-5.5F));
  CHECK(HatStrAnchor(record, record.file, shrunk).scale == 0.5F);
}

TEST_CASE("Ancre d'un chapeau : le client renonce quand tout est négatif", "[render][hateffect]") {
  // -17 et un pas à gauche : hauteur -6, droite -1. Plus de déplacement, et le
  // relèvement ordinaire revient.
  HatEffect low;
  low.file = "x.str";
  low.offset = -17.0F;
  low.offset_x = -1.0F;
  const HatEffectAnchor anchor = HatStrAnchor(low, low.file, {.job = kSwordman});
  CHECK(anchor.view_offset == glm::vec2(0.0F));
  CHECK(anchor.lift_pixels == ro::render::kEffectAnchorLiftPixels);
  // Une seule des deux positive suffit à garder l'ancre.
  low.offset_x = 0.0F;
  CHECK(HatStrAnchor(low, low.file, {.job = kSwordman}).lift_pixels == 0.0F);
}

TEST_CASE("Ancre d'un chapeau : la tête d'un Doram, et les fichiers nommés",
          "[render][hateffect]") {
  HatEffect head;
  head.file = "x.str";
  head.attached_head = true;
  // 18 - 7 - 3, à l'échelle d'un Doram adulte, qui est un.
  CHECK(HatStrAnchor(head, head.file, {.job = kSummoner}).view_offset.y == -8.0F);
  CHECK(HatStrAnchor(head, head.file, {.job = kSwordman}).view_offset.y == -11.0F);

  HatEffect lights;
  lights.file = "efst_STRANGELIGHTS\\strangelights.str";
  CHECK(HatStrAnchor(lights, lights.file, {.job = kSwordman}).view_offset.x == 5.0F);
  CHECK(HatStrAnchor(lights, "efst_ljosalfar\\ljosalfar.str", {.job = kSwordman}).view_offset.x ==
        8.0F);
}

TEST_CASE("Ancre d'un chapeau : un identifiant concret ne lit pas la fiche",
          "[render][hateffect]") {
  const HatEffectAnchor anchor = HatConcreteStrAnchor("x.str", {.job = kSwordman});
  CHECK(anchor.view_offset == glm::vec2(0.0F, -11.0F));
  CHECK(anchor.lift_pixels == 0.0F);
  CHECK(ro::render::HatIdIsReadAsStr(1012));
  CHECK_FALSE(ro::render::HatIdIsReadAsStr(421));
}

TEST_CASE("Classe de taille du métier affiché", "[render][hateffect]") {
  CHECK(ro::render::HatAnchorJobSizeClass(kSwordman) == 0);
  CHECK(ro::render::HatAnchorJobSizeClass(kKnightOnPeco) == 3);
  CHECK(ro::render::HatAnchorJobSizeClass(4044) == 3);
  CHECK(ro::render::HatAnchorJobSizeClass(4080) == 5);
  CHECK(ro::render::HatAnchorJobSizeClass(4281) == 5);
  CHECK(ro::render::HatAnchorJobSizeClass(4282) == 0);
}

TEST_CASE("Ancre d'un chapeau : une monture du shop porte la base à vingt-trois",
          "[render][hateffect]") {
  HatEffect plain;
  plain.file = "x.str";
  // 23 - 7 : seize unités, sans la classe de taille du métier.
  CHECK(HatStrAnchor(plain, plain.file, {.job = kSwordman, .cash_mount = true}).view_offset.y ==
        -16.0F);
  CHECK(HatStrAnchor(plain, plain.file, {.job = kKnightOnPeco, .cash_mount = true}).view_offset.y ==
        -16.0F);
  // Un Doram monté garde la base à pied.
  CHECK(HatStrAnchor(plain, plain.file, {.job = kSummoner, .cash_mount = true}).view_offset.y ==
        -11.0F);
  // Et la fiche qui ignore la monture l'ignore aussi.
  plain.ignore_riding = true;
  CHECK(HatStrAnchor(plain, plain.file, {.job = kSwordman, .cash_mount = true}).view_offset.y ==
        -11.0F);
  // Un identifiant concret la lit comme une fiche.
  CHECK(HatConcreteStrAnchor("x.str", {.job = kSwordman, .cash_mount = true}).view_offset.y ==
        -16.0F);
}

TEST_CASE("Ancre d'un chapeau : la recette est figée, le porteur se relit", "[render][hateffect]") {
  HatEffect record;
  record.file = "efst_STRANGELIGHTS\\strangelights.str";
  record.offset_x = 1.0F;
  record.adjust_position_when_shrunk = true;
  const ro::render::HatAnchorRecipe recipe = ro::render::HatStrAnchorRecipe(record, record.file);
  CHECK(recipe.right_units == 6.0F);
  // Le même fichier, sur un porteur qui monte puis rapetisse.
  CHECK(ro::render::HatAnchorFor(recipe, {.job = kSwordman}).view_offset.y == -11.0F);
  CHECK(ro::render::HatAnchorFor(recipe, {.job = kSwordman, .cash_mount = true}).view_offset.y ==
        -16.0F);
  CHECK(ro::render::HatAnchorFor(recipe, {.job = kSwordman, .shrink_scale = 0.5F}).view_offset.y ==
        Approx(-5.5F));
}

TEST_CASE("Ancre d'un chapeau : une fiche qui suit la tête suit son déplacement",
          "[render][hateffect]") {
  // Un novice qui s'assied : sa tête descend de dix-neuf pixels de sprite.
  const glm::vec2 seated(0.0F, 19.0F);
  const float sprite_pixel = 1.0F / ro::render::kSpritePixelsPerWorldUnit;
  HatEffect head;
  head.file = "x.str";
  head.attached_head = true;
  const HatEffectAnchor standing = HatStrAnchor(head, head.file, {.job = kSwordman});
  const HatEffectAnchor sitting =
      HatStrAnchor(head, head.file, {.job = kSwordman, .head_shift_pixels = seated});
  CHECK(sitting.view_offset.x == standing.view_offset.x);
  CHECK(sitting.view_offset.y == Approx(standing.view_offset.y + 19.0F * sprite_pixel));

  // À l'échelle du corps : celle d'un bébé, et la réduction quand la fiche la
  // demande pour sa place.
  const HatEffectAnchor baby_standing = HatStrAnchor(head, head.file, {.job = kBabyNovice});
  const HatEffectAnchor baby_sitting =
      HatStrAnchor(head, head.file, {.job = kBabyNovice, .head_shift_pixels = seated});
  CHECK(baby_sitting.view_offset.y - baby_standing.view_offset.y ==
        Approx(19.0F * sprite_pixel * ro::render::BodyScaleFor(kBabyNovice)));

  // UNE FICHE QUI NE SUIT PAS LA TÊTE l'ignore.
  HatEffect body;
  body.file = "x.str";
  CHECK(
      HatStrAnchor(body, body.file, {.job = kSwordman, .head_shift_pixels = seated}).view_offset ==
      HatStrAnchor(body, body.file, {.job = kSwordman}).view_offset);

  // ET LE RENONCEMENT NE L'EFFACE PAS : l'ancre revient au relèvement ordinaire,
  // la tête se suit toujours.
  head.offset = -30.0F;
  head.offset_x = -1.0F;
  const HatEffectAnchor renounced =
      HatStrAnchor(head, head.file, {.job = kSwordman, .head_shift_pixels = seated});
  CHECK(renounced.lift_pixels == ro::render::kEffectAnchorLiftPixels);
  CHECK(renounced.view_offset.x == 0.0F);
  CHECK(renounced.view_offset.y == Approx(19.0F * sprite_pixel));
}
