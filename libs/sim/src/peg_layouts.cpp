// LES PLATEAUX DE ROGGLE : où se posent les plots, et lesquels sont orange,
// verts ou bleus. Voir l'ADR 0139.

#include <algorithm>
#include <cmath>
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

}  // namespace

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
  }
  AssignKinds(pegs, random);
  return pegs;
}

}  // namespace ro::sim
