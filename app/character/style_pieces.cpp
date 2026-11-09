#include "character/style_pieces.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace moonclient {
namespace {

constexpr float kFullChannel = 255.0F;
constexpr float kDegreesPerTurn = 360.0F;
constexpr float kSextant = 60.0F;
// Les écarts des trois accords, en degrés.
constexpr float kAnalogousStep = 30.0F;
constexpr float kComplementaryStep = 180.0F;
constexpr float kTriadicStep = 120.0F;
// UNE PIÈCE NEUTRE : sous ces seuils, saturation ou clarté sur un, on voit du
// gris ou du noir — du métal, du cuir — que l'inspiration ne teint pas.
constexpr float kNeutralSaturation = 0.14F;
constexpr float kNeutralValue = 0.22F;
// La saturation minimale d'une pièce teinte : une pièce délavée qui change de
// teinte sans la montrer ne se verrait pas changer.
constexpr float kInspiredSaturation = 0.45F;

struct Hsv {
  float hue = 0.0F;
  float saturation = 0.0F;
  float value = 0.0F;
};

[[nodiscard]] Hsv ToHsv(const std::array<std::uint8_t, 3>& rgb) {
  const float red = static_cast<float>(rgb[0]) / kFullChannel;
  const float green = static_cast<float>(rgb[1]) / kFullChannel;
  const float blue = static_cast<float>(rgb[2]) / kFullChannel;
  const float high = std::max({red, green, blue});
  const float low = std::min({red, green, blue});
  const float span = high - low;
  Hsv out;
  out.value = high;
  out.saturation = high > 0.0F ? span / high : 0.0F;
  if (span <= 0.0F) {
    return out;
  }
  if (high == red) {
    out.hue = kSextant * std::fmod((green - blue) / span, 6.0F);
  } else if (high == green) {
    out.hue = kSextant * ((blue - red) / span + 2.0F);
  } else {
    out.hue = kSextant * ((red - green) / span + 4.0F);
  }
  if (out.hue < 0.0F) {
    out.hue += kDegreesPerTurn;
  }
  return out;
}

[[nodiscard]] std::uint8_t ToByte(float channel) {
  return static_cast<std::uint8_t>(std::clamp(std::lround(channel * kFullChannel), 0L, 255L));
}

[[nodiscard]] std::array<std::uint8_t, 3> ToRgb(const Hsv& hsv) {
  const float chroma = hsv.value * hsv.saturation;
  const float sector = std::fmod(hsv.hue, kDegreesPerTurn) / kSextant;
  const float second = chroma * (1.0F - std::fabs(std::fmod(sector, 2.0F) - 1.0F));
  const float floor = hsv.value - chroma;
  float red = 0.0F;
  float green = 0.0F;
  float blue = 0.0F;
  switch (static_cast<int>(sector)) {
    case 0:
      red = chroma;
      green = second;
      break;
    case 1:
      red = second;
      green = chroma;
      break;
    case 2:
      green = chroma;
      blue = second;
      break;
    case 3:
      green = second;
      blue = chroma;
      break;
    case 4:
      red = second;
      blue = chroma;
      break;
    default:
      red = chroma;
      blue = second;
      break;
  }
  return {ToByte(red + floor), ToByte(green + floor), ToByte(blue + floor)};
}

// Les teintes de l'accord, autour de celle de départ.
[[nodiscard]] std::vector<float> Hues(Harmony harmony, float start) {
  switch (harmony) {
    case Harmony::kAnalogous:
      return {start, start + kAnalogousStep, start - kAnalogousStep};
    case Harmony::kComplementary:
      return {start, start + kComplementaryStep};
    case Harmony::kTriadic:
      return {start, start + kTriadicStep, start + 2.0F * kTriadicStep};
  }
  return {start};
}

}  // namespace

void PieceMarks::Link(std::size_t from, std::size_t to) {
  if (from >= links_.size() || to >= links_.size() || from == to) {
    return;
  }
  std::uint8_t group = links_[from] != 0 ? links_[from] : links_[to];
  if (group == 0) {
    // Le plus petit numéro libre : il y a au plus quatre groupes de deux.
    group = 1;
    while (std::ranges::find(links_, group) != links_.end()) {
      ++group;
    }
  }
  // ⚠ DEUX GROUPES SE FONDENT : celui de `to` rejoint celui de `from`.
  const std::uint8_t absorbed = links_[to];
  for (std::uint8_t& link : links_) {
    if (absorbed != 0 && link == absorbed) {
      link = group;
    }
  }
  links_[from] = group;
  links_[to] = group;
}

void PieceMarks::Unlink(std::size_t rank) {
  if (rank >= links_.size() || links_[rank] == 0) {
    return;
  }
  const std::uint8_t group = links_[rank];
  links_[rank] = 0;
  const auto left = std::ranges::count(links_, group);
  if (left <= 1) {
    std::ranges::replace(links_, group, std::uint8_t{0});
  }
}

void PieceMarks::ToggleLock(std::size_t rank) {
  if (rank < ro::render::kMaxRamps) {
    locked_ ^= 1U << rank;
  }
}

std::vector<std::size_t> PieceMarks::Reached(std::size_t rank) const {
  std::vector<std::size_t> reached{rank};
  if (rank >= links_.size() || links_[rank] == 0) {
    return reached;
  }
  for (std::size_t other = 0; other < links_.size(); ++other) {
    if (other != rank && links_[other] == links_[rank]) {
      reached.push_back(other);
    }
  }
  return reached;
}

std::vector<std::optional<std::array<std::uint8_t, 3>>> Inspire(
    std::span<const std::array<std::uint8_t, 3>> origin, const PieceMarks& marks, Harmony harmony,
    std::uint32_t seed) {
  std::vector<std::optional<std::array<std::uint8_t, 3>>> out(origin.size());
  std::mt19937 draw(seed);
  std::uniform_real_distribution<float> turn(0.0F, kDegreesPerTurn);
  const std::vector<float> hues = Hues(harmony, turn(draw));
  // 🔴 DES PIÈCES LIÉES PRENNENT LA MÊME TEINTE : c'est ce que le lien dit.
  // ⚠ UN DE PLUS QUE DE PIÈCES : les numéros de groupe partent de un.
  std::array<std::optional<float>, ro::render::kMaxRamps + 1> group_hue{};
  std::size_t next = 0;
  for (std::size_t rank = 0; rank < origin.size() && rank < ro::render::kMaxRamps; ++rank) {
    const Hsv from = ToHsv(origin[rank]);
    if (marks.Locked(rank) || from.saturation < kNeutralSaturation || from.value < kNeutralValue) {
      continue;
    }
    const std::uint8_t group = marks.links()[rank];
    float hue = hues[next % hues.size()];
    if (group != 0 && group_hue[group].has_value()) {
      hue = *group_hue[group];
    } else {
      ++next;
      if (group != 0) {
        group_hue[group] = hue;
      }
    }
    Hsv to = from;
    to.hue = std::fmod(hue + kDegreesPerTurn, kDegreesPerTurn);
    to.saturation = std::max(from.saturation, kInspiredSaturation);
    out[rank] = ToRgb(to);
  }
  return out;
}

}  // namespace moonclient
