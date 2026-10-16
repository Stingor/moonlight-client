#pragma once

#include <string>

#include <imgui.h>

#include "ro/ui/character_seats.h"
#include "ro/ui/character_select_window.h"
#include "ro/ui/skin_art.h"

// LES RETOUCHES D'UN CONVIVE SUR LA TABLÉE : le changer de place, le renommer.
//
// En-tête PRIVÉ de la scène. Elle appelle ces pièces là où elle dessine un
// convive, où elle arbitre un clic sur un siège, et après elle-même pour la
// fenêtre.
namespace ro::ui {

// L'ÉTIQUETTE DES RETOUCHES, sous le nom : ce qu'il reste à ce personnage de
// déplacements, et s'il peut changer de nom. `top` est le haut de la ligne.
//
// 🔴 C'EST ELLE QUI DIT AU JOUEUR QUE SON COUPON A PORTÉ : le coupon s'utilise
// en jeu, et c'est ici seulement que son effet se voit.
void DrawAmendTag(ImDrawList* list, const SeatFrame& seat, const CharacterSlotView& who, float top);

// CET EMPLACEMENT PEUT-IL RECEVOIR LE CONVIVE QU'ON DÉPLACE ?
//
// ⚠ SEULEMENT CE QUE LE COMPTE PEUT REMPLIR : le serveur refuse une arrivée
// au-delà de `char_slots`. Une place occupée en est une, et le serveur échange
// alors les deux convives.
[[nodiscard]] bool IsMoveTarget(const CharacterSelectState& state, const CharacterSelectView& view,
                                int slot);

// L'infobulle d'une arrivée possible, au survol.
void MoveTargetTooltip(const CharacterSlotView& who, int slot);

// Ce que le bandeau dit pendant le mode déplacement.
[[nodiscard]] std::string MoveBannerText(const CharacterSelectState& state,
                                         const CharacterSelectView& view);

// LA FENÊTRE DU NOUVEAU NOM. Sans effet quand le brouillon est fermé.
//
// ⚠ ELLE SE DESSINE APRÈS LA SCÈNE, donc au-dessus d'elle : celle-ci est en
// `NoBringToFrontOnFocus`. Annuler la ferme ici même ; la réussite, c'est `app`
// qui la lit et qui ferme.
[[nodiscard]] CharacterSelectAction DrawRenameWindow(CharacterSelectState& state,
                                                     const CharacterSelectView& view,
                                                     const SkinArt& art);

}  // namespace ro::ui
