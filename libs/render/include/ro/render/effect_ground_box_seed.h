#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE SEMIS DE LA BOÎTE AU SOL, sous-type 64.
//
// UN SEUL bâtisseur et QUATRE sites, un par identifiant. Chaque site lui passe
// une image et un
// octet, et l'octet choisit tout le reste.
//
// ⚠ UNE SEULE PISTE VIVE SUR QUATRE. Le bâtisseur éteint les trois autres ;
// en allumer quatre peindrait quatre boîtes superposées.

// LES CINQ OCTETS QUE LE BÂTISSEUR NOMME. Quatre sont employés ; le
// troisième ne l'est par personne, et il ne pose ni alpha ni durée — sa boîte
// naîtrait invisible.
inline constexpr std::uint8_t kGroundBoxPlainByte = 0;
inline constexpr std::uint8_t kGroundBoxPaleByte = 1;
inline constexpr std::uint8_t kGroundBoxFullByte = 2;
inline constexpr std::uint8_t kGroundBoxMuteByte = 3;
inline constexpr std::uint8_t kGroundBoxPulsingByte = 4;

// LES TROIS HAUTEURS DE CLÉ, et celle que la bascule de réglage substitue.
//
// ⚠ NOUS PRENONS LA BRANCHE PLEINE. Le client consulte un réglage (le drapeau
// 0xE5) pour l'octet ZÉRO seulement, et rabaisse alors sa clé de cinquante à
// dix. Nous ne modélisons pas ce réglage — c'est le même choix que pour
// l'éclat qui fuit.
inline constexpr float kGroundBoxTallLift = 50.0F;
inline constexpr float kGroundBoxReducedLift = 10.0F;
inline constexpr float kGroundBoxMidLift = 16.0F;
inline constexpr float kGroundBoxFullLift = 32.0F;

// LES DEUX DEMI-CÔTÉS, et les deux alphas.
inline constexpr float kGroundBoxWideHalfSide = 5.0F;
inline constexpr float kGroundBoxNarrowHalfSide = 2.5F;
inline constexpr std::int16_t kGroundBoxDimAlpha = 30;
inline constexpr std::int16_t kGroundBoxBrightAlpha = 120;

// LES TROIS DURÉES, en tics.
inline constexpr std::int16_t kGroundBoxShortDuration = 864;
inline constexpr std::int16_t kGroundBoxMidDuration = 1880;
inline constexpr std::int16_t kGroundBoxEndlessDuration = 29999;

// CE QUE LE BLEU QUI PULSE TIRE au départ : de zéro à quarante-neuf.
inline constexpr std::int32_t kGroundBoxBlueFloorSpread = 50;

// CE QU'UN OCTET POSE. Les branches du bâtisseur, mises à plat.
struct EffectGroundBoxRecipe {
  float lift_key = 0.0F;
  std::int16_t alpha = 0;
  // La case que notre piste appelle « style », et qui porte ici une DURÉE en
  // tics — au-delà, l'alpha tombe de deux par tic (ADR 0017).
  std::int16_t duration = 0;
  float half_side = 0.0F;
  // ⚠ VRAI POUR LE SEUL OCTET QUATRE : lui seul tire le plancher de son bleu,
  // et lui seul le fait pulser. Tirer pour les autres décalerait la suite du
  // générateur.
  bool draws_blue = false;
};

[[nodiscard]] EffectGroundBoxRecipe EffectGroundBoxRecipeOf(std::uint8_t style_byte);

// UNE BOÎTE qu'un effet pose.
struct EffectGroundBoxPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::string_view texture;
  // L'OCTET DU SITE : il choisit la hauteur, l'alpha, la durée, la taille, la
  // teinte et le mélange.
  std::uint8_t style_byte = 0;
};

[[nodiscard]] std::span<const EffectGroundBoxPattern> EffectGroundBoxPatternTable();

[[nodiscard]] std::span<const EffectGroundBoxPattern> EffectGroundBoxPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES d'un nœud, dont UNE SEULE est vive.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedEffectGroundBoxTracks(
    const EffectGroundBoxPattern& pattern, std::uint32_t& generator);

// LE NŒUD qu'un patron décrit, prêt à rejoindre la troupe.
[[nodiscard]] PooledEffectNode SowEffectGroundBoxNode(const EffectGroundBoxPattern& pattern,
                                                      glm::vec3 anchor,
                                                      std::int32_t effect_life_ticks,
                                                      std::uint32_t& generator);

}  // namespace ro::render
