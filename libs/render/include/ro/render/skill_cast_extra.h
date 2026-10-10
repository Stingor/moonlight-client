#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <span>

#include "ro/render/skill_effect_table.h"

namespace ro::render {

// CE QUE L'INCANTATION FAIT EN PLUS, POUR UNE POIGNÉE DE COMPÉTENCES.
//
// 🔴 UN QUATRIÈME CHEMIN, et il ne ressemble à aucun des trois autres. Les
// effets d'annonce, de coup et d'unité au sol se cherchent chacun dans un
// répartiteur par compétence ; ceux-ci sont écrits en dur dans les deux
// DERNIERS gestes du visuel d'incantation, après la pose et après l'effet
// d'incantation :
//
//   · le premier agit sur le LANCEUR ;
//   · le second agit selon que le lancer vise QUELQU'UN ou une CASE.
//
// ⚠ DERRIÈRE LA SORTIE SUR DURÉE NULLE, et c'est ce qui les borne : les deux ne
// sont atteints que si l'incantation annonce une durée. Une compétence
// instantanée n'y passe pas — sauf celle de `CastOutlivesInstantExit`, qui n'a
// d'entrée dans aucune des deux tables.
//
// ⚠ CE QUE CES DEUX GESTES FONT ET QUE NOUS NE PORTONS PAS, dit une fois
// pour que personne ne le redécouvre en croyant à un trou :
//   · LE LANCEUR SE TOURNE — vers sa cible, ou vers la case visée. C'est le
//     geste le plus visible des deux, et il appartient à la posture,
//     non aux effets ;
//   · un REPÈRE AU SOL naît sous un lancer sans cible, dans la liste de nœuds
//     de la scène ;
//   · cinq POSES d'incantation forcées — MO_CALLSPIRITS, MO_STEELBODY,
//     MO_KITRANSLATION et CH_SOULCOLLECT prennent le rang 12, celui de
//     l'incantation ordinaire, et ABC_CHAIN_REACTION_SHOT le rang 11. Celle de
//     MO_STEELBODY BOUCLE, pour la durée annoncée plus une seconde ;
//   · un DRAPEAU et son échéance pour KN_BRANDISHSPEAR, KN_BOWLINGBASH et
//     HN_DOUBLEBOWLINGBASH, dont nous n'avons pas identifié le lecteur ;
//   · quatre COMMANDES 143 pour WL_TETRAVORTEX. Ce n'est pas un créateur
//     d'effet : le message 143 parcourt les effets DÉJÀ VIVANTS sur l'acteur et
//     ne parle qu'à ceux dont l'identifiant est passé — 735, 737, 736, 738,
//     les quatre particules élémentaires. Il faudrait donc d'abord les semer.

// LA SEULE INCANTATION QUI PASSE LA SORTIE SUR DURÉE NULLE : MO_EXTREMITYFIST.
//
// 🔴 C'EST TOUT CE QUI MONTRE SES GLYPHES EN COMBO. Lancé après Combo Finish ou
// Chain Crush, Asura Strike s'annonce avec une durée nulle ; le cas de la
// compétence saute par-dessus la sortie et va droit à la pose et à l'effet
// d'incantation — 328, ou 510 pour un Champion —, sans passer par le registre
// des effets nommés.
inline constexpr std::uint16_t kSkillOutlivingInstantExit = 271;

[[nodiscard]] inline constexpr bool CastOutlivesInstantExit(std::uint16_t skill_id) noexcept {
  return skill_id == kSkillOutlivingInstantExit;
}

// LE RÉTICULE QUI SE REFERME SUR CE QU'ON INCANTE : l'effet 60.
//
// 🔴 IL N'APPARTIENT À AUCUNE COMPÉTENCE, et c'est pourquoi il n'est pas dans la
// table ci-dessous. Le geste du sol l'envoie à la cible — message 86, portant
// l'identifiant et `durée / 16` — AVANT de traiter la compétence : tout lancer
// visant quelqu'un le pose, et les cas par compétence ne font que s'y ajouter.
inline constexpr std::uint16_t kLockOnReticleEffect = 60;

// LA SEULE COMPÉTENCE QUI N'EN POSE PAS. Son cas rejoint le semis commun par un
// saut qui passe PAR-DESSUS le message 86 ; les autres cas, eux, le laissent
// s'exécuter avant de poser le leur.
inline constexpr std::uint16_t kNoLockOnSkill = 5284;

// Si cette incantation pose le réticule sur ce qu'elle vise.
//
// Les quatre gardes viennent toutes du même geste, et chacune retire un cas
// que le client ne montre pas :
//   · une durée NULLE ne montre rien : la sortie sur durée nulle passe avant,
//     et l'incantation qui la saute donne au réticule une vie de `0 / 16`
//     tics — il meurt à sa première mise à jour ;
//   · SANS CIBLE, l'autre branche pose un repère au sol, pas un réticule ;
//   · SUR SOI-MÊME, rien ne se pose : le lanceur et la cible sont le même ;
//   · CD_FRAMEN saute par-dessus le message.
[[nodiscard]] inline constexpr bool ShowsLockOnReticle(std::uint16_t skill_id,
                                                       std::uint32_t caster_id,
                                                       std::uint32_t target_id,
                                                       std::uint32_t cast_duration_ms) noexcept {
  return cast_duration_ms != 0 && target_id != 0 && target_id != caster_id &&
         skill_id != kNoLockOnSkill;
}

// Un effet que l'incantation pose en plus.
struct SkillCastExtraEntry {
  std::uint16_t skill_id = 0;
  std::uint16_t effect_id = 0;
  // Sur le LANCEUR, ou sur ce qu'il vise. Les deux gestes se partagent les
  // deux cas, et une même compétence peut en poser plusieurs.
  SkillEffectOn on = SkillEffectOn::kTarget;
  // Si le client borne la vie de cet effet à la durée d'incantation annoncée.
  // Les autres la laissent à ce que leur fichier dit.
  //
  // ⚠ CONNU, PAS ENCORE CONSOMMÉ : notre semeur ne sait pas écourter un effet,
  // et ces deux-là dureront donc ce que leur fichier dure. C'est écrit ici pour
  // qu'on ne prenne pas ce champ pour une promesse tenue.
  bool lives_for_cast = false;
  // VRAI POUR UN OBJET LANCÉ DU LANCEUR VERS SA CIBLE : il naît sur le lanceur et
  // vise ce qu'il incante. Sans cible, il n'est pas lancé.
  bool thrown_at_target = false;
};

// LES NEUF, triées par compétence.
//
// ⚠ WL_SOULEXPANSION EN POSE DEUX, et c'est pourquoi ceci est une liste de
// paires et non une table à une entrée par compétence.
//
// ⚠ CD_FRAMEN EN FAIT PARTIE, avec son effet 1930, bien que le client le
// partage avec RA_AIMEDBOLT au lieu de l'écrire à part : l'oublier en laisserait
// six.
inline constexpr std::array<SkillCastExtraEntry, 9> kSkillCastExtras = {{
    // CH_SOULCOLLECT — les anneaux d'esprits, sur le moine qui les rassemble.
    {401, 402, SkillEffectOn::kCaster, false},
    // CR_ACIDDEMONSTRATION — le flacon de feu, puis celui d'acide, lancés vers la
    // cible.
    {490, 539, SkillEffectOn::kCaster, false, true},
    {490, 980, SkillEffectOn::kCaster, false, true},
    // WL_SOULEXPANSION — deux effets d'un coup sur ce qu'elle vise.
    {2202, 1589, SkillEffectOn::kTarget, false},
    {2202, 1590, SkillEffectOn::kTarget, false},
    // WL_CHAINLIGHTNING.
    {2214, 1637, SkillEffectOn::kTarget, false},
    // RA_AIMEDBOLT.
    {2236, 1651, SkillEffectOn::kTarget, true},
    // NC_FLAMELAUNCHER — sur le mécanicien, pas sur sa cible.
    {2259, 787, SkillEffectOn::kCaster, false},
    // CD_FRAMEN.
    {5284, 1930, SkillEffectOn::kTarget, true},
}};

// Ce que cette compétence pose en plus pendant son incantation. Vide pour la
// quasi-totalité d'entre elles.
[[nodiscard]] inline std::span<const SkillCastExtraEntry> SkillCastExtrasFor(
    std::uint16_t skill_id) noexcept {
  const auto first = std::ranges::find(kSkillCastExtras, skill_id, &SkillCastExtraEntry::skill_id);
  if (first == kSkillCastExtras.end()) {
    return {};
  }
  const auto last = std::find_if(
      first, kSkillCastExtras.end(),
      [skill_id](const SkillCastExtraEntry& entry) { return entry.skill_id != skill_id; });
  return {first, last};
}

}  // namespace ro::render
