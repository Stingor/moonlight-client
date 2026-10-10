// LES ACTIONS NOMMÉES DES COMPÉTENCES : ce qu'une méthode sème à son moment, et
// ce qu'elle fait de chaque semis — le tourner vers la cible, le retourner, le
// grandir, le coucher au sol, le retarder de l'allure du lanceur.
//
// Séparées de `effect_named.cpp`, qui garde la file commune des semis nommés et
// les états : ici vit ce qui ne vaut que pour une compétence, parce qu'une
// compétence seule a une source, une cible et un niveau.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/render/effect_frame.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/skill_named_recipe.h"
#include "ro/render/skill_named_table.h"
#include "ro/sim/movement.h"

#include "effects/effect_named_sounds.h"
#include "effects/effect_node_view.h"
#include "effects/effect_requests.h"
#include "effects/effect_rules.h"
#include "effects/effect_view.h"

namespace moonclient {
namespace {

// ⚠ LA CASE, ET NON LA POSITION FLOTTANTE, pour la file : un semis nommé se
// rejoue tant qu'il dure, et il se rejoue là où il est né. Ce que la position
// exacte a de plus s'ajoute au décalage — voir `ExactOffset`.
[[nodiscard]] ro::sim::CellPosition CellOf(const ro::sim::FractionalCell& cell) {
  return ro::sim::CellPosition{static_cast<std::uint16_t>(cell.x),
                               static_cast<std::uint16_t>(cell.y)};
}

[[nodiscard]] ro::sim::FractionalCell FractionalCellOf(ro::sim::WalkPlanner& planner,
                                                       const ro::sim::Actor& actor,
                                                       std::uint32_t at_ms) {
  return ro::sim::InterpolatedCell(actor, planner.PathFor(actor), at_ms);
}

[[nodiscard]] glm::vec3 GroundOf(const MapStage& stage, const ro::sim::FractionalCell& cell) {
  const std::array<float, 3> sol = GroundPositionAt(stage, cell.x, cell.y);
  return glm::vec3(sol[0], sol[1], sol[2]);
}

// CE QUE LA POSITION EXACTE A DE PLUS QUE SA CASE : le client sème un effet
// laissé à une position au point même où se tient le porteur, et un porteur en
// marche est entre deux cases.
[[nodiscard]] glm::vec3 ExactOffset(const MapStage& stage, const ro::sim::FractionalCell& cell) {
  const ro::sim::CellPosition entiere = CellOf(cell);
  return GroundOf(stage, cell) -
         GroundOf(stage, ro::sim::FractionalCell{static_cast<float>(entiere.x),
                                                 static_cast<float>(entiere.y)});
}

// Un décalage relevé dans le repère du client, dans le nôtre : la hauteur et la
// profondeur changent de signe.
[[nodiscard]] glm::vec3 OurOffset(const glm::vec3& client_offset) {
  return glm::vec3(client_offset.x, -client_offset.y, -client_offset.z);
}

// Où naît un effet, et sur qui : l'acteur qui le porte — nul pour un effet
// posé sur une case ou sur une unité —, et sa position.
struct NamedCarrier {
  const ro::sim::Actor* actor = nullptr;
  ro::sim::FractionalCell cell;
};

}  // namespace

void EffectsInPlay::PlayNamedSkillOnPair(const MapStage& stage, ro::sim::WalkPlanner& planner,
                                         std::uint16_t skill_id, std::uint8_t slot,
                                         const ro::sim::Actor* source, const ro::sim::Actor* target,
                                         std::uint32_t started_ms, std::uint32_t attack_motion_ms,
                                         const ro::sim::SkillUnit* fallback_unit) {
  NamedSkillMoment moment;
  moment.skill_id = skill_id;
  moment.slot = slot;
  moment.source = source;
  moment.target = target;
  moment.fallback_unit = fallback_unit;
  moment.attack_motion_ms = attack_motion_ms;
  moment.started_ms = started_ms;
  static_cast<void>(PlayNamedMoment(stage, planner, moment));
}

bool EffectsInPlay::PlayNamedSkillOnHit(const MapStage& stage, ro::sim::WalkPlanner& planner,
                                        std::uint16_t skill_id, const ro::sim::Actor* source,
                                        const ro::sim::Actor* target, std::uint32_t hit_index,
                                        std::uint32_t attack_motion_ms, std::uint32_t landed_ms,
                                        const std::optional<ro::sim::CellPosition>& unit_cell) {
  // UNE UNITÉ AU SOL FRAPPÉE est la cible du moment, à sa case : le client la
  // passe à la méthode comme il y passerait un acteur.
  if (target == nullptr && !unit_cell.has_value()) {
    return false;
  }
  NamedSkillMoment moment;
  moment.skill_id = skill_id;
  moment.slot = ro::render::kSkillNamedOnHit;
  moment.source = source;
  moment.target = target;
  if (target == nullptr) {
    moment.target_cell = unit_cell;
  }
  moment.hit_index = hit_index;
  moment.attack_motion_ms = attack_motion_ms;
  moment.started_ms = landed_ms;
  if (!PlayNamedMoment(stage, planner, moment)) {
    return false;
  }
  // CE QUE LA MÉTHODE FAIT ENTENDRE TOUT DE SUITE, à chaque frappe comme elle
  // sème : voir `PlayNamedMethodSounds`.
  if (source != nullptr) {
    const ro::sim::CellPosition target_cell =
        target != nullptr ? CellOf(FractionalCellOf(planner, *target, landed_ms)) : *unit_cell;
    PlayNamedMethodSounds(NamedMethodFamily::kSkill, skill_id, ro::render::kSkillNamedOnHit,
                          source->id, CellOf(FractionalCellOf(planner, *source, landed_ms)),
                          target_cell, per_map_.just_sown_named_sounds);
  }
  return true;
}

bool EffectsInPlay::PlayNamedMoment(const MapStage& stage, ro::sim::WalkPlanner& planner,
                                    const NamedSkillMoment& moment) {
  const std::span<const ro::render::SkillNamedEffectEntry> named =
      ro::render::SkillNamedEffects(moment.skill_id, moment.slot);
  if (named.empty()) {
    return false;
  }
  const std::uint32_t at_ms = moment.started_ms;
  // LES DEUX BOUTS DU MOMENT, au sol : la source, et la cible — un acteur, une
  // case visée, ou l'unité que le coup frappe.
  std::optional<NamedCarrier> source;
  if (moment.source != nullptr) {
    source = NamedCarrier{moment.source, FractionalCellOf(planner, *moment.source, at_ms)};
  }
  std::optional<NamedCarrier> target;
  if (moment.target != nullptr) {
    target = NamedCarrier{moment.target, FractionalCellOf(planner, *moment.target, at_ms)};
  } else if (moment.target_cell.has_value()) {
    target =
        NamedCarrier{nullptr, ro::sim::FractionalCell{static_cast<float>(moment.target_cell->x),
                                                      static_cast<float>(moment.target_cell->y)}};
  }
  std::optional<NamedCarrier> fallback;
  if (moment.fallback_unit != nullptr) {
    fallback = NamedCarrier{
        nullptr, ro::sim::FractionalCell{static_cast<float>(moment.fallback_unit->cell.x),
                                         static_cast<float>(moment.fallback_unit->cell.y)}};
  }

  // CE QUE LA MÉTHODE LIT DU MOMENT, une fois pour toutes ses entrées.
  ro::render::NamedSowingContext context;
  context.level = moment.level;
  context.hit_index = moment.hit_index;
  // L'ANGLE À L'ÉCRAN, de la source vers la cible — ou vers la case visée.
  if (camera_.has_value() && source.has_value() && target.has_value()) {
    context.screen_degrees = ro::render::NamedScreenDegrees(
        ro::render::NamedScreenPoint(*camera_, GroundOf(stage, source->cell)),
        ro::render::NamedScreenPoint(*camera_, GroundOf(stage, target->cell)));
  }
  if (moment.source != nullptr) {
    context.source_charm_element = moment.source->spirit_charm_element;
    if (actor_view_.sprite_direction_of) {
      context.source_sprite_direction = actor_view_.sprite_direction_of(*moment.source, at_ms);
    }
    if (actor_view_.pace_of) {
      context.source_pace =
          actor_view_.pace_of(*moment.source, moment.skill_id, moment.attack_motion_ms);
    }
  }
  const std::uint32_t target_id = moment.target != nullptr ? moment.target->id : 0;
  ro::render::NamedSowingDeck deck;
  for (const ro::render::SkillNamedEffectEntry& entry : named) {
    // QUI PORTE : la source, l'unité frappée, ou la cible.
    const std::optional<NamedCarrier>& bearer = entry.on_source          ? source
                                                : entry.on_fallback_unit ? fallback
                                                                         : target;
    // ⚠ SANS PORTEUR, RIEN NE NAÎT : la méthode du client vérifie le sien.
    if (!bearer.has_value()) {
      continue;
    }
    // ⚠ UNE PORTE SE LIT UNE FOIS PAR ENTRÉE, avant tout tirage : c'est la
    // méthode qui choisit son bras, pas chaque éclat.
    if (!NamedGateOpens(entry, target_id)) {
      continue;
    }
    // ⚠ UNE ALLURE ILLISIBLE — une source sans tenue chargée — laisse le délai
    // de la fiche : `ExpandNamedEntry` le rend alors.
    const std::uint32_t bearer_id = bearer->actor != nullptr ? bearer->actor->id : 0;
    const glm::vec3 bearer_ground = GroundOf(stage, bearer->cell);
    for (const ro::render::NamedSowing& sowing :
         ro::render::ExpandNamedEntry(entry, context, deck, aim_generator_)) {
      const glm::vec3 offset = OurOffset(sowing.client_offset);
      // UN EFFET PAR SON NUMÉRO, que la méthode pose au même instant : attaché
      // à son porteur, ou laissé à sa position, et visant la cible quand elle
      // le dit.
      if (entry.effect_id != 0) {
        // LE CERCLE D'INCANTATION DE L'ACTION : sur le lanceur, tant que dure
        // l'incantation qu'il prépare — et rien sans elle, la pose n'ayant
        // lieu qu'avec une durée.
        if (entry.life_from_cast) {
          const ro::sim::Actor* caster = bearer->actor;
          if (caster != nullptr && caster->cast.has_value() && caster->cast->duration_ms > 0) {
            static_cast<void>(SowCastRing(*this, per_map_, bearer_id, bearer_ground + offset,
                                          entry.effect_id, at_ms, caster->cast->duration_ms));
          }
          continue;
        }
        std::optional<glm::vec3> visee;
        if (entry.aimed_at_target && target.has_value()) {
          visee = GroundOf(stage, target->cell);
        }
        const bool attache = entry.anchor == ro::render::NamedEffectAnchor::kActor;
        static_cast<void>(PlayById(entry.effect_id, bearer_ground + offset, at_ms,
                                   attache ? bearer_id : 0, /*endless=*/false,
                                   /*life_floor_ticks=*/0, /*announce_sound=*/true,
                                   /*life_exact_ticks=*/0, /*instance_scale=*/0.0F,
                                   /*instance_second=*/0.0F, visee));
        continue;
      }
      // ⚠ LE NOM FORMÉ NE VIT QUE LE TEMPS DE CET APPEL : c'est celui de la
      // FICHE qui se garde, et une fiche absente ne montre rien — chez le client
      // non plus.
      const ro::formats::NamedEffect* record = NamedEffectFor(sowing.name);
      if (record == nullptr) {
        continue;
      }
      // ⚠ UN EFFET POSÉ PAR UNE UNITÉ OU UNE CASE n'a pas d'acteur : il reste à
      // sa position, quoi que dise son ancrage.
      const ro::render::NamedEffectAnchor ancrage =
          bearer->actor != nullptr ? entry.anchor : ro::render::NamedEffectAnchor::kPosition;
      PlayNamedSkillEntry(entry, record->name, bearer_id, CellOf(bearer->cell), ancrage,
                          moment.level, at_ms, ExactOffset(stage, bearer->cell) + offset, sowing);
    }
  }
  return true;
}

bool SowCastRing(EffectsInPlay& effects, EffectMapMemory& memory, std::uint32_t actor_id,
                 glm::vec3 ground_position, std::uint32_t effect_id, std::uint32_t started_ms,
                 std::uint32_t cast_ms) {
  // 🔴 LES ANNEAUX DONT LE BÂTISSEUR RECOPIE LA VIE DE L'INSTANCE vivent
  // l'incantation d'un seul tenant, et ne se relancent pas : voir
  // `ro::render::EffectLifeUnderCast`. Les autres gardent la vie de leur patron,
  // et c'est la RELANCE qui les fait durer — voir `NoticeSkills`.
  const std::int32_t life_under_cast = ro::render::EffectLifeUnderCast(effect_id, cast_ms);
  const bool sown = effects.PlayById(effect_id, ground_position, started_ms, actor_id,
                                     /*endless=*/false, /*life_floor_ticks=*/0,
                                     /*announce_sound=*/true, life_under_cast);
  // 🔴 TOUT CE QUI EST SEMÉ SE RETIENT, pour qu'une interruption l'éteigne ;
  // ⚠ MAIS TOUT NE SE RELANCE PAS : voir `CastAuraRelaunches`. Un cercle par
  // effet : l'aura de la table et le cercle de l'action se relancent chacun à
  // son rythme.
  if (sown) {
    std::vector<EffectMapMemory::CastRing>& rings = memory.cast_ring_sown[actor_id];
    if (std::ranges::none_of(rings, [effect_id](const EffectMapMemory::CastRing& ring) {
          return ring.effect_id == effect_id;
        })) {
      rings.push_back(EffectMapMemory::CastRing{
          started_ms, effect_id,
          life_under_cast == 0 &&
              CastAuraRelaunches(effect_id, EffectNodesInPlay::ProceduralLifeTicks(effect_id))});
    }
  }
  return sown;
}

bool EffectsInPlay::NamedGateOpens(const ro::render::SkillNamedEffectEntry& entry,
                                   std::uint32_t target_id) {
  using ro::render::NamedEffectGate;
  switch (entry.gate) {
    case NamedEffectGate::kAlways:
      return true;
    case NamedEffectGate::kUnlessMinimalEffects:
      return !minimal_effects_;
    case NamedEffectGate::kIfTargetCarries:
    case NamedEffectGate::kUnlessTargetCarries: {
      // PORTER, C'EST AVOIR L'EFFET VIVANT sous ce nom : à l'écran, ou en
      // attente de son tour pour une boucle qui se réinscrit.
      const bool carries =
          std::ranges::any_of(per_map_.playing,
                              [&](const Playing& shown) {
                                return shown.named_owner_id == target_id &&
                                       shown.named_as == entry.gate_name;
                              }) ||
          std::ranges::any_of(per_map_.pending_named, [&](const PendingNamed& waiting) {
            return !waiting.from_unit && waiting.owner_id == target_id &&
                   waiting.name == entry.gate_name;
          });
      return carries == (entry.gate == NamedEffectGate::kIfTargetCarries);
    }
    case NamedEffectGate::kOddDraw:
    case NamedEffectGate::kEvenDraw: {
      // Les deux bras d'un même tirage le lisent tour à tour : le premier tire,
      // le second reprend ce qui est tombé.
      if (entry.gate == NamedEffectGate::kOddDraw) {
        named_draw_is_odd_ = (ro::render::NextEffectNodeDraw(aim_generator_) & 1U) != 0;
      }
      return named_draw_is_odd_ == (entry.gate == NamedEffectGate::kOddDraw);
    }
  }
  return true;
}

}  // namespace moonclient
