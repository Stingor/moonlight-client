#include "ro/render/effect_ground_ribbon.h"

#include <algorithm>
#include <cmath>
#include <set>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_ground_ribbon_seed.h"
#include "ro/render/effect_node_pool.h"

using Catch::Approx;
using ro::render::ComposeGroundRibbonQuad;
using ro::render::EffectGroundRibbonBuilder;
using ro::render::EffectGroundRibbonPattern;
using ro::render::EffectGroundRibbonPatternsFor;
using ro::render::EffectGroundRibbonPatternTable;
using ro::render::EffectGroundRibbonStyleFor;
using ro::render::EffectGroundRibbonTextureFor;
using ro::render::EffectNodePool;
using ro::render::EffectTrack;
using ro::render::EffectWorldQuad;
using ro::render::kEffectTracksPerNode;
using ro::render::kGroundRibbonHeight;
using ro::render::kMarkOfRibbonStyle;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectGroundRibbonTracks;
using ro::render::SowEffectGroundRibbonNode;

namespace {

constexpr glm::vec3 kAtTheOrigin{0.0F, 0.0F, 0.0F};

[[nodiscard]] float QuadArea(const EffectWorldQuad& quad) {
  const glm::vec3 ab = quad.corners[1] - quad.corners[0];
  const glm::vec3 ac = quad.corners[2] - quad.corners[0];
  const glm::vec3 ad = quad.corners[3] - quad.corners[0];
  return (glm::length(glm::cross(ab, ac)) + glm::length(glm::cross(ac, ad))) * 0.5F;
}

// Les pistes d'un patron, avancées par le mouvement — elles naissent invisibles.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> Advanced(
    const EffectGroundRibbonPattern& pattern, std::int32_t ticks, std::int16_t cap = 0) {
  std::uint32_t seed = 17U;
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      SeedEffectGroundRibbonTracks(pattern, cap, seed);
  for (std::int32_t tick = 0; tick < ticks; ++tick) {
    ro::render::AdvanceGroundRibbonTracks(tracks);
  }
  return tracks;
}

}  // namespace

TEST_CASE("le ruban naît invisible et les quatre pistes sont vives", "[render][ground_ribbon]") {
  const EffectGroundRibbonPattern& pattern = EffectGroundRibbonPatternsFor(439).front();
  std::uint32_t seed = 3U;
  const std::array<EffectTrack, kEffectTracksPerNode> neuves =
      SeedEffectGroundRibbonTracks(pattern, 0, seed);
  for (const EffectTrack& track : neuves) {
    CHECK(track.active);
    CHECK(track.alpha == 0);
    CHECK_FALSE(ComposeGroundRibbonQuad(track, kAtTheOrigin).has_value());
  }
  // ⚠ ET LEURS TIRAGES DIFFÈRENT : quatre rubans, pas quatre fois le même.
  std::set<float> cles;
  for (const EffectTrack& track : neuves) {
    cles.insert(track.lift_key);
  }
  CHECK(cles.size() > 1);
}

TEST_CASE("le ruban est posé AU SOL, à plat", "[render][ground_ribbon]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      Advanced(EffectGroundRibbonPatternsFor(439).front(), 5);
  const std::optional<EffectWorldQuad> quad = ComposeGroundRibbonQuad(tracks.front(), kAtTheOrigin);
  REQUIRE(quad.has_value());
  CHECK(QuadArea(*quad) > 0.0F);
  // Les quatre coins à la même hauteur, juste AU-DESSUS du sol : le client
  // l'écrit négative dans son repère qui descend.
  for (const glm::vec3& corner : quad->corners) {
    CHECK(corner.y == Approx(-kGroundRibbonHeight));
    CHECK(corner.y > 0.0F);
  }
}

TEST_CASE("le ruban file VERS sa cible, hors de l'axe des x aussi", "[render][ground_ribbon]") {
  // Le cap se calcule dans le repère du client, dont la profondeur est
  // retournée : sans ramener les coins au nôtre, un ruban visant une cible en
  // profondeur négative partait vers la profondeur positive.
  EffectTrack track = Advanced(EffectGroundRibbonPatternsFor(439).front(), 5).front();
  track.tilt_degrees = ro::render::EffectGroundRibbonTargetDegrees(glm::vec3{0.0F, 0.0F, 0.0F},
                                                                   glm::vec3{0.0F, 0.0F, -100.0F});
  track.radius_at_anchor = 100.0F;
  track.value[0] = 0.0F;
  track.value[1] = 150.0F;
  track.value[2] = 50.0F;
  const std::optional<EffectWorldQuad> quad = ComposeGroundRibbonQuad(track, kAtTheOrigin);
  REQUIRE(quad.has_value());
  for (const glm::vec3& corner : quad->corners) {
    CHECK(corner.z < 0.0F);
  }
}

TEST_CASE("le seuil de cent décide du SENS de chaque extrémité", "[render][ground_ribbon]") {
  // ⚠ AU-DESSUS DE CENT l'extrémité s'écarte dans le sens du cap ; en dessous le
  // cap est retourné ET la soustraction inversée. Les traiter pareil replierait
  // le ruban sur lui-même pour la moitié des cas.
  EffectTrack track = Advanced(EffectGroundRibbonPatternsFor(439).front(), 5).front();
  track.tilt_degrees = 0;
  track.radius_at_anchor = 100.0F;
  track.value[0] = 0.0F;
  track.value[2] = 0.0F;

  track.value[1] = 150.0F;  // au-dessus du seuil
  const std::optional<EffectWorldQuad> forward = ComposeGroundRibbonQuad(track, kAtTheOrigin);
  track.value[1] = 50.0F;  // en dessous
  const std::optional<EffectWorldQuad> en_arriere = ComposeGroundRibbonQuad(track, kAtTheOrigin);
  REQUIRE(forward.has_value());
  REQUIRE(en_arriere.has_value());

  // Les deux portent la MÊME longueur — cinquante de part et d'autre de cent —
  // mais dans des directions OPPOSÉES.
  CHECK(forward->corners[0].x == Approx(-en_arriere->corners[0].x).margin(0.01F));
}

TEST_CASE("le style décide de la teinte et de la façon de peindre", "[render][ground_ribbon]") {
  SECTION("les styles sous trente se MÉLANGENT") {
    for (const std::uint8_t style : std::array<std::uint8_t, 6>{0, 1, 2, 10, 20, 22}) {
      INFO("style " << static_cast<int>(style));
      CHECK(EffectGroundRibbonStyleFor(style).blend_mode == 1);
      CHECK_FALSE(EffectGroundRibbonStyleFor(style).flattens_depth);
    }
  }
  SECTION("ceux de trente à trente-huit s'AJOUTENT et sont aplatis") {
    for (const std::uint8_t style : std::array<std::uint8_t, 4>{30, 32, 35, 38}) {
      INFO("style " << static_cast<int>(style));
      CHECK(EffectGroundRibbonStyleFor(style).blend_mode == 0);
      CHECK(EffectGroundRibbonStyleFor(style).flattens_depth);
    }
  }
  SECTION("un style inconnu prend le cas sans marque, qui se mélange") {
    CHECK(EffectGroundRibbonStyleFor(99).blend_mode == 1);
    CHECK(EffectGroundRibbonStyleFor(99).color == std::array<std::uint8_t, 3>{225, 225, 255});
  }
  SECTION("les dix-huit teintes sont distinctes") {
    std::set<std::array<std::uint8_t, 3>> seen;
    for (const std::uint8_t style : std::array<std::uint8_t, 18>{
             0, 1, 2, 10, 11, 12, 20, 21, 22, 30, 31, 32, 33, 34, 35, 36, 37, 38}) {
      seen.insert(EffectGroundRibbonStyleFor(style).color);
    }
    CHECK(seen.size() == 18);
  }
}

TEST_CASE("les deux familles de styles ont des rythmes INVERSES", "[render][ground_ribbon]") {
  // ⚠ SOUS DIX, l'alpha monte de douze et retombe de quatre ; au-delà il monte de
  // vingt et ne retombe que de deux. Celle qui s'allume lentement s'éteint vite.
  const auto alpha = [](std::uint8_t mark, std::int32_t ticks) {
    EffectTrack track;
    track.active = true;
    track.mark[kMarkOfRibbonStyle] = mark;
    std::array<EffectTrack, kEffectTracksPerNode> tracks;
    tracks.front() = track;
    for (std::int32_t tick = 0; tick < ticks; ++tick) {
      ro::render::AdvanceGroundRibbonTracks(tracks);
    }
    return tracks.front().alpha;
  };
  CHECK(alpha(0, 10) == 120);
  CHECK(alpha(10, 10) == 200);
  // Puis le palier, entre le dixième et le vingtième.
  CHECK(alpha(0, 20) == 120);
  // Puis la descente, et c'est la lente qui tombe le plus vite.
  CHECK(alpha(0, 30) == 120 - 40);
  CHECK(alpha(10, 30) == 200 - 20);
}

TEST_CASE("le ruban GLISSE, et pour la moitié des styles il accélère", "[render][ground_ribbon]") {
  const auto glissement = [](std::uint8_t mark, std::int32_t ticks) {
    std::array<EffectTrack, kEffectTracksPerNode> tracks;
    tracks.front().active = true;
    tracks.front().mark[kMarkOfRibbonStyle] = mark;
    tracks.front().value[3] = 1.0F;
    for (std::int32_t tick = 0; tick < ticks; ++tick) {
      ro::render::AdvanceGroundRibbonTracks(tracks);
    }
    return tracks.front();
  };
  // Une marque lente garde son pas ; une rapide le voit croître.
  CHECK(glissement(0, 10).value[3] == Approx(1.0F));
  CHECK(glissement(10, 10).value[3] > 1.0F);
  CHECK(glissement(10, 10).value[1] > glissement(0, 10).value[1]);
  // ⚠ ET LE GLISSEMENT SE REFERME À DEUX CENTS : c'est ce qui fait boucler le
  // glyphe au sol au lieu de l'envoyer à l'infini.
  CHECK(glissement(0, 400).value[1] < 200.0F);
}

TEST_CASE("l'image vient du site ou du bâtisseur, selon le bâtisseur", "[render][ground_ribbon]") {
  // ⚠ L'ADR 0019 DANS LES DEUX SENS. Deux bâtisseurs reçoivent leur image du
  // site, les deux autres la choisissent — supposer l'un pour toute la famille
  // se tromperait une fois sur deux.
  for (const EffectGroundRibbonPattern& pattern : EffectGroundRibbonPatternTable()) {
    INFO("effet " << pattern.effect_id);
    const std::string_view image = EffectGroundRibbonTextureFor(pattern);
    CHECK_FALSE(image.empty());
    if (pattern.builder == EffectGroundRibbonBuilder::kInitTextured) {
      CHECK_FALSE(pattern.texture.empty());
    } else {
      CHECK(pattern.texture.empty());
    }
  }
  // Et celui qui choisit tranche sur la dizaine.
  const EffectGroundRibbonPattern clair{0, "essai", "", EffectGroundRibbonBuilder::kBuildLineBeams,
                                        0};
  const EffectGroundRibbonPattern jaune{0, "essai", "", EffectGroundRibbonBuilder::kBuildLineBeams,
                                        10};
  CHECK(EffectGroundRibbonTextureFor(clair) != EffectGroundRibbonTextureFor(jaune));
}

TEST_CASE("un bâtisseur ajoute trente à sa marque, son jumeau rien", "[render][ground_ribbon]") {
  // ⚠ DEUX BÂTISSEURS QUI NE DIFFÈRENT QUE PAR CE NOMBRE donnent deux rendus
  // opposés : l'un s'ajoute et passe devant, l'autre se mélange.
  const EffectGroundRibbonPattern additif{0, "essai", "", EffectGroundRibbonBuilder::kInitLineBeam,
                                          0};
  const EffectGroundRibbonPattern blend{0, "essai", "",
                                        EffectGroundRibbonBuilder::kInitLineBeamYellow, 0};
  std::uint32_t a = 5U;
  std::uint32_t b = 5U;
  const auto additive_mark =
      SeedEffectGroundRibbonTracks(additif, 0, a).front().mark[kMarkOfRibbonStyle];
  const auto blended_mark =
      SeedEffectGroundRibbonTracks(blend, 0, b).front().mark[kMarkOfRibbonStyle];
  CHECK(additive_mark >= 30);
  CHECK(additive_mark <= 32);
  CHECK(blended_mark <= 2);
  CHECK(EffectGroundRibbonStyleFor(additive_mark).blend_mode == 0);
  CHECK(EffectGroundRibbonStyleFor(blended_mark).blend_mode == 1);
}

TEST_CASE("les douze effets sont en table, avec leurs deux sens", "[render][ground_ribbon]") {
  const std::span<const EffectGroundRibbonPattern> table = EffectGroundRibbonPatternTable();
  CHECK(std::ranges::is_sorted(table, {}, &EffectGroundRibbonPattern::effect_id));

  std::set<std::uint32_t> effects;
  for (const EffectGroundRibbonPattern& pattern : table) {
    effects.insert(pattern.effect_id);
  }
  CHECK(effects.size() == 12);
  // ⚠ LA PLUPART POSENT DEUX NŒUDS, un par sens : la table porte une ligne par
  // APPEL, pas par identifiant. N'en garder qu'une donnerait une moitié de figure.
  CHECK(table.size() > effects.size());

  for (const std::uint32_t effect_id : effects) {
    const std::span<const EffectGroundRibbonPattern> own_entries =
        EffectGroundRibbonPatternsFor(effect_id);
    INFO("effet " << effect_id);
    REQUIRE_FALSE(own_entries.empty());
    for (const EffectGroundRibbonPattern& pattern : own_entries) {
      const std::array<EffectTrack, kEffectTracksPerNode> tracks = Advanced(pattern, 6);
      for (const EffectTrack& track : tracks) {
        const std::optional<EffectWorldQuad> quad = ComposeGroundRibbonQuad(track, kAtTheOrigin);
        REQUIRE(quad.has_value());
        CHECK(QuadArea(*quad) > 0.0F);
      }
    }
  }
  CHECK(EffectGroundRibbonPatternsFor(1).empty());
}

TEST_CASE("le cap vient de la cible, pas d'un tirage", "[render][ground_ribbon]") {
  // ⚠ LE RUBAN POINTE VERS QUELQUE CHOSE. Le tirer au sort ferait pointer les
  // rubans n'importe où, et rien dans une capture fixe ne le dirait.
  const EffectGroundRibbonPattern& pattern = EffectGroundRibbonPatternsFor(439).front();
  std::uint32_t a = 1U;
  std::uint32_t b = 1U;
  const auto vers_zero = SeedEffectGroundRibbonTracks(pattern, 0, a);
  const auto toward_ninety = SeedEffectGroundRibbonTracks(pattern, 90, b);
  for (std::size_t rang = 0; rang < kEffectTracksPerNode; ++rang) {
    CHECK(vers_zero[rang].tilt_degrees == 0);
    CHECK(toward_ninety[rang].tilt_degrees == 90);
  }
}

TEST_CASE("le nœud semé rejoint la troupe", "[render][ground_ribbon]") {
  const EffectGroundRibbonPattern& pattern = EffectGroundRibbonPatternsFor(836).front();
  std::uint32_t seed = 2U;
  ro::render::PooledEffectNode node = SowEffectGroundRibbonNode(
      pattern, glm::vec3{7.0F, 0.0F, 3.0F}, 45, ro::render::kEffectNodeDefaultLifeTicks, seed);
  CHECK(node.shape == PooledNodeShape::kGroundRibbon);
  CHECK(node.resource_paths.size() == 1);

  EffectNodePool pool;
  pool.Add(node);
  for (int tick = 0; tick < 6; ++tick) {
    pool.Advance(16);
  }
  REQUIRE(pool.size() == 1);
  CHECK(pool.nodes().front().tracks[0].alpha > 0);
  CHECK(pool.nodes().front().tracks[0].tilt_degrees == 45);
}
