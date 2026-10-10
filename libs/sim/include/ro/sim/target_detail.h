#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "ro/sim/status_effects.h"

namespace ro::sim {

// De quelle espèce est l'entité désignée.
//
// ⚠ CE N'EST PAS LE MASQUE `bl_type` DU SERVEUR, qui ne tiendrait pas sur un
// octet : c'est une énumération dense, faite pour le fil.
enum class TargetKind : std::uint8_t {
  kUnknown = 0,
  kPlayer = 1,
  kMonster = 2,
  kNpc = 3,
  kHomunculus = 4,
  kMercenary = 5,
  kPet = 6,
  kElemental = 7,
};

// Les classes de monstre, telles que `e_aegis_monsterclass` les numérote.
// ⚠ QUATRE NE MANQUE PAS PAR ERREUR : le serveur saute le trois.
inline constexpr std::uint8_t kMonsterClassNormal = 0;
inline constexpr std::uint8_t kMonsterClassBoss = 1;
inline constexpr std::uint8_t kMonsterClassGuardian = 2;
inline constexpr std::uint8_t kMonsterClassBattlefield = 4;
inline constexpr std::uint8_t kMonsterClassEvent = 5;

// Le plus grand rang que l'énumération du serveur emploie. Au-delà, on ne sait
// pas — et on le dit, plutôt que de ranger une entité dans la mauvaise espèce.
inline constexpr std::uint8_t kLastTargetKind = 7;

// Ce que le serveur dit d'une entité DÉSIGNÉE, quand on le lui demande.
//
// 🔴 CHAQUE CHAMP EST FACULTATIF, ET CE N'EST PAS DE LA PRUDENCE. Le serveur
// envoie un masque qui dit ce qu'il a RENSEIGNÉ, et il en tait délibérément :
// sur un adversaire, ses points de vie et de magie ne partent pas — c'est une
// information de jeu, elle ne se donne pas. « Zéro point de vie » et « points de
// vie inconnus » ne se ressemblent pas à l'écran : le premier annonce une mort
// qui n'a pas eu lieu.
//
// ⚠ ET LE SERVEUR NE GARDE RIEN. Il n'y a pas d'abonnement : ce dossier est la
// dernière réponse reçue, et c'est au client de redemander.
struct TargetDetail {
  // Zéro tant que rien n'a été demandé. ⚠ IL NOMME L'ENTITÉ À LAQUELLE LES
  // CHAMPS SE RAPPORTENT : sans lui, une réponse en retard se lirait comme une
  // description de la cible suivante.
  std::uint32_t id = 0;

  // Vrai dès que le couple de l'état a répondu pour cette entité : c'est ce qui
  // sépare « hors de portée » de « pas encore de réponse ».
  bool told = false;

  // Faux quand le serveur répond « hors de portée ou inexistante ». C'est ce qui
  // sépare « aucun état » du SILENCE d'une entité qu'on ne voit plus.
  bool reachable = false;

  TargetKind kind = TargetKind::kUnknown;

  std::optional<std::int16_t> level;
  std::optional<std::uint32_t> health;
  std::optional<std::uint32_t> max_health;
  std::optional<std::uint32_t> magic;
  std::optional<std::uint32_t> max_magic;

  // Race, élément et taille arrivent ensemble, sous un seul bit du masque :
  // les séparer laisserait croire qu'ils peuvent manquer l'un sans l'autre.
  struct Nature {
    std::uint8_t race = 0;
    std::uint8_t element = 0;
    std::uint8_t element_level = 0;
    std::uint8_t size = 0;

    // 🔴 CE N'EST PAS UN BOOLÉEN, C'EST UNE CLASSE. Le serveur y met
    // `status_get_class_`, donc `e_aegis_monsterclass` : normal, boss, gardien,
    // champ de bataille, événement. Le lire comme « non nul = boss » ferait
    // passer un GARDIEN pour un MVP — et rien ne le dirait, les deux se
    // dessinant de la même façon.
    std::uint8_t monster_class = 0;
  };
  std::optional<Nature> nature;

  // ── LES ÉTATS, qui viennent d'un AUTRE paquet ────────────────────────────
  //
  // 🔴 VIDE N'EST PAS INCONNU, et le drapeau existe pour cela. Une liste vide
  // veut dire « aucun état », ce qui est une information ; pas de liste du tout
  // veut dire qu'on n'a pas encore demandé, ou qu'on n'a pas eu de réponse.
  bool statuses_told = false;
  std::vector<ActiveStatus> statuses;
  // Combien de fois le serveur a refusé de dire ses états. Qui redemande compte
  // les refus, pour se taire un moment après chacun.
  std::uint32_t statuses_refusals = 0;
};

}  // namespace ro::sim
