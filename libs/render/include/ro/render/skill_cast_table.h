#pragma once

#include <cstdint>
#include <span>

namespace ro::render {

// CE QU'UNE INCANTATION MONTRE : un effet qui naît sur le lanceur, et une pose
// qu'il prend le temps du sort.
//
// Le client résout les deux d'un coup : deux défauts, et huit cents cas qui
// s'en écartent. Rien de tout cela n'est dans un fichier de données — c'est
// une table que le client porte en dur.

// L'effet que le client fait naître quand la compétence n'a rien de propre.
inline constexpr std::int16_t kCastEffectDefault = 16;
// L'effet qui n'est PAS final : quand le résolveur le pose, c'est l'élément du
// sort qui dit lequel jouer — voir CastEffectForElement.
inline constexpr std::int16_t kCastEffectElemental = 12;
// Une incantation qui ne montre AUCUN effet. Sonic Blow est de celles-là : tout
// son visuel vient du chemin des dégâts.
inline constexpr std::int16_t kCastEffectNone = -1;

// La pose ordinaire d'une incantation.
inline constexpr std::int8_t kCastStateDefault = 8;
// L'incantation qui ne change RIEN à la posture : le lanceur reste sur ce qu'il
// faisait. Le client ne change aucune pose sur cet état.
inline constexpr std::int8_t kCastStateNone = -1;

// L'incantation d'une compétence, telle que le résolveur du client la rend.
//
// La table ne porte QUE ce qui s'écarte des deux défauts : une compétence
// absente incante avec l'effet 16 et la pose 8.
struct SkillCastEntry {
  std::uint16_t skill_id = 0;
  std::int16_t effect_id = kCastEffectDefault;
  std::int8_t state = kCastStateDefault;
};

// La table entière, triée par identifiant de compétence.
[[nodiscard]] std::span<const SkillCastEntry> SkillCastTable();

// UN EFFET D'INCANTATION QUI CHANGE AVEC LA CLASSE : à partir de `first_job`, la
// compétence joue `effect_id` au lieu de celui de la table. Asura Strike en est
// le seul cas — les glyphes du Champion ne sont pas ceux du Monk.
struct CastEffectJobOverride {
  std::uint16_t skill_id = 0;
  std::uint16_t first_job = 0;
  std::int16_t effect_id = kCastEffectDefault;
};
[[nodiscard]] std::span<const CastEffectJobOverride> CastEffectJobOverrideTable();

// L'aura élémentaire, indexée par l'élément que le serveur annonce.
//
// Le client ne la consulte que pour l'effet kCastEffectElemental ; un élément
// qu'elle ne nomme pas laisse cet effet tel quel.
[[nodiscard]] std::span<const std::int16_t> CastElementalEffects();

// Ce qu'un état d'action JOUE : le rang d'animation du .act d'un joueur.
struct CastStateEntry {
  std::int8_t state = kCastStateDefault;
  std::uint8_t animation = 0;
};

// La table des états, triée par état. kCastStateNone n'y figure pas : il ne
// joue rien.
//
// ⚠ ELLE SERT AUX DEUX RÉSOLVEURS, celui de l'incantation et celui de l'EMPLOI
// (`skill_use_motion.h`). Un état qu'elle ne nomme pas ne joue RIEN : les deux
// tables du client rendent zéro pour ce qu'elles ignorent, et zéro est aussi le
// rang de l'attente — les confondre planterait l'acteur debout là où le client
// ne change rien.
[[nodiscard]] std::span<const CastStateEntry> CastStateTable();

// Ce qu'un état joue POUR UN MÉTIER DONNÉ, quand il n'en joue pas la même chose
// pour tous.
//
// ⚠ TROIS ÉTATS SEULEMENT, mais l'un d'eux est celui que 1 088 compétences
// prennent par défaut : le SEPT. Un barde, une danseuse, un ninja ou un
// canonnier n'y jouent pas le rang de l'incantation mais celui de la garde, et
// les trois métiers montés jouent celui de l'attaque. Ignorer cette table
// donnerait la même posture à tout le monde.
struct CastStateJobRank {
  std::int8_t state = kCastStateDefault;
  std::uint16_t job = 0;
  std::uint8_t animation = 0;
};

// Triée par (état, métier) : la recherche est dichotomique.
[[nodiscard]] std::span<const CastStateJobRank> CastStateJobRankTable();

}  // namespace ro::render
