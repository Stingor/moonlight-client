#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU GRAIN QUI TOMBE, et les effets du sous-type 157.
//
// Une image, et tout le reste tiré au sort. Les QUATRE pistes sont vives, et chacune tire
// six nombres — sa clé de descente, son rayon de grain, son azimut, son rayon de
// disque, sa profondeur de départ et sa teinte.
//
// ⚠ CE BÂTISSEUR EST UN ÉMETTEUR, non un poseur. Il ne pose pas un nœud mais en
// sème un TOUS LES DIX TICS tant que l'effet a moins de mille tics — soit une
// centaine de nœuds de quatre grains chacun. C'est cela, la pluie : ce n'est pas
// un nœud qui dure, c'est un nœud qui revient.
//
// ⚠ ET SES GRAINS NAISSENT LOIN AU-DESSUS DE LEUR NŒUD, de soixante à
// quatre-vingts unités, à une ordonnée NÉGATIVE : l'ancre reste dans le repère
// du client, qui descend, et le dessinateur la ramène au nôtre. Ils tombent
// de deux et demie à trois unités par tic, et ne commencent à s'éteindre qu'une
// fois le nœud rejoint. Les semer à hauteur du nœud les éteindrait aussitôt.

// LA CLÉ DE DESCENTE : de deux et demie à trois unités par tic.
inline constexpr float kRainParticleLiftFloor = 2.5F;
inline constexpr float kRainParticleLiftStep = 0.01F;
inline constexpr std::uint32_t kRainParticleLiftSpread = 51;

// LE RAYON DU GRAIN : de sept à dix unités.
inline constexpr float kRainParticleRadiusFloor = 7.0F;
inline constexpr float kRainParticleRadiusStep = 0.1F;
inline constexpr std::uint32_t kRainParticleRadiusSpread = 31;

// LE RAYON DU DISQUE où le grain se pose, de zéro à trente-deux unités.
inline constexpr std::uint32_t kRainParticleOrbitSpread = 33;

// LA PROFONDEUR DE DÉPART, sous le nœud : de soixante à quatre-vingts unités.
inline constexpr float kRainParticleDepthFloor = -80.0F;
inline constexpr std::uint32_t kRainParticleDepthSpread = 21;

// COMBIEN DE TEINTES le tirage de marque couvre.
inline constexpr std::uint32_t kRainParticleTintSpread = 3;

// LE RYTHME DE L'ÉMETTEUR : une bouffée tous les dix tics, du tic zéro au
// dernier multiple de dix sous mille. Une bouffée vit de trente-sept à
// quarante-neuf tics : il y en a quatre ou cinq à l'écran en régime.
inline constexpr EffectNodeSchedule kRainParticleSchedule = {0, 990, 10, 0};

// UNE PLUIE QU'UN EFFET POSE : ce que son site d'appel passe au bâtisseur.
struct EffectRainParticlePattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
};

// Toutes les pluies connues, par identifiant d'effet.
[[nodiscard]] std::span<const EffectRainParticlePattern> EffectRainParticlePatternTable();

// TOUTES les pluies d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectRainParticlePattern> EffectRainParticlePatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UNE BOUFFÉE, toutes vives et toutes tirées séparément.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRainParticleTracks(
    std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectRainParticleNode(const EffectRainParticlePattern& pattern,
                                                         glm::vec3 anchor,
                                                         std::int32_t effect_life_ticks,
                                                         std::uint32_t& generator);

}  // namespace ro::render
