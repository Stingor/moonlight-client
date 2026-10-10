#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "ro/core/error.h"

namespace ro::proto {

// ── LA RECHERCHE D'ÉCHOPPES : les trois demandes qui n'ont pas de structure ────
//
// Les réponses du serveur et le clic sur un résultat (`CzSsilistItemClick`) sont
// décrits par le serveur et engendrés dans `map_packets.h`. La demande de
// recherche y figure aussi, mais SANS opcode ni encodeur : le serveur la lit par
// une table de décalages. Elle est donc écrite ici, avec la page suivante et la
// fermeture, qu'il lit sans structure du tout.

// « Cherche ces objets dans les échoppes. »
//
// La queue porte d'abord les objets, puis les cartes, quatre octets chacun :
// le serveur coupe la liste au compte d'objets annoncé.
//
// ⚠ LE PRIX MAXIMAL PRÉCÈDE LE MINIMAL sur le fil. Le serveur les échange
// s'ils sont à l'envers, PUIS lit zéro comme « sans borne » : un maximum nul
// sous un minimum non nul devient donc la borne basse. Voir
// `sim::StoreSearchPlan`, qui ne l'envoie jamais ainsi.
struct StoreSearchRequest {
  static constexpr std::uint16_t kOpcode = 0x0835;
  // Opcode, longueur, sorte, prix maximal, prix minimal, deux comptes.
  static constexpr std::size_t kHeaderSize = 15;
  static constexpr std::size_t kIdSize = 4;

  // 0 : les échoppes de vente ; 1 : les échoppes d'achat.
  std::uint8_t search_type = 0;
  std::uint32_t max_price_zeny = 0;
  std::uint32_t min_price_zeny = 0;
  std::vector<std::uint32_t> item_ids;
  std::vector<std::uint32_t> card_ids;
};

// « La page suivante des résultats. » Le serveur n'envoie rien s'il n'en a plus.
struct StoreSearchNextPageRequest {
  static constexpr std::uint16_t kOpcode = 0x0838;
  static constexpr std::size_t kWireSize = 2;
};

// « Je ferme la recherche. » Le serveur ne répond pas ; il oublie les
// résultats ET les essais qui restaient.
struct StoreSearchCloseRequest {
  static constexpr std::uint16_t kOpcode = 0x083B;
  static constexpr std::size_t kWireSize = 2;
};

// ⚠ REFUSE plus de 255 objets ou cartes : chaque compte tient sur un octet.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(const StoreSearchRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(
    const StoreSearchNextPageRequest& request);
[[nodiscard]] core::Result<std::vector<std::uint8_t>> Encode(
    const StoreSearchCloseRequest& request);

}  // namespace ro::proto
