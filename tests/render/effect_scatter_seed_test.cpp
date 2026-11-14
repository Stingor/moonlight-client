#include "ro/render/effect_scatter_seed.h"

#include <algorithm>
#include <cstdint>
#include <span>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_tinted_quad.h"

using ro::render::ComposeTintedRotatedQuad;
using ro::render::EffectNodePool;
using ro::render::EffectScatterNodePattern;
using ro::render::EffectScatterNodePatternsFor;
using ro::render::EffectScatterNodePatternTable;
using ro::render::EffectScatterStyleSeed;
using ro::render::EffectScatterStyleSeedFor;
using ro::render::EffectScatterStyleSeedTable;
using ro::render::EffectScatterStyleTextures;
using ro::render::EffectScatterWorld;
using ro::render::EffectTintedQuadStyleFor;
using ro::render::EffectTrack;
using ro::render::kEffectNodeDefaultLifeTicks;
using ro::render::kEffectTracksPerNode;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectScatterTracks;
using ro::render::SowEffectScatterNode;

namespace {

constexpr std::size_t kSlotOfStyle = 7;
constexpr std::size_t kMarkOfDrawing = 2;

// L'effet qui sème QUATRE champs, à quatre styles — le seul de la table.
constexpr std::uint32_t kEffectWithFourFields = 1115;
// Le style de brouillard, le seul dont les textures diffèrent des nuages.
constexpr std::uint8_t kStyleOfFog = 3;
// Les deux styles qui sèment un champ d'ÉTOILES, à six images.
constexpr std::uint8_t kFirstStyleOfStars = 9;
constexpr std::uint8_t kSecondStyleOfStars = 15;
constexpr std::size_t kStarCount = 6;
// Le style dont la marque de dessin se pose SANS BRANCHER, et ce qu'elle vaut.
constexpr std::uint8_t kStyleWithComputedMark = 14;
constexpr std::uint8_t kComputedMark = 24;
// Un style qu'aucun site d'appel n'emploie.
constexpr std::uint8_t kStyleNobodyCalls = 200;
// La plus courte vie qu'une particule puisse tirer.
constexpr std::int16_t kShortestLifeTicks = 300;

}  // namespace

TEST_CASE("Champs : la table des nœuds est TRIÉE par identifiant", "[render][table]") {
  const std::span<const EffectScatterNodePattern> table = EffectScatterNodePatternTable();
  REQUIRE(!table.empty());
  for (std::size_t rang = 1; rang < table.size(); ++rang) {
    CHECK(table[rang - 1].effect_id <= table[rang].effect_id);
  }
}

TEST_CASE("Champs : tout nœud nomme un style que la table des styles porte", "[render][table]") {
  for (const EffectScatterNodePattern& pattern : EffectScatterNodePatternTable()) {
    INFO("effet " << pattern.effect_id << ", style " << static_cast<int>(pattern.style));
    CHECK(EffectScatterStyleSeedFor(pattern.style) != nullptr);
  }
  CHECK(EffectScatterStyleSeedFor(kStyleNobodyCalls) == nullptr);
}

TEST_CASE("Champs : TOUT style charge au moins une texture, et plusieurs en mêlent",
          "[render][table]") {
  // ⚠ LES TEXTURES VIENNENT DU BÂTISSEUR, pas du site d'appel : c'est le premier
  // de la famille dans ce cas. Un style sans texture serait un champ invisible.
  std::size_t melangent = 0;
  for (const EffectScatterStyleSeed& seed : EffectScatterStyleSeedTable()) {
    INFO("style " << static_cast<int>(seed.style));
    const std::span<const std::string_view> textures = EffectScatterStyleTextures(seed);
    REQUIRE_FALSE(textures.empty());
    for (const std::string_view path : textures) {
      CHECK(path.find('.') != std::string_view::npos);
    }
    if (textures.size() > 1) {
      ++melangent;
    }
  }
  // Le client TIRE AU SORT entre elles ; aucun style n'en a une seule.
  CHECK(melangent == EffectScatterStyleSeedTable().size());
}

TEST_CASE("Champs : les étoiles sont SIX, et le brouillard n'est pas le nuage", "[render][table]") {
  const EffectScatterStyleSeed* etoiles = EffectScatterStyleSeedFor(kFirstStyleOfStars);
  const EffectScatterStyleSeed* autres = EffectScatterStyleSeedFor(kSecondStyleOfStars);
  const EffectScatterStyleSeed* brouillard = EffectScatterStyleSeedFor(kStyleOfFog);
  REQUIRE(etoiles != nullptr);
  REQUIRE(autres != nullptr);
  REQUIRE(brouillard != nullptr);
  CHECK(EffectScatterStyleTextures(*etoiles).size() == kStarCount);
  CHECK(EffectScatterStyleTextures(*autres).size() == kStarCount);
  // Deux styles partagent la même tranche : elle n'est rangée qu'une fois.
  CHECK(etoiles->first_texture == autres->first_texture);
  CHECK(EffectScatterStyleTextures(*brouillard).front() !=
        EffectScatterStyleTextures(*etoiles).front());
}

TEST_CASE("Champs : la marque posée SANS BRANCHER vaut vingt-quatre, et elle dessine",
          "[render][table]") {
  // Le relevé ne sait pas lire cette sélection ; elle est transcrite sous son
  // adresse. Le moins qu'on doive vérifier est qu'elle dessine vraiment.
  const EffectScatterStyleSeed* seed = EffectScatterStyleSeedFor(kStyleWithComputedMark);
  REQUIRE(seed != nullptr);
  CHECK(seed->draw_mark == kComputedMark);
  CHECK(EffectTintedQuadStyleFor(seed->draw_mark, 0).draws);
}

TEST_CASE("Champs : les QUATRE pistes naissent vives et ÉTEINTES", "[render][track]") {
  const EffectScatterStyleSeed* seed = EffectScatterStyleSeedFor(kStyleOfFog);
  REQUIRE(seed != nullptr);
  std::uint32_t generator = 1;
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectScatterTracks(*seed, generator);
  for (const EffectTrack& track : tracks) {
    CHECK(track.active);
    CHECK(track.alpha == 0);
    CHECK(track.value[kSlotOfStyle] == Catch::Approx(static_cast<float>(seed->style)));
    CHECK(track.mark[kMarkOfDrawing] == seed->draw_mark);
  }
}

TEST_CASE("Champs : chaque piste tire SA vie, et la nuée s'échelonne", "[render][track]") {
  // ⚠ C'EST CE QUI SÉPARE UNE NUÉE D'UN CLIGNOTEMENT. Le compteur naît à la plus
  // courte des vies ; une particule dont la vie tirée est plus longue reste donc
  // muette jusqu'à ce que le compteur la dépasse. Quatre vies, quatre entrées en
  // scène. Les semer identiques les ferait toutes paraître ensemble.
  const EffectScatterStyleSeed* seed = EffectScatterStyleSeedFor(kStyleOfFog);
  REQUIRE(seed != nullptr);
  std::uint32_t generator = 3;
  const auto tracks = SeedEffectScatterTracks(*seed, generator);
  std::size_t distinctes = 0;
  for (std::size_t rang = 0; rang < tracks.size(); ++rang) {
    CHECK(tracks[rang].azimuth_degrees >= kShortestLifeTicks);
    CHECK(tracks[rang].ticks == kShortestLifeTicks);
    bool view = false;
    for (std::size_t other = 0; other < rang; ++other) {
      view = view || tracks[other].azimuth_degrees == tracks[rang].azimuth_degrees;
    }
    distinctes += view ? 0 : 1;
  }
  CHECK(distinctes > 1);
}

TEST_CASE("Champs : le nœud semé porte ses textures et sa forme", "[render][pool]") {
  const std::span<const EffectScatterNodePattern> four =
      EffectScatterNodePatternsFor(kEffectWithFourFields);
  REQUIRE(four.size() == 4);
  std::uint32_t generator = 7;
  const ro::render::PooledEffectNode node = SowEffectScatterNode(
      four.front(), {2.0F, 1.0F, 3.0F}, kEffectNodeDefaultLifeTicks, generator);
  CHECK(node.shape == PooledNodeShape::kScatterField);
  CHECK(node.resource_paths.size() > 1);
  CHECK(node.node.life_ticks == kEffectNodeDefaultLifeTicks);
  CHECK(node.tracks.front().active);
}

TEST_CASE("Champs : la troupe finit par MONTRER le champ", "[render][pool]") {
  const std::span<const EffectScatterNodePattern> patterns =
      EffectScatterNodePatternsFor(kEffectWithFourFields);
  REQUIRE_FALSE(patterns.empty());
  std::uint32_t generator = 5;
  EffectNodePool pool;
  EffectScatterWorld world;
  world.ground_height = [](float, float) { return 0.0F; };
  pool.SetWorld(world);
  pool.Add(SowEffectScatterNode(patterns.front(), {}, kEffectNodeDefaultLifeTicks, generator));

  // ⚠ RIEN NE SE MONTRE AVANT QUE LE MOUVEMENT AIT TOURNÉ : l'alpha naît nul, et
  // la première vie s'écoule en silence. Un banc qui composerait sans faire
  // battre la troupe conclurait que la figure est vide.
  const EffectScatterStyleSeed* seed = EffectScatterStyleSeedFor(patterns.front().style);
  REQUIRE(seed != nullptr);
  bool shows = false;
  for (std::int32_t tick = 0; tick < kEffectNodeDefaultLifeTicks && !shows; ++tick) {
    pool.Advance(1000 / ro::render::kEffectTicksPerSecond);
    for (const ro::render::PooledEffectNode& node : pool.nodes()) {
      for (const EffectTrack& track : node.tracks) {
        shows = shows || ComposeTintedRotatedQuad(track, seed->draw_mark, 0, 0, 0).has_value();
      }
    }
  }
  CHECK(shows);
}

TEST_CASE("Champs : sur la carte qui plafonne, nuages et brouillard restent sous cent",
          "[render][pool]") {
  // 🔴 LE CHAMP DISPERSÉ EST DESSINÉ PAR LE CARRÉ TEINTÉ, et c'est la famille même
  // que le client lie à cette carte. Au style ordinaire, ses particules montent
  // de trois par tic pendant quatre-vingts tics : sans le plafond, elles
  // passaient deux cents.
  constexpr std::uint32_t kCloudsOfOrdinaryStyle = 230;
  constexpr std::int32_t kLongLifeTicks = 2000;
  constexpr std::int32_t kTicksToWatch = 600;
  const auto highest_alpha = [](bool capped) {
    EffectNodePool pool;
    EffectScatterWorld world;
    world.ground_height = [](float, float) { return 0.0F; };
    world.caps_tinted_quad_alpha = capped;
    pool.SetWorld(world);
    std::uint32_t generator = 5;
    const std::span<const EffectScatterNodePattern> patterns =
        EffectScatterNodePatternsFor(kCloudsOfOrdinaryStyle);
    REQUIRE(patterns.size() == 1);
    pool.Add(SowEffectScatterNode(patterns.front(), {}, kLongLifeTicks, generator));
    std::int16_t highest = 0;
    for (std::int32_t tick = 0; tick < kTicksToWatch; ++tick) {
      static_cast<void>(pool.Advance(1000 / ro::render::kEffectTicksPerSecond));
      for (const ro::render::PooledEffectNode& node : pool.nodes()) {
        for (const EffectTrack& track : node.tracks) {
          highest = std::max(highest, track.alpha);
        }
      }
    }
    return highest;
  };
  CHECK(highest_alpha(true) == ro::render::kTintedQuadAlphaCapOnThatMap);
  CHECK(highest_alpha(false) > ro::render::kTintedQuadAlphaCapOnThatMap);
}
