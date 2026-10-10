#pragma once

// LES RÉCAPITULATIFS QU'ÉCRIT L'OUVERTURE DE MON ÉCHOPPE, dans la conversation,
// tels que les écrit le client d'origine :
//
//   ======================  SALE LIST  ======================   jaune pâle
//       <nom composé>  :  <prix>  Zeny                         jaune
//   Commission : 1 EA = <commission> Zeny                       jaune
//   (… une paire par objet, dans l'ordre de la liste …)
//                                             Current Zeny : <zeny> Z   jaune moyen
//   ============================ ============================  jaune pâle
//
// L'échoppe d'achat écrit la même chose sous « BUY LIST », chaque objet par
// « <nom> : <prix>  Zeny => <quantité> ea. », sans raffinement ni carte.
//
// ⚠ PURES : elles ne lisent que ce qu'on leur donne. La liste est celle que le
// serveur a ACCEPTÉE, pas la saisie.

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "ro/sim/buying_store.h"
#include "ro/sim/vending.h"
#include "ro/vfs/item_words.h"

#include "game/vending_said.h"

namespace moonclient {

// Les couleurs du récapitulatif (0xRRGGBB) : le cadre, chaque objet, le zeny.
inline constexpr std::uint32_t kRecapFrameColor = 0xFFFF96;
inline constexpr std::uint32_t kRecapItemColor = 0xFFFF5A;
inline constexpr std::uint32_t kRecapZenyColor = 0xFFFF78;

// La commission ANNONCÉE pour un exemplaire : cinq pour cent au-delà de dix
// millions de zeny l'unité, rien en deçà, tronquée au zeny.
//
// ⚠ C'EST LA RÈGLE QU'AFFICHE LE CLIENT, PAS CELLE DU SERVEUR : Moonlight taxe
// cinq pour cent d'un ACHAT dont le total atteint cent millions.
[[nodiscard]] std::int64_t AnnouncedCommission(std::int64_t unit_price_zeny);

// Un objet du récapitulatif : son nom déjà composé, son prix unitaire, et la
// quantité (lue par la seule échoppe d'achat).
struct TradeRecapItem {
  std::string name;
  std::int64_t unit_price_zeny = 0;
  std::int64_t amount = 0;
};

// Les lignes, dans l'ordre où elles s'écrivent. Un texte que la langue n'a pas
// su imprimer reste vide, et ne s'écrit pas.
[[nodiscard]] std::vector<VendingChatLine> SaleListRecap(const ro::vfs::ItemWords& words,
                                                         std::span<const TradeRecapItem> items,
                                                         std::int64_t zeny);
[[nodiscard]] std::vector<VendingChatLine> BuyListRecap(const ro::vfs::ItemWords& words,
                                                        std::span<const TradeRecapItem> items,
                                                        std::int64_t zeny);

// QUAND ÉCRIRE : à l'ouverture RÉUSSIE qu'on n'a pas encore dite, d'une échoppe
// que CE client a demandée — une échoppe retrouvée sans demande n'a pas de
// liste fraîchement saisie à rappeler. `said_seq` est le rang de la dernière
// nouvelle dite : le résultat d'ouverture pour la vente, la liste du serveur
// pour l'achat (un refus y arrive par un autre paquet, et ne la change pas).
[[nodiscard]] bool SaleListRecapDue(const ro::sim::Vending& vending, std::uint32_t said_seq);
[[nodiscard]] bool BuyListRecapDue(const ro::sim::BuyingStore& store, std::uint32_t said_seq);

}  // namespace moonclient
