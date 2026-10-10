#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/core/error.h"
#include "ro/formats/image.h"

namespace ro::formats {

// ── UNE IMAGE VENUE DU WEB ──────────────────────────────────────────────────
//
// Les octets qu'un aperçu de lien a téléchargés : les reconnaître, les mesurer
// SANS les décoder, couper un GIF trop long, puis les décoder.
//
// 🔴 CES OCTETS VIENNENT DE N'IMPORTE OÙ, et le décodage pleine taille d'une
// image annoncée géante est une bombe : on mesure d'abord, on refuse avant.
// Trois bombes ne se voient pourtant pas à l'en-tête, et chacune a sa garde :
//   · un flux compressé qui gonfle sans fin — la mémoire du décodeur est
//     bornée pendant tout le décodage ;
//   · un flux LZW de GIF qui émet bien plus de pixels que l'image n'en a —
//     il se rejoue à blanc, code par code, avant tout décodage ;
//   · un JPEG progressif aux passes innombrables — elles se comptent avant.

// LES BORNES. Un côté au plus (celui de `stb_image`), les pixels décodés au
// plus, et les images d'une animation au plus.
inline constexpr int kWebPictureMostSide = 16384;
inline constexpr std::size_t kWebPictureMostDecodedBytes = 64U * 1024U * 1024U;
inline constexpr int kWebPictureMostFrames = 60;
// CE QUE LE DÉCODEUR PEUT TENIR EN MÉMOIRE À LA FOIS, en multiple des pixels
// permis : les octets compressés, le flux décompressé et l'image de sortie
// coexistent un temps.
inline constexpr std::size_t kWebPictureWorkingFactor = 4;
inline constexpr std::size_t kWebPictureMostWorkingBytes =
    kWebPictureWorkingFactor * kWebPictureMostDecodedBytes;

enum class WebPictureFormat : std::uint8_t { kUnknown, kPng, kJpeg, kGif, kBmp, kWebp };
[[nodiscard]] WebPictureFormat SniffWebPicture(std::span<const std::uint8_t> bytes);

struct WebPictureInfo {
  int width = 0;
  int height = 0;
  int frames = 1;
};
// Les dimensions par l'en-tête, et le nombre d'images d'un GIF compté sans
// décoder. Le compte s'arrête à `kWebPictureMostFrames + 1` : au-delà, il dit
// seulement « trop ». Un GIF dont une image émet plus de pixels qu'elle n'en
// a est refusé ici, avec ce qui suit.
[[nodiscard]] core::Result<WebPictureInfo> ProbeWebPicture(std::span<const std::uint8_t> bytes);

// Coupe un GIF après sa n-ième image et pose la fin de flux (0x3B).
[[nodiscard]] std::vector<std::uint8_t> TruncateGifFrames(std::span<const std::uint8_t> gif,
                                                          int most_frames);

struct DecodedWebPicture {
  std::vector<Image> frames;
  std::vector<std::uint32_t> delays_ms;
};
// PNG, JPEG, BMP, et GIF composé image par image — `kWebPictureMostFrames` au
// plus. Le WebP rend `kUnsupportedVersion` : son décodeur n'est pas ici.
//
// 🔴 LA MÉMOIRE DU DÉCODEUR EST BORNÉE à `most_working_bytes` le temps du
// décodage : au-delà, `kInvalidArgument`, sans que l'allocation ait eu lieu.
[[nodiscard]] core::Result<DecodedWebPicture> DecodeWebPicture(
    std::span<const std::uint8_t> bytes,
    std::size_t most_working_bytes = kWebPictureMostWorkingBytes);

}  // namespace ro::formats
