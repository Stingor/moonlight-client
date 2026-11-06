#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

namespace ro::ui {

// ── LE LAPHINE ──────────────────────────────────────────────────────────────
//
// Un objet qu'on utilise ouvre cette fenêtre, et ce qu'on y dépose en sort
// transformé : les objets du sac à gauche, ce qui est déposé à droite.
//
// 🔴 CE QUI EST DÉPOSÉ N'EST PAS À LA FENÊTRE : `app` le tient, juge et montre. Elle
// DEMANDE un dépôt, un retrait, une transformation.
//
// 🔴 UNE TRANSFORMATION SE CONFIRME TOUJOURS : elle consomme des items, et rien ne
// les rend.

inline constexpr const char* kLaphineWindowId = "laphine";

// UN OBJET DU SAC, déposable ou non.
struct LaphineItemView {
  std::uint16_t client_slot = 0;
  TradeItemView item;
  // Combien il en part, écrit après le nom ; zéro n'écrit rien.
  int amount = 0;
  // Pourquoi il ne se dépose pas, déjà traduit ; vide s'il le peut.
  std::string_view refusal;
  // Ce que le joueur doit savoir avant de le déposer, déjà traduit.
  std::string_view warning;
};

// UN OBJET QUE LA RECETTE ACCEPTE, tenu ou non.
struct LaphineAcceptedView {
  TradeItemView item;
  int amount = 0;
};

struct LaphineView {
  std::string_view title;
  // Ce qu'il faut faire, déjà écrit.
  std::string_view instruction;
  // Le nom du bouton qui transforme.
  std::string_view make_label;
  std::span<const LaphineItemView> candidates;
  std::span<const LaphineItemView> deposited;
  std::span<const LaphineAcceptedView> accepted;
  // Le rang de ce qui est déposé : la confirmation le renvoie, et `app` refuse une
  // demande dont le dépôt a changé depuis.
  std::uint32_t piles_seq = 0;
  // Une demande attend sa réponse : le bouton se retient, le dépôt aussi.
  bool waiting = false;
  // Le dernier refus du serveur, déjà écrit.
  std::string_view notice;
  // Les raisons, déjà traduites, qui retiennent le bouton.
  std::span<const std::string_view> blockers;
  // La question de la confirmation, déjà écrite.
  std::string_view confirmation;
};

// Ce que la fenêtre garde d'une image à l'autre.
struct LaphineWindowState {
  // Recopié par `app` depuis le modèle ; la croix, Échap et « Fermer » le baissent.
  bool open = false;
  // Le rang de l'ouverture montrée : une ouverture neuve remet tout à zéro.
  std::uint32_t opening_seen = 0;
  // La confirmation attendue, et le rang du dépôt qu'elle regarde.
  bool asking = false;
  std::uint32_t asked_piles_seq = 0;
  // Le menu qu'un clic droit sur un item a armé.
  ItemLinkMenu item_menu;
};

struct LaphineWindowAction {
  // La croix, Échap ou « Fermer ». ⚠ AUSSI ÉCRITE DANS `state.open`.
  bool close = false;
  // Une case à déposer, à retirer ; zéro pour rien.
  std::uint16_t deposit_slot = 0;
  std::uint16_t withdraw_slot = 0;
  // La transformation confirmée, sur le dépôt de ce rang.
  bool make = false;
  std::uint32_t piles_seq = 0;
  // Ce que la souris survole et ce qu'un geste vise : une case du sac, ou un item de
  // la liste des acceptés.
  std::uint16_t hovered_slot = 0;
  std::uint32_t hovered_id = 0;
  std::uint16_t describe_slot = 0;
  std::uint32_t describe_id = 0;
  ChatLinkGesture item_gesture = ChatLinkGesture::kDescribe;
};

// Remet la fenêtre à l'état d'une ouverture neuve.
void OpenLaphineOn(LaphineWindowState& state, std::uint32_t opening_seq);

// Dessine la fenêtre si elle est ouverte.
[[nodiscard]] LaphineWindowAction DrawLaphineWindow(LaphineWindowState& state,
                                                    const LaphineView& view, const SkinArt& art,
                                                    ItemLinkFacts link_facts = {});

}  // namespace ro::ui
