#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

namespace ro::ui {

// ── L'ENCHANTEMENT ──────────────────────────────────────────────────────────
//
// Un PNJ ouvre cette fenêtre : à gauche les pièces du sac que le groupe admet, à
// droite la pièce choisie et ses quatre cases, puis trois pages — enchanter
// (tirage ou choix sûr), améliorer un enchantement, tout ôter — avec ce que
// chacune pose, coûte et réussit.
//
// 🔴 LE CHOIX N'EST PAS À LA FENÊTRE : `app` le tient, juge et montre. Elle garde
// seulement la page, l'enchantement et la case que le joueur désigne, et DEMANDE.
//
// 🔴 UNE TENTATIVE SE CONFIRME TOUJOURS : elle consomme zenys et matériaux, et peut
// échouer ou tout ôter.

inline constexpr const char* kItemEnchantWindowId = "item_enchant";

// LES PAGES, dans l'ordre des onglets.
enum class ItemEnchantPage : std::uint8_t {
  kEnchant,
  kUpgrade,
  kReset,
};

// UNE PIÈCE DU SAC, enchantable ou non.
struct ItemEnchantPieceView {
  std::uint16_t client_slot = 0;
  // Le nom composé porte déjà le raffinement et le grade.
  TradeItemView item;
  // Pourquoi elle ne se choisit pas, déjà traduit ; vide si elle le peut.
  std::string_view refusal;
  bool chosen = false;
};

// UNE DES QUATRE CASES DE LA PIÈCE CHOISIE.
struct ItemEnchantSlotView {
  std::uint8_t slot = 0;
  // Ce qu'elle porte ; `item_id` nul pour une case vide.
  TradeItemView item;
  // Une case de carte de la pièce : l'enchantement n'y va jamais.
  bool card_slot = false;
  // La case que le tirage ou le choix sûr rempliraient.
  bool next = false;
  // La case que la page Améliorer vise, et celles qu'elle peut viser.
  bool selected = false;
  bool selectable = false;
};

// UNE LIGNE DE LA LISTE : un enchantement que le tirage peut poser, que le joueur
// peut choisir, ou que l'amélioration donne.
struct ItemEnchantOptionView {
  TradeItemView item;
  // La probabilité du tirage, déjà écrite ; vide hors tirage.
  std::string_view odds;
  // Le joueur peut la choisir, et l'a choisie.
  bool selectable = false;
  bool chosen = false;
  // Ce qu'elle coûte n'est pas dans le sac ou la bourse : grisée, toujours choisissable.
  bool short_of = false;
};

// UN MATÉRIAU DE L'OPÉRATION.
struct ItemEnchantMaterialView {
  TradeItemView item;
  // Ce que le serveur compte, et ce qu'il demande.
  int owned = 0;
  int needed = 0;
};

struct ItemEnchantView {
  std::string_view title;
  // Ce qu'il faut faire, déjà écrit.
  std::string_view instruction;
  std::span<const ItemEnchantPieceView> candidates;
  // La pièce choisie ; `item_id` nul quand rien n'est choisi.
  TradeItemView chosen;
  std::span<const ItemEnchantSlotView> slots;
  // Ce que chaque page peut faire sur la pièce choisie : un onglet sans rien se grise.
  bool enchant_open = false;
  bool random_open = false;
  bool perfect_open = false;
  bool upgrade_open = false;
  bool reset_open = false;
  // Le titre de la liste, déjà écrit, et ses lignes.
  std::string_view options_title;
  std::span<const ItemEnchantOptionView> options;
  std::span<const ItemEnchantMaterialView> materials;
  // Les zenys demandés, et la bourse ; une bourse inconnue vaut `-1`.
  std::int64_t zeny = 0;
  std::int64_t purse = -1;
  // La chance de réussite, déjà écrite ; vide quand elle est certaine ou sans objet.
  std::string_view chance;
  // Ce que l'opération fait, déjà écrit (« Case 4 : tirage parmi 12 »).
  std::string_view summary;
  // Le rang du choix : la confirmation le renvoie, et `app` refuse une demande dont
  // le choix a changé depuis.
  std::uint32_t choice_seq = 0;
  // Une demande attend sa réponse : tout se retient.
  bool waiting = false;
  // Le dernier mot du serveur, déjà écrit, et sa couleur : vert pour une réussite.
  std::string_view notice;
  bool notice_good = false;
  // Les raisons, déjà traduites, qui retiennent le bouton.
  std::span<const std::string_view> blockers;
  // Le libellé du bouton et la question de la confirmation, déjà écrits.
  std::string_view action_label;
  std::string_view confirmation;
};

// Aucune case visée par la page Améliorer.
inline constexpr std::uint8_t kItemEnchantNoSlot = 0xFF;

// Ce que la fenêtre garde d'une image à l'autre.
struct ItemEnchantWindowState {
  // Recopié par `app` depuis le modèle ; la croix, Échap et « Fermer » le baissent.
  bool open = false;
  // Le rang de l'ouverture montrée : une ouverture neuve remet tout à zéro.
  std::uint32_t opening_seen = 0;
  ItemEnchantPage page = ItemEnchantPage::kEnchant;
  // Page Enchanter : le choix sûr plutôt que le tirage, et l'enchantement choisi.
  bool perfect = false;
  std::uint32_t enchant_item_id = 0;
  // Page Améliorer : la case visée, `kItemEnchantNoSlot` pour aucune.
  std::uint8_t upgrade_slot = kItemEnchantNoSlot;
  // La confirmation attendue, et le rang du choix qu'elle regarde.
  bool asking = false;
  std::uint32_t asked_choice_seq = 0;
  // Le menu qu'un clic droit sur un item a armé.
  ItemLinkMenu item_menu;
};

struct ItemEnchantWindowAction {
  // La croix, Échap ou « Fermer ». ⚠ AUSSI ÉCRITE DANS `state.open`.
  bool close = false;
  // La case du sac à choisir ; zéro pour rien.
  std::uint16_t choose_slot = 0;
  // La tentative confirmée, sur le choix de ce rang, avec la page et ce que l'état
  // désigne.
  bool enchant = false;
  std::uint32_t choice_seq = 0;
  // Ce que la souris survole et ce qu'un geste vise : une case du sac, ou un item
  // désigné par son identifiant (enchantement, matériau).
  std::uint16_t hovered_slot = 0;
  std::uint32_t hovered_id = 0;
  std::uint16_t describe_slot = 0;
  std::uint32_t describe_id = 0;
  ChatLinkGesture item_gesture = ChatLinkGesture::kDescribe;
};

// Remet la fenêtre à l'état d'une ouverture neuve.
void OpenItemEnchantOn(ItemEnchantWindowState& state, std::uint32_t opening_seq);

// Dessine la fenêtre si elle est ouverte.
[[nodiscard]] ItemEnchantWindowAction DrawItemEnchantWindow(ItemEnchantWindowState& state,
                                                            const ItemEnchantView& view,
                                                            const SkinArt& art,
                                                            ItemLinkFacts link_facts = {});

}  // namespace ro::ui
