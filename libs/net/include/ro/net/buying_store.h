#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/buying_store.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// L'ÉCHOPPE D'ACHAT : huit paquets reçus, cinq demandes émises. Voir
// `ro/sim/buying_store.h` pour les deux échoppes que le modèle tient, et
// `ro/sim/buying_store_checks.h` pour ce qu'il faut refuser avant d'émettre.
//
// Les ENSEIGNES ne passent pas ici : elles sont au monde, au-dessus de
// l'acheteur (`ApplyFrame`).
//
// Ce traducteur lit aussi les deux changements de carte, SANS LES CONSOMMER :
// le monde les applique, et lui en tire la fermeture de ce qui ne survit pas au
// voyage. Il rend donc `kUnhandled` pour eux.
[[nodiscard]] FrameOutcome ApplyBuyingStoreFrame(const Frame& frame, sim::BuyingStore& store);

// 🔴 LA VENTE À UN ACHETEUR RETIRE L'OBJET DU SAC, et par ce paquet seul : le
// serveur l'envoie à la place du retrait ordinaire. Ce second traducteur lit la
// même trame que le précédent, et ne touche qu'au sac.
[[nodiscard]] FrameOutcome ApplyBuyingStoreSaleToBag(const Frame& frame, sim::Inventory& bag);

// LES ENSEIGNES D'ACHAT, au monde : l'enseigne se pose sur l'ACHETEUR et se
// retire sous son identifiant, comme celle d'une échoppe de vente. Rend
// `kUnhandled` pour toute autre trame — c'est un maillon de la chaîne du monde.
[[nodiscard]] FrameOutcome ApplyBuyingStoreSignboardFrame(const Frame& frame, sim::World& world);

// LE TITRE TEL QU'IL PARTIRA : une phrase, en 1252 tant qu'elle s'y écrit, en
// UTF-8 sinon, coupée à ce que le serveur en garde — sur une frontière de
// caractère. C'est ce titre-là qu'il faut montrer et valider : c'est celui que
// les autres joueurs liront.
[[nodiscard]] std::string BuyingStoreTitleOnWire(std::string_view title_utf8);

// ── LES VERBES ──────────────────────────────────────────────────────────────
//
// ⚠ ILS ÉMETTENT, ET PRESQUE RIEN DE PLUS : l'échoppe ne s'ouvre qu'à la liste
// du serveur, une vente ne compte qu'à sa confirmation. Ils ne marquent au
// modèle que ce qui empêche d'émettre deux fois.
//
// Ils refusent ce que le serveur ignorerait EN SILENCE, ou ce que le fil
// déformerait ; ce qu'il refuse avec un message part, et c'est lui qui le dira.

// « Ouvre mon échoppe. » Refusé hors préparation, sans ligne, pour une limite
// que le fil ne porte pas, et pour une quantité qui ne tient pas sur deux
// octets. Le modèle passe alors en attente de réponse, avec le titre montré.
[[nodiscard]] core::Result<void> RequestBuyingStoreOpen(
    Connection& link, sim::BuyingStore& store, std::string_view title_utf8, std::int64_t zeny_limit,
    std::span<const sim::BuyingStoreWish> lines);

// « J'y renonce. » Le serveur ne répond rien et n'a rien à défaire ; le client
// d'origine le dit quand même, et la préparation se referme.
[[nodiscard]] core::Result<void> RequestBuyingStoreCancel(Connection& link,
                                                          sim::BuyingStore& store);

// « J'ai fermé mon échoppe. » Voir `sim::BuyingStore::TakeFarewell`.
[[nodiscard]] core::Result<void> SayBuyingStoreClosed(Connection& link);

// « Montre-moi ce que cet acheteur veut. » ⚠ UN ACHETEUR HORS DE VUE, OU QUI
// N'ACHÈTE PLUS, NE VAUT AUCUNE RÉPONSE.
[[nodiscard]] core::Result<void> RequestBuyingStoreList(Connection& link,
                                                        std::uint32_t buyer_account_id);

// « Je vends ceci à l'acheteur que je visite. » Refusé hors visite, sans ligne,
// tant qu'une vente attend sa réponse, et au-delà de ce qu'une trame porte.
[[nodiscard]] core::Result<void> RequestBuyingStoreSale(
    Connection& link, sim::BuyingStore& store, std::span<const sim::BuyingStoreSaleLine> lines);

}  // namespace ro::net
