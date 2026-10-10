#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA GRILLE DE VISÉE, sous-type 112.
//
// UN SEUL identifiant,
// EF_GROUNDSAMPLE, et le bâtisseur ne prend aucun paramètre : son image est une
// constante, et il n'allume qu'une piste sur quatre.
//
// ⚠ IL NE SÈME QU'UNE FOIS. Le répartiteur le rejoue à chaque tic, et sa garde
// est le compteur de l'instance : le nœud naît au premier tic et ne se redouble
// jamais. Le semer à chaque tic empilerait des grilles identiques.
//
// ⚠ ET LA PISTE NAÎT NUE. Le bâtisseur ne pose que deux choses — la piste zéro
// vive, son azimut à zéro — et laisse tout le reste à son défaut, alpha compris.
// Ce n'est pas un oubli : la figure ne lit pas l'alpha, elle porte ses deux
// teintes en dur.

// UNE GRILLE qu'un effet pose.
struct EffectMagicTargetPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
};

[[nodiscard]] std::span<const EffectMagicTargetPattern> EffectMagicTargetPatternTable();

[[nodiscard]] std::span<const EffectMagicTargetPattern> EffectMagicTargetPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, dont UNE SEULE est vive.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectMagicTargetTracks();

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectMagicTargetNode(const EffectMagicTargetPattern& pattern,
                                                        glm::vec3 anchor,
                                                        std::int32_t effect_life_ticks);

}  // namespace ro::render
