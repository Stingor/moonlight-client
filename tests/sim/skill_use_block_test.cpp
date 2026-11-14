#include "ro/sim/skill_use_block.h"

#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/actor.h"
#include "ro/sim/player_status.h"
#include "ro/sim/status_effects.h"
#include "ro/sim/world.h"

using ro::sim::LocalRefusal;
using ro::sim::SkillUseBlock;
using ro::sim::SkillUseBlockFor;
using ro::sim::World;

// CE QUE LE SERVEUR JETTE SANS UN PAQUET, et que le client lit sur son propre
// personnage pour le dire au lieu de se taire.

namespace {

constexpr std::uint32_t kPlayer = 150000;
constexpr std::uint16_t kFireBolt = 19;
constexpr std::uint16_t kColdBolt = 14;
constexpr std::uint32_t kNowMs = 1000;

[[nodiscard]] World WorldWithPlayer() {
  World world;
  ro::sim::Actor self;
  self.id = kPlayer;
  world.Spawn(self);
  world.set_player_id(kPlayer);
  return world;
}

void SetBody(World& world, std::uint16_t body_state, std::uint16_t health_state,
             std::uint32_t option) {
  REQUIRE(world.SetStates(kPlayer, body_state, health_state, option, kNowMs, false));
}

[[nodiscard]] std::optional<SkillUseBlock> Block(const World& world, std::uint16_t skill_id,
                                                 bool lent = false) {
  return SkillUseBlockFor(world, skill_id, lent);
}

}  // namespace

TEST_CASE("Skill bloqué : rien ne bloque un personnage libre, ni un monde sans joueur",
          "[sim][skill]") {
  CHECK_FALSE(Block(World{}, kFireBolt).has_value());
  CHECK_FALSE(Block(WorldWithPlayer(), kFireBolt).has_value());
}

TEST_CASE("Skill bloqué : assis, rien ne part, même un skill prêté", "[sim][skill]") {
  World world = WorldWithPlayer();
  REQUIRE(world.SetPosture(kPlayer, ro::sim::Posture::kSitting, kNowMs));
  CHECK(Block(world, kFireBolt) == SkillUseBlock{.reason = LocalRefusal::kSitting});
  CHECK(Block(world, kFireBolt, true) == SkillUseBlock{.reason = LocalRefusal::kSitting});
}

TEST_CASE("Skill bloqué : le corps pris retient tout, sauf les skills qui délivrent",
          "[sim][skill]") {
  World world = WorldWithPlayer();
  SetBody(world, ro::sim::kBodyStunned, 0, 0);
  CHECK(Block(world, kFireBolt) ==
        SkillUseBlock{.reason = LocalRefusal::kStatus, .status = ro::sim::kEfstStun});
  // Le prêt d'un objet n'y change rien.
  CHECK(Block(world, kFireBolt, true).has_value());
  CHECK_FALSE(Block(world, ro::sim::kRefreshSkill).has_value());
  CHECK_FALSE(Block(world, ro::sim::kGroomingSkill).has_value());
  // ⚠ LA PÉTRIFICATION EN COURS ET LA BRÛLURE NE RETIENNENT PAS : le serveur
  // les excepte.
  SetBody(world, ro::sim::kBodyPetrifying, 0, 0);
  CHECK_FALSE(Block(world, kFireBolt).has_value());
  SetBody(world, ro::sim::kBodyBurning, 0, 0);
  CHECK_FALSE(Block(world, kFireBolt).has_value());
}

TEST_CASE("Skill bloqué : Silence et Berserk passent un skill prêté par un objet", "[sim][skill]") {
  World world = WorldWithPlayer();
  SetBody(world, 0, ro::sim::kHealthSilenced, 0);
  CHECK(Block(world, kFireBolt) ==
        SkillUseBlock{.reason = LocalRefusal::kStatus, .status = ro::sim::kEfstSilence});
  CHECK_FALSE(Block(world, kFireBolt, true).has_value());
  CHECK_FALSE(Block(world, ro::sim::kGentleTouchCureSkill).has_value());

  SetBody(world, 0, 0, 0);
  world.NotePlayerStatus(ro::sim::kEfstBerserk, true, 0, kNowMs);
  CHECK(Block(world, kFireBolt) ==
        SkillUseBlock{.reason = LocalRefusal::kStatus, .status = ro::sim::kEfstBerserk});
  CHECK_FALSE(Block(world, kFireBolt, true).has_value());
  world.NotePlayerStatus(ro::sim::kEfstBerserk, false, 0, kNowMs);
  CHECK_FALSE(Block(world, kFireBolt).has_value());
}

TEST_CASE("Skill bloqué : Play Dead et Chase Walk n'autorisent qu'eux-mêmes", "[sim][skill]") {
  World world = WorldWithPlayer();
  world.NotePlayerStatus(ro::sim::kEfstTrickDead, true, 0, kNowMs);
  CHECK(Block(world, kFireBolt) ==
        SkillUseBlock{.reason = LocalRefusal::kStatus, .status = ro::sim::kEfstTrickDead});
  CHECK_FALSE(Block(world, ro::sim::kPlayDeadSkill).has_value());
  world.NotePlayerStatus(ro::sim::kEfstTrickDead, false, 0, kNowMs);

  SetBody(world, 0, 0, ro::sim::kOptionChaseWalk);
  CHECK(Block(world, kFireBolt) == SkillUseBlock{.reason = LocalRefusal::kChaseWalk});
  CHECK_FALSE(Block(world, ro::sim::kChaseWalkSkill).has_value());
}

TEST_CASE("Skill bloqué : des manières négatives coupent les skills, pas un skill prêté",
          "[sim][skill]") {
  World world = WorldWithPlayer();
  REQUIRE(world.SetPlayerParameter(static_cast<std::uint16_t>(ro::sim::ServerParameter::kManner),
                                   -3) == ro::sim::ParameterOutcome::kStored);
  CHECK(Block(world, kFireBolt) == SkillUseBlock{.reason = LocalRefusal::kMuted});
  CHECK_FALSE(Block(world, kFireBolt, true).has_value());
}

TEST_CASE("Skill bloqué : la monture d'événement n'autorise rien", "[sim][skill]") {
  World world = WorldWithPlayer();
  world.NotePlayerStatus(ro::sim::kStatusAllRiding, true, 0, kNowMs);
  CHECK(Block(world, kFireBolt, true) ==
        SkillUseBlock{.reason = LocalRefusal::kStatus, .status = ro::sim::kStatusAllRiding});
}

TEST_CASE("Skill bloqué : le refus noté porte la raison et l'état", "[sim][skill]") {
  const ro::sim::SkillRefusal refusal = ro::sim::BlockedRefusal(
      kFireBolt, SkillUseBlock{.reason = LocalRefusal::kStatus, .status = ro::sim::kEfstSilence},
      kNowMs);
  CHECK(refusal.skill_id == kFireBolt);
  CHECK(refusal.local == LocalRefusal::kStatus);
  CHECK(refusal.blocking_status == ro::sim::kEfstSilence);
  CHECK(refusal.refused_ms == kNowMs);
}

TEST_CASE("Skill bloqué : la porte retient le lancer et ne redit pas le même refus",
          "[sim][skill]") {
  World world = WorldWithPlayer();
  ro::sim::SkillBlockGate gate;
  CHECK_FALSE(gate.Refuse(world, kFireBolt, false, kNowMs));
  CHECK(world.skill_book().refusals().empty());

  REQUIRE(world.SetPosture(kPlayer, ro::sim::Posture::kSitting, kNowMs));
  CHECK(gate.Refuse(world, kFireBolt, false, kNowMs));
  REQUIRE(world.skill_book().refusals().size() == 1);
  CHECK(world.skill_book().refusals()[0].local == LocalRefusal::kSitting);
  CHECK(world.skill_book().refusals()[0].refused_ms == kNowMs);

  // ⚠ UNE TOUCHE TENUE redemande : bloquée encore, mais dite une seule fois.
  CHECK(gate.Refuse(world, kFireBolt, false, kNowMs + 30));
  CHECK(world.skill_book().refusals().size() == 1);
  // Un autre skill, ou un lancer parti entre-temps, se redit.
  CHECK(gate.Refuse(world, kColdBolt, false, kNowMs + 60));
  CHECK(world.skill_book().refusals().size() == 2);
  gate.NoteSent();
  CHECK(gate.Refuse(world, kColdBolt, false, kNowMs + 90));
  CHECK(world.skill_book().refusals().size() == 3);

  // Debout, la porte s'ouvre ; rassis, le refus se redit.
  REQUIRE(world.SetPosture(kPlayer, ro::sim::Posture::kStanding, kNowMs + 120));
  CHECK_FALSE(gate.Refuse(world, kColdBolt, false, kNowMs + 120));
  REQUIRE(world.SetPosture(kPlayer, ro::sim::Posture::kSitting, kNowMs + 150));
  CHECK(gate.Refuse(world, kColdBolt, false, kNowMs + 150));
  CHECK(world.skill_book().refusals().size() == 4);
}

TEST_CASE("Incantation interrompue : le grimoire la note pour le joueur seul", "[sim][skill]") {
  World world = WorldWithPlayer();
  ro::sim::Actor other;
  other.id = kPlayer + 1;
  world.Spawn(other);
  ro::sim::Cast cast;
  cast.skill_id = kFireBolt;
  cast.started_ms = kNowMs;
  cast.duration_ms = 2000;
  REQUIRE(world.RecordCast(kPlayer, cast));
  REQUIRE(world.RecordCast(other.id, cast));

  // Un autre lanceur coupé ne regarde pas le joueur.
  REQUIRE(world.CancelCast(other.id, kNowMs + 500));
  CHECK(world.skill_book().refusals().empty());

  REQUIRE(world.CancelCast(kPlayer, kNowMs + 500));
  REQUIRE(world.skill_book().refusals().size() == 1);
  const ro::sim::SkillRefusal& noted = world.skill_book().refusals()[0];
  CHECK(noted.skill_id == kFireBolt);
  CHECK(noted.local == LocalRefusal::kInterrupted);
  CHECK(noted.refused_ms == kNowMs + 500);

  // ⚠ UNE INCANTATION DÉJÀ ÉCHUE N'EST PAS INTERROMPUE : rien de plus.
  REQUIRE(world.RecordCast(kPlayer, cast));
  REQUIRE(world.CancelCast(kPlayer, kNowMs + 2500));
  CHECK(world.skill_book().refusals().size() == 1);

  // Auto Counter coupe sa jauge en se déclenchant : ce n'est pas un échec.
  cast.skill_id = ro::sim::kAutoCounterSkill;
  REQUIRE(world.RecordCast(kPlayer, cast));
  REQUIRE(world.CancelCast(kPlayer, kNowMs + 500));
  CHECK(world.skill_book().refusals().size() == 1);
}
