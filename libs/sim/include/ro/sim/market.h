#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/inventory.h"
#include "ro/sim/store_search.h"

namespace ro::sim {

// ── LE MARCHÉ ────────────────────────────────────────────────────────────────
//
// Toutes les échoppes ouvertes du serveur — de vente ou d'achat, marchands hors
// ligne compris — et les boutiques de PNJ, sur toutes les cartes, demandées par
// `CZ 0x0F41` et reçues par `ZC 0x0F42` (ADR 0114). Il remplace la recherche
// d'échoppes du client d'origine.
//
// 🔴 PARTOUT : aucune condition de carte ni de catalogue. Un Universal Catalog
// n'est qu'un raccourci qui ouvre la fenêtre, et le serveur ne le consomme pas.
// 🔴 LECTURE SEULE : rien ici n'achète ni ne vend. « Y aller » guide jusqu'à
// l'échoppe, où l'achat se fait comme d'habitude.
//
// Le serveur FILTRE ; le client TRIE et PAGINE. Une réponse arrive en plusieurs
// trames, et ne se montre qu'entière.

enum class MarketKind : std::uint8_t {
  kVending = 0,
  kBuying = 1,
  // Les boutiques de PNJ : le prix de leur liste, dans leur monnaie.
  kNpc = 2,
};

// LA MONNAIE D'UNE OFFRE, telle que le fil la porte. Une échoppe de joueur se
// paie toujours en zeny.
enum class MarketCurrency : std::uint8_t {
  kZeny = 0,
  kCash = 1,
  // Un item : `MarketOffer::currency_item`.
  kItem = 2,
  // Des points : `MarketOffer::currency_name`, le nom de leur variable.
  kPoints = 3,
  // Un troc : des items, et le prix en zeny en plus.
  kBarter = 4,
};

// LES FAMILLES D'OBJET du filtre, dans l'ordre de la liste déroulante et telles
// que le fil les porte.
enum class MarketCategory : std::uint8_t {
  kAny = 0,
  kUsable = 1,
  kWeapon = 2,
  kArmor = 3,
  kCard = 4,
  kEtc = 5,
  kAmmo = 6,
  kPet = 7,
};
inline constexpr std::size_t kMarketCategoryCount = 8;

// Ce que dit le serveur, par son code sur le fil.
enum class MarketStatus : std::uint8_t {
  kOk = 0,
  // Le serveur ouvre la fenêtre : un catalogue, `@shopsearch`.
  kOpen = 1,
  // Le code 2 est RÉSERVÉ : le serveur ne l'émet pas, et une trame qui le porte
  // ne se décode pas. Il ne se réattribue pas.
  kTooSoon = 3,
  // L'échoppe dont on voulait le contenu n'est plus ouverte.
  kStoreGone = 4,
  kBadRequest = 5,
};

// Les drapeaux de la demande.
inline constexpr std::uint8_t kMarketWithCards = 0x01;
inline constexpr std::uint8_t kMarketWithOptions = 0x02;
// COMPARER l'échoppe de `owner_aid` : le serveur cherche tous les items qu'elle
// porte. Ses propres lignes restent dans la réponse, comme référence.
inline constexpr std::uint8_t kMarketCompare = 0x04;

// Le plus d'identifiants qu'une demande porte : un nom qui en désigne davantage
// est trop vague.
inline constexpr std::size_t kMarketMostIds = 200;
// Le plus haut raffinement que le filtre propose.
inline constexpr int kMarketHighestRefine = 20;
// Les lignes d'une page de la fenêtre.
inline constexpr std::size_t kMarketRowsPerPage = 20;

// UNE OFFRE : une ligne d'une échoppe.
struct MarketOffer {
  // L'identifiant de l'échoppe, qui la redemande pour « Voir l'échoppe ».
  // ⚠ CE N'EST PAS UN ACTEUR.
  std::uint32_t store_id = 0;
  // Le compte du marchand.
  std::uint32_t owner_aid = 0;
  // L'objet tel qu'il se vend ou s'achète.
  InventoryItem item;
  // Le prix UNITAIRE.
  std::uint32_t price_zeny = 0;
  // Le marchand est hors ligne.
  bool autotrade = false;
  // Le nom interne de la carte, et la case de l'échoppe.
  std::string map_name;
  int cell_x = 0;
  int cell_y = 0;
  // Le titre de l'échoppe, ou le nom du PNJ, en UTF-8.
  std::string shop_name;
  // ⚠ CHEZ UN PNJ, `item.amount` est un stock — celui d'un marketshop ou d'un
  // troc limité — et zéro veut dire « illimité ».
  MarketCurrency currency = MarketCurrency::kZeny;
  std::uint32_t currency_item = 0;
  std::string currency_name;
};

// CE QUI PART AU SERVEUR.
struct MarketQuery {
  std::uint16_t query_id = 0;
  MarketKind kind = MarketKind::kVending;
  MarketCategory category = MarketCategory::kAny;
  std::uint8_t refine_min = 0;
  std::uint8_t refine_max = kMarketHighestRefine;
  std::uint8_t flags = 0;
  std::uint32_t price_min_zeny = 0;
  // Zéro : sans plafond.
  std::uint32_t price_max_zeny = 0;
  // Non nul : le contenu de cette échoppe seule, filtres ignorés.
  std::uint32_t store_id = 0;
  std::uint32_t owner_aid = 0;
  // Vide : tous les objets.
  std::vector<std::uint32_t> item_ids;
  // UNE DEMANDE À PART, que pose une autre fenêtre — « Vendu chez » de la base
  // de données. Sa réponse ne remplace pas la liste du Marché. Rien sur le fil.
  bool aside = false;
};

// UNE TRAME DE LA RÉPONSE, telle que le fil la porte.
struct MarketReplyPart {
  std::uint16_t query_id = 0;
  MarketStatus status = MarketStatus::kOk;
  MarketKind kind = MarketKind::kVending;
  std::uint16_t part = 0;
  std::uint16_t part_count = 1;
  std::uint16_t total = 0;
  bool truncated = false;
  std::uint32_t store_id = 0;
  std::uint32_t owner_aid = 0;
  // L'OUVERTURE : la saisie à poser — un ID, un nom, ou rien.
  std::string query;
  std::vector<MarketOffer> offers;
};

// UNE RÉPONSE ATTENDUE, qui se remplit trame à trame.
class MarketAnswer {
 public:
  // Une demande vient de partir : ce qui précédait s'oublie.
  void Await(const MarketQuery& query);
  // Une trame de la réponse : vrai si elle est de la demande attendue.
  bool Take(MarketReplyPart part);
  // Le serveur a refusé la demande : on ne l'attend plus, et rien ne se montre.
  void Abandon() noexcept { waiting_ = false; }

  [[nodiscard]] std::uint16_t query_id() const noexcept { return query_id_; }
  [[nodiscard]] MarketKind kind() const noexcept { return kind_; }
  [[nodiscard]] std::uint32_t store_id() const noexcept { return store_id_; }
  [[nodiscard]] std::uint32_t owner_aid() const noexcept { return owner_aid_; }
  // Une demande est partie et sa réponse n'est pas entière.
  [[nodiscard]] bool waiting() const noexcept { return waiting_; }
  // Toutes les trames sont là, et les offres se montrent.
  [[nodiscard]] bool complete() const noexcept { return complete_; }
  [[nodiscard]] bool truncated() const noexcept { return truncated_; }
  [[nodiscard]] std::span<const MarketOffer> offers() const noexcept {
    return complete_ ? std::span<const MarketOffer>(offers_) : std::span<const MarketOffer>();
  }
  // Change quand la réponse devient entière.
  [[nodiscard]] std::uint32_t complete_seq() const noexcept { return complete_seq_; }

 private:
  std::uint16_t query_id_ = 0;
  MarketKind kind_ = MarketKind::kVending;
  std::uint32_t store_id_ = 0;
  std::uint32_t owner_aid_ = 0;
  bool waiting_ = false;
  bool complete_ = false;
  bool truncated_ = false;
  std::uint16_t parts_seen_ = 0;
  std::uint32_t complete_seq_ = 0;
  std::vector<MarketOffer> offers_;
};

class Market {
 public:
  // Le numéro de la prochaine demande ; jamais zéro, que le serveur réserve à
  // ses ouvertures (catalogue, `@shopsearch`).
  [[nodiscard]] std::uint16_t NextQueryId() noexcept;

  // Une demande part : la recherche, ou le contenu d'une échoppe.
  void NoteSent(const MarketQuery& query);
  // Une trame reçue.
  void Tell(MarketReplyPart part);

  [[nodiscard]] const MarketAnswer& search() const noexcept { return search_; }
  [[nodiscard]] const MarketAnswer& store() const noexcept { return store_; }
  // La réponse de la dernière demande À PART, et son dernier refus.
  [[nodiscard]] const MarketAnswer& aside() const noexcept { return aside_; }
  [[nodiscard]] MarketStatus aside_refusal() const noexcept { return aside_refusal_; }
  [[nodiscard]] std::uint32_t aside_refusal_seq() const noexcept { return aside_refusal_seq_; }

  // Change à chaque ouverture du serveur (catalogue, `@shopsearch`).
  [[nodiscard]] std::uint32_t open_seq() const noexcept { return open_seq_; }
  // La saisie que la dernière ouverture du serveur pose.
  [[nodiscard]] const std::string& open_query() const noexcept { return open_query_; }
  // Le dernier refus, et son rang.
  [[nodiscard]] MarketStatus refusal() const noexcept { return refusal_; }
  [[nodiscard]] std::uint32_t refusal_seq() const noexcept { return refusal_seq_; }
  // Le refus porte-t-il sur le contenu d'une échoppe ?
  [[nodiscard]] bool refusal_for_store() const noexcept { return refusal_for_store_; }

 private:
  std::uint16_t last_query_id_ = 0;
  MarketAnswer search_;
  MarketAnswer store_;
  MarketAnswer aside_;
  MarketStatus aside_refusal_ = MarketStatus::kOk;
  std::uint32_t aside_refusal_seq_ = 0;
  std::uint32_t open_seq_ = 0;
  std::string open_query_;
  MarketStatus refusal_ = MarketStatus::kOk;
  std::uint32_t refusal_seq_ = 0;
  bool refusal_for_store_ = false;
};

// ── LA SAISIE, JUGÉE AVANT L'ENVOI ──────────────────────────────────────────

enum class MarketFault : std::uint8_t {
  kNone,
  // Le nom ne désigne aucun objet.
  kUnknownItem,
  // Le nom en désigne plus de `kMarketMostIds`.
  kTooManyItems,
  // Un prix illisible, ou un minimum au-dessus du maximum.
  kBadPrice,
  // Un raffinement minimum au-dessus du maximum.
  kBadRefine,
};

struct MarketForm {
  MarketKind kind = MarketKind::kVending;
  // UN NOM, OU UN IDENTIFIANT : une saisie toute en chiffres cherche l'objet de
  // ce numéro ; sinon tout objet dont le nom CONTIENT la saisie, sans casse ni
  // accents. Vide : tous les objets.
  std::string_view text;
  MarketCategory category = MarketCategory::kAny;
  int refine_min = 0;
  int refine_max = kMarketHighestRefine;
  std::string_view min_price;
  std::string_view max_price;
  bool with_cards = false;
  bool with_options = false;
};

struct MarketPlan {
  MarketFault fault = MarketFault::kNone;
  MarketQuery query;
};

// `catalogue` : la table des objets du client — ce sont les noms que le joueur
// lit. ⚠ TRIÉE par identifiant : la demande garde cet ordre.
[[nodiscard]] MarketPlan PlanMarketSearch(const MarketForm& form,
                                          std::span<const StoreSearchCatalogueEntry> catalogue);

// LA COMPARAISON d'une échoppe de vente ou d'achat : tous ses items, partout.
// ⚠ `store_id` reste nul : c'est une recherche, dont la réponse remplace la
// liste du Marché.
[[nodiscard]] MarketQuery MarketCompareQuery(MarketKind kind, std::uint32_t owner_aid);

// La demande du contenu d'une échoppe, d'après l'une de ses offres.
[[nodiscard]] MarketQuery MarketStoreQuery(MarketKind kind, std::uint32_t store_id,
                                           std::uint32_t owner_aid);

// « VENDU CHEZ » : les boutiques de PNJ qui vendent cet objet, partout. Une
// demande À PART : la liste du Marché ne bouge pas.
[[nodiscard]] MarketQuery MarketNpcSellersQuery(std::uint32_t item_id);

// LE NOM D'UNE ÉCHOPPE TEL QUE LE JOUEUR LE LIT : le titre s'arrête au retour
// chariot. Chez un PNJ, le fil porte son nom ENTIER, partie cachée comprise
// (« Tool Dealer#tu ») : seule la partie avant le dièse se montre, comme sur sa
// plaque en jeu. ⚠ SEULEMENT CHEZ UN PNJ : le titre d'un joueur peut porter un
// dièse qui lui appartient.
[[nodiscard]] std::string_view MarketShownShopName(MarketKind kind,
                                                   std::string_view shop_name) noexcept;

// ── LE TRI ET LES PAGES ─────────────────────────────────────────────────────

// Les colonnes que l'on trie, dans l'ordre de la table.
enum class MarketColumn : std::uint8_t {
  kItem,
  kPrice,
  kAmount,
  kShop,
  kMap,
};

struct MarketSort {
  MarketColumn column = MarketColumn::kPrice;
  bool descending = false;
};

// L'ORDRE DES OFFRES : les rangs dans `offers`, triés. `item_keys` porte, au
// même rang, la clé de tri du nom MONTRÉ (`core::SearchKey`). À égalité, le prix
// croissant, puis le rang reçu : l'ordre ne saute pas d'une image à l'autre.
[[nodiscard]] std::vector<std::size_t> MarketOrder(std::span<const MarketOffer> offers,
                                                   std::span<const std::string> item_keys,
                                                   MarketSort sort);

// Le nombre de pages, un au moins.
[[nodiscard]] int MarketPageCount(std::size_t offer_count) noexcept;

// LES RANGS D'UNE PAGE, comptée depuis un et ramenée dans les pages qui
// existent.
[[nodiscard]] std::span<const std::size_t> MarketPage(std::span<const std::size_t> order,
                                                      int page) noexcept;

}  // namespace ro::sim
