// LA MÉMOIRE DE L'IA DES COMPAGNONS : le délai d'une règle gardé au portail,
// à un changement de profil et à une reconnexion, échu pendant l'absence ; la
// trace d'un skill et sa suspension.

#include "ro/sim/companion_memory.h"

#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_ai.h"
#include "ro/sim/companion_rules.h"

#include "companion_rule_scene.h"

using companion_rule_scene::Always;
using companion_rule_scene::kHomunculusId;
using companion_rule_scene::kOwnerX;
using companion_rule_scene::kRowY;
using companion_rule_scene::kSelfBuffId;
using companion_rule_scene::kStartMs;
using companion_rule_scene::kTickMs;
using companion_rule_scene::Scene;
using ro::sim::CompanionMemory;
using ro::sim::CompanionRule;
using ro::sim::RuleAction;

namespace {

constexpr std::uint32_t kRuleDelayMs = 10000;
constexpr std::uint16_t kFarCells = 6;

// UNE RÈGLE QUI RAMÈNE AU MAÎTRE, pas plus d'une fois toutes les dix secondes.
// L'homoncule est à six cases, et le suivi de base ne joue qu'au-delà de dix :
// seule la règle le fait revenir.
constexpr int kLooseFollowCells = 10;

void Far(Scene& scene) {
  REQUIRE(scene.world.PlaceAt(kHomunculusId, ro::sim::CellPosition{kOwnerX + kFarCells, kRowY, 0}));
  scene.settings.follow_distance_cells = kLooseFollowCells;
  scene.settings.rules = {CompanionRule{.action = RuleAction::kReturnToOwner,
                                        .conditions = {Always()},
                                        .own_delay_ms = kRuleDelayMs}};
}

// LA RÈGLE DÉCIDE À CET INSTANT.
[[nodiscard]] bool RuleDecides(Scene& scene, std::uint32_t now_ms) {
  static_cast<void>(scene.Tick(now_ms));
  const std::optional<ro::sim::DecidingRule>& decided = scene.brain.deciding_rule();
  return decided.has_value() && decided->at_ms == now_ms;
}

}  // namespace

TEST_CASE("Mémoire du compagnon : le délai d'une règle survit au portail",
          "[sim][companion_memory]") {
  Scene scene;
  Far(scene);
  std::uint32_t now = kStartMs;
  REQUIRE(RuleDecides(scene, now));
  scene.brain.ForgetMap(false, now += kTickMs);
  // Le délai court toujours : la règle se tait, le reste de l'IA repart.
  CHECK_FALSE(RuleDecides(scene, now += kTickMs));
  CHECK(RuleDecides(scene, now += kRuleDelayMs));
}

TEST_CASE("Mémoire du compagnon : à la reconnexion, gardée ou échue selon l'absence",
          "[sim][companion_memory]") {
  Scene scene;
  Far(scene);
  std::uint32_t now = kStartMs;
  REQUIRE(RuleDecides(scene, now));
  const CompanionMemory memory = scene.brain.Remember(now += kTickMs);
  REQUIRE(memory.rules.size() == 1);
  // UNE AUTRE SESSION : une horloge du client qui n'a rien à voir.
  constexpr std::uint32_t kOtherClockMs = 7000000;
  constexpr std::uint32_t kShortAbsenceMs = 2000;
  SECTION("une absence courte : le délai court encore") {
    scene.brain = ro::sim::CompanionBrain();
    scene.brain.Recall(ro::sim::AgedMemory(memory, kShortAbsenceMs), kOtherClockMs);
    CHECK_FALSE(RuleDecides(scene, kOtherClockMs + kTickMs));
  }
  SECTION("une absence plus longue que le délai : échu") {
    const CompanionMemory aged = ro::sim::AgedMemory(memory, kRuleDelayMs);
    scene.brain = ro::sim::CompanionBrain();
    scene.brain.Recall(aged, kOtherClockMs);
    CHECK(RuleDecides(scene, kOtherClockMs + kTickMs));
  }
}

TEST_CASE("Mémoire du compagnon : l'état suit sa règle, pas son rang", "[sim][companion_memory]") {
  Scene scene;
  Far(scene);
  std::uint32_t now = kStartMs;
  REQUIRE(RuleDecides(scene, now));
  const CompanionMemory memory = scene.brain.Remember(now);
  // LE PROFIL DE RETOUR a une règle de plus en tête : le délai suit la sienne.
  scene.brain.Restart(memory, now += kTickMs);
  const CompanionRule guard = companion_rule_scene::Guard(
      ro::sim::RuleCondition{.subject = ro::sim::RuleSubject::kCompanion,
                             .predicate = ro::sim::RulePredicate::kHpPercent,
                             .comparison = ro::sim::RuleComparison::kBelow,
                             .number = 1});
  scene.settings.rules.insert(scene.settings.rules.begin(), guard);
  CHECK_FALSE(RuleDecides(scene, now += kTickMs));
  CHECK(RuleDecides(scene, now += kRuleDelayMs));
  CHECK(scene.brain.deciding_rule()->index == 1);
}

TEST_CASE("Mémoire du compagnon : la trace d'un skill et sa suspension",
          "[sim][companion_memory]") {
  Scene scene;
  std::uint32_t now = kStartMs;
  REQUIRE(scene.world.RecordCast(kHomunculusId,
                                 ro::sim::Cast{.skill_id = kSelfBuffId, .started_ms = now}));
  static_cast<void>(scene.Tick(now));
  scene.brain.NoteSkillRefused(companion_rule_scene::kBoltId, now);
  const CompanionMemory memory = scene.brain.Remember(now += kTickMs);
  bool traced = false;
  bool suspended = false;
  for (const ro::sim::CompanionSkillMemory& skill : memory.skills) {
    traced = traced || (skill.skill_id == kSelfBuffId && skill.cast_ago_ms == kTickMs);
    suspended = suspended || (skill.skill_id == companion_rule_scene::kBoltId &&
                              skill.suspended_for_ms == ro::sim::kSkillRefusalSuspendMs - kTickMs);
  }
  CHECK(traced);
  CHECK(suspended);
  // LA TRACE SURVIT AU PORTAIL : « lancé il y a moins de dix secondes » tient.
  scene.brain.ForgetMap(false, now);
  ro::sim::RuleCondition cast{.subject = ro::sim::RuleSubject::kCompanion,
                              .predicate = ro::sim::RulePredicate::kCastWithin,
                              .comparison = ro::sim::RuleComparison::kIs,
                              .seconds = 10,
                              .id = kSelfBuffId};
  scene.world.Remove(kHomunculusId);
  ro::sim::Actor fresh = companion_rule_scene::MakeActor(
      kHomunculusId, ro::sim::ActorKind::kNonPlayer, kOwnerX + 1, companion_rule_scene::kLifJob);
  fresh.familiar = ro::sim::Familiar::kHomunculus;
  scene.world.Spawn(fresh);
  CHECK(scene.Holds(cast, now += kTickMs));
}

TEST_CASE("Mémoire du compagnon : l'attente gardée seulement si on la garde",
          "[sim][companion_memory]") {
  Scene scene;
  scene.brain.ToggleStandby();
  REQUIRE(scene.brain.standby());
  CompanionMemory memory = scene.brain.Remember(kStartMs);
  CHECK(memory.standby);
  scene.brain.ForgetMap(false, kStartMs);
  CHECK_FALSE(scene.brain.standby());
  scene.brain.Recall(memory, kStartMs);
  CHECK(scene.brain.standby());
}

TEST_CASE("Mémoire du compagnon : un âge relu plus grand que l'horloge du processus",
          "[sim][companion_memory]") {
  // LE CLIENT VIENT DE REPARTIR, cinq minutes après avoir quitté : son horloge n'a pas
  // encore atteint l'âge du souvenir.
  constexpr std::uint32_t kFreshClockMs = 25000;
  constexpr std::uint32_t kAgeMs = 300000;
  CompanionMemory memory;
  memory.skills.push_back(
      ro::sim::CompanionSkillMemory{.skill_id = kSelfBuffId, .cast_ago_ms = kAgeMs});
  Scene scene;
  scene.brain.Recall(memory, kFreshClockMs);
  const ro::sim::RuleCondition cast{.subject = ro::sim::RuleSubject::kCompanion,
                                    .predicate = ro::sim::RulePredicate::kCastWithin,
                                    .comparison = ro::sim::RuleComparison::kIs,
                                    .seconds = 10,
                                    .id = kSelfBuffId};
  CHECK_FALSE(scene.Holds(cast, kFreshClockMs + kTickMs));
  // L'ÂGE SURVIT À LA CARTE SUIVANTE : ni remis à zéro, ni perdu.
  const CompanionMemory again = scene.brain.Remember(kFreshClockMs);
  REQUIRE(again.skills.size() == 1);
  CHECK(again.skills.front().cast_ago_ms == kAgeMs);
}
