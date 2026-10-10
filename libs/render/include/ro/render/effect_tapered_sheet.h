#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LES FEUILLES QUI S'ÉTIRENT, sous-type 141 : la figure et le mouvement.
//
// Le mouvement et la figure sont propres à ce sous-type, et un seul effet le
// pose : le 811, quatre feuilles bleues empilées au-dessus de son porteur, qui
// s'étirent l'une après l'autre à dix tics d'écart puis pâlissent.
//
// FIGURE — pour chaque piste allumée d'alpha positif, un rectangle de largeur
// le rayon de la piste, couché le long de son cap : son bord extérieur au rayon,
// son bord intérieur à `rayon − étirement · rayon / 50`, si bien qu'il part sans
// aire, atteint le centre à cinquante et le dépasse jusqu'à l'autre rayon à
// cent. Redressé vers la caméra par les deux angles de panneau, posé à l'ancre
// de la piste (coins ABSOLUS), peint en addition, bleu (100, 100, 255).
//
// MOUVEMENT — le client range les deux angles de panneau, recopie la place de
// l'instance dans le nœud, puis, pour chaque piste allumée : le compteur avance
// et, une fois positif, l'étirement gagne cinq ; passé cent, il y est ramené et
// l'alpha perd deux ; l'alpha est borné à [0, 255] ; l'ancre reprend l'abscisse
// et la profondeur du nœud, et sa hauteur de naissance.
// ⚠ UNE PISTE À ZÉRO NE MEURT PAS : elle reste allumée jusqu'à la fin du nœud.

// LES CASES QUE LE BÂTISSEUR, LE MOUVEMENT ET LA FIGURE PARTAGENT. Voir
// `effect_track.h`.
//
// La HAUTEUR DE L'ANCRE, dans NOTRE repère, que le mouvement recopie à chaque
// tic dans l'ancre : la place de naissance de l'instance, relevée d'un étage.
inline constexpr std::size_t kTaperedSheetSlotOfHeight = 4;
// Une case posée à zéro que ni le mouvement ni la figure ne relisent.
inline constexpr std::size_t kTaperedSheetSlotOfUnread = 8;
// L'IMAGE de la liste du nœud que la figure peint, la teinte que personne ne lit
// et l'octet du site, que personne ne lit non plus.
inline constexpr std::size_t kTaperedSheetMarkOfImage = 0;
inline constexpr std::size_t kTaperedSheetMarkOfUnreadTint = 2;
inline constexpr std::size_t kTaperedSheetMarkOfSiteOctet = 4;
// ⚠ L'ÉTIREMENT vit dans `azimuth_degrees` et le CAP dans `degrees` : ce sont
// les deux champs du client que le mouvement et la figure lisent là.

// LE MOUVEMENT : l'étirement gagne cinq par tic jusqu'à cent, puis l'alpha perd
// deux par tic.
inline constexpr std::int16_t kTaperedSheetStretchStep = 5;
inline constexpr std::int16_t kTaperedSheetStretchCeiling = 100;
inline constexpr std::int16_t kTaperedSheetAlphaFall = 2;

// LA FIGURE : l'étirement qui fait parcourir un rayon au bord intérieur, la
// demi-largeur en part du rayon, et la teinte.
inline constexpr float kTaperedSheetStretchPerRadius = 50.0F;
inline constexpr float kTaperedSheetHalfWidthShare = 0.5F;
inline constexpr std::array<std::uint8_t, 3> kTaperedSheetColor = {100, 100, 255};

// LE MOUVEMENT DU SOUS-TYPE 141, d'un tic. `node_position` est la place du
// nœud, déjà recopiée de son porteur.
void AdvanceTaperedSheetTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               glm::vec3 node_position, std::int16_t billboard_turn_degrees,
                               std::int16_t billboard_tilt_degrees);

// LE QUAD D'UNE PISTE, ou rien quand elle est éteinte ou que son alpha n'est pas
// positif. `turn_degrees` et `tilt_degrees` sont les styles des pistes zéro et
// un, où le mouvement les a rangés.
[[nodiscard]] std::optional<EffectWorldQuad> ComposeTaperedSheetQuad(const EffectTrack& track,
                                                                     std::int16_t turn_degrees,
                                                                     std::int16_t tilt_degrees);

}  // namespace ro::render
