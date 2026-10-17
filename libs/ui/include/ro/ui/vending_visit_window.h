#pragma once

#include <cstdint>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

namespace ro::ui {

// ── L'ÉCHOPPE D'UN AUTRE JOUEUR ─────────────────────────────────────────────
//
// Les offres avec leur stock, leur prix et une quantité ; un panier ; le total
// et les zeny ; « Acheter ». Ctrl + clic sur « Ajouter » achète toute la ligne
// tout de suite.
//
// Trois gardes de sûreté :
// - le Ctrl + clic se GRISE quand la ligne coûte plus que la bourse, et `app`
//   le rejuge comme un panier, au lieu de l'envoyer sans compter ;
// - un achat au-delà de `kVendingConfirmTotalZeny` se CONFIRME, total montré —
//   le prix de la liste est exactement celui que le serveur facture ;
// - le panier ne se vide qu'une fois l'achat SERVI : un refus le laisse tel
//   qu'il était, au lieu de le perdre avant même la réponse.
//
// ⚠ AUCUN PAQUET NE FERME UNE VISITE : la croix, Échap et « Fermer » ne font
// que la clore ici.

inline constexpr const char* kVendingVisitWindowId = "###vending_visit";

// Le total à partir duquel un achat se confirme : le seuil du « safe check »
// du client d'origine pour les prix.
inline constexpr std::int64_t kVendingConfirmTotalZeny = 10'000'000;

// UNE OFFRE DU VENDEUR.
struct VendingOfferView {
  TradeItemView item;
  // La case que l'achat renvoie. ⚠ JAMAIS ZÉRO : celle du cart du vendeur, que
  // le client décale de deux.
  std::uint16_t shop_slot = 0;
  int stock = 0;
  std::int64_t price = 0;
};

// UNE LIGNE DU PANIER : une offre, une quantité.
struct VendingBasketEntry {
  std::uint16_t shop_slot = 0;
  int amount = 0;
};

struct VendingVisitView {
  // Le nom du vendeur, et le titre de son enseigne ; l'un ou l'autre peut
  // manquer.
  std::string_view vendor_name;
  std::string_view title;
  std::span<const VendingOfferView> offers;
  // La bourse ; `kTradeNoPurse` si le serveur ne l'a pas dite.
  std::int64_t zeny = kTradeNoPurse;
  // Un achat attend sa liste : rien ne repart avant elle.
  bool awaiting = false;
  // LE VERDICT DU PANIER, déjà traduit : ce qui retient « Acheter ».
  std::span<const std::string_view> blockers;
  std::int64_t basket_total = 0;
  // LE DERNIER REFUS, avec les mots du jeu ; vide s'il n'y en a pas.
  std::string_view outcome;
};

struct VendingVisitWindowState {
  bool open = false;
  std::uint32_t opening_seen = 0;
  std::vector<VendingBasketEntry> basket;
  // La quantité tapée par offre, par case.
  std::vector<std::pair<std::uint16_t, int>> offer_amounts;
  // L'ACHAT QUI ATTEND SA CONFIRMATION, et s'il vient d'un Ctrl + clic.
  std::vector<VendingBasketEntry> pending;
  bool pending_immediate = false;
  // La boîte de confirmation est demandée. ⚠ ELLE S'OUVRE AU NIVEAU DE LA
  // FENÊTRE, là où elle se dessine, jamais depuis une ligne de table.
  bool pending_asked = false;
  // Le menu qu'un clic droit sur un item a armé ; son jeton est la case de
  // l'échoppe.
  ItemLinkMenu item_menu;
};

struct VendingVisitAction {
  // La croix, Échap ou « Fermer ». ⚠ AUSSI ÉCRIT DANS `state.open`.
  bool close = false;
  // CE QUI PART, confirmé s'il le fallait. ⚠ `app` REJUGE.
  std::vector<VendingBasketEntry> buy;
  // Un Ctrl + clic : le panier n'est pas touché.
  bool immediate = false;
  // La case de l'échoppe survolée, et celle qu'un geste vise ; zéro pour aucune.
  std::uint16_t hovered_slot = 0;
  std::uint16_t describe_slot = 0;
  // Ce que le geste demande de l'item visé : sa description, son lien dans la
  // saisie (Maj + clic), ou une autre entrée du menu des objets.
  ChatLinkGesture item_gesture = ChatLinkGesture::kDescribe;
};

// Remet la fenêtre à l'état d'une visite neuve.
void OpenVendingVisitOn(VendingVisitWindowState& state, std::uint32_t opening_seq);

// Ajoute au panier, ou à la ligne de la même offre, borné au stock. Rend faux
// si rien n'est entré.
bool AddToVendingBasket(std::vector<VendingBasketEntry>& basket, std::uint16_t shop_slot,
                        int amount, int stock);

// Le coût d'un panier au prix des offres ; une offre disparue ne coûte rien.
[[nodiscard]] std::int64_t VendingBasketTotal(std::span<const VendingBasketEntry> basket,
                                              std::span<const VendingOfferView> offers);

// Borne le panier à la liste qui vient d'arriver : une offre partie sort, une
// quantité au-delà du nouveau stock redescend.
void FitVendingBasket(std::vector<VendingBasketEntry>& basket,
                      std::span<const VendingOfferView> offers);

// `link_facts` : ce que le menu d'un item sait d'avance — vote shop, album.
[[nodiscard]] VendingVisitAction DrawVendingVisitWindow(VendingVisitWindowState& state,
                                                        const VendingVisitView& view,
                                                        const SkinArt& art,
                                                        ItemLinkFacts link_facts = {});

}  // namespace ro::ui
