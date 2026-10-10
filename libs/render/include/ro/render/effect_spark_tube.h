#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE TUBE D'ÉTINCELLE : la figure du sous-type 100.
//
// ⚠ CE N'EST PAS UNE ÉTOILE (ADR 0017) : ce ne sont pas des rayons partant d'un
// centre, c'est
// UN TUBE À TROIS FACES balayé le long d'une COURBE, et la courbe est
// l'HISTORIQUE d'un angle qui monte. Le chercher comme une étoile n'en
// trouverait rien.
//
// Les cinq identifiants qui l'emploient sont des étincelles de coup —
// EF_HITLINE3 à 6 et EF_STORMKICK.

// COMBIEN DE FACES LE TUBE A, et de combien ses arêtes s'écartent.
//
// ⚠ TROIS FACES, ET QUATRE PAS. Le client balaie zéro, cent vingt, deux cent
// quarante et TROIS CENT SOIXANTE — le dernier retombe sur le premier et
// referme le tube. Trois pas laisseraient une fente sur toute la longueur.
inline constexpr std::int32_t kSparkTubeEdgeStepDegrees = 120;
inline constexpr std::size_t kSparkTubeFaces = 3;

// LE QUART DE TOUR qui met la section du tube EN TRAVERS de la courbe.
inline constexpr std::int32_t kSparkTubeCrossQuarterTurn = 90;

// DE COMBIEN LA SECTION DESCEND sous la courbe, en unités du monde.
//
// ⚠ CE N'EST PAS UN RAYON MAIS UN DÉCALAGE : le client retranche cinq à la
// hauteur de chaque sommet de la section, si bien que le tube ne s'enroule pas
// autour de la courbe mais pend sous elle. Notre repère monte, donc nous
// ajoutons.
inline constexpr float kSparkTubeSectionDrop = 5.0F;

// LE PLEIN ANGLE de la courbe : au-delà, elle ne monte plus.
inline constexpr float kSparkTubeFullAngleDegrees = 90.0F;

// DE COMBIEN L'ALPHA TOMBE d'un point de la courbe au suivant.
//
// ⚠ C'EST CE QUI FAIT LA TRAÎNÉE : la tête est pleine, la queue s'efface. Les
// peindre au même alpha donnerait un tube uniforme, et l'étincelle n'aurait
// plus de sens de marche.
inline constexpr std::int16_t kSparkTubeAlphaFadePerPoint = 10;

// LE STYLE QUI DOUBLE LE TUBE, et de combien.
//
// ⚠ UN SEUL STYLE PEINT UN SECOND TUBE, BLEU, QUATRE FOIS PLUS LARGE, à la
// MOITIÉ de l'alpha. Le client entre dans la boucle pour tous les styles à
// partir de trois, mais n'émet que pour le trois exactement — les styles
// au-delà calculent et ne montrent rien.
inline constexpr std::int16_t kSparkTubeHaloStyle = 3;
inline constexpr float kSparkTubeHaloWidening = 4.0F;
inline constexpr std::int16_t kSparkTubeHaloAlphaDivisor = 2;

// LA TEINTE D'UN POINT DE LA COURBE, selon le style du nœud et le rang du
// point.
//
// ⚠ LA TEINTE CHANGE LE LONG DE LA COURBE, et c'est la seconde moitié de la
// traînée : la tête et la queue n'ont pas la même couleur. Les quatre styles
// la font varier sur un canal différent.
[[nodiscard]] std::array<std::uint8_t, 3> EffectSparkTubeColor(std::int16_t style,
                                                               std::size_t point);

// LES QUADS D'UNE PISTE.
//
// ⚠ SES COINS SONT ABSOLUS. Le client passe le drapeau d'absolu à UN : les
// sommets sont déjà dans le monde, parce que l'ANCRE de la piste l'est — le
// mouvement y recopie la position du porteur à chaque tic. Y ajouter l'origine
// du nœud doublerait la position.
//
// Rend une liste VIDE quand la piste est éteinte, son alpha nul, ou son
// compteur encore négatif — c'est le retard que le bâtisseur a tiré.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeSparkTubeQuads(const EffectTrack& track);

}  // namespace ro::render
