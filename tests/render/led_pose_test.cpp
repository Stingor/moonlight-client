#include "ro/render/led_pose.h"

#include <array>
#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/skill_held_pose.h"
#include "ro/render/skill_impl.h"

using ro::render::LedPoseWithin;
using ro::render::StateLeadsPose;
using ro::render::StateLedFrameAt;
using ro::render::UseLedPoseOf;

namespace {

// Un métier que rien ne distingue : aucun état ne lit le sien.
constexpr std::uint32_t kNoviceJob = 0;

}  // namespace

TEST_CASE("Pose menée : l'emploi ordinaire de seize métiers en mène une", "[render]") {
  // La danse : l'image zéro, quatre cents millisecondes.
  const auto danse = UseLedPoseOf(4021);
  REQUIRE(danse.has_value());
  CHECK(danse->play_rank == 12);
  CHECK(danse->first_frame == 0);
  CHECK(danse->last_frame == 0);
  CHECK(danse->hold_ms == 400);
  CHECK(danse->period_ticks == 4);
  CHECK(UseLedPoseOf(15).has_value());
  // Le ninja : deux à cinq, une seconde ; le canonnier : zéro à trois.
  const auto ninja = UseLedPoseOf(4222);
  REQUIRE(ninja.has_value());
  CHECK(ninja->first_frame == 2);
  CHECK(ninja->last_frame == 5);
  CHECK(ninja->hold_frame == 5);
  CHECK(ninja->hold_ms == 1000);
  const auto canonnier = UseLedPoseOf(24);
  REQUIRE(canonnier.has_value());
  CHECK(canonnier->last_frame == 3);
  // Un chevalier n'en a pas.
  CHECK_FALSE(UseLedPoseOf(7).has_value());
}

TEST_CASE("Pose menée : la fenêtre compte son début et non sa fin", "[render]") {
  CHECK_FALSE(LedPoseWithin(99, 100, 400));
  CHECK(LedPoseWithin(100, 100, 400));
  CHECK(LedPoseWithin(499, 100, 400));
  CHECK_FALSE(LedPoseWithin(500, 100, 400));
}

TEST_CASE("Pose menée : l'incantation d'état 19, ses cinq pas et sa tenue", "[render]") {
  // 🔴 LES TROIS COMPÉTENCES que la table d'incantation met à l'état 19.
  for (const std::uint32_t skill : {426U, 2581U, 5023U}) {
    CHECK(ro::render::SkillImplFor(skill).CastState() == ro::render::kLedCastState);
  }
  constexpr std::uint32_t kTickMs = 16;
  constexpr std::uint32_t kStepMs = 10 * kTickMs;
  const auto at = [](std::uint32_t elapsed_ms) {
    return StateLedFrameAt(ro::render::kLedCastState, 5, elapsed_ms, kTickMs, 0, kNoviceJob);
  };
  CHECK(at(0)->frame.frame == 0);
  CHECK(at(3 * kStepMs - 1)->frame.frame == 0);
  CHECK(at(3 * kStepMs)->frame.frame == 1);
  CHECK(at(2'999)->frame.frame == 1);
  CHECK(at(0)->frame.rank == 5);
  CHECK(at(2'999)->running);
  // Passé l'échéance, l'image reste ; la pose ne court plus.
  REQUIRE(at(ro::render::kLedCastPoseMs).has_value());
  CHECK(at(ro::render::kLedCastPoseMs)->frame.frame == 1);
  CHECK_FALSE(at(ro::render::kLedCastPoseMs)->running);
}

TEST_CASE("Pose menée : l'état 18 de Sharpshooting tient l'image zéro, même échu", "[render]") {
  // 🔴 LES CINQ COMPÉTENCES que la table d'incantation met à l'état 18 : Arrow
  // Shower, Sharpshooting, Arrow Vulcan et deux autres tirs.
  for (const std::uint32_t skill : {47U, 382U, 394U, 2288U, 5334U}) {
    CHECK(ro::render::SkillImplFor(skill).CastState() == 18);
  }
  constexpr std::int8_t kShootingState = 18;
  constexpr std::uint32_t kTickMs = 16;
  CHECK(StateLeadsPose(kShootingState));
  for (const std::uint32_t elapsed_ms : {0U, 160U, 1'000U, 2'999U, 3'000U, 60'000U}) {
    const auto led = StateLedFrameAt(kShootingState, 11, elapsed_ms, kTickMs, 1, kNoviceJob);
    REQUIRE(led.has_value());
    CHECK(led->frame.rank == 11);
    CHECK(led->frame.frame == 0);
    CHECK(led->running == (elapsed_ms < 3'000));
  }
  // L'incantation ordinaire ne mène rien : elle tient son image autrement.
  CHECK_FALSE(StateLeadsPose(8));
  CHECK_FALSE(StateLedFrameAt(8, 12, 0, kTickMs, 0, kNoviceJob).has_value());
}

TEST_CASE("Pose menée : les autres états d'incantation qui mènent leur pose", "[render]") {
  constexpr std::uint32_t kTickMs = 16;
  const auto frame = [](std::int8_t state, std::uint32_t steps, std::uint32_t period,
                        std::uint8_t draw) {
    return StateLedFrameAt(state, 12, steps * period * kTickMs, kTickMs, draw, kNoviceJob)
        ->frame.frame;
  };
  // Seize : un va-et-vient 1, 2, 3, 2 qui ne finit pas.
  CHECK(frame(16, 0, 10, 0) == 1);
  CHECK(frame(16, 1, 10, 0) == 2);
  CHECK(frame(16, 2, 10, 0) == 3);
  CHECK(frame(16, 3, 10, 0) == 2);
  CHECK(frame(16, 4, 10, 0) == 1);
  CHECK(frame(16, 7, 10, 0) == 2);
  // Vingt-huit : zéro à quatre, puis quatre tenue.
  CHECK(frame(28, 4, 10, 0) == 4);
  CHECK(frame(28, 5, 10, 0) == 4);
  // Trente-six : deux à cinq, six avances chacune.
  CHECK(frame(36, 0, 6, 0) == 2);
  CHECK(frame(36, 3, 6, 0) == 5);
  CHECK(frame(36, 9, 6, 0) == 5);
  // Trente-sept et quarante et un : le tirage ajoute un à l'image.
  CHECK(frame(37, 0, 10, 0) == 2);
  CHECK(frame(37, 0, 10, 1) == 3);
  CHECK(frame(41, 0, 10, 1) == 2);
  // Quarante-deux et cinquante : l'image zéro.
  CHECK(frame(42, 0, 10, 0) == 0);
  CHECK(frame(50, 100, 10, 0) == 0);
}

TEST_CASE("Pose menée : les états que l'emploi et le coup atteignent", "[render]") {
  // 🔴 ILS NE MENAIENT RIEN : une passe du rang se jouait, ou rien du tout pour
  // un état que la table des rangs ne nomme pas.
  constexpr std::uint32_t kTickMs = 16;
  constexpr std::uint8_t kStateRank = 12;
  const auto at = [](std::int8_t state, std::uint32_t steps, std::uint32_t period,
                     std::uint8_t draw, std::uint32_t job) {
    return *StateLedFrameAt(state, kStateRank, steps * period * kTickMs, kTickMs, draw, job);
  };
  for (const std::int8_t state :
       std::to_array<std::int8_t>({17, 20, 21, 22, 23, 24, 25, 26, 27, 30, 31, 32, 33, 34,
                                   35, 38, 39, 40, 43, 44, 46, 47, 48, 49, 51, 55, 56, 57})) {
    CHECK(StateLeadsPose(state));
  }
  // Ce qui déroule son rang ou ne pose rien n'en mène pas.
  for (const std::int8_t state : std::to_array<std::int8_t>({10, 12, 29, 45, 52, 53, 54, 58, 59})) {
    CHECK_FALSE(StateLeadsPose(state));
  }
  // Dix-sept tourne 1, 2, 3 ; quarante-sept, 0 à 3.
  CHECK(at(17, 3, 10, 0, kNoviceJob).frame.frame == 1);
  CHECK(at(17, 5, 10, 0, kNoviceJob).frame.frame == 3);
  CHECK(at(47, 4, 6, 0, kNoviceJob).frame.frame == 0);
  CHECK(at(47, 7, 6, 0, kNoviceJob).frame.frame == 3);
  // Vingt : quatre fois l'image 1, puis l'image 5 de la MARCHE.
  CHECK(at(20, 3, 10, 0, kNoviceJob).frame.rank == kStateRank);
  CHECK(at(20, 4, 10, 0, kNoviceJob).frame.rank == 1);
  CHECK(at(20, 4, 10, 0, kNoviceJob).frame.frame == 5);
  // Trente et un joue l'ATTAQUE, quel que soit le rang de l'état.
  constexpr std::array<std::uint8_t, 5> kDownKickFrames = {3, 5, 7, 8, 2};
  for (std::uint32_t step = 0; step < kDownKickFrames.size(); ++step) {
    CHECK(at(31, step, 5, 0, kNoviceJob).frame.rank == 10);
    CHECK(at(31, step, 5, 0, kNoviceJob).frame.frame == kDownKickFrames[step]);
  }
  // Vingt-six : la lignée du Soul Linker a ses propres images.
  CHECK(at(26, 0, 4, 0, kNoviceJob).frame.frame == 2);
  CHECK(at(26, 4, 4, 0, kNoviceJob).frame.frame == 4);
  CHECK(at(26, 0, 4, 0, 4049).frame.frame == 4);
  CHECK(at(26, 4, 4, 0, 4242).frame.frame == 2);
  // Trente-huit : le tirage choisit entre une montée et une descente.
  CHECK(at(38, 0, 6, 0, kNoviceJob).frame.frame == 0);
  CHECK(at(38, 3, 6, 0, kNoviceJob).frame.frame == 3);
  CHECK(at(38, 0, 6, 1, kNoviceJob).frame.frame == 3);
  CHECK(at(38, 3, 6, 1, kNoviceJob).frame.frame == 0);
  // Les gardes d'une image : 21 à 25, deux secondes.
  CHECK(at(23, 0, 10, 0, kNoviceJob).frame.frame == 3);
  CHECK(at(25, 0, 10, 0, kNoviceJob).frame.frame == 1);
  CHECK(StateLedFrameAt(23, kStateRank, 1'999, kTickMs, 0, kNoviceJob)->running);
  CHECK_FALSE(StateLedFrameAt(23, kStateRank, 2'000, kTickMs, 0, kNoviceJob)->running);
  // Quarante-neuf : huit images, six avances chacune, une seconde.
  CHECK(at(49, 7, 6, 0, kNoviceJob).frame.frame == 7);
  CHECK_FALSE(StateLedFrameAt(49, kStateRank, 1'000, kTickMs, 0, kNoviceJob)->running);
}

TEST_CASE("Pose menée : 49, 51, 56 et 57 se reposent en finissant, comme l'attaque", "[render]") {
  for (const std::int8_t state : std::to_array<std::int8_t>({2, 7, 12, 49, 51, 56, 57})) {
    CHECK(ro::render::StateRestsWhenFinished(state));
  }
  for (const std::int8_t state : std::to_array<std::int8_t>({8, 16, 18, 23, 36, 47, 50, 55})) {
    CHECK_FALSE(ro::render::StateRestsWhenFinished(state));
  }
  // Quarante-neuf s'inscrit comme l'emploi ordinaire ; cinquante et un finit
  // en attaque, et se repose donc en garde.
  CHECK(ro::render::StateLeftByFinishedMotion(49) == 7);
  CHECK(ro::render::StateLeftByFinishedMotion(51) == 2);
  CHECK(ro::render::StateLeftByFinishedMotion(23) == 23);
}

TEST_CASE("Pose menée : ce que trois familles d'états font naître en se posant", "[render]") {
  using ro::render::StateEffectKind;
  constexpr std::uint32_t kSummonerJob = 4218;
  const auto nineteen = ro::render::StateEffectsOf(19, kNoviceJob);
  REQUIRE(nineteen.size() == 4);
  CHECK(nineteen[0].kind == StateEffectKind::kRemove);
  CHECK(nineteen[0].effect_id == 446);
  CHECK(nineteen[1].effect_id == 445);
  CHECK(nineteen[2].kind == StateEffectKind::kSpawn);
  CHECK(nineteen[2].effect_id == 445);
  CHECK(nineteen[3].effect_id == 411);
  // Un Doram n'en fait rien.
  CHECK(ro::render::StateEffectsOf(19, kSummonerJob).empty());
  for (const std::int8_t state : std::to_array<std::int8_t>({21, 22, 23, 24, 25})) {
    const auto ready = ro::render::StateEffectsOf(state, kNoviceJob);
    REQUIRE(ready.size() == 1);
    CHECK(ready[0].kind == StateEffectKind::kSpawn);
    CHECK(ready[0].effect_id == 449);
  }
  const auto fallen = ro::render::StateEffectsOf(46, kNoviceJob);
  REQUIRE(fallen.size() == 4);
  CHECK(fallen[1].effect_id == 1072);
  CHECK(fallen[2].effect_id == 1072);
  CHECK(ro::render::StateEffectsOf(20, kNoviceJob).empty());
  CHECK(ro::render::StateEffectsOf(26, kNoviceJob).empty());
}

TEST_CASE("Pose menée : une pose de compétence tient dès l'annonce", "[render]") {
  // La garde de la blessure lit la même pose que l'écran : dès l'instant zéro,
  // toute pose qui dure montre une image. Celles qui durent le délai de combo
  // ne durent que ce que `ImpactPoseHoldMs` leur donne.
  constexpr std::uint32_t kTickMs = 16;
  for (const ro::render::SkillHeldPose& pose : ro::render::SkillHeldPoseTable()) {
    if (pose.hold_from_combo_delay || (pose.timed && pose.hold_ms == 0)) {
      continue;
    }
    CHECK(ro::render::HeldPoseFrameAt(pose, 0, kTickMs).has_value());
  }
}
