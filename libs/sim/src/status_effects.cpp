#include "ro/sim/status_effects.h"

namespace ro::sim {
namespace {

// Les valeurs d'`opt1` (`e_opt1`, `status.hpp`). ⚠ CINQ N'EXISTE PAS : le
// serveur saute ce rang, et la pétrification EN COURS vaut six.
constexpr std::uint16_t kOpt1Stone = 1;
constexpr std::uint16_t kOpt1Freeze = 2;
constexpr std::uint16_t kOpt1Stun = 3;
constexpr std::uint16_t kOpt1Sleep = 4;
constexpr std::uint16_t kOpt1StoneWait = 6;
constexpr std::uint16_t kOpt1Burning = 7;
constexpr std::uint16_t kOpt1Imprison = 8;

// Les bits d'`opt2` (`e_opt2`).
//
// ⚠ `OPT2_ANGELUS` (0x0020) N'EST PAS ICI, et ce n'est pas un oubli : le
// serveur ne le traduit pas non plus en icône — Angelus porte son propre EFST,
// annoncé par un vrai paquet d'état.
constexpr std::uint16_t kOpt2Poison = 0x0001;
constexpr std::uint16_t kOpt2Curse = 0x0002;
constexpr std::uint16_t kOpt2Silence = 0x0004;
constexpr std::uint16_t kOpt2Confusion = 0x0008;
constexpr std::uint16_t kOpt2Blind = 0x0010;
constexpr std::uint16_t kOpt2Bleeding = 0x0040;
constexpr std::uint16_t kOpt2DeadlyPoison = 0x0080;
constexpr std::uint16_t kOpt2Fear = 0x0100;

}  // namespace

void AilmentStatuses(std::uint16_t body_state, std::uint16_t health_state,
                     std::vector<StatusEffectId>& into) {
  into.clear();

  // 🔴 UNE VALEUR, DONC UN SEUL ÉTAT DU CORPS À LA FOIS. On ne peut pas dormir
  // ET être pétrifié : le serveur remplace, il n'ajoute pas.
  switch (body_state) {
    case kOpt1Stone:
      into.push_back(kEfstStoneCurse);
      break;
    case kOpt1Freeze:
      into.push_back(kEfstFreezing);
      break;
    case kOpt1Stun:
      into.push_back(kEfstStun);
      break;
    case kOpt1Sleep:
      into.push_back(kEfstSleep);
      break;
    case kOpt1StoneWait:
      into.push_back(kEfstStoneCurseInProgress);
      break;
    case kOpt1Burning:
      into.push_back(kEfstBurning);
      break;
    case kOpt1Imprison:
      into.push_back(kEfstImprison);
      break;
    default:
      break;
  }

  // 🔴 UN MASQUE, DONC AUTANT D'ÉTATS QUE DE BITS. Un empoisonné aveuglé porte
  // les deux, et l'ordre est celui du serveur.
  const auto carries = [health_state](std::uint16_t bit) { return (health_state & bit) != 0; };
  if (carries(kOpt2Poison)) {
    into.push_back(kEfstPoison);
  }
  if (carries(kOpt2Curse)) {
    into.push_back(kEfstCurse);
  }
  if (carries(kOpt2Silence)) {
    into.push_back(kEfstSilence);
  }
  if (carries(kOpt2Confusion)) {
    into.push_back(kEfstConfusion);
  }
  if (carries(kOpt2Blind)) {
    into.push_back(kEfstBlind);
  }
  if (carries(kOpt2Bleeding)) {
    into.push_back(kEfstBleeding);
  }
  if (carries(kOpt2DeadlyPoison)) {
    into.push_back(kEfstHeavyPoison);
  }
  if (carries(kOpt2Fear)) {
    into.push_back(kEfstFear);
  }
}

bool DarkensTheMap(const std::set<StatusEffectId>& player_statuses) {
  if (player_statuses.contains(kEfstNight) || player_statuses.contains(kEfstSoulLink)) {
    return true;
  }
  const auto spirit = player_statuses.lower_bound(kEfstSoulFairy);
  return spirit != player_statuses.end() && *spirit <= kEfstSoulDivision;
}

}  // namespace ro::sim
