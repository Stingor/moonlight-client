#pragma once

#include <cstdint>

#include "ro/render/effect_node_seed.h"

namespace ro::render {

struct EffectNodePattern;

// LA GRAINE D'UN SPRITE SEMÉ PAR UN PATRON — la jumelle d'`EffectNodeFromPattern`,
// qui fait le même travail pour les figures du monde.
//
// ⚠ ELLE NE TOUCHE AUCUN ÉTAT DE LA PARTIE : ce que le patron dit, le rang de ce
// semis-ci (`tick`, qui choisit la teinte d'une traînée et le roulis d'une
// boule) et le paramètre de l'instance. C'est ce qui la rend essayable hors
// jeu ; l'échelle, la dispersion et le lancer, qui se tirent nœud par nœud, se
// posent au semis.
//
// `ticks_left` est ce qui reste de vie à l'effet : un nœud ne lui survit pas, et
// un rendez-vous (`EffectNodePattern::life_until_tick`) le remplace.
[[nodiscard]] EffectNodeSeed EffectNodeSeedFromPattern(const EffectNodePattern& pattern,
                                                       std::int32_t ticks_left, std::int32_t tick,
                                                       std::int32_t param);

// LE CYCLE DE L'INSTANCE QUI SÈME CE PATRON au tic `tick`, quand son bâtisseur
// en ramène l'âge à zéro : la vie de la table des durées, que ce bâtisseur ne
// récrit pas, et l'âge du semis dans le cycle. Vide pour une instance dont
// l'âge ne reboucle pas. Voir `EffectNodeFadesWithACyclingInstance`.
[[nodiscard]] EffectInstanceCycle EffectNodePatternInstanceCycle(const EffectNodePattern& pattern,
                                                                 std::int32_t tick);

}  // namespace ro::render
