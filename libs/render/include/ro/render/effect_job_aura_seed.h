#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE L'AURA, sous-types 99 et 35.
//
// DEUX bâtisseurs — un pour le 99, un pour le 35 — et SIX sites, un par
// identifiant. Chacun leur passe DEUX octets : le nombre de pistes vives, puis
// la marque de teinte.
//
// ⚠ LE NŒUD N'EST BÂTI QU'UNE FOIS. Les deux bâtisseurs testent leur propre
// pointeur de nœud enfant et ne font rien s'il est déjà posé — or le répartiteur
// les rappelle à CHAQUE tic. C'est un effet PERSISTANT, pas un effet qui sème :
// semer à chaque tic couvrirait le joueur d'auras empilées.

// LES CINQ IMAGES que les deux bâtisseurs chargent, dans l'ordre où leur numéro
// les désigne.
inline constexpr std::size_t kJobAuraTextureCount = 5;

// CE QUI SÉPARE LES DEUX SOUS-TYPES AU SEMIS : le rayon du carré et les alphas
// de départ.
inline constexpr float kLayeredAuraRadius = 2.0F;
inline constexpr float kPlainAuraRadius = 3.5F;

// ⚠ LES ALPHAS DE DÉPART SONT ÉCHELONNÉS, et c'est TOUT ce qui étale les
// renaissances. Les quatre particules naissent à la même phase — DIX, donc déjà
// en descente — et ne se distinguent que par là. Leur donner le même alpha les
// ferait clignoter à l'unisson pour toute la vie de l'aura.
inline constexpr float kJobAuraStartPhase = 10.0F;
inline constexpr std::array<float, 4> kLayeredAuraStartAlphas = {40.0F, 80.0F, 120.0F, 160.0F};
inline constexpr std::array<float, 4> kPlainAuraStartAlphas = {70.0F, 130.0F, 190.0F, 250.0F};

// SUR QUELLE HAUTEUR une particule naît.
//
// ⚠ CE N'EST PAS L'INTERVALLE DE LA RENAISSANCE : le bâtisseur tire sur dix à
// partir de zéro, le mouvement sur huit à partir de trois. Recopier l'un pour
// l'autre resserrerait l'aura d'un cran et la décollerait des pieds.
inline constexpr std::int32_t kJobAuraSpawnHeightSpread = 10;

// COMBIEN DE TIRAGES une piste consomme : trois par particule, plus un.
inline constexpr std::size_t kJobAuraDrawsPerTrack = 13;

// UNE AURA qu'un effet pose sur son porteur.
struct EffectJobAuraPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  // LES CINQ IMAGES du nœud, dans l'ordre de leur numéro.
  std::span<const std::string_view> textures;
  // COMBIEN DE PISTES le site allume : une, deux ou quatre.
  std::uint8_t live_tracks = 1;
  // LA MARQUE DE TEINTE que le site pousse. Elle choisit la palette du 99 et,
  // pour le 35, elle décide du rouge.
  std::uint8_t palette_mark = 0;
  // VRAI POUR LE SOUS-TYPE 99, qui peint trois couches et plafonne son alpha.
  bool layered = false;
};

[[nodiscard]] std::span<const EffectJobAuraPattern> EffectJobAuraPatternTable();

[[nodiscard]] std::span<const EffectJobAuraPattern> EffectJobAuraPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, dont seules les premières sont vives.
//
// ⚠ LES PISTES ÉTEINTES NE TIRENT PAS, au contraire des lucioles : le bâtisseur
// les éteint TOUTES d'abord, puis ne boucle que sur celles qu'il allume. Leur
// faire consommer des tirages décalerait la suite du générateur.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectJobAuraTracks(
    const EffectJobAuraPattern& pattern, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectJobAuraNode(const EffectJobAuraPattern& pattern,
                                                    glm::vec3 anchor,
                                                    std::int32_t effect_life_ticks,
                                                    std::uint32_t& generator);

}  // namespace ro::render
