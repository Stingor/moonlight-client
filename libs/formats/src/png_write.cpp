// L'ENCODEUR PNG, et le point d'implémentation de `stb_image_write` pour cette
// couche.
//
// 🔴 EN LIAISON STATIQUE : `roc-cli` pose sa propre implémentation, et deux
// unités qui déploient les mêmes symboles publics se heurteraient à l'édition
// de liens. Ici, chaque fonction de la bibliothèque reste interne au fichier.
//
// ⚠ CE FICHIER NE CONTIENT QUE CELA, pour la même raison que
// `stb_image_impl.cpp` : les milliers de lignes de C que la macro déploie ne se
// recompilent pas à chaque unité qui écrit une image.

#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "ro/formats/png.h"

#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif
#include <stb_image_write.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace ro::formats {
namespace {

constexpr int kRgbaChannels = 4;

void AppendTo(void* context, void* data, int size) {
  auto* out = static_cast<std::vector<std::uint8_t>*>(context);
  const auto* bytes = static_cast<const std::uint8_t*>(data);
  out->insert(out->end(), bytes, bytes + size);
}

}  // namespace

core::Result<std::vector<std::uint8_t>> EncodePng(const Image& image) {
  const auto expected = static_cast<std::size_t>(image.width) *
                        static_cast<std::size_t>(image.height) * kRgbaChannels;
  if (image.width <= 0 || image.height <= 0 || image.pixels.size() != expected) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "PNG : image vide ou incohérente");
  }
  // La largeur d'une rangée passe par un `int` : c'est l'interface de la
  // bibliothèque.
  if (image.width > std::numeric_limits<int>::max() / kRgbaChannels) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "PNG : image trop large");
  }
  std::vector<std::uint8_t> encoded;
  if (stbi_write_png_to_func(&AppendTo, &encoded, image.width, image.height, kRgbaChannels,
                             image.pixels.data(), image.width * kRgbaChannels) == 0) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "PNG : encodage refusé");
  }
  return encoded;
}

}  // namespace ro::formats
