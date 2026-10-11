#pragma once

#include <cstdint>
#include <span>

#include "ro/core/error.h"
#include "ro/formats/web_picture.h"

namespace moonclient {

// LE DÉCODAGE D'UNE IMAGE WEBP, que `stb` ne lit pas, par libwebp sur toutes
// les plates-formes. En RGBA8 non prémultiplié ; un WebP animé rend ses images
// recomposées sur le canevas, avec la durée de chacune, comme un GIF.
//
// 🔴 LE WEBP SEUL, et sous la garde des images du web
// (`kWebPictureMostSide`, `kWebPictureMostDecodedBytes`,
// `kWebPictureMostFrames`) : un en-tête de quelques octets peut annoncer un
// gigaoctet de pixels.
[[nodiscard]] ro::core::Result<ro::formats::DecodedWebPicture> DecodeWebpPicture(
    std::span<const std::uint8_t> bytes);

}  // namespace moonclient
