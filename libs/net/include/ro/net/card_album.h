#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/card_album.h"

namespace ro::net {

// L'ALBUM DE CARTES SUR LE FIL — `ZC 0x0F33` et `CZ 0x0F34`, réservés à un
// client reconnu par la poignée de main ET qui a annoncé la surface d'album
// (`CZ 0x0F24`, voir `ro/net/ui_caps.h`). Sans l'annonce, le serveur REFUSE toute commande
// d'album, et c'est voulu : un sacrifice est irréversible, et un client qui
// n'aurait aucune surface pour en montrer le résultat ne doit pas en payer un.
//
//   ZC 0x0F33 : [op:2][len:2][result:1][count:2] puis count entrées de
//               [card_id:4][reserve:2][flags:1][equip:4] et, chez qui a
//               annoncé `kUiCapCardAlbumDropper`, [dropper:1] en queue.
//   CZ 0x0F34 : [op:2][len:2][cmd:1][arg:4][amount:2]
//
// 🔴 LA TAILLE D'UNE ENTRÉE SE DÉDUIT DE LA TRAME, elle ne s'écrit pas en dur :
// client et serveur ne se déploient jamais à la même seconde. Un client neuf
// devant un serveur d'avant lirait sinon tout le catalogue de travers — chaque
// entrée décalée d'un octet de plus que la précédente — au lieu de perdre la
// seule couleur qu'il ne pouvait pas connaître.

// Le bloc fixe de `ZC 0x0F33`, et les deux tailles d'entrée connues.
inline constexpr std::size_t kCardAlbumFixedSize = 7;
inline constexpr std::size_t kCardAlbumEntrySize = 11;
inline constexpr std::size_t kCardAlbumEntryWithDropperSize = 12;

// Le bit 0 des drapeaux d'une entrée : la pochette est ouverte.
inline constexpr std::uint8_t kCardAlbumUnlockedFlag = 0x01;

// LES COMMANDES — miroir exact de `e_card_album_cmd` côté serveur. Un
// identifiant ajouté ici sans l'être là-bas est ignoré sans un mot.
enum class CardAlbumCommand : std::uint8_t {
  // Renvoyer l'état complet, ne rien modifier. ⚠ C'EST AUSSI CE QUI PREND LE
  // VERROU : un seul compte de jeu du même compte Moonlight tient l'album.
  kRefresh = 0,
  // `arg` = place d'inventaire CLIENT ; sacrifie UNE copie.
  kUnlock = 1,
  // `arg` = place d'inventaire CLIENT.
  kPut = 2,
  // `arg` = identifiant SERVEUR de la carte.
  kGet = 3,
  // La fenêtre se ferme : rendre le verrou. ⚠ SANS RÉPONSE.
  kClose = 4,
};

// Ce qu'une trame `ZC 0x0F33` dit, décodé sans rien toucher.
struct CardAlbumReply {
  sim::CardAlbumResult result = sim::CardAlbumResult::kOk;
  std::vector<sim::CardAlbumCard> cards;
  bool dropper_known = false;
};

// Décode une trame `ZC 0x0F33`. Rien si elle n'en est pas une, ou si son bloc
// fixe est tronqué.
[[nodiscard]] std::optional<CardAlbumReply> DecodeCardAlbum(std::span<const std::uint8_t> frame);

// Applique une réponse d'album.
[[nodiscard]] FrameOutcome ApplyCardAlbumFrame(const Frame& frame, sim::CardAlbum& album);

// Émet une commande d'album. ⚠ ELLE N'APPLIQUE RIEN d'avance : la réponse du
// serveur porte l'état complet, et afficher un sacrifice qu'il n'a pas confirmé
// serait mentir sur un paiement.
[[nodiscard]] core::Result<void> RequestCardAlbum(Connection& link, CardAlbumCommand command,
                                                  std::uint32_t arg, std::uint16_t amount);

}  // namespace ro::net
