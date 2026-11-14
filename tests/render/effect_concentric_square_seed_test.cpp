#include "ro/render/effect_concentric_square_seed.h"

#include <algorithm>
#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_concentric_square.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::EffectConcentricSquareHeadingFor;
using ro::render::EffectConcentricSquareMarkFor;
using ro::render::EffectConcentricSquarePattern;
using ro::render::EffectConcentricSquarePatternsFor;
using ro::render::EffectConcentricSquarePatternTable;
using ro::render::EffectTrack;
using ro::render::kEffectTracksPerNode;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectConcentricSquareTracks;
using ro::render::SowEffectConcentricSquareNode;

TEST_CASE("les halos vont aux premiers noeuds d'une GRANDE gerbe", "[render][effect][concentric]") {
  // ⚠ LE COMPTE EST LE BRUT, NON BORNE A CINQ. Une gerbe de douze porte des
  // halos ; une gerbe de trois n'en porte aucun. Croire le compte plafonne
  // ferait conclure que ces marques ne servent jamais, et l'on retirerait les
  // deux couches larges de la figure.
  CHECK(EffectConcentricSquareMarkFor(12, 0, 0) == 21);
  CHECK(EffectConcentricSquareMarkFor(12, 1, 0) == 21);
  // Passe le premier seuil, il ne reste plus assez de noeuds pour le grand halo.
  CHECK(EffectConcentricSquareMarkFor(12, 5, 0) == 20);
  // Et une petite gerbe n'en a aucun.
  CHECK(EffectConcentricSquareMarkFor(3, 0, 0) == 0);
}

TEST_CASE("la marque suit d'abord l'octet du site", "[render][effect][concentric]") {
  // L'enfant n'en a aucune, les pieces la marque un, meme en grande gerbe.
  CHECK(EffectConcentricSquareMarkFor(12, 0, 1) == 0);
  CHECK(EffectConcentricSquareMarkFor(12, 0, 3) == 1);
  CHECK(EffectConcentricSquareMarkFor(2, 0, 4) == 1);
  // ⚠ UN ELEMENT GARDE SON OCTET, et le halo qu'un grand compte lui donnerait
  // est ecrase.
  CHECK(EffectConcentricSquareMarkFor(12, 0, 13) == 13);
  CHECK(EffectConcentricSquareMarkFor(5, 0, 10) == 10);
}

TEST_CASE("le cap avance de soixante-douze degres par rang", "[render][effect][concentric]") {
  // Les spheres du Monk, quel que soit leur compte.
  CHECK(EffectConcentricSquareHeadingFor(3, 1, 0) == Approx(72.0F));
  CHECK(EffectConcentricSquareHeadingFor(12, 3, 0) == Approx(216.0F));
  // ⚠ SEULES CINQ PIECES AU PLUS se partagent le tour entier.
  CHECK(EffectConcentricSquareHeadingFor(4, 1, 3) == Approx(90.0F));
  CHECK(EffectConcentricSquareHeadingFor(2, 1, 4) == Approx(180.0F));
  CHECK(EffectConcentricSquareHeadingFor(8, 2, 3) == Approx(144.0F));
}

TEST_CASE("chaque branche pose sa hauteur, son rayon et sa cle", "[render][effect][concentric]") {
  std::uint32_t seed = 9U;
  const auto first = [&seed](std::uint8_t style, std::int32_t count) {
    const EffectConcentricSquarePattern pattern{0, "essai", "image.bmp", style};
    return SeedEffectConcentricSquareTracks(pattern, count, 0, seed)[0];
  };
  // Les spheres du Monk : quinze unites plus haut, rayon deux.
  const EffectTrack monk = first(0, 3);
  CHECK(monk.value[7] == Approx(-15.0F));
  CHECK(monk.radius_at_anchor == Approx(2.0F));
  CHECK(monk.value[10] == Approx(0.0F));
  // L'enfant : quatorze, un et trois dixiemes, et la cle du facteur.
  const EffectTrack child = first(1, 3);
  CHECK(child.value[7] == Approx(-14.0F));
  CHECK(child.radius_at_anchor == Approx(1.3F));
  CHECK(child.value[10] == Approx(1.0F));
  // Les pieces : rayon un, sauf les premieres d'une grande gerbe.
  CHECK(first(3, 3).radius_at_anchor == Approx(1.0F));
  CHECK(first(3, 8).radius_at_anchor == Approx(2.0F));
  // Un element : rayon six.
  CHECK(first(12, 3).radius_at_anchor == Approx(6.0F));
}

TEST_CASE("le semis tire SEPT angles et les range", "[render][effect][concentric]") {
  const EffectConcentricSquarePattern pattern{0, "essai", "image.bmp", 10};
  std::uint32_t seed = 23U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectConcentricSquareTracks(pattern, 3, 0, seed);
  const EffectTrack& track = tracks[0];
  CHECK(track.active);
  // Les six du tremblement et le septieme du souffle, tous dans le tour.
  std::set<float> tires;
  for (const std::size_t rang : {0U, 1U, 2U, 3U, 4U, 5U, 8U}) {
    CHECK(track.value[rang] >= 0.0F);
    CHECK(track.value[rang] < 360.0F);
    tires.insert(track.value[rang]);
  }
  // ⚠ SEPT TIRAGES DISTINCTS : s'ils etaient egaux, les trois paires du
  // tremblement marcheraient ensemble et la figure ne tremblerait pas.
  CHECK(tires.size() > 1);
}

TEST_CASE("la table des carres porte l'image que l'octet choisit", "[render][effect][concentric]") {
  const std::span<const EffectConcentricSquarePattern> table = EffectConcentricSquarePatternTable();
  REQUIRE(table.size() == 8);
  CHECK(std::ranges::is_sorted(table, {}, &EffectConcentricSquarePattern::effect_id));

  // ⚠ LE RELEVE DONNAIT `thunder_center.bmp` AUX HUIT, et cinq en ont une autre.
  // C'est le piege de la branche par defaut, et il se reconnait a ce que la
  // valeur relevee est plausible : trois des huit ont bien cette image.
  const auto image = [&](std::uint32_t effect_id) {
    return EffectConcentricSquarePatternsFor(effect_id).front().texture;
  };
  CHECK(image(735) == "data\\texture\\effect\\fire-1.bmp");
  CHECK(image(736) == "data\\texture\\effect\\lightning-1.bmp");
  CHECK(image(737) == "data\\texture\\effect\\water-1.bmp");
  CHECK(image(738) == "data\\texture\\effect\\stone-1.bmp");
  CHECK(image(1135) == "data\\texture\\effect\\rcoin1.bmp");
  CHECK(image(228) == "data\\texture\\effect\\thunder_center.bmp");

  CHECK(EffectConcentricSquarePatternsFor(1).empty());
}

TEST_CASE("le noeud seme rejoint la troupe", "[render][effect][concentric]") {
  std::uint32_t seed = 31U;
  ro::render::EffectNodePool pool;
  pool.Add(SowEffectConcentricSquareNode(EffectConcentricSquarePatternsFor(735).front(), 1, 0,
                                         glm::vec3{2.0F, 0.0F, 3.0F},
                                         ro::render::kEffectNodeDefaultLifeTicks, seed));
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes().front().shape == PooledNodeShape::kConcentricSquare);
  CHECK(pool.nodes().front().tracks[0].active);
}
