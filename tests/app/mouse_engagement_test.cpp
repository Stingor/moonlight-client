// L'engagement de la souris : quand un clic d'action le lève, et chacune des
// façons dont il retombe. C'est lui qui décide si la flèche de sélection se
// montre, et rien de tout cela ne se voit hors du jeu autrement qu'ici.

#include "world/mouse_engagement.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/progress_bar.h"
#include "ro/sim/world.h"

#include "world/cast_bar_view.h"

using moonclient::AdvanceDeed;
using moonclient::CompanionSkillDisengages;
using moonclient::Disengage;
using moonclient::EndDeed;
using moonclient::EngagementFrame;
using moonclient::EnterMap;
using moonclient::MouseEngagement;
using moonclient::MouseRoute;
using moonclient::NoteAttackRearmed;
using moonclient::NoteDeed;
using moonclient::NoteEngagingClick;
using moonclient::NoteLeftPress;
using moonclient::NoteMapMode;
using moonclient::NoteRelease;
using moonclient::NoteStrike;
using moonclient::OwnBarShown;
using moonclient::PlayerDeed;
using moonclient::PointerPx;
using moonclient::Settle;
using moonclient::SiegeMap;
using moonclient::WorldMouseFrozen;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::World;

namespace {

constexpr std::uint32_t kSelf = 150001;
constexpr std::uint32_t kWolf = 2000123;
constexpr std::uint32_t kPoring = 2000456;
constexpr std::uint32_t kRogue = 150002;

[[nodiscard]] Actor Spawned(std::uint32_t id, ActorKind kind) {
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  return actor;
}

[[nodiscard]] World Peopled() {
  World world;
  world.Spawn(Spawned(kSelf, ActorKind::kPlayer));
  world.set_player_id(kSelf);
  world.Spawn(Spawned(kWolf, ActorKind::kMonster));
  world.Spawn(Spawned(kPoring, ActorKind::kMonster));
  world.Spawn(Spawned(kRogue, ActorKind::kPlayer));
  return world;
}

// La souris au repos, au même point qu'à l'image d'avant.
[[nodiscard]] EngagementFrame Still(bool anything_under) {
  EngagementFrame frame;
  frame.anything_under = anything_under;
  return frame;
}

// Un clic d'action tel que le jeu le traite : appui, engagement, bouton tenu.
void ClickToAct(MouseEngagement& engagement, std::uint32_t id, bool continuous) {
  NoteLeftPress(engagement, /*second_of_double=*/false, MouseRoute{});
  NoteEngagingClick(engagement, id, continuous);
}

}  // namespace

TEST_CASE("Engagement : un clic d'action le lève sur son entité", "[app][fleche]") {
  MouseEngagement engagement;
  ClickToAct(engagement, kWolf, /*continuous=*/true);
  CHECK(engagement.engaged);
  CHECK(engagement.continuous);
  CHECK(engagement.clicked_id == kWolf);
}

TEST_CASE("Engagement continu : il tient jusqu'au prochain appui", "[app][fleche]") {
  const World world = Peopled();
  MouseEngagement engagement;
  ClickToAct(engagement, kWolf, /*continuous=*/true);
  NoteRelease(engagement);
  // Relâché dans le vide, il tient.
  Settle(engagement, Still(false), world);
  CHECK(engagement.engaged);

  // Le second appui d'un double-clic ne dégage pas.
  NoteLeftPress(engagement, /*second_of_double=*/true, MouseRoute{});
  CHECK(engagement.engaged);
  // Une souris arrêtée — mort, dialogue, coffre, sort au sol armé — non plus.
  NoteLeftPress(engagement, /*second_of_double=*/false, MouseRoute{.frozen = true});
  CHECK(engagement.engaged);
  NoteLeftPress(engagement, /*second_of_double=*/false, MouseRoute{.suspended = true});
  CHECK(engagement.engaged);

  // Un appui frais, n'importe où — une fenêtre comprise —, dégage d'abord ; le
  // clic peut ré-engager aussitôt sur sa propre cible.
  ClickToAct(engagement, kPoring, /*continuous=*/true);
  CHECK(engagement.engaged);
  CHECK(engagement.clicked_id == kPoring);
  NoteLeftPress(engagement, /*second_of_double=*/false, MouseRoute{});
  CHECK_FALSE(engagement.engaged);
  // La dernière entité cliquée reste : un nouvel engagement la remplacera.
  CHECK(engagement.clicked_id == kPoring);
}

TEST_CASE("Engagement non continu : il tombe quand la souris ne désigne plus rien",
          "[app][fleche]") {
  const World world = Peopled();
  MouseEngagement engagement;
  ClickToAct(engagement, kWolf, /*continuous=*/false);
  // L'image de l'appui : le monstre est sous la souris.
  Settle(engagement, Still(true), world);

  // Bouton tenu, rien ne bouge, même dans le vide.
  Settle(engagement, Still(false), world);
  CHECK(engagement.engaged);

  // Relâché au-dessus de quelque chose, il tient.
  NoteRelease(engagement);
  Settle(engagement, Still(true), world);
  CHECK(engagement.engaged);

  // Puis ce qui était sous le curseur s'en va, et la souris n'a pas bougé : il
  // tombe à l'image même.
  Settle(engagement, Still(false), world);
  CHECK_FALSE(engagement.engaged);
}

TEST_CASE("Engagement non continu : relâché dans le vide, il tombe à l'image du relâchement",
          "[app][fleche]") {
  const World world = Peopled();
  MouseEngagement engagement;
  ClickToAct(engagement, kWolf, /*continuous=*/false);
  NoteRelease(engagement);
  Settle(engagement, Still(false), world);
  CHECK_FALSE(engagement.engaged);
}

TEST_CASE("Engagement : un joueur survolé le retient jusqu'au premier mouvement", "[app][fleche]") {
  const World world = Peopled();
  MouseEngagement engagement;
  ClickToAct(engagement, kWolf, /*continuous=*/false);
  NoteRelease(engagement);

  EngagementFrame over_rogue = Still(true);
  over_rogue.player_under = kRogue;
  over_rogue.player_attackable = true;
  Settle(engagement, over_rogue, world);
  CHECK(engagement.hovered_player_id == kRogue);

  // Le joueur s'éloigne du curseur immobile : l'engagement tient.
  Settle(engagement, Still(false), world);
  CHECK(engagement.engaged);

  // Au premier mouvement, il tombe — dans la même image.
  EngagementFrame moved = Still(false);
  moved.mouse = PointerPx{10.0F, 0.0F};
  Settle(engagement, moved, world);
  CHECK_FALSE(engagement.engaged);
  CHECK(engagement.hovered_player_id == 0);
}

TEST_CASE("Engagement : un joueur qu'on ne peut pas frapper ne se retient pas une fois engagé",
          "[app][fleche]") {
  const World world = Peopled();
  MouseEngagement engagement;
  ClickToAct(engagement, kWolf, /*continuous=*/false);
  NoteRelease(engagement);
  EngagementFrame over_rogue = Still(true);
  over_rogue.player_under = kRogue;
  Settle(engagement, over_rogue, world);
  CHECK(engagement.hovered_player_id == 0);
}

TEST_CASE("Engagement : une carte le fait tomber, la même carte le garde", "[app][fleche]") {
  MouseEngagement engagement;
  ClickToAct(engagement, kWolf, /*continuous=*/true);
  EnterMap(engagement, /*same_map=*/true);
  CHECK(engagement.engaged);
  CHECK(engagement.continuous);
  CHECK(engagement.clicked_id == 0);
  CHECK_FALSE(engagement.left_held);

  EnterMap(engagement, /*same_map=*/false);
  CHECK_FALSE(engagement.engaged);
  CHECK_FALSE(engagement.continuous);
}

TEST_CASE("Engagement : la mort de la cible le fait tomber, pas son corps", "[app][fleche]") {
  // 🔴 LA FLÈCHE S'ÉTEINT À LA MORT, et non quand le cadavre s'efface : le
  // paquet de mort efface la cible de l'action, et l'attaque s'achève au tic
  // suivant alors même que le corps reste affiché.
  World world = Peopled();
  MouseEngagement engagement;
  ClickToAct(engagement, kWolf, /*continuous=*/true);
  NoteDeed(engagement, PlayerDeed::kAttack, kWolf, /*stops_on_release=*/false);
  NoteRelease(engagement);
  NoteStrike(engagement, kWolf);
  AdvanceDeed(engagement, world);
  CHECK(engagement.engaged);

  REQUIRE(world.SetPosture(kWolf, ro::sim::Posture::kDead, 0));
  AdvanceDeed(engagement, world);
  CHECK_FALSE(engagement.engaged);
  CHECK(engagement.action.deed == PlayerDeed::kNothing);
}

TEST_CASE("Engagement : une cible déjà morte fait attendre sans fin", "[app][fleche]") {
  World world = Peopled();
  REQUIRE(world.SetPosture(kWolf, ro::sim::Posture::kDead, 0));
  MouseEngagement engagement;
  ClickToAct(engagement, kWolf, /*continuous=*/true);
  NoteDeed(engagement, PlayerDeed::kSkillOnTarget, kWolf, /*stops_on_release=*/false);
  AdvanceDeed(engagement, world);
  CHECK(engagement.engaged);
  CHECK(engagement.action.deed == PlayerDeed::kSkillOnTarget);

  // Partie de la vue, l'action s'achève.
  REQUIRE(world.Remove(kWolf));
  AdvanceDeed(engagement, world);
  CHECK_FALSE(engagement.engaged);
}

TEST_CASE("Engagement : une compétence sur soi ne cherche pas sa cible", "[app][fleche]") {
  const World world = Peopled();
  MouseEngagement engagement;
  ClickToAct(engagement, kSelf, /*continuous=*/true);
  NoteDeed(engagement, PlayerDeed::kSkillOnTarget, kSelf, /*stops_on_release=*/false);
  AdvanceDeed(engagement, world);
  CHECK(engagement.action.deed == PlayerDeed::kSkillOnTarget);
}

TEST_CASE("Engagement : le serveur réarme l'attaque sans relever la flèche", "[app][fleche]") {
  MouseEngagement engagement;
  ClickToAct(engagement, kWolf, /*continuous=*/false);
  NoteDeed(engagement, PlayerDeed::kAttack, kWolf, /*stops_on_release=*/true);
  NoteRelease(engagement);
  // Sur la même cible, le relâchement reste acquis ; seul le coup est à refaire.
  NoteAttackRearmed(engagement, kWolf, /*stops_on_release=*/false);
  CHECK(engagement.action.released);
  CHECK(engagement.action.stops_on_release);
  CHECK_FALSE(engagement.action.struck);

  // Après une retombée, il recrée l'attaque, et l'engagement reste retombé.
  EndDeed(engagement);
  NoteAttackRearmed(engagement, kPoring, /*stops_on_release=*/false);
  CHECK(engagement.action.deed == PlayerDeed::kAttack);
  CHECK(engagement.action.target_id == kPoring);
  CHECK_FALSE(engagement.engaged);
}

TEST_CASE("Engagement : une attaque qui ne se poursuit pas s'arrête au relâchement",
          "[app][fleche]") {
  MouseEngagement engagement;
  // Déjà frappé : le relâchement l'achève.
  ClickToAct(engagement, kWolf, /*continuous=*/false);
  NoteDeed(engagement, PlayerDeed::kAttack, kWolf, /*stops_on_release=*/true);
  NoteStrike(engagement, kWolf);
  CHECK(engagement.engaged);
  NoteRelease(engagement);
  CHECK_FALSE(engagement.engaged);

  // Relâché en chemin : elle porte un dernier coup en arrivant, et s'achève.
  ClickToAct(engagement, kWolf, /*continuous=*/false);
  NoteDeed(engagement, PlayerDeed::kAttack, kWolf, /*stops_on_release=*/true);
  NoteRelease(engagement);
  CHECK(engagement.engaged);
  NoteStrike(engagement, kWolf);
  CHECK_FALSE(engagement.engaged);
}

TEST_CASE("Engagement : une compétence visée retombe au relâchement", "[app][fleche]") {
  // La flèche tombe, mais la compétence reste l'action en cours : elle part à
  // portée.
  MouseEngagement engagement;
  ClickToAct(engagement, kWolf, /*continuous=*/true);
  NoteDeed(engagement, PlayerDeed::kSkillOnTarget, kWolf, /*stops_on_release=*/false);
  NoteRelease(engagement);
  CHECK_FALSE(engagement.engaged);
  CHECK(engagement.action.deed == PlayerDeed::kSkillOnTarget);
}

TEST_CASE("Engagement : se désengager n'oublie pas la dernière entité cliquée", "[app][fleche]") {
  MouseEngagement engagement;
  ClickToAct(engagement, kWolf, /*continuous=*/true);
  engagement.hovered_player_id = kRogue;
  Disengage(engagement);
  CHECK_FALSE(engagement.engaged);
  CHECK(engagement.hovered_player_id == 0);
  CHECK(engagement.clicked_id == kWolf);
  EndDeed(engagement);
  CHECK(engagement.clicked_id == kWolf);
}

TEST_CASE("Engagement : le siège annoncé tient jusqu'à une autre carte", "[app][fleche]") {
  // 🔴 LE CLIENT NE LÈVE PAS LE SIÈGE quand la carte cesse de l'annoncer — une
  // fin de guerre en cours de carte, par exemple. Seule une autre carte le fait.
  MouseEngagement engagement;
  ro::sim::MapMode siege;
  siege.guild_emblems_shown = true;
  CHECK(SiegeMap(engagement, siege));
  NoteMapMode(engagement, siege);
  const ro::sim::MapMode lifted;
  CHECK(SiegeMap(engagement, lifted));
  EnterMap(engagement, /*same_map=*/true);
  CHECK(SiegeMap(engagement, lifted));
  EnterMap(engagement, /*same_map=*/false);
  CHECK_FALSE(SiegeMap(engagement, lifted));
}

TEST_CASE("Engagement non continu : un appui qui ne désigne rien le fait tomber", "[app][fleche]") {
  // Seul le bouton TENU retient l'engagement : l'appui lui-même ne le protège
  // pas, et un clic sur une fenêtre au-dessus du vide le fait tomber.
  const World world = Peopled();
  MouseEngagement engagement;
  ClickToAct(engagement, kWolf, /*continuous=*/false);
  NoteRelease(engagement);
  Settle(engagement, Still(true), world);
  REQUIRE(engagement.engaged);
  NoteLeftPress(engagement, /*second_of_double=*/false, MouseRoute{});
  Settle(engagement, Still(false), world);
  CHECK_FALSE(engagement.engaged);
}

TEST_CASE("Engagement : le gel de la mort tombe avec une carte", "[app][fleche]") {
  MouseEngagement engagement;
  engagement.death_freeze = true;
  EnterMap(engagement, /*same_map=*/true);
  CHECK_FALSE(engagement.death_freeze);
}

TEST_CASE("Gel : la barre de progression du joueur garde la souris vivante sous un dialogue",
          "[app][fleche]") {
  constexpr std::uint32_t kKafra = 110012;
  constexpr std::uint32_t kStartMs = 10000;
  constexpr std::uint32_t kThreeSeconds = 3;
  constexpr std::uint32_t kThreeSecondsMs = 3000;
  World world = Peopled();
  world.SayNpcDialog(kKafra, "Un instant...", kStartMs);
  const MouseEngagement engagement;
  REQUIRE(WorldMouseFrozen(engagement, world, OwnBarShown(world, kStartMs)));

  ro::sim::ProgressReport report;
  REQUIRE(ro::sim::StartOwnProgress(world, report, kThreeSeconds, kStartMs));
  CHECK_FALSE(WorldMouseFrozen(engagement, world, OwnBarShown(world, kStartMs)));
  CHECK_FALSE(
      WorldMouseFrozen(engagement, world, OwnBarShown(world, kStartMs + kThreeSecondsMs - 1)));
  // Échue, la barre rend le dialogue à son gel.
  CHECK(WorldMouseFrozen(engagement, world, OwnBarShown(world, kStartMs + kThreeSecondsMs)));

  // La barre au-dessus d'un PNJ n'est pas celle du joueur : elle ne dégèle rien.
  World other = Peopled();
  other.Spawn(Spawned(kKafra, ActorKind::kNonPlayer));
  other.SayNpcDialog(kKafra, "Un instant...", kStartMs);
  REQUIRE(ro::sim::StartActorProgress(other, kKafra, kThreeSeconds, kStartMs));
  CHECK(WorldMouseFrozen(engagement, other, OwnBarShown(other, kStartMs)));
}

TEST_CASE("Gel : la mort et le coffre ne cèdent pas à une barre", "[app][fleche]") {
  World world = Peopled();
  MouseEngagement engagement;
  engagement.death_freeze = true;
  CHECK(WorldMouseFrozen(engagement, world, /*own_bar_shown=*/true));
  engagement.death_freeze = false;
  world.OpenStorage();
  CHECK(WorldMouseFrozen(engagement, world, /*own_bar_shown=*/true));
}

TEST_CASE("Engagement : le skill d'une IA de compagnon le fait tomber",
          "[app][fleche][compagnon]") {
  World world = Peopled();
  constexpr std::uint32_t kOnGround = 0;
  constexpr std::uint32_t kOutOfSight = 2000999;
  CHECK(CompanionSkillDisengages(kOnGround, world));
  CHECK(CompanionSkillDisengages(kSelf, world));
  CHECK(CompanionSkillDisengages(kWolf, world));
  CHECK(CompanionSkillDisengages(kOutOfSight, world));

  // Une cible cachée ne reçoit pas le skill : rien ne retombe.
  for (const std::uint32_t option :
       {ro::sim::kOptionHide, ro::sim::kOptionCloak, ro::sim::kOptionInvisible}) {
    Actor hidden = Spawned(kRogue, ActorKind::kPlayer);
    hidden.option = option;
    world.Spawn(hidden);
    CHECK_FALSE(CompanionSkillDisengages(kRogue, world));
  }
}

TEST_CASE("Engagement : la retombée d'un skill de compagnon laisse l'action du personnage",
          "[app][fleche][compagnon]") {
  // Le skill de l'IA abaisse l'engagement, et rien de plus : l'attaque du
  // personnage continue, et la dernière entité cliquée reste.
  MouseEngagement engagement;
  ClickToAct(engagement, kWolf, /*continuous=*/true);
  NoteDeed(engagement, PlayerDeed::kAttack, kWolf, /*stops_on_release=*/false);
  Disengage(engagement);
  CHECK_FALSE(engagement.engaged);
  CHECK(engagement.clicked_id == kWolf);
  CHECK(engagement.action.deed == PlayerDeed::kAttack);
  CHECK(engagement.action.target_id == kWolf);
}
