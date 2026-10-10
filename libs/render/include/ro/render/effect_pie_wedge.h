#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE QUARTIER DE TARTE : un triangle de dix degrés, pointe au centre.
//
// Le dessinateur du sous-type 162. Il sort par la voie A, une fois par piste.
//
// ⚠ C'EST LA MÊME GÉOMÉTRIE QUE LE COIN DE CÔNE, au champ près : deux points sur
// un cercle et l'apex répété, la voie A ne sachant émettre que des quads. Trois
// choses l'en séparent, et aucune n'est une nuance — son ouverture est FIXE à
// cinq degrés de part et d'autre, sa teinte est le bleu pur pour toutes les
// pistes, et il ne pose qu'UN triangle par piste au lieu de deux emboîtés.
//
// ⚠ ET SON REBOUCLAGE EST CELUI DU CLIENT, une borne par sens : il ajoute
// trois cent cinquante-cinq au lieu de retrancher cinq quand l'angle est sous
// cinq, et retranche trois cent cinquante-cinq au lieu d'ajouter cinq quand il
// est au-dessus de trois cent cinquante-quatre. C'est le même angle, et le dire
// autrement ferait chercher un cas particulier qui n'existe pas.

// DE COMBIEN LE QUARTIER S'OUVRE de part et d'autre de son inclinaison. Douze
// quartiers de dix degrés couvrent le tour complet, et c'est exactement ce que
// son bâtisseur sème.
inline constexpr std::int32_t kPieWedgeHalfOpeningDegrees = 5;

// LA CASE QUE CETTE FIGURE LIT, et son sens lui appartient (ADR 0017) : la
// hauteur de la pointe, que les deux coins du bord partagent.
inline constexpr std::size_t kSlotOfPieWedgeApexHeight = 0;

// LA TEINTE, la même pour toutes les pistes : le client la passe en dur.
inline constexpr std::uint8_t kPieWedgeBlue = 255;

// LE QUAD D'UNE PISTE, ou un lot VIDE quand elle est éteinte ou son alpha nul.
[[nodiscard]] std::vector<EffectWorldQuad> ComposePieWedgeQuads(const EffectTrack& track,
                                                                glm::vec3 node_origin,
                                                                const EffectOwnerTint& owner = {});

}  // namespace ro::render
