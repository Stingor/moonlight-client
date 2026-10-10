#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE COIN DE CÔNE : deux triangles concentriques, l'un dans l'autre.
//
// C'est la figure du sous-type 89. Elle sort par la voie A, deux fois par
// piste. Huit identifiants l'emploient, tous posés par le même bâtisseur :
// celui des éventails orientés.
//
// ⚠ CHAQUE QUAD EST UN TRIANGLE DÉGUISÉ : ses deux derniers coins valent le même
// point, l'apex. La voie A ne sait émettre que des quads, et le client s'en sert
// pour poser des triangles en répétant un sommet. Chercher un quatrième coin
// distinct n'en trouverait pas.
//
// ⚠ ET LES DEUX COINS N'ONT JAMAIS LA MÊME TEINTE. Le large n'est pas une copie
// élargie de l'étroit : il change de couleur, et parfois de mélange. Les peindre
// pareil perdrait le dégradé du cœur vers le bord, qui est tout l'effet.

// DE COMBIEN CHAQUE COIN S'OUVRE de part et d'autre de l'inclinaison, en degrés.
// Le premier est étroit, le second large.
inline constexpr std::int32_t kConeWedgeNarrowDegrees = 2;
inline constexpr std::int32_t kConeWedgeWideDegrees = 5;
// ⚠ UNE SEULE VARIANTE LES RESSERRE, à un et deux degrés. Son éventail est donc
// deux fois plus fin que les autres.
inline constexpr std::int32_t kConeWedgeTightNarrowDegrees = 1;
inline constexpr std::int32_t kConeWedgeTightWideDegrees = 2;
inline constexpr float kConeWedgeTightVariant = 2.0F;

// LES CASES QUE CETTE FIGURE LIT, et leur sens lui appartient (ADR 0017).
inline constexpr std::size_t kSlotOfWedgeApexHeight = 0;
inline constexpr std::size_t kSlotOfWedgeVariant = 4;

// CE QU'UNE VARIANTE DONNE À SES DEUX COINS.
struct EffectConeWedgeStyle {
  std::array<std::uint8_t, 3> narrow_color = {255, 255, 255};
  std::array<std::uint8_t, 3> wide_color = {255, 255, 255};
  // Dans la numérotation du seizième paramètre de la voie A : zéro est l'ADDITION.
  std::int16_t narrow_blend = 0;
  std::int16_t wide_blend = 0;
  // ⚠ UNE SEULE VARIANTE APLATIT SA PROFONDEUR, et pour ses DEUX coins : ses
  // quads passent devant tout. Voir `EffectQuadDepth`.
  bool flattens_depth = false;
};

// Ce qu'une variante demande. Toute valeur hors des cinq relevées rend le cas par
// défaut du client, qui teinte vraiment — ce n'est pas une absence.
[[nodiscard]] EffectConeWedgeStyle EffectConeWedgeStyleFor(float variant);

// LES DEUX QUADS D'UNE PISTE : l'étroit puis le large, dans l'ordre du client.
//
// Rend un lot VIDE quand la piste est éteinte ou que son alpha est nul. ⚠ ET
// C'EST LE CAS AU PREMIER TIC : le bâtisseur sème l'alpha à zéro.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeConeWedgeQuads(const EffectTrack& track,
                                                                 glm::vec3 node_origin,
                                                                 const EffectOwnerTint& owner = {});

}  // namespace ro::render
