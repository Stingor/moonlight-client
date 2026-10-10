#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include <glm/glm.hpp>

namespace ro::render {

// LES CHARGES AU-DESSUS DE LA TÊTE : un compte que l'état porte, montré par
// autant de copies d'un même fichier.
//
// Un seul effet par acteur les tient toutes. Quand le compte monte, les copies
// manquantes s'ajoutent sans toucher aux autres, dont l'image continue ; quand
// il descend, tout part et le compte se rebâtit depuis zéro. Au-delà du plus
// grand compte, rien ne change.
//
// ⚠ ELLES NE MEURENT PAS AVEC L'ÉTAT : l'effet vit tant que son porteur est là,
// et chaque copie s'arrête sur sa dernière image. C'est le prochain compte plus
// petit qui les efface.

// L'identifiant de l'effet qui porte les charges.
inline constexpr std::uint16_t kChargeCountEffectId = 1764;

// Le fichier de chaque copie, relatif au dossier des effets.
inline constexpr std::string_view kChargeCountFile =
    "DRAGON_KNIGHT\\DK_CHARGINGPIERCE\\chargingpierce_cast2\\chargingpierce_cast2.str";

// Le plus grand compte montré.
inline constexpr std::int32_t kChargeCountMost = 10;

// L'échelle de chaque copie, autour du point qui la porte.
inline constexpr float kChargeCountScale = 0.75F;

// OÙ CHAQUE COPIE SE TIENT, dans le plan de la VUE et en unités du monde,
// autour des pieds du porteur : l'abscisse vers la droite de l'écran,
// l'ordonnée vers le BAS. Deux rangées de cinq, au-dessus de la tête.
inline constexpr std::array<glm::vec2, kChargeCountMost> kChargeCountViewOffsets = {{
    {-7.5F, -23.45F},
    {-3.75F, -23.45F},
    {0.0F, -23.45F},
    {3.75F, -23.45F},
    {7.5F, -23.45F},
    {-7.5F, -20.0F},
    {-3.75F, -20.0F},
    {0.0F, -20.0F},
    {3.75F, -20.0F},
    {7.5F, -20.0F},
}};

}  // namespace ro::render
