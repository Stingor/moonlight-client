// LES PLATEAUX DE ROGGLE : où se posent les plots, et lesquels sont orange,
// verts ou bleus, et le plateau que trace l'image d'un objet. Voir les ADR 0139
// et 0140.

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>

#include "ro/sim/peg_game.h"

namespace ro::sim {
namespace {

constexpr float kTau = 2.0F * std::numbers::pi_v<float>;
constexpr float kMiddleX = kPegBoardWidth * 0.5F;

// LA GRILLE ÉPARSE : décalée d'une demi-maille une rangée sur deux, avec des
// places laissées vides.
constexpr float kGridMarginX = 44.0F;
constexpr float kGridTopY = 148.0F;
constexpr float kGridBottomMargin = 150.0F;
constexpr float kGridSpacingX = 44.0F;
constexpr float kGridSpacingY = 42.0F;
constexpr float kGridGapChance = 0.14F;

// LA PYRAMIDE : une rangée de plus à chaque étage.
constexpr int kPyramidRows = 8;
constexpr int kPyramidTopPegs = 3;
constexpr float kPyramidTopY = 160.0F;

// LES ANNEAUX, qui tournent en sens contraires, et une rangée fixe dessous.
constexpr PegPoint kRingsCenter{.x = kMiddleX, .y = 310.0F};
constexpr float kInnerRingRadius = 80.0F;
constexpr int kInnerRingPegs = 12;
constexpr float kInnerRingSpeedRadS = 0.5F;
constexpr float kOuterRingRadius = 150.0F;
constexpr int kOuterRingPegs = 20;
constexpr float kOuterRingSpeedRadS = -0.35F;
constexpr float kRingsFloorY = 495.0F;
constexpr float kRingsFloorMarginX = 64.0F;

// LE PORING : le tour de la tête, deux yeux et un sourire.
constexpr PegPoint kFaceCenter{.x = kMiddleX, .y = 330.0F};
constexpr float kFaceRadius = 160.0F;
constexpr int kFacePegs = 26;
constexpr float kEyeOffsetX = 60.0F;
constexpr float kEyeY = 290.0F;
constexpr float kEyeRadius = 22.0F;
constexpr int kEyePegs = 5;
constexpr PegPoint kSmileCenter{.x = kMiddleX, .y = 340.0F};
constexpr float kSmileRadius = 80.0F;
constexpr int kSmilePegs = 7;
constexpr float kSmileFromRad = std::numbers::pi_v<float> / 6.0F;
constexpr float kSmileToRad = std::numbers::pi_v<float> * 5.0F / 6.0F;

// LES VAGUES : des rangées ondulées, parallèles entre elles.
constexpr int kWaveRows = 7;
constexpr float kWaveTopY = 165.0F;
constexpr float kWaveSpacingY = 50.0F;
constexpr float kWaveSpacingX = 40.0F;
constexpr float kWaveMarginX = 40.0F;
constexpr float kWaveHeight = 14.0F;
constexpr float kWaveLength = 180.0F;

// Un plot sur quatre est orange ; deux sont verts.
constexpr std::size_t kOrangeShare = 4;
constexpr std::size_t kGreenPegs = 2;

void Scattered(std::vector<Peg>& pegs, std::mt19937& random) {
  std::uniform_real_distribution<float> chance(0.0F, 1.0F);
  int row = 0;
  for (float y = kGridTopY; y <= kPegBoardHeight - kGridBottomMargin; y += kGridSpacingY, ++row) {
    const float shift = (row % 2 == 1) ? kGridSpacingX * 0.5F : 0.0F;
    for (float x = kGridMarginX + shift; x <= kPegBoardWidth - kGridMarginX; x += kGridSpacingX) {
      if (chance(random) >= kGridGapChance) {
        pegs.push_back(Peg{.x = x, .y = y});
      }
    }
  }
}

void Pyramid(std::vector<Peg>& pegs) {
  for (int row = 0; row < kPyramidRows; ++row) {
    const int count = kPyramidTopPegs + row;
    const float y = kPyramidTopY + static_cast<float>(row) * kGridSpacingY;
    for (int index = 0; index < count; ++index) {
      const float from_middle = static_cast<float>(index) - static_cast<float>(count - 1) * 0.5F;
      pegs.push_back(Peg{.x = kMiddleX + from_middle * kGridSpacingX, .y = y});
    }
  }
}

void Ring(std::vector<Peg>& pegs, PegPoint center, float radius, int count, float speed_rad_s) {
  for (int index = 0; index < count; ++index) {
    const float angle = kTau * static_cast<float>(index) / static_cast<float>(count);
    pegs.push_back(Peg{
        .x = center.x + std::cos(angle) * radius,
        .y = center.y + std::sin(angle) * radius,
        .orbit = PegOrbit{
            .center = center, .radius = radius, .speed_rad_s = speed_rad_s, .angle_rad = angle}});
  }
}

void Rings(std::vector<Peg>& pegs) {
  Ring(pegs, kRingsCenter, kInnerRingRadius, kInnerRingPegs, kInnerRingSpeedRadS);
  Ring(pegs, kRingsCenter, kOuterRingRadius, kOuterRingPegs, kOuterRingSpeedRadS);
  for (float x = kRingsFloorMarginX; x <= kPegBoardWidth - kRingsFloorMarginX; x += kGridSpacingX) {
    pegs.push_back(Peg{.x = x, .y = kRingsFloorY});
  }
}

void Arc(std::vector<Peg>& pegs, PegPoint center, float radius, int count, float from_rad,
         float to_rad) {
  for (int index = 0; index < count; ++index) {
    const float share = static_cast<float>(index) / static_cast<float>(count - 1);
    const float angle = from_rad + (to_rad - from_rad) * share;
    pegs.push_back(
        Peg{.x = center.x + std::cos(angle) * radius, .y = center.y + std::sin(angle) * radius});
  }
}

void PoringFace(std::vector<Peg>& pegs) {
  // Un tour complet ne répète pas son premier plot.
  const float closing_rad =
      kTau * static_cast<float>(kFacePegs - 1) / static_cast<float>(kFacePegs);
  Arc(pegs, kFaceCenter, kFaceRadius, kFacePegs, 0.0F, closing_rad);
  for (const float side : {-1.0F, 1.0F}) {
    const PegPoint eye{.x = kMiddleX + side * kEyeOffsetX, .y = kEyeY};
    pegs.push_back(Peg{.x = eye.x, .y = eye.y});
    const float eye_closing_rad =
        kTau * static_cast<float>(kEyePegs - 2) / static_cast<float>(kEyePegs - 1);
    Arc(pegs, eye, kEyeRadius, kEyePegs - 1, 0.0F, eye_closing_rad);
  }
  Arc(pegs, kSmileCenter, kSmileRadius, kSmilePegs, kSmileFromRad, kSmileToRad);
}

void Waves(std::vector<Peg>& pegs) {
  for (int row = 0; row < kWaveRows; ++row) {
    const float base_y = kWaveTopY + static_cast<float>(row) * kWaveSpacingY;
    const float shift = (row % 2 == 1) ? kWaveSpacingX * 0.5F : 0.0F;
    for (float x = kWaveMarginX + shift; x <= kPegBoardWidth - kWaveMarginX; x += kWaveSpacingX) {
      pegs.push_back(Peg{.x = x, .y = base_y + std::sin(x * kTau / kWaveLength) * kWaveHeight});
    }
  }
}

// Les sortes, tirées sans remise ; le violet se choisit au début de la partie.
void AssignKinds(std::vector<Peg>& pegs, std::mt19937& random) {
  std::vector<std::size_t> order(pegs.size());
  for (std::size_t index = 0; index < order.size(); ++index) {
    order[index] = index;
  }
  std::shuffle(order.begin(), order.end(), random);
  const std::size_t oranges = std::max<std::size_t>(1, pegs.size() / kOrangeShare);
  const std::size_t greens = std::min(kGreenPegs, pegs.size() - std::min(pegs.size(), oranges));
  std::uniform_int_distribution<int> power(1, kPegPowerCount);
  for (std::size_t rank = 0; rank < order.size(); ++rank) {
    Peg& peg = pegs[order[rank]];
    if (rank < oranges) {
      peg.kind = PegKind::kOrange;
    } else if (rank < oranges + greens) {
      peg.kind = PegKind::kGreen;
      peg.power = static_cast<PegPower>(power(random));
    }
  }
}

// ── LE PLATEAU D'OBJET ──────────────────────────────────────────────────────

// LA ZONE DES PLOTS où l'image se pose : sous le canon, au-dessus du seau.
constexpr float kItemZoneMarginX = 40.0F;
constexpr float kItemZoneTop = 110.0F;
constexpr float kItemZoneBottomMargin = 140.0F;
constexpr float kItemZoneWidth = kPegBoardWidth - 2.0F * kItemZoneMarginX;
constexpr float kItemZoneHeight = kPegBoardHeight - kItemZoneTop - kItemZoneBottomMargin;

// LA MAILLE, en quinconce comme la grille éparse : on part de la plus large, et
// l'on resserre jusqu'à tracer assez de plots. La plus serrée laisse encore
// passer la bille entre deux plots voisins ; la plus large ne pose pas plus
// d'environ soixante-dix plots, même sur un objet qui remplit son cadre.
constexpr float kItemWidestSpacingX = 52.0F;
constexpr float kItemNarrowestSpacingX = 2.0F * (kPegRadius + kPegBallRadius);
constexpr float kItemSpacingStep = 2.0F;
constexpr float kItemRowRatio = kGridSpacingY / kGridSpacingX;
constexpr std::size_t kItemTargetPegs = 60;
// La part de sa case que l'image doit couvrir pour y poser un plot.
constexpr float kItemCoveredShare = 0.45F;
constexpr float kChannelMax = 255.0F;

// Une case de la maille, et ce que l'image y montre.
struct ItemCell {
  PegPoint at;
  float vividness = 0.0F;
};

// LA MOYENNE de l'opacité et de la vivacité sous un rectangle du plateau.
struct CellCover {
  float opacity = 0.0F;
  float vividness = 0.0F;
};

[[nodiscard]] CellCover CoverUnder(const PegPicture& picture, const PegFrame& frame, PegPoint at,
                                   float half_width, float half_height) {
  const float scale = frame.width / static_cast<float>(picture.width);
  const auto pixel_x = [&](float board_x) {
    return std::clamp(static_cast<int>(std::floor((board_x - frame.origin.x) / scale)), 0,
                      picture.width);
  };
  const auto pixel_y = [&](float board_y) {
    return std::clamp(static_cast<int>(std::floor((board_y - frame.origin.y) / scale)), 0,
                      picture.height);
  };
  const int left = pixel_x(at.x - half_width);
  const int right = std::max(left + 1, pixel_x(at.x + half_width));
  const int top = pixel_y(at.y - half_height);
  const int bottom = std::max(top + 1, pixel_y(at.y + half_height));
  float opacity = 0.0F;
  float vividness = 0.0F;
  int counted = 0;
  for (int y = top; y < bottom && y < picture.height; ++y) {
    for (int x = left; x < right && x < picture.width; ++x) {
      const auto index = static_cast<std::size_t>(y) * static_cast<std::size_t>(picture.width) +
                         static_cast<std::size_t>(x);
      opacity += static_cast<float>(picture.opacity[index]);
      vividness += static_cast<float>(picture.vividness[index]);
      ++counted;
    }
  }
  if (counted == 0) {
    return {};
  }
  const float area = static_cast<float>(counted) * kChannelMax;
  return CellCover{.opacity = opacity / area, .vividness = vividness / area};
}

// LES CASES QUE L'IMAGE COUVRE, à cette maille.
[[nodiscard]] std::vector<ItemCell> CoveredCells(const PegPicture& picture, const PegFrame& frame,
                                                 float spacing_x) {
  const float spacing_y = spacing_x * kItemRowRatio;
  const float right = frame.origin.x + frame.width;
  const float bottom = frame.origin.y + frame.height;
  std::vector<ItemCell> cells;
  int row = 0;
  for (float y = frame.origin.y + spacing_y * 0.5F; y < bottom; y += spacing_y, ++row) {
    const float shift = (row % 2 == 1) ? spacing_x * 0.5F : 0.0F;
    for (float x = frame.origin.x + shift; x < right; x += spacing_x) {
      const PegPoint at{.x = x, .y = y};
      const CellCover cover = CoverUnder(picture, frame, at, spacing_x * 0.5F, spacing_y * 0.5F);
      if (cover.opacity >= kItemCoveredShare) {
        cells.push_back(ItemCell{.at = at, .vividness = cover.vividness});
      }
    }
  }
  return cells;
}

// LES SORTES D'UN PLATEAU D'OBJET : l'orange sur les cases les plus vives, le
// hasard départageant les égales ; deux verts au hasard parmi le reste.
std::vector<Peg> KindsOfCells(const std::vector<ItemCell>& cells, std::mt19937& random) {
  std::vector<std::size_t> order(cells.size());
  for (std::size_t index = 0; index < order.size(); ++index) {
    order[index] = index;
  }
  std::shuffle(order.begin(), order.end(), random);
  std::ranges::stable_sort(order, [&](std::size_t left, std::size_t right) {
    return cells[left].vividness > cells[right].vividness;
  });
  const std::size_t oranges = std::max<std::size_t>(1, cells.size() / kOrangeShare);
  std::shuffle(order.begin() + static_cast<std::ptrdiff_t>(oranges), order.end(), random);
  const std::size_t greens = std::min(kGreenPegs, cells.size() - std::min(cells.size(), oranges));
  std::uniform_int_distribution<int> power(1, kPegPowerCount);
  std::vector<Peg> pegs(cells.size());
  for (std::size_t rank = 0; rank < order.size(); ++rank) {
    const ItemCell& cell = cells[order[rank]];
    Peg& peg = pegs[order[rank]];
    peg.x = cell.at.x;
    peg.y = cell.at.y;
    if (rank < oranges) {
      peg.kind = PegKind::kOrange;
    } else if (rank < oranges + greens) {
      peg.kind = PegKind::kGreen;
      peg.power = static_cast<PegPower>(power(random));
    }
  }
  return pegs;
}

[[nodiscard]] bool Readable(const PegPicture& picture) noexcept {
  const auto pixels =
      static_cast<std::size_t>(picture.width) * static_cast<std::size_t>(picture.height);
  return picture.width > 0 && picture.height > 0 && picture.opacity.size() == pixels &&
         picture.vividness.size() == pixels;
}

}  // namespace

PegFrame PegPictureFrame(const PegPicture& picture) noexcept {
  if (!Readable(picture)) {
    return {};
  }
  const float scale = std::min(kItemZoneWidth / static_cast<float>(picture.width),
                               kItemZoneHeight / static_cast<float>(picture.height));
  const float width = static_cast<float>(picture.width) * scale;
  const float height = static_cast<float>(picture.height) * scale;
  return PegFrame{.origin = {.x = kItemZoneMarginX + (kItemZoneWidth - width) * 0.5F,
                             .y = kItemZoneTop + (kItemZoneHeight - height) * 0.5F},
                  .width = width,
                  .height = height};
}

std::vector<Peg> BuildItemPegLayout(const PegPicture& picture, std::mt19937& random) {
  if (!Readable(picture)) {
    return {};
  }
  const PegFrame frame = PegPictureFrame(picture);
  std::vector<ItemCell> cells;
  for (float spacing_x = kItemWidestSpacingX; spacing_x >= kItemNarrowestSpacingX;
       spacing_x -= kItemSpacingStep) {
    cells = CoveredCells(picture, frame, spacing_x);
    if (cells.size() >= kItemTargetPegs) {
      break;
    }
  }
  if (cells.size() < static_cast<std::size_t>(kPegItemMinPegs)) {
    return {};
  }
  return KindsOfCells(cells, random);
}

std::vector<Peg> BuildPegLayout(PegLayout layout, std::mt19937& random) {
  std::vector<Peg> pegs;
  switch (layout) {
    case PegLayout::kScattered:
      Scattered(pegs, random);
      break;
    case PegLayout::kPyramid:
      Pyramid(pegs);
      break;
    case PegLayout::kRings:
      Rings(pegs);
      break;
    case PegLayout::kPoring:
      PoringFace(pegs);
      break;
    case PegLayout::kWaves:
      Waves(pegs);
      break;
    case PegLayout::kItem:
      // Tracé par une image : voir `BuildItemPegLayout`.
      return pegs;
  }
  AssignKinds(pegs, random);
  return pegs;
}

}  // namespace ro::sim
