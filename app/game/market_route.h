#pragma once

// CE QUE LE MARCHÉ FAIT DE CE QUE SA FENÊTRE A RENDU ET DE CE QUE LE SERVEUR
// A DIT, à part de son étage : des décisions pures, qui se jugent sur un banc
// sans fil ni fenêtre. L'étage (`MarketStage`) les exécute.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "ro/sim/actor.h"
#include "ro/sim/market.h"
#include "ro/ui/market_window.h"

namespace moonclient {

// OÙ « Y ALLER » MÈNE : la carte et la case d'une échoppe.
struct MarketDestination {
  std::string map_name;
  int cell_x = 0;
  int cell_y = 0;
};

[[nodiscard]] MarketDestination MarketDestinationOf(const ro::sim::MarketOffer& offer);

// CE QUE LA RÉPONSE DE LA FENÊTRE DEMANDE.
struct MarketRoute {
  // Une recherche neuve : la première page revient.
  bool search = false;
  // La même recherche, redemandée.
  bool refresh = false;
  // Le guidage jusqu'à une échoppe.
  std::optional<MarketDestination> go_to;
  // Le contenu d'une échoppe, à demander ; et l'offre qui l'a désignée.
  std::optional<ro::sim::MarketQuery> store_query;
  const ro::sim::MarketOffer* store_offer = nullptr;
};

// `offers` : la réponse montrée ; `page_ranks` : les rangs, dans `offers`, des
// lignes de la page que la fenêtre a dessinée ; `kind` : la sorte de cette
// réponse. Un rang de ligne hors de la page ne désigne rien.
[[nodiscard]] MarketRoute RouteMarketAnswer(const ro::ui::MarketAction& answer,
                                            std::span<const ro::sim::MarketOffer> offers,
                                            std::span<const std::size_t> page_ranks,
                                            ro::sim::MarketKind kind);

// CE QU'UN REFUS DU SERVEUR FAIT DE LA FENÊTRE.
enum class MarketRefusalStep : std::uint8_t {
  // Hors d'une ville : la fenêtre se ferme, et le refus se dit dans une boîte.
  kCloseWithNotice,
  // Le refus s'écrit dans la fenêtre du contenu d'une échoppe.
  kStoreStatus,
  // Le refus s'écrit au pied du Marché.
  kStatus,
};

[[nodiscard]] MarketRefusalStep StepOfMarketRefusal(ro::sim::MarketStatus status, bool for_store);

// COMPARER UNE ÉCHOPPE : celle de `owner_aid`, de vente ou d'achat.
struct MarketCompare {
  std::uint32_t owner_aid = 0;
  ro::sim::MarketKind kind = ro::sim::MarketKind::kVending;
  // Le nom de son marchand, pour le pied ; vide quand on ne le sait pas.
  std::string owner_name;
};

// CE QU'UNE OUVERTURE DEMANDE : une saisie, ou la comparaison d'une échoppe.
struct MarketOpening {
  std::string query;
  std::optional<MarketCompare> compare;
};

// LA SORTE QU'UNE ENSEIGNE COMPARE : vente ou achat ; rien pour un salon.
[[nodiscard]] std::optional<ro::sim::MarketKind> MarketKindOfSignboard(
    ro::sim::SignboardKind signboard);

// L'ONGLET d'une sorte.
[[nodiscard]] int MarketTabOf(ro::sim::MarketKind kind);

// LA SAISIE D'UNE OUVERTURE : l'identifiant de l'item visé, ou rien.
[[nodiscard]] std::string MarketOpeningQuery(std::optional<std::uint32_t> item_id);

// LE PRIX MONTRÉ D'UNE OFFRE, avec sa monnaie : « 1,500 z », « 200 cash »,
// « 3 x Jellopy », « 50 #KAFRAPOINTS », « troc + 100 z ». `currency_item_name`
// nomme l'item qui sert de monnaie à un itemshop.
[[nodiscard]] std::string MarketPriceText(const ro::sim::MarketOffer& offer,
                                          std::string_view currency_item_name);

// LA QUANTITÉ MONTRÉE : chez un PNJ, un stock nul est illimité, et s'écrit d'un
// tiret.
[[nodiscard]] std::string MarketAmountText(const ro::sim::MarketOffer& offer,
                                           ro::sim::MarketKind kind);

// `/marche` : ouvre le Marché. ⚠ UNE COMMANDE DE CE CLIENT, que le client
// d'origine n'a pas : elle ne part jamais au serveur. La ligne entière compte,
// sans la casse.
inline constexpr std::string_view kMarketCommand = "/marche";
[[nodiscard]] bool IsMarketCommand(std::string_view line);

}  // namespace moonclient
