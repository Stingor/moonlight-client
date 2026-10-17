#pragma once

#include <cstdint>
#include <string>

namespace ro::sim {

// CE QU'UN ACTEUR AFFICHE AU-DESSUS DE LUI, et qui n'est pas son nom : une
// échoppe qu'il tient, ou un salon qu'il a ouvert.
//
// ⚠ DEUX PAQUETS, DEUX ESPACES D'IDENTIFIANTS, ET ILS SE RESSEMBLENT. L'enseigne
// d'une échoppe nomme son VENDEUR, et se retire sous ce même identifiant
// (ZC_STORE_ENTRY puis ZC_DISAPPEAR_ENTRY). Un salon, lui, porte un identifiant
// À LUI, distinct de celui de son propriétaire, et c'est par celui-là qu'il se
// ferme (ZC_DESTROY_ROOM). Les confondre retirerait l'enseigne d'un autre.
//
// L'ÉCHOPPE D'ACHAT suit la règle de l'échoppe de vente : elle nomme son
// ACHETEUR, et se retire sous ce même identifiant (ZC_BUYING_STORE_ENTRY puis
// ZC_DISAPPEAR_BUYING_STORE_ENTRY).
enum class SignboardKind : std::uint8_t { kVendingShop, kChatRoom, kBuyingStore };

// L'état d'un salon, tel que le serveur le numérote : e_status_chatroom, dans
// src/map/clif.hpp.
//
// ⚠ ZÉRO EST « PRIVÉ », ET UN « PUBLIC ». L'ordre paraît inversé et ne l'est
// pas : c'est celui du serveur, et le deviner à l'endroit ferait peindre les
// salons fermés comme ouverts.
inline constexpr std::uint8_t kRoomPrivate = 0;
inline constexpr std::uint8_t kRoomPublic = 1;
inline constexpr std::uint8_t kRoomArena = 2;
inline constexpr std::uint8_t kRoomPkZone = 3;

struct Signboard {
  SignboardKind kind = SignboardKind::kVendingShop;
  std::string title;
  // L'identifiant DU SALON, jamais celui de son propriétaire. Zéro pour une
  // échoppe, qui n'en a pas.
  std::uint32_t room_id = 0;
  // Combien de monde s'y trouve, et combien il en tient. Les deux valent zéro
  // pour une échoppe : le serveur ne dit pas combien d'objets elle vend.
  std::uint16_t users = 0;
  std::uint16_t limit = 0;
  std::uint8_t status = kRoomPrivate;
};

}  // namespace ro::sim
