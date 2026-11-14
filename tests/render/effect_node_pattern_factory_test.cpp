// LA FABRIQUE COMMUNE DES SPRITES, lue type par type.
//
// La fabrique pose un nœud de sous-type cinq pour quatre-vingt-quatre types,
// chacun dans son cas. Lue d'un seul tenant — comme un bâtisseur ordinaire —,
// elle prêterait à TOUS les types l'alpha du type sept et l'échelle du type
// soixante et un. Ces essais tiennent les cas séparés.
#include <cstdint>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pattern_table.h"

using ro::render::EffectNodePattern;
using ro::render::EffectNodePatternsFor;
using ro::render::EffectNodeShape;

namespace {

const EffectNodePattern& SpriteOnly(std::uint32_t effect_id) {
  const std::span<const EffectNodePattern> patterns = EffectNodePatternsFor(effect_id);
  REQUIRE(patterns.size() == 1);
  REQUIRE(patterns.front().shape == EffectNodeShape::kSprite);
  return patterns.front();
}

}  // namespace

TEST_CASE("Fabrique : un type ne prête pas son alpha aux autres", "[render][table]") {
  // Le type sept (EF_FIREHIT, 583) pose deux cent vingt ; le type zéro (570) ne
  // pose rien, et garde le défaut du constructeur.
  CHECK(SpriteOnly(583).alpha_start == Catch::Approx(220.0F));
  CHECK(SpriteOnly(570).alpha_start == Catch::Approx(254.0F));
  // Ni son échelle : le tirage de 0,4 à 0,8 est celui du type soixante et un.
  CHECK(SpriteOnly(570).sprite_scale == Catch::Approx(1.0F));
  CHECK(SpriteOnly(570).sprite_scale_high == 0.0F);
  CHECK(SpriteOnly(912).sprite_scale == Catch::Approx(0.4F));
  CHECK(SpriteOnly(912).sprite_scale_high == Catch::Approx(0.8F));
}

TEST_CASE("Fabrique : le tronc vaut pour tous, le cas l'emporte", "[render][table]") {
  // Le tronc pose deux tics par image et une lecture unique ;
  // le type sept réécrit trois tics, le type dix réécrit la boucle.
  const EffectNodePattern& zero = SpriteOnly(570);
  CHECK(zero.sprite_frame_hold_ticks == 2);
  CHECK(zero.sprite_plays_once);
  CHECK(SpriteOnly(583).sprite_frame_hold_ticks == 3);
  CHECK_FALSE(SpriteOnly(586).sprite_plays_once);
  // Et la vie est celle de l'EFFET, que le tronc recopie de l'instance.
  CHECK(zero.life_from_effect);
}

TEST_CASE("Fabrique : ce qu'un type TIRE se tire", "[render][table]") {
  // Type dix-neuf (634) : `rand() % 5 + 3` tics par image, et il boucle.
  const EffectNodePattern& flamme = SpriteOnly(634);
  CHECK(flamme.sprite_frame_hold_ticks == 3);
  CHECK(flamme.sprite_frame_hold_ticks_high == 7);
  CHECK_FALSE(flamme.sprite_plays_once);
  // Type soixante et un (912) : alpha `rand() % 71 + 70`, teinte verte.
  const EffectNodePattern& arbre = SpriteOnly(912);
  CHECK(arbre.alpha_start == Catch::Approx(70.0F));
  CHECK(arbre.alpha_start_high == Catch::Approx(140.0F));
  CHECK(arbre.color[0] == 85);
  CHECK(arbre.color[1] == 255);
  CHECK(arbre.color[2] == 85);
}

TEST_CASE("Fabrique : le tronc garde la couleur du calque, le type 7 la rend au nœud",
          "[render][table]") {
  // Le tronc lève 0x200 dans les drapeaux du sprite (`|=`) : le sprite prend la
  // couleur de son calque. Le type 7 réécrit le champ entier à zéro.
  CHECK(SpriteOnly(570).sprite_flags == 0x200U);
  CHECK(SpriteOnly(583).sprite_flags == 0U);
}

TEST_CASE("Fabrique : une action TIRÉE se tire", "[render][table]") {
  // Type 61 : `rand() % 8`. Type 15 : pile ou face entre 0 et 4.
  CHECK(SpriteOnly(912).action == 0);
  CHECK(SpriteOnly(912).action_high == 7);
  CHECK(SpriteOnly(630).action == 0);
  CHECK(SpriteOnly(630).action_alternate == 4);
  // Et le type 30 attend le trentième tic de l'effet.
  CHECK(SpriteOnly(648).first_tick == 30);
}

TEST_CASE("Fabrique : un type que le cas passait par REGISTRE a sa ligne", "[render][table]") {
  // 584 passe son type (8) par registre et non en valeur immédiate : il a
  // pourtant sa ligne.
  CHECK(EffectNodePatternsFor(584).size() == 1);
  CHECK(EffectNodePatternsFor(630).size() == 1);
}
