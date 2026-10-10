#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA PARTICULE MONTANTE, et les huit effets qui l'emploient.
//
// Ce bâtisseur ne ressemble pas à celui des anneaux : il boucle sur ses quatre
// pistes, tire au sort les trois angles et le retard de chacune, et ancre les
// quatre au même point — la position de l'effet.

// COMBIEN DE TICS UN NŒUD DE PARTICULES VIT.
//
// ⚠ CE NOMBRE EST DÉDUIT DU MOUVEMENT, faute de mieux, et c'est dit ici pour
// qu'on ne le prenne pas pour une mesure. Aucun des huit cas ne pose la vie de
// son instance, et ce bâtisseur-là ne la relève pas comme celui des anneaux : le
// client la tient d'ailleurs, et nous ne l'avons pas trouvée.
//
// Ce que nous savons, en revanche, est entier : le retard le plus long que le
// bâtisseur tire, puis la montée, la pose et la descente de l'alpha jusqu'à
// zéro. Passé ce tic, aucune des quatre pistes ne peut plus rien montrer — un
// test l'éprouve sur le mouvement lui-même, et le tic d'avant ne suffit pas.
inline constexpr std::int32_t kEffectRisingParticleLifeTicks = 108;

// DE COMBIEN LE RAYON DE LA SPHÈRE CROÎT PAR TIC, quand le bâtisseur n'en dit
// pas davantage : un. Le bâtisseur des pièces lancées en l'air pose un et demi.
inline constexpr float kEffectRisingParticleRadiusGrowth = 1.0F;

// UN NŒUD DE PARTICULES QU'UN EFFET SÈME.
struct EffectRisingParticlePattern {
  std::uint32_t effect_id = 0;
  // L'étiquette de la routine du client qui pose ce nœud : elle mène d'une
  // figure qui se montre mal jusqu'au code qui la décrit.
  std::string_view client_function;
  std::string_view texture;
  // La demi-diagonale du carré, que le bâtisseur range en deuxième case de
  // chaque piste. Le site d'appel la passe en FLOTTANT, non en entier.
  float half_diagonal = 0.0F;
  // La marque que la figure lit : elle décide à la fois de la couleur, de
  // l'orientation et de la profondeur. Voir `effect_particle_figures.h`.
  std::uint8_t color_case = 0;
  // LE PAS DONT LE RAYON CROÎT, rangé en première case de chaque piste : un
  // pour le bâtisseur à paramètres, un et demi pour celui des pièces.
  float radius_growth_per_tick = kEffectRisingParticleRadiusGrowth;
  // COMBIEN DE FOIS LE SITE APPELLE LE BÂTISSEUR, chaque appel semant son nœud
  // et tirant ses propres angles : trente pour les pièces, un ailleurs.
  std::uint16_t copies = 1;
};

// Tous les nœuds de particules connus, par identifiant d'effet puis par ordre
// de semis.
[[nodiscard]] std::span<const EffectRisingParticlePattern> EffectRisingParticlePatternTable();

// TOUS les nœuds de particules d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectRisingParticlePattern> EffectRisingParticlePatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES D'UN NŒUD DE PARTICULES.
//
// ⚠ ELLES SE TIRENT AU SORT, et c'est la figure même de l'effet : trois angles
// et un retard par piste. Semer quatre pistes identiques les superposerait, et
// l'on ne verrait qu'une particule là où le client en montre quatre.
//
// Le générateur avance à chaque tirage : deux nœuds semés à la suite diffèrent,
// et repartir de la même graine rejoue exactement la même figure.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectRisingParticleTracks(
    float half_diagonal, std::uint8_t color_case, glm::vec3 anchor, std::uint32_t& generator,
    float radius_growth_per_tick = kEffectRisingParticleRadiusGrowth);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
// ⚠ UN SEUL, même quand le patron en veut plusieurs : c'est à l'appelant de
// répéter le semis `copies` fois, le générateur avançant à chaque nœud.
[[nodiscard]] PooledEffectNode SowEffectRisingParticleNode(
    const EffectRisingParticlePattern& pattern, glm::vec3 anchor, std::uint32_t& generator);

}  // namespace ro::render
