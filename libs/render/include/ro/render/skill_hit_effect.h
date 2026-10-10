#pragma once

#include <cstdint>
#include <span>

namespace ro::render {

// L'EFFET DE FRAPPE d'une compétence : ce que la CIBLE pose sur elle à chaque
// message de dégâts — un par frappe, à l'instant où le chiffre paraît, et non
// au départ du geste.
//
// C'est un second effet, qui n'est pas celui de la compétence. Cold Bolt fait
// tomber ses flèches de glace (effet 26, sur la cible) ET pose à chaque impact
// la fumée blanche du froid (EF_COLDHIT, 51) ; le feu pose EF_FIREHIT, la
// foudre EF_WINDHIT, et tout le reste des compétences classiques la gerbe
// ordinaire EF_HIT1.
//
// Le client le décide au moment du COUP, dans le répartiteur du message 37, et
// le range dans le message qu'il envoie à la cible ; la cible le pose en jouant
// ce message (messages 96 et 97).
//
// 🔴 ET CE MESSAGE PORTE DEUX EFFETS, PAS UN. La carte des effets de dégâts de
// la compétence remplit un second champ du MÊME message et pose un SECOND
// effet ; les deux jouent, et ne se remplacent pas. Voir
// `SkillDamageMsgEffect` plus bas : 205 compétences en posent un, et c'est lui
// qui porte le bruit de l'impact pour la plupart des compétences classiques.

// Ce qu'une compétence pose, quand ce n'est pas ce que la règle par défaut dit.
struct SkillHitEffectEntry {
  std::uint16_t skill_id = 0;
  // −1 quand la compétence ne pose RIEN, ce qui n'est pas la même chose que
  // zéro : zéro est EF_HIT1.
  std::int32_t effect_id = -1;
};

// LA RÈGLE PAR DÉFAUT, celle du client et non un choix : elle décide de tout ce que la
// table ne nomme pas, et c'est elle qui donne son impact à la plupart des
// compétences.
struct SkillHitEffectDefault {
  // Les identifiants de cette plage ne posent rien.
  std::uint16_t silent_from = 0;
  std::uint16_t silent_to = 0;
  // Ni ceux qui l'atteignent : tout le moderne est muet par ce chemin.
  std::uint16_t silent_beyond = 0;
  // Et partout ailleurs, cet effet-là.
  std::int32_t otherwise = 0;
};

// Ce dont dépend une entrée qui n'est pas fixe.
enum class SkillHitWhen : std::uint8_t {
  // La frappe a fait des dégâts. Le client lit le montant PAR FRAPPE.
  kAnyDamage,
  // Quelqu'un d'autre que moi l'a lancée.
  kAnotherCaster,
};

// Une entrée dont la valeur dépend du coup, et non du seul numéro.
struct SkillHitEffectWhen {
  std::uint16_t skill_id = 0;
  SkillHitWhen when = SkillHitWhen::kAnyDamage;
  std::int32_t then_effect = -1;
  std::int32_t otherwise = -1;
};

// La table des écarts, triée par identifiant de compétence.
[[nodiscard]] std::span<const SkillHitEffectEntry> SkillHitEffectTable();

// LE SECOND EFFET DU MÊME MESSAGE : la carte des effets de dégâts, triée par
// identifiant.
//
// 🔴 IL S'AJOUTE AU PREMIER, il ne le remplace pas. Bash pose la gerbe
// ordinaire par la règle par défaut ET l'effet 1 par cette carte-ci — et c'est
// le second qui sonne, `EF_hit2.wav`. Soixante et une compétences classiques
// sont dans ce cas.
[[nodiscard]] std::span<const SkillHitEffectEntry> SkillDamageMsgEffectTable();

// Les entrées qui dépendent du coup.
[[nodiscard]] std::span<const SkillHitEffectWhen> SkillHitEffectConditions();

// La règle par défaut.
[[nodiscard]] SkillHitEffectDefault SkillHitEffectDefaults();

// L'effet que cette frappe pose sur la cible, ou −1 quand elle n'en pose aucun.
//
// `any_damage` dit si la frappe a fait des dégâts, `cast_by_me` si c'est notre
// propre personnage qui l'a lancée : deux compétences le demandent.
[[nodiscard]] std::int32_t SkillHitEffect(std::uint16_t skill_id, bool any_damage, bool cast_by_me);

// L'effet que la carte du message pose EN PLUS, ou −1 quand elle n'en pose
// aucun.
//
// ⚠ PAS DE RÈGLE PAR DÉFAUT ICI, à la différence de l'effet de frappe : un
// identifiant absent ne pose rien, et la carte met elle-même −1 pour 97 des 302
// identifiants qu'elle nomme. Les deux se valent.
[[nodiscard]] std::int32_t SkillDamageMsgEffect(std::uint16_t skill_id);

// ── L'IMPACT D'UN COUP ORDINAIRE ────────────────────────────────────────────
//
// 🔴 UN COUP SANS COMPÉTENCE EN POSE UN, LUI AUSSI, et le premier port
// l'écartait : il ne regardait l'impact que pour `skill_id != 0`. Le client
// remplit les MÊMES deux champs du message de dégâts, mais depuis un autre
// endroit — le traitement des dégâts de l'acteur au lieu du répartiteur des
// compétences —, et la cible les pose par le même corps.
//
// C'est la gerbe qu'on voit à chaque coup au corps à corps.

// L'effet de FRAPPE d'un coup ordinaire : AUCUN, sauf pour les deux sortes
// CRITIQUES — 10 et 13 —, où c'est EF_HIT2.
[[nodiscard]] std::int32_t PlainHitStrikeEffect(std::uint8_t damage_type);

// Celui du MESSAGE : la gerbe ordinaire EF_HIT1 pour presque tout coup, deux
// autres figures pour seize classes, et RIEN quand on se frappe soi-même — le
// client met alors −2 dans le champ, ce qui fait taire le son avec.
//
// ⚠ LA CLASSE EST CELLE DE L'ATTAQUANT, et non de la cible comme pour le bruit
// du coup : le client la résout, classe affichée comprise, sur l'acteur qui
// traite son propre coup, avant d'écrire le champ que la cible lira.
[[nodiscard]] std::int32_t PlainHitMsgEffect(std::uint16_t attacker_job, bool self_inflicted);

// SI CETTE SORTE DE COUP MONTRE UN IMPACT. Le répartiteur du client ne connaît
// que sept sortes de dégâts, et deux d'entre elles — 5 et 14 — repartent avant
// d'en choisir un.
[[nodiscard]] bool DamageTypeShowsHitEffect(std::uint8_t damage_type);

// ⚠ SAUF POUR UNE POIGNÉE DE COMPÉTENCES, que le code du client nomme : elles
// montrent leur impact à la sorte 5 aussi. La table dit, pour chacune, les
// sortes ordinairement muettes où elle le montre, un bit par sorte.
struct SplashHitSkill {
  std::uint16_t skill_id = 0;
  std::uint16_t damage_types = 0;
};
[[nodiscard]] std::span<const SplashHitSkill> SplashHitSkillTable();

// Si un coup de cette compétence, de cette sorte, montre un impact.
[[nodiscard]] bool DamageTypeShowsHitEffect(std::uint8_t damage_type, std::uint16_t skill_id);

// CE QU'UNE UNITÉ AU SOL POSE, et c'est une AUTRE table.
//
// 🔴 UNE UNITÉ N'EST PAS UN ACTEUR ORDINAIRE. Quand la source d'un coup est une
// unité posée au sol — le blizzard de Storm Gust, la pluie de Meteor Storm —,
// le client délivre le message du coup au répartiteur des unités et non à
// celui des acteurs. Celui-là ne pose AUCUN effet de
// compétence, ni celui du chemin des dégâts ni l'effet visé, et se borne à
// choisir l'effet de frappe ; le champ de la carte des dégâts y vaut toujours
// −1.
//
// C'est ce qui empêche le blizzard de renaître à chaque battement de chacune de
// ses unités, et il y en a une par case.
[[nodiscard]] std::span<const SkillHitEffectEntry> SkillUnitHitEffectTable();

// La règle par défaut de cette table-là : −1 à partir d'un certain identifiant,
// la gerbe ordinaire en deçà. Les deux premières bornes ne servent pas.
[[nodiscard]] SkillHitEffectDefault SkillUnitHitEffectDefaults();

// L'effet qu'une unité au sol pose sur ce qu'elle touche, ou −1.
[[nodiscard]] std::int32_t SkillUnitHitEffect(std::uint16_t skill_id);

}  // namespace ro::render
