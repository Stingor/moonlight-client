#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// L'ÉTINCELLE QUI CONVERGE : quatre étincelles naissent sur une sphère autour
// du lanceur, foncent droit vers lui, et tournoient en arrivant.
//
// Porte son dessinateur et son mouvement (cas 159). Quatre identifiants
// l'emploient — EF_PRIMECHARGE et ses trois suites.
//
// ⚠ LES DEUX LAMES NE SE CROISENT PAS (ADR 0017). Elles sont centrées sur le
// MÊME angle : l'une fine, l'autre
// large, de deux teintes différentes. Les croiser en ferait une étoile.
//
// ⚠ ET LA MARQUE EST UNE MACHINE À ÉTATS, pas une couleur. Sa première case
// porte la PHASE du mouvement, sa deuxième un compteur, et sa troisième seule
// est la teinte. Les confondre ferait changer un effet de couleur en vol.

// LES DEUX LAMES, et leur demi-largeur en degrés.
inline constexpr std::size_t kHomingSparkBlades = 2;
inline constexpr std::array<std::int32_t, kHomingSparkBlades> kHomingSparkBladeHalfWidths = {2, 8};

// COMBIEN DE TEINTES LE DESSINATEUR NOMME. Au-delà, il ne dessine RIEN — son
// aiguillage n'a pas de cas par défaut, et retomber sur une teinte
// inventerait une figure.
inline constexpr std::uint8_t kHomingSparkMarks = 4;

// LES HUIT TEINTES, en dur dans le dessinateur : une par lame et par marque.
inline constexpr std::array<std::array<std::uint8_t, 3>, kHomingSparkMarks> kHomingSparkThinColors =
    {{{0, 250, 250}, {250, 250, 250}, {250, 100, 100}, {200, 200, 255}}};
inline constexpr std::array<std::array<std::uint8_t, 3>, kHomingSparkMarks> kHomingSparkWideColors =
    {{{250, 0, 0}, {250, 250, 0}, {255, 166, 210}, {5, 5, 255}}};

// LES CASES QUE CETTE FAMILLE LIT, et leur sens lui appartient (ADR 0017).
//
// La première valeur est un décalage vertical, la deuxième le PAS d'approche.
inline constexpr std::size_t kHomingSparkSlotOfLift = 0;
inline constexpr std::size_t kHomingSparkSlotOfStep = 1;
// La première marque porte la phase, la deuxième son compteur, la troisième
// la teinte.
inline constexpr std::size_t kHomingSparkMarkOfPhase = 0;
inline constexpr std::size_t kHomingSparkMarkOfSpinCount = 1;
inline constexpr std::size_t kHomingSparkMarkOfColor = 2;

// LES TROIS PHASES, dans l'ordre où la piste les traverse.
inline constexpr std::uint8_t kHomingSparkApproachPhase = 0;
inline constexpr std::uint8_t kHomingSparkSpinPhase = 1;
inline constexpr std::uint8_t kHomingSparkDriftPhase = 2;

// LES DEUX LAMES D'UNE PISTE, ou RIEN quand la piste est éteinte, muette, ou
// que sa marque de teinte dépasse les quatre que le client nomme.
//
// ⚠ LES COINS SONT RELATIFS à l'origine du nœud, et le décalage vertical de
// la première valeur s'ajoute À CHAQUE COIN, en plus de celui que l'ancre
// porte déjà. Le client l'écrit deux fois ; ne l'écrire qu'une abaisserait
// chaque lame de cinq unités.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeHomingSparkQuads(
    const EffectTrack& track, glm::vec3 node_origin, const EffectOwnerTint& owner = {});

// LE MOUVEMENT DU SOUS-TYPE 159 : une approche, un tournoiement, une dérive.
//
// ⚠ IL SUIT SON PORTEUR : le client recopie la position du propriétaire dans
// le nœud avant d'avancer les pistes, et c'est ainsi que la charge reste
// autour de celui qui la lève.
//
// ⚠ ET L'ANCRE AVANCE À TOUTES LES PHASES. Même une fois arrivée, l'étincelle
// continue de reculer d'un pas le long de ses deux angles — mais ces angles
// tournent, si bien qu'elle décrit une spirale au lieu de traverser.
void AdvanceHomingSparkTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render
