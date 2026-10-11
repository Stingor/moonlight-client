// Le compte d'une mission Taekwon, tenu par le client : voir `taekwon_mission.h`.

#include "world/taekwon_mission.h"

#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/sim/chat_line.h"

#include "chat/chat_monster_link.h"

namespace moonclient {
namespace {

using ro::core::Printed;
using ro::core::Tr;

// La couleur des avis d'étoile, celle de l'annonce de la mission.
constexpr std::uint32_t kMissionColor = 0xC8C8FF;

// Le coup venait-il du joueur, ou de ce qui le sert ?
bool StruckByUs(const ro::sim::World& world, std::uint32_t attacker_id) {
  if (attacker_id == world.player_id()) {
    return true;
  }
  const ro::sim::Actor* attacker = world.Find(attacker_id);
  return attacker != nullptr && attacker->summon_master_id == world.player_id();
}

}  // namespace

void TaekwonMission::Start(std::uint32_t monster_id, std::string monster_name,
                           std::uint32_t count) {
  target_ =
      Target{.monster_id = monster_id, .monster_name = std::move(monster_name), .count = count};
}

void TaekwonMission::Advance(ro::sim::World& world, std::uint32_t now_ms) {
  // LE DERNIER COUP de chaque monstre : celui qui l'achève quand la mort suit.
  for (const ro::sim::RecordedStrike& recorded : strikes_.Take(world.strikes())) {
    const ro::sim::Strike& strike = recorded.strike;
    if (strike.damage <= 0 && strike.second_damage <= 0) {
      continue;
    }
    last_blow_ours_.insert_or_assign(strike.target, StruckByUs(world, strike.attacker));
  }
  // Un monstre parti de la vue s'oublie : son identifiant peut revenir.
  std::erase_if(last_blow_ours_, [&world](const auto& kept) { return !world.Find(kept.first); });
  std::erase_if(seen_dead_, [&world](std::uint32_t id) { return world.Find(id) == nullptr; });
  for (const auto& [id, actor] : world.actors()) {
    if (actor.kind != ro::sim::ActorKind::kMonster || actor.posture != ro::sim::Posture::kDead ||
        !seen_dead_.insert(id).second) {
      continue;
    }
    const auto blow = last_blow_ours_.find(id);
    if (!target_.has_value() || actor.look.job != target_->monster_id ||
        blow == last_blow_ours_.end() || !blow->second || target_->count >= kTaekwonMissionGoal) {
      continue;
    }
    ++target_->count;
    std::string named = BuildMonsterLink(MonsterLink{.monster_id = target_->monster_id,
                                                     .rank = kMonsterRankOrdinary,
                                                     .name = target_->monster_name});
    if (named.empty()) {
      named = target_->monster_name;
    }
    world.RecordChat(
        ro::sim::ChatKind::kServer, 0,
        Printed(Tr("[Taekwon Mission] %s : %d / %d tués, il en reste %d"), named.c_str(),
                static_cast<int>(target_->count), static_cast<int>(kTaekwonMissionGoal),
                static_cast<int>(kTaekwonMissionGoal - target_->count)),
        now_ms, kMissionColor);
  }
}

}  // namespace moonclient
