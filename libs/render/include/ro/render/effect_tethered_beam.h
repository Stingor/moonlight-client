#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE TRAIT TENDU ENTRE DEUX ACTEURS : un tube à dix côtés qui pousse du porteur
// vers sa cible, peint deux fois — une âme, puis une gaine au double du rayon.
//
// La figure sort par la voie A, et le mouvement est le cas 38 du répartiteur.
// CINQ identifiants l'emploient.
//
// ⚠ LA FIGURE NE BALAIE AUCUN ARC (ADR 0017). C'est le
// même prisme que le trait gainé du sous-type 69, à dix faces au lieu de cinq.
//
// ⚠ ET LE « MUR DE L'ACTEUR » N'EN EST PAS UN ICI. Le client va chercher
// l'acteur visé pour en lire TROIS FLOTTANTS — sa position — et rien d'autre.
// C'est la forme que `app` donne déjà à l'essaim et à la colonne : `render` ne
// connaît toujours aucun acteur, il reçoit un point.

// COMBIEN DE FACES le tube porte, de combien de degrés on avance d'une à la
// suivante, et combien de fois il se peint.
inline constexpr std::size_t kTetheredBeamSides = 10;
inline constexpr std::int32_t kTetheredBeamStepDegrees = 36;
inline constexpr std::size_t kTetheredBeamPasses = 2;

// EN COMBIEN DE TICS LE TRAIT POUSSE, et de combien de degrés de sinus par tic.
//
// ⚠ IL NE POUSSE PAS LINÉAIREMENT : le bout lointain se place au sinus de six
// fois le nombre de tics, ce qui le fait partir vite et arriver doucement.
inline constexpr std::int32_t kTetheredBeamGrowthTicks = 15;
inline constexpr std::int32_t kTetheredBeamGrowthStepDegrees = 6;

// DE COMBIEN CHAQUE BOUT S'ÉLÈVE AU-DESSUS DE SON ACTEUR.
inline constexpr float kTetheredBeamLift = 8.0F;

// LES QUATRE VARIANTES QUE LE DESSINATEUR NOMME, rangées en flottant dans la
// piste comme le client les range.
inline constexpr float kTetheredBeamYellowVariant = 0.0F;
inline constexpr float kTetheredBeamTealVariant = 2.0F;
inline constexpr float kTetheredBeamPinkVariant = 3.0F;
inline constexpr float kTetheredBeamGreyVariant = 4.0F;

// LES CASES QUE CETTE FIGURE LIT, et leur sens lui appartient (ADR 0017).
//
// Les deux bouts vivent dans la QUEUE de la piste : le proche dans ses trois
// premières cases, le lointain dans ses trois dernières.
inline constexpr std::size_t kTetheredBeamTailOfNearEnd = 0;
inline constexpr std::size_t kTetheredBeamTailOfFarEnd = 6;
inline constexpr std::size_t kTetheredBeamSlotOfVariant = 4;

// LES DEUX MÉLANGES : le mode ZÉRO de la voie A est l'ADDITION, et la variante
// grise est la seule à prendre le mélange ordinaire.
inline constexpr std::int16_t kTetheredBeamAddedBlend = 0;
inline constexpr std::int16_t kTetheredBeamOrdinaryBlend = 1;

// CE QU'UNE VARIANTE PEINT, à une passe donnée — zéro pour l'âme, un pour la
// gaine. Rend RIEN pour une variante que le client ne nomme pas : son
// aiguillage retombe sur le jaune et le bleu, et c'est la variante zéro.
[[nodiscard]] std::array<std::uint8_t, 3> EffectTetheredBeamColor(float variant, std::size_t pass);

// LE MÉLANGE d'une variante : ordinaire pour la grise, additif pour les autres.
[[nodiscard]] std::int16_t EffectTetheredBeamBlendMode(float variant);

// L'ALPHA d'une passe.
//
// ⚠ LA GAINE SE PEINT À L'ALPHA DOUBLÉ, et le client le TRONQUE à huit bits au
// lieu de le plafonner : au-delà de cent vingt-sept, il reboucle. Aucun des
// cinq semis n'y arrive, mais plafonner serait inventer une règle.
//
// ⚠ ET LA VARIANTE GRISE PREND LA MOITIÉ, aux deux passes.
[[nodiscard]] std::uint8_t EffectTetheredBeamAlpha(std::int16_t alpha, float variant,
                                                   std::size_t pass);

// OÙ LE BOUT LOINTAIN SE TROUVE à un tic donné : sur la cible passé le
// quinzième, entre les deux avant.
[[nodiscard]] glm::vec3 EffectTetheredBeamFarEnd(const EffectTrack& track);

// LE TUBE D'UNE PISTE : dix faces, deux fois.
//
// Rend un lot VIDE quand la piste est éteinte ou que son alpha n'est pas
// strictement positif.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeTetheredBeamQuads(
    const EffectTrack& track, const EffectOwnerTint& owner = {});

// LE MOUVEMENT DU SOUS-TYPE 38 : les deux bouts suivent deux acteurs.
//
// `owner_position` est la position du porteur à ce tic ; `target_position`
// celle de l'acteur visé, ABSENTE quand il n'y en a pas.
//
// ⚠ PAS DE CIBLE, PAS DE TRAIT : le client éteint sa piste et annule son alpha
// dès que sa recherche échoue. Il a deux façons d'échouer — un identifiant nul
// et un acteur introuvable —, et elles se confondent ici en une seule.
void AdvanceTetheredBeamTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               glm::vec3 owner_position,
                               const std::optional<glm::vec3>& target_position);

}  // namespace ro::render
