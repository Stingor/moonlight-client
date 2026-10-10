#include "ro/render/actor_scale.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <span>

namespace ro::render {
namespace {

// Le client lit une table de sinus indexée en DEGRÉS, remplie au démarrage.
constexpr float kFirstDegree = 90.0F;
constexpr float kDegreesPerStep = 4.0F;
constexpr float kHalfTurnDegrees = 180.0F;

}  // namespace

float SizeChangeScale(float target, std::int32_t steps) {
  if (steps <= 0) {
    return 1.0F;
  }
  if (steps >= kSizeChangeSteps) {
    return target;
  }
  const float degrees = kFirstDegree + kDegreesPerStep * static_cast<float>(steps);
  const float radians = degrees * std::numbers::pi_v<float> / kHalfTurnDegrees;
  // Le sinus va de un à moins un sur ces quarante-cinq pas ; la progression est
  // donc sa moitié complémentaire, de zéro à un.
  const float progress = (1.0F - std::sin(radians)) * 0.5F;
  return 1.0F + (target - 1.0F) * progress;
}

namespace {

// Une plage de classes, bornes comprises.
struct JobRange {
  std::uint32_t first = 0;
  std::uint32_t last = 0;
};

// Les bébés de PREMIÈRE classe, du novice au voleur, plus le super novice et
// sa forme étendue.
constexpr std::array<JobRange, 3> kBabyFirstClasses = {JobRange{4023, 4029}, JobRange{4045, 4045},
                                                       JobRange{4191, 4191}};

// Les autres bébés que le client rétrécit : les classes promues, les
// troisièmes et quatrièmes classes, leurs montures et les classes étendues.
constexpr std::array<JobRange, 9> kBabyOtherClasses = {
    JobRange{4030, 4044}, JobRange{4096, 4112}, JobRange{4158, 4182},
    JobRange{4193, 4196}, JobRange{4205, 4210}, JobRange{4222, 4238},
    JobRange{4241, 4242}, JobRange{4244, 4244}, JobRange{4247, 4248}};

// Le bébé invocateur, seul bébé Doram que le client connaisse.
constexpr std::uint32_t kBabySummoner = 4220;

constexpr float kBabyFirstClassScale = 0.75F;
constexpr float kBabySummonerScale = 0.8F;
constexpr float kBabyScale = 0.82F;

bool Within(std::span<const JobRange> ranges, std::uint32_t job) {
  return std::ranges::any_of(
      ranges, [job](const JobRange& range) { return job >= range.first && job <= range.last; });
}

}  // namespace

float BodyScaleFor(std::uint32_t job) {
  if (Within(kBabyFirstClasses, job)) {
    return kBabyFirstClassScale;
  }
  if (job == kBabySummoner) {
    return kBabySummonerScale;
  }
  if (Within(kBabyOtherClasses, job)) {
    return kBabyScale;
  }
  return 1.0F;
}

}  // namespace ro::render
