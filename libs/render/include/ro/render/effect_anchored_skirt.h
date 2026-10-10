#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA GERBE : une jupe d'anneau qui balaie sept huitièmes de tour, DÉRIVE au fil
// des tics, et dont l'image DÉFILE.
//
// La figure et le mouvement du sous-type 146. TROIS identifiants l'emploient,
// tous des jets d'eau.
//
// ⚠ C'EST LA JUPE D'ANNEAU DE `ComposeEffectTrackSkirt`, À TROIS CHOSES PRÈS :
//
//   - AUCUNE CLÉ DE SOULÈVEMENT. Le bord intérieur est à hauteur ZÉRO,
//     toujours ; la jupe, elle, compare sa clé à deux flottants exacts ;
//   - UN DÉCALAGE GLOBAL, pris dans la queue de la PISTE ZÉRO et non dans celle
//     qu'on dessine. Les quatre pistes partagent donc le même, et le bord
//     intérieur le reçoit aux TROIS QUARTS quand l'extérieur le reçoit entier ;
//   - UNE IMAGE QUI DÉFILE, choisie par le compteur de tics modulo le nombre
//     d'images du nœud.
//
// Les mettre en commun aurait demandé cinq drapeaux à une figure qui n'en a
// aucun ; les deux restent séparées, et ce commentaire dit pourquoi.

// SUR COMBIEN DE PAS le balayage se divise, et combien de bandes de texture il
// compte.
inline constexpr std::int32_t kAnchoredSkirtSteps = 20;
inline constexpr std::int32_t kAnchoredSkirtBands = kAnchoredSkirtSteps + 1;

// QUELLE PART DU DÉCALAGE GLOBAL le bord intérieur reçoit.
inline constexpr float kAnchoredSkirtInnerShare = 0.75F;

// LES CASES DE LA QUEUE qui portent le décalage global et la variante de teinte.
//
// ⚠ LA VARIANTE EST DANS LA PISTE et le dessinateur la recopie dans le NŒUD
// avant chaque quad. C'est le premier dessinateur du lot à écrire dans son
// nœud ; le lire comme une lecture pure perdrait ce passage.
inline constexpr std::size_t kAnchoredSkirtTailOfDriftX = 0;
inline constexpr std::size_t kAnchoredSkirtTailOfDriftZ = 2;
inline constexpr std::size_t kAnchoredSkirtTailOfVariant = 6;

// LE DÉCALAGE GLOBAL d'un nœud : celui de sa PREMIÈRE piste, quelle que soit
// celle qu'on dessine.
[[nodiscard]] glm::vec2 EffectAnchoredSkirtDrift(
    const std::array<EffectTrack, kEffectTracksPerNode>& tracks);

// LA JUPE D'UNE PISTE.
//
// `drift` est le décalage global, `texture_count` le nombre d'images du nœud.
//
// Rend un lot VIDE quand la piste est éteinte, quand son compteur n'est pas
// strictement positif, ou quand son balayage est trop court pour donner un pas.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeAnchoredSkirtQuads(const EffectTrack& track,
                                                                     glm::vec2 drift,
                                                                     std::size_t texture_count,
                                                                     glm::vec3 anchor);

// LE MOUVEMENT DU SOUS-TYPE 146 : la gerbe s'ouvre, dérive, s'efface, et
// RECOMMENCE plus bas.
//
// ⚠ ELLE NE MEURT PAS DU PREMIER EFFACEMENT. Quand son alpha passe sous zéro,
// le client rabaisse son inclinaison de vingt, raccourcit son rayon de quatre
// dixièmes, remet le compteur à zéro et la relance. Elle ne s'éteint qu'une
// fois l'inclinaison tombée à quarante. La laisser mourir au premier tour n'en
// montrerait qu'un sur trois.
void AdvanceAnchoredSkirtTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks);

}  // namespace ro::render
