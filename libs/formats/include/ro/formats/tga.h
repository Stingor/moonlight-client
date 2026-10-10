#pragma once

#include <cstdint>
#include <span>

#include "ro/core/error.h"
#include "ro/formats/image.h"

namespace ro::formats {

// Décode un TGA.
//
// Le format admet des variantes très différentes ; un recensement sur 4000
// fichiers du client dit lesquelles comptent réellement :
//
//   RGB non compressé  3993      32 bits  3969
//   RGB compressé RLE     5      24 bits    29
//   niveaux de gris       2       8 bits     2
//
// Les trois types sont donc gérés, compressés ou non. Les variantes à palette
// sont refusées explicitement plutôt que mal interprétées : `roc-cli texture
// scan` décode 10 430 des 10 431 .tga de l'installation, et l'unique refus est
// précisément ce cas-là.
//
// ⚠ Contrairement au BMP, le quatrième canal d'un TGA 32 bits est un VRAI
// alpha, et c'est lui qui découpe la végétation et les grilles. L'ignorer
// donnerait des feuillages en blocs rectangulaires.
[[nodiscard]] core::Result<Image> DecodeTga(std::span<const std::uint8_t> data);

}  // namespace ro::formats
