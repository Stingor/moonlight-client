#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

namespace ro::ui {

// ── MON ÉCHOPPE OUVERTE, ET LE JOURNAL DE SES VENTES ─────────────────────────
//
// - MON ÉCHOPPE montre ce qui reste, à quel prix, et ce qu'elle a encaissé — le
//   cumul des rapports de vente, et non le champ du client d'origine qui ne
//   garde que la dernière. Sa fermeture se CONFIRME, par le bouton comme par la
//   croix ou Échap : elle rend au cart tout ce qui n'est pas vendu.
// - LE JOURNAL se montre quand l'échoppe s'est fermée après au moins une vente,
//   et reste jusqu'à ce qu'on le ferme. Il dit aussi l'HEURE de chaque vente et
//   l'acheteur, que le client d'origine tient sans les montrer.
//
// Ni l'une ni l'autre ne parle au serveur : `app` le fait sur leur réponse.

inline constexpr const char* kVendingShopWindowId = "###vending_shop";
inline constexpr const char* kVendingLogWindowId = "###vending_log";

// UNE LIGNE ENCORE EN VENTE.
struct VendingShopLineView {
  TradeItemView item;
  std::uint16_t cart_slot = 0;
  int amount = 0;
  std::int64_t price = 0;
};

struct VendingShopView {
  // Le titre retenu à l'envoi, vide si le serveur l'a ouverte sans nous.
  std::string_view title;
  std::span<const VendingShopLineView> lines;
  std::int64_t earned = 0;
};

struct VendingShopWindowState {
  // Recopié par `app`. ⚠ BAISSÉ PAR LA CROIX OU ÉCHAP, IL SE RELÈVE AUSSITÔT et
  // la question se pose : fermer l'échoppe n'est jamais un geste involontaire.
  bool open = false;
  std::uint32_t opening_seen = 0;
  bool confirming = false;
  // Le menu qu'un clic droit sur un item a armé ; son jeton est la case du cart.
  ItemLinkMenu item_menu;
};

struct VendingShopAction {
  // Fermeture CONFIRMÉE : `app` la dit au serveur.
  bool close_shop = false;
  // La case du cart survolée, et celle qu'un geste vise ; zéro pour aucune.
  std::uint16_t hovered_slot = 0;
  std::uint16_t describe_slot = 0;
  // Ce que le geste demande de l'item visé : sa description, son lien dans la
  // saisie (Maj + clic), ou une autre entrée du menu des objets.
  ChatLinkGesture item_gesture = ChatLinkGesture::kDescribe;
};

// `link_facts` : ce que le menu d'un item sait d'avance — vote shop, album.
[[nodiscard]] VendingShopAction DrawVendingShopWindow(VendingShopWindowState& state,
                                                      const VendingShopView& view,
                                                      const SkinArt& art,
                                                      ItemLinkFacts link_facts = {});

// UNE VENTE DU JOURNAL.
struct VendingLogLineView {
  TradeItemView item;
  // Le rang de la vente : identité ImGui, et ce que le survol rend.
  std::size_t rank = 0;
  int amount = 0;
  // Ce que la ligne a rapporté en tout.
  std::int64_t zeny = 0;
  std::uint32_t buyer_char_id = 0;
  // L'heure, déjà écrite par `app` dans le fuseau du joueur.
  std::string_view sold_at;
};

struct VendingLogView {
  std::span<const VendingLogLineView> sales;
  std::int64_t total = 0;
};

struct VendingLogWindowState {
  bool open = false;
  // Le rang du journal montré : `app` rouvre la fenêtre sur un journal neuf.
  std::uint32_t journal_seen = 0;
  // Le menu qu'un clic droit sur un item a armé ; son jeton est le rang de la
  // vente, plus un.
  ItemLinkMenu item_menu;
};

struct VendingLogAction {
  bool close = false;
  // Le rang de la vente survolée, et de celle qu'un geste vise, plus un ; zéro
  // pour aucune.
  std::size_t hovered_rank = 0;
  std::size_t describe_rank = 0;
  // Ce que le geste demande de l'item visé : sa description, son lien dans la
  // saisie (Maj + clic), ou une autre entrée du menu des objets.
  ChatLinkGesture item_gesture = ChatLinkGesture::kDescribe;
};

// `link_facts` : ce que le menu d'un item sait d'avance — vote shop, album.
[[nodiscard]] VendingLogAction DrawVendingLogWindow(VendingLogWindowState& state,
                                                    const VendingLogView& view, const SkinArt& art,
                                                    ItemLinkFacts link_facts = {});

}  // namespace ro::ui
