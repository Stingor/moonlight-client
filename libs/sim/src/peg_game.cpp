#include "ro/sim/peg_game.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace ro::sim {
namespace {

constexpr float kGravity = 950.0F;  // px/s²
constexpr float kLaunchSpeed = 560.0F;
// La vitesse au-delà de laquelle on rabote : repousser la bille hors d'un plot
// peut lui ajouter un rien d'énergie, qui s'accumulerait de rebond en rebond.
constexpr float kMaxSpeed = 1700.0F;
constexpr float kWallRestitution = 0.86F;
constexpr float kPegRestitution = 0.98F;
constexpr float kBucketSpeed = 155.0F;
// Une bille coincée entre deux plots finit par sortir.
constexpr float kBallTimeoutS = 12.0F;
// Le temps rattrapé au plus après un gel : au-delà, il est perdu, plutôt que
// de figer l'image suivante à le rattraper.
constexpr float kMostCatchUpS = 0.25F;
// Le tir part toujours vers le bas : la composante verticale de sa direction
// ne descend pas sous cette valeur.
constexpr float kLowestAimY = 0.15F;
// Une distance plus petite ne donne plus de direction.
constexpr float kTinyLength = 1.0F;

// La grille des plots : décalée d'une demi-maille une rangée sur deux.
constexpr float kPegMarginX = 44.0F;
constexpr float kPegTopY = 148.0F;
constexpr float kPegBottomMargin = 150.0F;
constexpr float kPegSpacingX = 44.0F;
constexpr float kPegSpacingY = 42.0F;
// La part des places laissées vides, pour varier les grilles.
constexpr float kPegGapChance = 0.14F;
// Un plot sur quatre est orange.
constexpr int kOrangeShare = 4;

// Les paliers du multiplicateur : la part d'orange effacée qui le fait monter.
constexpr float kDoubleAtShare = 0.5F;
constexpr float kTripleAtShare = 0.75F;

// Les pas du guide de visée, et un point montré tous les combien.
constexpr int kGuideSteps = 96;
constexpr int kGuideEvery = 3;

}  // namespace

void PegGame::NewGame() {
  std::vector<Peg> pegs;
  std::uniform_real_distribution<float> chance(0.0F, 1.0F);
  int row = 0;
  for (float y = kPegTopY; y <= kPegBoardHeight - kPegBottomMargin; y += kPegSpacingY, ++row) {
    const float shift = (row % 2 == 1) ? kPegSpacingX * 0.5F : 0.0F;
    for (float x = kPegMarginX + shift; x <= kPegBoardWidth - kPegMarginX; x += kPegSpacingX) {
      if (chance(random_) < kPegGapChance) {
        continue;
      }
      pegs.push_back(Peg{.x = x, .y = y});
    }
  }
  // Les plots orange, tirés sans remise.
  std::shuffle(pegs.begin(), pegs.end(), random_);
  const std::size_t oranges = std::max<std::size_t>(1, pegs.size() / kOrangeShare);
  for (std::size_t index = 0; index < oranges && index < pegs.size(); ++index) {
    pegs[index].orange = true;
  }
  SetPegs(std::move(pegs));
}

void PegGame::SetPegs(std::vector<Peg> pegs) {
  pegs_ = std::move(pegs);
  Restart();
}

void PegGame::Restart() {
  orange_total_ = static_cast<int>(std::ranges::count_if(pegs_, &Peg::orange));
  orange_left_ = orange_total_;
  balls_left_ = kPegStartBalls;
  score_ = 0;
  catches_ = 0;
  phase_ = PegPhase::kAiming;
  bucket_x_ = kPegBoardWidth * 0.5F;
  bucket_direction_ = 1.0F;
  pending_s_ = 0.0F;
  news_ = {};
  started_ = true;
}

void PegGame::Aim(PegPoint target) noexcept { aim_ = target; }

PegPoint PegGame::LaunchVelocity() const noexcept {
  float dx = aim_.x - kPegCannonX;
  float dy = aim_.y - kPegCannonY;
  float length = std::sqrt(dx * dx + dy * dy);
  if (length < kTinyLength) {
    dx = 0.0F;
    dy = 1.0F;
    length = 1.0F;
  }
  dx /= length;
  dy /= length;
  if (dy < kLowestAimY) {
    dy = kLowestAimY;
    const float across = std::sqrt(std::max(0.0F, 1.0F - dy * dy));
    dx = dx < 0.0F ? -across : across;
  }
  return PegPoint{.x = dx * kLaunchSpeed, .y = dy * kLaunchSpeed};
}

void PegGame::Launch() {
  if (phase_ != PegPhase::kAiming) {
    return;
  }
  PlaceBall(PegPoint{.x = kPegCannonX, .y = kPegCannonY}, LaunchVelocity());
}

void PegGame::PlaceBall(PegPoint at, PegPoint velocity) {
  ball_ = at;
  velocity_ = velocity;
  ball_time_s_ = 0.0F;
  caught_ = false;
  phase_ = PegPhase::kFlying;
}

int PegGame::multiplier() const noexcept {
  if (orange_total_ == 0) {
    return 1;
  }
  const float cleared =
      static_cast<float>(orange_total_ - orange_left_) / static_cast<float>(orange_total_);
  if (cleared >= kTripleAtShare) {
    return 3;
  }
  return cleared >= kDoubleAtShare ? 2 : 1;
}

void PegGame::EndBall() {
  for (Peg& peg : pegs_) {
    if (peg.lit && !peg.removed) {
      peg.removed = true;
      if (peg.orange && orange_left_ > 0) {
        --orange_left_;
      }
    }
    peg.lit = false;
  }
  if (!caught_) {
    --balls_left_;
  }
  if (orange_left_ <= 0) {
    score_ += balls_left_ * kPegBallLeftPoints;
    phase_ = PegPhase::kWon;
    news_.finished = true;
  } else if (balls_left_ <= 0) {
    phase_ = PegPhase::kLost;
    news_.finished = true;
  } else {
    phase_ = PegPhase::kAiming;
  }
}

void PegGame::Step(float step_s) {
  // Le seau va et vient, quoi qu'il arrive à la bille.
  bucket_x_ += bucket_direction_ * kBucketSpeed * step_s;
  const float half_bucket = kPegBucketWidth * 0.5F;
  if (bucket_x_ < half_bucket) {
    bucket_x_ = half_bucket;
    bucket_direction_ = 1.0F;
  }
  if (bucket_x_ > kPegBoardWidth - half_bucket) {
    bucket_x_ = kPegBoardWidth - half_bucket;
    bucket_direction_ = -1.0F;
  }
  if (phase_ != PegPhase::kFlying) {
    return;
  }

  ball_time_s_ += step_s;
  velocity_.y += kGravity * step_s;
  const float speed2 = velocity_.x * velocity_.x + velocity_.y * velocity_.y;
  if (speed2 > kMaxSpeed * kMaxSpeed) {
    const float scale = kMaxSpeed / std::sqrt(speed2);
    velocity_.x *= scale;
    velocity_.y *= scale;
  }
  ball_.x += velocity_.x * step_s;
  ball_.y += velocity_.y * step_s;

  // Les murs de gauche, de droite et du haut ; le bas est ouvert.
  if (ball_.x < kPegBallRadius) {
    ball_.x = kPegBallRadius;
    velocity_.x = -velocity_.x * kWallRestitution;
  }
  if (ball_.x > kPegBoardWidth - kPegBallRadius) {
    ball_.x = kPegBoardWidth - kPegBallRadius;
    velocity_.x = -velocity_.x * kWallRestitution;
  }
  if (ball_.y < kPegBallRadius) {
    ball_.y = kPegBallRadius;
    velocity_.y = -velocity_.y * kWallRestitution;
  }

  // Les plots : la vitesse se réfléchit sur la normale du contact.
  const float reach = kPegBallRadius + kPegRadius;
  for (Peg& peg : pegs_) {
    if (peg.removed) {
      continue;
    }
    const float dx = ball_.x - peg.x;
    const float dy = ball_.y - peg.y;
    const float distance2 = dx * dx + dy * dy;
    if (distance2 >= reach * reach) {
      continue;
    }
    const float distance = std::max(std::sqrt(distance2), 1e-4F);
    const float nx = dx / distance;
    const float ny = dy / distance;
    ball_.x = peg.x + nx * reach;
    ball_.y = peg.y + ny * reach;
    const float along_normal = velocity_.x * nx + velocity_.y * ny;
    velocity_.x -= (1.0F + kPegRestitution) * along_normal * nx;
    velocity_.y -= (1.0F + kPegRestitution) * along_normal * ny;
    if (!peg.lit) {
      peg.lit = true;
      score_ += (peg.orange ? kPegOrangePoints : kPegBluePoints) * multiplier();
      ++(peg.orange ? news_.orange_hits : news_.blue_hits);
    }
  }

  // Le seau, près du bas : la bille lui revient, une fois par tir.
  if (!caught_ && ball_.y > kPegBucketY - kPegBucketHeight &&
      ball_.y < kPegBucketY + kPegBucketHeight && ball_.x > bucket_x_ - half_bucket &&
      ball_.x < bucket_x_ + half_bucket) {
    caught_ = true;
    ++catches_;
    news_.caught = true;
  }

  if (ball_.y > kPegBoardHeight + kPegBallRadius || ball_time_s_ > kBallTimeoutS) {
    EndBall();
  }
}

void PegGame::Advance(float elapsed_s) {
  pending_s_ = std::min(pending_s_ + std::max(0.0F, elapsed_s), kMostCatchUpS);
  while (pending_s_ >= kPegStepS) {
    Step(kPegStepS);
    pending_s_ -= kPegStepS;
  }
}

PegNews PegGame::TakeNews() noexcept { return std::exchange(news_, PegNews{}); }

std::vector<PegPoint> PegGame::AimGuide() const {
  std::vector<PegPoint> guide;
  if (phase_ != PegPhase::kAiming) {
    return guide;
  }
  PegPoint velocity = LaunchVelocity();
  PegPoint at{.x = kPegCannonX, .y = kPegCannonY};
  for (int step = 0; step < kGuideSteps; ++step) {
    velocity.y += kGravity * kPegStepS;
    at.x += velocity.x * kPegStepS;
    at.y += velocity.y * kPegStepS;
    if (at.x < kPegBallRadius || at.x > kPegBoardWidth - kPegBallRadius) {
      velocity.x = -velocity.x;
    }
    if (at.y > kPegBoardHeight) {
      break;
    }
    if (step % kGuideEvery == 0) {
      guide.push_back(at);
    }
  }
  return guide;
}

}  // namespace ro::sim
