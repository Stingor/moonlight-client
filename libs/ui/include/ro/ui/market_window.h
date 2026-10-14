#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

#include "ro/ui/chat_link.h"
#include "ro/ui/item_link_cell.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

namespace ro::ui {

// ── LE MARCHÉ ───────────────────────────────────────────────────────────────
//
// Toutes les échoppes ouvertes du serveur et les boutiques de PNJ, en trois
// onglets : « Ventes », « Achats » et « PNJ ». Une saisie — un nom ou un ID d'item —, des filtres,
// puis une table triée au clic sur ses colonnes et tournée par pages. Une ligne se déplie sur ses
// cartes, son grade et ses options ; « Y aller » guide jusqu'à l'échoppe, « Voir » montre tout son
// contenu dans une seconde fenêtre.
//
// 🔴 LECTURE SEULE : aucun bouton n'achète ni ne vend. La fenêtre ne possède
// rien : `app` compose la vue, juge la saisie et parle au serveur.

inline constexpr const char* kMarketWindowId = "###market";
inline constexpr const char* kMarketStoreWindowId = "###market_store";

// La taille à la première ouverture, en pixels d'interface : toutes les
// colonnes à leur largeur initiale, et le nom de l'échoppe bien au-delà de sa
// largeur minimale.
inline constexpr float kMarketWindowWidthPixels = 860.0F;
inline constexpr float kMarketWindowHeightPixels = 480.0F;
inline constexpr float kMarketStoreWindowWidthPixels = 420.0F;
inline constexpr float kMarketStoreWindowHeightPixels = 360.0F;

// La place d'un champ de saisie, NUL compris.
inline constexpr std::size_t kMarketQueryBytes = 64;
inline constexpr std::size_t kMarketPriceBytes = 16;

// Le plus haut raffinement que le filtre propose.
inline constexpr int kMarketWindowHighestRefine = 20;

inline constexpr int kMarketNoRow = -1;

// Les onglets, dans l'ordre : ils portent la sorte d'échoppe.
inline constexpr int kMarketSellTab = 0;
inline constexpr int kMarketBuyTab = 1;
inline constexpr int kMarketNpcTab = 2;

// LES COLONNES TRIABLES, dans l'ordre de la table ; la dernière, celle des
// actions, ne se trie pas.
inline constexpr int kMarketItemColumn = 0;
inline constexpr int kMarketPriceColumn = 1;
inline constexpr int kMarketAmountColumn = 2;
inline constexpr int kMarketShopColumn = 3;
inline constexpr int kMarketMapColumn = 4;
inline constexpr int kMarketActionsColumn = 5;

// LES COLONNES SE RÈGLENT À LA SOURIS, et le fichier de réglages d'ImGui
// retient leur largeur. Celle du nom de l'échoppe ou du PNJ prend la place qui
// reste ; la fenêtre ne se rétrécit pas en deçà de ce qui lui laisse au moins
// cette largeur, les autres colonnes à leur largeur initiale.
inline constexpr float kMarketShopColumnLeastPixels = 160.0F;

// UNE LIGNE MONTRÉE.
struct MarketRowView {
  TradeItemView item;
  // L'emblème du grade dans le coin bas gauche de l'icône ; vide sans grade.
  SkinImage grade_badge;
  // Le prix unitaire et sa monnaie, déjà composés : « 1,500 z », « 200 cash ».
  std::string_view price;
  // La quantité, déjà composée ; un tiret pour le stock illimité d'un PNJ.
  std::string_view amount;
  // Le titre de l'échoppe, ou le nom du PNJ.
  std::string_view shop_name;
  // La carte et la case, déjà composées.
  std::string_view place;
  // Le marchand est hors ligne.
  bool autotrade = false;
  // LA LIGNE DE L'ÉCHOPPE COMPARÉE : sa référence, peinte à part.
  bool compared = false;
  // Ce que la ligne dépliée montre : cartes, grade, options. Vide : la ligne ne
  // se déplie pas.
  std::span<const std::string> details;
};

struct MarketView {
  // Les lignes de la page montrée.
  std::span<const MarketRowView> rows;
  int page_count = 1;
  // « 42 offres », ou ce qu'on attend.
  std::string_view summary;
  // Le dernier refus, en rouge ; vide s'il n'y en a pas.
  std::string_view status;
  // Une demande est en route : les boutons qui en enverraient une se grisent.
  bool busy = false;
  // LA COLONNE DES QUANTITÉS : masquée quand elle n'aurait que des tirets — des
  // PNJ au stock illimité.
  bool amount_shown = true;
  // LE CONTENU D'UNE ÉCHOPPE, quand il est demandé.
  std::string_view store_title;
  std::string_view store_status;
  std::span<const MarketRowView> store_rows;
  bool store_amount_shown = true;
};

struct MarketWindowState {
  bool open = false;
  // Mise au premier plan à la prochaine image.
  bool raise = false;
  int tab = kMarketSellTab;
  // L'onglet de `tab` s'impose à la prochaine image : vrai à chaque ouverture.
  bool select_tab = true;
  std::array<char, kMarketQueryBytes> query{};
  int category = 0;
  int refine_min = 0;
  int refine_max = kMarketWindowHighestRefine;
  std::array<char, kMarketPriceBytes> min_price{};
  std::array<char, kMarketPriceBytes> max_price{};
  bool with_cards = false;
  bool with_options = false;
  // Le tri : prix unitaire croissant à l'ouverture.
  int sort_column = kMarketPriceColumn;
  bool sort_descending = false;
  // La page montrée, comptée depuis un.
  int page = 1;
  // La ligne dépliée de la page ; une seule à la fois.
  int expanded_row = kMarketNoRow;
  // La fenêtre du contenu d'une échoppe.
  bool store_open = false;
  // Le menu qu'un clic droit sur un objet a armé, dans chacune des deux
  // fenêtres : un menu s'ouvre dans la fenêtre du clic.
  ItemLinkMenu item_menu;
  ItemLinkMenu store_item_menu;
};

struct MarketAction {
  // La croix ou Échap. ⚠ AUSSI ÉCRIT DANS `state.open`.
  bool close = false;
  // Une recherche neuve : « Rechercher », Entrée, ou un autre onglet.
  bool search = false;
  // « Actualiser » : la même recherche, redemandée.
  bool refresh = false;
  // Le rang dans la page de la ligne dont on veut rejoindre l'échoppe, ou voir
  // le contenu.
  int go_to_row = kMarketNoRow;
  int view_store_row = kMarketNoRow;
  // L'objet survolé : un rang de la page, ou du contenu d'une échoppe.
  int hovered_row = kMarketNoRow;
  bool hovered_in_store = false;
  // L'objet visé par un geste, et ce que le geste demande.
  int describe_row = kMarketNoRow;
  bool describe_in_store = false;
  ChatLinkGesture item_gesture = ChatLinkGesture::kDescribe;
};

// Remet la fenêtre à neuf, ouverte au premier plan, la saisie remplie de `query`.
void OpenMarketOn(MarketWindowState& state, std::string_view query);

// `vote_shop` : le catalogue du vote shop, pour le menu d'un objet.
[[nodiscard]] MarketAction DrawMarketWindow(MarketWindowState& state, const MarketView& view,
                                            const SkinArt& art,
                                            std::span<const VoteShopOffer> vote_shop = {});

}  // namespace ro::ui
