#include "gif_reader.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace ro::testing {
namespace {

// Un curseur qui refuse de sortir du tampon : un GIF tronqué doit rendre
// « rien », jamais une lecture au-delà.
class Reader {
 public:
  explicit Reader(std::span<const std::uint8_t> bytes) : bytes_(bytes) {}

  [[nodiscard]] bool Has(std::size_t count) const noexcept { return at_ + count <= bytes_.size(); }
  [[nodiscard]] std::uint8_t Byte() noexcept { return bytes_[at_++]; }
  [[nodiscard]] unsigned Word() noexcept {
    const unsigned low = Byte();
    return low | (static_cast<unsigned>(Byte()) << 8);
  }
  [[nodiscard]] std::span<const std::uint8_t> Take(std::size_t count) noexcept {
    const std::span<const std::uint8_t> slice = bytes_.subspan(at_, count);
    at_ += count;
    return slice;
  }
  void Skip(std::size_t count) noexcept { at_ += count; }

 private:
  std::span<const std::uint8_t> bytes_;
  std::size_t at_ = 0;
};

// Recolle les blocs de données d'une suite : chacun porte sa longueur, zéro
// termine.
[[nodiscard]] bool ReadBlocks(Reader& in, std::vector<std::uint8_t>* out) {
  while (true) {
    if (!in.Has(1)) {
      return false;
    }
    const std::size_t length = in.Byte();
    if (length == 0) {
      return true;
    }
    if (!in.Has(length)) {
      return false;
    }
    const std::span<const std::uint8_t> slice = in.Take(length);
    if (out != nullptr) {
      out->insert(out->end(), slice.begin(), slice.end());
    }
  }
}

// Décompression LZW de GIF. Le dictionnaire est reconstruit ici à partir de
// rien : c'est la moitié qui prouve que l'encodeur émet des codes lisibles, y
// compris le cas où un code désigne la suite qu'on est en train de définir.
[[nodiscard]] bool DecodeLzw(std::span<const std::uint8_t> data, int min_code_size,
                             std::size_t wanted, std::vector<std::uint8_t>* out) {
  constexpr int kLargestCodeBits = 12;
  const unsigned clear_code = 1U << min_code_size;
  const unsigned end_code = clear_code + 1;

  std::vector<std::vector<std::uint8_t>> table;
  const auto reset = [&] {
    table.clear();
    table.reserve(1U << kLargestCodeBits);
    for (unsigned code = 0; code < clear_code; ++code) {
      table.push_back({static_cast<std::uint8_t>(code)});
    }
    table.emplace_back();  // clear
    table.emplace_back();  // end
  };
  reset();

  int code_bits = min_code_size + 1;
  unsigned buffer = 0;
  int held = 0;
  std::size_t at = 0;
  bool first = true;
  std::vector<std::uint8_t> previous;

  out->clear();
  while (true) {
    while (held < code_bits) {
      if (at >= data.size()) {
        return false;  // la suite s'arrête avant son code de fin
      }
      buffer |= static_cast<unsigned>(data[at++]) << held;
      held += 8;
    }
    const unsigned code = buffer & ((1U << code_bits) - 1U);
    buffer >>= code_bits;
    held -= code_bits;

    if (code == clear_code) {
      reset();
      code_bits = min_code_size + 1;
      first = true;
      continue;
    }
    if (code == end_code) {
      return out->size() == wanted;
    }

    std::vector<std::uint8_t> piece;
    if (code < table.size() && !(code == clear_code || code == end_code)) {
      piece = table[code];
    } else if (code == table.size() && !first) {
      // Le cas où un code désigne la suite qu'on définit à l'instant même.
      piece = previous;
      piece.push_back(previous.front());
    } else {
      return false;
    }

    out->insert(out->end(), piece.begin(), piece.end());
    if (!first) {
      std::vector<std::uint8_t> grown = previous;
      grown.push_back(piece.front());
      table.push_back(std::move(grown));
      if (table.size() == (std::size_t{1} << code_bits) && code_bits < kLargestCodeBits) {
        ++code_bits;
      }
    }
    previous = std::move(piece);
    first = false;
  }
}

constexpr std::uint8_t kExtensionIntroducer = 0x21;
constexpr std::uint8_t kGraphicControlLabel = 0xF9;
constexpr std::uint8_t kApplicationLabel = 0xFF;
constexpr std::uint8_t kImageSeparator = 0x2C;
constexpr std::uint8_t kTrailer = 0x3B;

}  // namespace

std::optional<GifClip> ReadGif(std::span<const std::uint8_t> bytes) {
  Reader in(bytes);
  if (!in.Has(13)) {
    return std::nullopt;
  }
  const std::span<const std::uint8_t> signature = in.Take(6);
  if (std::memcmp(signature.data(), "GIF89a", 6) != 0) {
    return std::nullopt;
  }

  GifClip clip;
  clip.width = static_cast<int>(in.Word());
  clip.height = static_cast<int>(in.Word());
  const std::uint8_t packed = in.Byte();
  in.Skip(2);  // index du fond, rapport d'aspect

  std::vector<std::uint8_t> table;
  if ((packed & 0x80U) != 0) {
    const std::size_t entries = std::size_t{1} << ((packed & 0x07U) + 1);
    if (!in.Has(entries * 3)) {
      return std::nullopt;
    }
    const std::span<const std::uint8_t> colors = in.Take(entries * 3);
    table.assign(colors.begin(), colors.end());
    for (std::size_t entry = 0; entry < entries; ++entry) {
      if (colors[entry * 3] != 0 || colors[entry * 3 + 1] != 0 || colors[entry * 3 + 2] != 0) {
        ++clip.nonblack_palette_entries;
      }
    }
  }
  if (table.empty()) {
    return std::nullopt;  // nous écrivons toujours une table globale
  }

  formats::Image canvas;
  canvas.width = clip.width;
  canvas.height = clip.height;
  canvas.pixels.assign(
      static_cast<std::size_t>(clip.width) * static_cast<std::size_t>(clip.height) * 4U, 0);

  int delay_centiseconds = 0;
  int disposal = 0;
  int transparent = -1;
  while (in.Has(1)) {
    const std::uint8_t kind = in.Byte();
    if (kind == kTrailer) {
      return clip;
    }
    if (kind == kExtensionIntroducer) {
      if (!in.Has(1)) {
        return std::nullopt;
      }
      const std::uint8_t label = in.Byte();
      if (label == kGraphicControlLabel) {
        if (!in.Has(6)) {
          return std::nullopt;
        }
        in.Skip(1);  // longueur du bloc
        const std::uint8_t flags = in.Byte();
        disposal = (flags >> 2) & 0x07;
        delay_centiseconds = static_cast<int>(in.Word());
        const std::uint8_t index = in.Byte();
        transparent = (flags & 0x01U) != 0 ? static_cast<int>(index) : -1;
        in.Skip(1);  // terminateur
        continue;
      }
      if (label == kApplicationLabel) {
        if (!in.Has(1)) {
          return std::nullopt;
        }
        const std::size_t length = in.Byte();
        if (!in.Has(length)) {
          return std::nullopt;
        }
        const std::span<const std::uint8_t> name = in.Take(length);
        clip.loops_forever =
            clip.loops_forever || (length >= 8 && std::memcmp(name.data(), "NETSCAPE", 8) == 0);
      }
      if (!ReadBlocks(in, nullptr)) {
        return std::nullopt;
      }
      continue;
    }
    if (kind != kImageSeparator) {
      return std::nullopt;
    }

    if (!in.Has(9)) {
      return std::nullopt;
    }
    GifFrame frame;
    frame.left = static_cast<int>(in.Word());
    frame.top = static_cast<int>(in.Word());
    frame.width = static_cast<int>(in.Word());
    frame.height = static_cast<int>(in.Word());
    frame.delay_centiseconds = delay_centiseconds;
    frame.disposal = disposal;
    frame.transparent_index = transparent;
    const std::uint8_t image_flags = in.Byte();
    if ((image_flags & 0x80U) != 0) {
      return std::nullopt;  // table locale : nous n'en écrivons pas
    }
    if (!in.Has(1)) {
      return std::nullopt;
    }
    const int min_code_size = in.Byte();

    std::vector<std::uint8_t> data;
    if (!ReadBlocks(in, &data)) {
      return std::nullopt;
    }
    const auto wanted =
        static_cast<std::size_t>(frame.width) * static_cast<std::size_t>(frame.height);
    std::vector<std::uint8_t> indices;
    if (!DecodeLzw(data, min_code_size, wanted, &indices)) {
      return std::nullopt;
    }

    for (int y = 0; y < frame.height; ++y) {
      for (int x = 0; x < frame.width; ++x) {
        const std::size_t source =
            static_cast<std::size_t>(y) * static_cast<std::size_t>(frame.width) +
            static_cast<std::size_t>(x);
        const int index = indices[source];
        if (index == transparent) {
          continue;  // « laisser en place » : la toile garde son pixel
        }
        const std::size_t target =
            (static_cast<std::size_t>(frame.top + y) * static_cast<std::size_t>(clip.width) +
             static_cast<std::size_t>(frame.left + x)) *
            4U;
        canvas.pixels[target + 0] = table[static_cast<std::size_t>(index) * 3 + 0];
        canvas.pixels[target + 1] = table[static_cast<std::size_t>(index) * 3 + 1];
        canvas.pixels[target + 2] = table[static_cast<std::size_t>(index) * 3 + 2];
        canvas.pixels[target + 3] = 255;
      }
    }
    frame.canvas = canvas;
    // « REMETTRE LE FOND » : le rectangle se vide APRÈS avoir été montré, donc
    // avant l'image suivante.
    if (disposal == kGifRestoreBackground) {
      for (int y = 0; y < frame.height; ++y) {
        const std::size_t row_start =
            (static_cast<std::size_t>(frame.top + y) * static_cast<std::size_t>(clip.width) +
             static_cast<std::size_t>(frame.left)) *
            4U;
        std::fill_n(canvas.pixels.begin() + static_cast<std::ptrdiff_t>(row_start),
                    static_cast<std::ptrdiff_t>(frame.width) * 4, std::uint8_t{0});
      }
    }
    clip.frames.push_back(std::move(frame));
  }
  return std::nullopt;  // pas de marqueur de fin
}

}  // namespace ro::testing
