#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_effect_table.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/actor_wardrobe.h"
#include "ro/render/led_pose.h"
#include "ro/render/skill_held_pose.h"
#include "ro/render/skill_impl.h"
#include "ro/render/skill_use_motion.h"
#include "ro/render/sprite_frame.h"
#include "ro/sim/actor.h"
#include "ro/sim/combat_guard.h"
#include "ro/sim/combat_notice.h"
#include "ro/sim/packed_position.h"

#include "support/actor_client.h"
#include "world/actor_view.h"

using moonclient::LedPoseAt;
using moonclient::LedPoseHoldsAt;
using moonclient::MotionEvents;
using ro::render::Motion;
using ro::render::SpriteLayout;

namespace {

constexpr std::uint16_t kDanseuse = 4021;
constexpr std::uint16_t kNinja = 25;
// Un soin : l'emploi ordinaire, sans pose de compétence à lui.
constexpr std::uint16_t kHealSkill = 28;
constexpr std::uint32_t kStartMs = 10'000;
// L'avance de l'acteur : seize millisecondes.
constexpr std::uint32_t kTickMs = 16;

ro::sim::Actor Employant(std::uint16_t job, std::uint16_t skill) {
  ro::sim::Actor subject;
  subject.id = 1;
  subject.kind = ro::sim::ActorKind::kPlayer;
  subject.look.job = job;
  subject.skill_use = ro::sim::SkillUse{skill, kStartMs};
  return subject;
}

std::optional<ro::render::HeldPoseFrame> HeldPoseAt(const ro::sim::Actor& subject,
                                                    std::uint32_t at_ms) {
  return LedPoseAt(nullptr, subject, SpriteLayout::kPlayer, Motion::kIdle, MotionEvents{}, at_ms);
}

}  // namespace

TEST_CASE("Pose menée : l'emploi d'une danseuse tient quatre cents millisecondes",
          "[app][posture]") {
  REQUIRE(ro::render::SkillUseStateFor(kHealSkill) == 7);
  REQUIRE(ro::render::SkillHeldPoseOf(kHealSkill) == nullptr);
  const ro::sim::Actor danseuse = Employant(kDanseuse, kHealSkill);
  const auto view = HeldPoseAt(danseuse, kStartMs + 100);
  REQUIRE(view.has_value());
  CHECK(view->rank == 12);
  CHECK(view->frame == 0);
  CHECK_FALSE(HeldPoseAt(danseuse, kStartMs + 400).has_value());
  // Et la blessure n'a pas lieu pendant ce temps.
  CHECK(
      LedPoseHoldsAt(nullptr, danseuse, SpriteLayout::kPlayer, Motion::kIdle, {}, kStartMs + 399));
  CHECK_FALSE(
      LedPoseHoldsAt(nullptr, danseuse, SpriteLayout::kPlayer, Motion::kIdle, {}, kStartMs + 400));
}

TEST_CASE("Pose menée : le ninja avance de deux à cinq, puis tient", "[app][posture]") {
  const ro::sim::Actor ninja = Employant(kNinja, kHealSkill);
  // Une image toutes les quatre avances.
  constexpr std::uint32_t kImageMs = 4 * kTickMs;
  CHECK(HeldPoseAt(ninja, kStartMs)->frame == 2);
  CHECK(HeldPoseAt(ninja, kStartMs + kImageMs)->frame == 3);
  CHECK(HeldPoseAt(ninja, kStartMs + 3 * kImageMs)->frame == 5);
  CHECK(HeldPoseAt(ninja, kStartMs + 900)->frame == 5);
  CHECK_FALSE(HeldPoseAt(ninja, kStartMs + 1000).has_value());
}

TEST_CASE("Pose menée : l'action suivante la termine", "[app][posture]") {
  // 🔴 LA MOTION SUIVANTE REMET LA POSE À ZÉRO, avant son échéance.
  ro::sim::Actor frappe = Employant(kNinja, kHealSkill);
  frappe.attack = ro::sim::Strike{};
  frappe.attack->started_ms = kStartMs + 100;
  CHECK(HeldPoseAt(frappe, kStartMs + 50).has_value());
  CHECK_FALSE(HeldPoseAt(frappe, kStartMs + 200).has_value());

  ro::sim::Actor march = Employant(kNinja, kHealSkill);
  march.movement = ro::sim::CellMovement{};
  march.movement_started_ms = kStartMs + 100;
  CHECK_FALSE(HeldPoseAt(march, kStartMs + 200).has_value());

  ro::sim::Actor assis = Employant(kNinja, kHealSkill);
  assis.posture_since_ms = kStartMs + 100;
  CHECK_FALSE(HeldPoseAt(assis, kStartMs + 200).has_value());

  // Ce qui précède la pose ne la termine pas.
  ro::sim::Actor before = Employant(kNinja, kHealSkill);
  before.movement = ro::sim::CellMovement{};
  before.movement_started_ms = kStartMs - 100;
  CHECK(HeldPoseAt(before, kStartMs + 200).has_value());
}

TEST_CASE("Pose menée : une pose de compétence finit aussi à l'action suivante", "[app][posture]") {
  const ro::render::SkillHeldPose* pose = nullptr;
  for (const ro::render::SkillHeldPose& candidate : ro::render::SkillHeldPoseTable()) {
    if (candidate.damage_types == 0 && !candidate.hold_from_combo_delay &&
        (!candidate.timed || candidate.hold_ms > 200)) {
      pose = &candidate;
      break;
    }
  }
  REQUIRE(pose != nullptr);
  ro::sim::Actor caster_actor = Employant(0, pose->skill_id);
  CHECK(HeldPoseAt(caster_actor, kStartMs).has_value());
  caster_actor.pickup_since_ms = kStartMs + 50;
  CHECK_FALSE(HeldPoseAt(caster_actor, kStartMs + 100).has_value());
}

TEST_CASE("Pose menée : l'incantation d'état 19 et sa suite de cinq pas", "[app][posture]") {
  ro::sim::Actor caster_actor;
  caster_actor.kind = ro::sim::ActorKind::kPlayer;
  caster_actor.look.job = 0;
  caster_actor.cast = ro::sim::Cast{};
  caster_actor.cast->skill_id = 426;
  caster_actor.cast->started_ms = kStartMs;
  caster_actor.cast->duration_ms = 2'000;
  // Dix avances par pas : l'image zéro trois pas durant, puis l'image un.
  constexpr std::uint32_t kStepMs = 10 * kTickMs;
  const auto start = HeldPoseAt(caster_actor, kStartMs);
  REQUIRE(start.has_value());
  CHECK(start->rank == 5);
  CHECK(start->frame == 0);
  CHECK(HeldPoseAt(caster_actor, kStartMs + 3 * kStepMs - 1)->frame == 0);
  CHECK(HeldPoseAt(caster_actor, kStartMs + 3 * kStepMs)->frame == 1);
  CHECK(HeldPoseAt(caster_actor, kStartMs + 2'999)->frame == 1);
  // Passé trois secondes, le client cesse d'avancer l'image, et elle reste ;
  // seule la garde de la blessure tombe.
  const auto after = HeldPoseAt(caster_actor, kStartMs + ro::render::kLedCastPoseMs);
  REQUIRE(after.has_value());
  CHECK(after->frame == 1);
  CHECK(LedPoseHoldsAt(nullptr, caster_actor, SpriteLayout::kPlayer, Motion::kIdle, MotionEvents{},
                       kStartMs + 2'999));
  CHECK_FALSE(LedPoseHoldsAt(nullptr, caster_actor, SpriteLayout::kPlayer, Motion::kIdle,
                             MotionEvents{}, kStartMs + ro::render::kLedCastPoseMs));
}

namespace {

// AGI UP, PICK STONE, et une compétence qu'on incante en état huit : les
// trois ramènent au repos quand on les emploie.
constexpr std::uint16_t kIncreaseAgility = 29;
constexpr std::uint16_t kPickStone = 151;
constexpr std::uint16_t kRestAfterGuardingCast = 256;
constexpr std::uint8_t kLookingLeft = 2;

// Un novice de treize rangs, deux images par direction : de quoi jouer
// l'incantation, au rang douze.
struct DressedNovice {
  DressedNovice()
      : sprite(*ro::formats::SprSprite::Parse(ro::test_support::SpriteBytes())),
        animation(*ro::formats::ActAnimation::Parse(
            ro::test_support::AnimationBytes(kPlayerAnimationCount))),
        outfit(Pieces(sprite, animation)) {}

  static constexpr std::size_t kPlayerAnimationCount = 13;

  static std::vector<ro::render::ActorPiece> Pieces(const ro::formats::SprSprite& body_sprite,
                                                    const ro::formats::ActAnimation& body_act) {
    ro::render::ActorPiece body;
    body.sprite = &body_sprite;
    body.animation = &body_act;
    body.part = ro::render::ActorPart::kBody;
    return {body};
  }

  // Les deux postures de compétence, dans l'ordre où la scène les consulte,
  // par-dessus le repos ou la garde. La scène tient ensuite l'image du regard
  // d'un joueur à l'attente.
  moonclient::PlayedMotion At(const ro::sim::Actor& actor, std::uint32_t at_ms,
                              const MotionEvents& events = MotionEvents{}) {
    const Motion resting = moonclient::RestMotionAt(actor, events, at_ms);
    moonclient::PlayedMotion played;
    played.motion = resting;
    played.animation = ro::render::AnimationFor(resting, SpriteLayout::kPlayer);
    if (actor.cast.has_value()) {
      played = moonclient::CastPostureAt(nullptr, actor, outfit, SpriteLayout::kPlayer,
                                         played.motion, events, at_ms)
                   .value_or(played);
    }
    if (played.motion != Motion::kCast && actor.skill_use.has_value()) {
      played = moonclient::SkillUsePostureAt(nullptr, actor, outfit, SpriteLayout::kPlayer,
                                             played.motion, events, at_ms)
                   .value_or(played);
    }
    return played;
  }

  ro::formats::SprSprite sprite;
  ro::formats::ActAnimation animation;
  ro::render::ActorOutfit outfit;
};

ro::sim::Actor Novice(std::uint16_t skill_id) {
  ro::sim::Actor novice = Employant(0, skill_id);
  novice.head_direction = kLookingLeft;
  return novice;
}

ro::sim::Cast CastOf(std::uint16_t skill_id, std::uint32_t started_ms, std::uint32_t duration_ms) {
  ro::sim::Cast cast;
  cast.skill_id = skill_id;
  cast.started_ms = started_ms;
  cast.duration_ms = duration_ms;
  return cast;
}

}  // namespace

TEST_CASE("Repos : Agi Up employé tient l'attente et le regard, sans balayer de la tête",
          "[app][posture]") {
  // 🔴 L'ÉTAT ZÉRO RAMÈNE AU REPOS. Joué comme un geste, le rang zéro faisait
  // défiler ses trois images — trois orientations de la tête.
  REQUIRE(ro::render::SkillUseStateFor(kIncreaseAgility) == 0);
  DressedNovice novice;
  ro::sim::Actor caster = Novice(kIncreaseAgility);
  // L'incantation d'état sept qui précède ne laisse pas la garde.
  caster.cast = CastOf(kIncreaseAgility, kStartMs - 800, 800);
  for (const std::uint32_t after_ms : {0U, 100U, 250U, 500U, 2'000U}) {
    const moonclient::PlayedMotion played = novice.At(caster, kStartMs + after_ms);
    CHECK(played.motion == Motion::kIdle);
    CHECK(played.animation == ro::render::kAnimationIdle);
  }
}

TEST_CASE("Repos : Pick Stone se baisse une fois pendant l'incantation, puis se relève",
          "[app][posture]") {
  REQUIRE(ro::render::SkillUseStateFor(kPickStone) == 0);
  DressedNovice novice;
  ro::sim::Actor caster = Novice(kPickStone);
  constexpr std::uint32_t kCastMs = 500;
  const std::uint32_t cast_start_ms = kStartMs - kCastMs;
  caster.cast = CastOf(kPickStone, cast_start_ms, kCastMs);
  caster.skill_use = ro::sim::SkillUse{kPickStone, kStartMs};

  // Le ramassage, depuis sa PREMIÈRE image à l'annonce, et non à une phase de
  // l'horloge commune.
  const moonclient::PlayedMotion crouching = novice.At(caster, cast_start_ms + 10);
  CHECK(crouching.animation == ro::render::AnimationFor(Motion::kPickup, SpriteLayout::kPlayer));
  CHECK_FALSE(crouching.loops);
  CHECK(crouching.elapsed_ms == 10.0F);
  // 🔴 UNE PASSE, PUIS L'ATTENTE, la barre courant encore : le client voit le
  // geste fini et repasse au repos, et le ramassage n'est pas de ceux qu'on
  // quitte pour la garde. Deux images de quatre-vingt-seize millisecondes.
  const moonclient::PlayedMotion pass_over = novice.At(caster, cast_start_ms + 400);
  CHECK(pass_over.motion == Motion::kIdle);

  // L'emploi ne rejoue rien : l'attente, et non la garde qu'une incantation
  // laisserait chez nous.
  const moonclient::PlayedMotion after = novice.At(caster, kStartMs + 150);
  CHECK(after.motion == Motion::kIdle);
}

TEST_CASE("Repos : quittant une incantation d'état huit, l'acteur prend la garde",
          "[app][posture]") {
  REQUIRE(ro::render::SkillUseStateFor(kRestAfterGuardingCast) == 0);
  REQUIRE(ro::render::SkillImplFor(kRestAfterGuardingCast).CastState() == 8);
  DressedNovice novice;
  ro::sim::Actor caster = Novice(kRestAfterGuardingCast);
  caster.cast = CastOf(kRestAfterGuardingCast, kStartMs - 1'000, 1'000);
  CHECK(novice.At(caster, kStartMs + 100).motion == Motion::kReady);

  // Et la marche qui suit le termine.
  caster.movement = ro::sim::CellMovement{};
  caster.movement_started_ms = kStartMs + 200;
  CHECK(novice.At(caster, kStartMs + 300).motion != Motion::kReady);
}

namespace {

// Cold Bolt : l'incantation ordinaire, d'état huit ; Bash : un coup d'état
// deux ; un soin : l'emploi ordinaire, d'état sept.
constexpr std::uint16_t kColdBolt = 14;
constexpr std::uint16_t kBash = 5;
constexpr std::uint16_t kHeal = 28;
// La durée d'une passe des deux images du novice.
constexpr std::uint32_t kOnePassMs = 2 * 96;

ro::sim::Strike BlowOf(std::uint16_t skill_id, std::uint32_t started_ms) {
  ro::sim::Strike strike;
  strike.attacker = 1;
  strike.target = 2;
  strike.skill_id = skill_id;
  strike.started_ms = started_ms;
  return strike;
}

}  // namespace

TEST_CASE("Incantation : l'état huit tient sa PREMIÈRE image jusqu'à la fin de la barre",
          "[app][posture]") {
  // 🔴 LE CLIENT POSE LE RANG ET FIGE L'IMAGE D'EMBLÉE : rien ne défile. Jouée
  // une fois, l'incantation montrait les cinq autres images du rang, que le jeu
  // ne montre jamais là.
  REQUIRE(ro::render::SkillImplFor(kColdBolt).CastState() == 8);
  DressedNovice novice;
  ro::sim::Actor caster = Novice(kColdBolt);
  caster.skill_use.reset();
  constexpr std::uint32_t kCastMs = 2'000;
  caster.cast = CastOf(kColdBolt, kStartMs, kCastMs);
  for (const std::uint32_t after_ms : {0U, 10U, 500U, 1'999U}) {
    const moonclient::PlayedMotion held = novice.At(caster, kStartMs + after_ms);
    CHECK(held.motion == Motion::kCast);
    CHECK(held.animation == ro::render::kAnimationCast);
    CHECK(held.held_frame == std::size_t{0});
    CHECK_FALSE(held.loops);
  }
  // À la fin de la barre, le client repasse au repos : la GARDE, puisque l'état
  // quitté est l'incantation.
  CHECK(novice.At(caster, kStartMs + kCastMs).motion == Motion::kReady);
  // Un coup encaissé en plein milieu la termine, et elle ne revient pas : la
  // scène dit l'instant où il porte, comme `MotionAt` le fait.
  ro::sim::Actor struck = caster;
  struck.hit = BlowOf(0, kStartMs + 300);
  struck.hit->attacker = 2;
  struck.hit->target = 1;
  struck.hit->flinches = true;
  const MotionEvents struck_events{kStartMs + 300, {}, kStartMs + 1'000};
  CHECK(novice.At(struck, kStartMs + 1'000, struck_events).motion != Motion::kCast);
}

TEST_CASE("Incantation : Sharpshooting tient l'image zéro de son tir, sans la rejouer",
          "[app][posture]") {
  // 🔴 L'ÉTAT 18 MÈNE SA POSE : le client pose le rang onze et n'en montre que
  // l'image zéro, trois secondes, puis la laisse jusqu'à la motion suivante —
  // la fin de la barre n'y touche pas. Jouée comme un geste, l'incantation du
  // Sniper défilait son tir au lieu de le tenir armé.
  constexpr std::uint16_t kSharpshooting = 382;
  constexpr std::size_t kShootingRank = 11;
  REQUIRE(ro::render::SkillImplFor(kSharpshooting).CastState() == 18);
  DressedNovice novice;
  ro::sim::Actor sniper = Novice(kSharpshooting);
  sniper.skill_use.reset();
  constexpr std::uint32_t kCastMs = 2'000;
  sniper.cast = CastOf(kSharpshooting, kStartMs, kCastMs);
  for (const std::uint32_t after_ms : {0U, 10U, 500U, 1'999U, 2'000U, 2'999U, 4'000U}) {
    const moonclient::PlayedMotion held = novice.At(sniper, kStartMs + after_ms);
    CHECK(held.motion == Motion::kCast);
    CHECK(held.animation == kShootingRank);
    CHECK(held.held_frame == std::size_t{0});
    CHECK_FALSE(held.loops);
    const auto led = HeldPoseAt(sniper, kStartMs + after_ms);
    REQUIRE(led.has_value());
    CHECK(led->rank == kShootingRank);
    CHECK(led->frame == 0);
  }
  // La blessure reste écartée trois secondes, pas davantage.
  CHECK(LedPoseHoldsAt(nullptr, sniper, SpriteLayout::kPlayer, Motion::kIdle, MotionEvents{},
                       kStartMs + 2'999));
  CHECK_FALSE(LedPoseHoldsAt(nullptr, sniper, SpriteLayout::kPlayer, Motion::kIdle, MotionEvents{},
                             kStartMs + 3'000));
  // Un coup encaissé la termine, comme toute motion posée depuis l'annonce.
  ro::sim::Actor struck = sniper;
  struck.hit = BlowOf(0, kStartMs + 300);
  struck.hit->attacker = 2;
  struck.hit->target = 1;
  struck.hit->flinches = true;
  const MotionEvents struck_events{kStartMs + 300, {}, kStartMs + 1'000};
  CHECK(novice.At(struck, kStartMs + 1'000, struck_events).motion != Motion::kCast);
}

TEST_CASE("Incantation : étalée sur la barre, son geste se déroule une fois", "[app][posture]") {
  // Un réglage du joueur, qui s'écarte du client : au lieu de tenir l'image
  // zéro, le geste se déroule du début de la barre à sa fin.
  DressedNovice novice;
  ro::sim::Actor caster = Novice(kColdBolt);
  caster.skill_use.reset();
  constexpr std::uint32_t kCastMs = 2'000;
  caster.cast = CastOf(kColdBolt, kStartMs, kCastMs);
  const float pass_ms = novice.outfit.AnimationDurationMs(ro::render::kAnimationCast);
  REQUIRE(pass_ms > 0.0F);

  moonclient::PlayedMotion halfway = novice.At(caster, kStartMs + kCastMs / 2);
  REQUIRE(halfway.held_frame == std::size_t{0});
  moonclient::SpreadCastOverBar(caster, novice.outfit, SpriteLayout::kPlayer,
                                kStartMs + kCastMs / 2, halfway);
  CHECK(halfway.motion == Motion::kCast);
  CHECK_FALSE(halfway.held_frame.has_value());
  CHECK(halfway.elapsed_ms == pass_ms / 2.0F);
  CHECK_FALSE(halfway.loops);

  // Hors de sa barre, ou pour une autre pose, rien ne change.
  moonclient::PlayedMotion after = novice.At(caster, kStartMs + kCastMs);
  const moonclient::PlayedMotion untouched = after;
  moonclient::SpreadCastOverBar(caster, novice.outfit, SpriteLayout::kPlayer, kStartMs + kCastMs,
                                after);
  CHECK(after.motion == untouched.motion);
  CHECK(after.held_frame == untouched.held_frame);
}

TEST_CASE("Incantation : lancée pendant un coup, elle le termine sur-le-champ", "[app][posture]") {
  // 🔴 LE CLIENT POSE L'INCANTATION SANS REGARDER CE QUE FAIT L'ACTEUR : un coup
  // encore en cours cède aussitôt. Le coup l'emportait jusqu'à la fin de son
  // animation, et une barre plus courte ne se voyait jamais.
  ro::sim::Actor fighter = Novice(kColdBolt);
  fighter.skill_use.reset();
  fighter.attack = BlowOf(0, kStartMs);
  const moonclient::ChangeWindow since_blow{kStartMs, kStartMs + 300, true};
  CHECK_FALSE(moonclient::MotionChangedSince(nullptr, fighter, SpriteLayout::kPlayer,
                                             Motion::kAttack, MotionEvents{}, since_blow));
  fighter.cast = CastOf(kColdBolt, kStartMs + 100, 1'000);
  CHECK(moonclient::MotionChangedSince(nullptr, fighter, SpriteLayout::kPlayer, Motion::kAttack,
                                       MotionEvents{}, since_blow));
  // Une incantation SANS durée ne pose rien : le coup continue.
  fighter.cast = CastOf(kColdBolt, kStartMs + 100, 0);
  CHECK_FALSE(moonclient::MotionChangedSince(nullptr, fighter, SpriteLayout::kPlayer,
                                             Motion::kAttack, MotionEvents{}, since_blow));
}

TEST_CASE("Incantation : interrompue, elle laisse la garde ; achevée par un emploi, l'attente",
          "[app][posture]") {
  DressedNovice novice;
  ro::sim::Actor caster = Novice(kHeal);
  caster.skill_use.reset();
  // Coupée à trois cents millisecondes : le monde ramène sa durée à ce qui a
  // été joué, et le repos sait qu'on quitte l'incantation.
  caster.cast = CastOf(kColdBolt, kStartMs, 300);
  CHECK(novice.At(caster, kStartMs + 100).motion == Motion::kCast);
  CHECK(novice.At(caster, kStartMs + 400).motion == Motion::kReady);

  // Un soin employé après une incantation : l'emploi d'état sept joue une
  // passe, puis c'est l'ATTENTE — l'état quitté est l'emploi, pas l'incantation.
  caster.cast = CastOf(kHeal, kStartMs, 500);
  caster.skill_use = ro::sim::SkillUse{kHeal, kStartMs + 500};
  CHECK(novice.At(caster, kStartMs + 500 + kOnePassMs).motion == Motion::kIdle);
}

TEST_CASE("Repos : la garde suit le coup, l'attente suit le coup d'une compétence d'état sept",
          "[app][posture]") {
  // Un coup ordinaire quitte l'attaque : la garde, jusqu'à la marche suivante.
  ro::sim::Actor fighter = Novice(kBash);
  fighter.skill_use.reset();
  fighter.attack = BlowOf(0, kStartMs);
  CHECK(moonclient::RestMotionAt(fighter, MotionEvents{}, kStartMs + 1'000) == Motion::kReady);
  fighter.movement = ro::sim::CellMovement{};
  fighter.movement_started_ms = kStartMs + 1'500;
  CHECK(moonclient::RestMotionAt(fighter, MotionEvents{}, kStartMs + 2'000) == Motion::kIdle);
  // Bash frappe en état deux : la garde. Cold Bolt frappe en état sept, celui
  // de l'emploi ordinaire : le client le voit fini et repasse à l'ATTENTE.
  REQUIRE(ro::render::SkillUseStateFor(kBash) == 2);
  REQUIRE(ro::render::SkillUseStateFor(kColdBolt) == 7);
  fighter.attack = BlowOf(kBash, kStartMs + 3'000);
  CHECK(moonclient::RestMotionAt(fighter, MotionEvents{}, kStartMs + 4'000) == Motion::kReady);
  fighter.attack = BlowOf(kColdBolt, kStartMs + 5'000);
  CHECK(moonclient::RestMotionAt(fighter, MotionEvents{}, kStartMs + 6'000) == Motion::kIdle);
  // Un coup encaissé, lui, laisse toujours la garde.
  ro::sim::Actor struck = Novice(kBash);
  struck.skill_use.reset();
  struck.hit = BlowOf(0, kStartMs);
  struck.hit->flinches = true;
  CHECK(moonclient::RestMotionAt(struck, MotionEvents{}, kStartMs + 1'000) == Motion::kReady);
}

TEST_CASE("Repos : le mode PK met en garde au sortir de n'importe quel geste", "[app][posture]") {
  ro::sim::Actor healer = Novice(kHeal);
  REQUIRE(ro::render::SkillUseStateFor(kHeal) == 7);
  CHECK(moonclient::RestMotionAt(healer, MotionEvents{}, kStartMs + 1'000) == Motion::kIdle);
  healer.pk_mode = true;
  CHECK(moonclient::RestMotionAt(healer, MotionEvents{}, kStartMs + 1'000) == Motion::kReady);
}

TEST_CASE("Pose menée : le coup de pied sauté tient son élan, puis se repose en garde",
          "[app][posture]") {
  // 🔴 L'EFFET NE FORCE PAS D'ANIMATION : à son cinquième tic il écrit l'état
  // vingt-sept et mène l'image cinq du rang douze, qui reste après ses
  // vingt-cinq millisecondes ; à son trente-cinquième il écrit l'état deux, que
  // le client voit fini — et l'acteur se repose, en garde.
  constexpr std::uint32_t kJumpKick = 457;
  ro::sim::Actor kicker = Novice(kBash);
  kicker.skill_use.reset();
  const std::array<ro::render::ActorEffectPlay, 1> rising = {
      ro::render::ActorEffectPlay{kJumpKick, 10}};
  const MotionEvents rising_events{std::nullopt, rising, kStartMs};
  const auto elan =
      LedPoseAt(nullptr, kicker, SpriteLayout::kPlayer, Motion::kIdle, rising_events, kStartMs);
  REQUIRE(elan.has_value());
  CHECK(elan->rank == 12);
  CHECK(elan->frame == 5);
  CHECK(moonclient::RestMotionAt(kicker, rising_events, kStartMs) == Motion::kIdle);
  // Et la blessure n'a pas lieu tant que la pose est minutée : vingt-cinq
  // millisecondes après son tic.
  const std::array<ro::render::ActorEffectPlay, 1> just_posed = {
      ro::render::ActorEffectPlay{kJumpKick, 5}};
  const MotionEvents just_posed_events{std::nullopt, just_posed, kStartMs};
  CHECK(LedPoseHoldsAt(nullptr, kicker, SpriteLayout::kPlayer, Motion::kIdle, just_posed_events,
                       kStartMs));

  const std::array<ro::render::ActorEffectPlay, 1> landed = {
      ro::render::ActorEffectPlay{kJumpKick, 40}};
  const MotionEvents landed_events{std::nullopt, landed, kStartMs};
  CHECK_FALSE(
      LedPoseAt(nullptr, kicker, SpriteLayout::kPlayer, Motion::kIdle, landed_events, kStartMs)
          .has_value());
  CHECK(moonclient::RestMotionAt(kicker, landed_events, kStartMs) == Motion::kReady);
}

TEST_CASE("Pose menée : le coup d'une compétence d'état sept mène la pose du métier",
          "[app][posture]") {
  // 🔴 LE COUP POSE L'ÉTAT D'EMPLOI par la même motion que l'emploi annoncé :
  // la danseuse qui achève Cold Bolt tient l'image zéro de son rang douze
  // quatre cents millisecondes, comme à un soin.
  REQUIRE(ro::render::SkillUseStateFor(kColdBolt) == 7);
  ro::sim::Actor danseuse = Employant(kDanseuse, kHealSkill);
  danseuse.skill_use.reset();
  danseuse.attack = BlowOf(kColdBolt, kStartMs);
  const auto view = HeldPoseAt(danseuse, kStartMs + 100);
  REQUIRE(view.has_value());
  CHECK(view->rank == 12);
  CHECK(view->frame == 0);
  CHECK_FALSE(HeldPoseAt(danseuse, kStartMs + 400).has_value());
  // Bash frappe en état deux : aucune pose menée.
  danseuse.attack = BlowOf(kBash, kStartMs);
  CHECK_FALSE(HeldPoseAt(danseuse, kStartMs + 100).has_value());
  // Assise, l'emploi posé par le coup ne touche à rien.
  danseuse.attack = BlowOf(kColdBolt, kStartMs);
  danseuse.posture = ro::sim::Posture::kSitting;
  CHECK_FALSE(LedPoseAt(nullptr, danseuse, SpriteLayout::kPlayer, Motion::kSit, MotionEvents{},
                        kStartMs + 100)
                  .has_value());
}

TEST_CASE("Pose menée : un coup qui laisse la motion ne la termine pas", "[app][posture]") {
  // 🔴 LES ENCHAÎNEMENTS DU MOINE NE POSENT AUCUNE MOTION : le coup de Chain
  // Combo laisse la pose en cours, là où Bash, qui frappe, la remet à zéro.
  constexpr std::uint16_t kChainCombo = 272;
  REQUIRE_FALSE(ro::render::SkillHitMotionState(kChainCombo, 6).has_value());
  ro::sim::Actor combo = Employant(kNinja, kHealSkill);
  combo.attack = BlowOf(kChainCombo, kStartMs + 100);
  combo.attack->type = 6;
  CHECK(HeldPoseAt(combo, kStartMs + 200).has_value());

  ro::sim::Actor bashing = Employant(kNinja, kHealSkill);
  bashing.attack = BlowOf(kBash, kStartMs + 100);
  CHECK_FALSE(HeldPoseAt(bashing, kStartMs + 200).has_value());
}

TEST_CASE("Pose menée : au même instant, la pose de la compétence écrase celle du métier",
          "[app][posture]") {
  // 🔴 LE RÉPARTITEUR POSE L'ÉTAT D'EMPLOI EN TÊTE, puis la pose de la compétence
  // dans son aiguillage : la seconde remplace la première, qui ne revient pas
  // quand la seconde finit. Une pose d'annonce d'emploi ordinaire, plus courte
  // que la seconde du ninja, le montre.
  const ro::render::SkillHeldPose* brief = nullptr;
  for (const ro::render::SkillHeldPose& pose : ro::render::SkillHeldPoseTable()) {
    if (pose.damage_types == 0 && !pose.hold_from_combo_delay && pose.timed && pose.hold_ms > 0 &&
        pose.hold_ms < 900 && ro::render::SkillUseStateFor(pose.skill_id) == 7 &&
        ro::render::SkillHeldPoseOf(pose.skill_id) == &pose) {
      brief = &pose;
      break;
    }
  }
  REQUIRE(brief != nullptr);
  const ro::sim::Actor ninja = Employant(kNinja, brief->skill_id);
  const auto view = HeldPoseAt(ninja, kStartMs);
  REQUIRE(view.has_value());
  CHECK(view->rank == brief->play_rank);
  CHECK(view->frame == brief->first_frame);
  // Sans l'écrasement, le ninja montrerait son image cinq jusqu'à une seconde.
  CHECK_FALSE(HeldPoseAt(ninja, kStartMs + brief->hold_ms + 10).has_value());
}

TEST_CASE("Pose menée : le répartiteur de la case mène celle d'un piège du Windhawk",
          "[app][posture]") {
  // WH_DEEPBLINDTRAP : les images 0 à 2 du rang 3, puis l'image 2 du rang 12,
  // tenue au-delà de ses 500 ms jusqu'à la motion suivante. Rien ne la menait.
  constexpr std::uint16_t kDeepBlindTrap = 5331;
  constexpr std::uint16_t kNovice = 0;
  constexpr std::uint32_t kLongAfterMs = 5'000;
  ro::sim::Actor poseur = Employant(kNovice, kDeepBlindTrap);
  poseur.ground_skill = ro::sim::GroundSkillUse{
      .skill_id = kDeepBlindTrap, .level = 1, .cell = {.x = 3, .y = 3}, .started_ms = kStartMs};
  const auto start = HeldPoseAt(poseur, kStartMs);
  REQUIRE(start.has_value());
  CHECK(start->rank == 3);
  CHECK(start->frame == 0);
  const auto held_pose = HeldPoseAt(poseur, kStartMs + kLongAfterMs);
  REQUIRE(held_pose.has_value());
  CHECK(held_pose->rank == 12);
  CHECK(held_pose->frame == 2);
}

namespace {

constexpr std::uint16_t kPaladin = 4015;
constexpr std::uint16_t kKnight = 7;
constexpr std::uint16_t kLordKnight = 4008;
constexpr std::uint16_t kSpearQuicken = 258;
constexpr std::uint16_t kTwoHandQuicken = 60;
constexpr std::uint16_t kAuraBlade = 355;
constexpr std::uint16_t kGrandCross = 254;
// Quatre cents millisecondes : la pose que l'emploi ordinaire d'un paladin mène.
constexpr std::uint32_t kPaladinPoseMs = 400;

// UN CORPS DONT L'INCANTATION EST CELLE D'UN PALADIN : six images au rang
// douze, 576 ms, plus longue que la pose ; deux images aux autres rangs.
constexpr std::size_t kBodyRankCount = 13;
constexpr std::size_t kCastRank = 12;
constexpr std::size_t kCastFrames = 6;
constexpr std::size_t kOtherRankFrames = 2;

std::vector<std::uint8_t> LongCastBodyBytes() {
  ro::test_support::ActBuildOptions options;
  for (std::size_t rank = 0; rank < kBodyRankCount; ++rank) {
    const std::size_t frames = rank == kCastRank ? kCastFrames : kOtherRankFrames;
    for (std::size_t direction = 0; direction < ro::test_support::kDirectionCount; ++direction) {
      ro::test_support::ActActionSpec spec;
      for (std::size_t frame = 0; frame < frames; ++frame) {
        ro::test_support::ActFrameSpec frame_spec;
        ro::test_support::ActLayerSpec layer;
        layer.frame_index = static_cast<std::int32_t>(frame % kOtherRankFrames);
        frame_spec.layers.push_back(layer);
        frame_spec.anchors.push_back(ro::test_support::ActAnchorSpec{});
        spec.frames.push_back(std::move(frame_spec));
      }
      options.actions.push_back(std::move(spec));
      options.speeds.push_back(ro::test_support::kFrameSpeed);
    }
  }
  return ro::test_support::BuildAct(options);
}

struct LongCastBody {
  LongCastBody()
      : sprite(*ro::formats::SprSprite::Parse(ro::test_support::SpriteBytes())),
        animation(*ro::formats::ActAnimation::Parse(LongCastBodyBytes())),
        outfit(DressedNovice::Pieces(sprite, animation)) {}

  std::optional<moonclient::PlayedMotion> UseAt(const ro::sim::Actor& actor, std::uint32_t at_ms) {
    return moonclient::SkillUsePostureAt(nullptr, actor, outfit, SpriteLayout::kPlayer,
                                         Motion::kIdle, MotionEvents{}, at_ms);
  }

  ro::formats::SprSprite sprite;
  ro::formats::ActAnimation animation;
  ro::render::ActorOutfit outfit;
};

}  // namespace

TEST_CASE("Pose menée : Spear Quicken d'un paladin tient l'image zéro, puis revient au repos",
          "[app][posture]") {
  // 🔴 À L'ÉCHÉANCE DE LA POSE, LE CLIENT DÉCLARE L'EMPLOI FINI et repose
  // aussitôt l'attente : la fin du rang d'incantation ne se joue pas. Nous
  // reprenions la passe là où la pose l'avait laissée, et le paladin jouait
  // les images quatre et cinq que le jeu ne montre pas.
  REQUIRE(ro::render::SkillUseStateFor(kSpearQuicken) == 7);
  REQUIRE(ro::render::SkillHeldPoseOf(kSpearQuicken) == nullptr);
  const std::optional<ro::render::SkillHeldPose> job_pose = ro::render::UseLedPoseOf(kPaladin);
  REQUIRE(job_pose.has_value());
  REQUIRE(job_pose->hold_ms == kPaladinPoseMs);
  LongCastBody body;
  REQUIRE(body.outfit.AnimationDurationMs(kCastRank) > static_cast<float>(kPaladinPoseMs));

  ro::sim::Actor paladin = Employant(kPaladin, kSpearQuicken);
  const auto posed = HeldPoseAt(paladin, kStartMs + 100);
  REQUIRE(posed.has_value());
  CHECK(posed->rank == kCastRank);
  CHECK(posed->frame == 0);
  CHECK(body.UseAt(paladin, kStartMs + kPaladinPoseMs - 1).has_value());

  for (const std::uint32_t after_ms : {kPaladinPoseMs, kPaladinPoseMs + 50, kPaladinPoseMs + 150}) {
    CHECK_FALSE(HeldPoseAt(paladin, kStartMs + after_ms).has_value());
    CHECK_FALSE(body.UseAt(paladin, kStartMs + after_ms).has_value());
    CHECK(moonclient::RestMotionAt(paladin, MotionEvents{}, kStartMs + after_ms) == Motion::kIdle);
  }
  // En mode PK, le repos qui suit est la garde.
  paladin.pk_mode = true;
  CHECK(moonclient::RestMotionAt(paladin, MotionEvents{}, kStartMs + kPaladinPoseMs) ==
        Motion::kReady);
}

TEST_CASE("Pose menée : sans pose plus courte que lui, l'emploi joue sa passe entière",
          "[app][posture]") {
  // Le chevalier n'a pas de pose de métier : Two-Hand Quicken déroule tout son
  // rang d'incantation, puis se repose.
  LongCastBody body;
  const float pass_ms = body.outfit.AnimationDurationMs(kCastRank);
  REQUIRE_FALSE(ro::render::UseLedPoseOf(kKnight).has_value());
  const ro::sim::Actor knight = Employant(kKnight, kTwoHandQuicken);
  const std::optional<moonclient::PlayedMotion> late = body.UseAt(knight, kStartMs + 500);
  REQUIRE(late.has_value());
  CHECK(late->animation == kCastRank);
  CHECK(late->elapsed_ms == 500.0F);
  CHECK_FALSE(body.UseAt(knight, kStartMs + static_cast<std::uint32_t>(pass_ms)).has_value());

  // Aura Blade mène une pose d'une seconde, plus longue que la passe : rien ne
  // change avant la fin de celle-ci.
  const ro::render::SkillHeldPose* aura = ro::render::SkillHeldPoseOf(kAuraBlade);
  REQUIRE(aura != nullptr);
  REQUIRE(static_cast<float>(aura->hold_ms) >= pass_ms);
  const ro::sim::Actor lord = Employant(kLordKnight, kAuraBlade);
  CHECK(body.UseAt(lord, kStartMs + 500).has_value());
}

TEST_CASE("Pose menée : le coup d'état sept d'un paladin finit à l'échéance de sa pose",
          "[app][posture]") {
  // Le coup pose l'emploi ordinaire par la même motion : même pose, même fin.
  REQUIRE(ro::render::SkillUseStateFor(kGrandCross) == 7);
  ro::sim::Actor paladin = Employant(kPaladin, kSpearQuicken);
  paladin.skill_use.reset();
  paladin.attack = BlowOf(kGrandCross, kStartMs);
  CHECK_FALSE(moonclient::StrikeFinishedByLedPose(paladin, SpriteLayout::kPlayer, Motion::kIdle,
                                                  kStartMs + kPaladinPoseMs - 1));
  CHECK(moonclient::StrikeFinishedByLedPose(paladin, SpriteLayout::kPlayer, Motion::kIdle,
                                            kStartMs + kPaladinPoseMs));
  // Assis, le coup ne pose rien, et ne finit donc rien.
  CHECK_FALSE(moonclient::StrikeFinishedByLedPose(paladin, SpriteLayout::kPlayer, Motion::kSit,
                                                  kStartMs + kPaladinPoseMs));
  // Sans pose de métier, ou pour un coup d'état deux sans pose d'impact, la
  // passe du coup va à son terme.
  ro::sim::Actor knight = paladin;
  knight.look.job = kKnight;
  CHECK_FALSE(moonclient::StrikeFinishedByLedPose(knight, SpriteLayout::kPlayer, Motion::kIdle,
                                                  kStartMs + 5'000));
  paladin.attack = BlowOf(kBash, kStartMs);
  CHECK_FALSE(moonclient::StrikeFinishedByLedPose(paladin, SpriteLayout::kPlayer, Motion::kIdle,
                                                  kStartMs + 5'000));
}

TEST_CASE("Blade Stop : le moine garde sa pose après le relâchement, jusqu'à sa motion suivante",
          "[app][posture]") {
  ro::sim::Actor monk;
  monk.id = 1;
  monk.kind = ro::sim::ActorKind::kPlayer;
  monk.blade_stop_caster = true;
  monk.blade_stop_since_ms = kStartMs;
  const auto held = moonclient::BladeStopPoseAt(nullptr, monk, SpriteLayout::kPlayer, Motion::kIdle,
                                                MotionEvents{}, kStartMs + 100);
  REQUIRE(held.has_value());
  CHECK(held->rank == ro::render::kBladeStopHeldAnimation);
  CHECK(held->frame == ro::render::kBladeStopHeldFrame);
  // Tant que la prise dure, la blessure n'a pas lieu.
  CHECK(LedPoseHoldsAt(nullptr, monk, SpriteLayout::kPlayer, Motion::kIdle, {}, kStartMs + 100));

  // 🔴 RELÂCHÉ, IL GARDE L'IMAGE : l'échéance ne la lui ôte pas.
  constexpr std::uint32_t kReleasedMs = kStartMs + 5000;
  monk.blade_stop_since_ms.reset();
  monk.blade_stop_released_ms = kReleasedMs;
  CHECK(moonclient::BladeStopPoseAt(nullptr, monk, SpriteLayout::kPlayer, Motion::kIdle,
                                    MotionEvents{}, kReleasedMs + 3000)
            .has_value());
  CHECK_FALSE(
      LedPoseHoldsAt(nullptr, monk, SpriteLayout::kPlayer, Motion::kIdle, {}, kReleasedMs + 10));

  // Sa motion suivante — ici, une marche qui part — la termine.
  monk.movement = ro::sim::CellMovement{};
  monk.movement_started_ms = kReleasedMs + 4000;
  CHECK(moonclient::BladeStopPoseAt(nullptr, monk, SpriteLayout::kPlayer, Motion::kIdle,
                                    MotionEvents{}, kReleasedMs + 3999)
            .has_value());
  CHECK_FALSE(moonclient::BladeStopPoseAt(nullptr, monk, SpriteLayout::kPlayer, Motion::kIdle,
                                          MotionEvents{}, kReleasedMs + 4000)
                  .has_value());
}

TEST_CASE("État du coup : le paquet des dégâts donne ensuite le sien au lanceur",
          "[app][posture]") {
  // Comet de NPC et de Warlock, Frost Misty : l'état sept ; Death Bound : l'attaque.
  constexpr std::uint16_t kNpcComet = 708;
  constexpr std::uint16_t kDeathBound = 2003;
  constexpr std::uint16_t kFrostMisty = 2203;
  ro::sim::Strike strike;
  strike.type = 8;
  strike.skill_id = kNpcComet;
  CHECK(moonclient::StrikeState(strike) == std::optional<std::int8_t>{std::int8_t{7}});
  strike.skill_id = kDeathBound;
  CHECK(moonclient::StrikeState(strike) == std::optional<std::int8_t>{std::int8_t{2}});
  REQUIRE(ro::render::NotifySkillCasterStateOf(kFrostMisty).has_value());
  CHECK_FALSE(ro::render::NotifySkillCasterStateOf(kFrostMisty)->needs_target);
  // Une compétence ordinaire garde l'état de son coup.
  strike.skill_id = kBash;
  CHECK(moonclient::StrikeState(strike) == ro::render::SkillHitMotionState(kBash, 8));
  // Une unité au sol n'est pas un lanceur pour ce paquet.
  strike.skill_id = kNpcComet;
  strike.by_ground_unit = true;
  CHECK(moonclient::StrikeState(strike) == ro::render::SkillHitMotionState(kNpcComet, 8));
}
