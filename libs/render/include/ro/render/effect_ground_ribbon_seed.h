#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_ground_ribbon.h"
#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU RUBAN AU SOL, et les douze effets qui l'emploient par le
// sous-type 56.
//
// ⚠ QUATRE BÂTISSEURS, UN SEUL SEMIS. Relevés par leurs écritures de champs, les
// quatre touchent EXACTEMENT les mêmes cases dans le même ordre. Ce qui les
// sépare tient aux VALEURS, et trois des quatre partagent la même famille de
// constantes. Le coût que leur nombre laissait craindre n'était pas là.
//
// ⚠ ET LES QUATRE PISTES SONT VIVES, toutes garnies du même tirage : quatre
// rubans qui partent ensemble, chacun avec ses nombres.

// LEQUEL DES QUATRE pose ce nœud. Le nom ne sert qu'à désigner : c'est le
// semis ci-dessous qui dit ce que chacun fait.
enum class EffectGroundRibbonBuilder : std::uint8_t {
  kInitTextured,
  kBuildLineBeams,
  kInitLineBeam,
  kInitLineBeamYellow,
};

// UN RUBAN QU'UN EFFET POSE.
struct EffectGroundRibbonPattern {
  std::uint32_t effect_id = 0;
  // LA FONCTION DU CLIENT qui pose ce ruban, par son nom.
  std::string_view client_function;
  // ⚠ VIDE QUAND LE BÂTISSEUR CHOISIT SON IMAGE LUI-MÊME. Deux des quatre la
  // reçoivent de leur site, les deux autres la décident — l'ADR 0019 se lit ici
  // dans les deux sens, et supposer l'un pour toute la famille se tromperait une
  // fois sur deux.
  std::string_view texture;
  EffectGroundRibbonBuilder builder = EffectGroundRibbonBuilder::kInitTextured;
  // L'argument que le site pousse : son reste par dix décide du SENS, et sa
  // dizaine du jeu de constantes.
  std::int16_t builder_argument = 0;
};

// Tous les rubans connus, par identifiant d'effet.
[[nodiscard]] std::span<const EffectGroundRibbonPattern> EffectGroundRibbonPatternTable();

// TOUS les rubans d'un effet, ou un span VIDE. ⚠ Un effet peut en poser DEUX,
// un par sens.
[[nodiscard]] std::span<const EffectGroundRibbonPattern> EffectGroundRibbonPatternsFor(
    std::uint32_t effect_id);

// L'IMAGE QU'UN PATRON DEMANDE : la sienne, ou celle que son bâtisseur choisit.
[[nodiscard]] std::string_view EffectGroundRibbonTextureFor(
    const EffectGroundRibbonPattern& pattern);

// LE MOT DE RENDU que le bâtisseur écrit dans son nœud. Voir
// `effect_render_word.h`.
[[nodiscard]] std::uint32_t EffectGroundRibbonRenderWord(const EffectGroundRibbonPattern& pattern);

// LES QUATRE PISTES, toutes vives et toutes tirées.
//
// `target_degrees` est le cap vers la cible : le client le calcule entre la
// position de l'effet et un point que l'instance porte. ⚠ CE N'EST PAS UN ANGLE
// SEMÉ mais une DIRECTION — le ruban pointe vers quelque chose, et le tirer au
// sort ferait pointer les rubans n'importe où.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectGroundRibbonTracks(
    const EffectGroundRibbonPattern& pattern, std::int16_t target_degrees,
    std::uint32_t& generator);

// LE CAP VERS LA CIBLE, en degrés entiers : celui du point du sol que l'effet
// vise, vu depuis celui où il naît. Les deux points sont dans NOTRE repère ; le
// cap se mesure dans celui du client, profondeur retournée. Deux points
// confondus rendent zéro.
[[nodiscard]] std::int16_t EffectGroundRibbonTargetDegrees(glm::vec3 from, glm::vec3 to);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectGroundRibbonNode(const EffectGroundRibbonPattern& pattern,
                                                         glm::vec3 anchor,
                                                         std::int16_t target_degrees,
                                                         std::int32_t effect_life_ticks,
                                                         std::uint32_t& generator);

}  // namespace ro::render
