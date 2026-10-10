#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

namespace ro::ui {

// ── MON ÉCHOPPE D'ACHAT, OUVERTE ────────────────────────────────────────────
//
// Ce qui est encore voulu, à quel prix, ce qui reste à dépenser.
//
// 🔴 LA FERMER SE CONFIRME, par le bouton comme par la croix ou Échap : c'est
// un geste qu'on ne défait pas — il faudra relancer le skill, et reposer
// chaque ligne. La fenêtre ne se baisse donc jamais d'elle-même : un
// interrupteur baissé par la croix ou par la pile d'Échap la RELÈVE et pose la
// question.
//
// La limite d'ouverture et ce qui a déjà été dépensé se disent à côté de ce
// qui reste — le client d'origine ne montre que le reste.

inline constexpr const char* kBuyingStoreOwnWindowName = "###buying_store_own";

struct BuyingStoreOwnLineView {
  TradeItemView item;
  // Combien l'échoppe en veut ENCORE : zéro est une ligne pleine.
  int wanted = 0;
  std::int64_t price = 0;
};

struct BuyingStoreOwnView {
  std::string_view window_title;
  // Le nom de l'échoppe, tel que les autres le lisent. Vide si le serveur a
  // ouvert une échoppe que cette session n'a pas demandée.
  std::string_view store_title;
  std::span<const BuyingStoreOwnLineView> lines;
  std::int64_t zeny_left = 0;
  std::int64_t zeny_at_opening = 0;
  std::int64_t spent = 0;
  int purchases = 0;
};

struct BuyingStoreOwnState {
  // Recopié par `app` depuis l'échoppe. ⚠ BAISSÉ PAR LA CROIX OU ÉCHAP, IL EST
  // RELEVÉ par la fenêtre, qui demande alors confirmation.
  bool open = false;
  std::uint32_t opening_seen = 0;
  // La question est à poser à la prochaine image. ⚠ ELLE SE POSE DANS LA
  // FENÊTRE : l'identité d'une boîte dépend de la pile d'ImGui où on l'ouvre.
  bool ask_close = false;
  // Le menu qu'un clic droit sur un item a armé.
  ItemLinkMenu item_menu;
};

struct BuyingStoreOwnAction {
  // La fermeture, CONFIRMÉE.
  bool close = false;
  // L'item survolé, pour l'aperçu, et celui qu'un geste vise.
  std::uint32_t hovered_id = 0;
  std::uint32_t describe_id = 0;
  // Ce que le geste demande de l'item visé : sa description, son lien dans la
  // saisie (Maj + clic), ou une autre entrée du menu des objets.
  ChatLinkGesture item_gesture = ChatLinkGesture::kDescribe;
};

void OpenBuyingStoreOwnOn(BuyingStoreOwnState& state, std::uint32_t opening_seq);

// `vote_shop` : le catalogue du vote shop, pour le menu d'un item.
[[nodiscard]] BuyingStoreOwnAction DrawBuyingStoreOwnWindow(
    BuyingStoreOwnState& state, const BuyingStoreOwnView& view, const SkinArt& art,
    std::span<const VoteShopOffer> vote_shop = {});

}  // namespace ro::ui
