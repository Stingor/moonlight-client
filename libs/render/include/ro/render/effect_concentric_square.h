#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE CARRÉ CONCENTRIQUE : la figure du sous-type 32.
//
// La figure tient en peu de choses : le MÊME CARRÉ, deux à quatre fois, à des
// échelles différentes.
//
// ⚠ LES QUATRE COINS SONT À ZÉRO, QUATRE-VINGT-DIX, CENT QUATRE-VINGTS ET DEUX
// CENT SOIXANTE-DIX DEGRÉS, lus dans les tables de sinus et de cosinus du
// client : un CARRÉ sur un cercle, et non quatre points quelconques.

// LES ÉCHELLES DES QUATRE COUCHES POSSIBLES, de la plus large à la plus étroite.
inline constexpr float kConcentricSquareWidest = 3.0F;
inline constexpr float kConcentricSquareWide = 1.7F;
inline constexpr float kConcentricSquareMedium = 1.3F;
inline constexpr float kConcentricSquareFull = 1.0F;
inline constexpr float kConcentricSquareNarrow = 0.5F;

// LES DEUX MARQUES QUI OUVRENT DES COUCHES DE PLUS.
inline constexpr std::uint8_t kConcentricSquareMarkWithTwoHalos = 21;
inline constexpr std::uint8_t kConcentricSquareMarkWithOneHalo = 20;

// DE COMBIEN LE RAYON RESPIRE : un vingtième, au rythme d'une case de la piste.
inline constexpr float kConcentricSquareBreath = 0.05F;

// CE QU'UN CARRÉ CONCENTRIQUE DEMANDE POUR SE DESSINER.
struct EffectConcentricSquareRequest {
  // Le rayon du cercle sur lequel les quatre coins se posent, avant échelle.
  float radius = 0.0F;
  // ⚠ L'ANGLE DU SOUFFLE, en degrés, pris dans une CASE de la piste — la
  // huitième. Ce n'est pas un compteur de tics : le mouvement l'entretient.
  float breath_degrees = 0.0F;
  std::int16_t alpha = 0;
  // ⚠ LES DEUX ANGLES DE ROTATION, pris dans les champs de STYLE des pistes zéro
  // et UN. Ce ne sont pas des numéros de style (ADR 0017), et le mouvement les
  // calcule depuis la CAMÉRA — c'est ce qui fait de cette figure un billboard.
  std::int16_t turn_degrees = 0;
  std::int16_t tilt_degrees = 0;
  // La marque de la piste, qui décide du nombre de couches et de leurs teintes.
  std::uint8_t mark = 0;
  // ⚠ VRAI QUAND LE DRAPEAU DE MÉLANGE DU NŒUD VAUT UN, ce qui RETIRE le mélange au lieu
  // de l'ajouter : la couche pleine passe alors zéro au lieu de dix. Le nom dit
  // l'effet et non la valeur du champ, parce que lire « le champ vaut un » ne
  // dit pas dans quel sens il agit.
  //
  // Il ne change rien hors des marques de dix à treize : les autres passent
  // zéro sans même regarder le champ.
  bool plain_blend = false;
  // OÙ LE CARRÉ SE CENTRE, en unités du monde et dans LE REPÈRE DU CLIENT —
  // celui dont l'ordonnée descend. C'est le mouvement qui l'y écrit, et la
  // figure le retourne au nôtre en même temps que ses coins.
  glm::vec3 center = {0.0F, 0.0F, 0.0F};
};

// LES DEUX À QUATRE COUCHES, de la plus large à la plus étroite.
//
// ⚠ LA PREMIÈRE N'A QUE LA MOITIÉ DE L'ALPHA, et elle seule : la plus large est
// aussi la plus pâle. Lui donner l'alpha entier noierait les trois autres.
//
// Rend une liste VIDE quand l'alpha est nul ou le rayon nul — un carré sans aire
// ne se distingue pas d'une absence.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeConcentricSquareQuads(
    const EffectConcentricSquareRequest& request, glm::vec3 node_origin);

// CE QU'UN NŒUD DEMANDE, lu dans ses deux premières pistes.
//
// ⚠ DEUX PISTES, PAS UNE. La figure lit tout dans la piste zéro, sauf le second
// angle de rotation, qu'elle prend dans le champ de style de la piste UN.
[[nodiscard]] EffectConcentricSquareRequest EffectConcentricSquareRequestOf(
    const EffectTrack& first, const EffectTrack& second, bool plain_blend);

}  // namespace ro::render
