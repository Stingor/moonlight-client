#include "world/battle_log.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "ro/sim/world.h"

#include "world/actor_view.h"
#include "world/struck_place.h"

namespace moonclient {
namespace {

// Le nom qu'un identifiant porte dans le monde, ou rien.
[[nodiscard]] std::string_view NameIn(const ro::sim::World& world, std::uint32_t id) {
  const ro::sim::Actor* actor = world.Find(id);
  return actor != nullptr ? std::string_view(actor->name) : std::string_view();
}

// Vrai quand l'acteur a le métier d'un joueur — un PNJ vêtu d'un métier
// compris, comme le client, qui lit le métier et non l'espèce.
[[nodiscard]] bool HasPlayerJob(const ro::sim::World& world, std::uint32_t id) {
  const ro::sim::Actor* actor = world.Find(id);
  return actor != nullptr && ComposedAsPlayer(*actor);
}

}  // namespace

void BattleLog::Notice(ro::sim::World& world, ro::render::ActorWardrobe* wardrobe,
                       const ro::sim::PartyRoster& party, std::uint32_t now_ms) {
  const std::uint32_t self_id = world.player_id();
  for (const ro::sim::RecordedStrike& taken : noticed_.Take(world.strikes())) {
    const ro::sim::Strike& strike = taken.strike;
    // LE CLIENT NE REJOUE RIEN SANS CIBLE : c'est elle qui tient la file.
    if (world.Find(strike.target) == nullptr) {
      continue;
    }
    const ro::sim::Actor* attacker = world.Find(strike.attacker);
    const StrikeSides sides{
        .target_is_player = world.Find(strike.target)->kind == ro::sim::ActorKind::kPlayer,
        .target_is_self = self_id != 0 && strike.target == self_id,
        .attacker_is_self = self_id != 0 && strike.attacker == self_id,
        .attacker_is_known = attacker != nullptr,
        .attacker_is_player = attacker != nullptr && attacker->kind == ro::sim::ActorKind::kPlayer,
        .attacker_is_ground_unit = StrikeByGroundUnit(world, strike),
        .attacker_job = attacker != nullptr ? attacker->look.job : 0U};
    std::uint32_t first_hit_ms = strike.started_ms;
    if (wardrobe != nullptr) {
      first_hit_ms += static_cast<std::uint32_t>(HitLandsAtMs(*wardrobe, world, strike));
    }
    const std::string target_name = BattleName(*words_, NameIn(world, strike.target));
    const std::string attacker_name = BattleName(*words_, NameIn(world, strike.attacker));
    for (const BattleMessage& message :
         BattleMessagesOf(strike, sides, first_hit_ms, HasPlayerJob(world, strike.target),
                          HasPlayerJob(world, strike.attacker))) {
      pending_.push_back(Pending{message, target_name, attacker_name});
    }
  }
  // Les lignes dues, dans l'ordre de leur instant : un coup multiple et un coup
  // simple reçus ensemble s'entrelacent comme leurs frappes.
  std::ranges::stable_sort(pending_, {},
                           [](const Pending& waiting) { return waiting.message.due_ms; });
  const auto due = std::ranges::find_if(
      pending_, [now_ms](const Pending& waiting) { return waiting.message.due_ms > now_ms; });
  for (auto it = pending_.begin(); it != due; ++it) {
    const std::optional<BattleLogLine> line =
        BattleLogLineFor(*words_, it->message, self_id, party, it->target_name, it->attacker_name);
    if (line.has_value()) {
      world.RecordChat(line->kind, 0, line->text, it->message.due_ms, line->color);
    }
  }
  pending_.erase(pending_.begin(), due);
}

}  // namespace moonclient
