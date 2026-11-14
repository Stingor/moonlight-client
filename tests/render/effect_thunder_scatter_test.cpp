#include "ro/render/effect_thunder_scatter.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <span>
#include <utility>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_frame.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_thunder_scatter_seed.h"
#include "ro/render/effect_tinted_quad.h"

#include "support/effect_bearers.h"

using Catch::Approx;
using ro::render::AdvanceThunderScatterTracks;
using ro::render::ComposeTintedRotatedQuad;
using ro::render::EffectNodeLifeFor;
using ro::render::EffectNodePool;
using ro::render::EffectThunderScatterPattern;
using ro::render::EffectThunderScatterPatternsFor;
using ro::render::EffectThunderScatterPatternTable;
using ro::render::EffectTintedQuadStyleFor;
using ro::render::EffectTrack;
using ro::render::kEffectTickMs;
using ro::render::kEffectTracksPerNode;
using ro::render::KnowsProceduralEffect;
using ro::render::kThunderScatterRenderWord;
using ro::render::kThunderScatterSlotOfRise;
using ro::render::kThunderScatterSlotOfSpeed;
using ro::render::kThunderScatterTexture;
using ro::render::kThunderScatterTint;
using ro::render::NextEffectNodeDraw;
using ro::render::PooledEffectNode;
using ro::render::PooledNodeShape;
using ro::render::SeedThunderScatterTracks;
using ro::render::SowThunderScatterNode;

namespace {

using Tracks = std::array<EffectTrack, kEffectTracksPerNode>;

// LES DEUX CAS QUI APPELLENT LE BÂTISSEUR : EF_TEIHIT2 et EF_BACKSTAP.
constexpr std::uint32_t kTeihit2EffectId = 264;
constexpr std::uint32_t kBackstabEffectId = 275;

// Ce que le bâtisseur écrit en dur, relu ici pour que l'essai ne se recopie pas.
constexpr std::int16_t kBlueTintRed = 130;
constexpr std::int16_t kBlueTintBlue = 250;
constexpr float kLift = 8.0F;
// Neuf pas de quinze, sans plafond.
constexpr std::int16_t kAlphaAtPeak = 135;
constexpr int kRisingTicks = 9;
// Quarante-cinq pas de trois ramènent le sommet à zéro.
constexpr int kFadingTicksToZero = 45;

// Une piste prête à prendre son cap au prochain tic, à vitesse et montée
// connues : le compteur à zéro est celui du client juste avant le tic utile.
[[nodiscard]] Tracks TrackReadyToLaunch(glm::vec3 anchor, float speed, float rise) {
  Tracks tracks;
  tracks[0].active = true;
  tracks[0].ticks = 0;
  tracks[0].anchor = anchor;
  tracks[0].value[kThunderScatterSlotOfSpeed] = speed;
  tracks[0].value[kThunderScatterSlotOfRise] = rise;
  tracks[0].mark[2] = kThunderScatterTint;
  tracks[0].radius_at_anchor = 0.5F;
  return tracks;
}

[[nodiscard]] float Radians(std::int32_t degrees) {
  return static_cast<float>(degrees) * std::numbers::pi_v<float> / 180.0F;
}

}  // namespace

TEST_CASE("Étincelles visées : deux sites, un bâtisseur, et la famille est connue",
          "[render][effectnode]") {
  const std::span<const EffectThunderScatterPattern> table = EffectThunderScatterPatternTable();
  REQUIRE(table.size() == 2);
  for (const std::uint32_t effect_id : {kTeihit2EffectId, kBackstabEffectId}) {
    const std::span<const EffectThunderScatterPattern> patterns =
        EffectThunderScatterPatternsFor(effect_id);
    REQUIRE(patterns.size() == 1);
    CHECK(patterns[0].client_function == "Effect_Build_ThunderCenterScatter2d");
    // ⚠ SANS CETTE LIGNE, la compétence qui pose l'effet passerait pour muette.
    CHECK(KnowsProceduralEffect(effect_id));
  }
  CHECK(EffectThunderScatterPatternsFor(kTeihit2EffectId + 1).empty());
}

TEST_CASE("Étincelles visées : quatre pistes ancrées huit unités au-dessus du point visé",
          "[render][effectnode]") {
  const glm::vec3 aimed{120.0F, -4.0F, 37.0F};
  std::uint32_t seed = 1234U;
  std::uint32_t witness = seed;
  const Tracks tracks = SeedThunderScatterTracks(aimed, seed);
  for (const EffectTrack& track : tracks) {
    // L'ORDRE DES TIRAGES est celui du client : compteur, rayon, vitesse, montée.
    const auto delay = static_cast<std::int32_t>(NextEffectNodeDraw(witness) % 10U);
    const auto radius = static_cast<float>(NextEffectNodeDraw(witness) % 16U);
    const auto speed = static_cast<float>(NextEffectNodeDraw(witness) % 50U);
    const auto rise = static_cast<float>(NextEffectNodeDraw(witness) % 200U);
    CHECK(track.active);
    CHECK(track.ticks == delay - 40);
    CHECK(track.ticks >= -40);
    CHECK(track.ticks <= -31);
    CHECK(track.radius_at_anchor == Approx(radius * 0.05F + 0.1F));
    CHECK(track.value[kThunderScatterSlotOfSpeed] == Approx(speed * 0.01F + 0.75F));
    CHECK(track.value[kThunderScatterSlotOfRise] == Approx(rise * 0.01F - 1.0F));
    CHECK(track.alpha == 0);
    CHECK(track.degrees == 0);
    CHECK(track.azimuth_degrees == 0);
    // ⚠ LE CLIENT RETRANCHE HUIT dans un repère qui descend : l'ancre MONTE.
    CHECK(track.anchor.x == Approx(aimed.x));
    CHECK(track.anchor.y == Approx(aimed.y + kLift));
    CHECK(track.anchor.z == Approx(aimed.z));
    CHECK(track.mark[0] == 0);
    CHECK(track.mark[2] == kThunderScatterTint);
  }
  CHECK(seed == witness);
}

TEST_CASE("Étincelles visées : sans point visé, rien ; sinon un nœud né sur le porteur",
          "[render][effectnode]") {
  std::uint32_t seed = 7U;
  const glm::vec3 bearer{10.0F, 0.0F, 20.0F};
  CHECK_FALSE(SowThunderScatterNode(bearer, std::nullopt, 300, seed).has_value());

  const glm::vec3 aimed{40.0F, 0.0F, 20.0F};
  const std::optional<PooledEffectNode> node =
      SowThunderScatterNode(bearer, aimed, EffectNodeLifeFor(kBackstabEffectId), seed);
  REQUIRE(node.has_value());
  CHECK(node->shape == PooledNodeShape::kThunderScatter);
  CHECK(node->ground_position == bearer);
  CHECK(node->owner_position == bearer);
  REQUIRE(node->target_position.has_value());
  CHECK(*node->target_position == aimed);
  CHECK(node->node.render_word == kThunderScatterRenderWord);
  CHECK(node->node.life_ticks == 300);
  REQUIRE(node->resource_paths.size() == 1);
  CHECK(node->resource_paths[0] == kThunderScatterTexture);
}

TEST_CASE("Étincelles visées : une piste retardée ne bouge pas", "[render][effectnode]") {
  Tracks tracks = TrackReadyToLaunch({0.0F, 0.0F, 0.0F}, 1.0F, 0.5F);
  tracks[0].ticks = -2;
  std::uint32_t seed = 3U;
  const std::uint32_t before = seed;
  AdvanceThunderScatterTracks(tracks, {0.0F, 0.0F, 0.0F}, {50.0F, 0.0F, 0.0F}, 0, 0, seed);
  CHECK(tracks[0].ticks == -1);
  CHECK(tracks[0].degrees == 0);
  CHECK(tracks[0].anchor == glm::vec3{0.0F, 0.0F, 0.0F});
  CHECK(seed == before);
}

TEST_CASE(
    "Étincelles visées : au premier tic utile, un cap vers le point visé, à trente degrés près",
    "[render][effectnode]") {
  // 🔴 LE SENS DE LA PROFONDEUR : un point visé vers +z chez nous doit faire
  // filer l'étincelle vers +z. Un cap recopié sans le nier la ferait partir à
  // l'opposé.
  for (const float cote : {1.0F, -1.0F}) {
    const glm::vec3 node{0.0F, 0.0F, 0.0F};
    const glm::vec3 aimed{0.0F, 0.0F, 100.0F * cote};
    Tracks tracks = TrackReadyToLaunch(aimed, 1.0F, 0.0F);
    std::uint32_t seed = 99U;
    std::uint32_t witness = seed;
    AdvanceThunderScatterTracks(tracks, node, aimed, 0, 0, seed);
    const auto offset = static_cast<std::int32_t>(NextEffectNodeDraw(witness) % 60U) - 30;
    CHECK(seed == witness);
    CHECK(tracks[0].ticks == 1);
    // La rotation précède la branche du cap : 0 − 5 replié.
    CHECK(tracks[0].degrees == 355);
    // L'alpha ne bouge pas au tic du cap.
    CHECK(tracks[0].alpha == 0);
    // Le cap du client vaut 270 vers +z chez nous (90 vers −z), écart compris ;
    // le nôtre en est l'opposé.
    const std::int32_t cap_client = ((cote > 0.0F ? 270 : 90) + offset + 360) % 360;
    CHECK(tracks[0].azimuth_degrees == (360 - cap_client) % 360);
    CHECK((tracks[0].anchor.z - aimed.z) * cote > 0.8F);
    CHECK(tracks[0].anchor.x == Approx(std::cos(Radians(tracks[0].azimuth_degrees))));
  }
}

TEST_CASE(
    "Étincelles visées : quinze par tic pendant neuf tics, puis moins trois jusqu'à s'éteindre",
    "[render][effectnode]") {
  Tracks tracks = TrackReadyToLaunch({0.0F, 0.0F, 0.0F}, 0.0F, 0.0F);
  std::uint32_t seed = 5U;
  const glm::vec3 node{0.0F, 0.0F, 0.0F};
  const glm::vec3 aimed{10.0F, 0.0F, 0.0F};
  AdvanceThunderScatterTracks(tracks, node, aimed, 0, 0, seed);  // le cap
  for (int tick = 0; tick < kRisingTicks; ++tick) {
    AdvanceThunderScatterTracks(tracks, node, aimed, 0, 0, seed);
  }
  // ⚠ AUCUN PLAFOND : neuf pas de quinze.
  CHECK(tracks[0].alpha == kAlphaAtPeak);
  // Quarante-cinq pas de trois ramènent à zéro sans éteindre…
  for (int tick = 0; tick < kFadingTicksToZero; ++tick) {
    AdvanceThunderScatterTracks(tracks, node, aimed, 0, 0, seed);
  }
  CHECK(tracks[0].alpha == 0);
  CHECK(tracks[0].active);
  // … et le suivant passe sous zéro : la piste s'éteint.
  AdvanceThunderScatterTracks(tracks, node, aimed, 0, 0, seed);
  CHECK(tracks[0].alpha == 0);
  CHECK_FALSE(tracks[0].active);
}

TEST_CASE("Étincelles visées : la montée se retranche, et les angles de caméra se rangent",
          "[render][effectnode]") {
  Tracks tracks = TrackReadyToLaunch({0.0F, 10.0F, 0.0F}, 0.0F, 0.5F);
  std::uint32_t seed = 11U;
  constexpr std::int16_t kTour = 123;
  constexpr std::int16_t kTilt = 45;
  AdvanceThunderScatterTracks(tracks, {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, kTour, kTilt, seed);
  // ⚠ LE CLIENT AJOUTE +0,5 À UNE ORDONNÉE QUI DESCEND : chez nous, elle baisse.
  CHECK(tracks[0].anchor.y == Approx(9.5F));
  CHECK(tracks[0].style == kTour);
  CHECK(tracks[1].style == kTilt);
}

TEST_CASE("Étincelles visées : le nœud suit son porteur, les ancres restent dans le monde",
          "[render][effectnode]") {
  EffectNodePool pool;
  const glm::vec3 bearer{0.0F, 0.0F, 0.0F};
  const glm::vec3 aimed{50.0F, 0.0F, 0.0F};
  std::uint32_t seed = 21U;
  std::optional<PooledEffectNode> node =
      SowThunderScatterNode(bearer, aimed, EffectNodeLifeFor(kTeihit2EffectId), seed);
  REQUIRE(node.has_value());
  pool.AnchorNextTo(17U, bearer);
  pool.Add(std::move(*node));
  pool.AnchorNextTo(0U);

  const glm::vec3 farther{-20.0F, 0.0F, 5.0F};
  ro::test_support::MoveBearer(pool, 17U, farther);
  pool.Advance(kEffectTickMs);
  REQUIRE(pool.size() == 1);
  const PooledEffectNode& alive = pool.nodes()[0];
  CHECK(alive.ground_position.x == Approx(farther.x).margin(0.001F));
  CHECK(alive.ground_position.z == Approx(farther.z).margin(0.001F));
  for (const EffectTrack& track : alive.tracks) {
    CHECK(track.anchor.x == Approx(aimed.x));
    CHECK(track.anchor.y == Approx(aimed.y + kLift));
  }
}

TEST_CASE("Étincelles visées : la marque deux peint un carré bleu, ajouté",
          "[render][effectnode]") {
  CHECK(EffectTintedQuadStyleFor(kThunderScatterTint, 0).draws);
  CHECK(EffectTintedQuadStyleFor(kThunderScatterTint, 0).color[0] == kBlueTintRed);
  CHECK(EffectTintedQuadStyleFor(kThunderScatterTint, 0).color[2] == kBlueTintBlue);
  Tracks tracks = TrackReadyToLaunch({0.0F, 0.0F, 0.0F}, 1.0F, 0.0F);
  std::uint32_t seed = 2U;
  AdvanceThunderScatterTracks(tracks, {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, 0, 0, seed);
  // Au tic du cap l'alpha est encore nul : rien ne se voit.
  CHECK_FALSE(ComposeTintedRotatedQuad(tracks[0], kThunderScatterTint, 0, 0, 0).has_value());
  AdvanceThunderScatterTracks(tracks, {0.0F, 0.0F, 0.0F}, {1.0F, 0.0F, 0.0F}, 0, 0, seed);
  CHECK(ComposeTintedRotatedQuad(tracks[0], kThunderScatterTint, 0, 0, 0).has_value());
}
