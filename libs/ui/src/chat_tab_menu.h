#pragma once

#include <span>

#include "ro/ui/chat_strip.h"
#include "ro/ui/chat_window.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// LE MENU D'UN ONGLET DE LA CONVERSATION, à part de la fenêtre.
//
// ⚠ UN EN-TÊTE INTERNE, et il n'a pas vocation à sortir de `src/` : seule la
// conversation ouvre ce menu, et il se dessine dans SA fenêtre — l'identifiant
// d'un popup se hache avec la pile d'identifiants de la fenêtre courante.

// L'identité du menu d'un onglet. Elle ne se montre pas.
inline constexpr const char* kChatTabMenuId = "##menu_onglet";

// LE MENU D'UN ONGLET, ouvert au clic droit sur sa languette, comme au client.
// Sans effet tant qu'il n'est pas ouvert ; refermé ailleurs, il cesse le
// réglage en cours.
void DrawChatTabMenu(ChatWindowState& state, const ChatAppearance& look,
                     std::span<const ChatTypeToggle> types, std::span<const ChatStripTab> tabs,
                     int tabs_everywhere, const ChatTabLook& tuned, const SkinArt& art,
                     ChatWindowAction* asked);

}  // namespace ro::ui
