#include "survivors/arena_meter.h"

#include <algorithm>
#include <unordered_map>

namespace moonclient {
namespace {

// Le niveau du skill que le héros possède, zéro pour un coup sans skill.
[[nodiscard]] std::uint16_t SkillLevelOf(const ro::sim::SurvivorsHero& hero,
                                         std::uint16_t skill_id) {
  const auto owned = std::ranges::find(hero.skills, skill_id, &ro::sim::OwnedSkill::skill_id);
  return owned != hero.skills.end() ? owned->level : std::uint16_t{0};
}

}  // namespace

void RecordArenaBlows(const ro::sim::SurvivorsNews& news, const ro::sim::SurvivorsField& field,
                      const ArenaFoeNamer& foe_name_of, ro::sim::DamageMeter& meter,
                      std::uint32_t now_ms) {
  const std::uint32_t self_id = meter.self_id();
  if (!news.hits.empty()) {
    // Le monstre tombé dans l'image n'est plus dans le champ : sa chute le nomme.
    std::unordered_map<std::uint32_t, std::uint16_t> foe_id_of;
    foe_id_of.reserve(field.foes.size() + news.falls.size());
    for (const ro::sim::SurvivorsFoe& foe : field.foes) foe_id_of.emplace(foe.serial, foe.foe_id);
    for (const ro::sim::SurvivorsFall& fall : news.falls) {
      foe_id_of.emplace(fall.foe_serial, fall.foe_id);
    }
    std::unordered_map<std::uint16_t, std::string> names;
    for (const ro::sim::SurvivorsHit& hit : news.hits) {
      ro::sim::MeterBlow blow{.mode = ro::sim::MeterMode::kDamageDone,
                              .who = self_id,
                              .amount = hit.amount,
                              .skill_id = hit.skill_id,
                              .skill_level = SkillLevelOf(field.hero, hit.skill_id),
                              .other_id = hit.foe_serial};
      if (const auto found = foe_id_of.find(hit.foe_serial); found != foe_id_of.end()) {
        auto [named, fresh] = names.try_emplace(found->second);
        if (fresh) named->second = foe_name_of(found->second);
        blow.other_name = named->second;
      }
      meter.Record(blow, now_ms);
    }
  }
  if (news.hero_hurt > 0) {
    meter.Record(
        ro::sim::MeterBlow{
            .mode = ro::sim::MeterMode::kDamageTaken, .who = self_id, .amount = news.hero_hurt},
        now_ms);
  }
}

}  // namespace moonclient
