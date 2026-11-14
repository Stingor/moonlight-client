// LES EMPREINTES : le sous-type 67, son semis, son mouvement et sa figure.

#include <array>
#include <cstdint>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_breathing_quad.h"
#include "ro/render/effect_footprint_seed.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

using Catch::Approx;
using ro::render::ActorEffectPlay;
using ro::render::EffectFootprintPattern;
using ro::render::EffectFootprintPatternsFor;
using ro::render::EffectTrack;
using ro::render::kEffectTracksPerNode;

namespace {

constexpr std::int16_t kBirthAlpha = 150;

const EffectFootprintPattern& OnlyPatternOf(std::uint32_t effect_id) {
  const auto patterns = EffectFootprintPatternsFor(effect_id);
  REQUIRE(patterns.size() == 1);
  return patterns.front();
}

}  // namespace

TEST_CASE("Empreintes : la course et Chase Walk ont chacune leurs deux pieds",
          "[render][footprint]") {
  // Le pas de course : deux couches qui s'ajoutent, un rayon de six.
  for (const std::uint32_t effect_id : ro::render::kFootprintEffects) {
    const EffectFootprintPattern& pattern = OnlyPatternOf(effect_id);
    CHECK(pattern.node_style == 100);
    CHECK(pattern.radius_at_anchor == Approx(6.0F));
    CHECK(pattern.alpha == kBirthAlpha);
    CHECK(ro::render::KnowsProceduralEffect(effect_id));
  }
  // Celui de Chase Walk : une couche mêlée, un rayon de cinq.
  for (const std::uint32_t effect_id : ro::render::kChaseWalkFootprintEffects) {
    const EffectFootprintPattern& pattern = OnlyPatternOf(effect_id);
    CHECK(pattern.node_style == 0);
    CHECK(pattern.radius_at_anchor == Approx(5.0F));
  }
  CHECK(OnlyPatternOf(447).texture != OnlyPatternOf(448).texture);
}

TEST_CASE("Empreintes : une seule piste, tournée dans le sens de la marche",
          "[render][footprint]") {
  const std::array<EffectTrack, kEffectTracksPerNode> tracks =
      ro::render::SeedEffectFootprintTracks(OnlyPatternOf(447), 225);
  CHECK(tracks[0].active);
  for (std::size_t rank = 1; rank < tracks.size(); ++rank) {
    CHECK_FALSE(tracks[rank].active);
  }
  CHECK(tracks[0].azimuth_degrees == 225);
  CHECK(tracks[0].alpha == kBirthAlpha);
  CHECK(tracks[0].value[ro::render::kSlotOfBreathingHeight] ==
        Approx(ro::render::kFootprintClientHeight));
  // La figure du carré : deux couches pour le style cent, à neuf dixièmes du
  // rayon puisque rien ne fait respirer l'empreinte.
  const auto quads = ro::render::ComposeBreathingQuad(tracks[0], 100, 0, glm::vec3(0.0F));
  CHECK(quads.size() == 2);
}

TEST_CASE("Empreintes : elles s'effacent d'un cran tous les deux tics", "[render][footprint]") {
  std::array<EffectTrack, kEffectTracksPerNode> tracks =
      ro::render::SeedEffectFootprintTracks(OnlyPatternOf(447), 0);
  ro::render::AdvanceFootprintTracks(tracks);
  CHECK(tracks[0].alpha == kBirthAlpha - 1);
  ro::render::AdvanceFootprintTracks(tracks);
  CHECK(tracks[0].alpha == kBirthAlpha - 1);
  for (int tick = 0; tick < 2 * kBirthAlpha; ++tick) {
    ro::render::AdvanceFootprintTracks(tracks);
  }
  CHECK(tracks[0].alpha == 0);
}

TEST_CASE("Empreintes : le semeur tourne dès la course, et pose après sept tics",
          "[render][footprint]") {
  using ro::render::TrailAfterEffects;
  const std::array<ActorEffectPlay, 1> begin = {ActorEffectPlay{442, 3}};  // EF_RUN
  REQUIRE(TrailAfterEffects(begin).has_value());
  CHECK_FALSE(TrailAfterEffects(begin)->clock_past_period);
  const std::array<ActorEffectPlay, 1> launched = {ActorEffectPlay{442, 8}};
  CHECK(TrailAfterEffects(launched)->clock_past_period);
  // L'horloge vit dans un octet : elle repasse par zéro, et la course se tait.
  const std::array<ActorEffectPlay, 1> wrapped = {ActorEffectPlay{442, 256 + 3}};
  CHECK_FALSE(TrailAfterEffects(wrapped)->clock_past_period);
  const std::array<ActorEffectPlay, 1> cloaked = {ActorEffectPlay{120, 50}};  // EF_CLOAKING
  CHECK_FALSE(TrailAfterEffects(cloaked).has_value());
}

TEST_CASE("Empreintes : l'angle suit la marche, tourné d'un quart de tour et demi",
          "[render][footprint]") {
  using ro::render::FootprintAzimuthDegrees;
  // Vers les x croissants : zéro degré de marche.
  CHECK(FootprintAzimuthDegrees(1.0F, 0.0F) == 135);
  // Vers les profondeurs croissantes du client : quatre-vingt-dix degrés.
  CHECK(FootprintAzimuthDegrees(0.0F, 1.0F) == 225);
  // Vers les profondeurs décroissantes : le cosinus est le même, le sinus
  // retourne l'angle.
  CHECK(FootprintAzimuthDegrees(0.0F, -1.0F) == 45);
  // Vers les x décroissants : le demi-tour.
  CHECK(FootprintAzimuthDegrees(-1.0F, 0.0F) == 315);
  // Le pas ne compte que par sa direction.
  CHECK(FootprintAzimuthDegrees(3.0F, 0.0F) == FootprintAzimuthDegrees(1.0F, 0.0F));
  // Sans marche, pas d'empreinte.
  CHECK_FALSE(FootprintAzimuthDegrees(0.0F, 0.0F).has_value());
}
