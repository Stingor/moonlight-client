#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// L'ONDE DE RUBAN, sous-type 36 : un anneau qui respire et que parcourt une
// vague.
//
// Quatre identifiants l'emploient — 239, 240, 241 et 620.
//
// ⚠ SA FIGURE ÉTAIT PORTÉE, SON MOUVEMENT NON, et c'est désormais la forme la
// plus fréquente de ce qui reste : quarante-sept identifiants ont leur
// dessinateur porté et attendent un mouvement. Voir `AdvanceRibbonWaveTracks`
// et `scripts/releve_pieces_de_l_onde_de_ruban.py`.

// CE QU'UN EFFET SÈME.
struct EffectRibbonWavePattern {
  std::uint32_t effect_id = 0;
  // L'étiquette du site d'appel qui pose ce nœud : elle mène d'une figure qui
  // se montre mal jusqu'au code qui la décrit.
  std::string_view client_function;
  std::string_view texture;
  // Le numéro de teinte que la primitive de ruban lit — voir
  // `effect_trail_palette.h`. C'est la SEULE chose que le paramètre décide.
  std::uint8_t trail_variant = 0;
};

// Tous les anneaux connus, par identifiant d'effet puis par ordre de semis.
[[nodiscard]] std::span<const EffectRibbonWavePattern> EffectRibbonWavePatternTable();

// TOUTES les ondes d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectRibbonWavePattern> EffectRibbonWavePatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES, dont UNE SEULE est vive.
//
// ⚠ DEUX TIRAGES, ET LE SECOND DÉCALE LE CYCLE. L'azimut se tire sur le tour,
// et l'ALPHA DE DÉPART sur cent : deux anneaux semés ensemble ne respirent donc
// pas en phase. Leur donner le même alpha les ferait battre d'un seul cœur.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRibbonWaveTracks(
    std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, piste semée, prêt à rejoindre la troupe.
//
// ⚠ IL SUIT SON PORTEUR : le répartiteur recopie à chaque tic la position de
// l'acteur dans celle du nœud. L'anneau reste donc sous les pieds de qui le
// porte, et l'oublier le laisserait au point du lancement.
[[nodiscard]] PooledEffectNode SowEffectRibbonWaveNode(const EffectRibbonWavePattern& pattern,
                                                       glm::vec3 ground_position,
                                                       std::uint32_t& generator);

}  // namespace ro::render
