#include "ro/render/companion_falcon.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

#include "ro/render/shown_job.h"
#include "ro/render/sprite_frame.h"

namespace ro::render {
namespace {

// Les métiers dont l'oiseau n'est pas le faucon ordinaire, dans la table des
// métiers du serveur (`e_job`).
constexpr std::uint32_t kJobSniper = 4012;
constexpr std::uint32_t kJobBabyHunter = 4034;
constexpr std::uint32_t kJobGangsi = 4050;
constexpr std::uint32_t kJobDeathKnight = 4051;
constexpr std::uint32_t kJobDarkCollector = 4052;
constexpr std::uint32_t kJobRanger = 4062;
constexpr std::uint32_t kJobRangerTrans = 4098;
constexpr std::uint32_t kJobWindHawk = 4257;
// Le Wind Hawk MONTÉ sur son loup : le corps que montre un Wind Hawk dont
// l'option de monture est levée.
constexpr std::uint32_t kJobWindHawkRiding = 4278;

constexpr FalconArt kHawk = {"data\\sprite\\이팩트\\매.spr", "data\\sprite\\이팩트\\매.act"};
constexpr FalconArt kSniperHawk = {"data\\sprite\\이팩트\\매2.spr",
                                   "data\\sprite\\이팩트\\매2.act"};
constexpr FalconArt kBabyHawk = {"data\\sprite\\이팩트\\매.spr",
                                 "data\\sprite\\이팩트\\베이비매.act"};
constexpr FalconArt kPungus = {"data\\sprite\\몬스터\\demon_pungus.spr",
                               "data\\sprite\\몬스터\\demon_pungus.act"};
constexpr FalconArt kExplosion = {"data\\sprite\\몬스터\\explosion.spr",
                                  "data\\sprite\\몬스터\\explosion.act"};
constexpr FalconArt kLude = {"data\\sprite\\몬스터\\lude.spr", "data\\sprite\\몬스터\\lude.act"};
constexpr FalconArt kOwl = {"data\\sprite\\이팩트\\OWL.spr", "data\\sprite\\이팩트\\OWL.act"};
constexpr FalconArt kWindHawk = {"data\\sprite\\이팩트\\WINDHAWK_HAWK.spr",
                                 "data\\sprite\\이팩트\\WINDHAWK_HAWK.act"};

// Les compétences de dégâts dont le coup envoie le faucon de l'auteur.
constexpr std::uint32_t kSkillBlitzBeat = 129;       // HT_BLITZBEAT
constexpr std::uint32_t kSkillFalconAssault = 381;   // SN_FALCONASSAULT
constexpr std::uint32_t kSkillHawkRush = 5326;       // WH_HAWKRUSH
constexpr std::uint32_t kSkillHawkBoomerang = 5329;  // WH_HAWKBOOMERANG
constexpr std::uint32_t kSkillWildWalk = 6520;       // WH_WILD_WALK
constexpr auto kFalconStrikeSkills = std::to_array<std::uint32_t>(
    {kSkillBlitzBeat, kSkillFalconAssault, kSkillHawkRush, kSkillHawkBoomerang, kSkillWildWalk});

constexpr std::uint32_t kOptionHide = 0x2;
constexpr std::uint32_t kOptionCloak = 0x4;
constexpr std::uint32_t kOptionInvisible = 0x40;

constexpr float kDegreesPerHalfTurn = 180.0F;
constexpr float kDegreesPerTurn = 360.0F;
constexpr float kDegreesPerEighth = kDegreesPerTurn / static_cast<float>(kDirectionCount);

float DistanceXz(const FalconPoint& from, const FalconPoint& to) noexcept {
  return std::hypot(to.x - from.x, to.z - from.z);
}

// Un pas vers le maître : l'écart sur le plan, divisé.
void StepToward(Falcon& falcon, const FalconPoint& master, float divisor) noexcept {
  falcon.position.x += (master.x - falcon.position.x) / divisor;
  falcon.position.z += (master.z - falcon.position.z) / divisor;
}

// La poursuite du maître, au repos.
void Pursue(Falcon& falcon, const FalconMaster& master, const FalconGround& ground) {
  const float distance = DistanceXz(falcon.position, master.feet);
  falcon.shown_heading_degrees = falcon.heading_degrees;
  const bool far = distance > kFalconGlideRangeUnits;
  if (!falcon.arrived) {
    // Revenu à portée, il est arrivé ; mais ce tic-ci plane encore.
    falcon.arrived = !far;
    StepToward(falcon, master.feet, kFalconGlideDivisor);
    falcon.frame = kFalconGlideFrame;
  } else if (distance > kFalconCruiseRangeUnits) {
    StepToward(falcon, master.feet, distance);
  } else if (distance > kFalconSettleRangeUnits) {
    StepToward(falcon, master.feet, kFalconSettleDivisor);
  }
  falcon.heading_degrees =
      FalconHeadingToward(falcon.position, master.feet.x, master.feet.z, falcon.heading_degrees);
  falcon.position.y = ground(falcon.position.x, falcon.position.z) - kFalconHoverUnits;
}

// Un tic du vol : il tourne le dos à son maître.
void Fly(Falcon& falcon, const FalconMaster& master) noexcept {
  float heading =
      FalconHeadingToward(falcon.position, master.feet.x, master.feet.z, falcon.heading_degrees) -
      kDegreesPerHalfTurn;
  if (heading < 0.0F) {
    heading += kDegreesPerTurn;
  }
  falcon.heading_degrees = heading;
  falcon.shown_heading_degrees = heading;
  falcon.position.x += falcon.velocity.x;
  const std::int32_t tick = falcon.counter_ticks;
  if (tick <= kFalconDescentLastTick || tick >= kFalconClimbFirstTick) {
    falcon.position.y += falcon.velocity.y;
    falcon.position.z += falcon.velocity.z;
    if (tick >= kFalconClimbFirstTick) {
      falcon.velocity.y *= kFalconClimbFactor;
      falcon.climbing = true;
    }
  } else {
    falcon.position.z += falcon.velocity.z;
  }
  ++falcon.counter_ticks;
  falcon.frame = 0;
}

// Le battement d'ailes, au repos : une image tous les six tics, seulement une
// fois arrivé.
void Flap(Falcon& falcon, std::size_t rest_frame_count) noexcept {
  std::int32_t counter = falcon.counter_ticks;
  if (counter % kFalconFrameDelayTicks == 0) {
    if (falcon.arrived) {
      ++falcon.frame;
    }
    counter = 0;
  }
  if (falcon.frame >= rest_frame_count) {
    falcon.frame = 0;
  }
  falcon.counter_ticks = counter + 1;
}

}  // namespace

std::uint32_t CompanionShownJob(std::uint32_t job_id, std::uint32_t option) noexcept {
  const std::uint32_t shown = ShownJobFor(job_id, option);
  return IsCostumeJob(shown) ? kShownCostumeJob : shown;
}

FalconArt FalconArtFor(std::uint32_t job_id, std::uint32_t option) noexcept {
  switch (CompanionShownJob(job_id, option)) {
    case kJobSniper:
      return kSniperHawk;
    case kJobBabyHunter:
      return kBabyHawk;
    case kJobGangsi:
      return kPungus;
    case kJobDeathKnight:
      return kExplosion;
    case kJobDarkCollector:
      return kLude;
    case kJobRanger:
    case kJobRangerTrans:
      return kOwl;
    case kJobWindHawk:
    case kJobWindHawkRiding:
      return kWindHawk;
    default:
      return kHawk;
  }
}

bool CarriesFalcon(std::uint32_t option) noexcept {
  return (option & kOptionFalcon) != 0 &&
         (option & (kOptionHide | kOptionCloak | kOptionInvisible)) == 0;
}

Falcon BornFalcon(const FalconMaster& master) noexcept {
  Falcon falcon;
  // Le cap du maître se lit en DEGRÉS ENTIERS : le jeu le tronque pour sa table
  // de sinus.
  const float heading_radians =
      std::trunc(master.heading_degrees) * std::numbers::pi_v<float> / kDegreesPerHalfTurn;
  falcon.position.x = master.feet.x - kFalconBirthBehindUnits * std::sin(heading_radians);
  falcon.position.y = master.feet.y - kFalconBirthRiseUnits;
  falcon.position.z = master.feet.z + kFalconBirthBehindUnits * std::cos(heading_radians);
  return falcon;
}

void LaunchFalcon(Falcon& falcon, const FalconPoint& target) noexcept {
  falcon.velocity.x = (target.x - falcon.position.x) / kFalconDiveDivisor;
  falcon.velocity.y = (target.y - falcon.position.y) / kFalconDiveDivisor;
  falcon.velocity.z = (target.z - falcon.position.z) / kFalconDiveDivisor;
  falcon.flying = true;
  falcon.arrived = false;
}

bool SkillHitSendsFalcon(std::uint32_t skill_id) noexcept {
  return std::ranges::find(kFalconStrikeSkills, skill_id) != kFalconStrikeSkills.end();
}

void StepFalcon(Falcon& falcon, const FalconMaster& master, const FalconGround& ground,
                const FalconFrameCount& frame_count, float camera_yaw_degrees) {
  // LA FIN DU VOL se décide avant tout le reste, et ce tic-là ne fait rien
  // d'autre : la poursuite reprend au suivant.
  if (falcon.climbing && falcon.counter_ticks > kFalconFlightLastTick && falcon.flying) {
    falcon.flying = false;
    falcon.climbing = false;
    return;
  }
  if (falcon.flying) {
    Fly(falcon, master);
    return;
  }
  Pursue(falcon, master, ground);
  Flap(falcon, frame_count(FalconAction(falcon, camera_yaw_degrees)));
}

float FalconHeadingToward(const FalconPoint& from, float to_x, float to_z,
                          float unchanged) noexcept {
  const float delta_x = to_x - from.x;
  const float delta_z = to_z - from.z;
  if (delta_x == 0.0F && delta_z == 0.0F) {
    return unchanged;
  }
  float heading = std::atan2(delta_x, -delta_z) / std::numbers::pi_v<float> * kDegreesPerHalfTurn;
  if (heading >= kDegreesPerTurn) {
    heading -= kDegreesPerTurn;
  }
  if (heading < 0.0F) {
    heading += kDegreesPerTurn;
  }
  return heading;
}

std::size_t FalconScreenDirection(float heading_degrees, float camera_yaw_degrees) noexcept {
  float seen = std::fmod(camera_yaw_degrees - heading_degrees, kDegreesPerTurn);
  if (seen < 0.0F) {
    seen += kDegreesPerTurn;
  }
  // Le huitième de tour le plus proche : le premier et le dernier demi-huitième
  // regardent tous deux vers la caméra.
  const auto eighth =
      static_cast<std::size_t>((seen + kDegreesPerEighth / 2.0F) / kDegreesPerEighth);
  return eighth % kDirectionCount;
}

std::size_t FalconAction(const Falcon& falcon, float camera_yaw_degrees) noexcept {
  const std::size_t direction =
      FalconScreenDirection(falcon.shown_heading_degrees, camera_yaw_degrees);
  return falcon.flying ? kFalconFlightActionBase + direction : direction;
}

}  // namespace ro::render
