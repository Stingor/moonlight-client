#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

#include "ro/render/floating_number.h"
#include "ro/render/skill_effect_table.h"

namespace ro::render {

// LES CAS PARTICULIERS DU COUP : neuf compétences que le répartiteur des
// frappes traite à la main, en plus de l'effet d'emploi.
//
// 🔴 CE N'EST PAS LA TABLE DE L'EFFET D'EMPLOI, et les deux vivent dans la même
// fonction. Le visuel de coup demande d'abord au résolveur d'emploi l'état et
// l'effet — c'est `kSkillUseEffects` —,
// puis traverse un switch par compétence où neuf d'entre elles font naître
// leurs propres effets, jusqu'à TROIS chacune.
//
// Ce sont les enchaînements du moine et du champion, et les coups du Sura :
// MO_CHAINCOMBO, MO_COMBOFINISH, CH_PALMSTRIKE, CH_TIGERFIST, CH_CHAINCRUSH,
// SR_EARTHSHAKER, SR_FALLENEMPIRE, SR_RAMPAGEBLASTER, RL_QD_SHOT.
//
// ⚠ LEURS SONS SONT AILLEURS, dans `skill_sound_table` : cette table ne porte
// que les images.
//
// ⚠ CE QUE CES NEUF CAS FONT ENCORE, et que cette table ne porte pas : une POSE
// chacun (avec des bornes d'images qui leur sont propres) et le message 122 au mode de jeu, qui
// accompagne cinq d'entre eux.

// D'où un effet de coup part, et ce qui l'écarte.
enum class HitExtraGuard : std::uint8_t {
  // Rien ne l'écarte.
  kNone,
  // 🔴 RIEN QUAND LA CIBLE EST LE LANCEUR. SR_RAMPAGEBLASTER sort avant ses
  // trois effets dans ce cas.
  kNotOnSelf,
  // 🔴 RIEN SI L'ACTEUR PORTE DÉJÀ CET EFFET. SR_EARTHSHAKER interroge
  // la présence de l'effet 888 sur la source et renonce à TOUT — son compris —
  // quand il le trouve. Sans cette garde, une compétence de zone rejouerait sa
  // gerbe et son bruit à chaque victime, comme Magnum Break sans la sienne.
  kNotIfAlreadyThere,
};

// Un effet que le coup d'une de ces neuf compétences fait naître.
struct SkillHitExtraEntry {
  std::uint16_t skill_id = 0;
  std::uint16_t effect_id = 0;
  // Sur le LANCEUR, ou sur ce qui reçoit le coup.
  SkillEffectOn on = SkillEffectOn::kTarget;
  HitExtraGuard guard = HitExtraGuard::kNone;
  // VRAI QUAND LE CLIENT DONNE À L'INSTANCE LA PLACE DE LA CIBLE (message 14),
  // aussitôt née : l'effet que CH_CHAINCRUSH et SR_RAMPAGEBLASTER posent sur le
  // lanceur est tendu vers ce qu'il frappe. ⚠ EN QUEUE : la table se pose par
  // position.
  bool aims_at_target = false;
};

// LES DIX-NEUF, triées par compétence et contiguës par compétence.
inline constexpr std::array<SkillHitExtraEntry, 19> kSkillHitExtras = {{
    // MO_CHAINCOMBO — deux sur ce qu'il touche, une sur lui.
    {272, 262, SkillEffectOn::kTarget, HitExtraGuard::kNone},
    {272, 263, SkillEffectOn::kCaster, HitExtraGuard::kNone},
    {272, 273, SkillEffectOn::kTarget, HitExtraGuard::kNone},
    // MO_COMBOFINISH.
    {273, 263, SkillEffectOn::kCaster, HitExtraGuard::kNone},
    {273, 330, SkillEffectOn::kCaster, HitExtraGuard::kNone},
    // CH_PALMSTRIKE.
    {370, 376, SkillEffectOn::kTarget, HitExtraGuard::kNone},
    // CH_TIGERFIST.
    {371, 377, SkillEffectOn::kCaster, HitExtraGuard::kNone},
    {371, 263, SkillEffectOn::kCaster, HitExtraGuard::kNone},
    // CH_CHAINCRUSH.
    {372, 263, SkillEffectOn::kCaster, HitExtraGuard::kNone},
    {372, 512, SkillEffectOn::kCaster, HitExtraGuard::kNone, true},
    // SR_EARTHSHAKER — les deux ensemble, ou aucun.
    {2328, 886, SkillEffectOn::kCaster, HitExtraGuard::kNotIfAlreadyThere},
    {2328, 888, SkillEffectOn::kCaster, HitExtraGuard::kNotIfAlreadyThere},
    // SR_FALLENEMPIRE — les seuls deux que le client fasse naître par son
    // gestionnaire d'effets plutôt que par le semeur de primitives.
    {2329, 1570, SkillEffectOn::kTarget, HitExtraGuard::kNone},
    {2329, 1571, SkillEffectOn::kTarget, HitExtraGuard::kNone},
    // SR_RAMPAGEBLASTER — rien sur soi-même.
    {2332, 549, SkillEffectOn::kTarget, HitExtraGuard::kNotOnSelf},
    {2332, 550, SkillEffectOn::kTarget, HitExtraGuard::kNotOnSelf},
    {2332, 891, SkillEffectOn::kCaster, HitExtraGuard::kNotOnSelf, true},
    // RL_QD_SHOT — les deux mêmes que l'enchaînement du moine, sur la cible.
    //
    // ⚠ LE SECOND EST PARTAGÉ PAR UN LABEL, et c'est ainsi qu'on le manque :
    // le cas de RL_QD_SHOT pose le 262 puis TOMBE dans celui de MO_CHAINCOMBO,
    // dont il hérite le 273. Lire le cas jusqu'à son `break` ne suffit pas —
    // il faut suivre le `goto`.
    {2559, 262, SkillEffectOn::kTarget, HitExtraGuard::kNone},
    {2559, 273, SkillEffectOn::kTarget, HitExtraGuard::kNone},
}};

// 🔴 L'EFFET QUE SR_EARTHSHAKER CHERCHE AVANT DE SE JOUER. C'est l'un des deux
// qu'il pose lui-même : trouver le second veut dire « je viens déjà de me
// jouer sur ce coup-ci », et la compétence se tait.
inline constexpr std::uint16_t kEarthshakerAlreadyThereEffect = 888;

// 🔴 LES COMPÉTENCES QUI REJOUENT LEUR COUP À CHAQUE FRAPPE : à chaque frappe
// d'un coup multiple après la première, le lanceur se renvoie un message
// différé, dû à l'instant où cette frappe touche, qui le remet dans sa pose
// d'attaque — l'animation repart de sa première image, une image par tic de
// vingt-quatre millisecondes, quelle que soit la cadence du fichier.
//
// ⚠ LE CLIENT DE RÉFÉRENCE N'EST PAS L'EXÉCUTABLE NU : son patch
// `RestoreSkillAttackAnimation` renvoie AS_SONICBLOW (136) et CG_ARROWVULCAN
// (394) sur le chemin de CH_CHAINCRUSH (372), que l'exécutable nu est seul à
// prendre. C'est le client que le joueur compare, et c'est lui qu'on suit.
inline constexpr std::uint16_t kSwingReplayingSkill = 372;
inline constexpr std::array<std::uint16_t, 3> kSwingReplayingSkills = {136, 372, 394};

// Combien de fois le lanceur rejoue son attaque : une par frappe au-delà de la
// première, pour les sortes qui comptent leurs frappes ; zéro pour toute autre
// compétence.
[[nodiscard]] inline std::uint16_t SwingReplaysOf(std::uint16_t skill_id, std::uint8_t damage_type,
                                                  std::uint16_t announced_hits) {
  if (std::ranges::find(kSwingReplayingSkills, skill_id) == kSwingReplayingSkills.end()) {
    return 0;
  }
  return static_cast<std::uint16_t>(ShownHitCount(damage_type, announced_hits) - 1U);
}

// Ce que le coup de cette compétence pose en plus. Vide pour toutes les autres.
[[nodiscard]] inline std::span<const SkillHitExtraEntry> SkillHitExtrasFor(
    std::uint16_t skill_id) noexcept {
  const auto first = std::ranges::find(kSkillHitExtras, skill_id, &SkillHitExtraEntry::skill_id);
  if (first == kSkillHitExtras.end()) {
    return {};
  }
  const auto last = std::find_if(
      first, kSkillHitExtras.end(),
      [skill_id](const SkillHitExtraEntry& entry) { return entry.skill_id != skill_id; });
  return {first, last};
}

}  // namespace ro::render
