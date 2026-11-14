#include "ro/render/effect_falling_seed.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_tinted_quad.h"

using ro::render::AdvanceFallingTrailTracks;
using ro::render::ComposeTintedRotatedQuad;
using ro::render::EffectFallingNodePattern;
using ro::render::EffectFallingNodePatternsFor;
using ro::render::EffectFallingNodePatternTable;
using ro::render::EffectFallingStyleSeed;
using ro::render::EffectFallingStyleSeedFor;
using ro::render::EffectFallingStyleSeedTable;
using ro::render::EffectTintedQuadStyleFor;
using ro::render::EffectTrack;
using ro::render::kEffectNodeDefaultLifeTicks;
using ro::render::kEffectTracksPerNode;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectFallingTracks;
using ro::render::SowEffectFallingNode;

namespace {

constexpr std::size_t kMarkOfDrawing = 2;
constexpr std::size_t kMarkOfSubDrawing = 3;
constexpr std::size_t kMarkOfStyle = 4;
constexpr std::size_t kTailSlotOfHeight = 4;

// L'effet qui lance TROIS objets — le seul de la table.
constexpr std::uint32_t kEffectWithThreeItems = 930;
// Le seul style qui tire sa hauteur de départ, et un qui la pose.
constexpr std::uint8_t kStyleThatDrawsItsHeight = 4;
constexpr std::uint8_t kPlainStyle = 1;
constexpr std::uint8_t kStyleNobodyCalls = 200;

void Advance(std::array<EffectTrack, kEffectTracksPerNode>& tracks, std::int32_t ticks,
             glm::vec3 bearer = {}) {
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    (void)AdvanceFallingTrailTracks(tracks, bearer, 0, 0);
  }
}

}  // namespace

TEST_CASE("Chutes : la table des nœuds est TRIÉE par identifiant", "[render][table]") {
  const std::span<const EffectFallingNodePattern> table = EffectFallingNodePatternTable();
  REQUIRE(!table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id <= table[rang].effect_id);
  }
}

TEST_CASE("Chutes : tout nœud nomme un style porté, et toute marque dessine", "[render][table]") {
  for (const EffectFallingNodePattern& pattern : EffectFallingNodePatternTable()) {
    INFO("effet " << pattern.effect_id);
    CHECK(EffectFallingStyleSeedFor(pattern.style) != nullptr);
  }
  CHECK(EffectFallingStyleSeedFor(kStyleNobodyCalls) == nullptr);
  for (const EffectFallingStyleSeed& seed : EffectFallingStyleSeedTable()) {
    INFO("style " << static_cast<int>(seed.style));
    // ⚠ UNE MARQUE QUI NE DESSINE PAS serait un objet invisible : le style
    // n'aurait aucun moyen de le dire, et la table aurait l'air complète.
    CHECK(EffectTintedQuadStyleFor(seed.draw_mark, seed.sub_mark).draws);
  }
}

TEST_CASE("Chutes : un seul style TIRE sa hauteur, les autres la posent", "[render][table]") {
  std::size_t tirent = 0;
  for (const EffectFallingStyleSeed& seed : EffectFallingStyleSeedTable()) {
    CHECK(seed.lift_above_owner > 0.0F);
    if (seed.lift_spread > 0.0F) {
      ++tirent;
      CHECK(seed.style == kStyleThatDrawsItsHeight);
    }
  }
  CHECK(tirent == 1);
}

TEST_CASE("Chutes : les QUATRE pistes naissent vives, et TROIS sont nues", "[render][track]") {
  // ⚠ C'EST LE MOUVEMENT QUI LES REMPLIT, un cran par tic. Les garnir au semis
  // donnerait quatre objets au lieu d'un objet et de sa traînée.
  const EffectFallingStyleSeed* seed = EffectFallingStyleSeedFor(kPlainStyle);
  REQUIRE(seed != nullptr);
  std::uint32_t generator = 1;
  const auto tracks = SeedEffectFallingTracks(*seed, generator);
  for (const EffectTrack& track : tracks) {
    CHECK(track.active);
    CHECK(track.alpha == 0);
  }
  CHECK(tracks[0].mark[kMarkOfStyle] == seed->style);
  CHECK(tracks[0].mark[kMarkOfDrawing] == seed->draw_mark);
  CHECK(tracks[0].mark[kMarkOfSubDrawing] == seed->sub_mark);
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    INFO("piste " << rang);
    CHECK(tracks[rang].mark[kMarkOfStyle] == 0);
    CHECK(tracks[rang].radius_at_anchor == Catch::Approx(0.0F));
  }
}

TEST_CASE("Chutes : trois lances tirées partent de trois hauteurs", "[render][track]") {
  // ⚠ SANS CE TIRAGE LES TROIS SE SUPERPOSERAIENT. L'effet 930 les lance
  // ensemble, et c'est la hauteur qui les sépare.
  const EffectFallingStyleSeed* seed = EffectFallingStyleSeedFor(kStyleThatDrawsItsHeight);
  REQUIRE(seed != nullptr);
  std::uint32_t generator = 9;
  const auto first = SeedEffectFallingTracks(*seed, generator);
  const auto second = SeedEffectFallingTracks(*seed, generator);
  const auto troisieme = SeedEffectFallingTracks(*seed, generator);
  const float a = first[0].tail[kTailSlotOfHeight];
  const float b = second[0].tail[kTailSlotOfHeight];
  const float c = troisieme[0].tail[kTailSlotOfHeight];
  CHECK(a >= seed->lift_above_owner);
  CHECK(b >= seed->lift_above_owner);
  CHECK(c >= seed->lift_above_owner);
  CHECK_FALSE((a == b && b == c));
}

TEST_CASE("Chutes : l'objet DESCEND vers son porteur, malgré le nom", "[render][track]") {
  // Le bâtisseur s'appelle « RisingSprite » et son sprite se rapproche du sol.
  const EffectFallingStyleSeed* seed = EffectFallingStyleSeedFor(kPlainStyle);
  REQUIRE(seed != nullptr);
  std::uint32_t generator = 2;
  auto tracks = SeedEffectFallingTracks(*seed, generator);
  const float start = tracks[0].tail[kTailSlotOfHeight];
  Advance(tracks, 5);
  CHECK(tracks[0].tail[kTailSlotOfHeight] < start);
  CHECK(tracks[0].anchor.y < start);
}

TEST_CASE("Chutes : la traînée SE DÉPLIE d'un cran par tic", "[render][track]") {
  // ⚠ ET NON D'UN BLOC. Les faire toutes sauter au même tic donnerait quatre
  // copies superposées au premier tic, puis plus rien de neuf.
  const EffectFallingStyleSeed* seed = EffectFallingStyleSeedFor(kPlainStyle);
  REQUIRE(seed != nullptr);
  std::uint32_t generator = 4;
  auto tracks = SeedEffectFallingTracks(*seed, generator);
  Advance(tracks, 2);
  CHECK(tracks[1].radius_at_anchor > 0.0F);
  CHECK(tracks[2].radius_at_anchor == Catch::Approx(0.0F));
  Advance(tracks, 1);
  CHECK(tracks[2].radius_at_anchor > 0.0F);
  CHECK(tracks[3].radius_at_anchor == Catch::Approx(0.0F));
  Advance(tracks, 1);
  CHECK(tracks[3].radius_at_anchor > 0.0F);
}

TEST_CASE("Chutes : la tête est plus VIVE que sa traînée", "[render][track]") {
  const EffectFallingStyleSeed* seed = EffectFallingStyleSeedFor(kPlainStyle);
  REQUIRE(seed != nullptr);
  std::uint32_t generator = 6;
  auto tracks = SeedEffectFallingTracks(*seed, generator);
  Advance(tracks, 8);
  CHECK(tracks[0].alpha > tracks[1].alpha);
  CHECK(tracks[1].alpha >= tracks[2].alpha);
  for (const EffectTrack& track : tracks) {
    CHECK(track.alpha >= 0);
  }
}

TEST_CASE("Chutes : l'objet finit par S'EFFACER", "[render][track]") {
  const EffectFallingStyleSeed* seed = EffectFallingStyleSeedFor(kPlainStyle);
  REQUIRE(seed != nullptr);
  std::uint32_t generator = 8;
  auto tracks = SeedEffectFallingTracks(*seed, generator);
  Advance(tracks, 20);
  const std::int16_t full = tracks[0].alpha;
  CHECK(full > 0);
  Advance(tracks, 120);
  CHECK(tracks[0].alpha == 0);
}

TEST_CASE("Chutes : le style qui se pose SONNE, et cesse d'être lui-même", "[render][track]") {
  const EffectFallingStyleSeed* seed = EffectFallingStyleSeedFor(kStyleThatDrawsItsHeight);
  REQUIRE(seed != nullptr);
  std::uint32_t generator = 11;
  auto tracks = SeedEffectFallingTracks(*seed, generator);
  bool sonne = false;
  for (std::int32_t tick = 0; tick < 200 && !sonne; ++tick) {
    sonne = AdvanceFallingTrailTracks(tracks, {}, 0, 0);
  }
  CHECK(sonne);
  // En touchant, sa marque de style retombe à zéro : il finit comme les autres.
  CHECK(tracks[0].mark[kMarkOfStyle] == 0);
}

TEST_CASE("Chutes : le nœud semé porte sa forme et sa texture", "[render][pool]") {
  const std::span<const EffectFallingNodePattern> three =
      EffectFallingNodePatternsFor(kEffectWithThreeItems);
  REQUIRE(three.size() == 3);
  std::uint32_t generator = 3;
  const ro::render::PooledEffectNode node = SowEffectFallingNode(
      three.front(), {1.0F, 0.0F, 2.0F}, kEffectNodeDefaultLifeTicks, generator);
  CHECK(node.shape == PooledNodeShape::kFallingItem);
  CHECK(node.resource_paths.size() == 1);
  CHECK(node.tracks.front().active);
}

TEST_CASE("Chutes : la troupe finit par MONTRER l'objet", "[render][pool]") {
  const std::span<const EffectFallingNodePattern> patterns =
      EffectFallingNodePatternsFor(kEffectWithThreeItems);
  REQUIRE_FALSE(patterns.empty());
  const EffectFallingStyleSeed* seed = EffectFallingStyleSeedFor(patterns.front().style);
  REQUIRE(seed != nullptr);
  std::uint32_t generator = 13;
  ro::render::EffectNodePool pool;
  pool.Add(SowEffectFallingNode(patterns.front(), {}, kEffectNodeDefaultLifeTicks, generator));
  bool shows = false;
  for (std::int32_t tick = 0; tick < 40 && !shows; ++tick) {
    pool.Advance(1000 / ro::render::kEffectTicksPerSecond);
    for (const ro::render::PooledEffectNode& node : pool.nodes()) {
      for (const EffectTrack& track : node.tracks) {
        shows = shows ||
                ComposeTintedRotatedQuad(track, seed->draw_mark, seed->sub_mark, 0, 0).has_value();
      }
    }
  }
  CHECK(shows);
}
