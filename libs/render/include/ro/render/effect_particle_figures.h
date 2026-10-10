#pragma once

#include <array>
#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LES FIGURES DE LA VOIE A qui se dessinent depuis une piste.
//
// Elles sortent par `effect_particle_quad.h` — quatre points du monde, une
// teinte, un mélange — et tirent leur géométrie de l'état qu'une piste
// entretient, comme les figures de ruban. Voir `effect_track.h`.

// CE QU'UNE MARQUE DÉCIDE, dans la particule montante.
//
// Le client n'en fait pas qu'une couleur : elle choisit aussi si le quad FAIT
// FACE à la caméra, et si sa profondeur est aplatie. Trois décisions sur un même
// octet, et aucune ne se déduit des autres.
struct EffectParticleAppearance {
  std::array<std::uint8_t, 3> color = {255, 255, 255};
  // La figure est-elle un billboard — un carré plat tourné vers l'œil — ou un
  // carré DEBOUT que son propre angle fait tourner sur place.
  bool faces_camera = false;
  // DEUX MARQUES PASSENT DEVANT TOUT : voir `EffectQuadDepth`.
  bool flattened_depth = false;
};

// La marque donne-t-elle une figure ? Le client a un cas par défaut qui ne
// dessine RIEN, et c'est une réponse : une marque hors table n'est pas une
// particule blanche, c'est une particule absente.
[[nodiscard]] bool EffectRisingParticleIsDrawn(std::uint8_t mark);

// Ce que la marque décide. Rend le neutre pour une marque hors table ; c'est à
// l'appelant d'avoir demandé `EffectRisingParticleIsDrawn` d'abord.
[[nodiscard]] EffectParticleAppearance EffectRisingParticleAppearance(std::uint8_t mark);

// LES DEUX ANGLES QUE LE MOUVEMENT TIRE DE LA CAMÉRA.
//
// Le mouvement du sous-type 43 les écrit dans les champs de style des deux
// premières pistes du nœud, et sa figure les relit de là pour tourner ses coins.
// Nous les calculons au moment de composer : les ranger dans une piste
// obligerait le mouvement à connaître la caméra, ce que `render` interdit de
// faire remonter jusqu'à `sim` — et deux pistes sur quatre porteraient un champ
// qui n'est pas à elles.
//
// ⚠ CE SONT LES ANGLES DE LA CAMÉRA DU CLIENT, dans son repère : trois cent
// soixante moins SON lacet, quatre cent cinquante moins SON altitude. Son lacet
// est l'opposé du nôtre et son altitude l'opposé de notre élévation, d'où
// `360 + yaw` et `450 + pitch`. Le carré qu'ils tournent est donc un carré DU
// CLIENT : il revient au nôtre par `S` entier (`EffectPointFromClient`), et
// fait alors face à notre œil sans miroir. Ne retourner que l'ordonnée le
// présenterait par la tranche.
[[nodiscard]] std::int16_t EffectBillboardTurnDegrees(const Camera& camera);
[[nodiscard]] std::int16_t EffectBillboardTiltDegrees(const Camera& camera);

// LA PARTICULE MONTANTE : un carré posé sur une sphère qui grandit.
//
// ⚠ CE N'EST UN BILLBOARD que pour trois marques sur dix (ADR 0017). La piste donne le centre — un
// point de la sphère de rayon `radius_at_anchor`, à la longitude que porte l'inclinaison et à la
// latitude que porte l'angle —, et le rayon croît à chaque tic : les quatre particules s'éloignent
// de l'ancre dans quatre directions tirées au sort.
//
// Autour de ce centre, un carré de demi-diagonale prise en deuxième case, ses
// quatre coins à quatre-vingt-dix degrés d'écart depuis l'azimut. Selon la
// marque, il est soit DEBOUT et tourné sur place par l'angle de la troisième
// case, soit COUCHÉ puis redressé vers l'œil par les deux angles donnés.
//
// Rend RIEN quand la piste est éteinte, quand son alpha est nul, ou quand sa
// marque n'a pas de cas — le client ne dessine alors pas, et poser un quad
// blanc à la place montrerait ce qu'il cache.
//
// ⚠ L'ANCRE EST DANS NOTRE REPÈRE, le reste dans celui du client. Le centre et
// les coins se calculent en ordonnée DESCENDANTE, comme le client, puis se
// retournent avant de s'ajouter à l'ancre. Retourner l'ancre aussi renverrait la
// figure sous le sol.
[[nodiscard]] std::optional<EffectWorldQuad> ComposeRisingParticleQuad(
    const EffectTrack& track, std::int16_t billboard_turn_degrees,
    std::int16_t billboard_tilt_degrees);

}  // namespace ro::render
