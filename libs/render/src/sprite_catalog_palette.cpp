// Le numéro de fichier d'une palette de corps.

#include <cstdint>

#include "ro/render/sprite_catalog.h"

#include "sprite_catalog_internal.h"

namespace ro::render::catalog_internal {
namespace {

// Les classes transcendantes de PREMIÈRE classe, de High Novice à High
// Merchant : les seules dont la couleur de vêtement 1 se lit dans body_1.pal.
constexpr std::uint32_t kFirstHighFirstClass = 4001;
constexpr std::uint32_t kLastHighFirstClass = 4007;

// La plus petite couleur de vêtement qui désigne son propre fichier.
constexpr std::int16_t kFirstLiteralBodyPalette = 2;

}  // namespace

std::int16_t BodyPaletteFileNumber(const ActorLook& look) {
  if (look.body_palette_names_file || look.body_palette >= kFirstLiteralBodyPalette ||
      WearsAlternateOutfit(look)) {
    return look.body_palette;
  }
  return look.job >= kFirstHighFirstClass && look.job <= kLastHighFirstClass ? 1 : 0;
}

}  // namespace ro::render::catalog_internal
