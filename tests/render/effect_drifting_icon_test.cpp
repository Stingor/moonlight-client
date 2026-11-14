#include "ro/render/effect_drifting_icon.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>
#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_drifting_icon_seed.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_tinted_quad.h"
#include "ro/render/effect_track.h"

using Catch::Approx;
using ro::render::AdvanceDriftingIconTracks;
using ro::render::ComposeTintedRotatedQuad;
using ro::render::EffectDriftingIconPatternsFor;
using ro::render::EffectDriftingIconTextureCount;
using ro::render::EffectScatterWorld;
using ro::render::EffectTintedQuadStyleFor;
using ro::render::EffectTrack;
using ro::render::kDriftingIconDriftFactor;
using ro::render::kDriftingIconFadeTicks;
using ro::render::kDriftingIconNodesPerPlay;
using ro::render::kDriftingIconRenderWord;
using ro::render::kDriftingIconSlotOfSpinDegrees;
using ro::render::kDriftingIconTint;
using ro::render::kEffectTracksPerNode;
using ro::render::SeedEffectDriftingIconTracks;
using ro::render::SowEffectDriftingIconNode;

namespace {

using Tracks = std::array<EffectTrack, kEffectTracksPerNode>;

constexpr std::uint32_t kWhiteHerbEffectId = 573;
constexpr std::uint32_t kWhiteSlimEffectId = 574;
constexpr std::uint32_t kBottlesEffectId = 575;

// La vie que la table donne à ces trois effets.
constexpr std::int32_t kLifeTicks = 300;

constexpr std::int16_t kTurn = 200;
constexpr std::int16_t kTilt = 30;

[[nodiscard]] float Radians(std::int32_t degrees) {
  return static_cast<float>(degrees) * std::numbers::pi_v<float> / 180.0F;
}

[[nodiscard]] EffectScatterWorld SolA(float altitude) {
  EffectScatterWorld world;
  world.ground_height = [altitude](float, float) { return altitude; };
  return world;
}

// Une piste allumée, sans course : ses deux cibles égalent leurs valeurs.
[[nodiscard]] EffectTrack CalmTrack() {
  EffectTrack track;
  track.active = true;
  track.azimuth_degrees = static_cast<std::int16_t>(kLifeTicks - kDriftingIconFadeTicks);
  return track;
}

}  // namespace

TEST_CASE("trois sites, un par octet, et l'octet ne choisit que l'image",
          "[render][drifting_icon]") {
  const auto herbe = EffectDriftingIconPatternsFor(kWhiteHerbEffectId);
  const auto potion = EffectDriftingIconPatternsFor(kWhiteSlimEffectId);
  const auto fioles = EffectDriftingIconPatternsFor(kBottlesEffectId);
  REQUIRE(herbe.size() == 1);
  REQUIRE(potion.size() == 1);
  REQUIRE(fioles.size() == 1);
  CHECK(herbe[0].octet == 0);
  CHECK(potion[0].octet == 1);
  CHECK(fioles[0].octet == 2);
  CHECK(EffectDriftingIconTextureCount(herbe[0]) == 1);
  CHECK(EffectDriftingIconTextureCount(potion[0]) == 1);
  CHECK(EffectDriftingIconTextureCount(fioles[0]) == 3);
  CHECK(herbe[0].client_function == "EffectInst_InitScatterParticlesCamAnchored");
  CHECK(kDriftingIconNodesPerPlay == 80);
}

TEST_CASE(
    "les icônes se posent dans un carré de cent soixante, cinq à soixante-dix au-dessus du sol",
    "[render][drifting_icon]") {
  const glm::vec3 player = {500.0F, 12.0F, -300.0F};
  constexpr float kSol = -7.0F;
  std::uint32_t generator = 11;
  for (int attempt = 0; attempt < 50; ++attempt) {
    const Tracks tracks = SeedEffectDriftingIconTracks(player, kLifeTicks, SolA(kSol), generator);
    for (const EffectTrack& track : tracks) {
      CHECK(track.active);
      CHECK(track.alpha == 0);
      CHECK(track.anchor.x >= player.x - 80.0F);
      CHECK(track.anchor.x < player.x + 80.0F);
      // La profondeur se retranche dans notre repère : de −79 à +80.
      CHECK(track.anchor.z > player.z - 80.0F);
      CHECK(track.anchor.z <= player.z + 80.0F);
      // ⚠ LE CLIENT RETRANCHE À UNE ORDONNÉE QUI DESCEND : AU-DESSUS du sol, et
      // du SOL, pas du joueur.
      CHECK(track.anchor.y >= kSol + 5.0F);
      CHECK(track.anchor.y <= kSol + 70.0F);
      CHECK(track.radius_at_anchor >= 3.0F - 0.001F);
      CHECK(track.radius_at_anchor <= 4.0F + 0.001F);
      CHECK(track.azimuth_degrees == kLifeTicks - kDriftingIconFadeTicks);
      CHECK(track.mark[2] == kDriftingIconTint);
      CHECK(track.degrees >= 0);
      CHECK(track.degrees < 360);
      for (std::size_t case_courante = 0; case_courante < 4; ++case_courante) {
        CHECK(track.value[case_courante] >= 0.0F);
        CHECK(track.value[case_courante] < 360.0F);
      }
    }
  }
}

TEST_CASE("sans sol à lire, l'icône flotte au-dessus du joueur", "[render][drifting_icon]") {
  const glm::vec3 player = {0.0F, 40.0F, 0.0F};
  std::uint32_t generator = 2;
  const Tracks tracks = SeedEffectDriftingIconTracks(player, kLifeTicks, {}, generator);
  for (const EffectTrack& track : tracks) {
    CHECK(track.anchor.y >= player.y + 5.0F);
    CHECK(track.anchor.y <= player.y + 70.0F);
  }
}

TEST_CASE("la marque sept peint en blanc, ajouté", "[render][drifting_icon]") {
  const auto style = EffectTintedQuadStyleFor(kDriftingIconTint, 0);
  CHECK(style.draws);
  CHECK_FALSE(style.ordinary_blend);
}

TEST_CASE("l'alpha monte de cinq jusqu'au tic quarante-neuf, et baisse passé le seuil",
          "[render][drifting_icon]") {
  Tracks tracks;
  tracks[0] = CalmTrack();
  std::uint32_t generator = 1;
  for (int tick = 0; tick < 49; ++tick) {
    AdvanceDriftingIconTracks(tracks, kTurn, kTilt, generator);
  }
  CHECK(tracks[0].alpha == 245);
  AdvanceDriftingIconTracks(tracks, kTurn, kTilt, generator);
  CHECK(tracks[0].alpha == 245);
  // Jusqu'au seuil, il tient ; au-delà, il perd cinq par tic.
  for (int tick = 50; tick < kLifeTicks - kDriftingIconFadeTicks; ++tick) {
    AdvanceDriftingIconTracks(tracks, kTurn, kTilt, generator);
  }
  CHECK(tracks[0].alpha == 245);
  AdvanceDriftingIconTracks(tracks, kTurn, kTilt, generator);
  CHECK(tracks[0].alpha == 240);
}

TEST_CASE("les deux pas se compensent quand le seuil tombe avant la fin de la montée",
          "[render][drifting_icon]") {
  Tracks tracks;
  tracks[0] = CalmTrack();
  tracks[0].azimuth_degrees = 2;
  std::uint32_t generator = 1;
  for (int tick = 0; tick < 10; ++tick) {
    AdvanceDriftingIconTracks(tracks, kTurn, kTilt, generator);
  }
  // Deux tics de montée seule, puis huit où monter et baisser s'annulent.
  CHECK(tracks[0].alpha == 10);
}

TEST_CASE("chaque valeur court vers sa cible, et en tire une neuve à l'arrivée",
          "[render][drifting_icon]") {
  Tracks tracks;
  tracks[0] = CalmTrack();
  // Le premier couple monte de 10 vers 12 ; le second descend de 20 vers 18.
  tracks[0].value[0] = 10.0F;
  tracks[0].value[1] = 12.0F;
  tracks[0].value[2] = 20.0F;
  tracks[0].value[3] = 18.0F;
  std::uint32_t generator = 77;
  AdvanceDriftingIconTracks(tracks, kTurn, kTilt, generator);
  CHECK(tracks[0].value[0] == Approx(11.0F));
  CHECK(tracks[0].value[2] == Approx(19.0F));
  // ⚠ ARRIVÉE PILE EN MONTANT : aucun tirage. En descendant : un tirage, et la
  // nouvelle cible est au-dessus de `359 − valeur`.
  const std::uint32_t before = generator;
  AdvanceDriftingIconTracks(tracks, kTurn, kTilt, generator);
  CHECK(tracks[0].value[0] == Approx(12.0F));
  CHECK(tracks[0].value[1] == Approx(12.0F));
  CHECK(tracks[0].value[2] == Approx(18.0F));
  CHECK(tracks[0].value[3] > 359.0F - 18.0F);
  CHECK(tracks[0].value[3] <= 359.0F);
  CHECK(generator != before);
  // Le tic suivant, le premier couple redescend d'un degré, et tire à son tour.
  AdvanceDriftingIconTracks(tracks, kTurn, kTilt, generator);
  CHECK(tracks[0].value[0] == Approx(11.0F));
  CHECK(tracks[0].value[1] > 359.0F - 11.0F);
}

TEST_CASE("une valeur qui dépasse sa cible en montant en tire une plus basse",
          "[render][drifting_icon]") {
  Tracks tracks;
  tracks[0] = CalmTrack();
  tracks[0].value[0] = 5.5F;
  tracks[0].value[1] = 6.0F;
  std::uint32_t generator = 3;
  AdvanceDriftingIconTracks(tracks, kTurn, kTilt, generator);
  CHECK(tracks[0].value[0] == Approx(6.5F));
  CHECK(tracks[0].value[1] < 6.0F);
}

TEST_CASE("une valeur nulle ne tire rien", "[render][drifting_icon]") {
  Tracks tracks;
  tracks[0] = CalmTrack();
  tracks[0].value[0] = 1.0F;
  tracks[0].value[1] = 0.0F;
  tracks[0].value[2] = 0.0F;
  tracks[0].value[3] = 0.0F;
  std::uint32_t generator = 3;
  AdvanceDriftingIconTracks(tracks, kTurn, kTilt, generator);
  CHECK(tracks[0].value[0] == Approx(0.0F));
  CHECK(tracks[0].value[1] == Approx(359.0F));
  CHECK(tracks[0].value[3] == Approx(359.0F));
  CHECK(generator == 3);
}

TEST_CASE("l'ancre dérive d'un dixième du SINUS des deux valeurs courantes",
          "[render][drifting_icon]") {
  Tracks tracks;
  tracks[0] = CalmTrack();
  tracks[0].value[0] = 30.0F;
  tracks[0].value[1] = 200.0F;
  tracks[0].value[2] = 100.0F;
  tracks[0].value[3] = 200.0F;
  tracks[0].anchor = {1.0F, 2.0F, 3.0F};
  std::uint32_t generator = 3;
  AdvanceDriftingIconTracks(tracks, kTurn, kTilt, generator);
  // Les valeurs ont d'abord avancé d'un degré.
  CHECK(tracks[0].anchor.x == Approx(1.0F + std::sin(Radians(31)) * kDriftingIconDriftFactor));
  // La profondeur se retranche : notre repère l'oppose à celui du client.
  CHECK(tracks[0].anchor.z == Approx(3.0F - std::sin(Radians(101)) * kDriftingIconDriftFactor));
  CHECK(tracks[0].anchor.y == Approx(2.0F));
}

TEST_CASE("le carré et l'angle propre tournent d'un degré par tic, repliés au tour",
          "[render][drifting_icon]") {
  Tracks tracks;
  tracks[0] = CalmTrack();
  tracks[0].degrees = 359;
  tracks[0].value[kDriftingIconSlotOfSpinDegrees] = 359.5F;
  std::uint32_t generator = 3;
  AdvanceDriftingIconTracks(tracks, kTurn, kTilt, generator);
  CHECK(tracks[0].degrees == 0);
  CHECK(tracks[0].value[kDriftingIconSlotOfSpinDegrees] == Approx(0.5F));
  CHECK(tracks[0].style == kTurn);
  CHECK(tracks[1].style == kTilt);
}

TEST_CASE("les quatre pistes vivent chacune pour soi", "[render][drifting_icon]") {
  Tracks tracks;
  for (EffectTrack& track : tracks) {
    track = CalmTrack();
  }
  tracks[2].active = false;
  std::uint32_t generator = 3;
  AdvanceDriftingIconTracks(tracks, kTurn, kTilt, generator);
  CHECK(tracks[0].alpha == 5);
  CHECK(tracks[1].alpha == 5);
  CHECK(tracks[2].alpha == 0);
  CHECK(tracks[3].alpha == 5);
}

TEST_CASE("le nœud des fioles tire son image, et se dessine une fois allumé",
          "[render][drifting_icon]") {
  const auto fioles = EffectDriftingIconPatternsFor(kBottlesEffectId)[0];
  std::array<bool, 3> seen = {false, false, false};
  std::uint32_t generator = 21;
  for (int attempt = 0; attempt < 60; ++attempt) {
    const auto node =
        SowEffectDriftingIconNode(fioles, {0.0F, 0.0F, 0.0F}, kLifeTicks, SolA(0.0F), generator);
    CHECK(node.shape == ro::render::PooledNodeShape::kDriftingIcons);
    CHECK(node.node.life_ticks == kLifeTicks);
    REQUIRE(node.node.render_word.has_value());
    CHECK(*node.node.render_word == kDriftingIconRenderWord);
    REQUIRE(node.resource_paths.size() == 1);
    for (std::size_t image = 0; image < seen.size(); ++image) {
      if (node.resource_paths[0] == std::string(fioles.textures[image])) {
        seen[image] = true;
      }
    }
  }
  CHECK(seen[0]);
  CHECK(seen[1]);
  CHECK(seen[2]);

  auto node =
      SowEffectDriftingIconNode(fioles, {0.0F, 0.0F, 0.0F}, kLifeTicks, SolA(0.0F), generator);
  // Invisible au semis, visible au premier tic.
  CHECK_FALSE(
      ComposeTintedRotatedQuad(node.tracks[0], kDriftingIconTint, 0, kTurn, kTilt).has_value());
  AdvanceDriftingIconTracks(node.tracks, kTurn, kTilt, node.node.wander_seed);
  CHECK(ComposeTintedRotatedQuad(node.tracks[0], node.tracks[0].mark[2], 0, kTurn, kTilt)
            .has_value());
}
