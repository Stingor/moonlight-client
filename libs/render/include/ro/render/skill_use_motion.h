#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>

namespace ro::render {

// LA POSE QU'UNE COMPÉTENCE IMPOSE QUAND ELLE EST EMPLOYÉE.
//
// 🔴 CE N'EST PAS LA POSE D'INCANTATION, et c'est tout l'intérêt. Le client a
// DEUX résolveurs, sur deux chemins que le serveur emprunte séparément :
//
//   · le premier sert le chemin de l'INCANTATION, par l'affichage de
//     l'incantation — qui SORT dès que la durée vaut zéro. Une compétence
//     INSTANTANÉE n'y met jamais les pieds ;
//   · le second sert le chemin de l'EMPLOI : l'effet se joue sur l'acteur, puis
//     la pose lui est imposée.
//
// C'est le second, et lui seul, qui donne sa posture à une compétence sans
// temps d'incantation. Sans lui, First Aid et tout ce qui lui ressemble reste
// planté. Voir `docs/protocole-de-controle-des-effets.md` : c'est le premier
// contrôle en jeu du protocole qui l'a trouvé.
//
// ⚠ L'ÉTAT SEPT EST LE DÉFAUT, et il donne le rang 12 du `.act` — l'incantation
// — pour la plupart des métiers. Il couvre 1 088 des 1 635 compétences que ce
// serveur porte ; les autres posent un état à elles, dont 105 l'état 2, qui
// FRAPPE au lieu d'incanter.
inline constexpr std::int8_t kSkillUseStateDefault = 7;

// L'EFFET que le MÊME résolveur rend.
//
// 🔴 IL REND DEUX VALEURS : l'état ET l'effet. Le client consomme l'effet au
// COUP, dans le répartiteur du message 37, et le fait naître sur la SOURCE.
//
// ⚠ ET CE DÉFAUT N'EST PAS INERTE : seize est `EF_BASH`, une vraie gerbe avec
// son propre son. Toute compétence dont le switch ne touche que l'ÉTAT le garde,
// et Bash en est le cas d'école — c'est ce qui manquait à son coup.
inline constexpr std::int16_t kSkillUseEffectDefault = 16;

// L'effet qui ne montre RIEN.
inline constexpr std::int16_t kSkillUseEffectNone = -1;

// L'état d'action d'une compétence employée. Traduire en rang d'animation est
// l'affaire d'`ActionAnimationFor`, la même table que pour l'incantation.
struct SkillUseMotionEntry {
  std::uint16_t skill_id = 0;
  std::int8_t state = kSkillUseStateDefault;
};

// L'effet, dans une table À PART.
//
// ⚠ DEUX TABLES, et ce n'est pas un rangement. Chacune distingue « le switch a
// POSÉ cette valeur » de « personne n'a rien posé, et le défaut est resté ».
// Les fondre en une seule table obligerait à écrire le
// défaut pour toute compétence qui n'a que l'autre valeur, et cette
// distinction-là mourrait en silence : 179 compétences ont un SEPT posé, qu'il
// faudra garder le jour où le registre d'effets remplira le reste.
struct SkillUseEffectEntry {
  std::uint16_t skill_id = 0;
  std::int16_t effect_id = kSkillUseEffectDefault;
};

// LES COMPÉTENCES DONT LE COUP NE POSE PAS CET EFFET.
//
// 🔴 SANS CETTE GARDE, LES SORTS DE MAGE POSERAIENT LEUR EFFET DEUX FOIS. Le
// client la consulte juste avant de faire naître l'effet d'emploi au coup ; elle
// écarte onze identifiants d'affilée — 11 à 21, les boulons et Soul Strike —
// dont l'effet vient du répartiteur des dégâts, et quinze autres.
inline constexpr std::array<std::uint16_t, 26> kSkillsWithoutUseEffectOnHit = {
    11, 12, 13, 14, 15, 16,  17,  18,  19,   20,   21,   27,   29,
    30, 32, 33, 77, 86, 340, 690, 731, 2008, 2533, 2534, 2536, 5004};

// Vrai si le COUP de cette compétence fait naître son effet d'emploi.
[[nodiscard]] constexpr bool SkillUseEffectShowsOnHit(std::uint32_t skill_id) {
  return std::ranges::find(kSkillsWithoutUseEffectOnHit, skill_id) ==
         kSkillsWithoutUseEffectOnHit.end();
}

// Triées par identifiant de compétence : la recherche est dichotomique.
[[nodiscard]] std::span<const SkillUseMotionEntry> SkillUseMotionTable();
[[nodiscard]] std::span<const SkillUseEffectEntry> SkillUseEffectTable();

// L'état qu'une compétence pose quand on l'emploie, `kSkillUseStateDefault`
// pour celles que la table ne nomme pas.
//
// ⚠ CE DÉFAUT EST CELUI DU CLIENT, pas un pis-aller : le switch du client ne nomme
// que 548 compétences, et toutes les autres sortent avec le sept qu'il a posé
// en entrant. Trois sources le décident en vrai — les fiches Lua
// `skilleffectinfo`, qui ne changent PAS l'état ; ce switch ; puis le registre
// d'effets, rempli à l'exécution, que nous ne portons pas encore. Voir
// `scripts/client_skill_use_motion.json`.
[[nodiscard]] std::int8_t SkillUseStateFor(std::uint32_t skill_id);

// L'effet qu'une compétence employée fait naître, `kSkillUseEffectDefault` pour
// celles que la table ne nomme pas.
//
// ⚠ ABSENT ET « AUCUN » NE SE VALENT PAS ICI, contrairement à la plupart de nos
// tables : une compétence absente prend SEIZE, une compétence à moins un ne
// montre RIEN. Les fondre donnerait `EF_BASH` aux 740 qui n'ont rien à montrer.
[[nodiscard]] std::int16_t SkillUseEffectFor(std::uint32_t skill_id);

// ── L'EFFET D'EMPLOI QUE LE COUP FAIT NAÎTRE ────────────────────────────────
//
// 🔴 À L'ARRIVÉE DU PAQUET, ET NON QUAND LE COUP TOUCHE. Le client traite le
// visuel du coup dès le message de dégâts : l'effet d'emploi naît sur le
// lanceur dans la même milliseconde, AVANT la suite de la compétence et son
// effet visé. Seuls l'effet de frappe et celui de la carte des dégâts attendent
// l'image de frappe, sur la cible. Jupitel Thunder et Frost Nova montrent ainsi
// leur cercle douze à l'instant du coup, cent quarante millisecondes avant la
// blessure de leur victime.
//
// L'effet ne dépend pas de l'élément : c'est douze pour un sort de vent comme
// pour un sort d'eau.

// Les sortes de coup que ce visuel connaît ; les autres ne montrent rien.
inline constexpr std::array<std::uint8_t, 7> kDamageTypesOfSkillHitVisual = {4, 5, 6, 8, 9, 13, 14};

// L'ÉCLABOUSSURE et sa variante encaissée : leur message repart AVANT l'effet
// d'emploi quand la cible est connue, sauf pour les compétences ci-dessous.
inline constexpr std::uint8_t kSplashDamageType = 5;
inline constexpr std::uint8_t kSplashEnduredDamageType = 14;
inline constexpr std::array<std::uint16_t, 31> kSkillsWithUseEffectOnSplash = {
    11,   47,   273,  413,  732,  2258, 2284, 2307, 2317, 2328, 2329, 2332, 2337, 2476, 2519, 2576,
    3006, 3007, 5026, 5046, 5211, 5230, 5235, 5243, 5244, 5245, 5248, 5252, 5283, 5284, 5316};

// 🔴 LE COUP D'ÉCLABOUSSURE NE POSE RIEN — sortes cinq et quatorze, les
// victimes `SD_ANIMATION` du serveur —, sauf pour les compétences ci-dessus,
// qui prennent le chemin ordinaire. Il touche à l'image de frappe de l'action
// que son auteur joue DÉJÀ : celle du coup principal du même instant, ou la
// pose d'emploi pour Raid, dont toutes les victimes éclaboussent.
[[nodiscard]] constexpr bool SplashHitKeepsAction(std::uint32_t skill_id,
                                                  std::uint8_t damage_type) {
  return skill_id != 0 &&
         (damage_type == kSplashDamageType || damage_type == kSplashEnduredDamageType) &&
         std::ranges::find(kSkillsWithUseEffectOnSplash, skill_id) ==
             kSkillsWithUseEffectOnSplash.end();
}

// ⚠ ET SANS CIBLE CONNUE, l'éclaboussure passe — sauf pour ces compétences,
// qui renoncent alors à tout le visuel du coup. La variante encaissée n'en
// écarte que deux.
inline constexpr std::array<std::uint16_t, 4> kSkillsSilentOnUnseenSplash = {739, 2450, 2477, 5030};
inline constexpr std::array<std::uint16_t, 2> kSkillsSilentOnUnseenEnduredSplash = {739, 2450};

// LE NIVEAU QUI TAIT L'EFFET D'EMPLOI : le serveur l'annonce pour les coups
// qu'une autre compétence déclenche — Chain Lightning, Sling Item.
inline constexpr std::int16_t kLevelWithoutUseEffect = -2;

// L'effet d'emploi que CE coup fait naître sur son lanceur, ou
// `kSkillUseEffectNone`. `announced_level` est le niveau signé du paquet ;
// `target_known`, vrai quand la cible est un acteur ou une unité que le client
// voit.
[[nodiscard]] inline std::int16_t SkillHitUseEffect(std::uint16_t skill_id,
                                                    std::uint8_t damage_type,
                                                    std::int16_t announced_level,
                                                    bool target_known) {
  const auto lists = [](const auto& skills, std::uint16_t skill) {
    return std::ranges::find(skills, skill) != skills.end();
  };
  if (skill_id == 0 || std::ranges::find(kDamageTypesOfSkillHitVisual, damage_type) ==
                           kDamageTypesOfSkillHitVisual.end()) {
    return kSkillUseEffectNone;
  }
  if (damage_type == kSplashDamageType || damage_type == kSplashEnduredDamageType) {
    const bool silent = target_known ? !lists(kSkillsWithUseEffectOnSplash, skill_id)
                        : damage_type == kSplashDamageType
                            ? lists(kSkillsSilentOnUnseenSplash, skill_id)
                            : lists(kSkillsSilentOnUnseenEnduredSplash, skill_id);
    if (silent) {
      return kSkillUseEffectNone;
    }
  }
  if (!SkillUseEffectShowsOnHit(skill_id) || announced_level == kLevelWithoutUseEffect) {
    return kSkillUseEffectNone;
  }
  return SkillUseEffectFor(skill_id);
}

// ── CE QUE LE COUP D'UNE COMPÉTENCE FAIT DE LA MOTION DU LANCEUR ────────────
//
// 🔴 LE COUP POSE L'ÉTAT D'EMPLOI, par la même table que l'emploi : Bash frappe
// en état deux — une passe d'attaque —, Cold Bolt en état sept — une passe du
// rang d'incantation. L'instant où le coup touche en dépend : voir
// `strike_play.h`.
//
// Une liste de compétences y échappe et garde la motion en cours : treize en
// dur — les enchaînements du moine et du champion, le poing d'Asura, quelques
// coups du Sura et deux autres —, une de plus quand son coup est de la sorte
// multiple, et douze que le registre des actions déclare pour ses classes de
// quatrième métier.
inline constexpr std::array<std::uint16_t, 25> kSkillsKeepingMotionOnHit = {
    143,  263,  271,  272,  273,  371,  2317, 2326, 2328, 2329, 2519, 2579, 5382,
    5411, 5412, 5415, 5445, 5455, 5457, 5459, 5471, 5472, 5473, 5501, 6522};
inline constexpr std::uint16_t kSkillKeepingMotionOnMultiHit = 2261;
inline constexpr std::uint8_t kMultiHitDamageType = 8;

// Vrai quand le coup de cette compétence, de cette sorte, laisse la motion du
// lanceur telle qu'elle est.
[[nodiscard]] constexpr bool SkillHitKeepsMotion(std::uint32_t skill_id, std::uint8_t damage_type) {
  if (skill_id == kSkillKeepingMotionOnMultiHit) {
    return damage_type == kMultiHitDamageType;
  }
  return std::ranges::find(kSkillsKeepingMotionOnHit, skill_id) != kSkillsKeepingMotionOnHit.end();
}

// L'ÉTAT D'ACTION QUE LE COUP POSE — celui de l'emploi —, ou rien quand il
// laisse la motion.
[[nodiscard]] inline std::optional<std::int8_t> SkillHitMotionState(std::uint32_t skill_id,
                                                                    std::uint8_t damage_type) {
  if (SkillHitKeepsMotion(skill_id, damage_type)) {
    return std::nullopt;
  }
  return SkillUseStateFor(skill_id);
}

// LES COMPÉTENCES DONT LE COUP NE RÈGLE PAS LA CADENCE de l'attaque sur la
// durée que le serveur donne : quatre en dur, quatre que le registre déclare.
// Leur geste garde la cadence de son fichier.
inline constexpr std::array<std::uint16_t, 8> kSkillsWithoutHitMotionFactor = {
    490, 2256, 2257, 3009, 5471, 5473, 6004, 6005};

[[nodiscard]] constexpr bool SkillHitSetsMotionFactor(std::uint32_t skill_id) {
  return std::ranges::find(kSkillsWithoutHitMotionFactor, skill_id) ==
         kSkillsWithoutHitMotionFactor.end();
}

}  // namespace ro::render
