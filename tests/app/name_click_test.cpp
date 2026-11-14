// Le clic sur un nom dans la fenêtre Groupe / Amis, et l'ordre de l'image qui
// décide la flèche avant que l'action en attente ne parte. Rien de cela ne se
// voit hors du jeu autrement qu'ici.

#include "world/name_click.h"

#include <cstdint>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/world.h"

#include "world/click_rules.h"
#include "world/mouse_engagement.h"
#include "world/native_marker.h"

using moonclient::AimedPlayerScene;
using moonclient::AimedSkillReachesMonster;
using moonclient::AimedSkillReachesPlayer;
using moonclient::FollowOwnAction;
using moonclient::MarkerOrder;
using moonclient::MarkerSampleMs;
using moonclient::MouseEngagement;
using moonclient::MouseRoute;
using moonclient::NameClickOrder;
using moonclient::NameClickOrderOf;
using moonclient::NameClickPkZone;
using moonclient::NameClickScene;
using moonclient::NamedAim;
using moonclient::NativeMarkerCall;
using moonclient::NoteDeed;
using moonclient::NoteEngagingClick;
using moonclient::NoteLeftPress;
using moonclient::NoteNamedClick;
using moonclient::PlayerDeed;
using moonclient::PressOnWindowDisarms;
using moonclient::SkillTargetsPlayerUnderShift;
using moonclient::SkillTargetsPlayerWithoutShift;
using ro::sim::Actor;
using ro::sim::ActorKind;
using ro::sim::World;

namespace {

constexpr std::uint16_t kHeal = 28;
constexpr std::uint16_t kDecreaseAgi = 30;
constexpr std::uint16_t kFireBolt = 19;
constexpr std::uint16_t kTalismanOfProtection = 5418;
constexpr std::uint16_t kBlessingOfMysticalCreatures = 5448;
constexpr std::uint16_t kMapTypeField = 2;
constexpr std::uint16_t kMapTypeFreePk = 6;
constexpr std::uint16_t kMapTypeBattlefield = 19;

constexpr std::uint32_t kSelf = 150001;
constexpr std::uint32_t kWolf = 2000123;
constexpr std::uint32_t kPriest = 150002;

[[nodiscard]] NameClickScene Armed(NamedAim aim, std::uint16_t skill_id) {
  NameClickScene scene;
  scene.aim = aim;
  scene.skill_id = skill_id;
  return scene;
}

[[nodiscard]] World Peopled() {
  World world;
  Actor self;
  self.id = kSelf;
  self.kind = ActorKind::kPlayer;
  world.Spawn(self);
  world.set_player_id(kSelf);
  Actor wolf;
  wolf.id = kWolf;
  wolf.kind = ActorKind::kMonster;
  world.Spawn(wolf);
  Actor priest;
  priest.id = kPriest;
  priest.kind = ActorKind::kPlayer;
  priest.player_sprite = true;
  world.Spawn(priest);
  return world;
}

}  // namespace

TEST_CASE("Nom : un soin se lance sans MAJ, une attaque la demande", "[app][nom]") {
  CHECK(NameClickOrderOf(Armed(NamedAim::kSupport, kHeal)) == NameClickOrder::kSkill);
  // Un sort offensif hors des deux tables n'agit sur un nom que sous MAJ.
  CHECK(NameClickOrderOf(Armed(NamedAim::kOffensive, kFireBolt)) == NameClickOrder::kNothing);
  NameClickScene shifted = Armed(NamedAim::kOffensive, kFireBolt);
  shifted.shift_held = true;
  CHECK(NameClickOrderOf(shifted) == NameClickOrder::kSkill);

  // Sans compétence, le nom n'attaque que sous MAJ — `/noshift` ne la remplace pas.
  NameClickScene bare;
  CHECK(NameClickOrderOf(bare) == NameClickOrder::kNothing);
  bare.no_shift = true;
  CHECK(NameClickOrderOf(bare) == NameClickOrder::kNothing);
  bare.shift_held = true;
  CHECK(NameClickOrderOf(bare) == NameClickOrder::kAttack);
}

TEST_CASE("Nom : `/noshift` remplace MAJ pour la seconde table seule", "[app][nom]") {
  REQUIRE_FALSE(SkillTargetsPlayerWithoutShift(kDecreaseAgi));
  REQUIRE(SkillTargetsPlayerUnderShift(kDecreaseAgi));
  NameClickScene scene = Armed(NamedAim::kOffensive, kDecreaseAgi);
  CHECK(NameClickOrderOf(scene) == NameClickOrder::kNothing);
  scene.no_shift = true;
  CHECK(NameClickOrderOf(scene) == NameClickOrder::kSkill);
}

TEST_CASE("Nom : les classes d'action à part comptent parmi les soutiens", "[app][nom]") {
  CHECK(SkillTargetsPlayerWithoutShift(kTalismanOfProtection));
  CHECK(SkillTargetsPlayerWithoutShift(kBlessingOfMysticalCreatures));
  CHECK_FALSE(SkillTargetsPlayerWithoutShift(kFireBolt));
}

TEST_CASE("Nom : le sien ne prend qu'un soutien", "[app][nom]") {
  NameClickScene own = Armed(NamedAim::kSupport, kHeal);
  own.self = true;
  CHECK(NameClickOrderOf(own) == NameClickOrder::kSkill);
  own.aim = NamedAim::kOffensive;
  own.shift_held = true;
  CHECK(NameClickOrderOf(own) == NameClickOrder::kNothing);
  NameClickScene bare;
  bare.self = true;
  bare.shift_held = true;
  CHECK(NameClickOrderOf(bare) == NameClickOrder::kNothing);
}

TEST_CASE("Nom : mort, ALT, ou second appui d'un double-clic, rien", "[app][nom]") {
  NameClickScene scene = Armed(NamedAim::kSupport, kHeal);
  scene.dead = true;
  CHECK(NameClickOrderOf(scene) == NameClickOrder::kNothing);
  scene = Armed(NamedAim::kSupport, kHeal);
  scene.alt_held = true;
  CHECK(NameClickOrderOf(scene) == NameClickOrder::kNothing);
  scene = Armed(NamedAim::kSupport, kHeal);
  scene.second_of_double = true;
  CHECK(NameClickOrderOf(scene) == NameClickOrder::kNothing);
}

TEST_CASE("Nom : une visée de case s'engage sous MAJ sans rien lancer", "[app][nom]") {
  NameClickScene scene = Armed(NamedAim::kElsewhere, kHeal);
  CHECK(NameClickOrderOf(scene) == NameClickOrder::kNothing);
  scene.shift_held = true;
  CHECK(NameClickOrderOf(scene) == NameClickOrder::kEngageOnly);
}

TEST_CASE("Nom : en zone PK, seul un membre en ligne du groupe est épargné", "[app][nom]") {
  CHECK_FALSE(NameClickPkZone(kMapTypeField));
  CHECK(NameClickPkZone(kMapTypeFreePk));
  CHECK(NameClickPkZone(kMapTypeBattlefield));

  NameClickScene scene = Armed(NamedAim::kOffensive, kFireBolt);
  scene.pk_zone = true;
  CHECK(NameClickOrderOf(scene) == NameClickOrder::kSkill);
  scene.online_party_member = true;
  CHECK(NameClickOrderOf(scene) == NameClickOrder::kNothing);
  // La zone PK passe avant le siège : un ennemi de guilde du groupe n'y est pas visé.
  scene.siege_map = true;
  scene.seen = true;
  CHECK(NameClickOrderOf(scene) == NameClickOrder::kNothing);
}

TEST_CASE("Nom : au siège, un joueur en vue d'une autre guilde se vise", "[app][nom]") {
  NameClickScene scene = Armed(NamedAim::kOffensive, kFireBolt);
  scene.siege_map = true;
  CHECK(NameClickOrderOf(scene) == NameClickOrder::kNothing);
  scene.seen = true;
  CHECK(NameClickOrderOf(scene) == NameClickOrder::kSkill);
  scene.allied_guild = true;
  CHECK(NameClickOrderOf(scene) == NameClickOrder::kNothing);
  scene.allied_guild = false;
  scene.same_guild = true;
  CHECK(NameClickOrderOf(scene) == NameClickOrder::kNothing);
}

TEST_CASE("Nom : il engage sans changer la sélection", "[app][nom][fleche]") {
  const World world = Peopled();
  MouseEngagement engagement;
  NoteEngagingClick(engagement, kWolf, /*continuous=*/false);
  engagement.engaged = false;

  NoteNamedClick(engagement, /*continuous=*/true);
  CHECK(engagement.engaged);
  CHECK(engagement.continuous);
  CHECK(engagement.clicked_id == kWolf);
  // La flèche paraît sur le loup, la dernière entité cliquée dans le monde.
  const auto call = NativeMarkerCall(engagement, world);
  CHECK(call.order == MarkerOrder::kShow);
  REQUIRE(call.on != nullptr);
  CHECK(call.on->id == kWolf);
}

TEST_CASE("Nom : hors de vue, l'action s'achève à la décision suivante", "[app][nom][fleche]") {
  const World world = Peopled();
  constexpr std::uint32_t kAway = 150099;
  MouseEngagement engagement;
  NoteNamedClick(engagement, /*continuous=*/false);
  NoteDeed(engagement, PlayerDeed::kSkillOnTarget, kAway, /*stops_on_release=*/false);
  FollowOwnAction(engagement, /*aimed_target_id=*/0, /*pickup_pending=*/false, world);
  CHECK_FALSE(engagement.engaged);
  CHECK(engagement.action.deed == PlayerDeed::kNothing);
}

TEST_CASE("Nom : le second appui d'un double-clic se retient", "[app][nom]") {
  MouseEngagement engagement;
  NoteLeftPress(engagement, /*second_of_double=*/true, MouseRoute{});
  CHECK(engagement.press_doubled);
  NoteLeftPress(engagement, /*second_of_double=*/false, MouseRoute{});
  CHECK_FALSE(engagement.press_doubled);
}

TEST_CASE("Image : une compétence lancée à portée garde sa flèche une image",
          "[app][nom][fleche]") {
  const World world = Peopled();
  MouseEngagement engagement;
  // Image 1 : le clic arme la compétence sur le loup ; la décision la voit en
  // attente, et montre la flèche.
  NoteEngagingClick(engagement, kWolf, /*continuous=*/false);
  FollowOwnAction(engagement, /*aimed_target_id=*/kWolf, /*pickup_pending=*/false, world);
  CHECK(NativeMarkerCall(engagement, world).order == MarkerOrder::kShow);
  // Le tic, après la décision, la lance : à portée, rien n'attend plus. Le
  // dessin de cette image porte encore la flèche décidée.
  // Image 2 : la décision la voit partie, et cache.
  FollowOwnAction(engagement, /*aimed_target_id=*/0, /*pickup_pending=*/false, world);
  CHECK(NativeMarkerCall(engagement, world).order == MarkerOrder::kHide);
}

TEST_CASE("Image : la flèche lit les positions de l'image précédente", "[app][fleche]") {
  constexpr std::uint32_t kPrevious = 1000;
  constexpr std::uint32_t kNow = 1016;
  CHECK(MarkerSampleMs(kPrevious, kNow) == kPrevious);
  CHECK(MarkerSampleMs(std::nullopt, kNow) == kNow);
}

TEST_CASE("Sprite : MAJ inverse la première table sur un joueur", "[app][nom][visee]") {
  AimedPlayerScene scene;
  scene.skill_id = kHeal;
  scene.support = true;
  CHECK(AimedSkillReachesPlayer(scene));
  scene.shift_held = true;
  CHECK_FALSE(AimedSkillReachesPlayer(scene));
  scene = AimedPlayerScene{};
  scene.skill_id = kFireBolt;
  CHECK_FALSE(AimedSkillReachesPlayer(scene));
  scene.shift_held = true;
  CHECK(AimedSkillReachesPlayer(scene));
  scene.alt_held = true;
  CHECK_FALSE(AimedSkillReachesPlayer(scene));
}

TEST_CASE("Sprite : la carte ouvre hors du groupe, le champ de bataille sur l'autre camp",
          "[app][nom][visee]") {
  AimedPlayerScene scene;
  scene.skill_id = kFireBolt;
  scene.open_field = true;
  CHECK(AimedSkillReachesPlayer(scene));
  scene.online_party_member = true;
  CHECK_FALSE(AimedSkillReachesPlayer(scene));

  scene = AimedPlayerScene{};
  scene.skill_id = kFireBolt;
  scene.battlefield = true;
  CHECK_FALSE(AimedSkillReachesPlayer(scene));
  scene.other_team = true;
  CHECK(AimedSkillReachesPlayer(scene));
}

TEST_CASE("Sprite : au siège, ma guilde par les tables, l'ennemi toujours", "[app][nom][visee]") {
  AimedPlayerScene scene;
  scene.skill_id = kDecreaseAgi;
  scene.siege_map = true;
  CHECK(AimedSkillReachesPlayer(scene));
  scene.same_guild = true;
  CHECK_FALSE(AimedSkillReachesPlayer(scene));
  scene.no_shift = true;
  CHECK(AimedSkillReachesPlayer(scene));
  // Un soutien sur ma guilde part même sous MAJ, au siège.
  scene = AimedPlayerScene{};
  scene.skill_id = kHeal;
  scene.siege_map = true;
  scene.allied_guild = true;
  scene.shift_held = true;
  CHECK(AimedSkillReachesPlayer(scene));
}

TEST_CASE("Sprite : sur soi, un soutien seul", "[app][nom][visee]") {
  AimedPlayerScene scene;
  scene.skill_id = kHeal;
  scene.self = true;
  CHECK_FALSE(AimedSkillReachesPlayer(scene));
  scene.support = true;
  CHECK(AimedSkillReachesPlayer(scene));
  scene.shift_held = true;
  CHECK_FALSE(AimedSkillReachesPlayer(scene));
  scene.no_shift = true;
  CHECK(AimedSkillReachesPlayer(scene));
}

TEST_CASE("Sprite : un soin sur un monstre exige MAJ", "[app][nom][visee]") {
  CHECK_FALSE(AimedSkillReachesMonster(kHeal, false, false, false, false));
  CHECK(AimedSkillReachesMonster(kHeal, true, false, false, false));
  CHECK(AimedSkillReachesMonster(kFireBolt, false, false, false, false));
  CHECK_FALSE(AimedSkillReachesMonster(kFireBolt, true, false, false, false));
  CHECK(AimedSkillReachesMonster(kFireBolt, true, true, false, false));
  CHECK_FALSE(AimedSkillReachesMonster(kFireBolt, false, false, true, false));
  // Un monstre de ma guilde garde l'anneau lent, `/noshift` compris.
  CHECK_FALSE(AimedSkillReachesMonster(kFireBolt, false, true, false, true));
}

TEST_CASE("Fenêtre : l'appui frais désarme, sauf second appui et cadre qui agit",
          "[app][nom][visee]") {
  CHECK(PressOnWindowDisarms(false, false));
  CHECK_FALSE(PressOnWindowDisarms(true, false));
  CHECK_FALSE(PressOnWindowDisarms(false, true));
}
