#pragma once

#include <cstdint>
#include <span>

#include "ro/core/error.h"
#include "ro/formats/web_picture.h"

namespace moonclient {

// LE DÉCODAGE D'UNE IMAGE WEBP, que `stb` ne lit pas. La première image
// seulement, en RGBA8 non prémultiplié.
//
// - Sous Windows, par le système (Windows Imaging Component). ⚠ LE CODEC PEUT
//   MANQUER — Windows N, LTSC, Server : l'erreur se rend, et l'aperçu se dit
//   indisponible.
// - Sous Linux, par libwebp.
//
// 🔴 LE WEBP SEUL, et sous la garde des images du web
// (`kWebPictureMostSide`, `kWebPictureMostDecodedBytes`) : un en-tête de
// quelques octets peut annoncer un gigaoctet de pixels.
[[nodiscard]] ro::core::Result<ro::formats::DecodedWebPicture> DecodeWebpPicture(
    std::span<const std::uint8_t> bytes);

}  // namespace moonclient
