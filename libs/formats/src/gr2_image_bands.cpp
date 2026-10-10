#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <vector>

#include "ro/core/byte_reader.h"
#include "ro/formats/gr2_arith.h"

#include "gr2_image_planes.h"

namespace ro::formats::gr2 {
namespace {

// L'en-tête d'une couche : la longueur du flux arithmétique, puis celle du
// flux de bits en clair qui le suit.
constexpr std::size_t kPlaneHeaderBytes = 8;

constexpr std::uint32_t kWordBits = 16;
// Le contexte d'un coefficient : le nombre de bits de la magnitude moyenne de
// ses voisins, plafonné.
constexpr std::uint32_t kLargestContext = 15;
constexpr std::uint32_t kNeighbourShift = 2;

// LES PLAGES D'UNE SOUS-BANDE. Un premier modèle dit combien de coefficients
// codés suivent — six bits en clair pour un compte neuf, et quatre comptes
// longs au-delà de 59 ; un second, combien de zéros ensuite — huit bits en
// clair, quatre comptes longs au-delà de 251, et deux de plus que le symbole.
constexpr std::uint32_t kRunCapacity = 64;
constexpr std::uint32_t kRunEscapeBits = 6;
constexpr std::uint32_t kFirstLongRun = 60;
constexpr std::array<std::uint32_t, 4> kLongRuns = {128, 256, 512, 1024};
constexpr std::uint32_t kZeroCapacity = 256;
constexpr std::uint32_t kZeroEscapeBits = 8;
constexpr std::uint32_t kFirstLongZeros = 252;
constexpr std::array<std::uint32_t, 4> kLongZeros = {512, 1024, 2048, 3072};
constexpr std::uint32_t kShortestZeroRun = 2;

std::int16_t Wrap16(std::int32_t value) {
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(value));
}

std::uint32_t MagnitudeContext(std::uint32_t magnitude) {
  return std::min<std::uint32_t>(static_cast<std::uint32_t>(std::bit_width(magnitude)),
                                 kLargestContext);
}

std::uint32_t Magnitude(std::int32_t value) { return static_cast<std::uint32_t>(std::abs(value)); }

// Le décodage d'une couche : les deux flux et le plan qu'ils remplissent.
class BandDecoder {
 public:
  BandDecoder(Gr2RangeDecoder& coded, Gr2BitReader& raw, Gr2Plane& plane)
      : coded_(coded), raw_(raw), samples_(plane.samples) {}

  // La bande la plus grossière : chaque valeur prédite par sa gauche sur la
  // première rangée, puis par la moyenne de sa gauche et du dessus.
  void Coarse(std::size_t at, std::size_t pitch, std::uint32_t width, std::uint32_t height) {
    if (raw_.Bit() != 0) {
      Fill(at, pitch, width, height, Wrap16(static_cast<std::int32_t>(raw_.Bits(kWordBits))));
      return;
    }
    const std::uint32_t top = raw_.Bits(kWordBits);
    Gr2AdaptiveModel model(top + 1);
    const auto delta = [&] { return Signed(Symbol(model, top + 1)); };
    auto value = static_cast<std::int32_t>(raw_.Bits(kWordBits));
    Put(at, value);
    for (std::uint32_t x = 1; x < width; ++x) {
      value += delta();
      Put(at + x, value);
    }
    for (std::uint32_t y = 1; y < height; ++y) {
      const std::size_t row = at + y * pitch;
      const std::size_t above = row - pitch;
      std::int32_t current = delta() + Get(above);
      Put(row, current);
      for (std::uint32_t x = 1; x < width; ++x) {
        const std::int32_t predicted = (current + Get(above + x)) / 2;
        current = delta() + predicted;
        Put(row + x, current);
      }
    }
  }

  // UNE BANDE DE DÉTAILS : des plages de coefficients codés, chacun dans le
  // contexte de ses voisins déjà connus, séparées par des plages de zéros.
  void Detail(std::size_t at, std::size_t pitch, std::uint32_t width, std::uint32_t height) {
    const auto step = static_cast<std::int32_t>(raw_.Bits(kWordBits));
    if (raw_.Bit() != 0) {
      Fill(at, pitch, width, height,
           Wrap16(step * static_cast<std::int32_t>(raw_.Bits(kWordBits))));
      return;
    }
    const std::uint32_t top = raw_.Bits(kWordBits);
    const std::uint32_t context_count =
        MagnitudeContext(static_cast<std::uint32_t>(step) * top) + 1;
    std::vector<Gr2AdaptiveModel> contexts(context_count, Gr2AdaptiveModel(top + 1));
    Gr2AdaptiveModel runs(kRunCapacity);
    Gr2AdaptiveModel zeros(kZeroCapacity);
    const auto coefficient = [&](std::uint32_t context) {
      const std::uint32_t chosen = std::min<std::uint32_t>(context, context_count - 1);
      return step * Signed(Symbol(contexts[chosen], top + 1));
    };

    std::int32_t first = Signed(coded_.Uniform(top + 1));
    first *= step;
    Put(at, first);
    // La fenêtre des voisins : `probe` désigne le voisin du dessus À DROITE
    // de la prochaine valeur, `above` et `above_left` les deux qui le
    // précèdent, `left` la dernière décodée. Sur la première rangée, le
    // « dessus » est la rangée elle-même.
    Window window{at + 1, at, first, first, first, width - 1, height};
    if (width == 1 && !NextRow(window, pitch, width)) {
      return;
    }
    while (true) {
      std::uint32_t run = RawEscaped(runs, kRunEscapeBits);
      if (run >= kFirstLongRun) {
        run = kLongRuns[std::min<std::size_t>(run - kFirstLongRun, kLongRuns.size() - 1)];
      }
      std::uint32_t zero_run = RawEscaped(zeros, kZeroEscapeBits);
      if (zero_run >= kFirstLongZeros) {
        zero_run =
            kLongZeros[std::min<std::size_t>(zero_run - kFirstLongZeros, kLongZeros.size() - 1)];
      }
      if (zero_run != 0) {
        zero_run += kShortestZeroRun;
      }
      while (run != 0) {
        if (window.left_in_row > 1) {
          const std::int32_t ahead = Get(window.probe);
          const std::uint32_t context =
              MagnitudeContext((Magnitude(window.left) + Magnitude(window.above) +
                                Magnitude(window.above_left) + Magnitude(ahead)) >>
                               kNeighbourShift);
          const std::int32_t value = coefficient(context);
          Put(window.write, value);
          window.left = value;
          ++window.write;
          ++window.probe;
          window.above_left = window.above;
          window.above = ahead;
          --run;
          --window.left_in_row;
          continue;
        }
        if (window.left_in_row == 1) {
          const std::uint32_t context =
              MagnitudeContext((Magnitude(window.above) + Magnitude(window.above_left) +
                                Magnitude(2 * window.left)) >>
                               kNeighbourShift);
          Put(window.write, coefficient(context));
          ++window.write;
          --run;
        }
        if (!NextRow(window, pitch, width)) {
          return;
        }
      }
      while (zero_run != 0) {
        if (zero_run < window.left_in_row) {
          for (std::uint32_t index = 0; index < zero_run; ++index) {
            Put(window.write++, 0);
          }
          window.left_in_row -= zero_run;
          window.probe += zero_run;
          window.above = Get(window.probe - 1);
          window.above_left = Get(window.probe - 2);
          window.left = 0;
          zero_run = 0;
        } else {
          zero_run -= window.left_in_row;
          for (std::uint32_t index = 0; index < window.left_in_row; ++index) {
            Put(window.write++, 0);
          }
          if (!NextRow(window, pitch, width)) {
            return;
          }
        }
      }
    }
  }

  // Un drapeau par rangée : un seuil tiré uniformément, puis chaque rangée
  // tombe d'un côté ou de l'autre, dans un modèle fixe.
  void RowFlags(std::vector<std::uint8_t>& flags, std::uint32_t height) {
    flags.assign(height, 0);
    const std::uint32_t threshold = coded_.Uniform(height + 1);
    for (std::uint32_t row = 0; row < height; ++row) {
      if (coded_.Target(height) < threshold) {
        coded_.Narrow(Gr2Interval{0, threshold, height});
      } else {
        flags[row] = 1;
        coded_.Narrow(Gr2Interval{threshold, height - threshold, height});
      }
    }
  }

 private:
  struct Window {
    std::size_t write = 0;
    std::size_t probe = 0;
    std::int32_t above = 0;
    std::int32_t above_left = 0;
    std::int32_t left = 0;
    std::uint32_t left_in_row = 0;
    std::uint32_t rows_left = 0;
  };

  bool NextRow(Window& window, std::size_t pitch, std::uint32_t width) {
    if (--window.rows_left == 0) {
      return false;
    }
    window.write += pitch - width;
    window.probe = window.write - pitch;
    window.above = Get(window.probe);
    ++window.probe;
    window.above_left = window.above;
    window.left = window.above;
    window.left_in_row = width;
    return true;
  }

  std::int32_t Signed(std::uint32_t magnitude) {
    const auto value = static_cast<std::int32_t>(magnitude);
    return value != 0 && raw_.Bit() != 0 ? -value : value;
  }

  std::uint32_t Symbol(Gr2AdaptiveModel& model, std::uint32_t escape_count) {
    const Gr2AdaptiveModel::Decoded decoded = model.Decode(coded_);
    if (!decoded.escaped) {
      return decoded.value;
    }
    const auto value = static_cast<std::uint16_t>(coded_.Uniform(escape_count));
    model.Learn(value);
    return value;
  }

  // Comme `Symbol`, mais la valeur d'un symbole neuf se lit en clair.
  std::uint32_t RawEscaped(Gr2AdaptiveModel& model, std::uint32_t bits) {
    const Gr2AdaptiveModel::Decoded decoded = model.Decode(coded_);
    if (!decoded.escaped) {
      return decoded.value;
    }
    const auto value = static_cast<std::uint16_t>(raw_.Bits(bits));
    model.Learn(value);
    return value;
  }

  void Fill(std::size_t at, std::size_t pitch, std::uint32_t width, std::uint32_t height,
            std::int16_t value) {
    for (std::uint32_t y = 0; y < height; ++y) {
      for (std::uint32_t x = 0; x < width; ++x) {
        Put(at + y * pitch + x, value);
      }
    }
  }

  // Les accès restent dans le plan quoi que dise le flux.
  [[nodiscard]] std::int32_t Get(std::size_t index) const {
    return index < samples_.size() ? samples_[index] : 0;
  }
  void Put(std::size_t index, std::int32_t value) {
    if (index < samples_.size()) {
      samples_[index] = Wrap16(value);
    }
  }

  Gr2RangeDecoder& coded_;
  Gr2BitReader& raw_;
  std::vector<std::int16_t>& samples_;
};

}  // namespace

core::Result<std::size_t> DecodeGr2PlaneBands(std::span<const std::uint8_t> data, Gr2Plane& plane,
                                              std::vector<std::uint8_t>& row_flags) {
  core::ByteReader header(data);
  const std::uint32_t coded_size = header.U32();
  const std::uint32_t raw_size = header.U32();
  const std::size_t total = kPlaneHeaderBytes + std::size_t{coded_size} + raw_size;
  if (!header.ok() || total > data.size()) {
    return core::Fail(core::ErrorCode::kUnexpectedEnd, "texture Granny : couche tronquée");
  }
  // Chaque flux peut lire au-delà de sa longueur annoncée, dans ce qui le suit :
  // ces bits-là ne changent pas le décodage, et on les lit tels quels.
  Gr2RangeDecoder coded(data, kPlaneHeaderBytes);
  Gr2BitReader raw(data, kPlaneHeaderBytes + coded_size);
  plane.samples.assign(std::size_t{plane.width} * plane.height, 0);
  BandDecoder decoder(coded, raw, plane);
  const std::uint32_t width = plane.width;
  const std::uint32_t height = plane.height;
  // La bande grossière : un seizième dans chaque sens, une rangée tous les
  // seize rangs.
  const std::uint32_t coarse_step = 1U << kGr2WaveletLevels;
  decoder.Coarse(0, std::size_t{coarse_step} * width, width >> kGr2WaveletLevels,
                 height >> kGr2WaveletLevels);
  for (std::uint32_t level = kGr2WaveletLevels; level >= 1; --level) {
    const std::uint32_t step = 1U << level;
    const std::size_t pitch = std::size_t{step} * width;
    const std::size_t half_step = std::size_t{step / 2} * width;
    const std::uint32_t band_width = width >> level;
    const std::uint32_t band_height = height >> level;
    decoder.Detail(band_width, pitch, band_width, band_height);
    decoder.Detail(half_step, pitch, band_width, band_height);
    decoder.Detail(half_step + band_width, pitch, band_width, band_height);
  }
  decoder.RowFlags(row_flags, height);
  return total;
}

}  // namespace ro::formats::gr2
