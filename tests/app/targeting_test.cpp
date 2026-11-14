// Le mode Ciblage : ce qui désigne, ce qui perd la cible, et ce que le clic veut
// dire sous lui. Aucun de ces gestes ne se voit hors du jeu autrement qu'ici.

#include "world/targeting.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/world.h"

using moonclient::kAttackActionOnce;
using moonclient::kAttackActionRepeat;
using moonclient::kAttackDropped;
using moonclient::SkillAimKind;
using moonclient::TargetCandidate;
using moonclient::Targeting;
using moonclient::TargetingSettings;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::World;

namespace {

constexpr std::uint32_t kSelf = 150001;
constexpr std::uint32_t kWolf = 2000123;
constexpr std::uint32_t kPoring = 2000456;
constexpr std::uint32_t kDrops = 2000789;
constexpr std::uint32_t kPriest = 150002;
constexpr std::uint32_t kPortal = 110001;
constexpr std::uint32_t kStart = 10000;

[[nodiscard]] Actor Spawned(std::uint32_t id, ActorKind kind) {
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  actor.name = "x";
  actor.card = ro::sim::NameCard{};
  actor.position.x = 100;
  actor.position.y = 100;
  return actor;
}

[[nodiscard]] World Peopled() {
  World world;
  world.Spawn(Spawned(kSelf, ActorKind::kPlayer));
  world.set_player_id(kSelf);
  world.Spawn(Spawned(kWolf, ActorKind::kMonster));
  world.Spawn(Spawned(kPoring, ActorKind::kMonster));
  world.Spawn(Spawned(kDrops, ActorKind::kMonster));
  world.Spawn(Spawned(kPriest, ActorKind::kPlayer));
  Actor portal = Spawned(kPortal, ActorKind::kNonPlayer);
  portal.look.job = ro::sim::kPortalJob;
  world.Spawn(portal);
  return world;
}

[[nodiscard]] TargetingSettings On() {
  TargetingSettings settings;
  settings.enabled = true;
  return settings;
}

// Trois monstres à l'écran, donnés dans le désordre : le cycle trie.
[[nodiscard]] std::vector<TargetCandidate> ThreeOnScreen() {
  return {{.id = kDrops, .distance_squared = 90.0F},
          {.id = kWolf, .distance_squared = 10.0F},
          {.id = kPoring, .distance_squared = 40.0F}};
}

}  // namespace

TEST_CASE("Ciblage : éteint, le mode est un vrai maître", "[app][cible]") {
  World world = Peopled();
  const TargetingSettings settings;  // éteint d'usine
  Targeting targeting(settings);
  targeting.NoteOnScreen(ThreeOnScreen());

  CHECK_FALSE(targeting.Designate(world, kWolf));
  CHECK_FALSE(targeting.CycleTarget(world, true, kStart));
  CHECK_FALSE(targeting.TargetNearest(world, kStart));
  targeting.NoteSkillTarget(world, kWolf);
  CHECK(world.selected_id() == 0);

  // Une cible laissée par un mode qu'on vient d'éteindre ne survit pas.
  REQUIRE(world.Select(kWolf));
  CHECK_FALSE(targeting.ClearTarget(world));
  targeting.Revalidate(world);
  CHECK(world.selected_id() == 0);
}

TEST_CASE("Ciblage : le cycle part du plus proche et parcourt par distance", "[app][cible]") {
  World world = Peopled();
  const TargetingSettings settings = On();
  Targeting targeting(settings);
  targeting.NoteOnScreen(ThreeOnScreen());

  REQUIRE(targeting.CycleTarget(world, true, kStart));
  CHECK(world.selected_id() == kWolf);
  REQUIRE(targeting.CycleTarget(world, true, kStart + 100));
  CHECK(world.selected_id() == kPoring);
  REQUIRE(targeting.CycleTarget(world, true, kStart + 200));
  CHECK(world.selected_id() == kDrops);
  // Au bout, on boucle.
  REQUIRE(targeting.CycleTarget(world, true, kStart + 300));
  CHECK(world.selected_id() == kWolf);
  REQUIRE(targeting.CycleTarget(world, false, kStart + 400));
  CHECK(world.selected_id() == kDrops);
}

TEST_CASE("Ciblage : une pause remet le cycle au plus proche, quel que soit le sens",
          "[app][cible]") {
  World world = Peopled();
  const TargetingSettings settings = On();
  Targeting targeting(settings);
  targeting.NoteOnScreen(ThreeOnScreen());

  REQUIRE(targeting.CycleTarget(world, true, kStart));
  REQUIRE(targeting.CycleTarget(world, true, kStart + 100));
  CHECK(world.selected_id() == kPoring);
  // Plus d'une seconde sans cycler : on reprend au plus proche.
  REQUIRE(targeting.CycleTarget(world, false, kStart + 100 + 1001));
  CHECK(world.selected_id() == kWolf);
}

TEST_CASE("Ciblage : sans boucle, le cycle s'arrête au dernier", "[app][cible]") {
  World world = Peopled();
  TargetingSettings settings = On();
  settings.cycle_wrap = false;
  settings.cycle_reset_ms = 0;
  Targeting targeting(settings);
  targeting.NoteOnScreen(ThreeOnScreen());

  REQUIRE(targeting.CycleTarget(world, true, kStart));
  REQUIRE(targeting.CycleTarget(world, true, kStart + 5000));
  REQUIRE(targeting.CycleTarget(world, true, kStart + 90000));
  CHECK(world.selected_id() == kDrops);
  CHECK_FALSE(targeting.CycleTarget(world, true, kStart + 90100));
  CHECK(world.selected_id() == kDrops);
}

TEST_CASE("Ciblage : le plus proche, et rien quand l'écran est vide", "[app][cible]") {
  World world = Peopled();
  const TargetingSettings settings = On();
  Targeting targeting(settings);
  CHECK_FALSE(targeting.TargetNearest(world, kStart));
  targeting.NoteOnScreen(ThreeOnScreen());
  REQUIRE(targeting.TargetNearest(world, kStart));
  CHECK(world.selected_id() == kWolf);
}

TEST_CASE("Ciblage : décibler rend faux quand il n'y a rien à lâcher", "[app][cible]") {
  World world = Peopled();
  const TargetingSettings settings = On();
  Targeting targeting(settings);
  CHECK_FALSE(targeting.ClearTarget(world));
  REQUIRE(targeting.Designate(world, kWolf));
  CHECK(targeting.ClearTarget(world));
  CHECK(world.selected_id() == 0);
}

TEST_CASE("Ciblage : un sort désigne, sauf sur soi-même", "[app][cible]") {
  World world = Peopled();
  const TargetingSettings settings = On();
  Targeting targeting(settings);
  REQUIRE(targeting.Designate(world, kWolf));
  targeting.NoteSkillTarget(world, kSelf);
  CHECK(world.selected_id() == kWolf);
  targeting.NoteSkillTarget(world, kPriest);
  CHECK(world.selected_id() == kPriest);
}

TEST_CASE("Ciblage : une cible cachée ou refusée est perdue", "[app][cible]") {
  World world = Peopled();
  const TargetingSettings settings = On();
  Targeting targeting(settings);

  REQUIRE(targeting.Designate(world, kPriest));
  // Des états arrivent avant l'état : ce n'est pas un refus.
  REQUIRE(world.TellTargetStatuses(kPriest, {}));
  targeting.Revalidate(world);
  CHECK(world.selected_id() == kPriest);

  ro::sim::TargetDetail refused;
  refused.id = kPriest;
  refused.told = true;
  refused.reachable = false;
  REQUIRE(world.TellTarget(refused));
  targeting.Revalidate(world);
  CHECK(world.selected_id() == 0);

  REQUIRE(targeting.Designate(world, kWolf));
  Actor hidden = *world.Find(kWolf);
  hidden.option = ro::sim::kOptionCloak;
  world.Spawn(hidden);
  targeting.Revalidate(world);
  CHECK(world.selected_id() == 0);
}

TEST_CASE("Ciblage : le clic qui cible sans attaquer", "[app][cible]") {
  TargetingSettings settings = On();
  Targeting targeting(settings);
  // Réglage éteint : le clic engage.
  CHECK_FALSE(targeting.SuppressClickEngage(false));
  CHECK(targeting.FilterBasicAttack(kWolf, kAttackActionRepeat) == kAttackActionRepeat);

  settings.click_no_attack = true;
  CHECK(targeting.SuppressClickEngage(false));
  // Une compétence armée emprunte le même chemin, et ne se bloque pas.
  CHECK_FALSE(targeting.SuppressClickEngage(true));
  // Et le coup lui-même est jeté, quoi qui l'ait armé.
  CHECK(targeting.FilterBasicAttack(kWolf, kAttackActionRepeat) == kAttackDropped);
}

TEST_CASE("Ciblage : un ordre explicite ouvre un passage, et une dispense", "[app][cible]") {
  TargetingSettings settings = On();
  settings.click_no_attack = true;
  Targeting targeting(settings);

  // Durable : le passage se consomme une fois, la dispense tient.
  targeting.NoteExplicitAttack(kWolf, /*once=*/false);
  CHECK_FALSE(targeting.SuppressClickEngage(false));
  CHECK(targeting.FilterBasicAttack(kWolf, kAttackActionRepeat) == kAttackActionRepeat);
  CHECK(targeting.FilterBasicAttack(kWolf, kAttackActionRepeat) == kAttackActionRepeat);
  CHECK(targeting.FilterBasicAttack(kPoring, kAttackActionRepeat) == kAttackDropped);
  // Le clic ordinaire suivant referme la dispense.
  CHECK(targeting.SuppressClickEngage(false));
  CHECK(targeting.FilterBasicAttack(kWolf, kAttackActionRepeat) == kAttackDropped);

  // D'un seul coup : l'action est réécrite, et la dispense se referme sur elle.
  targeting.NoteExplicitAttack(kWolf, /*once=*/true);
  CHECK(targeting.FilterBasicAttack(kWolf, kAttackActionRepeat) == kAttackActionOnce);
  CHECK(targeting.FilterBasicAttack(kWolf, kAttackActionRepeat) == kAttackDropped);
}

TEST_CASE("Ciblage : en zone de siège, l'attaque ne se poursuit jamais", "[app][cible]") {
  CHECK(Targeting::WantsContinuousAttack(/*siege_map=*/false, true, false));
  CHECK(Targeting::WantsContinuousAttack(/*siege_map=*/false, false, true));
  CHECK_FALSE(Targeting::WantsContinuousAttack(/*siege_map=*/false, false, false));
  CHECK_FALSE(Targeting::WantsContinuousAttack(/*siege_map=*/true, true, true));
}

TEST_CASE("Ciblage : seul le second appui d'un double-clic se rejoue", "[app][cible]") {
  World world = Peopled();
  TargetingSettings settings = On();
  settings.click_no_attack = true;
  Targeting targeting(settings);
  REQUIRE(targeting.Designate(world, kWolf));

  targeting.NoteWorldPress(/*second_of_double=*/false);
  targeting.NotePressedOn(kWolf);
  CHECK(targeting.TakeDoubleClickTarget(world, false) == 0);

  targeting.NoteWorldPress(/*second_of_double=*/true);
  // Rien sous une compétence armée, et le double est consommé.
  CHECK(targeting.TakeDoubleClickTarget(world, true) == 0);
  targeting.NoteWorldPress(/*second_of_double=*/true);
  targeting.NotePressedOn(kWolf);
  CHECK(targeting.TakeDoubleClickTarget(world, false) == kWolf);
  CHECK(targeting.TakeDoubleClickTarget(world, false) == 0);
}

TEST_CASE("Ciblage : un double-clic au sol n'attaque pas la cible", "[app][cible]") {
  // 🔴 LE SECOND APPUI DOIT TOMBER SUR ELLE. Double-cliquer pour marcher
  // attaquait la cible restée désignée.
  World world = Peopled();
  TargetingSettings settings = On();
  settings.click_no_attack = true;
  Targeting targeting(settings);
  REQUIRE(targeting.Designate(world, kWolf));
  targeting.NoteWorldPress(/*second_of_double=*/true);
  targeting.NotePressedOn(0);
  CHECK(targeting.TakeDoubleClickTarget(world, false) == 0);

  // Un appui dont le clic ne dit rien — un panneau l'a pris — compte comme le sol.
  targeting.NotePressedOn(kWolf);
  targeting.NoteWorldPress(/*second_of_double=*/true);
  CHECK(targeting.TakeDoubleClickTarget(world, false) == 0);
}

TEST_CASE("Ciblage : sans le clic sans attaque, le double-clic ne rejoue rien", "[app][cible]") {
  World world = Peopled();
  const TargetingSettings settings = On();
  Targeting targeting(settings);
  REQUIRE(targeting.Designate(world, kWolf));
  targeting.NoteWorldPress(/*second_of_double=*/true);
  targeting.NotePressedOn(kWolf);
  CHECK(targeting.TakeDoubleClickTarget(world, false) == 0);
}

TEST_CASE("Ciblage : les cadres n'agissent que verrouillés, et pas sous un sort au sol",
          "[app][cible]") {
  World world = Peopled();
  TargetingSettings settings = On();
  Targeting targeting(settings);
  REQUIRE(targeting.Designate(world, kWolf));
  CHECK_FALSE(targeting.FramesAct(world, true, false));
  settings.proxy_click = true;
  CHECK(targeting.FramesAct(world, true, false));
  CHECK_FALSE(targeting.FramesAct(world, false, false));
  CHECK_FALSE(targeting.FramesAct(world, true, true));

  targeting.NoteFrameClicks(world, true, false);
  CHECK(targeting.TakeFrameClick() == kWolf);
  CHECK(targeting.TakeFrameClick() == 0);
}

TEST_CASE("Ciblage : la flèche, jamais sur un portail ni sur un caché", "[app][cible]") {
  World world = Peopled();
  TargetingSettings settings = On();
  Targeting targeting(settings);
  REQUIRE(targeting.Designate(world, kWolf));
  REQUIRE(targeting.MarkedActor(world) != nullptr);
  settings.native_marker = false;
  CHECK(targeting.MarkedActor(world) == nullptr);
  settings.native_marker = true;
  REQUIRE(targeting.Designate(world, kPortal));
  CHECK(targeting.MarkedActor(world) == nullptr);
}

TEST_CASE("Ciblage : la cible d'un sort armé au clavier", "[app][cible]") {
  World world = Peopled();
  TargetingSettings settings = On();
  Targeting targeting(settings);
  REQUIRE(targeting.Designate(world, kWolf));
  CHECK(targeting.SkillTargetId(world, SkillAimKind::kOffensive) == 0);

  settings.cast_on_target = true;
  CHECK(targeting.SkillTargetId(world, SkillAimKind::kOffensive) == kWolf);
  Actor dead = *world.Find(kWolf);
  dead.posture = ro::sim::Posture::kDead;
  world.Spawn(dead);
  REQUIRE(targeting.Designate(world, kWolf));
  // Un cadavre ne se vise pas pour l'attaquer ; un soutien, lui, peut le relever.
  CHECK(targeting.SkillTargetId(world, SkillAimKind::kOffensive) == 0);
  CHECK(targeting.SkillTargetId(world, SkillAimKind::kSupport) == kWolf);
  // Jamais soi-même.
  targeting.NoteSkillTarget(world, kSelf);
  REQUIRE(world.Select(kSelf));
  CHECK(targeting.SkillTargetId(world, SkillAimKind::kSupport) == 0);
}

TEST_CASE("Ciblage : le mode répond aux sorts seulement allumé et réglé", "[app][cible]") {
  TargetingSettings settings;
  const Targeting targeting(settings);
  CHECK_FALSE(targeting.CastsOnTarget());
  settings.cast_on_target = true;
  // Réglé mais éteint : un vrai maître ne laisse rien passer.
  CHECK_FALSE(targeting.CastsOnTarget());
  settings.enabled = true;
  CHECK(targeting.CastsOnTarget());
}

TEST_CASE("Ciblage : un butin désigné ne fait jamais demander de carte", "[app][cible]") {
  World world = Peopled();
  const TargetingSettings settings = On();
  Targeting targeting(settings);

  constexpr std::uint32_t kLoot = 150004;
  Actor loot = Spawned(kLoot, ActorKind::kItem);
  loot.card.reset();
  world.Spawn(loot);
  REQUIRE(targeting.Designate(world, kLoot));

  CHECK(targeting.Poll(world, kStart, true).name_id == 0);
  CHECK(targeting.Poll(world, kStart + 700, true).name_id == 0);
}

TEST_CASE("Ciblage : les redemandes suivent leurs cadences", "[app][cible]") {
  World world = Peopled();
  const TargetingSettings settings = On();
  Targeting targeting(settings);
  CHECK(targeting.Poll(world, kStart, true).info_id == 0);

  // Un joueur dont la carte n'est pas encore arrivée.
  constexpr std::uint32_t kStranger = 150003;
  Actor stranger = Spawned(kStranger, ActorKind::kPlayer);
  stranger.card.reset();
  world.Spawn(stranger);
  REQUIRE(targeting.Designate(world, kStranger));

  moonclient::TargetRequests asked = targeting.Poll(world, kStart, true);
  CHECK(asked.info_id == kStranger);
  CHECK(asked.statuses_id == kStranger);
  CHECK(asked.name_id == kStranger);

  asked = targeting.Poll(world, kStart + 399, true);
  CHECK(asked.info_id == 0);
  CHECK(asked.name_id == 0);
  asked = targeting.Poll(world, kStart + 400, true);
  CHECK(asked.info_id == kStranger);
  CHECK(asked.statuses_id == 0);
  asked = targeting.Poll(world, kStart + 700, true);
  CHECK(asked.statuses_id == kStranger);
  CHECK(asked.name_id == kStranger);

  // Une carte posée ne se redemande plus.
  REQUIRE(targeting.Designate(world, kPriest));
  CHECK(targeting.Poll(world, kStart + 1000, true).name_id == 0);

  // Un refus fait taire la liste des états quinze secondes, pour cette entité.
  REQUIRE(world.RefuseTargetStatuses(kPriest));
  CHECK(targeting.Poll(world, kStart + 1400, true).statuses_id == 0);
  CHECK(targeting.Poll(world, kStart + 1400 + 14999, true).statuses_id == 0);
  CHECK(targeting.Poll(world, kStart + 1400 + 15000, true).statuses_id == kPriest);

  // Mes propres états ne se demandent jamais.
  REQUIRE(targeting.Designate(world, kSelf));
  CHECK(targeting.Poll(world, kStart + 20000, true).statuses_id == 0);
}
