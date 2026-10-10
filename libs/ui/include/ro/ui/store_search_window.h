#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

namespace ro::ui {

// ── LA RECHERCHE D'ÉCHOPPES ─────────────────────────────────────────────────
//
// Celle du client d'origine : la sorte d'échoppe (vente ou achat), le nom d'un
// objet, celui d'une carte, une fourchette de prix, la case « objets
// similaires », le bouton de recherche et les essais restants ; puis dix
// résultats par page — icône, nom de l'échoppe, objet, quantité, prix — et
// le pied, qui dit le dernier refus en rouge et tourne les pages.
//
// Les gestes sont les siens :
// - un DOUBLE-CLIC sur le nom d'une échoppe la désigne au serveur ; quand
//   l'objet de catalogue ouvre les échoppes à distance, une boîte le confirme ;
// - le survol montre en bulle le nom de l'échoppe, ou celui de l'objet ;
// - le clic DROIT sur l'icône ou l'objet ouvre sa description.
//
// ⚠ PRESQUE TOUS LES TEXTES SORTENT DE LA TABLE DE MESSAGES de l'installation,
// et la vue les apporte. Passent par notre catalogue : les quatre étiquettes
// au-dessus des champs, que le client d'origine écrit en anglais en dur, le
// bouton de recherche — une image chez lui —, et le « Oui » / « Non » de la
// confirmation.

inline constexpr const char* kStoreSearchWindowId = "###store_search";

// La place d'un champ de saisie, NUL compris : cinquante caractères, la limite
// des champs du client d'origine.
inline constexpr std::size_t kStoreSearchFieldBytes = 51;

// Un rang de ligne qui ne désigne rien.
inline constexpr int kStoreSearchNoRow = -1;

// Les deux sortes d'échoppe, dans l'ordre des boutons radio.
inline constexpr int kStoreSearchSellKind = 0;
inline constexpr int kStoreSearchBuyKind = 1;

// LES MOTS DE LA FENÊTRE, déjà traduits.
struct StoreSearchWords {
  std::string_view title;
  std::string_view sell;
  std::string_view buy;
  std::string_view shop_name;
  std::string_view item;
  std::string_view count;
  std::string_view price;
  std::string_view similar;
  std::string_view card_hint;
  std::string_view previous_page;
  std::string_view next_page;
  // « Attempts: 3 », déjà composé.
  std::string_view uses_left;
  // La question qui précède l'ouverture à distance.
  std::string_view open_store;
};

// UN RÉSULTAT DE LA PAGE MONTRÉE.
struct StoreSearchRowView {
  TradeItemView item;
  // Le nom de l'échoppe, arrêté au premier retour chariot.
  std::string_view shop_name;
  int amount = 0;
  std::int64_t price_zeny = 0;
  // L'IDENTITÉ DE LA LIGNE, que le double-clic emporte : la page peut changer
  // entre le clic et sa confirmation.
  std::uint32_t store_id = 0;
  std::uint32_t owner_aid = 0;
};

struct StoreSearchView {
  StoreSearchWords words;
  // Les résultats de la page montrée, dix au plus.
  std::span<const StoreSearchRowView> rows;
  // La page suivante est déjà reçue.
  bool next_held = false;
  // La page suivante se demande au serveur. Voir `sim::StoreSearchMayAskNextPage`.
  bool next_askable = false;
  // Un clic ouvre l'échoppe à distance : il se confirme.
  bool click_opens_store = false;
  // Le dernier refus, en rouge au pied ; vide s'il n'y en a pas.
  std::string_view status;
};

// UNE LIGNE DÉSIGNÉE : son rang dans la page, et son identité.
struct StoreSearchPick {
  int row = kStoreSearchNoRow;
  std::uint32_t store_id = 0;
  std::uint32_t owner_aid = 0;

  [[nodiscard]] bool chosen() const noexcept { return row != kStoreSearchNoRow; }
};

struct StoreSearchWindowState {
  bool open = false;
  std::uint32_t opening_seen = 0;
  std::uint32_t first_page_seen = 0;
  // La page montrée, comptée depuis un.
  int page = 1;
  int kind = kStoreSearchSellKind;
  std::array<char, kStoreSearchFieldBytes> item_name{};
  std::array<char, kStoreSearchFieldBytes> card_name{};
  std::array<char, kStoreSearchFieldBytes> min_price{};
  std::array<char, kStoreSearchFieldBytes> max_price{};
  // Cochée à chaque ouverture, comme chez le client d'origine.
  bool similar = true;
  // La ligne survolée à l'image précédente, peinte d'un bleu pâle.
  int hovered_row = kStoreSearchNoRow;
  // La ligne dont le clic attend sa confirmation.
  StoreSearchPick pending;
  // La boîte est demandée. ⚠ ELLE S'OUVRE AU NIVEAU DE LA FENÊTRE, jamais
  // depuis une ligne de table.
  bool pending_asked = false;
};

struct StoreSearchAction {
  // La croix ou Échap. ⚠ AUSSI ÉCRIT DANS `state.open`.
  bool close = false;
  // « Rechercher » : la saisie est dans `state`, et `app` la juge.
  bool search = false;
  // La page suivante est à demander au serveur ; `state.page` l'attend déjà.
  bool next_page = false;
  // Le résultat désigné, confirmé s'il le fallait. ⚠ `app` vérifie que la
  // ligne à ce rang est toujours la même.
  StoreSearchPick clicked;
  int hovered_row = kStoreSearchNoRow;
  int describe_row = kStoreSearchNoRow;
};
// Remet la fenêtre à l'état d'une recherche neuve : champs vides, première
// page, objets similaires cochés.
void OpenStoreSearchOn(StoreSearchWindowState& state, std::uint32_t opening_seq);

// LA COULEUR D'UN PRIX, selon son ordre de grandeur, comme le client d'origine
// l'écrit : le texte, et l'ombre décalée d'un pixel qui le double — absente
// (alpha nul) pour les ordres qui n'en ont pas.
struct StoreSearchPriceInk {
  ImU32 text = 0;
  ImU32 shadow = 0;
};
[[nodiscard]] StoreSearchPriceInk StoreSearchPriceInkOf(std::int64_t price_zeny);

[[nodiscard]] StoreSearchAction DrawStoreSearchWindow(StoreSearchWindowState& state,
                                                      const StoreSearchView& view,
                                                      const SkinArt& art);

}  // namespace ro::ui
