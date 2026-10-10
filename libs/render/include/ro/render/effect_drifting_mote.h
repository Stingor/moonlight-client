#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE GRAIN QUI DÉRIVE : la figure du sous-type 96.
//
// C'est la plus simple de tout le lot : UN carré par piste, tourné vers la
// caméra, posé à un décalage que le mouvement fait dériver.
//
// ⚠ QUATRE PISTES, UN SEUL CARRÉ CHACUNE. Compter quatre quads par
// piste en peindrait seize là où le client en peint quatre.

// DE COMBIEN LE CARRÉ EST TOURNÉ, et de combien ses coins s'écartent.
//
// ⚠ ENCORE QUARANTE-CINQ DEGRÉS : le client lit ses tables aux index 45, 135,
// 225 et 315. C'est la quatrième figure du lot bâtie sur ce même carré.
inline constexpr std::int32_t kDriftingMoteFirstCornerDegrees = 45;
inline constexpr std::int32_t kDriftingMoteDegreesPerCorner = 90;

// OÙ VIT LE DÉCALAGE du grain : les trois cases de queue TROIS à CINQ.
inline constexpr std::size_t kDriftingMoteSlotOfSideways = 3;
inline constexpr std::size_t kDriftingMoteSlotOfDepthBelow = 4;
inline constexpr std::size_t kDriftingMoteSlotOfForward = 5;

// LES TROIS TEINTES, choisies par la marque du nœud.
inline constexpr std::uint8_t kDriftingMoteMarkBlue = 2;
inline constexpr std::uint8_t kDriftingMoteMarkDim = 3;
inline constexpr std::array<std::uint8_t, 3> kDriftingMoteBlue = {105, 105, 225};
inline constexpr std::array<std::uint8_t, 3> kDriftingMoteDim = {55, 55, 55};
inline constexpr std::array<std::uint8_t, 3> kDriftingMoteWhite = {255, 255, 255};

[[nodiscard]] std::array<std::uint8_t, 3> EffectDriftingMoteColor(std::uint8_t mark);

// LE CARRÉ D'UNE PISTE, ou rien.
//
// `turn_degrees` et `tilt_degrees` sont les deux angles de caméra, que le
// mouvement range dans les champs de STYLE des pistes zéro et un.
//
// ⚠ IL NE SE MONTRE QUE SOUS LE NŒUD dans le repère du client — donc AU-DESSUS
// dans le nôtre —, et le client teste la hauteur du PREMIER COIN, pas celle du
// centre. Tester le centre le ferait paraître un demi-carré trop tôt.
[[nodiscard]] std::optional<EffectWorldQuad> ComposeDriftingMoteQuad(const EffectTrack& track,
                                                                     std::int16_t turn_degrees,
                                                                     std::int16_t tilt_degrees,
                                                                     glm::vec3 anchor);

}  // namespace ro::render
