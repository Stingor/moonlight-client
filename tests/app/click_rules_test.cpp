// Les règles du clic gauche sur le monde : ce que la souris figée laisse
// passer, ce que Ctrl, Maj et Alt retiennent, ce qu'un personnage assis ou
// caché refuse, la garde de poids, l'action du coup tant que le bouton est
// tenu, l'arrêt au relâchement, l'épée du curseur et le double-clic.

#include "world/click_rules.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/cursor.h"
#include "ro/sim/actor.h"
#include "ro/sim/player_status.h"

#include "world/click_action.h"
#include "world/mouse_engagement.h"
#include "world/targeting.h"

using moonclient::BasicAttackAction;
using moonclient::ClickButton;
using moonclient::ClickContactsNpc;
using moonclient::Disengage;
using moonclient::DoubleClickClock;
using moonclient::DoubleClickClocks;
using moonclient::EngagementShowsSword;
using moonclient::GroundClickMoves;
using moonclient::HeldClick;
using moonclient::kAttackActionOnce;
using moonclient::kAttackActionRepeat;
using moonclient::kBackStabSkillId;
using moonclient::kDarkSlashSkillId;
using moonclient::kDoubleClickMs;
using moonclient::kGrimtoothSkillId;
using moonclient::kShadowLeapSkillId;
using moonclient::kShadowSlashSkillId;
using moonclient::kWeightUnguardedSkillId;
using moonclient::kWeightWarnedSkillId;
using moonclient::LetGoLeft;
using moonclient::LetGoRight;
using moonclient::MonsterClickWithheld;
using moonclient::MouseEngagement;
using moonclient::MouseRoute;
using moonclient::NoteDeed;
using moonclient::NoteEngagingClick;
using moonclient::NoteLeftPress;
using moonclient::NoteRelease;
using moonclient::NoteStrike;
using moonclient::PlayerDeed;
using moonclient::RearmedAttackRepeats;
using moonclient::ReleaseStopsAttack;
using moonclient::RidingWithholdsAttack;
using moonclient::ShapeUnderShift;
using moonclient::TakesAttackOrder;
using moonclient::TakesSkillOrder;
using moonclient::WeightGuardOf;
using moonclient::WeightVerdict;
using moonclient::WorldMouse;
using moonclient::WorldTakesClicks;
using ro::render::CursorShape;
using ro::sim::Actor;
using ro::sim::ActorKind;

namespace {

constexpr std::uint32_t kWolf = 2000123;
constexpr std::uint32_t kPoring = 2000456;
constexpr std::uint32_t kStart = 10000;
// Une compétence que rien n'exempte.
constexpr std::uint16_t kHeal = 28;
// Un poids maximum en dixièmes, et son dixième : 90 % tombe pile sur un entier.
constexpr std::int64_t kMaxWeight = 24000;
constexpr std::int64_t kTenthOfMax = 2400;

[[nodiscard]] Actor Spawned(std::uint32_t id, ActorKind kind) {
  Actor actor;
  actor.id = id;
  actor.kind = kind;
  return actor;
}

// Un clic d'attaque tel que le routage le traite : appui, engagement, puis
// l'attaque notée sur la cible.
void ClickToAttack(MouseEngagement& engagement, std::uint32_t id, bool continuous) {
  NoteLeftPress(engagement, /*second_of_double=*/false, MouseRoute{});
  NoteEngagingClick(engagement, id, continuous);
  NoteDeed(engagement, PlayerDeed::kAttack, id, /*stops_on_release=*/!continuous);
}

}  // namespace

TEST_CASE("Clic : la souris figée ne touche plus au monde", "[app][clic]") {
  CHECK(WorldTakesClicks(MouseRoute{}));
  CHECK_FALSE(WorldTakesClicks(MouseRoute{.frozen = true}));
  // Un sort au sol en attente de sa case n'est pas un gel.
  CHECK(WorldTakesClicks(MouseRoute{.suspended = true}));
}

TEST_CASE("Clic : Ctrl tenu, le sol ne fait ni marcher ni tourner", "[app][clic]") {
  CHECK(GroundClickMoves(/*ctrl_held=*/false));
  CHECK_FALSE(GroundClickMoves(/*ctrl_held=*/true));
}

TEST_CASE("Clic : Maj sans /noshift, ou Alt, retient le clic sur un monstre", "[app][clic]") {
  CHECK_FALSE(MonsterClickWithheld(false, false, false));
  CHECK(MonsterClickWithheld(/*shift_held=*/true, /*no_shift=*/false, false));
  // `/noshift` relève Maj, il ne l'inverse pas.
  CHECK_FALSE(MonsterClickWithheld(/*shift_held=*/true, /*no_shift=*/true, false));
  CHECK_FALSE(MonsterClickWithheld(/*shift_held=*/false, /*no_shift=*/true, false));
  CHECK(MonsterClickWithheld(false, false, /*alt_held=*/true));
  CHECK(MonsterClickWithheld(false, /*no_shift=*/true, /*alt_held=*/true));
}

TEST_CASE("Curseur : Maj sur un monstre montre la flèche, Alt non", "[app][clic]") {
  const Actor wolf = Spawned(kWolf, ActorKind::kMonster);
  const Actor rogue = Spawned(kPoring, ActorKind::kPlayer);
  CHECK(ShapeUnderShift(CursorShape::kAttack, &wolf, true, false) == CursorShape::kArrow);
  CHECK(ShapeUnderShift(CursorShape::kAttack, &wolf, true, true) == CursorShape::kAttack);
  CHECK(ShapeUnderShift(CursorShape::kAttack, &wolf, false, false) == CursorShape::kAttack);
  // Un joueur que Maj force reste sous l'épée.
  CHECK(ShapeUnderShift(CursorShape::kAttack, &rogue, true, false) == CursorShape::kAttack);
  CHECK(ShapeUnderShift(CursorShape::kForbidden, nullptr, true, false) == CursorShape::kForbidden);
}

TEST_CASE("Clic : assis ou caché, le personnage ne prend pas l'ordre d'attaque", "[app][clic]") {
  Actor self = Spawned(1, ActorKind::kPlayer);
  CHECK(TakesAttackOrder(self));
  self.posture = ro::sim::Posture::kSitting;
  CHECK_FALSE(TakesAttackOrder(self));
  self.posture = ro::sim::Posture::kStanding;
  self.option = ro::sim::kOptionHide;
  CHECK_FALSE(TakesAttackOrder(self));
  // Le Cloaking n'est pas le Hiding : il laisse frapper.
  self.option = ro::sim::kOptionCloak;
  CHECK(TakesAttackOrder(self));
}

TEST_CASE("Clic : sur une monture cash-shop, le coup ne part pas", "[app][clic]") {
  Actor self = Spawned(1, ActorKind::kPlayer);
  CHECK_FALSE(RidingWithholdsAttack(self));
  self.status_effects.insert(ro::sim::kStatusAllRiding);
  CHECK(RidingWithholdsAttack(self));
  // L'ordre, lui, se prend encore : c'est l'envoi qui est retenu.
  CHECK(TakesAttackOrder(self));
  self.status_effects.erase(ro::sim::kStatusAllRiding);
  CHECK_FALSE(RidingWithholdsAttack(self));
}

TEST_CASE("Clic : assis, aucune compétence visée ; caché, seulement celles de l'ombre",
          "[app][clic]") {
  Actor self = Spawned(1, ActorKind::kPlayer);
  CHECK(TakesSkillOrder(self, kHeal));
  self.posture = ro::sim::Posture::kSitting;
  CHECK_FALSE(TakesSkillOrder(self, kHeal));
  // Assis, même celles de l'ombre ne partent pas.
  CHECK_FALSE(TakesSkillOrder(self, kBackStabSkillId));
  self.posture = ro::sim::Posture::kStanding;
  self.option = ro::sim::kOptionHide;
  CHECK_FALSE(TakesSkillOrder(self, kHeal));
  for (const std::uint16_t from_shadow : {kGrimtoothSkillId, kBackStabSkillId, kShadowLeapSkillId,
                                          kShadowSlashSkillId, kDarkSlashSkillId}) {
    CHECK(TakesSkillOrder(self, from_shadow));
  }
  // Le Cloaking n'est pas le Hiding : tout part.
  self.option = ro::sim::kOptionCloak;
  CHECK(TakesSkillOrder(self, kHeal));
}

TEST_CASE("Poids : à 90 % du maximum, l'ordre est refusé", "[app][clic][poids]") {
  CHECK(WeightGuardOf(kMaxWeight - kTenthOfMax - 1, kMaxWeight, 0) == WeightVerdict::kPasses);
  CHECK(WeightGuardOf(kMaxWeight - kTenthOfMax, kMaxWeight, 0) == WeightVerdict::kRefuses);
  CHECK(WeightGuardOf(kMaxWeight, kMaxWeight, kHeal) == WeightVerdict::kRefuses);
  // Un poids que le serveur n'a pas encore dit passe.
  CHECK(WeightGuardOf(ro::sim::kUnknownStat, ro::sim::kUnknownStat, 0) == WeightVerdict::kPasses);
  // Un maximum nul compte pour un : le moindre poids refuse.
  CHECK(WeightGuardOf(1, 0, 0) == WeightVerdict::kRefuses);
  CHECK(WeightGuardOf(0, 0, 0) == WeightVerdict::kPasses);
}

TEST_CASE("Poids : deux compétences échappent à l'abandon", "[app][clic][poids]") {
  // L'une n'est pas même regardée…
  CHECK(WeightGuardOf(kMaxWeight, kMaxWeight, kWeightUnguardedSkillId) == WeightVerdict::kPasses);
  // … l'autre fait écrire le refus, puis part.
  CHECK(WeightGuardOf(kMaxWeight, kMaxWeight, kWeightWarnedSkillId) ==
        WeightVerdict::kWarnsAndPasses);
  CHECK(WeightGuardOf(0, kMaxWeight, kWeightWarnedSkillId) == WeightVerdict::kPasses);
}

TEST_CASE("Clic : un compagnon ne reçoit pas de contact de PNJ", "[app][clic]") {
  Actor npc = Spawned(110001, ActorKind::kNonPlayer);
  CHECK(ClickContactsNpc(npc));
  npc.familiar = ro::sim::Familiar::kPet;
  CHECK_FALSE(ClickContactsNpc(npc));
  npc.familiar = ro::sim::Familiar::kHomunculus;
  CHECK_FALSE(ClickContactsNpc(npc));
  npc.familiar = ro::sim::Familiar::kNone;
  npc.look.job = ro::sim::kPortalJob;
  CHECK_FALSE(ClickContactsNpc(npc));
  CHECK_FALSE(ClickContactsNpc(Spawned(kWolf, ActorKind::kMonster)));
}

TEST_CASE("Attaque : un clic qui ne se poursuit pas frappe sans relâche tant qu'il est tenu",
          "[app][clic]") {
  MouseEngagement engagement;
  ClickToAttack(engagement, kWolf, /*continuous=*/false);
  CHECK(BasicAttackAction(false, engagement.action, kWolf) == kAttackActionRepeat);
  // Une autre cible n'est pas celle que le bouton tient.
  CHECK(BasicAttackAction(false, engagement.action, kPoring) == kAttackActionOnce);
  // Relâché avant d'arriver : un seul coup.
  NoteRelease(engagement);
  CHECK(BasicAttackAction(false, engagement.action, kWolf) == kAttackActionOnce);
}

TEST_CASE("Attaque : un clic continu frappe toujours sans relâche", "[app][clic]") {
  MouseEngagement engagement;
  ClickToAttack(engagement, kWolf, /*continuous=*/true);
  NoteRelease(engagement);
  CHECK(BasicAttackAction(true, engagement.action, kWolf) == kAttackActionRepeat);
  // Sans geste noté, le clic décide seul.
  CHECK(BasicAttackAction(false, MouseEngagement().action, kWolf) == kAttackActionOnce);
}

TEST_CASE("Attaque : le relâchement arrête la suite de coups d'un clic qui ne se poursuit pas",
          "[app][clic]") {
  MouseEngagement engagement;
  ClickToAttack(engagement, kWolf, /*continuous=*/false);
  // Rien n'a frappé : rien à arrêter.
  CHECK_FALSE(ReleaseStopsAttack(engagement));
  NoteStrike(engagement, kWolf);
  CHECK(ReleaseStopsAttack(engagement));
  NoteRelease(engagement);
  // L'action s'achève avec le relâchement : un second ne réarrête rien.
  CHECK(engagement.action.deed == PlayerDeed::kNothing);
  CHECK_FALSE(ReleaseStopsAttack(engagement));

  MouseEngagement continuous;
  ClickToAttack(continuous, kWolf, /*continuous=*/true);
  NoteStrike(continuous, kWolf);
  CHECK_FALSE(ReleaseStopsAttack(continuous));
}

TEST_CASE("Curseur : l'épée tant qu'un engagement qui ne se poursuit pas est tenu", "[app][clic]") {
  MouseEngagement engagement;
  ClickToAttack(engagement, kWolf, /*continuous=*/false);
  CHECK(EngagementShowsSword(engagement));
  NoteRelease(engagement);
  CHECK_FALSE(EngagementShowsSword(engagement));

  // Engagé en continu, le curseur garde sa forme ordinaire.
  MouseEngagement continuous;
  ClickToAttack(continuous, kWolf, /*continuous=*/true);
  CHECK_FALSE(EngagementShowsSword(continuous));
}

TEST_CASE("Attaque : le réarmement « trop loin » reprend le type du dernier clic", "[app][clic]") {
  MouseEngagement engagement;
  ClickToAttack(engagement, kWolf, /*continuous=*/true);
  // Même retombé, l'engagement garde le type du clic qui l'avait levé.
  Disengage(engagement);
  CHECK(RearmedAttackRepeats(engagement));
  ClickToAttack(engagement, kPoring, /*continuous=*/false);
  CHECK_FALSE(RearmedAttackRepeats(engagement));
}

TEST_CASE("Double-clic : deux appuis sans mouvement, sous le délai", "[app][clic]") {
  DoubleClickClock clock;
  CHECK_FALSE(clock.NotePress(kStart));
  CHECK(clock.NotePress(kStart + kDoubleClickMs - 1));
  // Un troisième appui ne fait pas un second double avec le deuxième.
  CHECK_FALSE(clock.NotePress(kStart + kDoubleClickMs));
  // À l'échéance pile, l'appui est neuf.
  CHECK_FALSE(clock.NotePress(kStart + (2 * kDoubleClickMs)));
}

TEST_CASE("Double-clic : le moindre mouvement entre les appuis le défait", "[app][clic]") {
  DoubleClickClock clock;
  CHECK_FALSE(clock.NotePress(kStart));
  clock.NoteMotion();
  CHECK_FALSE(clock.NotePress(kStart + 1));
  // Ce second appui compte comme un premier : le suivant, immobile, double.
  CHECK(clock.NotePress(kStart + 2));
}

TEST_CASE("Double-clic droit : la même horloge que le gauche", "[app][clic]") {
  DoubleClickClocks clocks;
  CHECK_FALSE(clocks.NotePress(ClickButton::kRight, kStart));
  CHECK(clocks.NotePress(ClickButton::kRight, kStart + kDoubleClickMs - 1));
  // Au-delà du délai, l'appui est neuf, quel que soit le réglage du système.
  CHECK_FALSE(clocks.NotePress(ClickButton::kRight, kStart + (2 * kDoubleClickMs)));
  CHECK_FALSE(clocks.NotePress(ClickButton::kRight, kStart + (3 * kDoubleClickMs)));
}

TEST_CASE("Double-clic droit : le moindre mouvement le défait", "[app][clic]") {
  DoubleClickClocks clocks;
  CHECK_FALSE(clocks.NotePress(ClickButton::kRight, kStart));
  clocks.NoteMotion();
  CHECK_FALSE(clocks.NotePress(ClickButton::kRight, kStart + 1));
  CHECK(clocks.NotePress(ClickButton::kRight, kStart + 2));
}

TEST_CASE("Double-clic : chaque bouton a son horloge", "[app][clic]") {
  DoubleClickClocks clocks;
  CHECK_FALSE(clocks.NotePress(ClickButton::kLeft, kStart));
  // Un appui droit ne complète pas le double du gauche, ni ne le défait.
  CHECK_FALSE(clocks.NotePress(ClickButton::kRight, kStart + 1));
  CHECK(clocks.NotePress(ClickButton::kLeft, kStart + 2));
  CHECK(clocks.NotePress(ClickButton::kRight, kStart + 3));
  // Le mouvement désarme les deux.
  CHECK_FALSE(clocks.NotePress(ClickButton::kLeft, kStart + 4));
  CHECK_FALSE(clocks.NotePress(ClickButton::kRight, kStart + 5));
  clocks.NoteMotion();
  CHECK_FALSE(clocks.NotePress(ClickButton::kLeft, kStart + 6));
  CHECK_FALSE(clocks.NotePress(ClickButton::kRight, kStart + 7));
}

TEST_CASE("Veille : le bouton gauche lâché arrête la marche tenue et l'attaque", "[app][clic]") {
  // 🔴 LA VEILLE AVALE CE RELÂCHEMENT : sans cela, le maintien redemanderait la
  // marche sans fin, et le personnage marcherait seul.
  WorldMouse mouse;
  mouse.held_click = HeldClick::kWalk;
  ClickToAttack(mouse.engagement, kWolf, /*continuous=*/false);
  NoteStrike(mouse.engagement, kWolf);
  CHECK(LetGoLeft(mouse));
  CHECK(mouse.held_click == HeldClick::kNothing);
  CHECK(mouse.engagement.action.deed == PlayerDeed::kNothing);
  CHECK_FALSE(mouse.engagement.left_held);
  // Rien à arrêter une seconde fois.
  CHECK_FALSE(LetGoLeft(mouse));
}

TEST_CASE("Veille : le bouton droit lâché oublie ce que son appui retenait", "[app][clic]") {
  WorldMouse mouse;
  mouse.right_on_signboard = true;
  mouse.right_pressed_at = glm::vec2{1.0F, 2.0F};
  mouse.right_press_doubled = true;
  LetGoRight(mouse);
  CHECK_FALSE(mouse.right_on_signboard);
  CHECK_FALSE(mouse.right_pressed_at.has_value());
  CHECK_FALSE(mouse.right_press_doubled);
}
