#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

namespace ro::ui {

// ── LA RÉFORME ──────────────────────────────────────────────────────────────
//
// Un PNJ ou un objet ouvre cette fenêtre : à gauche les pièces du sac que le
// déclencheur réforme, à droite la pièce choisie, ce qu'elle devient et les
// matériaux qu'elle coûte, possédés contre requis.
//
// 🔴 LE CHOIX N'EST PAS À LA FENÊTRE : `app` le tient, juge et montre. Elle
// DEMANDE un choix, une réforme.
//
// 🔴 UNE RÉFORME SE CONFIRME TOUJOURS : elle consomme la pièce et ses matériaux.

inline constexpr const char* kItemReformWindowId = "item_reform";

// UNE PIÈCE DU SAC, réformable ou non.
struct ItemReformPieceView {
  std::uint16_t client_slot = 0;
  TradeItemView item;
  // Pourquoi elle ne se choisit pas, déjà traduit ; vide si elle le peut.
  std::string_view refusal;
  bool chosen = false;
};

// UN MATÉRIAU DE LA PIÈCE CHOISIE.
struct ItemReformMaterialView {
  TradeItemView item;
  // Ce que le serveur compte, et ce qu'il demande.
  int owned = 0;
  int needed = 0;
};

struct ItemReformView {
  std::string_view title;
  // Ce qu'il faut faire, déjà écrit.
  std::string_view instruction;
  std::span<const ItemReformPieceView> candidates;
  // La pièce choisie et ce qu'elle devient ; `item_id` nul quand rien n'est choisi.
  TradeItemView chosen;
  TradeItemView result;
  std::span<const ItemReformMaterialView> materials;
  // Le rang du choix : la confirmation le renvoie, et `app` refuse une demande dont
  // le choix a changé depuis.
  std::uint32_t choice_seq = 0;
  // Une demande attend sa réponse : le bouton se retient, le choix aussi.
  bool waiting = false;
  // Le dernier refus du serveur, déjà écrit.
  std::string_view notice;
  // Les raisons, déjà traduites, qui retiennent le bouton.
  std::span<const std::string_view> blockers;
  // La question de la confirmation, déjà écrite.
  std::string_view confirmation;
};

// Ce que la fenêtre garde d'une image à l'autre.
struct ItemReformWindowState {
  // Recopié par `app` depuis le modèle ; la croix, Échap et « Fermer » le baissent.
  bool open = false;
  // Le rang de l'ouverture montrée : une ouverture neuve remet tout à zéro.
  std::uint32_t opening_seen = 0;
  // La confirmation attendue, et le rang du choix qu'elle regarde.
  bool asking = false;
  std::uint32_t asked_choice_seq = 0;
  // Le menu qu'un clic droit sur un item a armé.
  ItemLinkMenu item_menu;
};

struct ItemReformWindowAction {
  // La croix, Échap ou « Fermer ». ⚠ AUSSI ÉCRITE DANS `state.open`.
  bool close = false;
  // La case à choisir ; zéro pour rien.
  std::uint16_t choose_slot = 0;
  // La réforme confirmée, sur le choix de ce rang.
  bool reform = false;
  std::uint32_t choice_seq = 0;
  // Ce que la souris survole et ce qu'un geste vise : une case du sac, ou un item
  // désigné par son identifiant (résultat, matériau).
  std::uint16_t hovered_slot = 0;
  std::uint32_t hovered_id = 0;
  std::uint16_t describe_slot = 0;
  std::uint32_t describe_id = 0;
  ChatLinkGesture item_gesture = ChatLinkGesture::kDescribe;
};

// Remet la fenêtre à l'état d'une ouverture neuve.
void OpenItemReformOn(ItemReformWindowState& state, std::uint32_t opening_seq);

// Dessine la fenêtre si elle est ouverte.
[[nodiscard]] ItemReformWindowAction DrawItemReformWindow(ItemReformWindowState& state,
                                                          const ItemReformView& view,
                                                          const SkinArt& art,
                                                          ItemLinkFacts link_facts = {});

}  // namespace ro::ui
