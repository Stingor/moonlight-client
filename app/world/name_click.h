#pragma once

#include <cstdint>

namespace moonclient {

// ── LE CLIC SUR UN NOM ──────────────────────────────────────────────────────
//
// Le jeu laisse viser un joueur par son NOM : un appui gauche frais sur la
// ligne d'un membre du groupe ou d'un ami, EN LIGNE, agit sur lui comme un clic
// sur son sprite. C'est la fenêtre Groupe / Amis, ses deux onglets ; ni la
// conversation privée, ni la guilde, ni la grille du groupe ne désignent.
//
// Ce que le clic fait, dans l'ordre :
//
// 1. Mort, rien.
// 2. Un nom n'agit que sur un AUTRE joueur, sauf qu'une compétence de soutien
//    armée se lance aussi sur soi.
// 3. Il agit sous MAJ ; ou, une compétence visée armée — offensive ou de
//    soutien —, quand elle se lance sur un joueur sans la touche
//    (`SkillTargetsPlayerWithoutShift`), quand MAJ ou `/noshift` la permettent
//    (`SkillTargetsPlayerUnderShift`), sur une zone de combat libre quand le
//    nom n'est pas celui d'un membre EN LIGNE du groupe, et sur une carte de
//    siège quand le joueur est en vue sans être de ma guilde ni d'une guilde
//    alliée.
// 4. Ni le second appui d'un double-clic, ni ALT tenu.
//
// Alors la souris s'engage — continue sous Ctrl ou `/noctrl`, jamais sur une
// carte de siège — SANS que la sélection change, et l'ordre part comme d'un
// clic : la garde de poids, puis une attaque sans compétence armée, la
// compétence visée sinon, rien pour une visée de case ou de piège.
//
// ⚠ LE NOM NE DEMANDE PAS QUE LE JOUEUR SOIT EN VUE. L'ordre part sur
// l'identifiant ; le tic du personnage cherche sa cible parmi les acteurs, et
// l'abandonne en silence s'il ne la trouve pas — l'engagement retombe avec
// l'action. Hors de portée mais en vue, le personnage s'approche, comme pour un
// clic dans le monde.
//
// Le même appui, comme tout appui frais, désarme ensuite la compétence armée.

// La visée que le grimoire tient au moment de l'appui.
enum class NamedAim : std::uint8_t {
  kNothing,
  // Une compétence visée offensive.
  kOffensive,
  // Une compétence visée de soutien : la seule qui se lance sur son propre nom.
  kSupport,
  // Une visée de case ou de piège : un nom l'engage sans rien lancer.
  kElsewhere,
};

struct NameClickScene {
  NamedAim aim = NamedAim::kNothing;
  // La compétence armée, zéro sans visée.
  std::uint16_t skill_id = 0;
  // Le nom est celui du personnage.
  bool self = false;
  bool dead = false;
  bool shift_held = false;
  bool no_shift = false;
  bool alt_held = false;
  bool second_of_double = false;
  // La carte est une zone de combat libre au sens du type que le serveur
  // annonce : voir `NameClickPkZone`.
  bool pk_zone = false;
  // La carte est une carte de siège : voir `SiegeMap`.
  bool siege_map = false;
  // Le nom est celui d'un membre du groupe, et il est en ligne.
  bool online_party_member = false;
  // Le joueur nommé est en vue — jamais le personnage lui-même.
  bool seen = false;
  // … et il porte la guilde du personnage, ou une guilde alliée.
  bool same_guild = false;
  bool allied_guild = false;
};

enum class NameClickOrder : std::uint8_t {
  // Rien ne s'engage, rien ne part.
  kNothing,
  // La souris s'engage, et rien ne part : une visée de case ou de piège.
  kEngageOnly,
  // La souris s'engage, et une attaque s'arme sur le nom.
  kAttack,
  // La souris s'engage, et la compétence armée vise le nom.
  kSkill,
};

[[nodiscard]] NameClickOrder NameClickOrderOf(const NameClickScene& scene);

// Les compétences qui se lancent sur un joueur nommé sans MAJ : les soutiens.
[[nodiscard]] bool SkillTargetsPlayerWithoutShift(std::uint16_t skill_id);

// Celles qui s'y lancent sous MAJ ou `/noshift`.
[[nodiscard]] bool SkillTargetsPlayerUnderShift(std::uint16_t skill_id);

// LES TYPES DE CARTE (`ZC_NOTIFY_MAPPROPERTY2`) OÙ UN NOM HORS DU GROUPE SE
// VISE SANS MAJ : les deux zones PK libres, avec et sans pénalité, et le champ
// de bataille. Aucun autre paquet de ce protocole ne lève ce droit.
[[nodiscard]] bool NameClickPkZone(std::uint16_t map_type);

// ── LA MÊME VISÉE, SUR UN SPRITE ────────────────────────────────────────────
//
// Une compétence visée armée (offensive ou de soutien) ne part pas sur tout
// joueur ni tout monstre qu'on clique : les deux tables décident encore, mais
// AUTREMENT que pour un nom. Sur un sprite, MAJ INVERSE la première table : un
// soutien part sans la touche et se retient avec elle, un autre sort l'exige.
// Refusé, le clic ne lance rien — la visée se désarme comme sur le sol — et le
// curseur garde l'anneau lent au lieu de l'anneau vif. ALT tenu refuse aussi.

struct AimedPlayerScene {
  std::uint16_t skill_id = 0;
  // La visée est de soutien : la seule qui se lance sur soi.
  bool support = false;
  bool self = false;
  bool shift_held = false;
  bool no_shift = false;
  bool alt_held = false;
  // Le champ de bataille, et le joueur visé est de l'autre camp.
  bool battlefield = false;
  bool other_team = false;
  // Les drapeaux de la carte : le curseur d'attaque au-delà du groupe, au-delà
  // de la guilde ; le siège.
  bool beyond_party = false;
  bool beyond_guild = false;
  bool siege_map = false;
  // Le joueur visé porte le mode PK, la carte est une zone PK (voir
  // `NameClickPkZone`), ou elle lève la visée sans MAJ.
  bool open_field = false;
  bool online_party_member = false;
  bool same_guild = false;
  bool allied_guild = false;
};

// Sur un autre joueur : la première table sous MAJ inversée ; puis la carte.
// Sur un champ de bataille, l'autre camp se vise toujours, et rien d'autre ne
// compte. Ailleurs, hors du groupe là où le groupe borne l'attaque, hors de la
// guilde là où la guilde la borne ; au siège, un ennemi toujours, ma guilde et
// ses alliés par la première table, ou par la seconde sous MAJ ou `/noshift` ;
// hors siège, sur un terrain ouvert, quiconque n'est pas un membre en ligne du
// groupe.
//
// Sur soi : une visée de soutien seulement, par la première table sous MAJ
// inversée, ou d'office sur un terrain ouvert ou sous `/noshift`.
[[nodiscard]] bool AimedSkillReachesPlayer(const AimedPlayerScene& scene);

// Sur un monstre : `/noshift` ouvre ; sinon la première table EXIGE MAJ et
// toute autre compétence la refuse. ALT tenu refuse.
//
// ⚠ `own_guild_spared` NE VAUT QUE POUR LE CURSEUR : un monstre de ma guilde,
// là où la guilde borne l'attaque ou au siège, garde l'anneau lent — mais le
// clic, qui ne regarde pas la guilde, lance quand même.
[[nodiscard]] bool AimedSkillReachesMonster(std::uint16_t skill_id, bool shift_held, bool no_shift,
                                            bool alt_held, bool own_guild_spared);

}  // namespace moonclient
