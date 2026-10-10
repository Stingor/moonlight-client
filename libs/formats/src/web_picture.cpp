#include "ro/formats/web_picture.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include <stb_image.h>

#include "stb_budget.h"

namespace ro::formats {
namespace {

using detail::StbBudget;

constexpr std::size_t kChannels = 4;

// ── LES SIGNATURES ───────────────────────────────────────────────────────────

constexpr std::array<std::uint8_t, 8> kPngSignature = {0x89, 0x50, 0x4E, 0x47,
                                                       0x0D, 0x0A, 0x1A, 0x0A};
constexpr std::array<std::uint8_t, 3> kJpegSignature = {0xFF, 0xD8, 0xFF};
constexpr std::array<std::uint8_t, 6> kGif87Signature = {'G', 'I', 'F', '8', '7', 'a'};
constexpr std::array<std::uint8_t, 6> kGif89Signature = {'G', 'I', 'F', '8', '9', 'a'};
constexpr std::array<std::uint8_t, 2> kBmpSignature = {'B', 'M'};
constexpr std::array<std::uint8_t, 4> kRiffSignature = {'R', 'I', 'F', 'F'};
constexpr std::array<std::uint8_t, 4> kWebpSignature = {'W', 'E', 'B', 'P'};
constexpr std::size_t kWebpSignatureOffset = 8;

[[nodiscard]] bool StartsWith(std::span<const std::uint8_t> bytes,
                              std::span<const std::uint8_t> signature, std::size_t offset = 0) {
  return bytes.size() >= offset + signature.size() &&
         std::equal(signature.begin(), signature.end(), bytes.subspan(offset).begin());
}

[[nodiscard]] std::uint32_t Le16(std::span<const std::uint8_t> bytes, std::size_t at) {
  return static_cast<std::uint32_t>(bytes[at]) | (static_cast<std::uint32_t>(bytes[at + 1]) << 8U);
}

[[nodiscard]] std::uint32_t Le24(std::span<const std::uint8_t> bytes, std::size_t at) {
  return Le16(bytes, at) | (static_cast<std::uint32_t>(bytes[at + 2]) << 16U);
}

[[nodiscard]] std::uint32_t Le32(std::span<const std::uint8_t> bytes, std::size_t at) {
  return Le24(bytes, at) | (static_cast<std::uint32_t>(bytes[at + 3]) << 24U);
}

// ── L'EN-TÊTE D'UN WEBP ──────────────────────────────────────────────────────
//
// Ses dimensions se lisent sans décodeur, et c'est ce qui permet de refuser une
// bombe AVANT de la passer au système. Trois formes de premier morceau : avec
// perte (« VP8 »), sans perte (« VP8L »), étendue (« VP8X », qui porte la
// toile, animation comprise).

constexpr std::size_t kWebpChunkOffset = 12;
constexpr std::array<std::uint8_t, 4> kWebpLossy = {'V', 'P', '8', ' '};
constexpr std::array<std::uint8_t, 4> kWebpLossless = {'V', 'P', '8', 'L'};
constexpr std::array<std::uint8_t, 4> kWebpExtended = {'V', 'P', '8', 'X'};
// Avec perte : trois octets d'étiquette, le code de départ, puis les côtés sur
// quatorze bits.
constexpr std::size_t kWebpLossyWidthOffset = 26;
constexpr std::size_t kWebpLossyHeightOffset = 28;
constexpr std::uint32_t kWebpLossySideMask = 0x3FFF;
// Sans perte : un octet de signature, puis largeur − 1 et hauteur − 1 sur
// quatorze bits chacune.
constexpr std::size_t kWebpLosslessSizeOffset = 21;
constexpr std::uint32_t kWebpLosslessSideBits = 14;
constexpr std::uint32_t kWebpLosslessSideMask = (1U << kWebpLosslessSideBits) - 1U;
// Étendu : quatre octets de drapeaux, puis largeur − 1 et hauteur − 1 sur
// vingt-quatre bits.
constexpr std::size_t kWebpCanvasWidthOffset = 24;
constexpr std::size_t kWebpCanvasHeightOffset = 27;
constexpr std::size_t kWebpHeaderBytes = 30;

[[nodiscard]] core::Result<WebPictureInfo> ProbeWebp(std::span<const std::uint8_t> bytes) {
  if (bytes.size() < kWebpHeaderBytes) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "WebP : en-tête tronqué");
  }
  WebPictureInfo info;
  if (StartsWith(bytes, kWebpExtended, kWebpChunkOffset)) {
    info.width = static_cast<int>(Le24(bytes, kWebpCanvasWidthOffset) + 1U);
    info.height = static_cast<int>(Le24(bytes, kWebpCanvasHeightOffset) + 1U);
  } else if (StartsWith(bytes, kWebpLossless, kWebpChunkOffset)) {
    const std::uint32_t sides = Le32(bytes, kWebpLosslessSizeOffset);
    info.width = static_cast<int>((sides & kWebpLosslessSideMask) + 1U);
    info.height = static_cast<int>(((sides >> kWebpLosslessSideBits) & kWebpLosslessSideMask) + 1U);
  } else if (StartsWith(bytes, kWebpLossy, kWebpChunkOffset)) {
    info.width = static_cast<int>(Le16(bytes, kWebpLossyWidthOffset) & kWebpLossySideMask);
    info.height = static_cast<int>(Le16(bytes, kWebpLossyHeightOffset) & kWebpLossySideMask);
  } else {
    return core::Fail(core::ErrorCode::kCorruptData, "WebP : premier morceau inconnu");
  }
  // Une animation WebP se montre par sa première image : on n'en compte qu'une.
  info.frames = 1;
  return info;
}

// ── LA CHARPENTE D'UN GIF ────────────────────────────────────────────────────
//
// Le flux se parcourt bloc par bloc sans rien décompresser : c'est ce qui
// compte les images, coupe un GIF trop long, et isole chaque image pour la
// décoder seule.

constexpr std::size_t kGifScreenBytes = 13;
constexpr std::size_t kGifScreenWidthOffset = 6;
constexpr std::size_t kGifScreenHeightOffset = 8;
constexpr std::size_t kGifScreenFlagsOffset = 10;
constexpr std::size_t kGifBackgroundOffset = 11;
constexpr std::uint8_t kGifColorTableFlag = 0x80;
constexpr std::uint8_t kGifColorTableSizeMask = 0x07;
constexpr std::size_t kGifColorBytes = 3;
constexpr std::uint8_t kGifExtension = 0x21;
constexpr std::uint8_t kGifImage = 0x2C;
constexpr std::uint8_t kGifTrailer = 0x3B;
constexpr std::uint8_t kGifControlLabel = 0xF9;
constexpr std::uint8_t kGifControlBytes = 4;
constexpr std::size_t kGifDescriptorBytes = 10;
constexpr std::uint8_t kGifDisposalShift = 2;
constexpr std::uint8_t kGifDisposalMask = 0x07;
constexpr std::uint32_t kMsPerCentisecond = 10;

// Ce que dit la disposition d'une image, une fois montrée.
enum class GifDisposal : std::uint8_t { kKeep, kClear, kRestore };
constexpr std::uint8_t kGifDisposalClear = 2;
constexpr std::uint8_t kGifDisposalRestore = 3;

struct GifFrameSpan {
  // Le bloc de contrôle qui la précède, vide sans lui.
  std::size_t control_begin = 0;
  std::size_t control_end = 0;
  std::size_t image_begin = 0;
  std::size_t image_end = 0;
  int left = 0;
  int top = 0;
  int width = 0;
  int height = 0;
  GifDisposal disposal = GifDisposal::kKeep;
  std::uint32_t delay_ms = 0;
};

struct GifLayout {
  int width = 0;
  int height = 0;
  // La fin de l'en-tête et de la table de couleurs globale.
  std::size_t prefix_end = 0;
  // Les images ENTIÈRES : une image coupée par la fin du flux n'y est pas.
  std::vector<GifFrameSpan> frames;
};

// Saute une suite de sous-blocs, jusqu'au bloc vide qui la ferme. Rend la
// position qui suit, ou rien si le flux s'arrête avant.
[[nodiscard]] std::optional<std::size_t> SkipSubBlocks(std::span<const std::uint8_t> gif,
                                                       std::size_t at) {
  while (at < gif.size()) {
    const std::size_t length = gif[at];
    ++at;
    if (length == 0) {
      return at;
    }
    at += length;
  }
  return std::nullopt;
}

// ── LE FLUX LZW, REJOUÉ À BLANC ──────────────────────────────────────────────
//
// 🔴 UN FLUX LZW PEUT ÉMETTRE BIEN PLUS DE PIXELS QUE SON IMAGE N'EN A : un
// code désigne une chaîne de quatre mille pixels, et huit mégaoctets de codes
// répétés font des milliards de pixels qu'un décodeur parcourt pour les jeter.
// Le flux se rejoue donc ici, code par code, en ne tenant que la LONGUEUR de
// chaque chaîne — un pas par code —, avec les règles du décodeur qui suivra :
// mêmes codes refusés, même croissance de la taille de code.

constexpr std::uint8_t kLzwMostMinimumCodeSize = 12;
// Le dictionnaire au plus, et le dernier code qui fait encore grandir la
// taille de code.
constexpr std::uint32_t kLzwMostCodes = 8192;
constexpr std::uint32_t kLzwLastGrowingCode = 0x0FFF;
// Ce qu'une image peut émettre au-delà de ses pixels : la plus longue chaîne
// qu'un code peut porter, pour un encodeur qui déborde d'un code.
constexpr std::uint64_t kGifSurplusPixels = kLzwMostCodes;
constexpr int kBitsPerByte = 8;

// Rend la position qui suit les données de l'image, ou rien si le flux est
// coupé, illisible, ou émet plus de `frame_pixels` (plus la marge).
[[nodiscard]] std::optional<std::size_t> ReplayLzw(std::span<const std::uint8_t> gif,
                                                   std::size_t at, std::uint64_t frame_pixels) {
  if (at >= gif.size() || gif[at] > kLzwMostMinimumCodeSize) {
    return std::nullopt;
  }
  const std::uint32_t minimum_code_size = gif[at];
  ++at;
  const std::uint32_t clear = 1U << minimum_code_size;
  // La longueur de chaque chaîne du dictionnaire ; les codes de base en font un.
  std::vector<std::uint32_t> lengths(kLzwMostCodes, 1);
  std::uint32_t code_size = minimum_code_size + 1;
  std::uint32_t mask = (1U << code_size) - 1U;
  std::uint32_t avail = clear + 2;
  std::optional<std::uint32_t> previous;
  bool cleared = false;
  std::uint32_t bits = 0;
  std::uint32_t valid_bits = 0;
  std::size_t block_left = 0;
  const std::uint64_t most_pixels = frame_pixels + kGifSurplusPixels;
  std::uint64_t emitted = 0;
  while (true) {
    if (valid_bits < code_size) {
      if (block_left == 0) {
        if (at >= gif.size()) {
          return std::nullopt;
        }
        block_left = gif[at];
        ++at;
        if (block_left == 0) {
          // Les données finissent sans code de fin : l'image s'arrête là.
          return at;
        }
      }
      if (at >= gif.size()) {
        return std::nullopt;
      }
      --block_left;
      bits |= static_cast<std::uint32_t>(gif[at]) << valid_bits;
      ++at;
      valid_bits += kBitsPerByte;
      continue;
    }
    const std::uint32_t code = bits & mask;
    bits >>= code_size;
    valid_bits -= code_size;
    if (code == clear) {
      code_size = minimum_code_size + 1;
      mask = (1U << code_size) - 1U;
      avail = clear + 2;
      previous.reset();
      cleared = true;
      continue;
    }
    if (code == clear + 1) {
      return SkipSubBlocks(gif, at + block_left);
    }
    // Un code avant tout effacement, ou au-delà du dictionnaire : le décodeur
    // le refuserait.
    if (code > avail || !cleared) {
      return std::nullopt;
    }
    if (previous.has_value()) {
      if (avail >= kLzwMostCodes) {
        return std::nullopt;
      }
      lengths[avail] = lengths[*previous] + 1;
      ++avail;
    } else if (code == avail) {
      return std::nullopt;
    }
    emitted += lengths[code];
    if (emitted > most_pixels) {
      return std::nullopt;
    }
    if ((avail & mask) == 0 && avail <= kLzwLastGrowingCode) {
      ++code_size;
      mask = (1U << code_size) - 1U;
    }
    previous = code;
  }
}

[[nodiscard]] GifDisposal DisposalOf(std::uint8_t control_flags) {
  switch ((control_flags >> kGifDisposalShift) & kGifDisposalMask) {
    case kGifDisposalClear:
      return GifDisposal::kClear;
    case kGifDisposalRestore:
      return GifDisposal::kRestore;
    default:
      return GifDisposal::kKeep;
  }
}

// Les images s'arrêtent à `most_frames` : on ne rejoue pas ce qu'on ne
// montrera pas.
[[nodiscard]] core::Result<GifLayout> ReadGifLayout(std::span<const std::uint8_t> gif,
                                                    std::size_t most_frames) {
  if (gif.size() < kGifScreenBytes ||
      (!StartsWith(gif, kGif87Signature) && !StartsWith(gif, kGif89Signature))) {
    return core::Fail(core::ErrorCode::kBadSignature, "GIF : signature absente");
  }
  GifLayout layout;
  layout.width = static_cast<int>(Le16(gif, kGifScreenWidthOffset));
  layout.height = static_cast<int>(Le16(gif, kGifScreenHeightOffset));
  if (layout.width <= 0 || layout.height <= 0) {
    return core::Fail(core::ErrorCode::kCorruptData, "GIF : toile vide");
  }
  std::size_t at = kGifScreenBytes;
  if (const std::uint8_t flags = gif[kGifScreenFlagsOffset]; (flags & kGifColorTableFlag) != 0) {
    at += kGifColorBytes << ((flags & kGifColorTableSizeMask) + 1U);
  }
  if (at > gif.size()) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "GIF : table de couleurs tronquée");
  }
  layout.prefix_end = at;

  GifFrameSpan pending;
  while (at < gif.size() && layout.frames.size() < most_frames) {
    const std::uint8_t tag = gif[at];
    if (tag == kGifTrailer) {
      break;
    }
    if (tag == kGifExtension) {
      if (at + 2 > gif.size()) {
        break;
      }
      const std::size_t begin = at;
      const std::uint8_t label = gif[at + 1];
      const bool control = label == kGifControlLabel &&
                           at + 2 + 1 + kGifControlBytes < gif.size() &&
                           gif[at + 2] == kGifControlBytes;
      const std::optional<std::size_t> end = SkipSubBlocks(gif, at + 2);
      if (!end.has_value()) {
        break;
      }
      if (control) {
        pending.control_begin = begin;
        pending.control_end = *end;
        pending.disposal = DisposalOf(gif[at + 3]);
        pending.delay_ms = Le16(gif, at + 4) * kMsPerCentisecond;
      }
      at = *end;
      continue;
    }
    if (tag != kGifImage || at + kGifDescriptorBytes > gif.size()) {
      // Un bloc inconnu : le reste du flux ne se lit plus, ce qui précède vaut.
      break;
    }
    GifFrameSpan frame = pending;
    pending = GifFrameSpan();
    frame.image_begin = at;
    frame.left = static_cast<int>(Le16(gif, at + 1));
    frame.top = static_cast<int>(Le16(gif, at + 3));
    frame.width = static_cast<int>(Le16(gif, at + 5));
    frame.height = static_cast<int>(Le16(gif, at + 7));
    std::size_t data = at + kGifDescriptorBytes;
    if (const std::uint8_t flags = gif[at + kGifDescriptorBytes - 1];
        (flags & kGifColorTableFlag) != 0) {
      data += kGifColorBytes << ((flags & kGifColorTableSizeMask) + 1U);
    }
    // La taille de code LZW, puis les données, rejouées à blanc. Une image qui
    // émet trop est refusée, avec tout ce qui la suit.
    const std::uint64_t frame_pixels =
        static_cast<std::uint64_t>(frame.width) * static_cast<std::uint64_t>(frame.height);
    const std::optional<std::size_t> end = ReplayLzw(gif, data, frame_pixels);
    if (!end.has_value()) {
      break;
    }
    frame.image_end = *end;
    layout.frames.push_back(frame);
    at = *end;
  }
  if (layout.frames.empty()) {
    return core::Fail(core::ErrorCode::kCorruptData, "GIF : aucune image entière");
  }
  return layout;
}

// ── LE DÉCODAGE ──────────────────────────────────────────────────────────────

[[nodiscard]] std::size_t PixelBytes(int width, int height) {
  return static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * kChannels;
}

// ── LES PASSES D'UN JPEG PROGRESSIF ──────────────────────────────────────────
//
// 🔴 CHAQUE PASSE REPARCOURT TOUTE L'IMAGE : un JPEG de quelques kilo-octets
// peut en aligner des milliers, et le décodeur y passerait des minutes. Les
// passes se comptent donc avant, segment par segment. Un JPEG progressif
// ordinaire en a une dizaine.
constexpr int kJpegMostScans = 32;
constexpr std::uint8_t kJpegMarker = 0xFF;
constexpr std::uint8_t kJpegStuffing = 0x00;
constexpr std::uint8_t kJpegStartOfScan = 0xDA;
constexpr std::uint8_t kJpegEndOfImage = 0xD9;
constexpr std::uint8_t kJpegFirstRestart = 0xD0;
constexpr std::uint8_t kJpegLastRestart = 0xD7;
constexpr std::uint8_t kJpegTemporary = 0x01;
constexpr std::size_t kJpegStartOfImageBytes = 2;

// Un marqueur sans longueur derrière lui.
[[nodiscard]] bool StandaloneJpegMarker(std::uint8_t marker) {
  return marker == kJpegTemporary || (marker >= kJpegFirstRestart && marker <= kJpegLastRestart);
}

// Le nombre de passes, au plus `kJpegMostScans + 1` : au-delà, on a assez vu.
[[nodiscard]] int CountJpegScans(std::span<const std::uint8_t> jpeg) {
  int scans = 0;
  std::size_t at = kJpegStartOfImageBytes;
  while (at + 1 < jpeg.size() && scans <= kJpegMostScans) {
    if (jpeg[at] != kJpegMarker || jpeg[at + 1] == kJpegMarker) {
      ++at;
      continue;
    }
    const std::uint8_t marker = jpeg[at + 1];
    at += 2;
    if (marker == kJpegEndOfImage) {
      break;
    }
    if (StandaloneJpegMarker(marker) || marker == kJpegStuffing) {
      continue;
    }
    if (at + 2 > jpeg.size()) {
      break;
    }
    at += (static_cast<std::size_t>(jpeg[at]) << 8U) | jpeg[at + 1];
    if (marker != kJpegStartOfScan) {
      continue;
    }
    ++scans;
    // Les données de la passe, jusqu'au prochain vrai marqueur.
    while (at + 1 < jpeg.size() && !(jpeg[at] == kJpegMarker && jpeg[at + 1] != kJpegStuffing &&
                                     !StandaloneJpegMarker(jpeg[at + 1]))) {
      ++at;
    }
  }
  return scans;
}

// Décode une image fixe par `stb_image`, en RGBA8, dans la portée bornée
// `budget`.
[[nodiscard]] core::Result<Image> DecodeStill(std::span<const std::uint8_t> bytes, ImageFile file,
                                              const StbBudget& budget) {
  int width = 0;
  int height = 0;
  int channels = 0;
  stbi_uc* pixels = stbi_load_from_memory(bytes.data(), static_cast<int>(bytes.size()), &width,
                                          &height, &channels, static_cast<int>(kChannels));
  if (pixels == nullptr || width <= 0 || height <= 0) {
    const char* why = stbi_failure_reason();
    stbi_image_free(pixels);
    if (budget.exceeded()) {
      return core::Fail(core::ErrorCode::kInvalidArgument,
                        "image web refusée : son décodage dépasse la mémoire permise");
    }
    return core::Fail(core::ErrorCode::kCorruptData, std::string("image web illisible : ") +
                                                         (why != nullptr ? why : "sans motif"));
  }
  Image image;
  image.width = width;
  image.height = height;
  image.file = file;
  image.pixels.assign(pixels, pixels + PixelBytes(width, height));
  stbi_image_free(pixels);
  return image;
}

// UNE IMAGE D'UN GIF, ISOLÉE : l'en-tête, la table globale, son contrôle, son
// bloc, la fin de flux. Le fond est ramené à l'indice zéro, pour que ce que
// l'image ne peint pas reste transparent au lieu de prendre la couleur de fond.
[[nodiscard]] std::vector<std::uint8_t> IsolatedFrame(std::span<const std::uint8_t> gif,
                                                      const GifLayout& layout,
                                                      const GifFrameSpan& frame) {
  std::vector<std::uint8_t> alone(gif.begin(),
                                  gif.begin() + static_cast<std::ptrdiff_t>(layout.prefix_end));
  alone[kGifBackgroundOffset] = 0;
  alone.insert(alone.end(), gif.begin() + static_cast<std::ptrdiff_t>(frame.control_begin),
               gif.begin() + static_cast<std::ptrdiff_t>(frame.control_end));
  alone.insert(alone.end(), gif.begin() + static_cast<std::ptrdiff_t>(frame.image_begin),
               gif.begin() + static_cast<std::ptrdiff_t>(frame.image_end));
  alone.push_back(kGifTrailer);
  return alone;
}

// Efface le rectangle d'une image, borné à la toile.
void ClearRectangle(Image& canvas, const GifFrameSpan& frame) {
  const int right = std::min(canvas.width, frame.left + frame.width);
  const int bottom = std::min(canvas.height, frame.top + frame.height);
  for (int row = frame.top; row < bottom; ++row) {
    for (int column = frame.left; column < right; ++column) {
      const std::size_t at = PixelBytes(canvas.width, row) + PixelBytes(column, 1);
      std::fill_n(canvas.pixels.begin() + static_cast<std::ptrdiff_t>(at), kChannels,
                  std::uint8_t{0});
    }
  }
}

// LE GIF COMPOSÉ, image par image, comme un navigateur le montre.
//
// 🔴 LA COMPOSITION EST LA NÔTRE, pas celle de `stbi_load_gif_from_memory` :
// sous la disposition 3 (« revenir à l'image d'avant »), celle-ci relit la toile
// par un pointeur posé AVANT son tampon, et un GIF venu du web choisit sa
// disposition. Chaque image se décode donc seule, et se pose ici :
//   · un pixel peint recouvre la toile, un pixel transparent la laisse ;
//   · la disposition 2 efface ensuite le rectangle de l'image ;
//   · la disposition 3 rend la toile telle qu'elle était avant l'image.
[[nodiscard]] core::Result<DecodedWebPicture> DecodeGif(std::span<const std::uint8_t> gif,
                                                        const StbBudget& budget) {
  auto layout = ReadGifLayout(gif, static_cast<std::size_t>(kWebPictureMostFrames));
  if (!layout) {
    return std::unexpected(layout.error());
  }
  DecodedWebPicture decoded;
  Image canvas;
  canvas.width = layout->width;
  canvas.height = layout->height;
  canvas.pixels.assign(PixelBytes(canvas.width, canvas.height), 0);
  for (const GifFrameSpan& frame : layout->frames) {
    const std::vector<std::uint8_t> alone = IsolatedFrame(gif, *layout, frame);
    auto layer = DecodeStill(alone, ImageFile::kOther, budget);
    if (!layer) {
      // Une image illisible en cours de flux : ce qui précède se montre. Une
      // mémoire dépassée, elle, refuse tout.
      if (decoded.frames.empty() || budget.exceeded()) {
        return std::unexpected(layer.error());
      }
      break;
    }
    if (layer->width != canvas.width || layer->height != canvas.height) {
      return core::Fail(core::ErrorCode::kCorruptData, "GIF : image hors de la toile");
    }
    const std::vector<std::uint8_t> before =
        frame.disposal == GifDisposal::kRestore ? canvas.pixels : std::vector<std::uint8_t>();
    for (std::size_t at = 0; at < canvas.pixels.size(); at += kChannels) {
      if (layer->pixels[at + kChannels - 1] != 0) {
        std::copy_n(layer->pixels.begin() + static_cast<std::ptrdiff_t>(at), kChannels,
                    canvas.pixels.begin() + static_cast<std::ptrdiff_t>(at));
      }
    }
    decoded.frames.push_back(canvas);
    decoded.delays_ms.push_back(frame.delay_ms);
    if (frame.disposal == GifDisposal::kClear) {
      ClearRectangle(canvas, frame);
    } else if (frame.disposal == GifDisposal::kRestore) {
      canvas.pixels = before;
    }
  }
  return decoded;
}

}  // namespace

WebPictureFormat SniffWebPicture(std::span<const std::uint8_t> bytes) {
  if (StartsWith(bytes, kPngSignature)) {
    return WebPictureFormat::kPng;
  }
  if (StartsWith(bytes, kJpegSignature)) {
    return WebPictureFormat::kJpeg;
  }
  if (StartsWith(bytes, kGif87Signature) || StartsWith(bytes, kGif89Signature)) {
    return WebPictureFormat::kGif;
  }
  if (StartsWith(bytes, kBmpSignature)) {
    return WebPictureFormat::kBmp;
  }
  if (StartsWith(bytes, kRiffSignature) &&
      StartsWith(bytes, kWebpSignature, kWebpSignatureOffset)) {
    return WebPictureFormat::kWebp;
  }
  return WebPictureFormat::kUnknown;
}

core::Result<WebPictureInfo> ProbeWebPicture(std::span<const std::uint8_t> bytes) {
  // La taille passe par un `int` : c'est l'interface de `stb_image`.
  if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "image web trop lourde");
  }
  switch (SniffWebPicture(bytes)) {
    case WebPictureFormat::kUnknown:
      return core::Fail(core::ErrorCode::kBadSignature, "image web : format inconnu");
    case WebPictureFormat::kWebp:
      return ProbeWebp(bytes);
    case WebPictureFormat::kGif: {
      auto layout = ReadGifLayout(bytes, static_cast<std::size_t>(kWebPictureMostFrames) + 1);
      if (!layout) {
        return std::unexpected(layout.error());
      }
      return WebPictureInfo{.width = layout->width,
                            .height = layout->height,
                            .frames = static_cast<int>(layout->frames.size())};
    }
    case WebPictureFormat::kPng:
    case WebPictureFormat::kJpeg:
    case WebPictureFormat::kBmp:
      break;
  }
  WebPictureInfo info;
  int channels = 0;
  const StbBudget budget(kWebPictureMostWorkingBytes);
  if (stbi_info_from_memory(bytes.data(), static_cast<int>(bytes.size()), &info.width, &info.height,
                            &channels) == 0 ||
      info.width <= 0 || info.height <= 0) {
    const char* why = stbi_failure_reason();
    return core::Fail(
        core::ErrorCode::kCorruptData,
        std::string("image web : en-tête illisible, ") + (why != nullptr ? why : "sans motif"));
  }
  return info;
}

std::vector<std::uint8_t> TruncateGifFrames(std::span<const std::uint8_t> gif, int most_frames) {
  if (most_frames <= 0) {
    return {gif.begin(), gif.end()};
  }
  const auto layout = ReadGifLayout(gif, static_cast<std::size_t>(most_frames) + 1);
  if (!layout || layout->frames.size() <= static_cast<std::size_t>(most_frames)) {
    return {gif.begin(), gif.end()};
  }
  const GifFrameSpan& last = layout->frames[static_cast<std::size_t>(most_frames) - 1];
  std::vector<std::uint8_t> truncated(gif.begin(),
                                      gif.begin() + static_cast<std::ptrdiff_t>(last.image_end));
  truncated.push_back(kGifTrailer);
  return truncated;
}

core::Result<DecodedWebPicture> DecodeWebPicture(std::span<const std::uint8_t> bytes,
                                                 std::size_t most_working_bytes) {
  if (bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "image web trop lourde");
  }
  const StbBudget budget(most_working_bytes);
  ImageFile file = ImageFile::kOther;
  switch (SniffWebPicture(bytes)) {
    case WebPictureFormat::kUnknown:
      return core::Fail(core::ErrorCode::kBadSignature, "image web : format inconnu");
    case WebPictureFormat::kWebp:
      return core::Fail(core::ErrorCode::kUnsupportedVersion, "WebP : décodeur absent ici");
    case WebPictureFormat::kGif:
      return DecodeGif(bytes, budget);
    case WebPictureFormat::kPng:
      file = ImageFile::kPng;
      break;
    case WebPictureFormat::kJpeg:
      if (CountJpegScans(bytes) > kJpegMostScans) {
        return core::Fail(core::ErrorCode::kInvalidArgument,
                          "JPEG refusé : trop de passes progressives");
      }
      file = ImageFile::kJpeg;
      break;
    case WebPictureFormat::kBmp:
      file = ImageFile::kBmp;
      break;
  }
  auto still = DecodeStill(bytes, file, budget);
  if (!still) {
    return std::unexpected(still.error());
  }
  DecodedWebPicture decoded;
  decoded.frames.push_back(std::move(*still));
  decoded.delays_ms.push_back(0);
  return decoded;
}

}  // namespace ro::formats
