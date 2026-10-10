#pragma once

#include <cstdint>

namespace ro::render {

// LE NŒUD NOMMÉ SANS ÊTRE DÉFINI, et la disposition de ses exemplaires.
//
// Un en-tête qui ne fait que déclarer une fonction prenant un nœud s'en
// contente : la définition complète est dans `effect_node.h`, et seul le
// fichier qui lit ou écrit un champ l'inclut. L'énumération est déclarée avec
// son type sous-jacent, ce qui suffit à la passer par valeur ; ses énumérateurs
// restent dans `effect_node.h`.

struct EffectNode;
enum class EffectNodeInstancePlacement : std::uint8_t;

}  // namespace ro::render
