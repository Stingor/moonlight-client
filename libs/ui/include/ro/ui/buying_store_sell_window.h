#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

namespace ro::ui {

// ── LA VENTE À L'ÉCHOPPE D'UN ACHETEUR ──────────────────────────────────────
//
// Ce que l'acheteur recherche, mes piles qui y répondent, ce que je mets en
// vente, le total et les fonds de l'acheteur.
//
// 🔴 LE SERVEUR REFUSE LA VENTE ENTIÈRE POUR UNE LIGNE, avec un code qui ne dit
// presque rien. Tout ce que le client peut savoir se dit donc AVANT le clic,
// et le bouton se retient : une pile sertie est grisée avec sa raison, une
// quantité ne dépasse jamais ce que l'acheteur veut encore, ce que je possède,
// ni trente mille.
//
// Une pile entamée RESTE dans la liste avec ce qui en reste — le client
// d'origine la retire entière, et la même case ne peut de
// toute façon tenir qu'une ligne : un second ajout grossit la première ; la
// vente se CONFIRME par un récapitulatif ; la fenêtre reste ouverte après la
// vente, sur ce que l'acheteur veut encore.

inline constexpr const char* kBuyingStoreSellWindowName = "###buying_store_sell";

// Ce qu'une ligne de vente porte au plus d'un coup.
inline constexpr int kBuyingStoreMostSellAmount = 30'000;

struct BuyingStoreWantedView {
  TradeItemView item;
  int wanted = 0;
  std::int64_t price = 0;
};

// UNE PILE DE MON SAC dont l'acheteur veut l'item.
struct BuyingStoreOfferView {
  TradeItemView item;
  std::uint16_t client_slot = 0;
  int owned = 0;
  std::int64_t price = 0;
  // Ce qui interdit de la vendre, déjà traduit ; vide si rien. La ligne se
  // grise et le dit.
  std::string_view barred;
};

// UNE LIGNE « À VENDRE ». ⚠ LA CASE DU SAC FAIT L'IDENTITÉ : le serveur rejette
// la vente entière si la même case figure deux fois.
struct BuyingStoreSaleCartLine {
  std::uint16_t client_slot = 0;
  std::uint32_t item_id = 0;
  int amount = 0;
};

struct BuyingStoreSellView {
  // Le nom de l'acheteur, vide tant qu'on ne le connaît pas.
  std::string_view buyer;
  std::string_view store_title;
  std::span<const BuyingStoreWantedView> wanted;
  std::span<const BuyingStoreOfferView> offers;
  std::int64_t buyer_funds = 0;
  std::int64_t cart_total = 0;
  // LE VERDICT DE LA VENTE, déjà traduit : ce qui retient le bouton.
  std::span<const std::string_view> blockers;
  // Une vente attend sa réponse : rien ne repart avant elle.
  bool awaiting = false;
  // Le dernier résultat, avec les mots du jeu ; vide avant le premier.
  std::string_view outcome;
  bool outcome_done = false;
};

// Une quantité tapée pour une pile, retenue d'une image à l'autre.
struct BuyingStoreTypedAmount {
  std::uint16_t client_slot = 0;
  int amount = 0;
};

struct BuyingStoreSellState {
  // Recopié par `app` depuis la visite ; la croix, Échap et « Fermer » le
  // baissent.
  bool open = false;
  // L'échoppe montrée : un autre acheteur, ou une autre ouverture du même,
  // vide la vente préparée.
  std::uint32_t buyer_seen = 0;
  std::uint32_t store_seen = 0;
  std::vector<BuyingStoreSaleCartLine> cart;
  std::vector<BuyingStoreTypedAmount> typed;
  // LA VENTE ATTEND SA CONFIRMATION : ce qui partira si le joueur dit oui.
  std::vector<BuyingStoreSaleCartLine> pending;
  // Le menu qu'un clic droit sur un item a armé ; son jeton est la case du sac.
  ItemLinkMenu item_menu;
};

struct BuyingStoreSellAction {
  // La croix, Échap ou « Fermer ». ⚠ AUSSI ÉCRITE DANS `state.open`.
  bool close = false;
  // CE QUI PART, confirmé. ⚠ `app` REJUGE AVANT D'ÉMETTRE.
  std::vector<BuyingStoreSaleCartLine> sell;
  // L'item survolé, pour l'aperçu, et celui qu'un geste vise, avec leur case du
  // sac — nulle pour un item recherché.
  std::uint32_t hovered_id = 0;
  std::uint16_t hovered_slot = 0;
  std::uint32_t describe_id = 0;
  std::uint16_t describe_slot = 0;
  // Ce que le geste demande de l'item visé : sa description, son lien dans la
  // saisie (Maj + clic), ou une autre entrée du menu des objets.
  ChatLinkGesture item_gesture = ChatLinkGesture::kDescribe;
};

// Remet la vente à zéro si l'échoppe montrée change ; rend vrai dans ce cas.
bool OpenBuyingStoreSellOn(BuyingStoreSellState& state, std::uint32_t buyer_account_id,
                           std::uint32_t store_id);

// CE QU'ON PEUT ENCORE AJOUTER DE CETTE PILE : borné par ce que l'acheteur veut
// encore de l'item — toutes les lignes de l'item comptées —, par ce que la pile
// tient hors de la vente, et par ce qu'une ligne porte.
[[nodiscard]] int BuyingStoreAddable(std::span<const BuyingStoreWantedView> wanted,
                                     std::span<const BuyingStoreSaleCartLine> cart,
                                     const BuyingStoreOfferView& offer);

// Ajoute à la vente, ou grossit la ligne de la même case, borné par
// `BuyingStoreAddable`. Rend ce qui a été ajouté.
int AddToBuyingStoreSale(BuyingStoreSellState& state, std::span<const BuyingStoreWantedView> wanted,
                         const BuyingStoreOfferView& offer, int amount);

// `vote_shop` : le catalogue du vote shop, pour le menu d'un item.
[[nodiscard]] BuyingStoreSellAction DrawBuyingStoreSellWindow(
    BuyingStoreSellState& state, const BuyingStoreSellView& view, const SkinArt& art,
    std::span<const VoteShopOffer> vote_shop = {});

}  // namespace ro::ui
