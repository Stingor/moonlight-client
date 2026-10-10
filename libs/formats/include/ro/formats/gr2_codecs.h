#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"

namespace ro::formats {

// LES DEUX COMPRESSIONS D'UN FICHIER GRANNY 2.

// UNE SECTION COMPRESSÉE : un codage par dictionnaire — des octets littéraux et
// des copies de ce qui précède — dont chaque symbole passe par un modèle
// adaptatif et un codage arithmétique.
//
// La sortie se découpe en TROIS PLAGES, bornées par `stops` puis par la
// taille : chacune repart de modèles neufs, réglés par les trois paramètres
// qui ouvrent le flux. Le flux arithmétique, lui, court d'une plage à l'autre.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> DecompressGr2Section(
    std::span<const std::uint8_t> packed, const std::array<std::uint32_t, 2>& stops,
    std::uint32_t unpacked_size);

// Le nombre d'octets de marge que le flux d'une section compressée peut lire
// au-delà de sa fin.
inline constexpr std::uint32_t kGr2SectionReadMargin = 4;

// UNE TEXTURE CODÉE PAR ONDELETTES, rendue en RGBA8, rangée 0 en haut.
//
// Une image d'au plus 256 pixels est rangée brute : trois octets par pixel,
// quatre avec l'alpha. Au-delà, les dimensions s'arrondissent au multiple de
// seize, et chaque couche — luminance, deux chrominances, puis l'alpha — se
// transmet en sous-bandes : quatre niveaux d'ondelettes, dont les
// coefficients passent par des modèles adaptatifs. Sans alpha, il vaut 255.
[[nodiscard]] core::Result<std::vector<std::uint8_t>> DecodeGr2WaveletTexture(
    std::span<const std::uint8_t> data, std::int32_t width, std::int32_t height, bool has_alpha);

// Le plus grand nombre de pixels qu'une image rangée brute peut compter.
inline constexpr std::int32_t kGr2LargestRawTexturePixels = 256;

}  // namespace ro::formats
