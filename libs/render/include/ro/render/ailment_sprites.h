#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "ro/render/sleep_bubbles.h"

// CE QUE LES ALTÉRATIONS FONT PARAÎTRE ET ENTENDRE : gel, étourdissement,
// brûlure, emprisonnement, poison, malédiction, confusion, aveuglement.
//
// Toutes arrivent en BITS, dans le changement d'état : les quatre premières
// sont des valeurs de l'état du corps, les autres des bits de l'état de santé.
// Aucun paquet d'état ne les annonce ; c'est au client de traduire ces bits en
// images.
//
// ⚠ DEUX ÉTAGES : ce qui TIENT tant que l'état dure — le bloc de glace, les
// étoiles, la marque du maudit, la teinte, le noir et la brume du joueur —, et
// ce qui se joue UNE FOIS au changement — les bruits, l'éclatement de la glace.
//
// ⚠ ANGELUS N'A RIEN À MONTRER. Son bit fait naître un sprite-enfant qui ne se
// dessine jamais et ne fait que signaler son porteur, lequel ignore le signal.
namespace ro::render {

// ── LE BLOC DE GLACE D'UN GELÉ ──────────────────────────────────────────────
//
// Un sprite-enfant collé aux PIEDS de son porteur, sans décalage, qui tourne
// sans fin à la cadence de son fichier. ⚠ IL SE PEINT DANS LA SCÈNE, éprouvé en
// profondeur, et non en avant de tout comme les bulles : un mur devant le gelé
// cache sa glace. Il passe devant le corps qu'il enferme de cinq pas de calque.
inline constexpr std::string_view kFrozenIceSpritePath = "data\\sprite\\이팩트\\얼음땡";
inline constexpr std::size_t kFrozenIceAction = 0;
inline constexpr float kFrozenIceRiseUnits = 0.0F;

// Le pas de profondeur qui sépare deux calques d'un sprite, en inverse de
// distance, et combien de ces pas la glace prend d'avance sur le corps.
inline constexpr float kLayerDepthStep = 9.0e-6F;
inline constexpr float kFrozenIceLeadLayers = 5.0F;
inline constexpr float kFrozenIceDepthBias = kLayerDepthStep * kFrozenIceLeadLayers;

// ── L'ÉCLATEMENT DE LA GLACE ────────────────────────────────────────────────
//
// Au dégel, la seconde action du même fichier, jouée une fois là où le gelé se
// tenait — elle ne le suit pas —, une image toutes les « 1 / 1,2 » pas de
// cadence : le même rythme que la bouffée de fumée.
inline constexpr std::size_t kFrozenShatterAction = 1;
inline constexpr float kFrozenShatterFrameSteps = 1.0F / 1.2F;

// ── LES ÉTOILES D'UN ÉTOURDI ────────────────────────────────────────────────
//
// La même tenue que les bulles du sommeil : vingt unités au-dessus des pieds,
// en avant de toute la scène, en boucle à la cadence du fichier.
inline constexpr std::string_view kStunStarsSpritePath = "data\\sprite\\이팩트\\Status-stun";
inline constexpr std::size_t kStunStarsAction = 0;
inline constexpr float kStunStarsRiseUnits = kSleepBubblesRiseUnits;

// ── LE NOIR DE L'AVEUGLÉ ────────────────────────────────────────────────────
//
// L'effet qui assombrit l'écran autour du joueur aveuglé. ⚠ SUR LE SEUL JOUEUR :
// un autre aveuglé ne fait qu'un bruit.
inline constexpr std::uint32_t kBlindDarknessEffect = 334;

// ── LA BRUME DE L'EMPOISONNÉ ────────────────────────────────────────────────
//
// Sur le seul joueur, comme le noir de l'aveuglé : le poison et le poison
// mortel la posent, et elle se lève quand ni l'un ni l'autre ne tient plus.
inline constexpr std::uint32_t kPoisonMistEffect = 335;

// ── LA BRÛLURE ET L'EMPRISONNEMENT ──────────────────────────────────────────
//
// Deux effets que l'état du corps pose sur son porteur en y entrant, et retire
// en le quittant. ⚠ LA BRÛLURE SE REPOSE À CHAQUE ENTRÉE ; l'emprisonnement,
// seulement quand le porteur ne l'a pas déjà.
inline constexpr std::uint32_t kBurningEffect = 790;
inline constexpr std::uint32_t kImprisonEffect = 802;

// ── LA MARQUE DU MAUDIT ─────────────────────────────────────────────────────
//
// Un sprite-enfant QUINZE unités au-dessus des pieds, à demi transparent, qui
// tourne sans fin à la cadence de son fichier. Son action est la DIRECTION
// D'ÉCRAN du corps qu'il marque — une par face, huit en tout —, l'angle du corps
// relatif à la caméra TRONQUÉ au huitième. Il se peint dans la scène, éprouvé en
// profondeur : DEVANT le corps de trois pas de calque quand celui-ci montre son
// dos ou son profil gauche (faces deux à cinq), DERRIÈRE sinon.
inline constexpr std::string_view kCurseMarkSpritePath = "data\\sprite\\이팩트\\Status-curse";
inline constexpr float kCurseMarkRiseUnits = 15.0F;
inline constexpr std::uint8_t kCurseMarkAlpha = 127;
inline constexpr float kCurseMarkLeadLayers = 3.0F;
inline constexpr std::size_t kCurseMarkFrontFirstDirection = 2;
inline constexpr std::size_t kCurseMarkFrontLastDirection = 5;

// L'action et l'avance de la marque, pour un corps vu de cette face d'écran.
struct CurseMarkPlacement {
  std::size_t action = 0;
  float depth_bias = 0.0F;
};
[[nodiscard]] CurseMarkPlacement CurseMarkFor(std::size_t screen_direction) noexcept;

// Les valeurs de l'état du corps, et les bits de santé, que ces règles lisent.
inline constexpr std::uint16_t kAilmentBodyStone = 1;
inline constexpr std::uint16_t kAilmentBodyFrozen = 2;
inline constexpr std::uint16_t kAilmentBodyStunned = 3;
inline constexpr std::uint16_t kAilmentBodyBurning = 7;
inline constexpr std::uint16_t kAilmentBodyImprisoned = 8;
inline constexpr std::uint16_t kAilmentHealthPoison = 0x01;
inline constexpr std::uint16_t kAilmentHealthCurse = 0x02;
inline constexpr std::uint16_t kAilmentHealthConfusion = 0x08;
inline constexpr std::uint16_t kAilmentHealthBlind = 0x10;
inline constexpr std::uint16_t kAilmentHealthDeadlyPoison = 0x80;

// Les bruits. ⚠ ENTRER DANS LE GEL fait le bruit de la pétrification : c'est
// le fichier que le client joue, et non une erreur de relevé.
inline constexpr std::string_view kFreezeSound = "_stonecurse.wav";
inline constexpr std::string_view kThawSound = "_frozen_explosion.wav";
inline constexpr std::string_view kStoneBreakSound = "_stone_explosion.wav";
inline constexpr std::string_view kStunSound = "_stun.wav";
inline constexpr std::string_view kPoisonSound = "_poison.wav";
inline constexpr std::string_view kCurseSound = "_curse.wav";
inline constexpr std::string_view kConfusionSound = "_confusion.wav";
inline constexpr std::string_view kBlindSound = "_blind.wav";

// Ce qu'un acteur montrait, et ce qu'il montre après un changement d'état.
struct AilmentChange {
  std::uint16_t body_before = 0;
  std::uint16_t body_after = 0;
  std::uint16_t health_after = 0;
  // Vrai pour un changement d'état reçu en DIRECT ; faux pour ce qu'une
  // apparition ou une redite apporte. Les bruits de santé, ceux d'entrée du
  // corps, le noir et la brume du joueur n'y partent pas.
  bool live = false;
  // L'acteur est-il le joueur ?
  bool own_player = false;
  // Le joueur porte-t-il déjà le noir, la brume ? L'acteur, sa prison ? Le
  // client ne les repose pas.
  bool darkness_carried = false;
  bool poison_mist_carried = false;
  bool imprison_carried = false;
  // L'AVEUGLEMENT PROFOND tient-il le joueur ? Son propre noir survit alors à
  // la fin de l'aveuglement ordinaire.
  bool deep_blind_held = false;
  // L'aveuglement ordinaire, le poison, ont-ils été vus sur le joueur depuis
  // le dernier noir, la dernière brume levés ? C'est ce qui autorise à les lever.
  bool blind_seen = false;
  bool poison_seen = false;
};

// Ce que ce changement fait jouer.
struct AilmentReactions {
  // Dans l'ordre où le client les joue.
  std::vector<std::string_view> sounds;
  // La glace éclate aux pieds de l'acteur.
  bool shatter = false;
  // Les effets posés sur l'acteur, puis ceux qu'on lui retire.
  std::vector<std::uint32_t> posed_effects;
  std::vector<std::uint32_t> lifted_effects;
  // Ce que deviennent `AilmentChange::blind_seen` et `poison_seen`.
  bool blind_seen = false;
  bool poison_seen = false;
};

// 🔴 LES CHANGEMENTS DU CORPS NE COMPTENT QU'À LA TRANSITION : sortir du gel fait
// éclater la glace, même hors du direct. Les bits de santé, eux, se relisent à
// CHAQUE changement reçu en direct : un aveuglé refait son bruit quand une autre
// option bouge.
[[nodiscard]] AilmentReactions ReactToAilments(const AilmentChange& change);

// ── LA TEINTE QUE LA SANTÉ ET LE CORPS IMPOSENT ─────────────────────────────
//
// Rouge, vert, bleu, alpha. ⚠ LA SANTÉ PASSE DEVANT LE CORPS : le client écrit
// la teinte du corps à sa transition, puis celle du poison, puis celle de la
// malédiction, à chaque changement d'état. La malédiction l'emporte donc sur le
// poison mortel, qui l'emporte sur le poison, qui l'emporte sur le corps.
//
// ⚠ UN EMPOISONNÉ QUE L'EFFET 812 TEINT DÉJÀ garde la teinte de l'effet : le
// poison ordinaire ne l'écrase pas — le mortel, si.
inline constexpr std::array<std::uint8_t, 4> kStoneTint = {64, 64, 64, 255};
inline constexpr std::array<std::uint8_t, 4> kPetrifyingTint = {128, 128, 128, 255};
inline constexpr std::array<std::uint8_t, 4> kFrozenTint = {0, 128, 255, 255};
inline constexpr std::array<std::uint8_t, 4> kPoisonTint = {255, 128, 255, 255};
inline constexpr std::array<std::uint8_t, 4> kDeadlyPoisonTint = {255, 50, 255, 255};
inline constexpr std::array<std::uint8_t, 4> kCurseTint = {200, 50, 50, 255};
inline constexpr std::uint32_t kPoisonTintYieldingEffect = 812;
inline constexpr std::uint16_t kAilmentBodyPetrifying = 6;

[[nodiscard]] std::optional<std::array<std::uint8_t, 4>> AilmentTint(
    std::uint16_t body_state, std::uint16_t health_state, bool poison_tint_yields) noexcept;

// ── LA TEINTE QUI RESTE ─────────────────────────────────────────────────────
//
// 🔴 LE CLIENT NE RECALCULE PAS LA TEINTE, IL L'ÉCRIT, et ce qu'il a écrit
// reste. Un gelé guéri de son poison garde la teinte du poison jusqu'au dégel ;
// un corps qu'un handicap a teint la garde après la fin du handicap, jusqu'au
// changement d'état suivant qui la blanchit.
//
// Trois écrivains :
//   · le changement d'état, dans l'ordre : le corps QUITTÉ (le dégel blanchit),
//     le corps PRIS (pierre, gel, pétrification), le poison mortel ou le poison
//     (sauf sous l'effet 812), la malédiction ; puis il blanchit si RIEN ne
//     tient — ni poison, ni malédiction, ni pierre, gel ou pétrification, ni
//     Blade Stop, ni l'un des quatre handicaps teintants ;
//   · un handicap teintant qui se pose écrit sa couleur ; qui se lève n'écrit
//     rien — la couleur reste ;
//   · Blade Stop grise les deux corps à la prise et les blanchit au relâchement.
inline constexpr std::array<std::uint8_t, 4> kUntinted = {255, 255, 255, 255};
inline constexpr std::array<std::uint8_t, 4> kBladeStopTint = {64, 64, 64, 255};

// LA POSE DU MOINE QUI TIENT UNE LAME : l'image quatre de l'animation douze,
// menée sans échéance tant que la prise dure. Celui dont la lame est saisie,
// lui, se fige sur l'image qu'il montrait.
inline constexpr std::size_t kBladeStopHeldAnimation = 12;
inline constexpr std::size_t kBladeStopHeldFrame = 4;

// Les quatre handicaps qui teintent, et leur couleur : Holy Flame, Conflagration,
// Deadly Poison, Depression.
inline constexpr std::size_t kTintingHandicaps = 4;
inline constexpr std::array<std::uint16_t, kTintingHandicaps> kTintingHandicapStates = {1216, 1212,
                                                                                        1214, 1215};
inline constexpr std::array<std::array<std::uint8_t, 4>, kTintingHandicaps> kTintingHandicapTints =
    {{{255, 0, 0, 255}, {255, 0, 0, 255}, {170, 0, 170, 255}, {0, 0, 255, 255}}};

// Ce que l'acteur porte de teinte, et ce qui la retient de blanchir.
struct AilmentTintLatch {
  std::array<std::uint8_t, 4> tint = kUntinted;
  bool blade_stop = false;
  std::array<bool, kTintingHandicaps> handicaps{};
};

// UN CHANGEMENT D'ÉTAT, apparition comprise — le corps d'avant vaut alors zéro.
void LatchStateChange(AilmentTintLatch& latch, std::uint16_t body_before, std::uint16_t body_after,
                      std::uint16_t health_state, bool poison_tint_yields) noexcept;

// UN HANDICAP TEINTANT, de rang `slot` dans `kTintingHandicapStates`, posé ou levé.
void LatchHandicap(AilmentTintLatch& latch, std::size_t slot, bool active) noexcept;

// BLADE STOP, pris ou relâché.
void LatchBladeStop(AilmentTintLatch& latch, bool active) noexcept;

// La teinte à montrer, ou rien quand le corps est blanc : ses effets décident
// alors.
[[nodiscard]] std::optional<std::array<std::uint8_t, 4>> LatchedTint(
    const AilmentTintLatch& latch) noexcept;

}  // namespace ro::render
