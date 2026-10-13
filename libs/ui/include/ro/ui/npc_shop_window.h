#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <imgui.h>

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
};

// Remet la fenêtre à l'état d'une ouverture neuve, sur l'onglet qu'offre la
// boutique.
void OpenNpcShopOn(NpcShopWindowState& state, std::uint32_t opening_seq, bool buy_first);

// Ajoute au panier, ou à la ligne qui existe déjà, borné à `most`. Rend faux si
// la ligne était déjà pleine — un équipement ne s'achète qu'un à la fois.
bool AddToNpcShopCart(std::vector<NpcShopCartLine>& cart, const NpcShopCartLine& line);

// Une vente qui se confirme avant de partir : elle porte un objet précieux, ou
// elle vend tout le sac et le joueur veut qu'on le lui demande.
[[nodiscard]] bool NpcShopSaleNeedsConfirmation(const NpcShopWindowState& state,
                                                const NpcShopView& view,
                                                std::span<const NpcShopCartLine> lines);

// Dessine la fenêtre si elle est ouverte. `vote_shop` : le catalogue du vote
// shop, pour le menu d'un item.
[[nodiscard]] NpcShopWindowAction DrawNpcShopWindow(NpcShopWindowState& state,
                                                    const NpcShopView& view, const SkinArt& art,
                                                    std::span<const VoteShopOffer> vote_shop = {});

}  // namespace ro::ui
