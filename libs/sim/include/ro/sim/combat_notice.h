#pragma once

#include <cstdint>
#include <optional>

#include "ro/sim/packed_position.h"

namespace ro::sim {

// CE QUE LE COMBAT FAIT PARAÎTRE, et rien d'autre.
//
// Un coup, une compétence sans dégâts, une compétence lancée au sol, un
// changement d'échelle, une incantation : cinq annonces du serveur que le monde
// RETIENT SANS EN RIEN TIRER. Aucune ne change l'état d'un acteur — ni sa vie,
// ni sa place ; elles disent seulement ce qui vient de se produire, pour qui
// dessine.
//
// ⚠ ELLES VIVENT À PART DU MONDE parce qu'elles forment une famille que les
// lots de compétences font grandir, et qu'un en-tête qui porte tout se parcourt
// au lieu de se lire. C'est la même coupure que `ro/net/world_skill.h` a faite
// du côté de la traduction.

// Un coup, tel que le serveur l'annonce : qui frappe qui, quand, et les
// durées qu'il donne aux deux motions — l'attaque et la blessure —, en
// millisecondes, zéro quand il ne dit rien.
struct Strike {
  std::uint32_t attacker = 0;
  std::uint32_t target = 0;
  // L'instant du coup, dans l'horloge de l'APPELANT.
  std::uint32_t started_ms = 0;
  std::uint32_t attack_motion_ms = 0;
  std::uint32_t hurt_motion_ms = 0;
  // Faux quand la cible ne bronche pas : coup raté, ou encaissé.
  bool flinches = false;
  // Ce que le coup a fait : les dégâts, en points de vie — ou de magie, si
  // sp_damage —, et ceux de la seconde main quand elle frappe aussi ; le
  // nombre de coups que le total réunit ; et la sorte du coup, telle que le
  // serveur la numérote (e_damage_type, dans src/map/clif.hpp : ordinaire,
  // encaissé, critique…). Le monde n'en tire rien ; ce qu'on dessine, si.
  std::int32_t damage = 0;
  std::int32_t second_damage = 0;
  std::uint16_t hits = 1;
  std::uint8_t type = 0;
  bool sp_damage = false;
  // La COMPÉTENCE qui porte le coup, ou zéro pour un coup ordinaire. Le monde
  // n'en tire rien ; ce qu'on en montre — l'effet, le son — est l'affaire de
  // qui dessine, et il lui faut ce numéro.
  std::uint16_t skill_id = 0;
  std::uint16_t skill_level = 0;
  // LE NIVEAU TEL QUE LE PAQUET L'ANNONCE, signe compris : le serveur y met
  // moins un ou moins deux pour dire autre chose qu'un niveau, et moins deux
  // tait l'effet d'emploi du coup. `skill_level` le borne à zéro.
  std::int16_t announced_level = 0;
  // LA CASE DE L'UNITÉ AU SOL que le coup frappe — un bloc d'Ice Wall, un
  // piège —, relevée par le monde à l'arrivée du coup ; vide quand la cible
  // n'en est pas une. Le client garde une unité retirée tant qu'un coup attend
  // encore chez elle : celui qui brise le bloc se montre donc là, même quand
  // le serveur annonce le retrait avant que le coup ne touche.
  std::optional<CellPosition> struck_unit_cell;
  // VRAI QUAND UNE UNITÉ AU SOL A PORTÉ LE COUP — une météore, un bloc de
  // Storm Gust —, relevé par le monde à l'arrivée du coup. Le serveur retire
  // l'unité d'une météore dans le même envoi que son coup : la chercher plus
  // tard ne la trouve plus, et le coup passait pour celui d'un inconnu.
  bool by_ground_unit = false;
};

// Le même coup, frappant l'unité au sol posée sur `cell`.
[[nodiscard]] inline Strike StrikeOnUnitAt(Strike strike, CellPosition cell) {
  strike.struck_unit_cell = cell;
  return strike;
}

// Une compétence SANS DÉGÂTS reçue : un soin, un état posé, un renfort.
//
// Le serveur les annonce toutes par le même paquet, et le champ qui s'appelle
// « niveau » y porte en réalité le MONTANT quand la compétence en a un — le
// soin rendu, par exemple. Le monde ne sait pas lequel des deux c'est : il
// garde le numéro de la compétence et le nombre, et c'est à qui dessine de
// savoir ce que ce nombre veut dire.
struct SupportHit {
  std::uint16_t skill_id = 0;
  // Qui l'a lancée. Zéro quand le serveur ne le dit pas.
  std::uint32_t source_id = 0;
  std::int32_t amount = 0;
  // Faux quand la compétence a ÉCHOUÉ : le serveur l'annonce quand même.
  bool succeeded = false;
  // L'instant, dans l'horloge de l'APPELANT.
  std::uint32_t started_ms = 0;
};

// Une compétence lancée SUR UNE CASE, et qui n'y fait pas de dégâts : un
// portail, un mur, une zone.
//
// Le serveur l'annonce à part de tout le reste — ce n'est ni un coup, ni une
// unité au sol, ni un effet porté par quelqu'un —, et ne dit que trois choses :
// quelle compétence, à quel niveau, sur quelle case. Ce qu'il faut en montrer
// est l'affaire de qui dessine.
struct GroundSkillUse {
  std::uint16_t skill_id = 0;
  std::uint16_t level = 0;
  CellPosition cell;
  // L'instant, dans l'horloge de l'APPELANT.
  std::uint32_t started_ms = 0;
};

// Vrai pour les compétences de case qui LAISSENT LA MOTION DU LANCEUR : ni
// regard vers la case, ni motion d'emploi. Le client écrit à la place l'image
// de la dernière pose d'une action à part ; seule une motion qui TIENT son
// image la garde, bornée à ce qu'elle porte.
[[nodiscard]] bool GroundSkillKeepsCasterMotion(std::uint16_t skill_id) noexcept;

// La dernière compétence sans dégâts que cet acteur a EMPLOYÉE — sur quelqu'un
// ou sur une case. C'est ce qui lui donne sa posture d'emploi.
//
// 🔴 ELLE VIT SUR LE LANCEUR, et non sur la cible. `SupportHit` est rangé sur
// qui reçoit le soin : lire la posture là faisait lever les bras à l'allié
// soigné pendant que le prêtre restait planté.
struct SkillUse {
  std::uint16_t skill_id = 0;
  // L'instant, dans l'horloge de l'APPELANT.
  std::uint32_t started_ms = 0;
  // Le « niveau » du paquet — le montant quand la compétence en a un — et son
  // succès, tels que `SupportHit` les porte. Une compétence posée sur une case
  // n'en dit rien.
  std::int32_t amount = 0;
  bool succeeded = true;
};

// LA PORTÉE d'un sort, annoncée pendant qu'on l'incante.
//
// Le serveur l'envoie à côté de l'incantation, pour les compétences dont sa
// base porte le drapeau ShowScale — les sorts de MVP, pour l'essentiel. Elle
// dit où la zone tombera, et pendant combien de temps elle se montre : le
// temps de l'incantation.
//
// ⚠ Le centre est la case VISÉE, et non le lanceur. kRO ne posait ShowScale
// que sur des sorts visant leur propre case, où les deux coïncident ; Moonlight
// le corrige à l'émission (clif_useskill, dans src/map/clif.cpp) et passe la
// case voulue, ou celle de la cible désignée.
//
// COMBIEN DE CASES, le paquet ne le dit pas : le client demande à son propre Lua
// (GetSkillScale, qui lit SKILL_INFO_LIST[id].SkillScale[niveau], cent soixante-
// quatorze compétences en portent une). C'est donc une table du CLIENT, comme le
// fichier d'un effet, et le monde n'a pas à la connaître — il garde la
// compétence et son rang, qui suffisent à la retrouver.
//
// ⚠ RIEN NE LA PEINT ENCORE. Le client pose un effet 1114 par case du
// rectangle, et 1114 n'a PAS de
// fichier : c'est une classe de nœud inscrite au registre des effets, donc de la
// famille procédurale. Le carré se verra quand ce dessinateur-là sera porté ;
// d'ici là le monde sait où il tombe, et personne ne le montre.
struct SkillScale {
  std::uint16_t skill_id = 0;
  std::uint16_t level = 0;
  // La case où la zone est centrée.
  CellPosition centre;
  // L'instant, dans l'horloge de l'APPELANT, et ce que le serveur lui donne à
  // vivre — la durée d'incantation.
  std::uint32_t started_ms = 0;
  std::uint32_t duration_ms = 0;
};

// L'INCANTATION en cours : ce qu'un acteur prépare, et pour combien de temps.
//
// Le serveur ne l'annonce qu'au DÉBUT, et ne redit rien à la fin : une
// incantation qui va au bout s'éteint d'elle-même, à l'échéance qu'il a donnée.
// Seule son INTERRUPTION a son paquet. C'est donc à qui la montre de la voir
// expirer, et non au monde de l'effacer — le monde ne lit aucune horloge.
struct Cast {
  std::uint16_t skill_id = 0;
  // Qui est visé, ou zéro quand c'est une case qui l'est.
  std::uint32_t target_id = 0;
  // La case visée, quand le sort se lance au sol.
  CellPosition target_cell;
  // L'élément du sort, tel que le serveur le numérote (e_element). Le client
  // en tire la couleur de l'aura qu'il fait tourner sous le lanceur.
  std::uint32_t element = 0;
  // L'instant du début, dans l'horloge de l'APPELANT, et la durée annoncée.
  //
  // Une durée NULLE est un sort instantané : le serveur l'annonce quand même,
  // et le client ne montre alors aucune barre.
  std::uint32_t started_ms = 0;
  std::uint32_t duration_ms = 0;
  // LE SERVEUR L'A INTERROMPUE (`ZC_DISPEL`) : ses effets s'éteignent net, alors
  // que la fin normale d'une incantation les laisse finir d'eux-mêmes.
  bool cancelled = false;
};

// LA BARRE QU'UNE COMPÉTENCE ANNONCÉE POSE SOUS SON LANCEUR, hors de toute
// incantation : la même jauge que celle d'une incantation, que le client fait
// naître par un message à l'acteur et retire par un autre.
//
// ⚠ AUTO COUNTER EST LE SEUL, ET SEULEMENT POUR SOI : l'annonce pose une barre
// de quatre cents millisecondes par niveau, puis la retire aussitôt quand le
// lanceur n'est pas le joueur local. Une barre encore là se PROLONGE — son
// départ et sa fin repartent de l'instant.
struct SkillGauge {
  std::uint32_t started_ms = 0;
  std::uint32_t duration_ms = 0;
};
inline constexpr std::uint16_t kAutoCounterSkill = 61;
inline constexpr std::uint32_t kAutoCounterGaugeMsPerLevel = 400;

// LA POSE QU'UN COUP ARME POUR L'ARRIVÉE SUIVANTE : l'impact d'Asura Strike
// lève un drapeau sur son lanceur ; la prochaine fin de déplacement le baisse et
// fait tenir au lanceur une pose de trois secondes. Le serveur fait glisser le
// lanceur sitôt le coup porté, puis l'arrête : l'arrêt est cette arrivée.
//
// Les sortes de coup sont celles où l'impact d'Asura pose sa propre pose : 4,
// 6, 8, 9 et 13.
inline constexpr std::uint16_t kArrivalPoseSkill = 271;
inline constexpr std::uint16_t kArrivalPoseDamageTypes = 0x2350;
inline constexpr std::uint8_t kArrivalPoseDamageTypeBits = 16;

[[nodiscard]] constexpr bool ArmsArrivalPose(std::uint16_t skill_id, std::uint8_t damage_type) {
  return skill_id == kArrivalPoseSkill && damage_type < kArrivalPoseDamageTypeBits &&
         ((kArrivalPoseDamageTypes >> damage_type) & 1U) != 0;
}

// La durée de la barre qu'annoncer `skill_id` au niveau `level` pose sous le
// JOUEUR LOCAL, ou zéro quand elle n'en pose aucune.
[[nodiscard]] constexpr std::uint32_t SkillGaugeDurationMs(std::uint16_t skill_id,
                                                           std::int32_t level) {
  if (skill_id != kAutoCounterSkill || level <= 0) {
    return 0;
  }
  return kAutoCounterGaugeMsPerLevel * static_cast<std::uint32_t>(level);
}

// 🔴 LA GARDE D'AUTO COUNTER, que prend TOUT lanceur — et non le seul joueur
// local : l'emploi réussi le fige sur la première image de son attaque, le temps
// de la barre — quatre cents millisecondes par niveau —, puis le rend au repos
// ordinaire. Un emploi manqué ne la prend pas. Rien, sinon.
[[nodiscard]] constexpr std::optional<std::uint32_t> CounterStanceMs(const SkillUse& use) {
  if (use.skill_id != kAutoCounterSkill || !use.succeeded) {
    return std::nullopt;
  }
  return SkillGaugeDurationMs(use.skill_id, use.amount);
}

}  // namespace ro::sim
