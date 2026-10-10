// LES COMPÉTENCES SANS DÉGÂTS, LES INCANTATIONS ET LES JAUGES : la part du monde
// qui les note sur qui les porte.
//
// Elle vit à part de `world.cpp` pour tenir le plafond de huit cents lignes.

#include <algorithm>
#include <cstdint>
#include <optional>

#include "ro/sim/skill_side_effects.h"
#include "ro/sim/world.h"

#include "facing_rules.h"

namespace ro::sim {

bool World::RecordSupport(std::uint32_t target_id, const SupportHit& hit) {
  // LE SOIN SE COMPTE, cible en vue ou non ; un Sanctuary va à son lanceur.
  {
    damage_meter_.SetSelf(player_id_);
    const auto unit = skill_units_.find(hit.source_id);
    const std::uint32_t healer =
        unit != skill_units_.end() ? unit->second.creator_id : hit.source_id;
    const auto by = actors_.find(healer);
    const auto on = actors_.find(target_id);
    damage_meter_.NoteHeal(hit, healer, target_id, by != actors_.end() ? &by->second : nullptr,
                           on != actors_.end() ? &on->second : nullptr);
  }
  // LA POSTURE D'EMPLOI VA AU LANCEUR, même quand la cible est hors de vue.
  if (const auto caster = actors_.find(hit.source_id); caster != actors_.end()) {
    caster->second.skill_use = SkillUse{hit.skill_id, hit.started_ms, hit.amount, hit.succeeded};
    ApplyAnnouncedSkillToCaster(caster->second, hit, hit.source_id == player_id_,
                                actors_.contains(target_id));
    // Et il se tourne vers qui il soigne — règles dans `facing_rules.h`.
    facing::FaceSupporter(facing::Scene{actors_, skill_units_, walk_map_, player_id_},
                          caster->second, target_id, hit);
  }
  const auto found = actors_.find(target_id);
  if (found == actors_.end()) {
    return false;
  }
  found->second.support = hit;
  supports_.Record(target_id, hit);
  return true;
}

bool World::RecordCast(std::uint32_t id, const Cast& cast) {
  const auto caster = actors_.find(id);
  if (caster == actors_.end()) {
    return false;
  }
  caster->second.cast = cast;
  // LE LANCEUR SE TOURNE VERS CE QU'IL VISE, et c'est le geste le plus visible
  // de l'incantation — règles dans `facing_rules.h`.
  facing::FaceCaster(facing::Scene{actors_, skill_units_, walk_map_, player_id_}, caster->second,
                     cast);
  return true;
}

bool World::CancelCast(std::uint32_t id, std::uint32_t now_ms) {
  const auto found = actors_.find(id);
  if (found == actors_.end() || !found->second.cast.has_value()) {
    return false;
  }
  Cast& cast = *found->second.cast;
  // Une incantation déjà échue n'a plus rien à couper ; une autre s'arrête là
  // où on en est, jamais à zéro — une durée nulle dirait « instantanée », et
  // une incantation instantanée ne se quitte pas.
  const std::uint32_t played_ms = now_ms > cast.started_ms ? now_ms - cast.started_ms : 0;
  if (played_ms < cast.duration_ms) {
    cast.duration_ms = std::max<std::uint32_t>(played_ms, 1);
    cast.cancelled = true;
  }
  return true;
}

bool World::SetGauge(std::uint32_t id, std::optional<SkillGauge> gauge) {
  const auto found = actors_.find(id);
  if (found == actors_.end()) {
    return false;
  }
  found->second.gauge = gauge;
  return true;
}

}  // namespace ro::sim
