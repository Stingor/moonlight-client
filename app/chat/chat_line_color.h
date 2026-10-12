#pragma once

#include <cstdint>

#include <imgui.h>

#include "ro/sim/chat_line.h"

namespace moonclient {

// LA COULEUR D'UNE LIGNE DU FIL, selon son genre.
//
// ⚠ UNE SEULE TABLE POUR DEUX LECTEURS : le fil qui peint les lignes reçues, et
// le sélecteur de destination qui annonce, entrée par entrée, la couleur que
// prendra la ligne envoyée. Deux tables finiraient par diverger, et le joueur
// lirait « Groupe » dans une teinte et sa phrase dans une autre — c'est
// exactement ce qui fait croire qu'elle est partie ailleurs.
//
// ⚠ LE GROUPE EST ROSE, LA GUILDE VERTE : ce sont les teintes que les joueurs de
// Ragnarok associent à ces deux genres. Les inverser fait lire une phrase de
// groupe comme une phrase de guilde, alors qu'elle est partie au bon endroit.
//
// Zéro veut dire « la couleur ordinaire du fil ». `ui` ne connaît pas `sim` :
// c'est donc ici, en `app`, que le genre devient une teinte.
[[nodiscard]] ImU32 ColorOfChatKind(ro::sim::ChatKind kind);

// LA COULEUR D'UNE LIGNE : celle que le serveur a demandée, ou celle de son
// genre. `asked_rgb` est en 0xRRGGBB, zéro quand le serveur n'en demande pas.
// ⚠ L'OPACITÉ EST LA NÔTRE : le serveur n'envoie que trois octets, et les
// compléter par zéro rendrait la ligne invisible.
[[nodiscard]] ImU32 ColorOfChatLine(ro::sim::ChatKind kind, std::uint32_t asked_rgb);

// LA MÊME COULEUR, RAMENÉE EN `0xRRGGBB` : zéro pour la couleur ordinaire du
// fil. C'est la forme sous laquelle l'historique la garde, puisqu'il n'écrit
// pas le genre qui la donnait.
[[nodiscard]] std::uint32_t ShownRgbOfChatLine(ro::sim::ChatKind kind, std::uint32_t asked_rgb);

// La couleur d'une ligne relue, depuis cette forme-là.
[[nodiscard]] ImU32 ColorOfShownRgb(std::uint32_t shown_rgb);

// LE TEXTE D'UNE BULLE TEINTÉE, depuis `Actor::chat_bgr` (0x00BBGGRR). Zéro pour
// la couleur du thème. ⚠ PAS `ColorOfShownRgb` : la bulle lit ses octets dans
// l'autre ordre que la ligne.
[[nodiscard]] ImU32 ColorOfBubbleBgr(std::uint32_t bubble_bgr);

// LE FOND SUR LEQUEL CES TEINTES SONT PENSÉES : celui, sombre, du fil. Elles sont
// claires, et posées telles quelles sur une liste ou un champ clairs, elles ne
// se liraient plus — qui les montre ailleurs que dans le fil les pose sur lui.
inline constexpr ImU32 kChatLineBackdropColor = IM_COL32(40, 40, 46, 255);

}  // namespace moonclient
