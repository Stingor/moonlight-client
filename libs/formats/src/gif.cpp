#include "ro/formats/gif.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <unordered_map>

namespace ro::formats {
namespace {

// ── LES OCTETS DU FORMAT ────────────────────────────────────────────────────
// Ce sont des valeurs de la spécification GIF89a, pas des choix : elles portent
// leur nom pour qu'une relecture n'ait pas à ouvrir le document.
constexpr std::array<std::uint8_t, 6> kSignature = {'G', 'I', 'F', '8', '9', 'a'};
constexpr std::uint8_t kExtensionIntroducer = 0x21;
constexpr std::uint8_t kGraphicControlLabel = 0xF9;
constexpr std::uint8_t kApplicationLabel = 0xFF;
constexpr std::uint8_t kImageSeparator = 0x2C;
constexpr std::uint8_t kTrailer = 0x3B;
constexpr std::uint8_t kBlockTerminator = 0x00;

// Table globale présente, 8 bits par couleur d'origine, 256 entrées.
constexpr std::uint8_t kGlobalTableFlags = 0xF7;
// Disposition 1 — « laisser en place » —, plus le drapeau de transparence.
// 🔴 C'EST CE COUPLE QUI FAIT L'IMAGE DIFFÉRENTIELLE : sans « laisser en
// place », chaque image effacerait la précédente et les pixels inchangés, qu'on
// a écrits transparents, laisseraient un trou.
constexpr std::uint8_t kLeaveInPlaceAndTransparent = (1 << 2) | 0x01;
// L'index que la différence emploie pour dire « inchangé ».
constexpr std::uint8_t kTransparentIndex = 0;
// Une table de 256 entrées se code sur 8 bits.
constexpr int kGlobalMinCodeSize = 8;
// Un bloc de données ne porte que 255 octets : c'est un compte sur un octet, et
// zéro termine la suite.
constexpr std::size_t kLargestBlock = 255;

// La boucle sans fin, telle que Netscape l'a introduite et que tout le monde lit
// depuis. ⚠ SANS ELLE, LE GIF NE JOUE QU'UNE FOIS.
constexpr std::array<std::uint8_t, 19> kLoopForever = {kExtensionIntroducer,
                                                       kApplicationLabel,
                                                       0x0B,
                                                       'N',
                                                       'E',
                                                       'T',
                                                       'S',
                                                       'C',
                                                       'A',
                                                       'P',
                                                       'E',
                                                       '2',
                                                       '.',
                                                       '0',
                                                       0x03,
                                                       0x01,
                                                       0x00,
                                                       0x00,
                                                       0x00};

// ── L'HISTOGRAMME ───────────────────────────────────────────────────────────
// Les couleurs sont d'abord rangées dans une grille de 5 bits par canal : 32 768
// cases, assez petit pour se parcourir plusieurs fois, assez fin pour que
// l'arrondi ne se voie pas une fois le tout ramené à 255 couleurs.
constexpr int kChannelBits = 5;
constexpr int kChannelSteps = 1 << kChannelBits;  // 32
constexpr int kHistogramCells = kChannelSteps * kChannelSteps * kChannelSteps;
constexpr int kLargestStep = kChannelSteps - 1;

void PutU16(std::vector<std::uint8_t>& out, unsigned value) {
  out.push_back(static_cast<std::uint8_t>(value & 0xFFU));
  out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFU));
}

[[nodiscard]] constexpr std::uint16_t CellOf(std::uint8_t red, std::uint8_t green,
                                             std::uint8_t blue) noexcept {
  const unsigned shift = 8 - kChannelBits;
  return static_cast<std::uint16_t>(((red >> shift) << (2 * kChannelBits)) |
                                    ((green >> shift) << kChannelBits) | (blue >> shift));
}

[[nodiscard]] constexpr int CellRed(std::uint16_t cell) noexcept {
  return (cell >> (2 * kChannelBits)) & kLargestStep;
}
[[nodiscard]] constexpr int CellGreen(std::uint16_t cell) noexcept {
  return (cell >> kChannelBits) & kLargestStep;
}
[[nodiscard]] constexpr int CellBlue(std::uint16_t cell) noexcept { return cell & kLargestStep; }

// 5 bits vers 8, en recopiant les bits de poids fort dans les faibles.
//
// ⚠ SANS CETTE RECOPIE, TOUTE L'IMAGE S'ASSOMBRIT : un canal plein vaudrait 248
// au lieu de 255, et le blanc deviendrait gris.
[[nodiscard]] constexpr int StepToByte(int step) noexcept {
  return (step << (8 - kChannelBits)) | (step >> (2 * kChannelBits - 8));
}

// ── L'ÉCRITURE DES CODES ────────────────────────────────────────────────────
//
// Les codes LZW sortent bit de poids FAIBLE en tête, et la suite se découpe en
// blocs d'au plus 255 octets précédés de leur longueur.
class CodeWriter {
 public:
  explicit CodeWriter(std::vector<std::uint8_t>& out) : out_(out) {}

  void Write(unsigned code, int bits) {
    buffer_ |= code << held_;
    held_ += bits;
    while (held_ >= 8) {
      PushByte(static_cast<std::uint8_t>(buffer_ & 0xFFU));
      buffer_ >>= 8;
      held_ -= 8;
    }
  }

  // Les bits qui restent, le bloc entamé, puis le terminateur.
  void Finish() {
    if (held_ > 0) {
      PushByte(static_cast<std::uint8_t>(buffer_ & 0xFFU));
      buffer_ = 0;
      held_ = 0;
    }
    if (!block_.empty()) {
      out_.push_back(static_cast<std::uint8_t>(block_.size()));
      out_.insert(out_.end(), block_.begin(), block_.end());
      block_.clear();
    }
    out_.push_back(kBlockTerminator);
  }

 private:
  void PushByte(std::uint8_t value) {
    block_.push_back(value);
    if (block_.size() == kLargestBlock) {
      out_.push_back(static_cast<std::uint8_t>(kLargestBlock));
      out_.insert(out_.end(), block_.begin(), block_.end());
      block_.clear();
    }
  }

  std::vector<std::uint8_t>& out_;
  std::vector<std::uint8_t> block_;
  unsigned buffer_ = 0;
  int held_ = 0;
};

// LZW de GIF, dans sa forme standard.
//
// Le dictionnaire est indexé par (préfixe << 8 | octet) — un index de palette
// tient sur un octet, donc la clé tient sur un entier. La largeur de code grandit
// quand le code qu'on vient d'attribuer atteint sa puissance de deux, et le
// dictionnaire se vide à 4095, juste avant le plafond de douze bits.
void EmitLzw(CodeWriter& writer, std::span<const std::uint8_t> indices, int min_code_size) {
  constexpr int kLargestCodeBits = 12;
  constexpr int kDictionaryFull = 4095;
  const unsigned clear_code = 1U << min_code_size;
  const unsigned end_code = clear_code + 1;
  int code_bits = min_code_size + 1;
  int next_code = static_cast<int>(end_code) + 1;
  std::unordered_map<int, int> dictionary;
  dictionary.reserve(kDictionaryFull);

  writer.Write(clear_code, code_bits);
  if (indices.empty()) {
    writer.Write(end_code, code_bits);
    writer.Finish();
    return;
  }

  int current = indices[0];
  for (std::size_t at = 1; at < indices.size(); ++at) {
    const int next = indices[at];
    const int key = (current << 8) | next;
    const auto found = dictionary.find(key);
    if (found != dictionary.end()) {
      current = found->second;
      continue;
    }
    writer.Write(static_cast<unsigned>(current), code_bits);
    dictionary.emplace(key, next_code);
    if (next_code == (1 << code_bits) && code_bits < kLargestCodeBits) {
      ++code_bits;
    }
    if (next_code == kDictionaryFull) {
      writer.Write(clear_code, code_bits);
      dictionary.clear();
      code_bits = min_code_size + 1;
      next_code = static_cast<int>(end_code) + 1;
    } else {
      ++next_code;
    }
    current = next;
  }
  writer.Write(static_cast<unsigned>(current), code_bits);
  writer.Write(end_code, code_bits);
  writer.Finish();
}

// ── LA COUPE MÉDIANE ────────────────────────────────────────────────────────

// Une case de l'histogramme où au moins un pixel est tombé.
struct Bucket {
  std::uint16_t cell = 0;
  std::uint32_t pixels = 0;
};

// Un intervalle de cases, et l'étendue des couleurs qu'il couvre.
struct Box {
  int begin = 0;
  int end = 0;
  std::uint64_t pixels = 0;
  int red_low = 0, red_high = 0;
  int green_low = 0, green_high = 0;
  int blue_low = 0, blue_high = 0;
};

void Measure(Box& box, std::span<const Bucket> buckets) {
  box.red_low = box.green_low = box.blue_low = kLargestStep;
  box.red_high = box.green_high = box.blue_high = 0;
  box.pixels = 0;
  for (int at = box.begin; at < box.end; ++at) {
    const std::uint16_t cell = buckets[static_cast<std::size_t>(at)].cell;
    box.red_low = std::min(box.red_low, CellRed(cell));
    box.red_high = std::max(box.red_high, CellRed(cell));
    box.green_low = std::min(box.green_low, CellGreen(cell));
    box.green_high = std::max(box.green_high, CellGreen(cell));
    box.blue_low = std::min(box.blue_low, CellBlue(cell));
    box.blue_high = std::max(box.blue_high, CellBlue(cell));
    box.pixels += buckets[static_cast<std::size_t>(at)].pixels;
  }
}

[[nodiscard]] int Spread(const Box& box) noexcept {
  return std::max(
      {box.red_high - box.red_low, box.green_high - box.green_low, box.blue_high - box.blue_low});
}

[[nodiscard]] int StepOf(std::uint16_t cell, int channel) noexcept {
  if (channel == 0) {
    return CellRed(cell);
  }
  return channel == 1 ? CellGreen(cell) : CellBlue(cell);
}

// La coupe médiane de Heckbert : on scinde la boîte dont les couleurs s'étalent
// le plus, à la médiane de sa POPULATION de pixels et non de son nombre de
// cases — une couleur qui couvre la moitié de l'écran mérite ses entrées, une
// poignée de pixels égarés n'en mérite pas.
//
// ⚠ ELLE S'ARRÊTE QUAND PLUS RIEN NE SE COUPE : un clip de quarante couleurs
// distinctes obtient quarante entrées, et aucune entrée vide.
void MedianCut(std::vector<Bucket>& buckets, std::vector<Box>& boxes, int wanted) {
  boxes.clear();
  Box whole{.begin = 0, .end = static_cast<int>(buckets.size())};
  Measure(whole, buckets);
  boxes.push_back(whole);

  while (static_cast<int>(boxes.size()) < wanted) {
    int chosen = -1;
    int widest = 0;
    std::uint64_t most = 0;
    for (std::size_t at = 0; at < boxes.size(); ++at) {
      const Box& box = boxes[at];
      if (box.end - box.begin < 2) {
        continue;  // une seule case : plus rien à couper
      }
      const int spread = Spread(box);
      if (spread <= 0) {
        continue;  // une seule couleur
      }
      if (spread > widest || (spread == widest && box.pixels > most)) {
        chosen = static_cast<int>(at);
        widest = spread;
        most = box.pixels;
      }
    }
    if (chosen < 0) {
      break;
    }

    Box& box = boxes[static_cast<std::size_t>(chosen)];
    const int red_span = box.red_high - box.red_low;
    const int green_span = box.green_high - box.green_low;
    const int blue_span = box.blue_high - box.blue_low;
    const int channel = (red_span >= green_span && red_span >= blue_span) ? 0
                        : (green_span >= blue_span)                       ? 1
                                                                          : 2;
    std::sort(buckets.begin() + box.begin, buckets.begin() + box.end,
              [channel](const Bucket& left, const Bucket& right) {
                return StepOf(left.cell, channel) < StepOf(right.cell, channel);
              });

    const std::uint64_t half = box.pixels / 2;
    std::uint64_t seen = 0;
    int split = box.begin;
    for (int at = box.begin; at < box.end - 1; ++at) {
      seen += buckets[static_cast<std::size_t>(at)].pixels;
      split = at + 1;
      if (seen >= half) {
        break;
      }
    }
    // Les deux moitiés doivent être non vides, sinon la boucle tournerait sur la
    // même boîte sans jamais la réduire.
    split = std::clamp(split, box.begin + 1, box.end - 1);

    Box low{.begin = box.begin, .end = split};
    Box high{.begin = split, .end = box.end};
    Measure(low, buckets);
    Measure(high, buckets);
    boxes[static_cast<std::size_t>(chosen)] = low;
    boxes.push_back(high);
  }
}

// La table globale, et le chemin direct d'une case vers son entrée.
struct Palette {
  std::array<std::uint8_t, 256 * 3> colors = {};
  std::vector<std::uint8_t> of_cell;
};

[[nodiscard]] Palette BuildPalette(std::span<const Image> frames) {
  std::vector<std::uint32_t> histogram(kHistogramCells, 0);
  for (const Image& frame : frames) {
    for (std::size_t at = 0; at + 3 < frame.pixels.size(); at += 4) {
      ++histogram[CellOf(frame.pixels[at], frame.pixels[at + 1], frame.pixels[at + 2])];
    }
  }

  std::vector<Bucket> buckets;
  for (int cell = 0; cell < kHistogramCells; ++cell) {
    if (histogram[static_cast<std::size_t>(cell)] != 0) {
      buckets.push_back(Bucket{.cell = static_cast<std::uint16_t>(cell),
                               .pixels = histogram[static_cast<std::size_t>(cell)]});
    }
  }

  std::vector<Box> boxes;
  MedianCut(buckets, boxes, kGifPictureColors);

  Palette palette;
  palette.of_cell.assign(kHistogramCells, kTransparentIndex);
  for (std::size_t at = 0; at < boxes.size(); ++at) {
    const Box& box = boxes[at];
    // ⚠ LES ENTRÉES COMMENCENT À 1 : la zéro appartient à la transparence, donc
    // à la différence entre deux images.
    const auto index = static_cast<std::uint8_t>(at + 1);
    std::uint64_t red = 0;
    std::uint64_t green = 0;
    std::uint64_t blue = 0;
    std::uint64_t seen = 0;
    for (int bucket = box.begin; bucket < box.end; ++bucket) {
      const Bucket& one = buckets[static_cast<std::size_t>(bucket)];
      red += static_cast<std::uint64_t>(StepToByte(CellRed(one.cell))) * one.pixels;
      green += static_cast<std::uint64_t>(StepToByte(CellGreen(one.cell))) * one.pixels;
      blue += static_cast<std::uint64_t>(StepToByte(CellBlue(one.cell))) * one.pixels;
      seen += one.pixels;
      // Chaque case appartient à UNE SEULE boîte : le chemin d'un pixel vers sa
      // couleur est donc une lecture de table, et non une recherche de la plus
      // proche au cœur de la boucle.
      palette.of_cell[one.cell] = index;
    }
    seen = std::max<std::uint64_t>(seen, 1);
    palette.colors[static_cast<std::size_t>(index) * 3 + 0] = static_cast<std::uint8_t>(red / seen);
    palette.colors[static_cast<std::size_t>(index) * 3 + 1] =
        static_cast<std::uint8_t>(green / seen);
    palette.colors[static_cast<std::size_t>(index) * 3 + 2] =
        static_cast<std::uint8_t>(blue / seen);
  }
  return palette;
}

// La boîte de ce qui a bougé depuis l'image précédente.
struct Changed {
  int left = 0;
  int top = 0;
  int width = 0;
  int height = 0;
};

[[nodiscard]] Changed WhatMoved(std::span<const std::uint8_t> now,
                                std::span<const std::uint8_t> before, int width, int height) {
  int left = width;
  int top = height;
  int right = -1;
  int bottom = -1;
  for (int y = 0; y < height; ++y) {
    const std::size_t row = static_cast<std::size_t>(y) * static_cast<std::size_t>(width);
    for (int x = 0; x < width; ++x) {
      if (now[row + static_cast<std::size_t>(x)] == before[row + static_cast<std::size_t>(x)]) {
        continue;
      }
      left = std::min(left, x);
      right = std::max(right, x);
      top = std::min(top, y);
      bottom = std::max(bottom, y);
    }
  }
  // ⚠ RIEN N'A BOUGÉ : on écrit malgré tout une image d'un pixel sur un pixel.
  // Elle coûte une douzaine d'octets et BRÛLE SON DÉLAI — la sauter avancerait
  // le reste du clip, et une pause dans la scène deviendrait un saut.
  if (right < 0) {
    return Changed{.left = 0, .top = 0, .width = 1, .height = 1};
  }
  return Changed{.left = left, .top = top, .width = right - left + 1, .height = bottom - top + 1};
}

}  // namespace

core::Result<std::vector<std::uint8_t>> EncodeAnimatedGif(std::span<const Image> frames,
                                                          int delay_centiseconds) {
  if (frames.empty()) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "aucune image à encoder");
  }
  const int width = frames.front().width;
  const int height = frames.front().height;
  if (width <= 0 || height <= 0) {
    return core::Fail(core::ErrorCode::kInvalidArgument, "image de taille nulle");
  }
  const auto expected = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 4U;
  for (const Image& frame : frames) {
    if (frame.width != width || frame.height != height || frame.pixels.size() != expected) {
      return core::Fail(core::ErrorCode::kInvalidArgument,
                        "les images d'un GIF ont toutes la même taille");
    }
  }

  const Palette palette = BuildPalette(frames);
  const int delay = std::max(delay_centiseconds, 0);

  std::vector<std::uint8_t> out;
  out.insert(out.end(), kSignature.begin(), kSignature.end());
  PutU16(out, static_cast<unsigned>(width));
  PutU16(out, static_cast<unsigned>(height));
  out.push_back(kGlobalTableFlags);
  out.push_back(0x00);  // index du fond
  out.push_back(0x00);  // rapport d'aspect du pixel
  out.insert(out.end(), palette.colors.begin(), palette.colors.end());
  out.insert(out.end(), kLoopForever.begin(), kLoopForever.end());

  const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  std::vector<std::uint8_t> now(count);
  std::vector<std::uint8_t> before(count);
  std::vector<std::uint8_t> piece;
  for (std::size_t number = 0; number < frames.size(); ++number) {
    const Image& frame = frames[number];
    for (std::size_t at = 0; at < count; ++at) {
      now[at] = palette.of_cell[CellOf(frame.pixels[at * 4], frame.pixels[at * 4 + 1],
                                       frame.pixels[at * 4 + 2])];
    }

    // La première image porte le tableau entier : il n'y a rien dessous.
    const Changed moved = number == 0
                              ? Changed{.left = 0, .top = 0, .width = width, .height = height}
                              : WhatMoved(now, before, width, height);

    piece.resize(static_cast<std::size_t>(moved.width) * static_cast<std::size_t>(moved.height));
    for (int y = 0; y < moved.height; ++y) {
      const std::size_t row =
          static_cast<std::size_t>(moved.top + y) * static_cast<std::size_t>(width) +
          static_cast<std::size_t>(moved.left);
      for (int x = 0; x < moved.width; ++x) {
        const std::uint8_t index = now[row + static_cast<std::size_t>(x)];
        const bool same = number > 0 && index == before[row + static_cast<std::size_t>(x)];
        piece[static_cast<std::size_t>(y) * static_cast<std::size_t>(moved.width) +
              static_cast<std::size_t>(x)] = same ? kTransparentIndex : index;
      }
    }

    out.push_back(kExtensionIntroducer);
    out.push_back(kGraphicControlLabel);
    out.push_back(0x04);  // longueur du bloc de contrôle
    out.push_back(kLeaveInPlaceAndTransparent);
    PutU16(out, static_cast<unsigned>(delay));
    out.push_back(kTransparentIndex);
    out.push_back(kBlockTerminator);

    out.push_back(kImageSeparator);
    PutU16(out, static_cast<unsigned>(moved.left));
    PutU16(out, static_cast<unsigned>(moved.top));
    PutU16(out, static_cast<unsigned>(moved.width));
    PutU16(out, static_cast<unsigned>(moved.height));
    out.push_back(0x00);  // pas de table locale : la globale couvre tout le clip
    out.push_back(kGlobalMinCodeSize);
    CodeWriter writer(out);
    EmitLzw(writer, piece, kGlobalMinCodeSize);

    before.swap(now);
  }

  out.push_back(kTrailer);
  return out;
}

}  // namespace ro::formats
