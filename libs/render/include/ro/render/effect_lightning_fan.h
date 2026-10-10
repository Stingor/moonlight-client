#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_particle_quad.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// L'ÉCLAIR : un tube à trois faces qui part du nœud, se courbe, et TREMBLE.
//
// Le dessinateur (voie A) et le mouvement du sous-type 101. TROIS
// identifiants l'emploient, et chacun en pose DEUX — un éventail vers l'avant,
// un vers l'arrière.
//
// ⚠ C'EST L'ARC QUI SERPENTE DU SOUS-TYPE 70, SANS SA DÉCROISSANCE. Le même
// tube à trois faces, la même phase qui donne une distance et un bombement —
// mais aucun rang géométrique : tous les segments ont la même longueur, et le
// bombement est mis à l'échelle d'un facteur tiré au semis.
//
// ⚠ ET CETTE FIGURE TIRE AU SORT. C'est la SEULE du lot dans ce cas, et ce
// n'est pas une commodité : le client re-tire son tremblement à CHAQUE IMAGE,
// dans le dessinateur et non dans le mouvement. C'est ce qui fait grésiller
// l'éclair sans que rien de son état ne change. Le ranger dans la piste
// demanderait soixante-trois flottants qui n'y tiennent pas, et le calculer
// d'une règle fixe figerait le grésillement.

// COMBIEN DE FACES le tube porte, et de combien de degrés on avance d'une face
// à la suivante.
inline constexpr std::size_t kLightningFanFaces = 3;
inline constexpr std::int32_t kLightningFanFaceStepDegrees = 120;

// LES BORNES DU TREMBLEMENT : sur combien de crans il se tire, de quel pas, et
// de combien il part vers le bas.
inline constexpr std::int32_t kLightningFanJitterSpread = 7;
inline constexpr float kLightningFanJitterStep = 0.1F;
inline constexpr float kLightningFanJitterFloor = -0.3F;

// DE COMBIEN CHAQUE SEGMENT EST ABAISSÉ, dans le repère du client.
inline constexpr float kLightningFanDrop = -2.0F;

// QUELS SEGMENTS SE DESSINENT : du premier au dix-huitième, bornes comprises.
//
// ⚠ LE SEGMENT ZÉRO NE SE DESSINE PAS, ni rien au-delà du dix-huitième. Le
// client borne son index par un test non signé, et c'est ce qui détache
// l'éclair de son nœud. Les peindre tous le collerait au lanceur.
inline constexpr std::size_t kLightningFanFirstDrawnSegment = 1;
inline constexpr std::size_t kLightningFanLastDrawnSegment = 18;

// DE COMBIEN L'ALPHA TOMBE d'un segment au suivant.
inline constexpr std::int16_t kLightningFanAlphaPerSegment = 5;

// LES CASES DE LA PISTE que cette figure lit (ADR 0017) : le facteur de
// bombement et le NOMBRE DE SEGMENTS, tous deux dans la queue.
inline constexpr std::size_t kLightningFanTailOfBow = 0;
inline constexpr std::size_t kLightningFanTailOfSegments = 1;

// LA VALEUR DE BALAYAGE QUI REND LA TEINTE ALÉATOIRE. Toute autre valeur donne
// le dégradé.
inline constexpr std::int16_t kLightningFanRandomColorSweep = 3;

// LE DÉGRADÉ : de combien le rouge et le vert montent par segment, et le bleu
// qui ne bouge pas.
inline constexpr std::uint8_t kLightningFanGradientPerSegment = 6;
inline constexpr std::uint8_t kLightningFanBlue = 255;

// LES BORNES DE LA TEINTE ALÉATOIRE : le vert se tire sur cent, le rouge sur
// cinquante puis RECULE de cinquante-six — il reboucle donc par le haut.
inline constexpr std::int32_t kLightningFanRandomGreenSpread = 100;
inline constexpr std::int32_t kLightningFanRandomRedSpread = 50;
inline constexpr std::int32_t kLightningFanRandomRedOffset = -56;

// OÙ LE BOUT D'UN SEGMENT SE TROUVE, dans le repère du CLIENT — son ordonnée
// descend — et relativement à l'ancre.
//
// `jitter` est le tremblement de ce segment, déjà tiré.
[[nodiscard]] glm::vec3 EffectLightningFanJoint(const EffectTrack& track, std::size_t segment,
                                                glm::vec3 jitter);

// COMBIEN DE SEGMENTS une piste porte. Le client le range en FLOTTANT et le
// tronque à chaque usage (ADR 0017).
[[nodiscard]] std::size_t LightningFanSegmentsOf(const EffectTrack& track);

// LE TREMBLEMENT D'UNE PISTE, un par segment, tiré dans le repère du CLIENT.
//
// ⚠ `generator` AVANCE de TROIS tirages par segment, dans l'ordre hauteur,
// abscisse, profondeur — le client remplit ses trois tableaux à rebours. Deux
// dessinateurs le font à l'identique, en tête de piste et HORS de toute garde :
// celui-ci et la dispersion d'éclairs.
[[nodiscard]] std::array<glm::vec3, kEffectTrackSlots> DrawLightningFanJitters(
    const EffectTrack& track, std::uint32_t& generator);

// LE TUBE D'UNE PISTE.
//
// ⚠ `generator` AVANCE : cette figure tire trois nombres par segment, et deux
// de plus par quad quand la teinte est aléatoire. Voir l'avertissement en tête
// de fichier.
//
// Rend un lot VIDE quand la piste est éteinte, quand son alpha n'est pas
// strictement positif, ou quand son compteur n'est pas encore positif.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeLightningFanQuads(
    const EffectTrack& track, std::uint32_t& generator, const EffectOwnerTint& owner = {});

// LE MOUVEMENT DU SOUS-TYPE 101 : l'éclair enfle, brille, et s'écrit D'UN COUP.
//
// ⚠ SON HISTORIQUE SE REMPLIT AU PREMIER TIC, ET SEULEMENT LÀ. Le client y fait
// vingt et un tours de la même boucle — avancer la tête de neuf degrés, décaler
// tout le tableau — au lieu d'un par tic. L'éclair naît donc entier, et rien de
// sa forme ne bouge ensuite : c'est le TREMBLEMENT de sa figure qui le fait
// vivre. L'étaler sur vingt et un tics le ferait pousser comme un serpent.
void AdvanceLightningFanTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                               std::uint32_t& generator);

// L'HISTORIQUE D'UNE PISTE, écrit d'un coup : vingt et un tours de « la tête
// avance de neuf degrés, tout le tableau glisse d'une case », la tête butant à
// cent quatre-vingts.
//
// ⚠ DEUX MOUVEMENTS LE FONT À L'IDENTIQUE, au premier tic et là seulement : le
// mouvement du sous-type 101 et celui du sous-type 135.
void WriteLightningFanHistory(EffectTrack& track);

}  // namespace ro::render
