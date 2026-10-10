#include "ro/formats/png.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <string>

#include <stb_image.h>

namespace ro::formats {
namespace {

// LES HUIT OCTETS QUI OUVRENT TOUT PNG.
constexpr std::array<std::uint8_t, 8> kPngSignature = {0x89, 0x50, 0x4E, 0x47,
                                                       0x0D, 0x0A, 0x1A, 0x0A};

}  // namespace

core::Result<Image> DecodePng(std::span<const std::uint8_t> data) {
  // La taille passe par un `int` : c'est l'interface de `stb_image`. Voir
  // `DecodeJpeg`, qui a la même garde pour la même raison.
  if (data.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return core::Fail(core::ErrorCode::kUnsupportedVersion, "PNG trop grand pour être décodé");
  }
  // 🔴 `stb_image` DEVINE LE FORMAT : sans ce refus, un « .png » de l'archive qui
  // porterait autre chose passerait par un décodeur que rien ne devait exposer.
  if (data.size() < kPngSignature.size() ||
      !std::equal(kPngSignature.begin(), kPngSignature.end(), data.begin())) {
    return core::Fail(core::ErrorCode::kUnsupportedVersion, "PNG illisible : signature absente");
  }

  int width = 0;
  int height = 0;
  int channels = 0;
  stbi_uc* pixels = stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &width,
                                          &height, &channels, 4);
  if (pixels == nullptr || width <= 0 || height <= 0) {
    const char* why = stbi_failure_reason();
    stbi_image_free(pixels);
    return core::Fail(core::ErrorCode::kUnsupportedVersion,
                      std::string("PNG illisible : ") + (why != nullptr ? why : "sans motif"));
  }

  Image image;
  image.width = width;
  image.height = height;
  const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4;
  image.pixels.assign(pixels, pixels + count);
  stbi_image_free(pixels);
  return image;
}

}  // namespace ro::formats
