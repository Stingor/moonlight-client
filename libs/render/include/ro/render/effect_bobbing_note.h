#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LA NOTE QUI FLOTTE, sous-type 156 : une note de musique face à la caméra,
// six unités au-dessus de son porteur, qui monte et descend de trois unités sur
// un tour de trois cent soixante tics et frémit.
//
// Trois effets la posent — 856, 860 et 1569 —, et c'est l'OCTET de leur site qui
// la distingue : il ne change ni l'image ni le mouvement, seulement le
// MÉLANGE et la TEINTE de son halo.
//
// ⚠ UNE NOTE EST CINQ CARRÉS, et non un. Le carré plein d'abord, puis quatre
// copies à demi-alpha, décalées de deux dixièmes d'unité vers les quatre côtés
// du plan du sol et toujours ajoutées : c'est ce liseré qui lui donne son halo.
//
// ⚠ ET SES COINS NE FONT PAS UN CARRÉ. Le premier est à quarante-cinq degrés ;
// chacun des trois suivants est à quatre-vingt-cinq degrés du précédent PLUS un
// tirage de zéro à quinze. La note se déforme donc sans cesse, ce qui la fait
// vibrer.

// LES CASES DE LA PISTE.
//
// LE FRÉMISSEMENT, en degrés, dans la neuvième case flottante : il module le
// rayon de cinq pour cent.
inline constexpr std::size_t kBobbingNoteSlotOfShiver = 8;
// L'IMAGE et L'OCTET du site, dans les deux premières marques.
inline constexpr std::size_t kBobbingNoteMarkOfImage = 0;
inline constexpr std::size_t kBobbingNoteMarkOfByte = 1;

// LE SEMIS : le rayon, l'alpha, l'angle du premier coin.
inline constexpr float kBobbingNoteRadius = 6.0F;
inline constexpr std::int16_t kBobbingNoteAlpha = 120;
inline constexpr std::int16_t kBobbingNoteFirstCornerDegrees = 45;

// LE MOUVEMENT : cinq degrés de frémissement et un de balancement par tic.
inline constexpr float kBobbingNoteShiverPerTick = 5.0F;
inline constexpr std::int16_t kBobbingNoteSwayPerTick = 1;

// LA HAUTEUR DE LA NOTE au-dessus du porteur, et l'amplitude de son
// balancement. Dans NOTRE repère : elle monte quand le sinus baisse.
inline constexpr float kBobbingNoteLift = 6.0F;
inline constexpr float kBobbingNoteSwayAmplitude = 3.0F;

// LE FRÉMISSEMENT DU RAYON : `R · sin(frémissement) · 1/20 + R`.
inline constexpr float kBobbingNoteShiverShare = 0.05F;

// LES COINS SUIVANTS : quatre-vingt-cinq degrés de plus que le précédent, et un
// tirage de seize valeurs.
inline constexpr std::int32_t kBobbingNoteCornerStepDegrees = 85;
inline constexpr std::uint32_t kBobbingNoteCornerDraws = 16;

// LE HALO : quatre copies à demi-alpha, écartées de deux dixièmes d'unité.
inline constexpr float kBobbingNoteHaloOffset = 0.2F;
inline constexpr std::array<std::uint8_t, 3> kBobbingNoteBlueHalo = {5, 5, 255};

// LES TROIS OCTETS : zéro, carré au mélange ordinaire et halo blanc ; un, le
// même avec un halo BLEU ; deux, carré AJOUTÉ et halo blanc.
inline constexpr std::uint8_t kBobbingNoteByteOfBlueHalo = 1;
inline constexpr std::uint8_t kBobbingNoteByteOfAddedNote = 2;

// LE MOT DE RENDU que le bâtisseur écrit en dur.
inline constexpr std::uint32_t kBobbingNoteRenderWord = 5;

// UN SITE D'APPEL : l'effet, la fonction du client, l'image, celle que
// `/mineffect` lui substitue — vide quand le cas n'en a pas —, l'octet, et la
// vie de l'instance que le nœud recopie.
struct EffectBobbingNotePattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  std::string_view minimal_texture;
  std::uint8_t byte = 0;
  std::int32_t life_ticks = 0;
};

[[nodiscard]] std::span<const EffectBobbingNotePattern> EffectBobbingNotePatternTable();

[[nodiscard]] std::span<const EffectBobbingNotePattern> EffectBobbingNotePatternsFor(
    std::uint32_t effect_id);

// L'IMAGE À CHARGER, selon que `/mineffect` demande la variante allégée.
[[nodiscard]] std::string_view EffectBobbingNoteTexture(const EffectBobbingNotePattern& pattern,
                                                        bool minimal_effects);

// LES QUATRE PISTES DU NŒUD, la première seule allumée. Le générateur avance
// d'UN tirage, le cap du balancement.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedBobbingNoteTracks(
    std::uint8_t byte, glm::vec3 owner_position, std::uint32_t& generator);

[[nodiscard]] PooledEffectNode SowBobbingNoteNode(const EffectBobbingNotePattern& pattern,
                                                  glm::vec3 owner_position, bool minimal_effects,
                                                  std::uint32_t& generator);

// LE MOUVEMENT D'UN TIC : les deux angles de panneau se rangent dans les styles
// des pistes zéro et un ; puis, si la PREMIÈRE piste est allumée — elle seule —,
// son compteur, son frémissement et son balancement avancent, et son ancre se
// RÉÉCRIT au-dessus de `node_position`.
void AdvanceBobbingNoteTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                              glm::vec3 node_position, std::int16_t billboard_turn_degrees,
                              std::int16_t billboard_tilt_degrees);

// LES CINQ CARRÉS D'UNE PISTE, ou rien quand elle est éteinte ou sans alpha.
//
// ⚠ LES TROIS TIRAGES DES COINS SE REFONT à chaque tic de la piste, à partir de
// `jitter_seed` : le client les refait à chaque image peinte.
[[nodiscard]] std::vector<EffectWorldQuad> ComposeBobbingNoteQuads(const EffectTrack& track,
                                                                   std::int16_t turn_degrees,
                                                                   std::int16_t tilt_degrees,
                                                                   std::uint32_t jitter_seed);

}  // namespace ro::render
