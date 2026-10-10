#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

namespace ro::ui {

// ── L'HISTORIQUE DES ACHATS D'UNE ÉCHOPPE ───────────────────────────────────
//
// Chaque achat de l'échoppe : l'item, la quantité, le prix, le montant, et le
// total.
//
// Elle se montre QUAND L'ÉCHOPPE SE FERME, s'il y a eu des achats : pendant la
// vente, l'échoppe dit déjà ce qui reste voulu. La refermer oublie le journal.
//
// Chaque ligne porte aussi l'HEURE de l'achat (le client d'origine
// l'enregistre) et le VENDEUR, par son numéro de personnage — le serveur ne dit
// pas son nom.

inline constexpr const char* kBuyingStoreLogWindowName = "###buying_store_log";

struct BuyingStoreLogLineView {
  TradeItemView item;
  // « jj/mm - hh:mm:ss », en heure locale.
  std::string_view clock;
  std::uint32_t seller_char_id = 0;
  int amount = 0;
  // Ce que la ligne a coûté, en tout : le prix unitaire s'en déduit.
  std::int64_t zeny = 0;
};

struct BuyingStoreLogView {
  std::span<const BuyingStoreLogLineView> lines;
  std::int64_t total = 0;
};

struct BuyingStoreLogState {
  bool open = false;
  // Le menu qu'un clic droit sur un item a armé.
  ItemLinkMenu item_menu;
};

struct BuyingStoreLogAction {
  // Fermer, la croix ou Échap. ⚠ AUSSI ÉCRITE DANS `state.open`.
  bool close = false;
  // L'item survolé, pour l'aperçu, et celui qu'un geste vise.
  std::uint32_t hovered_id = 0;
  std::uint32_t describe_id = 0;
  // Ce que le geste demande de l'item visé : sa description, son lien dans la
  // saisie (Maj + clic), ou une autre entrée du menu des objets.
  ChatLinkGesture item_gesture = ChatLinkGesture::kDescribe;
};

// `vote_shop` : le catalogue du vote shop, pour le menu d'un item.
[[nodiscard]] BuyingStoreLogAction DrawBuyingStoreLogWindow(
    BuyingStoreLogState& state, const BuyingStoreLogView& view, const SkinArt& art,
    std::span<const VoteShopOffer> vote_shop = {});

}  // namespace ro::ui
