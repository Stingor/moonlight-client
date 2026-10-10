#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>

#include <glm/glm.hpp>

#include "ro/render/effect_node_pool_fwd.h"
#include "ro/render/effect_node_world.h"
#include "ro/render/effect_track_fwd.h"

namespace ro::render {

// LE VOILE TEINTÉ, sous-type 172 : un carré de quatre cents unités de côté face
// à la caméra, dix unités au-dessus de son porteur, qui baigne la scène d'une
// couleur.
//
// Deux effets le posent : 669 en rouge et 673 en bleu. Le rouge paraît vite, de
// dix par tic jusqu'à cinquante, puis s'éteint passé le soixante-cinquième tic ;
// le bleu paraît lentement, de cinq par tic, et reste.
//
// ⚠ SON MOUVEMENT EST CELUI DE LA NAPPE AU SOL, et c'est le même code chez le
// client : `AdvanceParticleFrameTracks`. C'est la marque dix de la piste — l'octet
// du site — qui y choisit le rythme de l'alpha. Sa FIGURE, elle, est à part : un
// seul carré, dont la teinte vient du NŒUD et non de la marque.
//
// ⚠ ET IL SE PEINT PAR-DESSUS TOUT : le bâtisseur lève le bit 0x200 du mot de
// rendu, qui retire l'épreuve de profondeur. Le voile ne se cache donc derrière
// rien.

// LA MARQUE QUI CHOISIT LE RYTHME DE L'ALPHA : celle de la nappe au sol.
inline constexpr std::size_t kColoredVeilMarkOfRhythm = 10;

// LA DEMI-TAILLE DU CARRÉ et sa hauteur, dans le repère DESCENDANT du client :
// moins dix, soit dix unités au-dessus du nœud.
inline constexpr float kColoredVeilHalfSize = 200.0F;
inline constexpr float kColoredVeilHeightDown = -10.0F;

// LE MOT DE RENDU : celui du constructeur, auquel le bâtisseur ajoute le bit
// qui peint par-dessus tout.
inline constexpr std::uint32_t kColoredVeilRenderWord = 0x201;

// L'IMAGE, la même pour tous.
inline constexpr std::string_view kColoredVeilTexture = "data\\texture\\effect\\white02.bmp";

// UN SITE D'APPEL : l'effet, la fonction du client, l'octet du rythme, la
// teinte et la vie de l'instance que le nœud recopie.
struct EffectColoredVeilPattern {
  std::uint32_t effect_id = 0;
  std::string_view client_function;
  std::uint8_t byte = 0;
  std::array<std::uint8_t, 3> color = {255, 255, 255};
  std::int32_t life_ticks = 0;
};

[[nodiscard]] std::span<const EffectColoredVeilPattern> EffectColoredVeilPatternTable();

[[nodiscard]] std::span<const EffectColoredVeilPattern> EffectColoredVeilPatternsFor(
    std::uint32_t effect_id);

// LES QUATRE PISTES DU NŒUD, la première seule allumée, alpha nul. Rien de tiré.
[[nodiscard]] std::array<EffectTrack, kEffectTracksPerNode> SeedColoredVeilTracks(
    std::uint8_t byte);

[[nodiscard]] PooledEffectNode SowColoredVeilNode(const EffectColoredVeilPattern& pattern,
                                                  glm::vec3 owner_position);

// LE CARRÉ DE LA PREMIÈRE PISTE, ou rien quand elle est éteinte ou sans alpha.
//
// `turn_degrees` et `tilt_degrees` sont les deux angles que le mouvement a
// rangés dans les styles des pistes zéro et un ; `color` est la teinte du nœud.
[[nodiscard]] std::optional<EffectWorldQuad> ComposeColoredVeilQuad(
    const EffectTrack& track, std::int16_t turn_degrees, std::int16_t tilt_degrees,
    const std::array<float, 3>& color, glm::vec3 node_origin);

}  // namespace ro::render
