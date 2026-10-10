#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

#include "ro/core/error.h"

namespace ro::formats {

// Une palette RO : 256 couleurs, quatre octets chacune.
constexpr std::size_t kPaletteEntryCount = 256;
constexpr std::size_t kPaletteBytes = kPaletteEntryCount * 4;

// Les 256 couleurs, TELLES QU'ÉCRITES dans le fichier — rouge, vert, bleu, puis
// un quatrième octet.
//
// Aucune convention n'est appliquée ici : c'est un lecteur de format. La règle
// de transparence, elle, appartient au dessin et vit dans ApplyPalette.
struct Palette {
  std::array<std::array<std::uint8_t, 4>, kPaletteEntryCount> entries{};
};

// Décode une palette : les 1024 PREMIERS octets.
//
// Dix des 1612 fichiers .pal de l'installation en font 1028. Les quatre octets
// excédentaires sont en QUEUE et valent zéro — leur premier quadruplet est une
// vraie couleur, pas un en-tête. Lire depuis la fin décalerait donc toutes les
// couleurs d'un cran, ce qui donne des teintes fausses mais plausibles : le
// pire des défauts, puisque rien ne le signale.
[[nodiscard]] core::Result<Palette> DecodePalette(std::span<const std::uint8_t> data);

}  // namespace ro::formats
