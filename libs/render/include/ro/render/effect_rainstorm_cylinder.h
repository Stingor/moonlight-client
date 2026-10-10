#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE CYLINDRE DE NOTES DE SEVERE RAINSTORM : la figure et le mouvement du
// sous-type 173.
//
// Un seul dessinateur y mène, et un seul mouvement. Aucun autre sous-type ne
// les emploie.
//
// ⚠ LA FIGURE N'EST PAS NEUVE : C'EST LA JUPE QUI DÉFILE DU SOUS-TYPE 144
// (`ComposeCyclingSkirtQuads`), à trois écarts près :
//
//   - LES DEUX BORDS SONT ABAISSÉS DE CINQ. Le client pose l'ordonnée du bord
//     intérieur à 5,0 et celle du bord extérieur à
//     `5,0 − longueur·sin(inclinaison)`. Son repère descend : c'est une
//     translation de cinq unités VERS LE BAS de toute la figure ;
//   - IL NE LIT PAS LA CLÉ. La jupe la compare à deux flottants exacts pour se
//     soulever ; ce dessinateur n'en porte aucun. La clé de ce sous-type vaut
//     vingt, qui ne soulève rien, mais on la neutralise plutôt que de compter
//     là-dessus ;
//   - IL ABANDONNE UNE PISTE DONT LE COMPTEUR N'EST PAS STRICTEMENT POSITIF.
//     La jupe ne teste pas le compteur.
//
// Le reste est la jupe, constante pour constante : pas angulaire du balayage
// divisé par vingt, index borné à vingt, fermeture du tour entier, rayon,
// inclinaison, longueurs, alpha passé deux fois à la primitive, et l'image
// choisie par le compteur modulo le nombre d'images du nœud.
//
// ⚠ UN BALAYAGE QUI N'EST PAS MULTIPLE DE VINGT DONNERAIT AU CLIENT un ou deux
// points de plus, l'index bloqué au vingtième ; la jupe s'arrête à vingt et un.
// Le bâtisseur pose cent vingt et le mouvement n'y touche pas : l'écart ne se
// présente pas.

// DE COMBIEN LE CLIENT ABAISSE LES DEUX BORDS, dans son repère qui descend.
inline constexpr float kRainstormCylinderDropBelowNode = 5.0F;

// LES QUADS D'UNE PISTE, image comprise, ou un lot VIDE.
//
// `variant` est la variante de palette du nœud ; `texture_count` le nombre
// d'images du nœud ; `anchor` le point du nœud, que le mouvement recale sur le
// porteur à chaque tic.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeRainstormCylinderQuads(const EffectTrack& track,
                                                                         std::uint8_t variant,
                                                                         std::size_t texture_count,
                                                                         glm::vec3 anchor);

// LE MOUVEMENT DU SOUS-TYPE 173, pour un nœud qui vit `life_ticks` tics.
//
// Pour chaque piste allumée, dans l'ordre du client :
//
//   - le compteur avance ; s'il n'est pas strictement positif, la piste
//     s'arrête là ;
//   - l'azimut tourne d'un degré et se replie sous trois cent soixante — c'est
//     ce qui fait tourner le cylindre ;
//   - pendant les VINGT-NEUF premiers tics (compteur sous trente), l'alpha
//     monte de cinq et plafonne à cent cinquante — quatre-vingts quand le
//     joueur a réduit les effets ;
//   - strictement au-delà de `life_ticks − 46`, l'alpha perd quatre, plancher
//     à zéro ;
//   - enfin l'onde : chaque case vaut la clé, plus trois dixièmes de la clé fois
//     le sinus de neuf degrés par case fois le sinus d'une phase que le rang
//     choisit entre deux formules. C'est l'onde du corps commun des sous-types
//     30, 139 et 144, au même seuil de rang.
//
// ⚠ LES DEUX BRANCHES D'ALPHA NE S'EXCLUENT PAS : une vie de moins de
// soixante-seize tics les ferait jouer au même tic, montée puis chute.
//
// ⚠ `effects_reduced` EST LE RÉGLAGE « effets réduits », que nous ne modélisons
// pas ailleurs : l'appelant prend la branche pleine, celle du joueur qui n'a
// rien réduit.
void AdvanceRainstormCylinderTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                    std::int32_t life_ticks, bool effects_reduced = false);

// CE QUE LE MOUVEMENT POSE EN DUR.
inline constexpr std::int16_t kRainstormCylinderTurnPerTick = 1;
inline constexpr std::int32_t kRainstormCylinderRiseTicks = 30;
inline constexpr std::int16_t kRainstormCylinderAlphaRisePerTick = 5;
inline constexpr std::int16_t kRainstormCylinderAlphaCeiling = 150;
inline constexpr std::int16_t kRainstormCylinderReducedAlphaCeiling = 80;
inline constexpr std::int32_t kRainstormCylinderFadeTicks = 46;
inline constexpr std::int16_t kRainstormCylinderAlphaFallPerTick = 4;
inline constexpr float kRainstormCylinderUndulationAmplitude = 0.3F;

}  // namespace ro::render
