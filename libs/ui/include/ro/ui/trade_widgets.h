#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/ui/chat_link.h"
#include "ro/ui/item_link_cell.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LES BRIQUES DES FENÊTRES DE COMMERCE ────────────────────────────────────
//
// Boutique de PNJ, échoppes de vente et d'achat, troc : quatre fenêtres, une
// même ligne d'objet, un même prix, un même geste de quantité. Les tenir ici
// est ce qui les garde pareilles.

// L'icône d'une ligne, au côté, avant l'échelle de l'interface.
inline constexpr float kTradeIconPixels = 20.0F;

// Le texte noir des corps clairs, et les deux couleurs de verdict d'une ligne
// d'en-tête : réussi, refusé.
inline constexpr ImVec4 kTradeBlackText = ImVec4(0.0F, 0.0F, 0.0F, 1.0F);
inline constexpr ImVec4 kTradeDoneText = ImVec4(0.10F, 0.55F, 0.20F, 1.0F);
inline constexpr ImVec4 kTradeRefusedText = ImVec4(0.80F, 0.15F, 0.15F, 1.0F);
// Un prix qu'on ne peut pas payer.
inline constexpr ImVec4 kTradeUnaffordableText = ImVec4(0.75F, 0.15F, 0.15F, 1.0F);

// UN OBJET À MONTRER DANS UNE LIGNE.
struct TradeItemView {
  std::uint32_t item_id = 0;
  // Le nom COMPOSÉ, prêt à montrer — raffinement, cartes, emplacements.
  std::string_view name;
  // L'icône, vide quand l'installation ne l'a pas : la case montre alors un
  // point d'interrogation grisé plutôt que rien.
  SkinImage icon;
  bool damaged = false;
};

// CE QU'UNE LIGNE D'OBJET A VU PASSER pendant l'image.
//
// 🔴 UN ITEM DE COMMERCE SE MANIE COMME UNE CELLULE D'OBJET (`DrawItemLinkCell`) :
// Maj + clic gauche repose son lien dans la saisie, le clic droit ouvre LE menu
// des objets, Ctrl + clic droit la description sans passer par lui — le geste
// de tous les items du client. Le clic gauche simple reste aux actions de la
// ligne — panier, glisser, double-clic. Les clics se lisent au relâchement :
// partie de l'enfoncement, la fenêtre qu'ils ouvrent naîtrait derrière le
// commerce.
struct TradeItemGesture {
  bool hovered = false;
  // Maj + clic gauche : le lien dans la saisie.
  bool repost = false;
  // Le clic droit : le menu.
  bool menu = false;
  // Ctrl + clic droit : la description.
  bool describe = false;
};

// L'icône et le nom, sur la ligne courante. Le nom d'un objet cassé porte
// l'ombre rouge du client ; `suffix` s'écrit après le nom (« x3 »).
TradeItemGesture DrawTradeItem(const TradeItemView& item, std::string_view suffix = {});

// Ajoute à `gesture` ce que le DERNIER élément ImGui a vu passer : pour une
// ligne qui groupe l'item avec ce qui l'accompagne, et se manie d'un bloc.
void AddLastItemGesture(TradeItemGesture& gesture);

// ARME LE MENU sur le clic droit d'une ligne. `token` est ce qui désigne l'item
// DANS SA FENÊTRE — une case, un rang —, rendu tel quel avec le choix : seule
// la fenêtre sait le relire. Rend vrai sur Maj + clic, que l'appelant note
// comme une demande de lien.
bool NoteTradeItemLink(const TradeItemGesture& gesture, const TradeItemView& item,
                       std::uint32_t token, ItemLinkMenu& menu);

// LE MENU qu'une ligne a armé : celui de la cellule d'objet (`DrawItemLinkMenu`),
// et la même règle — une fois par image, au niveau de la fenêtre, hors de toute
// table et de tout enfant. Rend ce qu'on y a choisi, avec l'item et le jeton de
// la ligne ; `kNothing` sinon.
[[nodiscard]] ChatLinkAsk DrawTradeItemMenu(const char* popup_id, ItemLinkMenu& menu,
                                            std::span<const VoteShopOffer> vote_shop);

// UN PRIX : « base -> facturé » quand ils diffèrent, sinon le prix seul.
//
// ⚠ LE PRIX FACTURÉ EST CELUI QUE LE SERVEUR A CALCULÉ, remise ou surcharge
// comprise : c'est lui qui s'écrit en clair et se totalise, la base n'est
// qu'un rappel grisé.
void DrawTradePrice(std::int64_t base_price, std::int64_t price, ImVec4 color,
                    std::string_view unit);

// La largeur que `DrawTradePrice` occupera : une colonne de prix se règle sur
// la plus large de ses lignes plutôt que sur une largeur devinée.
[[nodiscard]] float TradePriceWidth(std::int64_t base_price, std::int64_t price,
                                    std::string_view unit);

// Un texte grisé qui revient à la ligne au bord de la place disponible : une
// consigne trop longue ne se rogne pas.
void DrawTradeHint(std::string_view text);

// La place que réserve, sous une liste, le pied d'une fenêtre de commerce : un
// filet puis une ligne de boutons. Comptée au pixel près, sans quoi la fenêtre
// déborde et s'offre un ascenseur qui n'a rien à faire défiler.
[[nodiscard]] float TradeFooterHeight(const SkinArt& art);

// Un montant groupé par milliers, suivi de son unité : « 1,234,567z ».
[[nodiscard]] std::string TradeAmountText(std::int64_t amount, std::string_view unit);

// LES PALIERS DE QUANTITÉ D'UNE LIGNE : +1, +10, +100, +1k à l'achat ; à la
// vente, les paliers sous ce qu'on possède, puis ce qu'on possède — « tout ».
inline constexpr std::array<int, 4> kTradeQuantitySteps = {1, 10, 100, 1000};
inline constexpr std::size_t kTradeMostSteps = kTradeQuantitySteps.size();

struct TradeSteps {
  std::array<int, kTradeMostSteps> values{};
  std::size_t count = 0;
};

// Les paliers à proposer, bornés à `most` : un palier qui dépasserait ce qu'on
// peut prendre ne se propose pas, et le dernier devient `most` lui-même quand
// il reste de la place.
[[nodiscard]] TradeSteps TradeStepsFor(int most, bool selling);

// Ce qu'un palier a demandé : combien, et s'il faut agir TOUT DE SUITE (Ctrl).
struct TradeStepClick {
  int amount = 0;
  bool immediate = false;
};

// La bourse qui ne juge rien : une vente, ou un solde que le serveur n'a pas dit.
inline constexpr std::int64_t kTradeNoPurse = -1;

// Les boutons de paliers, sur la ligne courante.
//
// ⚠ CTRL ENFONCÉ, UN PALIER AGIT TOUT DE SUITE, et c'est alors qu'il se grise
// s'il coûte plus que `purse` : le clic simple ne fait que remplir le panier,
// qui a son propre verdict. `kTradeNoPurse` ne grise rien.
TradeStepClick DrawTradeSteps(const TradeSteps& steps, const SkinArt& art, std::int64_t unit_price,
                              std::int64_t purse);

// La largeur que `DrawTradeSteps` occupera, écarts compris : une colonne de
// paliers se règle sur la plus large de ses lignes.
[[nodiscard]] float TradeStepsWidth(const TradeSteps& steps, const SkinArt& art);

// Un champ de quantité borné à `[1, most]`, flanqué de « - » et « + ».
// Rend vrai quand la quantité a changé.
bool DrawTradeQuantity(int& amount, int most, const SkinArt& art);

// ── LES MONTANTS SAISIS ET LUS ──────────────────────────────────────────────

// Le plus grand montant qu'une saisie rend : au-delà, elle SATURE plutôt que de
// se replier. Un prix trop grand doit rester trop grand pour le contrôle qui le
// refuse, pas redevenir petit.
inline constexpr std::int64_t kTradeTypedAmountCeiling = 999'999'999'999;

// UN MONTANT TAPÉ PAR LE JOUEUR, séparateurs de milliers compris :
// « 1,500,000 », « 1.500.000 », « 1 500 000 » et « 1500000 » se valent.
//
// ⚠ UN SÉPARATEUR N'EST ADMIS QU'ENTRE DES GROUPES DE TROIS CHIFFRES : « 1,5 »
// ne se lit pas comme quinze. Un prix mal compris d'un facteur mille est le
// pire qu'une saisie puisse faire, et le refuser coûte seulement de le retaper.
// Vide, ou tout autre caractère : rien.
[[nodiscard]] std::optional<std::int64_t> ParseTradeAmount(std::string_view text);

// Un montant EN TOUTES LETTRES GROUPÉES : « 1 milliard 250 millions 300 mille
// 5 ». C'est ce que le client d'origine montre pour qu'un zéro de trop se voie,
// les chiffres seuls se relisant mal passé sept.
[[nodiscard]] std::string SpelledTradeAmount(std::int64_t amount);

// ── LA CASE D'UNE GRILLE ────────────────────────────────────────────────────

// Le côté d'une case de grille, avant l'échelle : celle du sac.
inline constexpr float kTradeTilePixels = 32.0F;

// Une case d'objet, jointive, sur la ligne courante : l'icône à sa taille d'art,
// et le badge — le raffinement, SINON la quantité au-delà d'un. C'est un bouton
// invisible : l'appelant peut en faire la source d'un glisser, et y lire le
// double-clic.
TradeItemGesture DrawTradeTile(const TradeItemView& item, int refine_level, int amount);

// ── LA BOÎTE D'AVIS ─────────────────────────────────────────────────────────

// UN AVIS DU SERVEUR QUI SE DIT DANS UNE BOÎTE, et non dans la conversation :
// celui qui doit être lu avant de continuer. `app` écrit le texte ; la boîte
// s'ouvre, et le vide quand le joueur la ferme.
struct TradeNoticeState {
  std::string text;
  // La boîte a été demandée à ImGui.
  bool asked = false;
};

// `id` porte la partie `###` stable ; le titre montré est le mot du jeu.
void DrawTradeNotice(TradeNoticeState& notice, const char* id, const SkinArt& art);

}  // namespace ro::ui
