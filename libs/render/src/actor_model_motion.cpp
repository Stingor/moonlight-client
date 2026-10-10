#include "ro/render/actor_model_motion.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>

namespace ro::render {
namespace {

// Les actions du client, par leur rang — huit fois leur numéro.
constexpr std::uint32_t kStandRank = 0;
constexpr std::uint32_t kWalkRank = 8;
constexpr std::uint32_t kPickupRank = 24;
constexpr std::uint32_t kReadyRank = 32;
constexpr std::uint32_t kFreezeRank = 40;
constexpr std::uint32_t kHurtRank = 48;
constexpr std::uint32_t kDieRank = 64;
constexpr std::uint32_t kAttackRank = 80;
constexpr std::uint32_t kSecondAttackRank = 88;

// Le numéro d'un modèle tient en quatre chiffres au plus.
constexpr std::size_t kMaxIndexDigits = 4;

// LA TABLE DES MODÈLES : pour chaque numéro, la classe dont le nom désigne son
// fichier d'attente. Mille est un monstre à sprite : ces numéros n'ont pas de
// modèle.
constexpr std::array<std::uint32_t, 10> kStandJobs = {1288, 722,  1324, 1000, 1000,
                                                      1000, 1000, 1286, 1285, 1287};
// Le drapeau de guilde ne cherche aucune animation.
constexpr std::int32_t kGuildFlagIndex = 1;

// L'unité de la cadence d'attaque, et le plafond qu'elle ne dépasse pas.
constexpr float kAttackMotionUnitMs = 432.0F;
// Une cadence nulle se lit comme la cadence ordinaire.
constexpr std::uint32_t kDefaultAttackMotionMs = 1440;

constexpr float kDegreesPerDirection = 45.0F;
constexpr std::uint32_t kDirections = 8;
constexpr std::uint32_t kHalfTurnDirections = 4;
constexpr float kHalfTurnDegrees = 180.0F;
constexpr float kFullTurnDegrees = 360.0F;
constexpr std::int32_t kFullTurn = 360;
constexpr float kClientPi = 3.141592F;

// Le modèle tourne de six degrés par tic tant qu'il est à plus de douze de son
// corps, par le plus court chemin.
constexpr float kTurnStepDegrees = 6.0F;
constexpr std::int32_t kTurnDeadZoneDegrees = 12;
constexpr std::int32_t kHalfTurn = 180;

// L'effacement du reste : l'alpha tombe d'un cran par tic, et le calcul du
// client DÉBORDE sur un octet — c'est ce débordement qui en fait une rampe.
constexpr std::int32_t kRemnantFadeOrigin = 88;
constexpr std::uint8_t kOpaque = 255;

}  // namespace

std::optional<ActorModelMotion> ActorModelMotionOfAction(std::uint32_t action_rank) {
  switch (action_rank) {
    case kStandRank:
      return ActorModelMotion::kStand;
    case kWalkRank:
      return ActorModelMotion::kMove;
    case kAttackRank:
    case kSecondAttackRank:
      return ActorModelMotion::kAttack;
    case kDieRank:
      return ActorModelMotion::kDead;
    case kHurtRank:
      return ActorModelMotion::kDamage;
    case kPickupRank:
    case kReadyRank:
    case kFreezeRank:
    default:
      return std::nullopt;
  }
}

std::optional<std::int32_t> ActorModelIndex(std::string_view resource_name) {
  const std::size_t underscore = resource_name.find('_');
  if (underscore == std::string_view::npos) {
    return std::nullopt;
  }
  std::string digits;
  for (std::size_t index = underscore + 1;
       index < resource_name.size() && resource_name[index] != '.' &&
       digits.size() < kMaxIndexDigits;
       ++index) {
    digits.push_back(resource_name[index]);
  }
  // Comme une lecture d'entier en C : ce qui ne commence pas par un chiffre vaut zéro.
  return static_cast<std::int32_t>(std::strtol(digits.c_str(), nullptr, 10));
}

std::optional<std::uint32_t> ActorModelStandJob(std::int32_t model_index) {
  if (model_index < 0 || static_cast<std::size_t>(model_index) >= kStandJobs.size()) {
    return std::nullopt;
  }
  return kStandJobs[static_cast<std::size_t>(model_index)];
}

std::optional<std::string> ActorModelStandPath(const SpriteCatalog& catalog,
                                               std::int32_t model_index) {
  const std::optional<std::uint32_t> job = ActorModelStandJob(model_index);
  if (!job.has_value() || model_index == kGuildFlagIndex) {
    return std::nullopt;
  }
  return std::string(kActorModelFolder) + NonPlayerResourceName(catalog, *job);
}

std::optional<std::string> ActorModelMotionPath(std::int32_t model_index, ActorModelMotion motion) {
  if (motion == ActorModelMotion::kStand || model_index == kGuildFlagIndex ||
      !ActorModelStandJob(model_index).has_value()) {
    return std::nullopt;
  }
  return "data\\model\\3dmob_bone\\" + std::to_string(model_index) + "_" +
         std::string(kActorModelMotionNames[static_cast<std::size_t>(motion)]) + ".gr2";
}

float ActorModelAttackSpeed(std::uint32_t attack_motion_ms) {
  const std::uint32_t motion = attack_motion_ms == 0 ? kDefaultAttackMotionMs : attack_motion_ms;
  return std::min(static_cast<float>(motion), kAttackMotionUnitMs) / kAttackMotionUnitMs;
}

float ActorBodyDegrees(std::uint8_t direction) {
  return kDegreesPerDirection *
         static_cast<float>((direction % kDirections + kHalfTurnDirections) % kDirections);
}

std::optional<float> ActorBodyDegreesToward(float from_x, float from_z, float to_x, float to_z) {
  const float dx = to_x - from_x;
  const float dz = to_z - from_z;
  if (dx == 0.0F && dz == 0.0F) {
    return std::nullopt;
  }
  float degrees = std::atan2(dx, -dz) / kClientPi * kHalfTurnDegrees;
  if (degrees >= kFullTurnDegrees) {
    degrees -= kFullTurnDegrees;
  }
  if (degrees < 0.0F) {
    degrees += kFullTurnDegrees;
  }
  return degrees;
}

float InitialActorModelYaw(float body_degrees) { return kHalfTurnDegrees - body_degrees; }

float TurnActorModelYaw(float yaw_degrees, float body_degrees) {
  float body = body_degrees;
  if (body > kFullTurnDegrees) {
    body = static_cast<float>(static_cast<std::int32_t>(body) % kFullTurn);
  }
  float yaw = yaw_degrees;
  if (yaw > kFullTurnDegrees) {
    yaw = static_cast<float>(static_cast<std::int32_t>(yaw) % kFullTurn);
  }
  if (yaw < 0.0F) {
    yaw += kFullTurnDegrees;
  }
  // Les écarts se prennent en ENTIERS, arrondis vers zéro à chaque étape.
  const auto target = static_cast<std::int32_t>(
      kHalfTurnDegrees - static_cast<float>(static_cast<std::int32_t>(body)));
  std::int32_t gap = static_cast<std::int32_t>(yaw - static_cast<float>(target));
  if (gap > kFullTurn || gap < -kFullTurn) {
    gap %= kFullTurn;
  }
  if (gap < 0) {
    gap += kFullTurn;
  }
  if (gap <= kTurnDeadZoneDegrees) {
    return yaw;
  }
  return gap < kHalfTurn ? yaw - kTurnStepDegrees : yaw + kTurnStepDegrees;
}

std::uint8_t ActorModelRemnantAlpha(std::int32_t tick) {
  if (tick <= kActorModelRemnantHoldTicks) {
    return kOpaque;
  }
  return static_cast<std::uint8_t>(static_cast<std::uint32_t>(kRemnantFadeOrigin - tick) & 0xFFU);
}

}  // namespace ro::render
