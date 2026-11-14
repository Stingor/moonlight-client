#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <string_view>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"
#include "ro/render/effect_turning_ring_seed.h"

using Catch::Approx;
using ro::render::AdvanceTurningRingTracks;
using ro::render::EffectTrack;
using ro::render::EffectTurningRingPattern;
using ro::render::EffectTurningRingPatternsFor;
using ro::render::EffectTurningRingPatternTable;
using ro::render::kEffectTracksPerNode;
using ro::render::PooledNodeShape;
using ro::render::SeedEffectTurningRingTracks;
using ro::render::SowEffectTurningRingNode;

namespace {

constexpr std::array<std::uint32_t, 4> kServis = {280, 285, 783, 784};
constexpr std::uint32_t kSeed = 20250716U;

[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> OneRing() {
  std::uint32_t generator = kSeed;
  return SeedEffectTurningRingTracks(generator);
}

}  // namespace

TEST_CASE("l'anneau qui tourne sert quatre identifiants", "[render][table]") {
  const std::span<const EffectTurningRingPattern> table = EffectTurningRingPatternTable();
  REQUIRE(table.size() == kServis.size());
  std::uint32_t previous = 0;
  for (const EffectTurningRingPattern& pattern : table) {
    REQUIRE(pattern.effect_id > previous);
    previous = pattern.effect_id;
    REQUIRE(pattern.texture.starts_with("data\\texture\\"));
    REQUIRE(pattern.client_function == std::string_view{"Effect_Build_SpinSpriteNode50"});
  }
  for (const std::uint32_t effect_id : kServis) {
    REQUIRE(EffectTurningRingPatternsFor(effect_id).size() == 1);
  }
  // Le 285 est le seul à changer de teinte : lui donner celle des autres
  // effacerait la différence que son site d'appel a voulue.
  CHECK(EffectTurningRingPatternsFor(285).front().trail_variant == 4);
  for (const std::uint32_t effect_id : {280U, 783U, 784U}) {
    CHECK(EffectTurningRingPatternsFor(effect_id).front().trail_variant == 8);
  }
}

TEST_CASE("l'anneau qui tourne n'ouvre qu'UNE piste, et son balayage n'est pas un tour",
          "[render][pool]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks = OneRing();
  CHECK(tracks[0].active);
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    CHECK_FALSE(tracks[rang].active);
  }
  // ⚠ TROIS CENT QUINZE, ET NON TROIS CENT SOIXANTE. L'anneau est OUVERT d'un
  // huitième, à la différence de celui du sous-type 36 ; le refermer lui ôterait
  // sa fente.
  CHECK(tracks[0].degrees == 315);
  CHECK(tracks[0].tilt_degrees == 90);
  CHECK(tracks[0].lift_key >= 25.0F);
  CHECK(tracks[0].lift_key < 35.0F);
  CHECK(tracks[0].alpha > 40);
  CHECK(tracks[0].alpha <= 50);
  // ⚠ LE RAYON N'EST PAS POSÉ : le mouvement l'écrit dès son premier tic.
  CHECK(tracks[0].radius_at_anchor == Approx(0.0F));
}

TEST_CASE("le rayon PULSE entre deux et quatre, et l'azimut TOURNE", "[render][pool]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneRing();
  const std::int16_t start_azimuth_degrees = tracks[0].azimuth_degrees;

  float smallest = 1000.0F;
  float plus_grand = -1000.0F;
  for (std::int32_t tick = 0; tick < 400; ++tick) {
    AdvanceTurningRingTracks(tracks);
    smallest = std::min(smallest, tracks[0].radius_at_anchor);
    plus_grand = std::max(plus_grand, tracks[0].radius_at_anchor);
  }
  // ⚠ LE RAYON EST UN SINUS DÉCALÉ DE TROIS : de deux à quatre, et il revient.
  // Le prendre pour une croissance — ce que fait le frère — le ferait grandir
  // sans fin.
  CHECK(smallest == Approx(2.0F).margin(0.01F));
  CHECK(plus_grand == Approx(4.0F).margin(0.01F));

  // L'azimut a tourné, et il est resté dans le tour.
  CHECK(tracks[0].azimuth_degrees != start_azimuth_degrees);
  CHECK(tracks[0].azimuth_degrees >= 0);
  CHECK(tracks[0].azimuth_degrees < 360);
}

TEST_CASE("il ne touche NI à l'alpha NI à l'inclinaison", "[render][pool]") {
  // ⚠ C'EST TOUT CE QUI LE SÉPARE DE SON FRÈRE, hors le rayon et l'azimut. Le
  // sous-type 36 fait respirer l'alpha et descendre l'inclinaison ; celui-ci
  // les laisse. Deux mouvements de la même figure, et pas un champ en commun
  // hors l'onde.
  std::array<EffectTrack, kEffectTracksPerNode> tracks = OneRing();
  const std::int16_t alpha = tracks[0].alpha;
  const std::int16_t tilt = tracks[0].tilt_degrees;
  for (std::int32_t tick = 0; tick < 200; ++tick) {
    AdvanceTurningRingTracks(tracks);
  }
  CHECK(tracks[0].alpha == alpha);
  CHECK(tracks[0].tilt_degrees == tilt);
}

TEST_CASE("le POULS se décale d'un anneau à l'autre", "[render][pool]") {
  // ⚠ LE COMPTEUR DU POULS EST LE CHAMP DE STYLE, et il se TIRE. Deux anneaux
  // semés ensemble ne pulsent donc pas d'accord ; le poser à zéro les ferait
  // battre d'un seul cœur.
  std::uint32_t un = 1U;
  std::uint32_t two = 2U;
  const std::array<EffectTrack, kEffectTracksPerNode> a = SeedEffectTurningRingTracks(un);
  const std::array<EffectTrack, kEffectTracksPerNode> b = SeedEffectTurningRingTracks(two);
  CHECK(a[0].style != b[0].style);
  for (std::uint32_t seed = 1; seed <= 64U; ++seed) {
    std::uint32_t generator = seed;
    const std::array<EffectTrack, kEffectTracksPerNode> tracks =
        SeedEffectTurningRingTracks(generator);
    REQUIRE(tracks[0].style >= 0);
    REQUIRE(tracks[0].style < 360);
  }
}

TEST_CASE("l'onde du cas 50 N'EST PAS celle de son frère : elle s'effile aux deux bouts",
          "[render][pool]") {
  // `clé · sin(9k) · (1 + 0,3 · sin(phase))`. Le sinus du long multiplie tout —
  // le ruban vaut ZÉRO aux deux bouts —, là où le cas 36 garde sa clé pour base
  // et n'ondule qu'autour d'elle. Les tenir pour identiques ferait de l'anneau
  // qui tourne une bande de largeur constante.
  std::array<EffectTrack, kEffectTracksPerNode> tournant = OneRing();
  AdvanceTurningRingTracks(tournant);
  const EffectTrack& track = tournant[0];
  REQUIRE(track.active);
  CHECK(track.value.front() == Approx(0.0F).margin(1e-4F));
  CHECK(track.value.back() == Approx(0.0F).margin(1e-3F));
  const float middle = track.value[track.value.size() / 2];
  CHECK(middle >= track.lift_key * 0.7F);
  CHECK(middle <= track.lift_key * 1.3F);
}

TEST_CASE("l'anneau qui tourne vit ce que vit son instance", "[render][pool]") {
  // Le bâtisseur recopie la vie de l'instance : vingt-neuf mille tics pour
  // l'effet 280. Le défaut de cent tics l'éteignait au bout d'une seconde et
  // demie.
  const EffectTurningRingPattern& pattern = EffectTurningRingPatternsFor(280).front();
  std::uint32_t generator = kSeed;
  const ro::render::PooledEffectNode node =
      SowEffectTurningRingNode(pattern, glm::vec3{0.0F, 0.0F, 0.0F}, generator);
  CHECK(node.node.life_ticks == ro::render::EffectNodeLifeFor(280));
  CHECK(node.node.life_ticks > 100);
}

TEST_CASE("un nœud d'anneau tournant prend sa forme", "[render][pool]") {
  const EffectTurningRingPattern& pattern = EffectTurningRingPatternsFor(280).front();
  std::uint32_t generator = kSeed;
  const ro::render::PooledEffectNode node =
      SowEffectTurningRingNode(pattern, glm::vec3{1.0F, 0.0F, 2.0F}, generator);
  CHECK(node.shape == PooledNodeShape::kTurningRing);
  CHECK(node.trail_variant == pattern.trail_variant);
  // ⚠ IL SUIT SON PORTEUR : la troupe recopie `owner_position` à chaque tic.
  CHECK(node.owner_position.z == Approx(2.0F));
  CHECK(node.resource_paths.size() == 1);
}
