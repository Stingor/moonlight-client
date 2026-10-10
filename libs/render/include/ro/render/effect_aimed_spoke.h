#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LES DEUX RAYONS POINTÉS VERS LA CIBLE, sous-type 154 : la figure et le
// mouvement.
//
// Le mouvement et la figure sont propres à ce sous-type. Quatre effets le
// posent : 854, 858 et 863 sur `line3.tga`, et 1584 sur le trait de Metallic
// Sound ; chacun sous un octet qui choisit ses teintes.
//
// FIGURE — pour chaque piste allumée d'alpha positif, DEUX rayons couchés à six
// unités au-dessus du nœud, à vingt-cinq degrés de part et d'autre du cap vers
// la cible. Un rayon est un triangle : le centre du nœud, la pointe au bout de
// la longueur, et la pointe déportée de la largeur d'un quart de tour EN
// ARRIÈRE de son propre angle. Le client le passe comme un quad dont le premier
// et le dernier coin sont confondus au centre.
//
// Chaque rayon se peint DEUX FOIS : d'abord au quart de l'alpha, en mélange
// ordinaire (mode un), puis à l'alpha entier, en addition (mode zéro). L'octet
// de la piste choisit les deux teintes — voir `AimedSpokeColors`.
//
// MOUVEMENT — le cas 154 du répartiteur des nœuds. Pour chaque piste allumée :
// le compteur avance ; jusqu'au quinzième tic l'alpha monte de treize, plafonné
// à deux cent cinquante-cinq ; du seizième au vingtième il tient ; au-delà il
// descend de trois, et la piste MEURT quand il passerait sous zéro.

// LES CASES QUE LE BÂTISSEUR ET LA FIGURE PARTAGENT. Voir `effect_track.h`.
//
// La LONGUEUR d'un rayon, dans la première case de valeur.
inline constexpr std::size_t kAimedSpokeSlotOfLength = 0;
// Une case posée à quatre-vingts que ni le mouvement ni la figure ne relisent.
inline constexpr std::size_t kAimedSpokeSlotOfUnread = 2;
// L'OCTET DU SITE, qui choisit les teintes.
inline constexpr std::size_t kAimedSpokeMarkOfTint = 0;
// ⚠ LA LARGEUR vit dans `lift_key` et le CAP vers la cible dans
// `tilt_degrees` : ce sont les deux champs du client que la figure lit là.

// LE MOUVEMENT : treize de plus par tic jusqu'au quinzième tic, rien jusqu'au
// vingtième, puis trois de moins par tic.
inline constexpr std::int32_t kAimedSpokeRisesUntilTick = 15;
inline constexpr std::int16_t kAimedSpokeAlphaRise = 13;
inline constexpr std::int32_t kAimedSpokeFadesAfterTick = 20;
inline constexpr std::int16_t kAimedSpokeAlphaFall = 3;

// LA FIGURE : l'écart de chaque rayon au cap, la hauteur du plan où ils se
// couchent (repère DESCENDANT du client : six unités au-dessus du nœud), et le
// quart de tour dont la largeur se déporte en arrière.
inline constexpr std::int32_t kAimedSpokeSpreadDegrees = 25;
inline constexpr float kAimedSpokeClientDepthBelow = -6.0F;
inline constexpr std::int32_t kAimedSpokeWidthBackDegrees = 90;
// Le premier passage se peint au quart de l'alpha, en division ENTIÈRE.
inline constexpr std::int16_t kAimedSpokeDimPassDivisor = 4;
// Les deux modes de mélange : ordinaire pour le premier passage, addition pour
// le second.
inline constexpr std::int16_t kAimedSpokeDimPassBlend = 1;
inline constexpr std::int16_t kAimedSpokeFullPassBlend = 0;

// LES DEUX TEINTES qu'un octet choisit, rouge, vert, bleu : celle du passage
// pâle, puis celle du passage plein.
struct AimedSpokeColors {
  std::array<std::uint8_t, 3> dim_pass = {255, 255, 255};
  std::array<std::uint8_t, 3> full_pass = {255, 255, 255};
};
// Un : rose puis rouge. Deux : blanc puis blanc. Tout autre : blanc puis bleu.
[[nodiscard]] AimedSpokeColors AimedSpokeColorsOf(std::uint8_t tint_mark);

// LE MOUVEMENT DU SOUS-TYPE 154, d'un tic, sur les quatre pistes.
//
// ⚠ LES DEUX ANGLES DE CAMÉRA que le client range d'abord dans le nœud ne sont
// pas portés : la figure ne les lit pas.
void AdvanceAimedSpokeTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// LES QUADS D'UNE PISTE : quatre — deux rayons, deux passages chacun —, ou
// aucun quand la piste est éteinte ou que son alpha n'est pas positif.
//
// `node_origin` est l'origine du nœud dans NOTRE repère ; les coins sont
// relatifs à elle.
void ComposeAimedSpokeQuads(const EffectTrack& track, glm::vec3 node_origin,
                            std::vector<EffectWorldQuad>& quads);

}  // namespace ro::render
