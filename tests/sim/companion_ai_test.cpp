// L'IA DES COMPAGNONS : suivi, choix de la cible, brides, ordres directs,
// attente, changement de carte.

#include "ro/sim/companion_ai.h"

#include <cstddef>
#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/map_mode.h"
#include "ro/sim/world.h"

using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::AiCompanion;
using ro::sim::BasicAggression;
using ro::sim::BasicCompanionAiSettings;
using ro::sim::CellMovement;
using ro::sim::CellPosition;
using ro::sim::CompanionAiReason;
using ro::sim::CompanionAiSettings;
using ro::sim::CompanionBrain;
using ro::sim::CompanionFacts;
using ro::sim::CompanionOrder;
using ro::sim::DirectOrder;
using ro::sim::Familiar;
using ro::sim::Strike;
using ro::sim::World;

namespace {

constexpr std::uint32_t kOwnerId = 100;
constexpr std::uint32_t kHomunculusId = 200;
constexpr std::uint32_t kMonsterId = 300;
constexpr std::uint32_t kSecondMonsterId = 301;
constexpr std::uint32_t kStrangerId = 400;
constexpr std::uint16_t kLifJob = 6001;
constexpr std::uint16_t kFilirJob = 6003;
constexpr std::uint16_t kOwnerX = 100;
constexpr std::uint16_t kRowY = 100;
constexpr std::uint16_t kSpeedMsPerCell = 150;
constexpr std::uint32_t kAmotionMs = 500;
// Un instant de départ quelconque, loin de zéro.
constexpr std::uint32_t kStartMs = 100000;

[[nodiscard]] Actor MakeActor(std::uint32_t id, ActorKind kind, std::uint16_t x) {
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  actor.position = CellPosition{x, kRowY, 0};
  actor.speed_ms_per_cell = kSpeedMsPerCell;
  return actor;
}

// Le maître en (100, 100), son homoncule à `homunculus_x` sur la même rangée.
struct Scene {
  World world;
  CompanionBrain brain;
  CompanionFacts facts;
  CompanionAiSettings settings;

  Scene(std::uint16_t job, std::uint16_t homunculus_x) {
    world.Spawn(MakeActor(kOwnerId, ActorKind::kPlayer, kOwnerX));
    world.set_player_id(kOwnerId);
    PlaceHomunculus(homunculus_x);
    facts = CompanionFacts{.actor_id = kHomunculusId,
                           .kind = AiCompanion::kHomunculus,
                           .job = job,
                           .active = true,
                           .hp = 100,
                           .max_hp = 100,
                           .sp = 50,
                           .max_sp = 50,
                           .amotion_ms = kAmotionMs,
                           .attack_range_cells = 1};
    settings = BasicCompanionAiSettings(AiCompanion::kHomunculus, job);
  }

  void PlaceHomunculus(std::uint16_t x) {
    Actor homunculus = MakeActor(kHomunculusId, ActorKind::kNonPlayer, x);
    homunculus.familiar = Familiar::kHomunculus;
    homunculus.look.job = facts.job;
    world.Spawn(homunculus);
  }

  void PlaceMonster(std::uint32_t id, std::uint16_t x) {
    world.Spawn(MakeActor(id, ActorKind::kMonster, x));
  }

  void Strikes(std::uint32_t attacker, std::uint32_t target, std::uint32_t now_ms) {
    static_cast<void>(
        world.RecordStrike(Strike{.attacker = attacker, .target = target, .started_ms = now_ms}));
  }

  [[nodiscard]] std::optional<CompanionOrder> Tick(std::uint32_t now_ms) {
    return brain.Tick(world, facts, settings, now_ms);
  }
};

[[nodiscard]] bool IsKind(const std::optional<CompanionOrder>& order, CompanionOrder::Kind kind) {
  return order.has_value() && order->kind == kind;
}

}  // namespace

TEST_CASE("IA compagnon : il suit au-delà de trois cases, et pas en deçà", "[sim][companion_ai]") {
  Scene near(kLifJob, kOwnerX + 3);
  CHECK_FALSE(near.Tick(kStartMs).has_value());

  Scene far(kLifJob, kOwnerX + 4);
  const auto order = far.Tick(kStartMs);
  REQUIRE(IsKind(order, CompanionOrder::Kind::kReturnToOwner));
  CHECK(far.brain.last_reason() == CompanionAiReason::kFollowOwner);
}

TEST_CASE("IA compagnon : pas de renvoi tant qu'il marche", "[sim][companion_ai]") {
  Scene scene(kLifJob, kOwnerX + 10);
  REQUIRE(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kReturnToOwner));
  // Il marche vers une case encore à six cases du maître, en 600 ms.
  REQUIRE(scene.world.StartMoving(
      kHomunculusId,
      CellMovement{CellPosition{kOwnerX + 10, kRowY, 0}, CellPosition{kOwnerX + 6, kRowY, 0}, 8, 8},
      kStartMs));
  CHECK_FALSE(scene.Tick(kStartMs + 140).has_value());
  CHECK_FALSE(scene.Tick(kStartMs + 500).has_value());
  // Arrêté trop loin : il redemande.
  CHECK(IsKind(scene.Tick(kStartMs + 1100), CompanionOrder::Kind::kReturnToOwner));
}

TEST_CASE("IA compagnon : défendre le maître passe avant sa propre cible", "[sim][companion_ai]") {
  Scene scene(kFilirJob, kOwnerX + 1);
  scene.PlaceMonster(kMonsterId, kOwnerX + 8);
  scene.PlaceMonster(kSecondMonsterId, kOwnerX + 2);
  scene.Strikes(kMonsterId, kOwnerId, kStartMs - 100);

  const auto order = scene.Tick(kStartMs);
  REQUIRE(IsKind(order, CompanionOrder::Kind::kMove));
  CHECK(scene.brain.target_id() == kMonsterId);
  CHECK(scene.brain.last_reason() == CompanionAiReason::kDefendOwner);
}

TEST_CASE("IA compagnon : Lif ne prend pas l'initiative, Filir oui", "[sim][companion_ai]") {
  Scene lif(kLifJob, kOwnerX + 1);
  lif.PlaceMonster(kMonsterId, kOwnerX + 2);
  CHECK_FALSE(lif.Tick(kStartMs).has_value());

  Scene filir(kFilirJob, kOwnerX + 1);
  filir.PlaceMonster(kMonsterId, kOwnerX + 2);
  const auto order = filir.Tick(kStartMs);
  REQUIRE(IsKind(order, CompanionOrder::Kind::kAttack));
  CHECK(order->target_id == kMonsterId);
  CHECK(filir.brain.last_reason() == CompanionAiReason::kOwnInitiative);
}

TEST_CASE("IA compagnon : l'agressivité d'origine suit l'espèce", "[sim][companion_ai]") {
  // Homoncules : Lif, Amistr non ; Filir, Vanilmirth oui ; sur les quatre lignées.
  CHECK_FALSE(BasicAggression(AiCompanion::kHomunculus, 6001));
  CHECK_FALSE(BasicAggression(AiCompanion::kHomunculus, 6002));
  CHECK(BasicAggression(AiCompanion::kHomunculus, 6003));
  CHECK(BasicAggression(AiCompanion::kHomunculus, 6004));
  CHECK_FALSE(BasicAggression(AiCompanion::kHomunculus, 6013));
  CHECK(BasicAggression(AiCompanion::kHomunculus, 6016));
  // Hors des seize types : pas de cible propre.
  CHECK_FALSE(BasicAggression(AiCompanion::kHomunculus, 6000));
  CHECK_FALSE(BasicAggression(AiCompanion::kHomunculus, 6017));
  CHECK_FALSE(BasicAggression(AiCompanion::kHomunculus, 6048));
  // Mercenaires : l'inverse, les trente des guildes non, tout autre oui.
  CHECK_FALSE(BasicAggression(AiCompanion::kMercenary, 6017));
  CHECK_FALSE(BasicAggression(AiCompanion::kMercenary, 6046));
  CHECK(BasicAggression(AiCompanion::kMercenary, 6016));
  CHECK(BasicAggression(AiCompanion::kMercenary, 6047));
  CHECK(BasicAggression(AiCompanion::kMercenary, 1002));
}

TEST_CASE("IA compagnon : sa vue s'arrête à vingt cases", "[sim][companion_ai]") {
  Scene beyond(kFilirJob, kOwnerX + 1);
  beyond.PlaceMonster(kMonsterId, kOwnerX + 22);
  CHECK_FALSE(beyond.Tick(kStartMs).has_value());

  Scene within(kFilirJob, kOwnerX + 1);
  within.PlaceMonster(kMonsterId, kOwnerX + 21);
  const auto order = within.Tick(kStartMs);
  REQUIRE(IsKind(order, CompanionOrder::Kind::kMove));
  CHECK(within.brain.target_id() == kMonsterId);
}

TEST_CASE("IA compagnon : le rayon d'engagement est le carré du serveur, pas un cercle",
          "[sim][companion_ai]") {
  constexpr int kEngageCells = 18;
  constexpr std::uint16_t kCornerDx = 17;
  constexpr std::uint16_t kCornerDy = 12;
  // 17 cases en x, 12 en y : dans le carré de 18, à plus de 20 cases à vol
  // d'oiseau.
  Scene corner(kFilirJob, kOwnerX + 1);
  // Le rayon de vue de Moonlight, pour que l'engagement ne soit pas borné en deçà.
  corner.world.session().view_cells = kEngageCells;
  corner.settings.engage_still_cells = kEngageCells;
  corner.settings.engage_walking_cells = kEngageCells;
  Actor monster = MakeActor(kMonsterId, ActorKind::kMonster, kOwnerX + kCornerDx);
  monster.position.y = kRowY + kCornerDy;
  corner.world.Spawn(monster);
  static_cast<void>(corner.Tick(kStartMs));
  CHECK(corner.brain.target_id() == kMonsterId);

  Scene beyond(kFilirJob, kOwnerX + 1);
  beyond.settings.engage_still_cells = kEngageCells;
  beyond.settings.engage_walking_cells = kEngageCells;
  beyond.PlaceMonster(kMonsterId, kOwnerX + kEngageCells + 1);
  static_cast<void>(beyond.Tick(kStartMs));
  CHECK(beyond.brain.target_id() == 0);
}

TEST_CASE("IA compagnon : la laisse ramène dans la laisse, même sous un suivi plus large",
          "[sim][companion_ai]") {
  constexpr int kLeashCells = 7;
  constexpr int kWideFollowCells = 18;
  constexpr std::uint16_t kBeyondLeashDx = 12;
  Scene scene(kFilirJob, kOwnerX + kBeyondLeashDx);
  scene.settings.follow_distance_cells = kWideFollowCells;
  scene.settings.leash_still_cells = kLeashCells;
  scene.settings.leash_walking_cells = kLeashCells;
  CHECK(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kReturnToOwner));
  CHECK(scene.brain.last_reason() == CompanionAiReason::kLeash);
}

TEST_CASE("IA compagnon : une cible hors d'atteinte de la laisse n'est pas prise",
          "[sim][companion_ai]") {
  constexpr int kLeashCells = 7;
  constexpr int kEngageCells = 18;
  // La laisse plus la portée d'un coup : 8 cases du maître au plus.
  Scene beyond(kFilirJob, kOwnerX + 1);
  beyond.settings.leash_still_cells = kLeashCells;
  beyond.settings.engage_still_cells = kEngageCells;
  beyond.PlaceMonster(kMonsterId, kOwnerX + kLeashCells + 2);
  static_cast<void>(beyond.Tick(kStartMs));
  CHECK(beyond.brain.target_id() == 0);

  Scene within(kFilirJob, kOwnerX + 1);
  within.settings.leash_still_cells = kLeashCells;
  within.settings.engage_still_cells = kEngageCells;
  within.PlaceMonster(kMonsterId, kOwnerX + kLeashCells + 1);
  static_cast<void>(within.Tick(kStartMs));
  CHECK(within.brain.target_id() == kMonsterId);
}

TEST_CASE("IA compagnon : une cible morte ou cachée est abandonnée", "[sim][companion_ai]") {
  Scene scene(kFilirJob, kOwnerX + 1);
  scene.PlaceMonster(kMonsterId, kOwnerX + 2);
  REQUIRE(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kAttack));

  SECTION("morte") {
    Actor dead = *scene.world.Find(kMonsterId);
    dead.posture = ro::sim::Posture::kDead;
    scene.world.Spawn(dead);
  }
  SECTION("cachée") {
    Actor hidden = *scene.world.Find(kMonsterId);
    hidden.option = ro::sim::kOptionHide;
    scene.world.Spawn(hidden);
  }
  CHECK_FALSE(scene.Tick(kStartMs + kAmotionMs + 140).has_value());
  CHECK(scene.brain.target_id() == 0);
}

TEST_CASE("IA compagnon : le monstre d'un autre joueur est laissé, sauf ordre direct",
          "[sim][companion_ai]") {
  Scene scene(kFilirJob, kOwnerX + 1);
  scene.world.Spawn(MakeActor(kStrangerId, ActorKind::kPlayer, kOwnerX + 3));
  scene.PlaceMonster(kMonsterId, kOwnerX + 2);
  scene.Strikes(kMonsterId, kStrangerId, kStartMs - 100);
  CHECK_FALSE(scene.Tick(kStartMs).has_value());

  scene.brain.Command(DirectOrder{.kind = DirectOrder::Kind::kAttack, .target_id = kMonsterId});
  const auto order = scene.Tick(kStartMs + 140);
  REQUIRE(IsKind(order, CompanionOrder::Kind::kAttack));
  CHECK(order->target_id == kMonsterId);
  CHECK(scene.brain.last_reason() == CompanionAiReason::kDirectAttack);
}

TEST_CASE("IA compagnon : pas d'attaque avant la fin du coup précédent", "[sim][companion_ai]") {
  Scene scene(kFilirJob, kOwnerX + 1);
  scene.PlaceMonster(kMonsterId, kOwnerX + 2);
  REQUIRE(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kAttack));
  CHECK_FALSE(scene.Tick(kStartMs + 140).has_value());
  CHECK_FALSE(scene.Tick(kStartMs + 420).has_value());
  CHECK(IsKind(scene.Tick(kStartMs + 560), CompanionOrder::Kind::kAttack));
}

TEST_CASE("IA compagnon : pas d'attaque pendant le délai d'arrivée", "[sim][companion_ai]") {
  Scene scene(kFilirJob, kOwnerX + 1);
  scene.PlaceMonster(kMonsterId, kOwnerX + 2);
  scene.brain.ForgetMap(false, kStartMs);
  CHECK_FALSE(scene.Tick(kStartMs).has_value());
  CHECK_FALSE(scene.Tick(kStartMs + 4900).has_value());
  CHECK(IsKind(scene.Tick(kStartMs + ro::sim::kServerCompanionAttackHoldMs + 40),
               CompanionOrder::Kind::kAttack));
}

TEST_CASE("IA compagnon : un « aller ici » lointain se coupe au point milieu",
          "[sim][companion_ai]") {
  Scene scene(kLifJob, kOwnerX + 1);
  scene.brain.Command(
      DirectOrder{.kind = DirectOrder::Kind::kMove, .cell = CellPosition{kOwnerX + 31, kRowY, 0}});
  const auto first = scene.Tick(kStartMs);
  REQUIRE(IsKind(first, CompanionOrder::Kind::kMove));
  CHECK(first->cell == CellPosition{kOwnerX + 16, kRowY, 0});
  CHECK(scene.brain.reserved_count() == 1);

  // Arrivé au point milieu : le reste du trajet part.
  scene.PlaceHomunculus(kOwnerX + 16);
  const auto rest = scene.Tick(kStartMs + 1100);
  REQUIRE(IsKind(rest, CompanionOrder::Kind::kMove));
  CHECK(rest->cell == CellPosition{kOwnerX + 31, kRowY, 0});
  CHECK(scene.brain.reserved_count() == 0);
}

TEST_CASE("IA compagnon : un ordre direct vide la file réservée", "[sim][companion_ai]") {
  Scene scene(kLifJob, kOwnerX + 1);
  const DirectOrder step{.kind = DirectOrder::Kind::kMove,
                         .cell = CellPosition{kOwnerX + 3, kRowY, 0}};
  CHECK(scene.brain.Reserve(step));
  CHECK(scene.brain.Reserve(step));
  CHECK(scene.brain.reserved_count() == 2);
  scene.brain.Command(step);
  CHECK(scene.brain.reserved_count() == 0);

  for (std::size_t queued = 0; queued < ro::sim::kCompanionReserveCapacity; ++queued) {
    CHECK(scene.brain.Reserve(step));
  }
  CHECK_FALSE(scene.brain.Reserve(step));
}

TEST_CASE("IA compagnon : l'attente bascule, ne riposte pas, et cède à un ordre",
          "[sim][companion_ai]") {
  Scene scene(kLifJob, kOwnerX + 1);
  scene.PlaceMonster(kMonsterId, kOwnerX + 2);
  scene.Strikes(kMonsterId, kHomunculusId, kStartMs - 100);

  scene.brain.ToggleStandby();
  REQUIRE(scene.brain.standby());
  CHECK_FALSE(scene.Tick(kStartMs).has_value());

  scene.brain.Command(DirectOrder{.kind = DirectOrder::Kind::kAttack, .target_id = kMonsterId});
  CHECK_FALSE(scene.brain.standby());
  CHECK(IsKind(scene.Tick(kStartMs + 140), CompanionOrder::Kind::kAttack));

  scene.brain.ToggleStandby();
  scene.brain.ToggleStandby();
  CHECK_FALSE(scene.brain.standby());
}

TEST_CASE("IA compagnon : en attente, il colle au maître", "[sim][companion_ai]") {
  Scene scene(kLifJob, kOwnerX + 6);
  scene.brain.ToggleStandby();
  REQUIRE(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kReturnToOwner));
  CHECK(scene.brain.last_reason() == CompanionAiReason::kStandby);
}

TEST_CASE("IA compagnon : le changement de carte remet tout à zéro", "[sim][companion_ai]") {
  Scene scene(kLifJob, kOwnerX + 1);
  scene.brain.ToggleStandby();
  CHECK(scene.brain.Reserve(DirectOrder{}));

  SECTION("l'attente se perd d'usine") {
    scene.brain.ForgetMap(false, kStartMs);
    CHECK_FALSE(scene.brain.standby());
  }
  SECTION("ou se garde si le réglage le dit") {
    scene.brain.ForgetMap(true, kStartMs);
    CHECK(scene.brain.standby());
  }
  CHECK(scene.brain.reserved_count() == 0);
}

TEST_CASE("IA compagnon : un skill ciblé attend que l'approche soit finie", "[sim][companion_ai]") {
  // Le serveur mesure la portée sur la position COURANTE : un skill lancé en
  // chemin serait refusé sans un mot.
  Scene scene(kLifJob, kOwnerX + 1);
  scene.PlaceMonster(kMonsterId, kOwnerX + 10);
  scene.brain.Command(DirectOrder{.kind = DirectOrder::Kind::kSkillOnActor,
                                  .target_id = kMonsterId,
                                  .skill_id = 8009,
                                  .skill_level = 1,
                                  .skill_range_cells = 2});
  REQUIRE(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kMove));
  // Sept cases à 150 ms : il arrive au bout de 1050 ms.
  REQUIRE(scene.world.StartMoving(
      kHomunculusId,
      CellMovement{CellPosition{kOwnerX + 1, kRowY, 0}, CellPosition{kOwnerX + 8, kRowY, 0}, 8, 8},
      kStartMs));
  CHECK_FALSE(IsKind(scene.Tick(kStartMs + 140), CompanionOrder::Kind::kSkill));
  CHECK_FALSE(IsKind(scene.Tick(kStartMs + 700), CompanionOrder::Kind::kSkill));
  CHECK(IsKind(scene.Tick(kStartMs + 1120), CompanionOrder::Kind::kSkill));
}

TEST_CASE("IA compagnon : un skill qui n'aboutit pas s'abandonne", "[sim][companion_ai]") {
  constexpr std::uint16_t kSkillId = 8013;
  Scene scene(kLifJob, kOwnerX + 1);

  SECTION("une case hors de vue") {
    scene.brain.Command(DirectOrder{.kind = DirectOrder::Kind::kSkillOnCell,
                                    .cell = CellPosition{kOwnerX + 30, kRowY, 0},
                                    .skill_id = kSkillId,
                                    .skill_level = 1,
                                    .skill_range_cells = 2});
    CHECK_FALSE(scene.Tick(kStartMs).has_value());
  }
  SECTION("une case qu'il n'atteint pas") {
    scene.brain.Command(DirectOrder{.kind = DirectOrder::Kind::kSkillOnCell,
                                    .cell = CellPosition{kOwnerX + 15, kRowY, 0},
                                    .skill_id = kSkillId,
                                    .skill_level = 1,
                                    .skill_range_cells = 2});
    const auto approach = scene.Tick(kStartMs);
    REQUIRE(IsKind(approach, CompanionOrder::Kind::kMove));
    // Il n'a pas bougé : arrêté hors de portée, il ne redemande pas.
    CHECK_FALSE(scene.Tick(kStartMs + 1100).has_value());
  }
  // L'ordre est fini : ni déplacement ni skill ensuite.
  for (std::uint32_t later_ms = 1400; later_ms <= 4200; later_ms += 1400) {
    const auto order = scene.Tick(kStartMs + later_ms);
    CHECK_FALSE(IsKind(order, CompanionOrder::Kind::kMove));
    CHECK_FALSE(IsKind(order, CompanionOrder::Kind::kSkill));
  }
}

TEST_CASE("IA compagnon : un skill ciblé s'approche, puis part une fois", "[sim][companion_ai]") {
  constexpr std::uint16_t kSkillId = 8009;
  constexpr std::uint16_t kSkillLevel = 3;
  Scene scene(kLifJob, kOwnerX + 1);
  scene.PlaceMonster(kMonsterId, kOwnerX + 10);
  scene.brain.Command(DirectOrder{.kind = DirectOrder::Kind::kSkillOnActor,
                                  .target_id = kMonsterId,
                                  .skill_id = kSkillId,
                                  .skill_level = kSkillLevel,
                                  .skill_range_cells = 2});
  const auto approach = scene.Tick(kStartMs);
  REQUIRE(IsKind(approach, CompanionOrder::Kind::kMove));
  CHECK(approach->cell == CellPosition{kOwnerX + 8, kRowY, 0});

  // Arrivé et posé : le skill part.
  scene.PlaceHomunculus(kOwnerX + 8);
  const auto cast = scene.Tick(kStartMs + 1100);
  REQUIRE(IsKind(cast, CompanionOrder::Kind::kSkill));
  CHECK(cast->target_id == kMonsterId);
  CHECK(cast->skill_id == kSkillId);
  CHECK(cast->skill_level == kSkillLevel);

  CHECK_FALSE(IsKind(scene.Tick(kStartMs + 1240), CompanionOrder::Kind::kSkill));
}

TEST_CASE("IA compagnon : il ne décide qu'une fois toutes les 140 ms", "[sim][companion_ai]") {
  Scene scene(kLifJob, kOwnerX + 10);
  REQUIRE(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kReturnToOwner));
  // Le maître s'éloigne : de quoi redemander, mais pas avant la cadence.
  scene.world.Spawn(MakeActor(kOwnerId, ActorKind::kPlayer, kOwnerX - 10));
  CHECK_FALSE(scene.Tick(kStartMs + ro::sim::kCompanionAiTickMs - 1).has_value());
  CHECK(IsKind(scene.Tick(kStartMs + ro::sim::kCompanionAiTickMs),
               CompanionOrder::Kind::kReturnToOwner));
}

TEST_CASE("IA compagnon : un monstre hors de vue ne l'empêche pas de suivre",
          "[sim][companion_ai]") {
  // L'agressif voit un monstre à vingt-cinq cases : il ne s'y fige pas.
  Scene scene(kFilirJob, kOwnerX - 5);
  scene.PlaceMonster(kMonsterId, kOwnerX + 20);
  REQUIRE(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kReturnToOwner));
  CHECK(scene.brain.target_id() == 0);
}

TEST_CASE("IA compagnon : la poursuite ne redemande pas tant que la case visée tient",
          "[sim][companion_ai]") {
  Scene scene(kFilirJob, kOwnerX + 1);
  scene.PlaceMonster(kMonsterId, kOwnerX + 8);
  const auto first = scene.Tick(kStartMs);
  REQUIRE(IsKind(first, CompanionOrder::Kind::kMove));
  CHECK(first->cell == CellPosition{kOwnerX + 7, kRowY, 0});
  // Le serveur l'annonce en marche, d'abord vers une case encore hors de portée.
  REQUIRE(scene.world.StartMoving(
      kHomunculusId,
      CellMovement{CellPosition{kOwnerX + 1, kRowY, 0}, CellPosition{kOwnerX + 4, kRowY, 0}, 8, 8},
      kStartMs));
  CHECK_FALSE(scene.Tick(kStartMs + 140).has_value());
  CHECK_FALSE(scene.Tick(kStartMs + 420).has_value());
  // Le monstre s'en va : la case visée change, la poursuite suit.
  scene.PlaceMonster(kMonsterId, kOwnerX + 12);
  const auto next = scene.Tick(kStartMs + 560);
  REQUIRE(IsKind(next, CompanionOrder::Kind::kMove));
  CHECK(next->cell == CellPosition{kOwnerX + 11, kRowY, 0});
}

TEST_CASE("IA compagnon : un monstre frappé par un joueur étranger est à lui",
          "[sim][companion_ai]") {
  Scene scene(kFilirJob, kOwnerX + 1);
  scene.world.Spawn(MakeActor(kStrangerId, ActorKind::kPlayer, kOwnerX + 4));
  scene.PlaceMonster(kMonsterId, kOwnerX + 2);
  scene.Strikes(kStrangerId, kMonsterId, kStartMs - 100);
  CHECK_FALSE(scene.Tick(kStartMs).has_value());
  CHECK(scene.brain.target_id() == 0);
}

TEST_CASE("IA compagnon : une incantation sur le maître est une attaque", "[sim][companion_ai]") {
  Scene scene(kLifJob, kOwnerX + 1);
  Actor caster = MakeActor(kMonsterId, ActorKind::kMonster, kOwnerX + 6);
  caster.cast = ro::sim::Cast{
      .skill_id = 1, .target_id = kOwnerId, .started_ms = kStartMs - 100, .duration_ms = 2000};
  scene.world.Spawn(caster);
  REQUIRE(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kMove));
  CHECK(scene.brain.target_id() == kMonsterId);
  CHECK(scene.brain.last_reason() == CompanionAiReason::kDefendOwner);
}

TEST_CASE("IA compagnon : l'attaque directe garde l'amotion et le délai d'arrivée",
          "[sim][companion_ai]") {
  Scene scene(kLifJob, kOwnerX + 1);
  scene.PlaceMonster(kMonsterId, kOwnerX + 2);
  scene.brain.ForgetMap(false, kStartMs);
  scene.brain.Command(DirectOrder{.kind = DirectOrder::Kind::kAttack, .target_id = kMonsterId});
  CHECK_FALSE(scene.Tick(kStartMs).has_value());
  CHECK(scene.brain.target_id() == kMonsterId);
  const std::uint32_t open_ms = kStartMs + ro::sim::kServerCompanionAttackHoldMs + 40;
  REQUIRE(IsKind(scene.Tick(open_ms), CompanionOrder::Kind::kAttack));
  CHECK_FALSE(scene.Tick(open_ms + 140).has_value());
  CHECK(IsKind(scene.Tick(open_ms + 560), CompanionOrder::Kind::kAttack));

  Actor dead = *scene.world.Find(kMonsterId);
  dead.posture = ro::sim::Posture::kDead;
  scene.world.Spawn(dead);
  CHECK_FALSE(IsKind(scene.Tick(open_ms + 1120), CompanionOrder::Kind::kAttack));
  CHECK(scene.brain.target_id() == 0);
}

TEST_CASE("IA compagnon : une cible jamais touchée malgré ses attaques est lâchée",
          "[sim][companion_ai]") {
  constexpr std::uint32_t kStepMs = 560;
  constexpr std::uint32_t kPastBoundMs = 11000;
  Scene scene(kFilirJob, kOwnerX + 1);
  scene.PlaceMonster(kMonsterId, kOwnerX + 2);

  SECTION("cible choisie seule : lâchée, puis reprise après le délai") {
    std::uint32_t now_ms = kStartMs;
    for (; now_ms < kStartMs + kPastBoundMs; now_ms += kStepMs) {
      static_cast<void>(scene.Tick(now_ms));
    }
    CHECK(scene.brain.target_id() == 0);
    CHECK_FALSE(IsKind(scene.Tick(now_ms + kStepMs), CompanionOrder::Kind::kAttack));
    // 🔴 SEULE À PORTÉE, ELLE N'EST PAS BOUDÉE POUR TOUJOURS.
    static_cast<void>(scene.Tick(now_ms + CompanionBrain::kAbandonedRetakeMs + kStepMs));
    CHECK(scene.brain.target_id() == kMonsterId);
  }
  SECTION("ordre direct : il insiste") {
    scene.brain.Command(DirectOrder{.kind = DirectOrder::Kind::kAttack, .target_id = kMonsterId});
    std::uint32_t now_ms = kStartMs;
    for (; now_ms < kStartMs + kPastBoundMs; now_ms += kStepMs) {
      static_cast<void>(scene.Tick(now_ms));
    }
    CHECK(scene.brain.target_id() == kMonsterId);
    CHECK(IsKind(scene.Tick(now_ms + kStepMs), CompanionOrder::Kind::kAttack));
  }
}

TEST_CASE("IA compagnon : un coup du compagnon répond à ses attaques", "[sim][companion_ai]") {
  constexpr std::uint32_t kStepMs = 560;
  constexpr std::uint32_t kPastBoundMs = 11000;
  Scene scene(kFilirJob, kOwnerX + 1);
  scene.PlaceMonster(kMonsterId, kOwnerX + 2);
  std::uint32_t now_ms = kStartMs;
  for (; now_ms < kStartMs + kPastBoundMs; now_ms += kStepMs) {
    if (IsKind(scene.Tick(now_ms), CompanionOrder::Kind::kAttack)) {
      scene.Strikes(kHomunculusId, kMonsterId, now_ms + 1);
    }
  }
  CHECK(scene.brain.target_id() == kMonsterId);
}

TEST_CASE("IA compagnon : le changement de carte en plein combat oublie la cible",
          "[sim][companion_ai]") {
  Scene scene(kFilirJob, kOwnerX + 1);
  scene.PlaceMonster(kMonsterId, kOwnerX + 2);
  REQUIRE(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kAttack));
  REQUIRE(scene.brain.target_id() == kMonsterId);
  scene.brain.ForgetMap(false, kStartMs + 140);
  CHECK(scene.brain.target_id() == 0);
  CHECK(scene.brain.last_reason() == CompanionAiReason::kNone);
}

TEST_CASE("IA compagnon : en PvP, il se défend contre un joueur, et seulement là",
          "[sim][companion_ai]") {
  Scene scene(kLifJob, kOwnerX + 1);
  scene.world.Spawn(MakeActor(kStrangerId, ActorKind::kPlayer, kOwnerX + 2));
  scene.Strikes(kStrangerId, kOwnerId, kStartMs - 100);

  SECTION("hors PvP : rien") { CHECK_FALSE(scene.Tick(kStartMs).has_value()); }
  SECTION("en PvP : il défend le maître") {
    ro::sim::MapMode versus;
    versus.attackable_beyond_party = true;
    scene.world.TellMapMode(versus);
    const auto order = scene.Tick(kStartMs);
    REQUIRE(IsKind(order, CompanionOrder::Kind::kAttack));
    CHECK(order->target_id == kStrangerId);
    CHECK(scene.brain.last_reason() == CompanionAiReason::kDefendOwner);
  }
}

TEST_CASE("IA compagnon : en PvP, un joueur qui n'attaque pas n'est pas une cible",
          "[sim][companion_ai]") {
  Scene scene(kFilirJob, kOwnerX + 1);
  scene.world.Spawn(MakeActor(kStrangerId, ActorKind::kPlayer, kOwnerX + 2));
  ro::sim::MapMode versus;
  versus.attackable_beyond_party = true;
  scene.world.TellMapMode(versus);
  CHECK_FALSE(scene.Tick(kStartMs).has_value());
}

TEST_CASE("IA compagnon : l'attaque se donne en deux appuis séparés", "[sim][companion_ai]") {
  using ro::sim::AttackPress;
  ro::sim::AttackDesignation designation;
  CHECK(designation.Press(kMonsterId, false, false) == AttackPress::kDesignated);
  // Le second appui d'un double-clic ne compte pas, et ne défait rien.
  CHECK(designation.Press(kMonsterId, false, true) == AttackPress::kIgnored);
  CHECK(designation.designated_id() == kMonsterId);
  CHECK(designation.Press(kMonsterId, false, false) == AttackPress::kAttack);
  CHECK(designation.designated_id() == 0);
  // La cible attaquée reste marquée.
  CHECK(designation.marked_id() == kMonsterId);
  // Un autre acteur déplace la désignation.
  CHECK(designation.Press(kMonsterId, false, false) == AttackPress::kDesignated);
  CHECK(designation.Press(kSecondMonsterId, false, false) == AttackPress::kDesignated);
  CHECK(designation.designated_id() == kSecondMonsterId);
  // Maj : l'attaque part tout de suite en réserve, sans rien défaire.
  CHECK(designation.Press(kMonsterId, true, false) == AttackPress::kReserveAttack);
  CHECK(designation.designated_id() == kSecondMonsterId);
  CHECK(designation.marked_id() == kSecondMonsterId);
  // Un ordre de marche éteint l'une et l'autre.
  designation.Clear();
  CHECK(designation.designated_id() == 0);
  CHECK(designation.marked_id() == 0);
}

TEST_CASE("IA compagnon : les moitiés d'un long trajet respectent la file", "[sim][companion_ai]") {
  // Une file pleine de longs trajets : le premier se dépile et se coupe en
  // moitiés, qui ne la font pas déborder.
  Scene scene(kLifJob, kOwnerX + 1);
  const DirectOrder far{.kind = DirectOrder::Kind::kMove,
                        .cell = CellPosition{kOwnerX + 400, kRowY, 0}};
  for (std::size_t queued = 0; queued < ro::sim::kCompanionReserveCapacity; ++queued) {
    REQUIRE(scene.brain.Reserve(far));
  }
  REQUIRE(IsKind(scene.Tick(kStartMs), CompanionOrder::Kind::kMove));
  CHECK(scene.brain.reserved_count() <= ro::sim::kCompanionReserveCapacity);
}
