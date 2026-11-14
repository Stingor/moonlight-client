// LES CONTEXTES ET LES CONDUITES ENVERS LES JOUEURS de l'IA des compagnons :
// le contexte d'une carte et son seul basculement par entrée, les lignes de
// personnage, de classe et génériques, les règles fixes, et rien hors PvP et
// WoE.

#include "ro/sim/companion_players.h"

#include <cstdint>
#include <optional>
#include <string>
#include <utility>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "ro/sim/companion_ai.h"
#include "ro/sim/map_mode.h"

#include "companion_rule_scene.h"

using companion_rule_scene::IsKind;
using companion_rule_scene::kFirstMonsterId;
using companion_rule_scene::kOwnerId;
using companion_rule_scene::kOwnerX;
using companion_rule_scene::kStartMs;
using companion_rule_scene::kTickMs;
using companion_rule_scene::MakeActor;
using companion_rule_scene::Scene;
using ro::sim::CompanionAiContext;
using ro::sim::CompanionOrder;
using ro::sim::CompanionPlayerConduct;
using ro::sim::MapKind;
using ro::sim::MapMode;
using ro::sim::PlayerConductKey;
using ro::sim::PlayerConductKeyKind;
using ro::sim::TacticRank;
using ro::sim::TacticTrigger;

namespace {

constexpr std::uint32_t kRivalId = 500;
constexpr std::uint16_t kPriestJob = 8;
constexpr std::uint16_t kKnightJob = 7;
constexpr std::uint32_t kOwnerGuild = 77;
constexpr std::uint32_t kOtherGuild = 78;

[[nodiscard]] MapMode Pvp() {
  MapMode mode;
  mode.kind = MapKind::kFreePvpZone;
  mode.attackable_beyond_party = true;
  mode.pvp_counter_shown = true;
  return mode;
}

[[nodiscard]] MapMode Woe() {
  MapMode mode;
  mode.kind = MapKind::kAgitZone;
  mode.attackable_beyond_guild = true;
  return mode;
}

// UN DUEL, tel que le serveur le déclare au duelliste.
[[nodiscard]] MapMode Duel() {
  MapMode mode;
  mode.kind = MapKind::kFreePvpZone;
  mode.attackable_beyond_party = true;
  mode.attackable_without_shift = true;
  return mode;
}

// UN DONJON GVG : la zone PvP libre, au bit « au-delà de la guilde ».
[[nodiscard]] MapMode GvgDungeon() {
  MapMode mode;
  mode.kind = MapKind::kFreePvpZone;
  mode.attackable_beyond_guild = true;
  mode.guild_emblems_shown = true;
  return mode;
}

[[nodiscard]] MapMode Battlefield() {
  MapMode mode;
  mode.kind = MapKind::kFreePvpZone;
  mode.attackable_beyond_guild = true;
  mode.battlefield = true;
  return mode;
}

// UN PRÊTRE NOMMÉ « Rival » au contact de l'homoncule.
void PlaceRival(Scene& scene, std::uint32_t guild_id = kOtherGuild) {
  ro::sim::Actor rival = MakeActor(kRivalId, ro::sim::ActorKind::kPlayer, kOwnerX + 2, kPriestJob);
  rival.name = "Rival";
  rival.guild_id = guild_id;
  scene.world.Spawn(rival);
}

[[nodiscard]] CompanionPlayerConduct Line(PlayerConductKey key, TacticRank rank,
                                          TacticTrigger trigger = TacticTrigger::kAlways) {
  return CompanionPlayerConduct{.key = std::move(key), .rank = rank, .trigger = trigger};
}

// L'HOMONCULE ATTAQUE-T-IL LE RIVAL de lui-même à cet instant ?
[[nodiscard]] bool AttacksRival(Scene& scene, std::uint32_t now_ms) {
  const std::optional<CompanionOrder> order = scene.Tick(now_ms);
  return IsKind(order, CompanionOrder::Kind::kAttack) && order->target_id == kRivalId;
}

}  // namespace

TEST_CASE("Conduites : le contexte d'une carte", "[sim][companion_players]") {
  CHECK(ro::sim::ContextOfMap(MapMode{}) == CompanionAiContext::kPvE);
  CHECK(ro::sim::ContextOfMap(Pvp()) == CompanionAiContext::kPvP);
  CHECK(ro::sim::ContextOfMap(Woe()) == CompanionAiContext::kWoE);
  // LE CHAMP DE BATAILLE EST DU PVP, même déclaré en zone de siège.
  MapMode battlefield = Woe();
  battlefield.battlefield = true;
  CHECK(ro::sim::ContextOfMap(battlefield) == CompanionAiContext::kPvP);
  // UN DUEL EST DU PVE : il n'ouvre que la défense.
  CHECK(ro::sim::ContextOfMap(Duel()) == CompanionAiContext::kPvE);
  CHECK(ro::sim::PlayerTargetingOf(Duel()) == ro::sim::PlayerTargeting::kDefenceOnly);
  CHECK(ro::sim::PlayerTargetingOf(battlefield) == ro::sim::PlayerTargeting::kByCamp);
  CHECK(ro::sim::PlayerTargetingOf(Pvp()) == ro::sim::PlayerTargeting::kFull);
  CHECK(ro::sim::PlayerTargetingOf(MapMode{}) == ro::sim::PlayerTargeting::kNone);
  // UNE VRAIE CARTE PVP lève le compteur : un duel qui s'y joue reste du PvP.
  MapMode duel_on_pvp = Duel();
  duel_on_pvp.pvp_counter_shown = true;
  CHECK(ro::sim::ContextOfMap(duel_on_pvp) == CompanionAiContext::kPvP);
  // LA GUILDE DU MAÎTRE : épargnée en siège et en donjon GvG, pas en champ de
  // bataille.
  CHECK(ro::sim::OwnerGuildSpared(Woe()));
  CHECK(ro::sim::OwnerGuildSpared(GvgDungeon()));
  CHECK_FALSE(ro::sim::OwnerGuildSpared(battlefield));
  CHECK_FALSE(ro::sim::OwnerGuildSpared(Pvp()));
}

TEST_CASE("Conduites : le délai part de l'arrivée, même après un long chargement",
          "[sim][companion_players]") {
  constexpr std::uint32_t kLoadMs = 5000;
  ro::sim::CompanionContextWatch watch;
  std::uint32_t now = kStartMs;
  REQUIRE(watch.Observe(1, Pvp(), now, now) == CompanionAiContext::kPvP);
  // L'ENTRÉE EST VUE AVANT LE CHARGEMENT : l'arrivée, datée après, ne fait rien
  // échoir, ni à l'image du chargement, ni à la suivante.
  const std::uint32_t arrived = now + kLoadMs;
  CHECK(watch.Observe(2, MapMode{}, arrived, now) == CompanionAiContext::kPvP);
  CHECK(watch.Observe(2, MapMode{}, arrived, arrived + kTickMs) == CompanionAiContext::kPvP);
  // Le serveur parle : le contexte n'a jamais basculé.
  CHECK(watch.Observe(2, Pvp(), arrived, arrived + 2 * kTickMs) == CompanionAiContext::kPvP);
}

TEST_CASE("Conduites : un seul basculement par entrée de carte", "[sim][companion_players]") {
  ro::sim::CompanionContextWatch watch;
  int switches = 0;
  CompanionAiContext seen = watch.context();
  std::uint32_t seen_entries = 0;
  std::uint32_t arrived = 0;
  const auto observe = [&](std::uint32_t entries, const MapMode& mode, std::uint32_t now_ms) {
    if (entries != seen_entries) {
      seen_entries = entries;
      arrived = now_ms;
    }
    const CompanionAiContext context = watch.Observe(entries, mode, arrived, now_ms);
    switches += context != seen ? 1 : 0;
    seen = context;
  };
  std::uint32_t now = kStartMs;
  observe(1, MapMode{}, now);
  observe(1, Pvp(), now += kTickMs);
  REQUIRE(seen == CompanionAiContext::kPvP);
  REQUIRE(switches == 1);
  // D'UNE CARTE PVP À UNE AUTRE : le mode, remis à neuf, reste neutre le temps
  // que le serveur le redise. Le contexte ne bouge pas.
  observe(2, MapMode{}, now += kTickMs);
  observe(2, MapMode{}, now += kTickMs);
  observe(2, Pvp(), now += kTickMs);
  CHECK(switches == 1);
  // D'UNE CARTE PVP À UNE VILLE : un seul basculement, quand le délai passe.
  observe(3, MapMode{}, now += kTickMs);
  CHECK(seen == CompanionAiContext::kPvP);
  observe(3, MapMode{}, now += ro::sim::kCompanionContextSettleMs);
  CHECK(seen == CompanionAiContext::kPvE);
  CHECK(switches == 2);
  // UNE GUERRE QUI COMMENCE SUR LA CARTE : le mode change sans entrée.
  observe(3, Woe(), now += kTickMs);
  CHECK(seen == CompanionAiContext::kWoE);
  CHECK(switches == 3);
}

TEST_CASE("Conduites : la ligne d'une classe en PvP, et rien hors PvP",
          "[sim][companion_players]") {
  Scene scene;
  PlaceRival(scene);
  REQUIRE(scene.settings.player_conducts.Set(Line(
      PlayerConductKey{.kind = PlayerConductKeyKind::kJob, .job = kPriestJob}, TacticRank::kHigh)));
  std::uint32_t now = kStartMs;
  SECTION("hors PvP : jamais un joueur") { CHECK_FALSE(AttacksRival(scene, now)); }
  SECTION("en PvP : la ligne « toujours » le prend") {
    scene.world.TellMapMode(Pvp());
    CHECK(AttacksRival(scene, now));
    CHECK(scene.brain.last_reason() == ro::sim::CompanionAiReason::kOwnInitiative);
  }
  SECTION("en PvP, une autre classe : rien") {
    scene.settings.player_conducts = ro::sim::CompanionPlayerConducts();
    REQUIRE(scene.settings.player_conducts.Set(
        Line(PlayerConductKey{.kind = PlayerConductKeyKind::kJob, .job = kKnightJob},
             TacticRank::kHigh)));
    scene.world.TellMapMode(Pvp());
    CHECK_FALSE(AttacksRival(scene, now));
  }
}

TEST_CASE("Conduites : le nom, la guilde et « tout autre joueur »", "[sim][companion_players]") {
  Scene scene;
  scene.world.TellMapMode(Pvp());
  scene.facts.owner_guild_id = kOwnerGuild;
  ro::sim::CompanionPlayerConducts& conducts = scene.settings.player_conducts;
  REQUIRE(conducts.Set(
      Line(PlayerConductKey{.kind = PlayerConductKeyKind::kAnyPlayer}, TacticRank::kMedium)));
  REQUIRE(conducts.Set(Line(PlayerConductKey{.kind = PlayerConductKeyKind::kJob, .job = kPriestJob},
                            TacticRank::kIgnore)));

  SECTION("la classe passe avant « tout autre joueur »") {
    PlaceRival(scene);
    CHECK_FALSE(AttacksRival(scene, kStartMs));
  }
  SECTION("le nom passe avant la classe") {
    PlaceRival(scene);
    REQUIRE(
        conducts.Set(Line(PlayerConductKey{.kind = PlayerConductKeyKind::kPlayer, .name = "Rival"},
                          TacticRank::kHigh)));
    CHECK(AttacksRival(scene, kStartMs));
  }
  SECTION("la guilde du maître passe avant la classe, en PvP") {
    PlaceRival(scene, kOwnerGuild);
    REQUIRE(conducts.Set(
        Line(PlayerConductKey{.kind = PlayerConductKeyKind::kOwnerGuild}, TacticRank::kHigh)));
    CHECK(AttacksRival(scene, kStartMs));
  }
  SECTION("« Ignorer » ne riposte même pas") {
    PlaceRival(scene);
    scene.Strikes(kRivalId, kOwnerId, kStartMs - kTickMs);
    CHECK_FALSE(AttacksRival(scene, kStartMs));
  }
}

TEST_CASE("Conduites : une classe montée suit la ligne de sa classe à pied",
          "[sim][companion_players]") {
  constexpr std::uint16_t kPecoKnightJob = 13;
  constexpr std::uint16_t kPecoLordKnightJob = 4014;
  constexpr std::uint16_t kLordKnightJob = 4008;
  CHECK(ro::sim::UnmountedJob(kPecoKnightJob) == kKnightJob);
  CHECK(ro::sim::UnmountedJob(kPecoLordKnightJob) == kLordKnightJob);
  CHECK(ro::sim::UnmountedJob(kPriestJob) == kPriestJob);
  // LA LISTE DES CLASSES ne propose ni variante montée ni costume.
  CHECK(ro::sim::IsConductJob(kKnightJob));
  CHECK_FALSE(ro::sim::IsConductJob(kPecoKnightJob));
  CHECK_FALSE(ro::sim::IsConductJob(kPecoLordKnightJob));

  ro::sim::CompanionPlayerConducts conducts;
  REQUIRE(conducts.Set(Line(PlayerConductKey{.kind = PlayerConductKeyKind::kJob, .job = kKnightJob},
                            TacticRank::kIgnore)));
  const ro::sim::Actor mounted =
      MakeActor(kRivalId, ro::sim::ActorKind::kPlayer, kOwnerX + 2, kPecoKnightJob);
  const CompanionPlayerConduct* line = ro::sim::ConductFor(conducts, mounted, false);
  REQUIRE(line != nullptr);
  CHECK(line->rank == TacticRank::kIgnore);
}

TEST_CASE("Conduites : les règles fixes ne se règlent pas", "[sim][companion_players]") {
  Scene scene;
  scene.facts.owner_guild_id = kOwnerGuild;
  REQUIRE(scene.settings.player_conducts.Set(
      Line(PlayerConductKey{.kind = PlayerConductKeyKind::kAnyPlayer}, TacticRank::kAbsolute)));
  REQUIRE(scene.settings.player_conducts.Set(
      Line(PlayerConductKey{.kind = PlayerConductKeyKind::kOwnerGuild}, TacticRank::kAbsolute)));
  scene.world.TellMapMode(Pvp());

  SECTION("un membre du groupe du maître") {
    PlaceRival(scene);
    scene.facts.party_member_ids = {kRivalId};
    CHECK_FALSE(AttacksRival(scene, kStartMs));
  }
  SECTION("un ami, même s'il frappe le maître") {
    PlaceRival(scene);
    scene.settings.friends = {"Rival"};
    scene.Strikes(kRivalId, kOwnerId, kStartMs - kTickMs);
    CHECK_FALSE(AttacksRival(scene, kStartMs));
  }
  SECTION("en WoE, la guilde du maître") {
    PlaceRival(scene, kOwnerGuild);
    scene.world.TellMapMode(Woe());
    CHECK_FALSE(AttacksRival(scene, kStartMs));
  }
  SECTION("en WoE, une autre guilde reste une cible") {
    PlaceRival(scene, kOtherGuild);
    scene.world.TellMapMode(Woe());
    CHECK(AttacksRival(scene, kStartMs));
  }
  SECTION("un joueur caché n'est jamais visé") {
    PlaceRival(scene);
    ro::sim::Actor hidden = *scene.world.Find(kRivalId);
    hidden.option |= ro::sim::kOptionHide;
    scene.world.Spawn(hidden);
    CHECK_FALSE(AttacksRival(scene, kStartMs));
  }
}

TEST_CASE("Conduites : une cible joueur se lâche quand la carte n'est plus PvP",
          "[sim][companion_players]") {
  Scene scene;
  PlaceRival(scene);
  REQUIRE(scene.settings.player_conducts.Set(
      Line(PlayerConductKey{.kind = PlayerConductKeyKind::kAnyPlayer}, TacticRank::kHigh)));
  scene.world.TellMapMode(Pvp());
  std::uint32_t now = kStartMs;
  REQUIRE(AttacksRival(scene, now));
  scene.world.TellMapMode(MapMode{});
  static_cast<void>(scene.Tick(now += kTickMs));
  CHECK(scene.brain.target_id() == 0);
}

TEST_CASE("Conduites : la défense passe d'abord, un monstre qui frappe le maître l'emporte",
          "[sim][companion_players]") {
  Scene scene;
  PlaceRival(scene);
  scene.Monster(kFirstMonsterId, kOwnerX + 2);
  REQUIRE(scene.settings.player_conducts.Set(
      Line(PlayerConductKey{.kind = PlayerConductKeyKind::kAnyPlayer}, TacticRank::kMedium)));
  scene.world.TellMapMode(Pvp());
  scene.Strikes(kFirstMonsterId, kOwnerId, kStartMs - kTickMs);
  const std::optional<CompanionOrder> order = scene.Tick(kStartMs);
  REQUIRE(IsKind(order, CompanionOrder::Kind::kAttack));
  CHECK(order->target_id == kFirstMonsterId);
}

TEST_CASE("Conduites : en duel et en champ de bataille, la défense seule",
          "[sim][companion_players]") {
  Scene scene;
  scene.facts.owner_guild_id = kOwnerGuild;
  scene.settings.assist_owner = true;
  REQUIRE(scene.settings.player_conducts.Set(
      Line(PlayerConductKey{.kind = PlayerConductKeyKind::kAnyPlayer}, TacticRank::kAbsolute)));
  const MapMode mode = GENERATE(Duel(), Battlefield());
  scene.world.TellMapMode(mode);
  SECTION("aucune conduite, aucune initiative") {
    PlaceRival(scene);
    CHECK_FALSE(AttacksRival(scene, kStartMs));
  }
  SECTION("aucune assistance") {
    PlaceRival(scene);
    scene.Strikes(kOwnerId, kRivalId, kStartMs - kTickMs);
    CHECK_FALSE(AttacksRival(scene, kStartMs));
  }
  SECTION("la défense du maître reste, contre un membre de sa guilde compris") {
    PlaceRival(scene, kOwnerGuild);
    scene.Strikes(kRivalId, kOwnerId, kStartMs - kTickMs);
    CHECK(AttacksRival(scene, kStartMs));
    CHECK(scene.brain.last_reason() == ro::sim::CompanionAiReason::kDefendOwner);
  }
}

TEST_CASE("Conduites : la visée d'un joueur, la carte précisée par son camp",
          "[sim][companion_players]") {
  using ro::sim::BattleRelation;
  using ro::sim::PlayerStance;
  using ro::sim::PlayerStanceOf;
  using ro::sim::PlayerTargeting;
  // 🔴 AU CHAMP DE BATAILLE : l'adversaire comme un monstre, le coéquipier
  // jamais, l'inconnu en défense.
  CHECK(PlayerStanceOf(PlayerTargeting::kByCamp, BattleRelation::kEnemy) ==
        PlayerStance::kAsMonster);
  CHECK(PlayerStanceOf(PlayerTargeting::kByCamp, BattleRelation::kAlly) == PlayerStance::kNone);
  CHECK(PlayerStanceOf(PlayerTargeting::kByCamp, BattleRelation::kUnknown) ==
        PlayerStance::kDefenceOnly);
  // Ailleurs, le camp ne dit rien : c'est la carte qui décide.
  for (const BattleRelation relation :
       {BattleRelation::kEnemy, BattleRelation::kAlly, BattleRelation::kUnknown}) {
    CHECK(PlayerStanceOf(PlayerTargeting::kNone, relation) == PlayerStance::kNone);
    CHECK(PlayerStanceOf(PlayerTargeting::kDefenceOnly, relation) == PlayerStance::kDefenceOnly);
    CHECK(PlayerStanceOf(PlayerTargeting::kFull, relation) == PlayerStance::kConducts);
  }
}

TEST_CASE("Conduites : au champ de bataille, l'adversaire se prend comme un monstre",
          "[sim][companion_players]") {
  constexpr std::uint16_t kOwnCamp = 3;
  constexpr std::uint16_t kOtherCamp = 4;
  Scene scene;
  scene.settings.attack_on_own = true;
  scene.world.TellMapMode(Battlefield());
  scene.world.battlefield().TellCamp(kOwnerId, kOwnCamp, kOwnerId);
  PlaceRival(scene);
  SECTION("le camp adverse : l'initiative d'« attaquer de lui-même »") {
    scene.world.battlefield().TellCamp(kRivalId, kOtherCamp, kOwnerId);
    CHECK(AttacksRival(scene, kStartMs));
    CHECK(scene.brain.last_reason() == ro::sim::CompanionAiReason::kOwnInitiative);
  }
  SECTION("sans « attaquer de lui-même », pas d'initiative, comme pour un monstre") {
    scene.settings.attack_on_own = false;
    scene.world.battlefield().TellCamp(kRivalId, kOtherCamp, kOwnerId);
    CHECK_FALSE(AttacksRival(scene, kStartMs));
  }
  SECTION("une conduite « Ignorer » l'épargne, comme la tactique d'un monstre") {
    REQUIRE(scene.settings.player_conducts.Set(
        Line(PlayerConductKey{.kind = PlayerConductKeyKind::kAnyPlayer}, TacticRank::kIgnore)));
    scene.world.battlefield().TellCamp(kRivalId, kOtherCamp, kOwnerId);
    CHECK_FALSE(AttacksRival(scene, kStartMs));
  }
  SECTION("son propre camp : jamais, même s'il frappe") {
    scene.world.battlefield().TellCamp(kRivalId, kOwnCamp, kOwnerId);
    CHECK_FALSE(AttacksRival(scene, kStartMs));
    scene.Strikes(kRivalId, kOwnerId, kStartMs);
    CHECK_FALSE(AttacksRival(scene, kStartMs + kTickMs));
  }
  SECTION("un camp que le serveur n'a pas dit : la défense seule") {
    CHECK_FALSE(AttacksRival(scene, kStartMs));
    scene.Strikes(kRivalId, kOwnerId, kStartMs);
    CHECK(AttacksRival(scene, kStartMs + kTickMs));
    CHECK(scene.brain.last_reason() == ro::sim::CompanionAiReason::kDefendOwner);
  }
  SECTION("hors du champ, le même camp adverse n'ouvre rien") {
    scene.world.battlefield().TellCamp(kRivalId, kOtherCamp, kOwnerId);
    scene.world.TellMapMode(MapMode{});
    CHECK_FALSE(AttacksRival(scene, kStartMs));
  }
}

TEST_CASE("Conduites : en PvP, l'assistance contre un joueur est voulue",
          "[sim][companion_players]") {
  Scene scene;
  scene.settings.assist_owner = true;
  scene.world.TellMapMode(Pvp());
  PlaceRival(scene);
  scene.Strikes(kOwnerId, kRivalId, kStartMs - kTickMs);
  CHECK(AttacksRival(scene, kStartMs));
  CHECK(scene.brain.last_reason() == ro::sim::CompanionAiReason::kAssistOwner);
}

TEST_CASE("Conduites : un donjon GvG épargne la guilde du maître", "[sim][companion_players]") {
  Scene scene;
  scene.facts.owner_guild_id = kOwnerGuild;
  REQUIRE(scene.settings.player_conducts.Set(
      Line(PlayerConductKey{.kind = PlayerConductKeyKind::kAnyPlayer}, TacticRank::kAbsolute)));
  scene.world.TellMapMode(GvgDungeon());
  SECTION("sa guilde") {
    PlaceRival(scene, kOwnerGuild);
    CHECK_FALSE(AttacksRival(scene, kStartMs));
  }
  SECTION("une autre") {
    PlaceRival(scene, kOtherGuild);
    CHECK(AttacksRival(scene, kStartMs));
  }
}

TEST_CASE("Conduites : un maître sans guilde connue n'épargne pas un joueur sans guilde",
          "[sim][companion_players]") {
  Scene scene;
  scene.facts.owner_guild_id = 0;
  REQUIRE(scene.settings.player_conducts.Set(
      Line(PlayerConductKey{.kind = PlayerConductKeyKind::kAnyPlayer}, TacticRank::kAbsolute)));
  scene.world.TellMapMode(Woe());
  PlaceRival(scene, 0);
  CHECK(AttacksRival(scene, kStartMs));
}

TEST_CASE("Conduites : une cible joueur en poursuite suit la vue d'un monstre",
          "[sim][companion_players]") {
  constexpr std::uint16_t kChasedCells = 15;
  constexpr std::uint16_t kGoneCells = ro::sim::kCompanionSightCells + 5;
  Scene scene;
  REQUIRE(scene.settings.player_conducts.Set(
      Line(PlayerConductKey{.kind = PlayerConductKeyKind::kAnyPlayer}, TacticRank::kHigh)));
  scene.world.TellMapMode(Pvp());
  PlaceRival(scene);
  std::uint32_t now = kStartMs;
  REQUIRE(AttacksRival(scene, now));
  REQUIRE(scene.world.PlaceAt(
      kRivalId, ro::sim::CellPosition{kOwnerX + 1 + kChasedCells, companion_rule_scene::kRowY, 0}));
  static_cast<void>(scene.Tick(now += kTickMs));
  CHECK(scene.brain.target_id() == kRivalId);
  REQUIRE(scene.world.PlaceAt(
      kRivalId, ro::sim::CellPosition{kOwnerX + 1 + kGoneCells, companion_rule_scene::kRowY, 0}));
  static_cast<void>(scene.Tick(now += kTickMs));
  CHECK(scene.brain.target_id() == 0);
}
