#pragma once

#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_fwd.h"
#include "ro/render/effect_node_world.h"

namespace ro::render {

// LA SPHÈRE UV, sous-type 14 : le dôme de feu de Magnum Break.
//
// Le client parcourt la
// latitude de moins quatre-vingt-dix jusqu'à `90 − pas`, et la longitude de
// zéro jusqu'à `360 − pas`, au pas angulaire du nœud — trente-six
// par défaut : cinq rangs de dix colonnes, cinquante quads qui couvrent la
// sphère ENTIÈRE, pôle à pôle. Chaque coin est `RotY(longitude + lacet) ·
// RotX(latitude) · (0, 0, rayon)`, le rayon étant celui de la couronne du nœud
// — que son mouvement fait croître.
//
// ⚠ C'EST UNE SPHÈRE PLEINE ET NON UN DÔME : posée aux pieds du porteur, sa
// moitié basse est sous le terrain, que la profondeur mange. Ce qui reste est
// le dôme que l'on voit. La bâtir en demi-sphère ferait un bord net là où le
// client en a un que le sol découpe.
//
// ⚠ ET CE N'EST PAS LA COQUILLE DU SOUS-TYPE 61 (`effect_sphere_shell.h`) :
// celle-là est quinze panneaux TANGENTS ouverts derrière, une figure de
// pistes ; celle-ci est une surface fermée bâtie des seuls champs du nœud.
//
// L'image se déroule une fois autour : l'abscisse de texture suit la
// longitude, l'ordonnée la latitude, du pôle bas (zéro) au pôle haut (un).
//
// Le lacet tourne la sphère sur son axe vertical, et le mouvement du client
// l'avance d'un pas par tic.

// Les deux étendues que le pas divise : un demi-tour de latitude, un tour de
// longitude.
inline constexpr float kUvSphereLatitudeSpanDegrees = 180.0F;
inline constexpr float kUvSphereLongitudeSpanDegrees = 360.0F;
inline constexpr float kUvSphereFirstLatitudeDegrees = -90.0F;

// LA SPHÈRE, POSÉE. Vide quand le pas est nul ou négatif — le client, lui,
// bouclerait sans fin. Un rayon nul rend ses quads SANS AIRE, comme chez lui :
// le dôme naît d'un point et s'étend.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeEffectNodeUvSphere(const EffectNode& node,
                                                                     glm::vec3 anchor,
                                                                     float yaw_degrees);

}  // namespace ro::render
