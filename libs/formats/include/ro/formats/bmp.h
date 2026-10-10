#pragma once

#include <cstdint>
#include <span>

#include "ro/core/error.h"
#include "ro/formats/image.h"

namespace ro::formats {

// Décode un BMP non compressé, en 8 bits palettisés, 24 ou 32 bits.
//
// Ce sont les seules formes que portent les données du jeu : les 70 textures de
// terrain relevées sur cinq cartes officielles sont toutes des BMP 8 bits
// palettisés, non compressés, en 256 × 256. Les variantes RLE et BITFIELDS sont
// refusées explicitement plutôt que mal interprétées.
//
// ⚠ Le quatrième octet d'un BMP 32 bits n'est PAS un canal alpha : le format ne
// lui en donne un que via BITFIELDS, non gérés ici. Il est ignoré et l'image
// rendue opaque. Le TGA, lui, porte un vrai alpha — voir tga.h.
[[nodiscard]] core::Result<Image> DecodeBmp(std::span<const std::uint8_t> data);

}  // namespace ro::formats
