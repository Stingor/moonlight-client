#pragma once

#include <span>
#include <string>
#include <string_view>

#include <imgui.h>

#include "ro/ui/chat_input.h"
#include "ro/ui/chat_link.h"

namespace ro::ui {

// LES PASTILLES DE LIEN DE LA RANGÉE DE SAISIE, à part de la rangée.
//
// ⚠ UN EN-TÊTE INTERNE, et il n'a pas vocation à sortir de `src/` : personne
// hors de la rangée ne peint des pastilles, et leur géométrie n'a de sens que
// posée sur le champ qui vient d'être soumis.
//
// Peint les pastilles par-dessus le texte du champ et rend le geste qu'on vient
// d'y faire. `field_at` et `field_size` sont ceux du widget, relevés AVANT que
// la rangée pose autre chose ; `wrap_width` vaut zéro pour une ligne unique.
[[nodiscard]] ChatLinkAsk PaintInputLinkChips(const ChatInputState& state,
                                              std::span<const ChatInputLink> links, ImGuiID field,
                                              ImVec2 field_at, ImVec2 field_size, float wrap_width,
                                              bool active, bool hovered,
                                              std::span<const VoteShopOffer> vote_shop);

// LE NOM `:nom:` D'UNE ÉMOTICONE, tel qu'il s'écrit dans une saisie.
[[nodiscard]] std::string EmoteTokenText(std::string_view token);

}  // namespace ro::ui
