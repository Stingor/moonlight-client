#pragma once

#include <cstdint>
#include <span>
#include <vector>

#include <imgui.h>

#include "ro/render/camera.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/chat_bubble.h"

#include "world/ground_unit_pick.h"

// CE QU'UNE UNITÉ AU SOL DIT : le message d'une Talkie Box qui se déclenche,
// posé au-dessus d'elle comme une parole au-dessus d'une tête.
namespace moonclient {

// LA COULEUR DU MESSAGE : bleu plein, rouge et vert à moitié. ⚠ La couleur
// d'une bulle se donne en ordre bleu-vert-rouge, le rouge en dernier : lue dans
// l'autre sens, la même valeur donne un rose.
inline constexpr ImU32 kTalkboxBubbleColor = IM_COL32(0x80, 0x80, 0xFF, 0xFF);

// De combien le message monte au-dessus du point de sa case : une part fixe de
// la hauteur d'écran, cent dix pixels sur quatre cent quatre-vingts.
inline constexpr float kTalkboxBubbleLiftOn480 = 110.0F;
inline constexpr float kTalkboxBubbleReferenceHeight = 480.0F;

// Les bulles des unités qui parlent encore. Une parole dure ce qu'une parole
// d'acteur dure : voir `ro::ui::ChatBubbleLifeMs`.
//
// ⚠ LA VUE RENDUE POINTE DANS LE MONDE : elle vit tant que le texte de l'unité.
[[nodiscard]] std::vector<ro::ui::ChatBubbleView> TalkboxBubblesOnScreen(
    const ro::render::Camera& camera, float aspect_ratio, std::span<const GroundUnitPick> speakers,
    const ro::sim::World& world, std::uint32_t now_ms, float viewport_width, float viewport_height,
    const ro::ui::ChatBubbleLook& look);

}  // namespace moonclient
