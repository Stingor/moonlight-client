// LE FICHIER D'UNE VIGNETTE GARDÉE : la clé et l'image font l'aller-retour, un
// échec se garde, et un fichier abîmé se refuse au lieu de s'afficher.

#include "ro/formats/vignette_file.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::core::FileStamp;
using ro::formats::DecodeVignetteFile;
using ro::formats::EncodeVignetteFile;
using ro::formats::Image;
using ro::formats::VignetteFile;
using ro::formats::VignetteKey;

namespace {

constexpr std::int32_t kSidePx = 4;
constexpr std::size_t kChannels = 4;
constexpr std::uint32_t kRevision = 3;
constexpr std::uint64_t kOffset = 123456;
constexpr std::uint64_t kStored = 789;
constexpr std::uint64_t kSize = 2048;

VignetteKey ArchiveKey() {
  return VignetteKey{.model_file = "대만\\대만_절벽1.rsm",
                     .side_px = kSidePx,
                     .render_revision = kRevision,
                     .stamp = FileStamp{.origin = FileStamp::Origin::kArchive,
                                        .container = "data.grf",
                                        .offset = kOffset,
                                        .stored_size = kStored,
                                        .size = kSize}};
}

// Une image en dégradé : chaque octet diffère de son voisin.
Image Gradient() {
  Image image;
  image.width = kSidePx;
  image.height = kSidePx;
  image.pixels.resize(static_cast<std::size_t>(kSidePx * kSidePx) * kChannels);
  for (std::size_t at = 0; at < image.pixels.size(); ++at) {
    image.pixels[at] = static_cast<std::uint8_t>(at);
  }
  return image;
}

}  // namespace

TEST_CASE("Vignette gardée : la clé et l'image font l'aller-retour", "[formats][vignette]") {
  const VignetteFile written{.key = ArchiveKey(), .image = Gradient()};
  const auto read = DecodeVignetteFile(EncodeVignetteFile(written));
  REQUIRE(read.has_value());
  CHECK(read->key == written.key);
  CHECK(read->image.width == kSidePx);
  CHECK(read->image.height == kSidePx);
  CHECK(read->image.pixels == written.image.pixels);
}

TEST_CASE("Vignette gardée : un échec se garde comme une image vide", "[formats][vignette]") {
  VignetteKey key = ArchiveKey();
  key.stamp = FileStamp{
      .origin = FileStamp::Origin::kDisk, .stored_size = kSize, .size = kSize, .written_ticks = -1};
  const auto read = DecodeVignetteFile(EncodeVignetteFile(VignetteFile{.key = key}));
  REQUIRE(read.has_value());
  CHECK(read->key == key);
  CHECK(read->image.empty());
}

TEST_CASE("Vignette gardée : un seul champ de la clé qui diffère la périme",
          "[formats][vignette]") {
  const VignetteKey key = ArchiveKey();
  VignetteKey moved = key;
  moved.stamp.offset += 1;
  CHECK_FALSE(moved == key);
  VignetteKey other_side = key;
  other_side.side_px += 1;
  CHECK_FALSE(other_side == key);
  VignetteKey other_render = key;
  other_render.render_revision += 1;
  CHECK_FALSE(other_render == key);
  VignetteKey other_archive = key;
  other_archive.stamp.container = "moonlight.grf";
  CHECK_FALSE(other_archive == key);
}

TEST_CASE("Vignette gardée : un fichier abîmé se refuse", "[formats][vignette]") {
  const std::vector<std::uint8_t> good =
      EncodeVignetteFile(VignetteFile{.key = ArchiveKey(), .image = Gradient()});

  std::vector<std::uint8_t> signature = good;
  signature[0] = 'X';
  CHECK_FALSE(DecodeVignetteFile(signature).has_value());

  const std::vector<std::uint8_t> truncated(good.begin(), good.end() - 1);
  CHECK_FALSE(DecodeVignetteFile(truncated).has_value());

  // Le dernier octet du flux zlib changé : la somme de contrôle le voit.
  std::vector<std::uint8_t> spoiled = good;
  spoiled.back() ^= 0xFFU;
  CHECK_FALSE(DecodeVignetteFile(spoiled).has_value());

  CHECK_FALSE(DecodeVignetteFile({}).has_value());
}
