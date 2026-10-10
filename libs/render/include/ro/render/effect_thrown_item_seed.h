#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE L'OBJET LANCÉ, et les onze EF_THROWITEM du sous-type 53.
//
// Une image, un style, et QUATRE pistes vives — la tête opaque et trois crans
// éteints, que le mouvement remplira l'un après l'autre.
//
// ⚠ IL LUI FAUT DEUX POINTS, et c'est ce qui le distingue de presque toute la
// famille procédurale : le bâtisseur mesure la DISTANCE entre le lanceur et sa
// cible, et en tire à la fois le pas d'avance, la pente et le nombre de tics du
// vol. Sans cible il n'y a rien à semer — un objet lancé nulle part n'est pas
// un objet lancé plus court.
//
// ⚠ ET LE STYLE DÉCIDE DU TIC DE NAISSANCE. La plupart des objets naissent au
// tic zéro ; le style dix naît au cinquième et le style onze au dixième. C'est
// ainsi qu'EF_THROWITEM4 jette deux flacons l'un après l'autre, d'un seul
// identifiant.

// LES CASES ET LES MARQUES QUE CE SEMIS REMPLIT, et leur sens appartient à sa
// figure (ADR 0017).
inline constexpr std::size_t kSlotOfThrownSpeed = 1;
inline constexpr std::size_t kSlotOfThrownRise = 2;
inline constexpr std::size_t kMarkOfThrownSteps = 1;
inline constexpr std::size_t kMarkOfThrownKindSeed = 2;
inline constexpr std::size_t kMarkOfThrownExtra = 3;
inline constexpr std::size_t kTailSlotOfThrownBaseHeight = 4;

// UN OBJET QU'UN EFFET LANCE : ce que son site d'appel passe au bâtisseur.
struct EffectThrownItemPattern {
  std::uint32_t effect_id = 0;
  // LA FONCTION DU CLIENT qui lance cet objet.
  std::string_view client_function;
  std::string_view texture;
  std::uint8_t builder_argument = 0;
};

// Tous les objets lancés connus, par identifiant d'effet.
[[nodiscard]] std::span<const EffectThrownItemPattern> EffectThrownItemPatternTable();

// TOUS les objets d'un effet, ou un span VIDE.
[[nodiscard]] std::span<const EffectThrownItemPattern> EffectThrownItemPatternsFor(
    std::uint32_t effect_id);

// À QUEL TIC DE L'EFFET CE STYLE NAÎT. Zéro pour presque tous.
[[nodiscard]] std::int32_t EffectThrownItemBirthTick(std::uint8_t builder_argument);

// LES QUATRE PISTES D'UN OBJET LANCÉ : la tête opaque, trois crans éteints.
//
// `source` est d'où l'objet part, `target` où il tombe. Les deux sont en unités
// du monde, et c'est leur ÉCART HORIZONTAL qui règle tout le vol.
//
// 🔴 `instance_param` EST LE PARAMÈTRE DE L'INSTANCE — le nombre de coups d'un
// coup multi-coups, un sinon —, BORNÉ À CINQUANTE par l'instance qui le reçoit
// (voir `InstanceParamOf`). Le style un en fait sa SOUS-MARQUE, tronqué à
// l'octet : la teinte du flacon en dépend, et à zéro il ne se dessine pas.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectThrownItemTracks(
    const EffectThrownItemPattern& pattern, glm::vec3 source, glm::vec3 target,
    std::int32_t instance_param);

// LE NŒUD qu'un patron décrit, pistes semées, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectThrownItemNode(const EffectThrownItemPattern& pattern,
                                                       glm::vec3 source, glm::vec3 target,
                                                       std::int32_t effect_life_ticks,
                                                       std::int32_t instance_param);

}  // namespace ro::render
