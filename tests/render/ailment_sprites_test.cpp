// CE QUE LE GEL, L'ÉTOURDISSEMENT ET L'AVEUGLEMENT FONT JOUER À LEUR CHANGEMENT.
//
// 🔴 TROIS PANNES MUETTES EN JEU : une glace qui n'éclate pas au dégel, un noir
// qui ne tombe pas sur le joueur aveuglé, un bruit qui manque. Rien n'y échoue.

#include "ro/render/ailment_sprites.h"

#include <algorithm>
#include <cstdint>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

namespace {

using ro::render::AilmentChange;
using ro::render::AilmentReactions;
using ro::render::ReactToAilments;

[[nodiscard]] bool Sounds(const AilmentReactions& reactions, std::string_view sound) {
  return std::ranges::find(reactions.sounds, sound) != reactions.sounds.end();
}

[[nodiscard]] bool Poses(const AilmentReactions& reactions, std::uint32_t effect_id) {
  return std::ranges::find(reactions.posed_effects, effect_id) != reactions.posed_effects.end();
}

[[nodiscard]] bool Lifts(const AilmentReactions& reactions, std::uint32_t effect_id) {
  return std::ranges::find(reactions.lifted_effects, effect_id) != reactions.lifted_effects.end();
}

}  // namespace

TEST_CASE("Gel : entrer fait le bruit de la pierre, sortir fait éclater la glace",
          "[render][ailment]") {
  const AilmentReactions frozen = ReactToAilments(
      AilmentChange{.body_before = 0, .body_after = ro::render::kAilmentBodyFrozen, .live = true});
  CHECK(Sounds(frozen, ro::render::kFreezeSound));
  CHECK_FALSE(frozen.shatter);

  // ⚠ LE DÉGEL ÉCLATE MÊME HORS DU DIRECT : c'est la transition qui compte.
  const AilmentReactions thawed = ReactToAilments(
      AilmentChange{.body_before = ro::render::kAilmentBodyFrozen, .body_after = 0, .live = false});
  CHECK(thawed.shatter);
  CHECK(Sounds(thawed, ro::render::kThawSound));

  // Une apparition déjà gelée ne fait pas de bruit.
  const AilmentReactions seen_frozen = ReactToAilments(
      AilmentChange{.body_before = 0, .body_after = ro::render::kAilmentBodyFrozen, .live = false});
  CHECK(seen_frozen.sounds.empty());

  // Un changement qui laisse le corps gelé ne rejoue rien.
  const AilmentReactions still =
      ReactToAilments(AilmentChange{.body_before = ro::render::kAilmentBodyFrozen,
                                    .body_after = ro::render::kAilmentBodyFrozen,
                                    .live = true});
  CHECK(still.sounds.empty());
  CHECK_FALSE(still.shatter);
}

TEST_CASE("Étourdissement : son bruit en direct, à l'entrée seulement", "[render][ailment]") {
  const AilmentReactions stunned = ReactToAilments(
      AilmentChange{.body_before = 0, .body_after = ro::render::kAilmentBodyStunned, .live = true});
  CHECK(Sounds(stunned, ro::render::kStunSound));
  const AilmentReactions recovered = ReactToAilments(
      AilmentChange{.body_before = ro::render::kAilmentBodyStunned, .body_after = 0, .live = true});
  CHECK(recovered.sounds.empty());
}

TEST_CASE("Aveuglement : le noir tombe sur le seul joueur, et se lève avec lui",
          "[render][ailment]") {
  // Un autre aveuglé ne fait qu'un bruit.
  const AilmentReactions other =
      ReactToAilments(AilmentChange{.health_after = ro::render::kAilmentHealthBlind, .live = true});
  CHECK(Sounds(other, ro::render::kBlindSound));
  CHECK_FALSE(Poses(other, ro::render::kBlindDarknessEffect));

  const AilmentReactions blinded = ReactToAilments(AilmentChange{
      .health_after = ro::render::kAilmentHealthBlind, .live = true, .own_player = true});
  CHECK(Poses(blinded, ro::render::kBlindDarknessEffect));
  CHECK(blinded.blind_seen);

  // ⚠ DÉJÀ PORTÉ, LE NOIR NE SE REPOSE PAS : un second changement d'état
  // l'empilerait.
  const AilmentReactions again =
      ReactToAilments(AilmentChange{.health_after = ro::render::kAilmentHealthBlind,
                                    .live = true,
                                    .own_player = true,
                                    .darkness_carried = true,
                                    .blind_seen = true});
  CHECK_FALSE(Poses(again, ro::render::kBlindDarknessEffect));
  CHECK(Sounds(again, ro::render::kBlindSound));

  const AilmentReactions cured =
      ReactToAilments(AilmentChange{.live = true, .own_player = true, .blind_seen = true});
  CHECK(Lifts(cured, ro::render::kBlindDarknessEffect));
  CHECK_FALSE(cured.blind_seen);

  // 🔴 L'AVEUGLEMENT PROFOND GARDE SON NOIR.
  const AilmentReactions deep = ReactToAilments(
      AilmentChange{.live = true, .own_player = true, .deep_blind_held = true, .blind_seen = true});
  CHECK_FALSE(Lifts(deep, ro::render::kBlindDarknessEffect));
  CHECK(deep.blind_seen);

  // Un noir que l'aveuglement ordinaire n'a pas posé ne se lève pas ici.
  const AilmentReactions never = ReactToAilments(AilmentChange{.live = true, .own_player = true});
  CHECK_FALSE(Lifts(never, ro::render::kBlindDarknessEffect));
}

TEST_CASE("Poison : la brume tombe sur le seul joueur, mortel ou non, et se lève avec lui",
          "[render][ailment]") {
  const AilmentReactions other = ReactToAilments(
      AilmentChange{.health_after = ro::render::kAilmentHealthPoison, .live = true});
  CHECK(Sounds(other, ro::render::kPoisonSound));
  CHECK_FALSE(Poses(other, ro::render::kPoisonMistEffect));

  const AilmentReactions deadly = ReactToAilments(AilmentChange{
      .health_after = ro::render::kAilmentHealthDeadlyPoison, .live = true, .own_player = true});
  CHECK(Sounds(deadly, ro::render::kPoisonSound));
  CHECK(Poses(deadly, ro::render::kPoisonMistEffect));
  CHECK(deadly.poison_seen);

  // Hors du direct, ni bruit ni brume : la brume ne part qu'au changement reçu.
  const AilmentReactions seen = ReactToAilments(
      AilmentChange{.health_after = ro::render::kAilmentHealthPoison, .own_player = true});
  CHECK(seen.sounds.empty());
  CHECK(seen.posed_effects.empty());
  CHECK(seen.poison_seen);

  const AilmentReactions carried =
      ReactToAilments(AilmentChange{.health_after = ro::render::kAilmentHealthPoison,
                                    .live = true,
                                    .own_player = true,
                                    .poison_mist_carried = true,
                                    .poison_seen = true});
  CHECK_FALSE(Poses(carried, ro::render::kPoisonMistEffect));

  const AilmentReactions cured =
      ReactToAilments(AilmentChange{.live = true, .own_player = true, .poison_seen = true});
  CHECK(Lifts(cured, ro::render::kPoisonMistEffect));
  CHECK_FALSE(cured.poison_seen);
}

TEST_CASE("Malédiction et confusion : un bruit à chaque changement reçu en direct",
          "[render][ailment]") {
  const AilmentReactions both = ReactToAilments(
      AilmentChange{.health_after = static_cast<std::uint16_t>(ro::render::kAilmentHealthCurse |
                                                               ro::render::kAilmentHealthConfusion),
                    .live = true});
  CHECK(Sounds(both, ro::render::kCurseSound));
  CHECK(Sounds(both, ro::render::kConfusionSound));
  const AilmentReactions quiet = ReactToAilments(
      AilmentChange{.health_after = ro::render::kAilmentHealthCurse, .live = false});
  CHECK(quiet.sounds.empty());
}

TEST_CASE("Brûlure et emprisonnement : l'effet se pose à l'entrée, part à la sortie",
          "[render][ailment]") {
  // ⚠ HORS DU DIRECT AUSSI : c'est la transition qui compte.
  const AilmentReactions burning = ReactToAilments(
      AilmentChange{.body_before = 0, .body_after = ro::render::kAilmentBodyBurning});
  CHECK(Poses(burning, ro::render::kBurningEffect));
  const AilmentReactions quenched = ReactToAilments(
      AilmentChange{.body_before = ro::render::kAilmentBodyBurning, .body_after = 0});
  CHECK(Lifts(quenched, ro::render::kBurningEffect));

  const AilmentReactions jailed = ReactToAilments(
      AilmentChange{.body_before = 0, .body_after = ro::render::kAilmentBodyImprisoned});
  CHECK(Poses(jailed, ro::render::kImprisonEffect));
  // Déjà porteur de sa prison, il ne la reprend pas.
  const AilmentReactions still_jailed =
      ReactToAilments(AilmentChange{.body_before = 0,
                                    .body_after = ro::render::kAilmentBodyImprisoned,
                                    .imprison_carried = true});
  CHECK_FALSE(Poses(still_jailed, ro::render::kImprisonEffect));
  const AilmentReactions freed = ReactToAilments(
      AilmentChange{.body_before = ro::render::kAilmentBodyImprisoned, .body_after = 0});
  CHECK(Lifts(freed, ro::render::kImprisonEffect));
}

TEST_CASE("Marque du maudit : son action est la face d'écran, devant le dos, derrière la face",
          "[render][ailment]") {
  const ro::render::CurseMarkPlacement facing_camera = ro::render::CurseMarkFor(0);
  CHECK(facing_camera.action == 0);
  CHECK(facing_camera.depth_bias < 0.0F);
  const ro::render::CurseMarkPlacement back = ro::render::CurseMarkFor(4);
  CHECK(back.action == 4);
  CHECK(back.depth_bias > 0.0F);
  CHECK(ro::render::CurseMarkFor(2).depth_bias > 0.0F);
  CHECK(ro::render::CurseMarkFor(5).depth_bias > 0.0F);
  CHECK(ro::render::CurseMarkFor(6).depth_bias < 0.0F);
  CHECK(ro::render::CurseMarkFor(1).depth_bias < 0.0F);
}

TEST_CASE("Teinte : la malédiction, puis le poison mortel, puis le poison, puis le corps",
          "[render][ailment]") {
  using ro::render::AilmentTint;
  CHECK(AilmentTint(ro::render::kAilmentBodyFrozen, 0, false) == ro::render::kFrozenTint);
  CHECK(AilmentTint(ro::render::kAilmentBodyFrozen, ro::render::kAilmentHealthPoison, false) ==
        ro::render::kPoisonTint);
  CHECK(AilmentTint(0,
                    static_cast<std::uint16_t>(ro::render::kAilmentHealthPoison |
                                               ro::render::kAilmentHealthDeadlyPoison),
                    false) == ro::render::kDeadlyPoisonTint);
  CHECK(AilmentTint(0,
                    static_cast<std::uint16_t>(ro::render::kAilmentHealthCurse |
                                               ro::render::kAilmentHealthDeadlyPoison),
                    false) == ro::render::kCurseTint);
  // ⚠ L'EFFET QUI TEINT DÉJÀ garde sa couleur contre le poison ordinaire.
  CHECK_FALSE(AilmentTint(0, ro::render::kAilmentHealthPoison, true).has_value());
  CHECK(AilmentTint(ro::render::kAilmentBodyStone, ro::render::kAilmentHealthPoison, true) ==
        ro::render::kStoneTint);
  CHECK_FALSE(AilmentTint(0, 0, false).has_value());
}

TEST_CASE("Teinte qui reste : le gelé guéri du poison garde le poison jusqu'au dégel",
          "[render][ailment]") {
  using ro::render::AilmentTintLatch;
  AilmentTintLatch latch;
  ro::render::LatchStateChange(latch, 0, ro::render::kAilmentBodyFrozen, 0, false);
  CHECK(ro::render::LatchedTint(latch) == ro::render::kFrozenTint);
  ro::render::LatchStateChange(latch, ro::render::kAilmentBodyFrozen,
                               ro::render::kAilmentBodyFrozen, ro::render::kAilmentHealthPoison,
                               false);
  CHECK(ro::render::LatchedTint(latch) == ro::render::kPoisonTint);
  // 🔴 LA GUÉRISON N'ÉCRIT RIEN : le corps gelé retient le blanchiment.
  ro::render::LatchStateChange(latch, ro::render::kAilmentBodyFrozen,
                               ro::render::kAilmentBodyFrozen, 0, false);
  CHECK(ro::render::LatchedTint(latch) == ro::render::kPoisonTint);
  // Le dégel blanchit.
  ro::render::LatchStateChange(latch, ro::render::kAilmentBodyFrozen, 0, 0, false);
  CHECK_FALSE(ro::render::LatchedTint(latch).has_value());
}

TEST_CASE("Teinte qui reste : un handicap teint à sa pose, et sa couleur survit à sa levée",
          "[render][ailment]") {
  using ro::render::AilmentTintLatch;
  AilmentTintLatch latch;
  // Deadly Poison, troisième des quatre.
  ro::render::LatchHandicap(latch, 2, true);
  CHECK(ro::render::LatchedTint(latch) == ro::render::kTintingHandicapTints[2]);
  // Un changement d'état pendant le handicap ne blanchit pas.
  ro::render::LatchStateChange(latch, 0, 0, 0, false);
  CHECK(ro::render::LatchedTint(latch) == ro::render::kTintingHandicapTints[2]);
  // Sa levée n'écrit rien ; le changement d'état suivant blanchit.
  ro::render::LatchHandicap(latch, 2, false);
  CHECK(ro::render::LatchedTint(latch) == ro::render::kTintingHandicapTints[2]);
  ro::render::LatchStateChange(latch, 0, 0, 0, false);
  CHECK_FALSE(ro::render::LatchedTint(latch).has_value());
}

TEST_CASE("Teinte qui reste : Blade Stop grise, retient le blanchiment, et blanchit au relâchement",
          "[render][ailment]") {
  using ro::render::AilmentTintLatch;
  AilmentTintLatch latch;
  ro::render::LatchBladeStop(latch, true);
  CHECK(ro::render::LatchedTint(latch) == ro::render::kBladeStopTint);
  ro::render::LatchStateChange(latch, 0, 0, 0, false);
  CHECK(ro::render::LatchedTint(latch) == ro::render::kBladeStopTint);
  ro::render::LatchBladeStop(latch, false);
  CHECK_FALSE(ro::render::LatchedTint(latch).has_value());
}

TEST_CASE("Teinte qui reste : le poison que l'effet 812 couvre n'écrit rien, mais retient",
          "[render][ailment]") {
  using ro::render::AilmentTintLatch;
  AilmentTintLatch latch;
  ro::render::LatchStateChange(latch, 0, 0, ro::render::kAilmentHealthCurse, false);
  CHECK(ro::render::LatchedTint(latch) == ro::render::kCurseTint);
  // La malédiction tombe, le poison couvert tient : la teinte d'avant reste.
  ro::render::LatchStateChange(latch, 0, 0, ro::render::kAilmentHealthPoison, true);
  CHECK(ro::render::LatchedTint(latch) == ro::render::kCurseTint);
}
