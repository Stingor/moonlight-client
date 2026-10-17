#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <imgui.h>

#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

namespace ro::ui {

// ── LE TROC ─────────────────────────────────────────────────────────────────
//
// Une boutique de PNJ qui se paie EN ITEMS, et parfois en zeny par surcroît.
// Dessinée dans le style de la boutique de PNJ : la liste à gauche, le panier à
// droite, un clic remplit le panier, Ctrl + clic échange tout de suite.
//
// ⚠ UNE SEULE FENÊTRE À DEUX VOLETS, là où le client d'origine en ouvre deux,
// ancrées l'une à l'autre : fermer l'une sans l'autre y laissait un panier
// orphelin, et un troc que le serveur croyait encore ouvert. Ici, il n'y a
// qu'une croix, et elle ferme tout.
//
// Elle sert les deux générations : la SIMPLE, une devise par article ; l'ÉTENDUE,
// plusieurs devises dont chacune peut exiger un raffinement, un prix en zeny et
// un raffinement de l'article livré.
//
// 🔴 LE PANIER N'EST PAS À LA FENÊTRE : il vit dans le troc, que `app` tient.
// La fenêtre DEMANDE un ajout, une quantité, un vidage ; `app` juge, applique,
// et la vue suivante le montre. C'est ce qui garde un seul panier, celui qui
// part.
//
// 🔴 UN ÉCHANGE SE CONFIRME TOUJOURS, Ctrl + clic compris : un troc consomme des
// items, parfois raffinés, et rien ne les rend. La confirmation récapitule ce
// qui PART et ce qui ARRIVE.

inline constexpr const char* kBarterWindowId = "barter";

// Le stock d'un article que rien ne borne.
inline constexpr std::int64_t kBarterNoStockLimit = -1;

// UNE DEVISE D'UN PRIX : ce qu'il faut rendre, et ce que le joueur en tient.
struct BarterPriceView {
  std::uint32_t item_id = 0;
  // Le raffinement exigé ; zéro, n'importe lequel.
  std::uint16_t refine_level = 0;
  // Ce qu'il faut pour UN exemplaire de l'article.
  std::int64_t amount = 0;
  // CE QUE LE JOUEUR TIENT, compté comme le serveur le cherche : sans ce qu'il
  // porte, sans ses favoris.
  std::int64_t held = 0;
  // Un exemplaire tenu est raffiné plus haut que l'exigence : l'échange peut le
  // prendre. La confirmation le dit.
  bool may_take_higher_refine = false;
  // L'icône et le nom composé, raffinement exigé compris (« +7 Knife »).
  TradeItemView item;
};

// UN ARTICLE DE LA BOUTIQUE.
struct BarterOfferView {
  // Le nom porte le raffinement de l'article livré.
  TradeItemView item;
  std::uint32_t shop_index = 0;
  // `kBarterNoStockLimit` quand il est illimité.
  std::int64_t stock = kBarterNoStockLimit;
  // Le poids d'un exemplaire, en dixièmes.
  std::int64_t weight_tenths = 0;
  // Étendu : le zeny d'un exemplaire, en plus des devises.
  std::int64_t zeny = 0;
  std::span<const BarterPriceView> prices;
  // COMBIEN ON PEUT ENCORE EN AJOUTER au panier sans qu'il soit refusé — stock,
  // plafond de ligne, poids, devises, zeny compris. Zéro : `refusal` dit
  // pourquoi, et les paliers ne se proposent pas.
  int most = 0;
  std::string_view refusal;
};

// UNE LIGNE DU PANIER.
struct BarterCartLineView {
  std::uint32_t shop_index = 0;
  int amount = 0;
  // Ce que la ligne peut atteindre sans que le panier soit refusé.
  int most = 0;
};

struct BarterView {
  bool expanded = false;
  // Le titre de la fenêtre ; vide, « Troc ».
  std::string_view title;
  // Le titre de la liste, avec les mots du jeu ; vide, le nôtre.
  std::string_view list_title;
  // Ce que le joueur a en zeny, déjà écrit, ou vide quand il n'est pas dit.
  std::string_view purse_line;
  std::span<const BarterOfferView> offers;

  std::span<const BarterCartLineView> cart;
  // Le rang du panier : l'échange confirmé le renvoie, et `app` refuse un
  // panier qui a changé depuis.
  std::uint32_t cart_seq = 0;
  // Ce que le panier ajoute au poids, déjà écrit, ou vide.
  std::string_view cart_weight;
  // LE VERDICT DU PANIER : les raisons, déjà traduites, qui retiennent le
  // bouton.
  std::span<const std::string_view> blockers;
};

// Une ligne à échanger : l'article, et combien.
struct BarterLine {
  std::uint32_t shop_index = 0;
  int amount = 0;
};

// CE QUE LA FENÊTRE DEMANDE AU PANIER, une fois par image au plus.
enum class BarterCartEditKind : std::uint8_t { kNone, kAdd, kSet, kClear };

struct BarterCartEdit {
  BarterCartEditKind kind = BarterCartEditKind::kNone;
  // Ajout : combien de plus ; quantité : la nouvelle, zéro pour retirer.
  BarterLine line;
};

// Ce que la fenêtre garde d'une image à l'autre.
struct BarterWindowState {
  // Recopié par `app` depuis le troc ; la croix, Échap et « Fermer » le
  // baissent.
  bool open = false;
  // Le rang de l'ouverture montrée : une ouverture neuve remet tout à zéro.
  std::uint32_t opening_seen = 0;
  ImGuiTextFilter filter;
  // L'ÉCHANGE QUI ATTEND SA CONFIRMATION, et d'où il vient.
  std::vector<BarterLine> pending;
  bool pending_immediate = false;
  std::uint32_t pending_cart_seq = 0;
  // Le menu qu'un clic droit sur un item a armé.
  ItemLinkMenu item_menu;
};

struct BarterWindowAction {
  // La croix, Échap ou « Fermer ». ⚠ AUSSI ÉCRITE DANS `state.open`.
  bool close = false;
  BarterCartEdit edit;
  // L'ÉCHANGE CONFIRMÉ : les lignes, vides pour rien. `immediate` : un Ctrl +
  // clic, qui échange cette ligne SEULE ; sinon, tout le panier au rang
  // `cart_seq`.
  //
  // ⚠ `app` REJUGE AVANT D'ÉMETTRE : le verdict dessiné a une image de retard.
  std::vector<BarterLine> exchange;
  bool immediate = false;
  std::uint32_t cart_seq = 0;
  // L'item survolé, pour l'aperçu, et celui qu'un geste vise.
  std::uint32_t hovered_id = 0;
  std::uint32_t describe_id = 0;
  // Ce que le geste demande de l'item visé : sa description, son lien dans la
  // saisie (Maj + clic), ou une autre entrée du menu des objets.
  ChatLinkGesture item_gesture = ChatLinkGesture::kDescribe;
};

// CE QU'UN ÉCHANGE CONSOMME ET LIVRE, devise par devise, cumulé sur ses lignes.
struct BarterCost {
  BarterPriceView price;
  // Le total à rendre ; `price.amount` reste celui d'un exemplaire.
  std::int64_t total = 0;
  [[nodiscard]] bool enough() const noexcept { return price.held >= total; }
};

struct BarterRecap {
  std::vector<BarterCost> costs;
  std::int64_t zeny = 0;
  // Ce qui arrive : l'article et sa quantité.
  std::vector<BarterLine> arrivals;
  // Une devise PART RAFFINÉE, exigée ou possiblement prise plus haut.
  bool refined_leaves = false;
};

// Totalise des lignes contre les articles de la vue. Une ligne dont l'article
// n'est plus dans la vue ne compte pas.
[[nodiscard]] BarterRecap RecapBarterLines(std::span<const BarterOfferView> offers,
                                           std::span<const BarterLine> lines);

// Le texte de la confirmation : ce qui part, ce qui arrive, et la mise en garde
// quand un item raffiné peut partir. Déjà traduit.
[[nodiscard]] std::string BarterConfirmationText(std::span<const BarterOfferView> offers,
                                                 const BarterRecap& recap);

// Remet la fenêtre à l'état d'une ouverture neuve.
void OpenBarterOn(BarterWindowState& state, std::uint32_t opening_seq);

// Dessine la fenêtre si elle est ouverte. `link_facts` : ce que le
// menu d'un item sait d'avance — vote shop, album.
[[nodiscard]] BarterWindowAction DrawBarterWindow(BarterWindowState& state, const BarterView& view,
                                                  const SkinArt& art,
                                                  ItemLinkFacts link_facts = {});

}  // namespace ro::ui
