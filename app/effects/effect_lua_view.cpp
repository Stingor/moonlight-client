#include <array>
#include <cstdint>
#include <utility>

#include "ro/render/actor_effect_table.h"
#include "ro/render/effect_class.h"
#include "ro/render/skill_lua_effect.h"
#include "ro/sim/inventory.h"
#include "ro/sim/movement.h"
#include "ro/sim/strike_log.h"
#include "ro/sim/world.h"

#include "effects/effect_class_view.h"
#include "effects/effect_view.h"
#include "world/actor_view.h"
#include "world/struck_place.h"

// LES EFFETS QUE SOIXANTE-SIX COMPÉTENCES TIENNENT DE LEUR LUA.
//
// 🔴 UN FICHIER À PART PARCE QUE LA SOURCE EST AUTRE. Tout le reste de ce
// dossier lit des tables compilées dans le client ; celles-ci viennent de
// `skilleffectinfolist.lub`, dans l'installation du joueur. Les mêler aux
// autres ferait croire que le client les porte en dur — il ne les porte pas,
// et c'est précisément pourquoi elles ont manqué si longtemps.
//
// Voir `ro/render/skill_lua_effect.h` pour les quatre fonctions du client qui
// interrogent ce Lua, et pour les quatre champs qu'aucune ne lit.

namespace moonclient {

void EffectsInPlay::NoticeLuaEffects(const ro::sim::World& world, const MapStage& stage,
                                     ro::sim::WalkPlanner& planner) {
  const auto ground_of = [&](const ro::sim::Actor& actor, std::uint32_t at_ms) {
    const ro::sim::FractionalCell cell =
        ro::sim::InterpolatedCell(actor, planner.PathFor(actor), at_ms);
    const std::array<float, 3> sol = GroundPositionAt(stage, cell.x, cell.y);
    return glm::vec3(sol[0], sol[1], sol[2]);
  };
  const auto sow = [&](const ro::sim::Actor& bearer, std::uint16_t effect_id,
                       std::uint32_t bearer_id, std::uint32_t at_ms) {
    PlayById(effect_id, ground_of(bearer, at_ms), at_ms, bearer_id);
  };
  // UNE CLASSE NAÎT SUR LE LANCEUR, liée à sa cible quand ce n'est pas lui-même :
  // `onTarget` ne déplace que les effets primitifs. Elle lit à sa naissance les
  // sphères de son lanceur.
  const auto sow_class = [&](const ro::sim::Actor& caster, std::uint16_t effect_id,
                             std::uint32_t target_id, std::uint32_t at_ms) {
    EffectClassesInPlay::Birth birth;
    birth.effect_id = effect_id;
    birth.owner_id = caster.id;
    birth.ground_position = ground_of(caster, at_ms);
    if (const ro::sim::Actor* target = world.Find(target_id);
        target != nullptr && target_id != caster.id) {
      birth.bound_id = target_id;
      birth.bound_ground_position = ground_of(*target, at_ms);
    }
    birth.owner_charm_element = caster.spirit_charm_element;
    for (const ro::sim::PlayedEffect& carried : caster.effects) {
      if (carried.id >= ro::render::kFirstElementalOrbEffect &&
          carried.id <= ro::render::kLastElementalOrbEffect) {
        birth.owner_charm_count = carried.value.value_or(0);
      }
    }
    static_cast<void>(classes_.Spawn(birth, at_ms));
    // LES KUNAI EN ÉTOILE FONT TOURNER LEUR LANCEUR : voir
    // `SpinDegreesAfterEffects`. Notés sur lui, ils ne renaissent pas.
    if (effect_id == ro::render::kKunaiStarEffect) {
      actor_effects_to_record_.push_back({caster.id, effect_id, at_ms});
      per_map_.noticed.insert_or_assign(std::pair{caster.id, std::uint32_t{effect_id}}, at_ms);
    }
  };

  // LE CHEMIN DE L'EMPLOI : celui d'une compétence annoncée sans dégâts —
  // toutes, dans l'ordre : voir `ro::sim::SupportLog`.
  for (const ro::sim::RecordedSupport& taken : per_map_.lua_supports.Take(world.supports())) {
    const ro::sim::Actor* const target = world.Find(taken.target_id);
    if (target == nullptr) {
      continue;
    }
    const std::uint32_t id = taken.target_id;
    const ro::sim::Actor& actor = *target;
    const ro::sim::SupportHit& given = taken.support;
    // ⚠ `onTarget` NE DÉPLACE QUE CET EMPLACEMENT-LÀ. Les effets de cible ont
    // déjà le leur, et appliquer le drapeau aux deux poserait deux fois le même
    // sur la victime.
    //
    // Le porteur du paquet sans dégâts EST la cible ; sa source est
    // `source_id`. Le drapeau choisit donc entre les deux.
    const bool on_target = ro::render::SkillLuaPutsCasterEffectOnTarget(given.skill_id);
    const std::uint32_t bearer_id = on_target ? id : given.source_id;
    const ro::sim::Actor* bearer = on_target ? &actor : world.Find(given.source_id);
    const ro::sim::Actor* caster = world.Find(given.source_id);
    for (const ro::render::SkillLuaEffectEntry& entry :
         ro::render::SkillLuaEffectsFor(given.skill_id, ro::render::SkillLuaSlot::kCaster)) {
      if (ro::render::IsPortedEffectClass(entry.effect_id)) {
        if (caster != nullptr) {
          sow_class(*caster, entry.effect_id, id, given.started_ms);
        }
      } else if (bearer != nullptr) {
        sow(*bearer, entry.effect_id, bearer_id, given.started_ms);
      }
    }
  }

  // LE CHEMIN DU COUP : l'effet naît sur ce qui reçoit.
  //
  // ⚠ PAS POUR UNE UNITÉ AU SOL : son coup part vers un autre répartiteur du
  // client, qui ne consulte pas ce Lua. Sans cette porte, une unité qui bat
  // une fois par case rejouerait l'effet à chaque battement.
  for (const ro::sim::RecordedStrike& taken : lua_strikes_.Take(world.strikes())) {
    const ro::sim::Strike& strike = taken.strike;
    const std::uint32_t id = strike.target;
    const ro::sim::Actor* const struck = world.Find(id);
    if (struck == nullptr || strike.skill_id == 0 || StrikeByGroundUnit(world, strike)) {
      continue;
    }
    const ro::sim::Actor& actor = *struck;
    for (const ro::render::SkillLuaEffectEntry& entry :
         ro::render::SkillLuaEffectsFor(strike.skill_id, ro::render::SkillLuaSlot::kTarget)) {
      sow(actor, entry.effect_id, id, strike.started_ms);
    }
  }
}

}  // namespace moonclient
