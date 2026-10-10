#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DU CARRÉ QUI FILE EN TOURNOYANT, sous-type 116.
//
// UN SEUL bâtisseur et UN SEUL site : l'effet 520, dont l'image est
// `effect\shield_boomerang.bmp`.
//
// ⚠ LE SITE LE RAPPELLE À CHAQUE TIC, ET IL SÈME CINQ FOIS : aux âges zéro,
// trois, six, neuf et douze de l'instance. Cinq objets, chacun avec son cap
// tiré au sort, chacun parti de la place que son porteur occupe À CE TIC-LÀ —
// le bâtisseur la relit à chaque appel.
//
// ⚠ LE SON (`effect\cru_shield boomerang.wav`, à l'âge zéro) appartient à la
// couche son : il est déjà dans `effect_sound_table.cpp`.

// QUAND LE BÂTISSEUR SÈME : du tic zéro au douzième, tous les trois tics.
inline constexpr EffectNodeSchedule kSwirlTrailSchedule = {0, 12, 3, 0};

// CE QUE LE BÂTISSEUR ÉCRIT EN DUR sur la meneuse.
//
// Le rayon (5.0), l'alpha de naissance (25), la vitesse (2.5), et la hauteur
// au-dessus du porteur : le client RETRANCHE huit à son ordonnée, qui
// descend — chez nous, l'ancre MONTE de huit.
inline constexpr float kSwirlTrailRadius = 5.0F;
inline constexpr std::int16_t kSwirlTrailAlphaAtBirth = 25;
inline constexpr float kSwirlTrailSpeed = 2.5F;
inline constexpr float kSwirlTrailLiftAboveOwner = 8.0F;

// DE COMBIEN DE PAS L'ANCRE RECULE AU SEMIS : quinze pas de vitesse contre le
// cap. L'objet part trente-sept unités et demie en
// arrière et traverse la place de son porteur au quinzième tic — celui où son
// alpha commence à baisser.
inline constexpr float kSwirlTrailBackStepCount = 15.0F;

// LE MOT DE RENDU qu'il écrit en dur (5) : le seau lit le
// mélange de chaque primitive, et passe l'épreuve de profondeur. Voir
// `effect_render_word.h`.
inline constexpr std::uint32_t kSwirlTrailRenderWord = 5;

// UN SITE : l'effet, et l'image qu'il passe.
struct EffectSwirlTrailPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
};

[[nodiscard]] std::span<const EffectSwirlTrailPattern> EffectSwirlTrailPatternTable();

[[nodiscard]] std::span<const EffectSwirlTrailPattern> EffectSwirlTrailPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un objet semé à la place `owner_position`.
//
// UN SEUL TIRAGE, le cap (`rand() % 360`). La meneuse naît allumée, à
// vingt-cinq ; les trois échos naissent allumés à alpha NUL — le mouvement les
// écrase dès leur première recopie.
//
// ⚠ LES MARQUES ZÉRO ET DEUX DE LA MENEUSE SONT NULLES : image zéro, teinte
// zéro — le blanc, en mélange ordinaire, coins dans l'ordre. Voir
// `EffectTintedQuadStyleFor`.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectSwirlTrailTracks(
    glm::vec3 owner_position, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, semé à la place du porteur.
//
// `life_ticks` est la vie qui reste à l'instance : le bâtisseur y recopie la
// sienne, et un nœud meurt avec son instance.
[[nodiscard]] PooledEffectNode SowEffectSwirlTrailNode(const EffectSwirlTrailPattern& pattern,
                                                       glm::vec3 owner_position,
                                                       std::int32_t life_ticks,
                                                       std::uint32_t& generator);

}  // namespace ro::render
