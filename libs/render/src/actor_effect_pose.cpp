#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "ro/render/actor_effect_table.h"

namespace ro::render {
namespace {

// LES TROIS BITS DU MOT DE RENDU QUE CETTE POSE CONNAÎT.
//
// Le saut et la réception vont par paire : le premier monte le corps en
// l'effaçant, le second le ramène en le rallumant. `EF_JUMPBODY` lève l'un,
// `EF_LANDBODY` l'autre, chacun tant que son instance vit.
inline constexpr std::uint32_t kFadeOutRising = 0x10000;
inline constexpr std::uint32_t kFadeInFalling = 0x20000;
inline constexpr std::uint32_t kOffScreen = 0x100000;

// LE CORPS QUI TREMBLE — les quatre `EF_QUAKEBODY`.
//
// Le client déplace les quatre BORDS du quad, chacun d'un tirage indépendant :
// le corps reste un rectangle et ses côtés vibrent. Le tirage vaut
// `rand() % 11`, moins 5.
inline constexpr std::uint32_t kJitteringBody = 0x2000;

// LE CORPS QUI TOURNE SUR UN CERCLE — `EF_SPINEDBODY` et ses deux voisins.
//
// Le client déplace le corps entier sur un cercle de quarante pixels, à un
// angle tiré d'un compteur.
//
// ⚠ CE N'EST PAS `EF_SPINMOVE`, qui fait PIVOTER l'acteur sur lui-même sans le
// déplacer. Ici il ne se retourne pas sur place : il fait le tour de sa place.
//
// ⚠ ET CE BIT FAIT UNE SECONDE CHOSE QUE NOUS NE MONTRONS PAS. Le client
// écrase aussi l'ANGLE du quad par le même
// `180 − 180 cos(5c)` : le sprite VRILLE pendant qu'il fait le tour, et cesse de
// vriller passé le trente-sixième pas. `DrawnActor` n'a pas d'angle, et le lui
// donner demande de faire tourner les coins du panneau. Le déplacement se voit,
// la vrille manque — on le dit plutôt que de le taire.
inline constexpr std::uint32_t kSwingingBody = 0x40;

// LE CORPS SOULEVÉ — `EF_KICKEDBODY`, celui qu'on envoie en l'air.
//
// Le client déplace les DEUX bords du quad vers le haut d'une part de leur
// écart : le corps monte sans changer de taille.
//
// ⚠ ET LE CLIENT TESTE CE BIT PAR LE SIGNE DE SON OCTET, sans masque : c'est
// bien le bit de poids fort de l'octet, et il soulève le corps.
inline constexpr std::uint32_t kKickedBody = 0x80;

// LE CORPS ÉCRASÉ — `EF_PRESSEDBODY`.
//
// Le client rapproche le bord HAUT du quad de son bord bas : la hauteur tombe
// à quatre dixièmes en trois pas, y vibre trente pas, puis revient d'un coup.
inline constexpr std::uint32_t kPressedBody = 0x20;

// LES TROIS BITS QUI CHOISISSENT LE MÉLANGE DE CERTAINS CALQUES.
//
// Le client met le mode de dépôt à CINQ — le seul qui se traduise en
// `D3DBLEND_ONE` — sur les calques zéro à quatre pour le premier, sur le
// cinquième — l'arme — pour le second. Le troisième soumet l'arme une seconde
// fois, en addition, une image sur deux.
//
// ⚠ LA CAPE SUIT LE PREMIER : voir `kBodyAndHeadwearParts`.
inline constexpr std::uint32_t kGlowingBody = 0x1;
inline constexpr std::uint32_t kGlowingWeapon = 0x2;
inline constexpr std::uint32_t kDoubledWeapon = 0x4;

// Les champs de l'acteur que ces deux bits lisent. Un bit sans son champ ne
// montre rien : c'est le couple qui fait la variation (ADR 0024).
inline constexpr std::uint16_t kFadeOutField = 0xF4;
inline constexpr std::uint16_t kFadeInField = 0xF6;

// LA MONTÉE, en pixels par unité de compteur. Le client multiplie le compteur par
// vingt, puis par une échelle d'écran valant quatre cents sur la hauteur de la
// fenêtre. Nous travaillons en pixels de sprite, où cette échelle vaut un.
inline constexpr float kRisePixelsPerStep = 20.0F;

// CE QUE LE COMPTEUR RETIRE À L'ALPHA à chaque pas, pour le saut.
inline constexpr std::int32_t kFadeOutStep = 20;

// LA RÉCEPTION, qui est l'inverse et n'a pas les mêmes nombres : l'alpha monte
// de dix-sept par pas à partir du dixième, et la hauteur descend de vingt par
// pas jusqu'au vingt-cinquième.
inline constexpr std::int32_t kFadeInStep = 17;
inline constexpr std::int32_t kFadeInFirstStep = 10;
inline constexpr std::int32_t kFadeInLastStep = 25;

inline constexpr std::int32_t kOpaque = 255;

// L'AMPLITUDE DU TREMBLEMENT, en pixels de sprite, de part et d'autre.
inline constexpr float kJitterPixels = 5.0F;

// LA COURBE DU SOULÈVEMENT, en fraction de la hauteur du corps. Il monte par un
// sinus jusqu'au dix-huitième pas, tient trois dixièmes deux pas, puis redescend
// d'un dixième par pas jusqu'au vingt-troisième.
inline constexpr std::int32_t kLiftRisingLastStep = 18;
inline constexpr std::int32_t kLiftHeldLastStep = 20;
inline constexpr std::int32_t kLiftFallLastStep = 23;
inline constexpr float kLiftHighest = 0.3F;
inline constexpr float kLiftFallPerStep = 0.1F;
inline constexpr float kLiftDegreesPerStep = 5.0F;

// LES PALIERS DE L'ÉCRASEMENT. Le client descend d'un cinquième par pas jusqu'au
// troisième, tient autour de quatre dixièmes jusqu'au trente-troisième, puis rend
// sa hauteur au corps.
//
// ⚠ IL FAIT VIBRER CE PALIER de cinq centièmes au plus, d'un tirage par image.
// Nous tenons la valeur basse : `render` reste sans hasard, et cinq centièmes de
// hauteur ne se voient pas comme cinq pixels de tremblement se voient.
inline constexpr std::int32_t kPressedFallLastStep = 3;
inline constexpr std::int32_t kPressedHoldLastStep = 33;
inline constexpr float kPressedFallPerStep = 0.2F;
inline constexpr float kPressedHeld = 0.4F;

// LES RAYONS DU CERCLE, en pixels de sprite : quarante pour la tête, dix pour
// les autres calques. Le corps, lui, reste en place.
inline constexpr float kSwingRadiusPixels = 40.0F;
inline constexpr float kLayerSwingRadiusPixels = 10.0F;

// L'ANGLE DU CERCLE, EN DEGRÉS, à ce pas du compteur.
//
// ⚠ LE CLIENT NE FAIT PAS TOURNER SON ANGLE LINÉAIREMENT. Il lit
// `cos(5 × compteur)` et en tire `180 − 180 cos` : le tour est lent aux deux
// bouts et rapide au milieu, et il est COMPLET quand le compteur atteint
// trente-six. La table qu'il lit pour cela n'est pas une table d'angles : c'est
// sa table de sinus, indexée quatre-vingt-dix degrés plus loin, donc un
// cosinus — elle commence par 1,0 ; 0,99985 ; 0,99939.
inline constexpr float kSwingHalfTurn = 180.0F;
inline constexpr float kSwingDegreesPerStep = 5.0F;

// Le champ dont le cercle tire son pas.
inline constexpr std::uint16_t kSwingField = 0xF4;

// Le champ que cet effet pousse sous ce décalage, ou rien.
const ActorFieldWrite* WriteOn(const ActorEffectChange& change, std::uint16_t field) {
  for (const ActorFieldWrite& write : change.writes) {
    if (write.field == field) {
      return &write;
    }
  }
  return nullptr;
}

}  // namespace

std::int32_t ValueAt(const ActorFieldWrite& write, std::uint32_t age_ticks,
                     std::uint16_t life_ticks) noexcept {
  if (!write.from_clock) {
    return write.value;
  }
  // ⚠ L'ÂGE SE FIGE AU DERNIER TIC ÉCRIT. Le client cesse d'écrire à sa borne,
  // et le champ garde la valeur du tic d'avant. Laisser l'âge courir ferait
  // descendre un corps bien après qu'il a fini de descendre.
  const std::uint32_t age = life_ticks == 0 || write.outlives_life
                                ? age_ticks
                                : std::min<std::uint32_t>(age_ticks, life_ticks - 1U);
  const std::int32_t value = write.slope * static_cast<std::int32_t>(age) + write.offset;
  return write.cap == kNoCap ? value : std::min(value, write.cap);
}

ActorBodyPose BodyPoseAfterEffects(std::span<const ActorEffectPlay> played) {
  ActorBodyPose pose;
  for (const ActorEffectPlay& play : played) {
    const ActorEffectChange* change = ActorEffectChangeFor(play.effect_id);
    if (change == nullptr) {
      continue;
    }
    // Les bits que l'effet relève À CET ÂGE : aucun passé la mort de son
    // instance, et seulement ceux qui ignorent sa vie une fois celle-ci passée.
    const std::uint32_t raised = RenderBitsAt(*change, play.age_ticks);
    if ((raised & kOffScreen) != 0) {
      pose.hidden = true;
    }
    if ((raised & kJitteringBody) != 0) {
      pose.jitter_pixels = kJitterPixels;
    }
    if ((raised & kKickedBody) != 0) {
      if (const ActorFieldWrite* write = WriteOn(*change, kSwingField)) {
        const std::int32_t step = std::max(0, ValueAt(*write, play.age_ticks, change->life_ticks));
        if (step <= kLiftRisingLastStep) {
          pose.lift_fraction += std::sin(kLiftDegreesPerStep * static_cast<float>(step) *
                                         std::numbers::pi_v<float> / kSwingHalfTurn) *
                                kLiftHighest;
        } else if (step <= kLiftHeldLastStep) {
          pose.lift_fraction += kLiftHighest;
        } else if (step <= kLiftFallLastStep) {
          pose.lift_fraction += static_cast<float>(kLiftFallLastStep - step) * kLiftFallPerStep;
        }
      }
    }
    if ((raised & kPressedBody) != 0) {
      if (const ActorFieldWrite* write = WriteOn(*change, kSwingField)) {
        const std::int32_t step = std::max(0, ValueAt(*write, play.age_ticks, change->life_ticks));
        if (step <= kPressedFallLastStep) {
          pose.height_scale = 1.0F - kPressedFallPerStep * static_cast<float>(step);
        } else if (step <= kPressedHoldLastStep) {
          pose.height_scale = kPressedHeld;
        }
      }
    }
    if ((raised & kGlowingBody) != 0) {
      pose.additive_parts = static_cast<ActorPartSet>(pose.additive_parts | kBodyAndHeadwearParts);
    }
    if ((raised & kGlowingWeapon) != 0) {
      pose.additive_parts =
          static_cast<ActorPartSet>(pose.additive_parts | PartBit(ActorPart::kWeapon));
    }
    if ((raised & kDoubledWeapon) != 0) {
      pose.doubles_weapon = true;
    }
    if ((raised & kSwingingBody) != 0) {
      if (const ActorFieldWrite* write = WriteOn(*change, kSwingField)) {
        // ⚠ LE COMPTEUR RESTE À ZÉRO TANT QUE LE CLIENT NE L'ÉCRIT PAS, comme
        // pour la croix : deux de ces trois effets ne commencent qu'au
        // quatorzième tic, et leur rampe est négative avant.
        const auto step =
            static_cast<float>(std::max(0, ValueAt(*write, play.age_ticks, change->life_ticks)));
        const float angle =
            kSwingHalfTurn - kSwingHalfTurn * std::cos(kSwingDegreesPerStep * step *
                                                       std::numbers::pi_v<float> / kSwingHalfTurn);
        const float radians = angle * std::numbers::pi_v<float> / kSwingHalfTurn;
        pose.swing_pixels[0] += std::sin(radians) * kSwingRadiusPixels;
        // Le client retranche `cos × 40 − 40` d'une ordonnée qui DESCEND : vers
        // le haut, cela vaut `40 cos − 40`, nul au départ du cercle.
        pose.swing_pixels[1] += std::cos(radians) * kSwingRadiusPixels - kSwingRadiusPixels;
        pose.layer_swing_pixels[0] += std::sin(radians) * kLayerSwingRadiusPixels;
        pose.layer_swing_pixels[1] +=
            std::cos(radians) * kLayerSwingRadiusPixels - kLayerSwingRadiusPixels;
      }
    }
    if ((raised & kFadeOutRising) != 0) {
      if (const ActorFieldWrite* write = WriteOn(*change, kFadeOutField)) {
        const std::int32_t step = ValueAt(*write, play.age_ticks, change->life_ticks);
        pose.up_pixels += kRisePixelsPerStep * static_cast<float>(step);
        pose.alpha = static_cast<std::uint8_t>(
            std::clamp<std::int32_t>(kOpaque - kFadeOutStep * step, 0, kOpaque));
      }
    }
    if ((raised & kFadeInFalling) != 0) {
      if (const ActorFieldWrite* write = WriteOn(*change, kFadeInField)) {
        const std::int32_t step = ValueAt(*write, play.age_ticks, change->life_ticks);
        pose.up_pixels +=
            std::max(0.0F, kRisePixelsPerStep * static_cast<float>(kFadeInLastStep - step));
        pose.alpha = static_cast<std::uint8_t>(
            std::clamp<std::int32_t>(kFadeInStep * (step - kFadeInFirstStep), 0, kOpaque));
      }
    }
  }
  return pose;
}

}  // namespace ro::render
