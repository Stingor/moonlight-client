#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"
#include "ro/render/effect_wall_quad.h"

namespace ro::render {

// LA BOÎTE AU SOL : les quatre murs d'une boîte carrée dressée sous un
// personnage, et dont la hauteur RESPIRE.
//
// Le dessinateur et le cas 64 du répartiteur de mouvement. Quatre identifiants
// l'emploient, tous des marques de sol.
//
// ⚠ LA BOÎTE NE TOURNE PAS (ADR 0017). L'azimut dont le dessinateur fait tourner ses coins vaut
// ZÉRO chez les quatre bâtisseurs : la boîte reste alignée sur les axes. Ce qui tourne est un AUTRE
// angle, qui ne sert qu'à faire respirer la hauteur.
//
// ⚠ ELLE DRESSE LE MÊME MUR QUE LE CADRE du sous-type 90, et c'est pourquoi ce
// mur vit à part — voir `ComposeEffectWallQuad`. Ce qui la distingue tient en
// trois choses : ses coins tournent, sa teinte suit une variante, et une de
// ses variantes ajoute deux panneaux en diagonale.

// COMBIEN DE MURS une boîte porte, et combien de panneaux la variante pleine
// ajoute en diagonale.
inline constexpr std::size_t kGroundBoxWalls = 4;
inline constexpr std::size_t kGroundBoxDiagonals = 2;

// LES QUATRE VARIANTES QUE LE DESSINATEUR NOMME, rangées en flottant dans la
// piste comme le client les range.
inline constexpr float kGroundBoxPlainVariant = 0.0F;
inline constexpr float kGroundBoxPaleVariant = 1.0F;
inline constexpr float kGroundBoxFullVariant = 2.0F;
inline constexpr float kGroundBoxPulsingVariant = 4.0F;

// LES TEINTES, en dur dans le dessinateur. Celle de la variante qui pulse n'a
// que deux canaux fixes : son bleu se calcule.
inline constexpr std::array<std::uint8_t, 3> kGroundBoxPlain = {255, 255, 255};
inline constexpr std::array<std::uint8_t, 3> kGroundBoxPale = {235, 255, 235};
inline constexpr std::array<std::uint8_t, 3> kGroundBoxFull = {80, 80, 80};
inline constexpr std::array<std::uint8_t, 2> kGroundBoxPulsingRedGreen = {255, 255};

// LES DEUX MÉLANGES : la variante nue S'AJOUTE, les trois autres se peignent
// par l'alpha de la source.
//
// ⚠ LE MODE ZÉRO DE LA VOIE A EST L'ADDITION, et non l'absence de mélange :
// c'est `EffectParticleBlend` qui en décide, et il donne la destination
// ordinaire à tout autre mode. Nommer le zéro « ordinaire » inverserait la
// lecture de quiconque reprend ces constantes.
inline constexpr std::int16_t kGroundBoxAddedBlend = 0;
inline constexpr std::int16_t kGroundBoxOrdinaryBlend = 1;

// LES CASES QUE CETTE FIGURE LIT, et leur sens lui appartient (ADR 0017).
inline constexpr std::size_t kGroundBoxSlotOfHalfX = 0;
inline constexpr std::size_t kGroundBoxSlotOfHalfZ = 1;
inline constexpr std::size_t kGroundBoxSlotOfHeight = 2;
inline constexpr std::size_t kGroundBoxSlotOfVariant = 3;
// LES DEUX CASES DU BLEU QUI PULSE : un tirage fixe, et l'onde que le
// mouvement y ajoute.
inline constexpr std::size_t kGroundBoxSlotOfBlueFloor = 4;
inline constexpr std::size_t kGroundBoxSlotOfBlueWave = 5;
// ET CELLE QUI DIT DANS QUEL SENS L'ONDE VA : zéro en montant, un en
// descendant.
inline constexpr std::size_t kGroundBoxSlotOfBlueFalling = 6;

// LA TEINTE d'une piste, selon sa variante.
[[nodiscard]] std::array<std::uint8_t, 3> EffectGroundBoxColor(const EffectTrack& track);

// LE MÉLANGE d'une piste, selon sa variante.
[[nodiscard]] std::int16_t EffectGroundBoxBlendMode(float variant);

// LES MURS D'UNE BOÎTE : quatre, ou SIX pour la variante pleine.
//
// Rend un lot VIDE quand la piste est éteinte ou que son alpha est nul.
//
// ⚠ LA VARIANTE PLEINE AJOUTE DEUX PANNEAUX EN DIAGONALE, à l'alpha DOUBLÉ et
// borné à deux cent cinquante-cinq. C'est ce qui fait un mur de brume plein au
// lieu d'un cadre vide ; les omettre laisserait voir au travers.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeGroundBoxQuads(const EffectTrack& track,
                                                                 glm::vec3 node_origin,
                                                                 const EffectOwnerTint& owner = {});

// LE MOUVEMENT DU SOUS-TYPE 64 : une boîte dont la hauteur respire, dont
// l'alpha tombe passé sa durée, et dont un bleu pulse pour une variante.
//
// ⚠ IL NE SUIT PAS SON PORTEUR : le bâtisseur recopie la position une fois, au
// semis. C'est une marque POSÉE AU SOL, pas une aura.
//
// ⚠ ET LA HAUTEUR NE TOMBE JAMAIS À ZÉRO : le client prend soixante-cinq
// centièmes de la clé, plus trente-cinq fois le sinus de l'angle. La lire comme
// un simple sinus écraserait la boîte deux fois par tour.
void AdvanceGroundBoxTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render
