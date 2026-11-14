// UN DRAPEAU VIT EXACTEMENT TANT QU'UN EFFET VIVANT LE RELÈVE.
//
// Le client remet le mot de rendu de l'acteur à zéro au premier pas qui suit
// chacun de ses dessins, et ce sont les instances d'effet vivantes qui le
// relèvent. Ces essais tiennent la règle de bout en bout : du monde, qui garde
// les effets d'un acteur, jusqu'à ce que la composition en montre.

#include <array>
#include <cstdint>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_effect_table.h"
#include "ro/render/actor_scale.h"
#include "ro/render/effect_frame.h"
#include "ro/sim/actor.h"
#include "ro/sim/world.h"

namespace {

constexpr std::uint32_t kBearerId = 150'303;
constexpr std::uint32_t kBornMs = 1'000;
constexpr auto kTickMs = static_cast<std::uint32_t>(ro::render::kEffectTickMs);

// Deux effets qui font trembler le corps : le premier vit quatorze tics, le
// second trente-cinq. Le même bit, deux instances.
constexpr std::uint32_t kShortJitterEffect = 426;
constexpr std::uint32_t kShortJitterLifeTicks = 14;
constexpr std::uint32_t kLongJitterEffect = 458;
constexpr std::uint32_t kLongJitterLifeTicks = 35;
// Le corps envoyé hors de l'écran, sans fin : seul un retrait l'éteint.
constexpr std::uint32_t kOffScreenEffect = 823;
// La réduction du corps : son bit est levé avant qu'elle ne compare son âge.
constexpr std::uint32_t kShrinkEffect = 420;
constexpr std::uint32_t kShrinkLifeTicks = 46;
// Le corps teint en magenta un tic sur deux, quatre-vingt-un tics durant.
constexpr std::uint32_t kMagentaBlinkEffect = 493;
constexpr std::uint32_t kMagentaLastTick = 80;

constexpr std::array<std::uint8_t, 4> kWhite = {255, 255, 255, 255};
constexpr std::array<std::uint8_t, 4> kMagenta = {255, 0, 255, 255};

std::uint32_t AtTick(std::uint32_t tick) { return kBornMs + tick * kTickMs; }

struct Bench {
  ro::sim::World world;

  Bench() {
    ro::sim::Actor bearer;
    bearer.id = kBearerId;
    bearer.kind = ro::sim::ActorKind::kPlayer;
    world.Spawn(bearer);
  }

  void Play(std::uint32_t effect_id, std::uint32_t since_ms = kBornMs) {
    REQUIRE(world.RecordEffect(kBearerId, effect_id, since_ms));
  }

  // Ce que la composition de la scène lit : chaque effet du porteur et son âge.
  [[nodiscard]] std::vector<ro::render::ActorEffectPlay> Played(std::uint32_t now_ms) const {
    std::vector<ro::render::ActorEffectPlay> played;
    for (const ro::sim::PlayedEffect& effect : world.Find(kBearerId)->effects) {
      played.push_back({effect.id, (now_ms - effect.since_ms) / kTickMs});
    }
    return played;
  }

  [[nodiscard]] ro::render::ActorBodyPose PoseAt(std::uint32_t now_ms) const {
    return ro::render::BodyPoseAfterEffects(Played(now_ms));
  }

  [[nodiscard]] float ScaleAt(std::uint32_t now_ms) const {
    return ro::render::BodyScaleAfterEffects(Played(now_ms));
  }

  [[nodiscard]] std::array<std::uint8_t, 4> TintAt(std::uint32_t now_ms) const {
    return ro::render::TintAfterEffectsAt(kWhite, Played(now_ms));
  }
};

}  // namespace

TEST_CASE("Drapeaux : un bit tombe le tic où son instance meurt", "[app][actoreffect]") {
  Bench bench;
  bench.Play(kShortJitterEffect);
  CHECK(bench.PoseAt(AtTick(kShortJitterLifeTicks - 1)).jitter_pixels > 0.0F);
  // ⚠ L'EFFET EST TOUJOURS DANS LA LISTE DU MONDE, mais son instance est
  // morte : plus personne ne relève le bit.
  CHECK(bench.PoseAt(AtTick(kShortJitterLifeTicks)).jitter_pixels == 0.0F);
  CHECK(bench.PoseAt(AtTick(kShortJitterLifeTicks * 100)).jitter_pixels == 0.0F);
}

TEST_CASE("Drapeaux : un bit ne tombe pas tant qu'un autre effet vivant le relève",
          "[app][actoreffect]") {
  Bench bench;
  bench.Play(kShortJitterEffect);
  bench.Play(kLongJitterEffect);
  // Le premier est mort, le second relève encore le même bit.
  CHECK(bench.PoseAt(AtTick(kShortJitterLifeTicks + 1)).jitter_pixels > 0.0F);
  CHECK(bench.PoseAt(AtTick(kLongJitterLifeTicks - 1)).jitter_pixels > 0.0F);
  CHECK(bench.PoseAt(AtTick(kLongJitterLifeTicks)).jitter_pixels == 0.0F);
}

TEST_CASE("Drapeaux : un bit tombe le tic où son effet est retiré", "[app][actoreffect]") {
  Bench bench;
  bench.Play(kOffScreenEffect);
  CHECK(bench.PoseAt(AtTick(1'000)).hidden);
  REQUIRE(bench.world.RemoveEffect(kBearerId, kOffScreenEffect));
  CHECK_FALSE(bench.PoseAt(AtTick(1'000)).hidden);

  // Retiré, un effet qui en partageait le bit avec un autre le laisse levé.
  bench.Play(kShortJitterEffect);
  bench.Play(kLongJitterEffect);
  REQUIRE(bench.world.RemoveEffect(kBearerId, kLongJitterEffect));
  CHECK(bench.PoseAt(AtTick(3)).jitter_pixels > 0.0F);
  REQUIRE(bench.world.RemoveEffect(kBearerId, kShortJitterEffect));
  CHECK(bench.PoseAt(AtTick(3)).jitter_pixels == 0.0F);
}

TEST_CASE("Drapeaux : un bit levé avant la borne de son cas vit autant que l'instance",
          "[app][actoreffect]") {
  Bench bench;
  bench.Play(kShrinkEffect);
  // Passé la borne de son compteur, le corps reste petit : le bit n'y est pas.
  CHECK(bench.ScaleAt(AtTick(kShrinkLifeTicks * 10)) == Catch::Approx(ro::render::kSmallBodyScale));
  REQUIRE(bench.world.RemoveEffect(kBearerId, kShrinkEffect));
  CHECK(bench.ScaleAt(AtTick(kShrinkLifeTicks * 10)) == Catch::Approx(1.0F));
}

TEST_CASE("Teinte : la couleur d'un effet qui a cessé d'écrire revient vers le blanc",
          "[app][actoreffect]") {
  Bench bench;
  bench.Play(kMagentaBlinkEffect);
  CHECK(bench.TintAt(AtTick(kMagentaLastTick)) == kMagenta);
  // Un tic sans écriture : huit de plus sur la seule composante qui n'est pas
  // déjà au plus haut.
  CHECK(bench.TintAt(AtTick(kMagentaLastTick + 1)) ==
        std::array<std::uint8_t, 4>{255, 8, 255, 255});
  CHECK(bench.TintAt(AtTick(kMagentaLastTick + 32)) == kWhite);
}

TEST_CASE("Teinte : la dernière écriture de chaque composante l'emporte", "[app][actoreffect]") {
  Bench bench;
  // Le camouflage écrit l'alpha à chaque tic et sans fin ; le magenta, joué
  // plus tard, ne touche pas l'alpha et cesse d'écrire à son quatre-vingt-unième
  // tic.
  constexpr std::uint32_t kCloakingEffect = 744;
  constexpr std::uint8_t kCloakedAlpha = 50;
  bench.Play(kCloakingEffect);
  bench.Play(kMagentaBlinkEffect, AtTick(10));
  CHECK(bench.TintAt(AtTick(10 + kMagentaLastTick)) ==
        std::array<std::uint8_t, 4>{255, 0, 255, kCloakedAlpha});
  // Le camouflage écrit encore : l'alpha ne remonte pas, le vert si.
  CHECK(bench.TintAt(AtTick(10 + kMagentaLastTick + 1)) ==
        std::array<std::uint8_t, 4>{255, 8, 255, kCloakedAlpha});
}

TEST_CASE("Teinte : l'ombre de Shadow Form tient tant que l'effet est porté, et part avec lui",
          "[app][actoreffect]") {
  constexpr std::uint32_t kShadowFormEffect = 809;
  constexpr std::array<std::uint8_t, 4> kShadow = {0, 0, 0, 50};
  Bench bench;
  bench.Play(kShadowFormEffect);
  CHECK(bench.TintAt(AtTick(0)) == kShadow);
  // Sa durée est l'infini du client : rien ne fait remonter la couleur.
  CHECK(bench.TintAt(AtTick(100'000)) == kShadow);
  // Retiré, l'effet cesse d'écrire : le corps est blanc à l'image suivante.
  REQUIRE(bench.world.RemoveEffect(kBearerId, kShadowFormEffect));
  CHECK(bench.TintAt(AtTick(100'001)) == kWhite);
}
