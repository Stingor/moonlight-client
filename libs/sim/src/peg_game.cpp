#include "ro/sim/peg_game.h"

#include <algorithm>
#include <cmath>
#include <numbers>
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
// Le dernier recours contre une bille qui ne sort pas.
constexpr float kShotTimeoutS = 12.0F;
// UNE BILLE COINCÉE : ni descendue de cette hauteur sous son point le plus bas,
// ni posée sur un plot neuf pendant ce temps, elle efface les plots allumés à
// portée de ce rayon.
constexpr float kStuckProgressPx = 24.0F;
constexpr float kStuckS = 2.0F;
constexpr float kUnstuckReach = kPegBallRadius + kPegRadius + 30.0F;
// Le temps rattrapé au plus après un gel : au-delà, il est perdu, plutôt que
// de figer l'image suivante à le rattraper.
constexpr float kMostCatchUpS = 0.25F;
// Le tir part toujours vers le bas : la composante verticale de sa direction
// ne descend pas sous cette valeur.
constexpr float kLowestAimY = 0.15F;
// Une distance plus petite ne donne plus de direction.
constexpr float kTinyLength = 1.0F;
// UN CRAN DE VISÉE FINE, et la distance du canon où se pose le point visé.
constexpr float kAimNudgeRad = 0.25F * std::numbers::pi_v<float> / 180.0F;
constexpr float kAimNudgeReach = 200.0F;
// Double Strafe lance la seconde bille en miroir ; une bille qui tombe droit
// reçoit cette poussée de côté.
constexpr float kStrafeKick = 160.0F;

// Les pas du guide de visée, et un point montré tous les combien.
constexpr int kGuideSteps = 144;
constexpr int kGuideEvery = 3;

[[nodiscard]] int PointsOf(PegKind kind) noexcept {
  switch (kind) {
    case PegKind::kOrange:
      return kPegOrangePoints;
    case PegKind::kPurple:
      return kPegPurplePoints;
    case PegKind::kGreen:
      return kPegGreenPoints;
    case PegKind::kBlue:
      return kPegBluePoints;
  }
  return kPegBluePoints;
}

[[nodiscard]] PegCalloutKind CalloutOf(PegPower power) noexcept {
  switch (power) {
    case PegPower::kMagnumBreak:
      return PegCalloutKind::kMagnumBreak;
    case PegPower::kSafetyWall:
      return PegCalloutKind::kSafetyWall;
    case PegPower::kDoubleStrafe:
      return PegCalloutKind::kDoubleStrafe;
    case PegPower::kNone:
      break;
  }
  return PegCalloutKind::kPoints;
}

[[nodiscard]] float DistanceSquared(PegPoint from, PegPoint to) noexcept {
  const float dx = to.x - from.x;
  const float dy = to.y - from.y;
  return dx * dx + dy * dy;
}

}  // namespace

bool BouncePegOnWalls(PegPoint& at, PegPoint& velocity) noexcept {
  bool touched = false;
  if (at.x < kPegBallRadius) {
    at.x = kPegBallRadius;
    velocity.x = -velocity.x * kWallRestitution;
    touched = true;
  }
  if (at.x > kPegBoardWidth - kPegBallRadius) {
    at.x = kPegBoardWidth - kPegBallRadius;
    velocity.x = -velocity.x * kWallRestitution;
    touched = true;
  }
  if (at.y < kPegBallRadius) {
    at.y = kPegBallRadius;
    velocity.y = -velocity.y * kWallRestitution;
    touched = true;
  }
  return touched;
}

void PegGame::NewGame() {
  std::uniform_int_distribution<int> pick(0, kPegLayoutCount - 1);
  NewGame(static_cast<PegLayout>(pick(seeds_)));
}

void PegGame::NewGame(PegLayout layout) {
  random_.seed(seeds_());
  StartBoard(layout, false, 0);
}

void PegGame::NewDailyGame(std::uint32_t day) {
  random_.seed(day);
  StartBoard(static_cast<PegLayout>(day % kPegLayoutCount), true, day);
}

void PegGame::StartBoard(PegLayout layout, bool daily, std::uint32_t day) {
  SetPegs(BuildPegLayout(layout, random_));
  layout_ = layout;
  daily_ = daily;
  day_ = day;
  OpenBoard();
}

void PegGame::OpenBoard() {
  purple_rotates_ = true;
  ChoosePurple();
  opening_pegs_ = pegs_;
  opening_random_ = random_;
}

void PegGame::Replay() {
  if (opening_pegs_.empty()) {
    return;
  }
  const PegLayout layout = layout_;
  const bool daily = daily_;
  const std::uint32_t day = day_;
  const std::uint32_t item_id = item_id_;
  const PegFrame picture_frame = picture_frame_;
  pegs_ = opening_pegs_;
  random_ = opening_random_;
  Restart();
  layout_ = layout;
  daily_ = daily;
  day_ = day;
  item_id_ = item_id;
  picture_frame_ = picture_frame;
  purple_rotates_ = true;
}

bool PegGame::NewItemGame(const PegPicture& picture) {
  random_.seed(seeds_());
  return StartItemBoard(picture, false, 0);
}

bool PegGame::NewDailyItemGame(std::uint32_t day, const PegPicture& picture) {
  random_.seed(day);
  return StartItemBoard(picture, true, day);
}

bool PegGame::StartItemBoard(const PegPicture& picture, bool daily, std::uint32_t day) {
  std::vector<Peg> pegs = BuildItemPegLayout(picture, random_);
  if (pegs.empty()) {
    return false;
  }
  SetPegs(std::move(pegs));
  layout_ = PegLayout::kItem;
  daily_ = daily;
  day_ = day;
  item_id_ = picture.item_id;
  picture_frame_ = PegPictureFrame(picture);
  OpenBoard();
  return true;
}

void PegGame::SetPegs(std::vector<Peg> pegs) {
  pegs_ = std::move(pegs);
  opening_pegs_.clear();
  Restart();
}

void PegGame::Restart() {
  orange_total_ = static_cast<int>(
      std::ranges::count_if(pegs_, [](const Peg& peg) { return peg.kind == PegKind::kOrange; }));
  orange_left_ = orange_total_;
  balls_left_ = PegStartBalls(static_cast<int>(pegs_.size()));
  score_ = 0;
  catches_ = 0;
  powers_used_ = 0;
  free_balls_ = 0;
  best_shot_ = 0;
  safety_shots_ = 0;
  finale_ = false;
  balls_.clear();
  spawned_.clear();
  phase_ = PegPhase::kAiming;
  layout_ = PegLayout::kScattered;
  daily_ = false;
  day_ = 0;
  item_id_ = 0;
  picture_frame_ = PegFrame();
  purple_rotates_ = false;
  bucket_x_ = kPegBoardWidth * 0.5F;
  bucket_direction_ = 1.0F;
  pending_s_ = 0.0F;
  news_ = {};
  started_ = true;
}

void PegGame::ChoosePurple() {
  std::vector<std::size_t> blues;
  for (std::size_t index = 0; index < pegs_.size(); ++index) {
    Peg& peg = pegs_[index];
    if (peg.removed) {
      continue;
    }
    if (peg.kind == PegKind::kPurple) {
      peg.kind = PegKind::kBlue;
    }
    if (peg.kind == PegKind::kBlue) {
      blues.push_back(index);
    }
  }
  if (blues.empty()) {
    return;
  }
  std::uniform_int_distribution<std::size_t> pick(0, blues.size() - 1);
  pegs_[blues[pick(random_)]].kind = PegKind::kPurple;
}

void PegGame::Aim(PegPoint target) noexcept { aim_ = target; }

void PegGame::NudgeAim(int steps) noexcept {
  const float angle = std::atan2(aim_.y - kPegCannonY, aim_.x - kPegCannonX) +
                      static_cast<float>(steps) * kAimNudgeRad;
  aim_ = PegPoint{.x = kPegCannonX + std::cos(angle) * kAimNudgeReach,
                  .y = kPegCannonY + std::sin(angle) * kAimNudgeReach};
}

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
  balls_.assign(1, PegBall{.at = at, .velocity = velocity, .lowest_y = at.y});
  spawned_.clear();
  shot_time_s_ = 0.0F;
  shot_score_ = 0;
  shot_lit_ = 0;
  shot_free_ball_ = false;
  caught_ = false;
  phase_ = PegPhase::kFlying;
}

PegPoint PegGame::ball() const noexcept { return balls_.empty() ? last_ball_ : balls_.front().at; }

float PegGame::bucket_width() const noexcept {
  return safety_shots_ > 0 ? kPegSafetyWallWidth : kPegBucketWidth;
}

int PegGame::multiplier() const noexcept {
  constexpr float kDoubleAtShare = 0.5F;
  constexpr float kTripleAtShare = 0.75F;
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

const Peg* PegGame::LastUnlitOrange() const noexcept {
  const Peg* last = nullptr;
  for (const Peg& peg : pegs_) {
    if (peg.kind != PegKind::kOrange || peg.removed || peg.lit) {
      continue;
    }
    if (last != nullptr) {
      return nullptr;
    }
    last = &peg;
  }
  return last;
}

bool PegGame::slowed() const noexcept {
  if (phase_ != PegPhase::kFlying || finale_) {
    return false;
  }
  const Peg* last = LastUnlitOrange();
  if (last == nullptr) {
    return false;
  }
  return std::ranges::any_of(balls_, [last](const PegBall& ball) {
    return DistanceSquared(ball.at, PegPoint{last->x, last->y}) < kPegSlowRadius * kPegSlowRadius;
  });
}

void PegGame::Award(PegCalloutKind kind, PegPoint at, int points) {
  score_ += points;
  shot_score_ += points;
  news_.callouts.push_back(PegCallout{.kind = kind, .at = at, .points = points});
  if (!shot_free_ball_ && shot_score_ >= kPegFreeBallShotPoints) {
    shot_free_ball_ = true;
    ++balls_left_;
    ++free_balls_;
    news_.free_ball = true;
    news_.callouts.push_back(PegCallout{.kind = PegCalloutKind::kFreeBall, .at = at});
  }
}

void PegGame::Light(Peg& peg, PegBall* ball) {
  peg.lit = true;
  ++shot_lit_;
  const PegPoint at{peg.x, peg.y};
  Award(PegCalloutKind::kPoints, at, PointsOf(peg.kind) * multiplier());
  switch (peg.kind) {
    case PegKind::kOrange:
      ++news_.orange_hits;
      break;
    case PegKind::kBlue:
      ++news_.blue_hits;
      break;
    case PegKind::kPurple:
    case PegKind::kGreen:
      ++news_.special_hits;
      break;
  }
  if (peg.kind == PegKind::kOrange && ball != nullptr) {
    if (ball->touched_peg && ball->since_peg_px >= kPegLongShotPx) {
      Award(PegCalloutKind::kLongShot, at, kPegLongShotPoints);
    } else if (ball->after_wall) {
      Award(PegCalloutKind::kBankShot, at, kPegBankShotPoints);
    }
  }
  if (shot_lit_ == kPegVolleyPegs) {
    Award(PegCalloutKind::kVolley, at, kPegVolleyPoints);
  }
  if (peg.kind == PegKind::kOrange && !finale_ && orange_left_ > 0 &&
      std::ranges::none_of(pegs_, [](const Peg& other) {
        return other.kind == PegKind::kOrange && !other.removed && !other.lit;
      })) {
    finale_ = true;
    news_.callouts.push_back(PegCallout{.kind = PegCalloutKind::kFinale, .at = at});
  }
  if (peg.kind == PegKind::kGreen) {
    TriggerPower(peg, ball);
  }
}

void PegGame::TriggerPower(const Peg& peg, PegBall* ball) {
  ++powers_used_;
  news_.power = true;
  const PegPoint at{peg.x, peg.y};
  news_.callouts.push_back(PegCallout{.kind = CalloutOf(peg.power), .at = at});
  switch (peg.power) {
    case PegPower::kMagnumBreak:
      for (Peg& other : pegs_) {
        if (!other.removed && !other.lit &&
            DistanceSquared(at, PegPoint{other.x, other.y}) <=
                kPegMagnumRadius * kPegMagnumRadius) {
          Light(other, nullptr);
        }
      }
      break;
    case PegPower::kSafetyWall:
      safety_shots_ = kPegSafetyWallShots;
      break;
    case PegPower::kDoubleStrafe:
      if (ball != nullptr) {
        PegPoint velocity{.x = -ball->velocity.x, .y = ball->velocity.y};
        if (std::abs(velocity.x) < kStrafeKick) {
          velocity.x = velocity.x < 0.0F ? -kStrafeKick : kStrafeKick;
        }
        spawned_.push_back(PegBall{.at = ball->at, .velocity = velocity, .lowest_y = ball->at.y});
      }
      break;
    case PegPower::kNone:
      break;
  }
}

void PegGame::ReleaseStuck(PegBall& ball) {
  for (Peg& peg : pegs_) {
    if (!peg.lit || peg.removed ||
        DistanceSquared(ball.at, PegPoint{peg.x, peg.y}) > kUnstuckReach * kUnstuckReach) {
      continue;
    }
    peg.removed = true;
    peg.lit = false;
    if (peg.kind == PegKind::kOrange && orange_left_ > 0) {
      --orange_left_;
    }
    news_.cleared.push_back(PegCleared{.at = {peg.x, peg.y}, .kind = peg.kind});
  }
  ball.lowest_y = ball.at.y;
  ball.still_s = 0.0F;
}

void PegGame::LeaveBoard(const PegBall& ball) {
  last_ball_ = ball.at;
  if (!finale_ || ball.at.y <= kPegBoardHeight) {
    return;
  }
  constexpr int kBins = static_cast<int>(kPegFinaleBins.size());
  const int bin = std::clamp(
      static_cast<int>(ball.at.x / (kPegBoardWidth / static_cast<float>(kBins))), 0, kBins - 1);
  const int points = kPegFinaleBins[static_cast<std::size_t>(bin)];
  score_ += points;
  shot_score_ += points;
  news_.callouts.push_back(PegCallout{.kind = PegCalloutKind::kBin,
                                      .at = {ball.at.x, kPegBoardHeight - kPegFinaleBinHeight},
                                      .points = points});
}

void PegGame::EndShot() {
  for (Peg& peg : pegs_) {
    if (peg.lit && !peg.removed) {
      peg.removed = true;
      if (peg.kind == PegKind::kOrange && orange_left_ > 0) {
        --orange_left_;
      }
      news_.cleared.push_back(PegCleared{.at = {peg.x, peg.y}, .kind = peg.kind});
    }
    peg.lit = false;
  }
  balls_.clear();
  spawned_.clear();
  if (!caught_) {
    --balls_left_;
  }
  best_shot_ = std::max(best_shot_, shot_score_);
  if (safety_shots_ > 0) {
    --safety_shots_;
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
    if (purple_rotates_) {
      ChoosePurple();
    }
  }
}

void PegGame::MoveBucket(float step_s) {
  bucket_x_ += bucket_direction_ * kBucketSpeed * step_s;
  const float half_bucket = bucket_width() * 0.5F;
  if (bucket_x_ < half_bucket) {
    bucket_x_ = half_bucket;
    bucket_direction_ = 1.0F;
  }
  if (bucket_x_ > kPegBoardWidth - half_bucket) {
    bucket_x_ = kPegBoardWidth - half_bucket;
    bucket_direction_ = -1.0F;
  }
}

void PegGame::MoveOrbits(float step_s) {
  for (Peg& peg : pegs_) {
    if (peg.removed || peg.orbit.radius <= 0.0F) {
      continue;
    }
    peg.orbit.angle_rad += peg.orbit.speed_rad_s * step_s;
    peg.x = peg.orbit.center.x + std::cos(peg.orbit.angle_rad) * peg.orbit.radius;
    peg.y = peg.orbit.center.y + std::sin(peg.orbit.angle_rad) * peg.orbit.radius;
  }
}

void PegGame::StepBall(PegBall& ball, float step_s) {
  ball.velocity.y += kGravity * step_s;
  const float speed2 = ball.velocity.x * ball.velocity.x + ball.velocity.y * ball.velocity.y;
  if (speed2 > kMaxSpeed * kMaxSpeed) {
    const float scale = kMaxSpeed / std::sqrt(speed2);
    ball.velocity.x *= scale;
    ball.velocity.y *= scale;
  }
  ball.at.x += ball.velocity.x * step_s;
  ball.at.y += ball.velocity.y * step_s;
  ball.since_peg_px += std::sqrt(std::min(speed2, kMaxSpeed * kMaxSpeed)) * step_s;

  if (BouncePegOnWalls(ball.at, ball.velocity)) {
    ball.after_wall = true;
    ++news_.wall_hits;
  }

  // Les plots : la vitesse se réfléchit sur la normale du contact.
  const float reach = kPegBallRadius + kPegRadius;
  for (Peg& peg : pegs_) {
    if (peg.removed) {
      continue;
    }
    const float dx = ball.at.x - peg.x;
    const float dy = ball.at.y - peg.y;
    const float distance2 = dx * dx + dy * dy;
    if (distance2 >= reach * reach) {
      continue;
    }
    const float distance = std::max(std::sqrt(distance2), 1e-4F);
    const float nx = dx / distance;
    const float ny = dy / distance;
    ball.at.x = peg.x + nx * reach;
    ball.at.y = peg.y + ny * reach;
    const float along_normal = ball.velocity.x * nx + ball.velocity.y * ny;
    ball.velocity.x -= (1.0F + kPegRestitution) * along_normal * nx;
    ball.velocity.y -= (1.0F + kPegRestitution) * along_normal * ny;
    if (!peg.lit) {
      Light(peg, &ball);
      ball.still_s = 0.0F;
    }
    ball.touched_peg = true;
    ball.since_peg_px = 0.0F;
    ball.after_wall = false;
  }

  // Le seau, près du bas : la bille lui revient, une fois par tir. Il n'est
  // plus là pendant la finale.
  const float half_bucket = bucket_width() * 0.5F;
  if (!finale_ && !caught_ && ball.at.y > kPegBucketY - kPegBucketHeight &&
      ball.at.y < kPegBucketY + kPegBucketHeight && ball.at.x > bucket_x_ - half_bucket &&
      ball.at.x < bucket_x_ + half_bucket) {
    caught_ = true;
    ++catches_;
    news_.caught = true;
  }

  if (ball.at.y > ball.lowest_y + kStuckProgressPx) {
    ball.lowest_y = ball.at.y;
    ball.still_s = 0.0F;
  } else {
    ball.still_s += step_s;
    if (ball.still_s >= kStuckS) {
      ReleaseStuck(ball);
    }
  }
}

void PegGame::Step(float step_s) {
  // Le seau et les anneaux bougent, quoi qu'il arrive à la bille.
  MoveBucket(step_s);
  MoveOrbits(step_s);
  if (phase_ != PegPhase::kFlying) {
    return;
  }

  shot_time_s_ += step_s;
  for (PegBall& ball : balls_) {
    StepBall(ball, step_s);
  }
  balls_.insert(balls_.end(), spawned_.begin(), spawned_.end());
  spawned_.clear();

  const bool timed_out = shot_time_s_ > kShotTimeoutS;
  std::erase_if(balls_, [this, timed_out](const PegBall& ball) {
    if (!timed_out && ball.at.y <= kPegBoardHeight + kPegBallRadius) {
      return false;
    }
    LeaveBoard(ball);
    return true;
  });
  if (balls_.empty()) {
    EndShot();
  }
}

void PegGame::Advance(float elapsed_s) {
  pending_s_ = std::min(pending_s_ + std::max(0.0F, elapsed_s), kMostCatchUpS);
  while (true) {
    const float real_step_s = slowed() ? kPegStepS / kPegSlowScale : kPegStepS;
    if (pending_s_ < real_step_s) {
      break;
    }
    Step(kPegStepS);
    pending_s_ -= real_step_s;
  }
}

PegNews PegGame::TakeNews() { return std::exchange(news_, PegNews{}); }

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
    BouncePegOnWalls(at, velocity);
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
