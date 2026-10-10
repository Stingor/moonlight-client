// CE QU'UN CHANGEMENT D'ÉTAT REÇU EN DIRECT MONTRE UNE FOIS.
//
// 🔴 EN DIRECT, ET JAMAIS À L'APPARITION. Un acteur qui entre dans le champ de
// vision déjà caché ou déjà muet ne fait ni fumée ni bruit : seul le paquet de
// changement d'état les déclenche. Les comptes de l'acteur le disent — voir
// `ro::sim::Actor::option_rise_count` et ses voisins —, et un acteur jamais vu
// retient ses comptes sans rien jouer.
//
// ⚠ SAUF CE QUE LE CORPS QUITTE : un gelé qui dégèle fait éclater sa glace quel
// que soit le paquet qui l'a dit. Voir `ro::render::ReactToAilments`.

#include <array>
#include <optional>
#include <string_view>

#include <glm/vec3.hpp>

#include "ro/render/ailment_sprites.h"
#include "ro/render/effect_duration.h"
#include "ro/render/effect_node.h"
#include "ro/render/option_effect_table.h"
#include "ro/render/pvp_rank_tint.h"
#include "ro/render/silence_dots.h"
#include "ro/render/skill_suite_table.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "world/map_stage.h"

namespace moonclient {

void EffectsInPlay::NoticeStateChanges(const ro::sim::World& world, const MapStage& stage,
                                       ro::sim::WalkPlanner& planner, std::uint32_t now_ms) {
  std::erase_if(per_map_.seen_state_counts,
                [&world](const auto& entry) { return !world.actors().contains(entry.first); });
  for (const auto& [id, actor] : world.actors()) {
    const EffectMapMemory::SeenStateCounts now_counts{actor.option_rise_count,
                                                      actor.option_fall_count,
                                                      actor.state_change_count, actor.body_state};
    const auto seen = per_map_.seen_state_counts.find(id);
    if (seen == per_map_.seen_state_counts.end()) {
      per_map_.seen_state_counts.emplace(id, now_counts);
      continue;
    }
    const EffectMapMemory::SeenStateCounts before = seen->second;
    seen->second = now_counts;
    const bool hide_rose =
        before.rises != now_counts.rises && (actor.option_risen & ro::sim::kOptionHide) != 0;
    const bool hide_fell =
        before.falls != now_counts.falls && (actor.option_fallen & ro::sim::kOptionHide) != 0;
    const bool state_changed = before.changes != now_counts.changes;
    const bool body_changed = before.body_state != now_counts.body_state;
    if (!hide_rose && !hide_fell && !state_changed && !body_changed) {
      continue;
    }
    const ro::sim::FractionalCell cell =
        ro::sim::InterpolatedCell(actor, planner.PathFor(actor), now_ms);
    const std::array<float, 3> ground = GroundPositionAt(stage, cell.x, cell.y);
    const glm::vec3 feet(ground[0], ground[1], ground[2]);
    const bool own_player = id == world.player_id();
    const ro::render::AilmentReactions ailments =
        ro::render::ReactToAilments(ro::render::AilmentChange{
            .body_before = before.body_state,
            .body_after = actor.body_state,
            .health_after = actor.health_state,
            .live = state_changed,
            .own_player = own_player,
            .darkness_carried = own_player && Carries(ro::render::kBlindDarknessEffect, id),
            .poison_mist_carried = own_player && Carries(ro::render::kPoisonMistEffect, id),
            .imprison_carried = Carries(ro::render::kImprisonEffect, id),
            .deep_blind_held = actor.status_effects.contains(ro::sim::kEfstDeepBlind),
            .blind_seen = per_map_.blind_seen,
            .poison_seen = per_map_.poison_seen});
    if (own_player) {
      per_map_.blind_seen = ailments.blind_seen;
      per_map_.poison_seen = ailments.poison_seen;
    }
    // LA GLACE ÉCLATE LÀ OÙ LE GELÉ SE TENAIT, et n'en bouge plus.
    if (ailments.shatter) {
      static_cast<void>(nodes_->PlayActOnce(ro::render::kFrozenIceSpritePath, feet,
                                            ro::render::kFrozenShatterFrameSteps,
                                            ro::render::kFrozenShatterAction));
    }
    // LES RETRAITS D'ABORD : un corps qui passe de la brûlure à la prison perd
    // l'une avant de prendre l'autre. Ce qui dure indéfiniment chez le client —
    // le noir, la brume, la brûlure — est une aura, qui tient jusqu'au retrait.
    for (const std::uint32_t lifted : ailments.lifted_effects) {
      StopById(lifted, id);
    }
    for (const std::uint32_t posed : ailments.posed_effects) {
      const bool endless = ro::render::EffectDurationTicks(posed) == ro::render::kEffectLifeForever;
      static_cast<void>(PlayStateEffect(posed, id, std::nullopt, ground, now_ms, endless));
    }
    for (const std::string_view sound : ailments.sounds) {
      per_map_.just_sown_named_sounds.push_back(
          SownNamedSound{sound, ro::sim::StandingCell(actor)});
    }
    // ⚠ DEUX CHANGEMENTS DANS LA MÊME IMAGE jouent chacun le sien : l'entrée
    // puis la sortie.
    for (const bool now_hidden : {true, false}) {
      if (now_hidden ? !hide_rose : !hide_fell) {
        continue;
      }
      const ro::render::HideTransitionLook look =
          ro::render::HideTransitionLookFor(now_hidden, actor.look.job);
      if (look.smoke_puff) {
        static_cast<void>(nodes_->PlayActOnce(ro::render::kSmokePuffSprite, feet,
                                              ro::render::kSmokePuffFrameSteps));
      }
      if (look.effect_id != 0) {
        static_cast<void>(PlayById(look.effect_id, feet, now_ms, id));
      }
      // 🔴 L'ANNEAU DU CLASSEMENT PVP d'un AUTRE qui se cache s'éteint avec
      // lui : le laisser tourner trahirait la case où il se tient.
      if (now_hidden && id != world.player_id()) {
        StopById(ro::render::kPvpTopRankEffect, id);
      }
    }
    if (state_changed && (actor.health_state & ro::sim::kHealthSilenced) != 0) {
      per_map_.just_sown_named_sounds.push_back(
          SownNamedSound{ro::render::kSilenceSound, ro::sim::StandingCell(actor)});
    }
  }
}

}  // namespace moonclient
