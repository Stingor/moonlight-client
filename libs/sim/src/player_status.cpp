#include "ro/sim/player_status.h"

#include <algorithm>
#include <array>

namespace ro::sim {

ParameterOutcome ApplyParameter(PlayerStatus& status, std::uint16_t parameter, std::int64_t value) {
  switch (static_cast<ServerParameter>(parameter)) {
    case ServerParameter::kSp:
      status.sp = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kMaxSp:
      status.max_sp = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kBaseLevel:
      status.base_level = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kJobLevel:
      status.job_level = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kBaseExp:
      status.base_exp = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kNextBaseExp:
      status.next_base_exp = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kJobExp:
      status.job_exp = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kNextJobExp:
      status.next_job_exp = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kZeny:
      status.zeny = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kWeight:
      status.weight = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kMaxWeight:
      status.max_weight = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kStatusPoint:
      status.status_point = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kSkillPoint:
      status.skill_point = value;
      return ParameterOutcome::kStored;

    case ServerParameter::kStr:
      status.str = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kAgi:
      status.agi = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kVit:
      status.vit = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kInt:
      status.int_ = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kDex:
      status.dex = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kLuk:
      status.luk = value;
      return ParameterOutcome::kStored;

    case ServerParameter::kUStr:
      status.str_cost = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kUAgi:
      status.agi_cost = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kUVit:
      status.vit_cost = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kUInt:
      status.int_cost = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kUDex:
      status.dex_cost = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kULuk:
      status.luk_cost = value;
      return ParameterOutcome::kStored;

    case ServerParameter::kAtk1:
      status.atk1 = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kAtk2:
      status.atk2 = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kMatk1:
      status.matk1 = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kMatk2:
      status.matk2 = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kDef1:
      status.def1 = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kDef2:
      status.def2 = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kMdef1:
      status.mdef1 = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kMdef2:
      status.mdef2 = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kHit:
      status.hit = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kFlee1:
      status.flee1 = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kFlee2:
      status.flee2 = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kCritical:
      status.critical = value;
      return ParameterOutcome::kStored;
    case ServerParameter::kAspd:
      status.amotion = value;
      return ParameterOutcome::kStored;

    // LA VIE ET LA VITESSE PASSENT AILLEURS : elles vivent sur l'acteur, parce
    // que tout le monde en a. `World::SetPlayerParameter` les intercepte avant
    // d'arriver ici ; si elles y arrivent quand même, c'est qu'on est entré par
    // la mauvaise porte, et le dire vaut mieux que de les perdre en silence.
    case ServerParameter::kHp:
    case ServerParameter::kMaxHp:
    case ServerParameter::kSpeed:
      return ParameterOutcome::kUnknown;

    // RECONNUS, ET DÉLIBÉRÉMENT PAS GARDÉS. Le client n'en montre rien : le
    // karma et la politesse ne s'affichent nulle part, tandis que le sexe et le
    // métier arrivent déjà avec l'APPARENCE de l'acteur — c'est elle qui décide
    // du sprite, et les garder ici en dédoublerait la vérité.
    case ServerParameter::kKarma:
    case ServerParameter::kManner:
    case ServerParameter::kSex:
    case ServerParameter::kClass:
      return ParameterOutcome::kKnownButUnused;

    default:
      return ParameterOutcome::kUnknown;
  }
}

bool ApplyAttributeBonus(PlayerStatus& status, std::uint16_t parameter, std::int64_t bonus) {
  switch (static_cast<ServerParameter>(parameter)) {
    case ServerParameter::kStr:
      status.str_bonus = bonus;
      return true;
    case ServerParameter::kAgi:
      status.agi_bonus = bonus;
      return true;
    case ServerParameter::kVit:
      status.vit_bonus = bonus;
      return true;
    case ServerParameter::kInt:
      status.int_bonus = bonus;
      return true;
    case ServerParameter::kDex:
      status.dex_bonus = bonus;
      return true;
    case ServerParameter::kLuk:
      status.luk_bonus = bonus;
      return true;
    default:
      return false;
  }
}

std::int64_t DisplayedAspd(std::int64_t amotion_ms) noexcept {
  if (amotion_ms == kUnknownStat) {
    return kUnknownStat;
  }
  // Le délai plancher du jeu, celui dont on ne descend pas.
  constexpr std::int64_t kStillMotionMs = 2000;
  // Un point de vitesse d'attaque par centième de seconde gagné.
  constexpr std::int64_t kMsPerAspdPoint = 10;
  return (kStillMotionMs - amotion_ms) / kMsPerAspdPoint;
}

bool StatusPointsSpendable(const PlayerStatus& status) noexcept {
  if (status.status_point <= 0) {
    return false;
  }
  const std::array<std::int64_t, 6> costs = {status.str_cost, status.agi_cost, status.vit_cost,
                                             status.int_cost, status.dex_cost, status.luk_cost};
  return std::ranges::any_of(
      costs, [&status](std::int64_t cost) { return cost > 0 && cost <= status.status_point; });
}

bool SkillPointsSpendable(const PlayerStatus& status) noexcept { return status.skill_point > 0; }

}  // namespace ro::sim
