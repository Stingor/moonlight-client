#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU SEMEUR QUI MARCHE, sous-type 120.
//
// UN SEUL bâtisseur, et trois cas qui l'appellent, chacun avec son octet. Il ne
// charge aucune image ; il recopie dans le nœud la cible de l'instance, et pose
// quatre pistes RETARDÉES — compteurs de moins trois à zéro —, chacune à la
// place de l'instance, relevée de six à dix unités tirées au sort.
//
// ⚠ UN NŒUD AU PREMIER TIC SEULEMENT : la garde est l'âge nul de l'instance.

// LE RETARD DE LA PREMIÈRE PISTE : chacune des suivantes part un tic plus tôt.
inline constexpr std::int32_t kWalkingSowerFirstTrackTicks = -3;
// LA HAUTEUR DE NAISSANCE, dans le repère DESCENDANT du client : `rand() % 5`
// ajouté à la place de l'instance moins dix. La piste naît donc entre six et
// dix unités AU-DESSUS de l'instance.
inline constexpr std::int32_t kWalkingSowerHeightDraws = 5;
inline constexpr float kWalkingSowerHeightBelowFloor = -10.0F;

// UN SITE : l'effet, celui que son nœud fait naître, la fonction du client et
// l'octet.
//
// ⚠ L'EFFET QUI NAÎT SE DÉDUIT DE L'OCTET — voir `WalkingSowerChildOf`. Il est
// en table pour que la mesure sache que le semeur se montre par lui, et un
// essai tient les deux d'accord.
struct EffectWalkingSowerPattern {
  std::uint32_t effect_id = 0;
  std::uint32_t spawned_effect_id = 0;
  std::string_view client_function;
  std::uint8_t byte = 0;
};

[[nodiscard]] std::span<const EffectWalkingSowerPattern> EffectWalkingSowerPatternTable();

[[nodiscard]] std::span<const EffectWalkingSowerPattern> EffectWalkingSowerPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES, toutes allumées. Le générateur avance d'un tirage par
// piste, dans l'ordre du client.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedWalkingSowerTracks(
    std::uint8_t byte, glm::vec3 instance_position, std::uint32_t& generator);

// LE CAP DU CLIENT, en degrés entiers, du nœud vers la cible — deux points de
// NOTRE monde. Zéro sans cible, comme pour un vecteur nul.
[[nodiscard]] std::int16_t WalkingSowerClientHeading(glm::vec3 node_position,
                                                     const std::optional<glm::vec3>& target);

// LE NŒUD qu'un patron décrit, né à la place de l'instance. `effect_life_ticks`
// est la vie de l'effet, que le bâtisseur recopie dans le nœud.
[[nodiscard]] PooledEffectNode SowWalkingSowerNode(const EffectWalkingSowerPattern& pattern,
                                                   glm::vec3 instance_position,
                                                   const std::optional<glm::vec3>& target,
                                                   std::int32_t effect_life_ticks,
                                                   std::uint32_t& generator);

}  // namespace ro::render
