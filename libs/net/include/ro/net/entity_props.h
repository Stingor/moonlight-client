#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"
#include "ro/net/connection.h"

namespace ro::net {

// ── CE QUE LE SERVEUR SAIT D'UNE ENTITÉ : l'inspecteur du staff ─────────────
//
//   CZ 0x0F22 [op][len:2][gid:4]
//   ZC 0x0F23 [op][len:2][gid:4][statut:1][nombre:1]
//             puis, `nombre` fois : [longueur:1][clé][longueur:1][valeur]
//
// Réservé aux clients ayant donné leur poignée de main (ADR 0040), sans bit de
// capacité. Le GID est celui que le client voit : l'AID d'un joueur,
// l'identifiant d'objet de tout le reste.
//
// 🔴 LA GARDE EST CELLE DU SERVEUR : niveau de groupe 80. Un compte en dessous
// reçoit le statut « refusé » et rien d'autre — la réponse porte des numéros de
// personnage et des chemins de scripts.
//
// 🔴 LE CORPS EST UNE LISTE CLÉ / VALEUR SANS SCHÉMA, écrite par le serveur
// selon le genre de l'entité, et montrée telle quelle, sans catalogue de
// traduction. Une paire à valeur vide est un TITRE DE SECTION.

// Le statut de la réponse, tel que le fil le porte. Un numéro inconnu se lit
// comme une entité introuvable.
enum class EntityPropsStatus : std::uint8_t {
  kFound = 0,
  kMissing = 1,
  kDenied = 2,
};

// Une paire de la réponse, en UTF-8.
struct EntityProp {
  std::string key;
  // Vide : la clé est un titre de section.
  std::string value;
};

struct EntityPropsReply {
  // L'identifiant DEMANDÉ, renvoyé en écho : c'est lui qui apparie la réponse.
  std::uint32_t gid = 0;
  EntityPropsStatus status = EntityPropsStatus::kMissing;
  // Vide hors du statut « trouvée ».
  std::vector<EntityProp> props;
};

// La taille de l'en-tête fixe, opcode compris.
inline constexpr std::size_t kEntityPropsHeaderSize = 10;
// L'opcode de la réponse, pour qui doit reconnaître une trame qui ne se décode
// pas.
inline constexpr std::uint16_t kEntityPropsReplyOpcode = 0x0F23;

// « Que sais-tu de cette entité ? »
[[nodiscard]] core::Result<void> RequestEntityProps(Connection& link, std::uint32_t gid);

// La réponse, ou rien quand la trame n'en est pas une ou que son en-tête est
// incomplet.
//
// 🔴 LA LISTE EST CRUE, MAIS PAS SUR PAROLE : une paire qui ne tient pas entière
// dans la trame arrête la lecture, et ce qui précède reste. Une clé entière dont
// la valeur manque est gardée seule — un titre, ce qui ne ment pas.
[[nodiscard]] std::optional<EntityPropsReply> DecodeEntityProps(
    std::span<const std::uint8_t> frame);
[[nodiscard]] std::optional<EntityPropsReply> ReadEntityProps(const Frame& frame);

}  // namespace ro::net
