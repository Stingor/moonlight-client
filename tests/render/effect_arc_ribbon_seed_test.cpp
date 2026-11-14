#include "ro/render/effect_arc_ribbon_seed.h"

#include <array>
#include <cstdint>
#include <optional>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_arc_ribbon.h"
#include "ro/render/effect_node_pool.h"

using Catch::Approx;
using ro::render::ComposeArcRibbonQuads;
using ro::render::EffectArcRibbonNodePattern;
using ro::render::EffectArcRibbonNodePatternsFor;
using ro::render::EffectArcRibbonNodePatternTable;
using ro::render::EffectArcRibbonStyleSeed;
using ro::render::EffectArcRibbonStyleSeedFor;
using ro::render::EffectArcRibbonStyleSeedTable;
using ro::render::EffectNodePool;
using ro::render::EffectTrack;
using ro::render::kEffectNodeDefaultLifeTicks;
using ro::render::kEffectTracksPerNode;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectArcRibbonTracks;
using ro::render::SowEffectArcRibbonNode;

namespace {

// L'effet qui sème QUATRE arcs à quatre styles — le seul de la table.
constexpr std::uint32_t kEffectWithFourArcs = 681;
// Deux styles que le relevé sépare et qu'une chaîne de gardes confondait : même
// rayon, mais un angle de départ et une forme différents.
constexpr std::uint8_t kStyleOfTarot = 1;
constexpr std::uint8_t kStyleOfSuccess = 3;
// Le style qui prend sa hauteur sur le sprite de l'acteur.
constexpr std::uint8_t kStyleOfActorHeight = 22;
// Un style qu'aucun site d'appel n'emploie : le bâtisseur porte sa branche.
constexpr std::uint8_t kStyleNobodyCalls = 99;

}  // namespace

TEST_CASE("Arcs : la table des nœuds est TRIÉE par identifiant", "[render][table]") {
  const std::span<const EffectArcRibbonNodePattern> table = EffectArcRibbonNodePatternTable();
  REQUIRE(!table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id <= table[rang].effect_id);
  }
}

TEST_CASE("Arcs : tout nœud de la table nomme un style que la table des styles porte",
          "[render][table]") {
  for (const EffectArcRibbonNodePattern& pattern : EffectArcRibbonNodePatternTable()) {
    INFO("effet " << pattern.effect_id << ", style " << static_cast<int>(pattern.style));
    CHECK(EffectArcRibbonStyleSeedFor(pattern.style) != nullptr);
  }
}

TEST_CASE("Arcs : un style qu'aucun site n'appelle n'a PAS de semis", "[render][table]") {
  // Inventer un semis pour une branche que personne n'atteint montrerait ce que
  // le client ne montre pas.
  CHECK(EffectArcRibbonStyleSeedFor(kStyleNobodyCalls) == nullptr);
}

TEST_CASE("Arcs : les styles DIFFÈRENT, ce qui est tout l'objet du parcours en avant",
          "[render][table]") {
  const EffectArcRibbonStyleSeed* tarot = EffectArcRibbonStyleSeedFor(kStyleOfTarot);
  const EffectArcRibbonStyleSeed* succes = EffectArcRibbonStyleSeedFor(kStyleOfSuccess);
  REQUIRE(tarot != nullptr);
  REQUIRE(succes != nullptr);
  // ⚠ C'EST LE TEST QUI AURAIT ATTRAPÉ LA TABLE FAUSSE. Une lecture à rebours
  // des chaînes de gardes donnait à ces deux styles le même angle de départ, la
  // même marque et la même forme — trente-six identifiants et une seule figure.
  CHECK(tarot->degrees != succes->degrees);
  CHECK(tarot->draw_mark != succes->draw_mark);
  CHECK(tarot->shape_key != succes->shape_key);
}

TEST_CASE("Arcs : la marque du MOUVEMENT n'est pas celle de la FIGURE", "[render][table]") {
  // ADR 0017 : le sens d'un champ appartient à son lecteur. Neuf styles sur
  // dix-huit portent une marque de dessin qui n'est pas la leur, et les
  // confondre donnerait la parure d'un style au mouvement d'un autre.
  std::size_t divergents = 0;
  for (const EffectArcRibbonStyleSeed& seed : EffectArcRibbonStyleSeedTable()) {
    if (seed.draw_mark != seed.style_mark) {
      ++divergents;
    }
    // La marque du mouvement EST le style, hormis le 4 qui se donne celle du 1.
    CHECK((seed.style_mark == seed.style || seed.style == 4));
  }
  CHECK(divergents > 0);
}

TEST_CASE("Arcs : le semis n'allume QU'UNE piste sur quatre", "[render][track]") {
  const EffectArcRibbonStyleSeed* seed = EffectArcRibbonStyleSeedFor(kStyleOfTarot);
  REQUIRE(seed != nullptr);
  std::uint32_t generator = 1;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectArcRibbonTracks(*seed, {10.0F, 0.0F, -4.0F}, generator);
  CHECK(tracks[0].active);
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    INFO("piste " << rang);
    CHECK_FALSE(tracks[rang].active);
  }
}

TEST_CASE("Arcs : l'élévation du style MONTE dans notre repère", "[render][track]") {
  const EffectArcRibbonStyleSeed* seed = EffectArcRibbonStyleSeedFor(kStyleOfTarot);
  REQUIRE(seed != nullptr);
  REQUIRE(seed->lift_above_effect > 0.0F);
  std::uint32_t generator = 1;
  const glm::vec3 point{10.0F, 12.0F, -4.0F};
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectArcRibbonTracks(*seed, point, generator);
  // ⚠ LE CLIENT RETRANCHE, ET POURTANT L'ARC MONTE : son repère descend. Ce test
  // a d'abord porté l'attente inverse, et il la portait parce que le code la
  // portait — deux fautes qui s'accordaient.
  CHECK(tracks[0].anchor.y == Catch::Approx(point.y + seed->lift_above_effect));
  CHECK(tracks[0].anchor.x == Catch::Approx(point.x));
  CHECK(tracks[0].anchor.z == Catch::Approx(point.z));
}

TEST_CASE("Arcs : le style qui lit l'acteur ne creuse RIEN plutôt qu'au hasard",
          "[render][track]") {
  const EffectArcRibbonStyleSeed* seed = EffectArcRibbonStyleSeedFor(kStyleOfActorHeight);
  REQUIRE(seed != nullptr);
  CHECK(seed->lift_from_actor_height);
  // Nous ne portons pas la hauteur de frame : mieux vaut un arc au pied qu'un
  // arc posé à une hauteur inventée.
  CHECK(seed->lift_above_effect == Catch::Approx(0.0F));
}

TEST_CASE("Arcs : l'angle tiré change d'un semis à l'autre, l'angle posé jamais",
          "[render][track]") {
  const EffectArcRibbonStyleSeed* pose = EffectArcRibbonStyleSeedFor(kStyleOfTarot);
  REQUIRE(pose != nullptr);
  REQUIRE_FALSE(pose->degrees_drawn);
  std::uint32_t generator = 7;
  const auto first = SeedEffectArcRibbonTracks(*pose, {}, generator);
  const auto second = SeedEffectArcRibbonTracks(*pose, {}, generator);
  CHECK(first[0].degrees == pose->degrees);
  CHECK(second[0].degrees == pose->degrees);
  // L'azimut se tire pour TOUS les styles : deux nœuds semés à la suite ne se
  // superposent donc jamais exactement.
  CHECK(first[0].azimuth_degrees != second[0].azimuth_degrees);
}

TEST_CASE("Arcs : l'alpha naît NUL, et le nœud ne montre donc rien au premier tic",
          "[render][track]") {
  const EffectArcRibbonStyleSeed* seed = EffectArcRibbonStyleSeedFor(kStyleOfTarot);
  REQUIRE(seed != nullptr);
  std::uint32_t generator = 3;
  const auto tracks = SeedEffectArcRibbonTracks(*seed, {}, generator);
  CHECK(tracks[0].alpha == 0);
  CHECK(ComposeArcRibbonQuads(tracks[0], 0, 0).empty());
}

TEST_CASE("Arcs : le nœud semé porte sa forme, sa texture et la vie qu'on lui donne",
          "[render][pool]") {
  const std::span<const EffectArcRibbonNodePattern> four =
      EffectArcRibbonNodePatternsFor(kEffectWithFourArcs);
  REQUIRE(four.size() == 4);
  std::uint32_t generator = 11;
  const ro::render::PooledEffectNode node = SowEffectArcRibbonNode(
      four.front(), {1.0F, 2.0F, 3.0F}, kEffectNodeDefaultLifeTicks, generator);
  CHECK(node.shape == PooledNodeShape::kArcRibbons);
  CHECK(node.resource_paths.size() == 1);
  CHECK(node.node.life_ticks == kEffectNodeDefaultLifeTicks);
  CHECK(node.tracks[0].active);
}

TEST_CASE("Arcs : le mouvement de la troupe finit par MONTRER l'arc", "[render][pool]") {
  const std::span<const EffectArcRibbonNodePattern> patterns =
      EffectArcRibbonNodePatternsFor(kEffectWithFourArcs);
  REQUIRE_FALSE(patterns.empty());
  std::uint32_t generator = 5;
  EffectNodePool pool;
  pool.Add(SowEffectArcRibbonNode(patterns.front(), {}, kEffectNodeDefaultLifeTicks, generator));

  // ⚠ SANS LE MOUVEMENT L'ARC RESTE INVISIBLE : son alpha naît nul. Un banc qui
  // composerait sans faire battre la troupe conclurait que la figure est vide.
  bool shows = false;
  for (std::int32_t tick = 0; tick < kEffectNodeDefaultLifeTicks && !shows; ++tick) {
    pool.Advance(1000 / ro::render::kEffectTicksPerSecond);
    for (const ro::render::PooledEffectNode& node : pool.nodes()) {
      for (const EffectTrack& track : node.tracks) {
        shows = shows || !ComposeArcRibbonQuads(track, 0, 0).empty();
      }
    }
  }
  CHECK(shows);
}

TEST_CASE("Arc de ruban : la brûlure se pose au-dessus du corps de son porteur",
          "[render][arc_ribbon]") {
  // 🔴 LE STYLE 22 LIT LE PREMIER CALQUE DU CORPS : `4 − ordonnée × 0,29` au-dessus
  // de l'effet, trente sans corps lisible. Posée à zéro, la marque tombait aux
  // pieds, sur le sprite, comme une icône.
  CHECK(ro::render::ArcRibbonLiftFromBody(std::nullopt) == Approx(30.0F));
  CHECK(ro::render::ArcRibbonLiftFromBody(0.0F) == Approx(30.0F));
  CHECK(ro::render::ArcRibbonLiftFromBody(-100.0F) == Approx(33.0F));

  const EffectArcRibbonStyleSeed* burning = EffectArcRibbonStyleSeedFor(22);
  REQUIRE(burning != nullptr);
  std::uint32_t generator = 1;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectArcRibbonTracks(*burning, {0.0F, 10.0F, 0.0F}, generator, 0.0F, 33.0F);
  CHECK(tracks[0].anchor.y == Approx(43.0F));

  // ⚠ LES AUTRES STYLES NE LE LISENT PAS : leur élévation reste la leur.
  const EffectArcRibbonStyleSeed* other = EffectArcRibbonStyleSeedFor(1);
  REQUIRE(other != nullptr);
  const std::array<EffectTrack, kEffectTracksPerNode> plain =
      SeedEffectArcRibbonTracks(*other, {0.0F, 10.0F, 0.0F}, generator, 0.0F, 33.0F);
  CHECK(plain[0].anchor.y == Approx(10.0F + other->lift_above_effect));
}
