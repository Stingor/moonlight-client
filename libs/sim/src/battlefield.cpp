// Le champ de bataille de la carte. Voir l'en-tête.

#include "ro/sim/battlefield.h"

#include <utility>

namespace ro::sim {
namespace {

// La case que le serveur donne à un coéquipier qui quitte la carte.
constexpr std::int16_t kMateGoneCell = -1;

}  // namespace

void Battlefield::TellCamp(std::uint32_t actor_id, std::uint16_t camp, std::uint32_t own_id) {
  // Un camp nul dit « hors de toute équipe » : l'acteur n'a plus de camp.
  if (camp == kNoBattleCamp) {
    camps_.erase(actor_id);
  } else {
    camps_[actor_id] = camp;
  }
  if (actor_id != own_id) {
    return;
  }
  own_camp_ = camp;
  if (camp == kNoBattleCamp) {
    return;
  }
  // NOTRE CAMP CHANGE : une autre manche commence, et les camps des autres
  // appartiennent à la précédente. Le serveur redit ceux de la nouvelle ; le
  // score et les coéquipiers, qu'il redit aussi, restent jusque-là.
  if (last_own_camp_ != kNoBattleCamp && last_own_camp_ != camp) {
    camps_.clear();
    camps_[actor_id] = camp;
  }
  last_own_camp_ = camp;
}

std::uint16_t Battlefield::CampOf(std::uint32_t actor_id) const {
  const auto found = camps_.find(actor_id);
  return found == camps_.end() ? kNoBattleCamp : found->second;
}

bool Battlefield::IsEnemy(std::uint32_t actor_id) const {
  const std::uint16_t camp = CampOf(actor_id);
  return own_camp_ != kNoBattleCamp && camp != kNoBattleCamp && camp != own_camp_;
}

bool Battlefield::IsAlly(std::uint32_t actor_id) const {
  const std::uint16_t camp = CampOf(actor_id);
  return own_camp_ != kNoBattleCamp && camp == own_camp_;
}

BattleSide Battlefield::own_side() const {
  if (own_camp_ == kNoBattleCamp) {
    return BattleSide::kUnknown;
  }
  bool seen_above = false;
  bool seen_below = false;
  for (const auto& entry : camps_) {
    seen_above = seen_above || entry.second > own_camp_;
    seen_below = seen_below || entry.second < own_camp_;
  }
  // Aucun camp adverse vu, ou des camps des deux côtés du nôtre : la règle ne
  // tranche pas.
  if (seen_above == seen_below) {
    return BattleSide::kUnknown;
  }
  return seen_above ? BattleSide::kLion : BattleSide::kEagle;
}

void Battlefield::TellScore(std::uint16_t lion, std::uint16_t eagle) {
  score_.lion = lion;
  score_.eagle = eagle;
}

void Battlefield::TellMate(std::uint32_t account_id, std::string name, std::uint16_t job,
                           std::int16_t x, std::int16_t y) {
  if (x == kMateGoneCell && y == kMateGoneCell) {
    mates_.erase(account_id);
    return;
  }
  mates_[account_id] = BattleMate{.account_id = account_id,
                                  .name = std::move(name),
                                  .job = job,
                                  .cell = BattleMateCell{.x = x, .y = y}};
}

void Battlefield::Clear() { *this = Battlefield(); }

}  // namespace ro::sim
