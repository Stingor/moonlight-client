#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/ui/npc_shop_saved_carts.h"
#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

namespace ro::ui {

// ── LA BOUTIQUE D'UN PNJ ────────────────────────────────────────────────────
//
// Pas de fenêtre « Acheter / Vendre /
// Annuler », mais une fenêtre unique à deux onglets, une liste à gauche et un
// panier à droite. Un clic remplit le panier, Ctrl + clic achète ou vend tout
// de suite.
//
// Elle sert les quatre boutiques du serveur — à zeny, market, à points, et les
// ouvertures par liste seule d'un script, qui n'offrent qu'un onglet.
//
// 🔴 LA FENÊTRE NE S'OUVRE PAS TOUTE SEULE, ET NE SE FERME PAS SANS LE DIRE.
// `app` recopie l'ouverture dans `open` ; la croix, Échap et « Fermer » la
// baissent, et c'est `app` qui dit la fermeture au serveur. Sans elle, le
// personnage ne pourrait plus marcher.
//
// Cette couche dessine et remplit le panier. Elle ne juge pas : `app` passe le
// verdict du panier — ce que le serveur refuserait — et la fenêtre retient le
// bouton en disant pourquoi.

inline constexpr const char* kNpcShopWindowId = "npc_shop";

// Quelle boutique : elle décide des colonnes et de l'unité des prix.
enum class NpcShopWindowKind : std::uint8_t { kZeny, kMarket, kPoints };

enum class NpcShopTab : std::uint8_t { kBuy, kSell };

// Le stock d'une ligne qu'aucune borne ne limite.
inline constexpr std::int64_t kNpcShopNoStock = -1;

// UNE LIGNE À ACHETER.
struct NpcShopOfferView {
  TradeItemView item;
  std::int64_t base_price = 0;
  std::int64_t price = 0;
  // `kNpcShopNoStock` hors market.
  std::int64_t stock = kNpcShopNoStock;
  // Ce qu'on peut en prendre en une fois : un pour un équipement, sinon ce qui
  // reste de place dans la pile, et jamais plus que le stock.
  int most = 1;
};

// UNE LIGNE À VENDRE : une case du sac que le serveur accepte d'acheter.
struct NpcShopSellView {
  TradeItemView item;
  std::uint16_t client_slot = 0;
  int amount = 0;
  std::int64_t base_price = 0;
  std::int64_t price = 0;
  // RAFFINÉ OU SERTI : il vaut sans doute plus que ce que le PNJ en donne, et sa
  // vente se confirme.
  bool precious = false;
  // LE TYPE DE L'OBJET, déjà nommé par `app` : celui de l'item_db du serveur,
  // ou la famille que le client lui connaît. Il range la ligne dans « Ajouter
  // par type ».
  std::string_view kind;
  // Porté : aucun ajout en bloc ne le prend.
  bool worn = false;
};

// UN TYPE DE LA LISTE DE VENTE : combien de lignes il range, et ce qu'elles
// rapporteraient, quantité complète.
struct NpcShopSellKind {
  std::string_view kind;
  int lines = 0;
  std::int64_t total = 0;
};

// UNE LIGNE DU PANIER. ⚠ À L'ACHAT, L'OBJET FAIT L'IDENTITÉ ; À LA VENTE, LA
// CASE DU SAC — deux piles du même objet sont deux lignes.
struct NpcShopCartLine {
  std::uint32_t item_id = 0;
  std::uint16_t client_slot = 0;
  int amount = 1;
  std::int64_t unit_price = 0;
  int most = 1;
};

struct NpcShopView {
  NpcShopWindowKind kind = NpcShopWindowKind::kZeny;
  bool can_buy = true;
  bool can_sell = true;
  // Le nom du PNJ, ou vide : le titre retombe alors sur « Shop ».
  std::string_view title;
  // CE QU'ON A POUR PAYER : les zeny, ou les points — déjà écrit par `app`,
  // avec les mots du jeu pour une boutique à points.
  std::string_view purse_line;
  // La bourse, pour griser un achat immédiat trop cher. `kTradeNoPurse` : non
  // dite.
  std::int64_t purse = kTradeNoPurse;
  // L'unité qui suit un prix : « z », ou celle des points.
  std::string_view unit;

  std::span<const NpcShopOfferView> offers;
  std::span<const NpcShopSellView> sells;
  // Les listes sont-elles arrivées ? Sinon, une ligne d'attente.
  bool offers_listed = false;
  bool sells_listed = false;
  // Combien de favoris le verrou de vente retire de la liste.
  int hidden_favorites = 0;
  // CE QUE LE SERVEUR MASQUE de lui-même, déjà écrit par `app` : sans cette
  // ligne, une carte ou une arme raffinée absente de la liste se lirait comme
  // une panne.
  std::string_view server_filter_note;

  // LE DERNIER RÉSULTAT, avec les mots du jeu : vide avant le premier.
  std::string_view outcome;
  bool outcome_done = false;
  // Une transaction attend son résultat : rien ne part avant lui.
  bool awaiting = false;

  // LE VERDICT DU PANIER : les raisons, déjà traduites, qui retiennent le
  // bouton ; le total ; le poids ajouté, écrit par `app` quand il se sait.
  std::span<const std::string_view> blockers;
  std::int64_t cart_total = 0;
  std::string_view cart_weight;

  // LES PANIERS ENREGISTRÉS DU COMPTE, dans l'ordre du carnet, et combien il
  // en tient. `saved_cart_report` : le dernier refus ou la dernière panne du
  // carnet, déjà écrit ; vide sinon.
  std::span<const NpcShopSavedCartView> saved_carts;
  std::size_t saved_cart_capacity = 0;
  std::string_view saved_cart_report;
};

// Ce que la fenêtre garde d'une image à l'autre.
struct NpcShopWindowState {
  // Recopié par `app` depuis la boutique ; la croix, Échap et « Fermer » le
  // baissent.
  bool open = false;
  // Le rang de l'ouverture montrée : une ouverture neuve remet tout à zéro.
  std::uint32_t opening_seen = 0;
  NpcShopTab tab = NpcShopTab::kBuy;
  // L'onglet à SÉLECTIONNER de force à la prochaine image. ⚠ ÉCRIRE `tab` NE
  // SUFFIT PAS : la barre d'onglets tient sa propre sélection.
  bool force_tab = false;
  ImGuiTextFilter filter;
  std::vector<NpcShopCartLine> cart;
  // « Tout ajouter au panier » : la boutique se ferme après la vente réussie.
  bool close_after_sale = false;
  // Réglage du joueur, descendu par `app` : éteint, « Tout ajouter au panier »
  // vend sans confirmation. Un objet précieux se confirme toujours.
  bool confirm_sell_all = true;
  // LA VENTE ATTEND SA CONFIRMATION : un objet précieux, ou tout le sac vendable.
  std::vector<NpcShopCartLine> pending_sale;
  // La boîte de confirmation est à OUVRIR, au niveau de la fenêtre : voir
  // `DrawNpcShopWindow`.
  bool sale_asked = false;
  // Le menu qu'un clic droit sur un item a armé ; son jeton est la case du sac.
  ItemLinkMenu item_menu;
  // « Paniers ▾ » : ses saisies, et ce que le dernier chargement a laissé.
  NpcShopSavedCartsState saved;
};

struct NpcShopWindowAction {
  // La croix, Échap ou « Fermer ». ⚠ AUSSI ÉCRITE DANS `state.open`.
  bool close = false;
  // Le joueur change d'onglet : la liste de cet onglet se redemande.
  bool tab_changed = false;
  // CE QUI PART : les lignes à acheter ou à vendre, vides pour rien.
  // ⚠ `app` REJUGE AVANT D'ÉMETTRE : la fenêtre a pu dessiner un verdict d'une
  // image de retard.
  std::vector<NpcShopCartLine> buy;
  std::vector<NpcShopCartLine> sell;
  // L'achat ou la vente vient d'un Ctrl + clic : le panier n'est pas touché.
  bool immediate = false;
  // L'objet survolé, pour l'aperçu, et celui qu'un geste vise. La case du sac
  // les accompagne à la vente : c'est elle qui dit le raffinement.
  std::uint32_t hovered_id = 0;
  std::uint16_t hovered_slot = 0;
  std::uint32_t describe_id = 0;
  std::uint16_t describe_slot = 0;
  // Ce que le geste demande de l'objet visé : sa description, son lien dans la
  // saisie (Maj + clic), ou une autre entrée du menu des objets.
  ChatLinkGesture item_gesture = ChatLinkGesture::kDescribe;
  // UN ITEM DU SAC A ÉTÉ LÂCHÉ SUR LA VENTE, mais le PNJ ne l'achète pas —
  // ou il est porté : `app` le dit.
  bool drop_refused = false;
  // Ce que « Paniers ▾ » demande au carnet. ⚠ « ENREGISTRER » PREND LE PANIER
  // DE `state.cart` : `app` le lit là.
  NpcShopCartBookAsk cart_book;
};

// Remet la fenêtre à l'état d'une ouverture neuve, sur l'onglet qu'offre la
// boutique.
void OpenNpcShopOn(NpcShopWindowState& state, std::uint32_t opening_seq, bool buy_first);

// Ajoute au panier, ou à la ligne qui existe déjà, borné à `most`. Rend faux si
// la ligne était déjà pleine — un équipement ne s'achète qu'un à la fois.
bool AddToNpcShopCart(std::vector<NpcShopCartLine>& cart, const NpcShopCartLine& line);

// 🔴 LES LIGNES QU'UN AJOUT EN BLOC PREND — « Tout ajouter au panier » comme
// « Ajouter par type » —, chacune pour toute sa pile, dans l'ordre de la liste.
// Les favoris verrouillés n'y sont pas — `app` les a déjà retirés de la liste —,
// et une pièce portée s'écarte ici. Une seule règle pour les deux boutons.
[[nodiscard]] std::vector<NpcShopCartLine> NpcShopSellableLines(const NpcShopView& view);

// Les types des lignes vendables, par ordre alphabétique de libellé. Un type
// sans ligne vendable n'y paraît pas.
[[nodiscard]] std::vector<NpcShopSellKind> NpcShopSellKinds(const NpcShopView& view);

// Ajoute au panier toutes les lignes vendables d'un type, pile complète. Rien
// ne se vend : « Vendre » reste à cliquer, et ses confirmations avec.
void AddNpcShopSellKind(std::vector<NpcShopCartLine>& cart, const NpcShopView& view,
                        std::string_view kind);

// Une vente qui se confirme avant de partir : elle porte un objet précieux, ou
// elle vend tout le sac et le joueur veut qu'on le lui demande.
[[nodiscard]] bool NpcShopSaleNeedsConfirmation(const NpcShopWindowState& state,
                                                const NpcShopView& view,
                                                std::span<const NpcShopCartLine> lines);

// CE QU'UN CHARGEMENT N'A PAS MIS AU PANIER, par le nom montré de chaque objet.
struct NpcShopCartLoad {
  // Les objets que cette boutique ne vend pas.
  std::vector<std::string> absent;
  // Les objets qu'elle vend, mais dont la pile du sac est déjà pleine.
  std::vector<std::string> full;
};

// 🔴 CHARGE UN PANIER ENREGISTRÉ : chaque objet que la boutique vend AJOUTE sa
// quantité au panier courant, par `AddToNpcShopCart` et avec les bornes de sa
// ligne — exactement ce que ferait un clic. Les autres sont rendus, pour que la
// fenêtre dise ce qui manque.
[[nodiscard]] NpcShopCartLoad LoadNpcShopSavedCart(std::vector<NpcShopCartLine>& cart,
                                                   const NpcShopView& view,
                                                   const NpcShopSavedCartView& saved);

// La ligne grise qui dit ce qu'un chargement a laissé ; vide s'il a tout pris.
[[nodiscard]] std::string NpcShopCartLoadNote(const NpcShopCartLoad& load);

// Dessine la fenêtre si elle est ouverte. `link_facts` : ce que le
// menu d'un item sait d'avance — vote shop, album.
[[nodiscard]] NpcShopWindowAction DrawNpcShopWindow(NpcShopWindowState& state,
                                                    const NpcShopView& view, const SkinArt& art,
                                                    ItemLinkFacts link_facts = {});

}  // namespace ro::ui
