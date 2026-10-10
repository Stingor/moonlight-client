#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"
#include "ro/render/effect_wall_quad.h"

namespace ro::render {

// LE CADRE ET LA BARRE : les deux figures du sous-type 90, dans UN SEUL nœud.
//
// Porte le dessinateur du cadre et de la barre et le mouvement du cas 90.
// Cinq identifiants l'emploient, et chacun pose DEUX nœuds.
//
// ⚠ UN NŒUD PORTE DEUX FIGURES, et c'est la première du lot dans ce cas. Ses
// deux PREMIÈRES pistes dressent un cadre — les quatre murs d'une boîte carrée
// — et ses deux DERNIÈRES font courir une barre sur le périmètre d'un carré à
// peine plus large. Le client les dessine en deux boucles séparées, avec deux
// jeux de teintes ; les traiter pareil peindrait quatre cadres ou quatre
// barres.
//
// ⚠ ET LA CASE QUE NOTRE PISTE APPELLE « STYLE » PORTE ICI UNE DURÉE, en tics,
// pendant que le style vit dans la cinquième valeur (ADR 0017). Deux mille
// quatre cents tics font plus de cent secondes : ce sont des marques de sol qui
// durent, pas des coups.

// LA CASE QUI PORTE LA DURÉE, et celle qui porte le style.
inline constexpr std::size_t kFrameAndBarSlotOfStyle = 4;
// LES CASES DE LA GÉOMÉTRIE : deux demi-côtés, une hauteur, une longueur.
inline constexpr std::size_t kFrameAndBarSlotOfHalfX = 0;
inline constexpr std::size_t kFrameAndBarSlotOfHalfZ = 1;
inline constexpr std::size_t kFrameAndBarSlotOfHeight = 2;
inline constexpr std::size_t kFrameAndBarSlotOfBarLength = 3;

// COMBIEN DE PISTES DRESSENT LE CADRE : les deux premières. Les deux autres
// font courir la barre.
inline constexpr std::size_t kFrameAndBarFrameTracks = 2;
// COMBIEN DE MURS un cadre porte, et combien de côtés son périmètre compte.
inline constexpr std::size_t kFrameAndBarWalls = 4;

// L'OCTET À PARTIR DUQUEL LA FAMILLE VIRE AU BLEU. En deçà elle est verte.
inline constexpr float kFrameAndBarBlueStyle = 10.0F;

// LES QUATRE TEINTES, en dur dans le dessinateur : le cadre est plus pâle que
// sa barre, et les deux virent ensemble.
inline constexpr std::array<std::uint8_t, 3> kFrameGreen = {200, 250, 200};
inline constexpr std::array<std::uint8_t, 3> kFrameBlue = {100, 100, 250};
inline constexpr std::array<std::uint8_t, 3> kBarGreen = {150, 250, 150};
inline constexpr std::array<std::uint8_t, 3> kBarBlue = {50, 50, 250};

// LES QUATRE MURS D'UN CADRE, dans l'ordre où le client les pose : le côté des
// x positifs, celui des z négatifs, celui des x négatifs, celui des z positifs.
//
// Rend un lot VIDE quand la piste est éteinte ou que son alpha est nul.
//
// ⚠ LES COINS SONT RELATIFS à l'origine du nœud : le client ne pousse pas son
// drapeau d'absolu. Les traiter comme absolus poserait tous les cadres à
// l'origine du monde.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeFrameWallQuads(const EffectTrack& track,
                                                                 glm::vec3 node_origin,
                                                                 const EffectOwnerTint& owner = {});

// LA BARRE QUI COURT SUR LE PÉRIMÈTRE : un mur dressé sur le segment qui va de
// l'abscisse curviligne de la piste à cette abscisse plus la longueur.
//
// Rend UN quad, ou DEUX quand la barre passe un coin — et VIDE quand la piste
// est éteinte ou muette.
//
// ⚠ LE CLIENT NE REPLIE QU'UNE FOIS. Il coupe le premier quad au coin et en
// dessine un second sur le côté suivant, mais il ne coupe PAS celui-là : une
// barre plus longue qu'un côté dépasse le coin suivant en ligne droite : avec
// l'octet dix, la barre fait 18,6 pour des côtés de 15,6.
// Ajouter une troisième coupure corrigerait le client au lieu de le porter.
[[nodiscard]] std::vector<EffectWorldQuad> ComposePerimeterBarQuads(
    const EffectTrack& track, glm::vec3 node_origin, const EffectOwnerTint& owner = {});

// LE POINT D'UN CÔTÉ du carré de demi-côté `half_side`, à une abscisse
// curviligne comptée depuis le DÉPART DU TOUR — et non depuis le côté.
//
// Le tour part du coin (+x, +z), descend le côté des x positifs, file vers les
// x négatifs, remonte, et revient. L'abscisse n'est PAS ramenée dans le côté
// demandé : c'est ainsi que le second morceau d'une barre trop longue dépasse
// son coin en ligne droite, comme le client le fait.
[[nodiscard]] glm::vec2 EffectPerimeterPointOnSide(float half_side, std::size_t side,
                                                   float travelled);

// SUR QUEL CÔTÉ tombe une abscisse curviligne, de zéro à trois.
[[nodiscard]] std::size_t EffectPerimeterSideOf(float half_side, float travelled);

// LE MOUVEMENT DU SOUS-TYPE 90 : une marque de sol qui s'élève, tient, et
// retombe — et dont la barre tourne à un demi par tic.
//
// ⚠ IL SUIT SON PORTEUR, et c'est ce qui permet à la marque de rester sous
// celui qui la tient.
//
// ⚠ ET LE PORTEUR QUI BOUGE LA COUPE, mais seulement pour les styles vingt et
// au-delà, seulement après le deux-centième tic, et seulement si la durée
// restante dépasse deux mille. Le client ramène alors la durée à l'âge courant,
// ce qui déclenche la retombée. C'est ainsi qu'une marque de chant s'arrête
// quand le barde s'en va ; l'appliquer à tous les styles couperait aussi les
// marques que le client laisse en place.
//
// `node_position` est l'endroit où le nœud se trouve AVANT ce tic, et
// `owner_position` celui où son porteur est maintenant. Les deux se comparent
// avant que le nœud ne rejoigne son porteur.
void AdvanceFrameAndBarTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              glm::vec3 node_position, glm::vec3 owner_position);

}  // namespace ro::render
