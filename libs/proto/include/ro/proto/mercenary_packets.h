#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// LES PAQUETS DU MERCENAIRE : sa fiche entière, une valeur qui change, la liste
// de ses compétences, et l'ordre que le joueur lui donne.
//
// Le serveur les écrit à la main, champ par champ : leur disposition est ici,
// décalage par décalage.

// LA FICHE ENTIÈRE, à l'invocation et à chaque entrée sur une carte.
//
//     029B <id>.L <atk>.W <matk>.W <hit>.W <crit>.W <def>.W <mdef>.W <flee>.W
//          <amotion>.W <nom>.24B <niveau>.W <pv>.L <pv max>.L <sp>.L <sp max>.L
//          <expiration>.L <fidélité>.W <appels>.L <victimes>.L <portée>.W
struct MercenaryInfo {
  static constexpr std::uint16_t kOpcode = 0x029B;
  static constexpr std::size_t kWireSize = 80;
  static constexpr std::size_t kNameSize = 24;

  // L'identifiant de l'ACTEUR du mercenaire dans le monde.
  std::uint32_t actor_id = 0;
  // ⚠ L'ATTAQUE EST TIRÉE AU HASARD par le serveur dans la fourchette de
  // l'arme, à chaque envoi : deux fiches successives ne la donnent pas égale.
  std::uint16_t atk = 0;
  std::uint16_t matk = 0;
  std::uint16_t hit = 0;
  // Le taux de critique, déjà divisé par dix.
  std::uint16_t crit = 0;
  std::int16_t def = 0;
  std::int16_t mdef = 0;
  std::uint16_t flee = 0;
  // Le délai entre deux attaques, en millisecondes. L'ASPD s'en déduit.
  std::uint16_t amotion_ms = 0;
  // Le nom de la base du serveur, dans la page de codes du jeu.
  std::string name;
  std::uint16_t level = 0;
  std::int32_t hp = 0;
  std::int32_t max_hp = 0;
  std::int32_t sp = 0;
  std::int32_t max_sp = 0;
  // 🔴 LA FIN DU CONTRAT EN SECONDES DEPUIS L'ÉPOQUE UNIX, à l'horloge du
  // serveur : un instant absolu, et non une durée restante.
  std::uint32_t expire_unix_s = 0;
  std::int16_t faith = 0;
  std::uint32_t calls = 0;
  std::uint32_t kills = 0;
  // La portée d'attaque, en cases.
  std::uint16_t range_cells = 0;
};

// UNE VALEUR QUI CHANGE : `02A2 <variable>.W <valeur>.L`.
struct MercenaryStatus {
  static constexpr std::uint16_t kOpcode = 0x02A2;
  static constexpr std::size_t kWireSize = 8;

  std::uint16_t variable = 0;
  std::int32_t value = 0;
};

// Les variables que ce paquet porte, dans la numérotation du serveur.
inline constexpr std::uint16_t kMercenaryVarHp = 5;
inline constexpr std::uint16_t kMercenaryVarMaxHp = 6;
inline constexpr std::uint16_t kMercenaryVarSp = 7;
inline constexpr std::uint16_t kMercenaryVarMaxSp = 8;
inline constexpr std::uint16_t kMercenaryVarAtk = 41;
inline constexpr std::uint16_t kMercenaryVarMatk = 43;
inline constexpr std::uint16_t kMercenaryVarDef = 45;
inline constexpr std::uint16_t kMercenaryVarMdef = 47;
inline constexpr std::uint16_t kMercenaryVarHit = 49;
inline constexpr std::uint16_t kMercenaryVarCrit = 52;
inline constexpr std::uint16_t kMercenaryVarAmotion = 53;
inline constexpr std::uint16_t kMercenaryVarFlee = 165;
inline constexpr std::uint16_t kMercenaryVarKills = 189;
inline constexpr std::uint16_t kMercenaryVarFaith = 190;

// UNE COMPÉTENCE DE LA LISTE.
struct MercenarySkillEntry {
  std::uint16_t skill_id = 0;
  // Les drapeaux de visée du serveur (e_skill_inf).
  std::uint32_t targeting = 0;
  std::uint16_t level = 0;
  std::uint16_t sp_cost = 0;
  std::uint16_t range_cells = 0;
  // Le nom INTERNE du serveur, « MS_BASH ».
  std::string name;
  // ⚠ TOUJOURS FAUX sur ce serveur : un mercenaire ne monte pas de compétence.
  bool upgradable = false;
};

// LA LISTE DES COMPÉTENCES : un en-tête de quatre octets, puis 37 par entrée.
//
//     029D <longueur>.W { <id>.W <visée>.L <niveau>.W <sp>.W <portée>.W
//                         <nom>.24B <montable>.B }*
struct MercenarySkillList {
  static constexpr std::uint16_t kOpcode = 0x029D;
  static constexpr std::size_t kHeaderSize = 4;
  static constexpr std::size_t kEntrySize = 37;
  static constexpr std::size_t kNameSize = 24;

  std::vector<MercenarySkillEntry> skills;
};

// L'ORDRE DU JOUEUR : `029F <commande>.B`.
struct MercenaryCommand {
  static constexpr std::uint16_t kOpcode = 0x029F;
  static constexpr std::size_t kWireSize = 3;

  std::uint8_t command = 0;
};

// « Statut du mercenaire ». ⚠ LE SERVEUR L'IGNORE : il ne lit que le renvoi, et
// ce paquet n'a aucun effet. Le client l'envoie quand même en ouvrant la fiche.
inline constexpr std::uint8_t kMercenaryCommandInfo = 1;
// RENVOYER le mercenaire : le contrat prend fin sur-le-champ, sans message.
inline constexpr std::uint8_t kMercenaryCommandDismiss = 2;

// ⚠ CHAQUE DÉCODEUR REFUSE une trame d'un autre opcode ou d'une autre longueur
// que la sienne.
[[nodiscard]] core::Result<MercenaryInfo> DecodeMercenaryInfo(std::span<const std::uint8_t> frame);
[[nodiscard]] core::Result<MercenaryStatus> DecodeMercenaryStatus(
    std::span<const std::uint8_t> frame);
// Refuse en plus une queue qui ne se découpe pas en entrées entières.
[[nodiscard]] core::Result<MercenarySkillList> DecodeMercenarySkillList(
    std::span<const std::uint8_t> frame);

// ⚠ REFUSE d'encoder quand la disposition écrite ici ne correspond plus à ce
// que le serveur déclare lire.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> EncodeMercenaryCommand(
    const MercenaryCommand& command);

// L'ENVERS DES DÉCODEURS, pour les essais : le client n'émet pas ces paquets.
[[nodiscard]] std::vector<std::uint8_t> EncodeMercenaryInfo(const MercenaryInfo& info);
[[nodiscard]] std::vector<std::uint8_t> EncodeMercenaryStatus(const MercenaryStatus& status);
[[nodiscard]] std::vector<std::uint8_t> EncodeMercenarySkillList(const MercenarySkillList& list);

}  // namespace ro::proto
