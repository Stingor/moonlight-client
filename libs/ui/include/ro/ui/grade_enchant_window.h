#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include <imgui.h>

#include "ro/ui/skin_art.h"
#include "ro/ui/trade_widgets.h"

namespace ro::ui {

// ── L'AMÉLIORATION DE GRADE ─────────────────────────────────────────────────
//
// Un PNJ ouvre cette fenêtre : à gauche les armes du sac, à droite la pièce
// choisie, le grade qu'elle vise, la chance, la poussière qui la relève et les
// mises possibles — une petite qui risque l'arme, une grosse qui ne la risque pas.
//
// 🔴 LE CHOIX N'EST PAS À LA FENÊTRE : `app` le tient, juge et montre. Elle garde
// seulement la mise et les doses que le joueur règle, et DEMANDE.
//
// 🔴 UNE TENTATIVE SE CONFIRME TOUJOURS : elle consomme joyaux et zenys, et peut
// détruire l'arme.

inline constexpr const char* kGradeEnchantWindowId = "grade_enchant";

// UNE ARME DU SAC.
struct GradeEnchantPieceView {
  std::uint16_t client_slot = 0;
  // Le nom composé porte déjà le grade.
  TradeItemView item;
  // Pourquoi elle ne se choisit pas, déjà traduit ; vide si elle le peut.
  std::string_view refusal;
  bool chosen = false;
};

// UNE MISE DE LA PIÈCE CHOISIE.
struct GradeEnchantStakeView {
  TradeItemView item;
  // Ce que le serveur compte, et ce qu'il demande.
  int owned = 0;
  int needed = 0;
  std::int64_t zeny = 0;
  // Ce qu'un échec coûte, déjà écrit.
  std::string_view risk;
  bool risky = false;
};

struct GradeEnchantView {
  // Ce qu'il faut faire, déjà écrit.
  std::string_view instruction;
  std::span<const GradeEnchantPieceView> candidates;
  // La pièce choisie ; `item_id` nul quand rien n'est choisi.
  TradeItemView chosen;
  // « Grade : D, puis C », déjà écrit.
  std::string_view step;
  // La proposition du serveur est arrivée, et ce qu'elle dit d'abord.
  bool offer_ready = false;
  // Ce que la proposition refuse d'emblée, déjà écrit ; vide sinon.
  std::string_view offer_refusal;
  std::uint32_t chance_percent = 0;
  // La poussière : l'objet nul quand le serveur n'en offre pas.
  TradeItemView blessing;
  int blessing_owned = 0;
  int blessing_per_step = 0;
  int blessing_most_steps = 0;
  int blessing_bonus_percent = 0;
  std::span<const GradeEnchantStakeView> stakes;
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
  // La question de la confirmation, déjà écrite.
  std::string_view confirmation;
};

// Ce que la fenêtre garde d'une image à l'autre.
struct GradeEnchantWindowState {
  // Recopié par `app` depuis le modèle ; la croix, Échap et « Fermer » le baissent.
  bool open = false;
  // Le rang de l'ouverture montrée : une ouverture neuve remet tout à zéro.
  std::uint32_t opening_seen = 0;
  // La mise choisie, et les doses de poussière.
  int stake = 0;
  int blessing_steps = 0;
  // La confirmation attendue, et le rang du choix qu'elle regarde.
  bool asking = false;
  std::uint32_t asked_choice_seq = 0;
  // Le menu qu'un clic droit sur un item a armé.
  ItemLinkMenu item_menu;
};

struct GradeEnchantWindowAction {
  // La croix, Échap ou « Fermer ». ⚠ AUSSI ÉCRITE DANS `state.open`.
  bool close = false;
  // La case à choisir ; zéro pour rien.
  std::uint16_t choose_slot = 0;
  // La tentative confirmée, sur le choix de ce rang, avec la mise et les doses
  // de l'état.
  bool enchant = false;
  std::uint32_t choice_seq = 0;
  // Ce que la souris survole et ce qu'un geste vise : une case du sac, ou un item
  // désigné par son identifiant (joyau, poussière).
  std::uint16_t hovered_slot = 0;
  std::uint32_t hovered_id = 0;
  std::uint16_t describe_slot = 0;
  std::uint32_t describe_id = 0;
  ChatLinkGesture item_gesture = ChatLinkGesture::kDescribe;
};

// Remet la fenêtre à l'état d'une ouverture neuve.
void OpenGradeEnchantOn(GradeEnchantWindowState& state, std::uint32_t opening_seq);

// Dessine la fenêtre si elle est ouverte.
[[nodiscard]] GradeEnchantWindowAction DrawGradeEnchantWindow(GradeEnchantWindowState& state,
                                                              const GradeEnchantView& view,
                                                              const SkinArt& art,
                                                              ItemLinkFacts link_facts = {});

}  // namespace ro::ui
