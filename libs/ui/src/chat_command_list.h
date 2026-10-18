#pragma once

// LE DESSIN DE LA LISTE DES COMMANDES, au-dessus de la barre de saisie.
//
// En-tête PRIVÉ de `libs/ui/src` : seule la rangée de saisie la dessine. Le
// filtrage et le clavier vivent dans `ro/ui/chat_command_hints.h`.

#include <cstddef>
#include <optional>
#include <span>

#include <imgui.h>

#include "ro/ui/chat_command_hints.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// Dessine la liste, son bas collé au-dessus de `field_corner` et large de
// `width`. Rend le rang de la rangée cliquée, ou rien.
//
// ⚠ LE CLIC SE PREND À L'ENFONCEMENT : cliquer hors du champ le désactive à la
// fin de l'image, et la liste, qui ne se montre qu'au champ actif, aurait
// disparu avant le relâchement.
[[nodiscard]] std::optional<std::size_t> DrawChatCommandList(ChatCommandListState& state,
                                                             const ChatCommandListView& view,
                                                             std::span<const ChatCommandHint> hints,
                                                             ImVec2 field_corner, float width,
                                                             const SkinArt& art);

}  // namespace ro::ui
