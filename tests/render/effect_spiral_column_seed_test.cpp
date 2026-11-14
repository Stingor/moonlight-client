#include "ro/render/effect_spiral_column_seed.h"

#include <algorithm>
#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_spiral_column.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_burst.h"

using Catch::Approx;
using ro::render::EffectSpiralColumnPattern;
using ro::render::EffectSpiralColumnPatternsFor;
using ro::render::EffectSpiralColumnPatternTable;
using ro::render::EffectSpiralColumnVariantFor;
using ro::render::EffectTrack;
using ro::render::kEffectColumnOpensAtTick;
using ro::render::kEffectTracksPerNode;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectSpiralColumnTracks;
using ro::render::SowEffectSpiralColumnNode;

namespace {

[[nodiscard]] EffectSpiralColumnPattern Pattern(std::uint8_t style, bool aimed = false) {
  return EffectSpiralColumnPattern{0, "essai", "image.tga", style, aimed};
}

}  // namespace

TEST_CASE("les quatre colonnes s'ecartent d'un quart de tour", "[render][effect][column]") {
  std::uint32_t seed = 5U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpiralColumnTracks(Pattern(3), {}, seed);
  CHECK(tracks[0].azimuth_degrees == 0);
  CHECK(tracks[1].azimuth_degrees == 90);
  CHECK(tracks[2].azimuth_degrees == 180);
  CHECK(tracks[3].azimuth_degrees == 270);
  for (const EffectTrack& track : tracks) {
    CHECK(track.active);
    CHECK(track.degrees == 360);
    // ⚠ L'INCLINAISON DROITE EST CE QUI FAIT LA COLONNE : couchée, ce serait une
    // jupe d'anneau ordinaire.
    CHECK(track.tilt_degrees == 90);
    // Un cercle minuscule, et une hauteur énorme.
    CHECK(track.radius_at_anchor < 0.35F);
    CHECK(track.lift_key > 20.0F);
    // Rien n'est semé dans les cases : c'est le mouvement qui ouvre.
    CHECK(track.value[0] == 0.0F);
    CHECK(track.mark[0] == 0);
  }
}

TEST_CASE("le retard est tire, et il decale les quatre colonnes", "[render][effect][column]") {
  std::uint32_t seed = 9U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpiralColumnTracks(Pattern(3), {}, seed);
  std::set<std::int32_t> retards;
  for (const EffectTrack& track : tracks) {
    // ⚠ LE COMPTEUR NAÎT DÉJÀ HAUT, entre soixante et quatre-vingt-dix-neuf : le
    // mouvement ne montrant rien avant le centième tic, la piste attend donc de
    // UN à QUARANTE tics. Le semer à zéro ferait attendre cent tics à tout le
    // monde, et les quatre colonnes partiraient ensemble.
    CHECK(track.ticks >= 60);
    CHECK(track.ticks < kEffectColumnOpensAtTick);
    retards.insert(track.ticks);
  }
  CHECK(retards.size() > 1);
}

TEST_CASE("le style en escalier monte de colonne en colonne", "[render][effect][column]") {
  std::uint32_t seed = 3U;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectSpiralColumnTracks(Pattern(11), {}, seed);
  // Soixante, puis vingt de plus par étage : c'est un escalier, pas quatre
  // colonnes égales.
  CHECK(tracks[0].lift_key == Approx(60.0F));
  CHECK(tracks[1].lift_key == Approx(80.0F));
  CHECK(tracks[3].lift_key == Approx(120.0F));
  CHECK(tracks[0].alpha == 50);
}

TEST_CASE("seul le batisseur qui vise pose une cible", "[render][effect][column]") {
  std::uint32_t seed = 7U;
  const glm::vec3 target{30.0F, 5.0F, -20.0F};
  const std::array<EffectTrack, kEffectTracksPerNode> droites =
      SeedEffectSpiralColumnTracks(Pattern(10, false), target, seed);
  seed = 7U;
  const std::array<EffectTrack, kEffectTracksPerNode> penchees =
      SeedEffectSpiralColumnTracks(Pattern(10, true), target, seed);
  // ⚠ C'EST LA SEULE DIFFÉRENCE ENTRE LES DEUX BÂTISSEURS : trois flottants.
  CHECK(droites[0].tail[6] == 0.0F);
  CHECK(droites[0].tail[8] == 0.0F);
  CHECK(penchees[0].tail[6] == Approx(30.0F));
  CHECK(penchees[0].tail[7] == Approx(5.0F));
  CHECK(penchees[0].tail[8] == Approx(-20.0F));
}

TEST_CASE("la variante decoule de l'octet, et se tire pour deux styles",
          "[render][effect][column]") {
  std::uint32_t seed = 11U;
  // Les styles fixes.
  CHECK(EffectSpiralColumnVariantFor(2, false, seed) == 7);
  CHECK(EffectSpiralColumnVariantFor(3, false, seed) == 6);
  CHECK(EffectSpiralColumnVariantFor(11, false, seed) == 5);
  CHECK(EffectSpiralColumnVariantFor(0, false, seed) == 4);
  // ⚠ UN OCTET NON NOMMÉ GARDE LE UN DU CONSTRUCTEUR, et le bâtisseur qui vise
  // ne nomme pas le douze.
  CHECK(EffectSpiralColumnVariantFor(5, false, seed) == 1);
  CHECK(EffectSpiralColumnVariantFor(13, true, seed) == 1);
  CHECK(EffectSpiralColumnVariantFor(12, true, seed) == 1);

  // ⚠ LE STYLE DIX TIRE ENTRE QUATRE ET HUIT.
  std::set<std::uint8_t> dispersees;
  for (int attempt = 0; attempt < 32; ++attempt) {
    const std::uint8_t variant = EffectSpiralColumnVariantFor(10, false, seed);
    CHECK(variant >= 4);
    CHECK(variant <= 8);
    dispersees.insert(variant);
  }
  CHECK(dispersees.size() > 1);

  // ⚠ ET LE STYLE DOUZE TIRE ENTRE TROIS VALEURS QUI NE SE SUIVENT PAS : quatre,
  // quarante, quarante et un. Les prendre pour une plage donnerait trente-sept
  // teintes que le client n'emploie jamais.
  std::set<std::uint8_t> three;
  for (int attempt = 0; attempt < 48; ++attempt) {
    three.insert(EffectSpiralColumnVariantFor(12, false, seed));
  }
  CHECK(three == std::set<std::uint8_t>{4, 40, 41});
}

TEST_CASE("la table des colonnes tient ce qu'elle a mesure", "[render][effect][column]") {
  const std::span<const EffectSpiralColumnPattern> table = EffectSpiralColumnPatternTable();
  REQUIRE(table.size() == 8);
  for (const EffectSpiralColumnPattern& pattern : table) {
    CHECK_FALSE(pattern.texture.empty());
    CHECK_FALSE(pattern.client_function.empty());
  }
  CHECK(std::ranges::is_sorted(table, {}, &EffectSpiralColumnPattern::effect_id));

  // ⚠ ET LE RELEVÉ SE TROMPAIT SUR QUATRE DES HUIT : son champ `echelle_sprite`
  // rend quatre à tout le monde. La mesure au site rend l'octet, dont la
  // variante découle — et pour EF_FIREPILLARON2 ce n'est pas quatre.
  REQUIRE(EffectSpiralColumnPatternsFor(718).size() == 1);
  CHECK(EffectSpiralColumnPatternsFor(718).front().style == 11);
  std::uint32_t seed = 1U;
  CHECK(EffectSpiralColumnVariantFor(EffectSpiralColumnPatternsFor(718).front().style, false,
                                     seed) == 5);

  // Les cinq du bâtisseur qui vise partagent un seul site d'appel, donc les
  // mêmes arguments.
  for (const std::uint32_t effect_id : {804U, 970U, 971U, 972U, 973U}) {
    REQUIRE(EffectSpiralColumnPatternsFor(effect_id).size() == 1);
    CHECK(EffectSpiralColumnPatternsFor(effect_id).front().aims_at_target);
    CHECK(EffectSpiralColumnPatternsFor(effect_id).front().style == 10);
  }
  CHECK_FALSE(EffectSpiralColumnPatternsFor(800).front().aims_at_target);
  CHECK(EffectSpiralColumnPatternsFor(1).empty());
}

TEST_CASE("le noeud seme rejoint la troupe et s'ouvre", "[render][effect][column]") {
  std::uint32_t seed = 13U;
  ro::render::EffectNodePool pool;
  pool.Add(SowEffectSpiralColumnNode(EffectSpiralColumnPatternsFor(800).front(),
                                     glm::vec3{1.0F, 0.0F, 2.0F}, glm::vec3{0.0F, 0.0F, 0.0F},
                                     ro::render::kEffectNodeDefaultLifeTicks, seed));
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes().front().shape == PooledNodeShape::kSpiralColumn);
  CHECK(pool.nodes().front().trail_variant == 6);
}
