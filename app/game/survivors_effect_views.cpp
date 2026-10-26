// LES EFFETS DANS LES VUES DU MODE SURVIVORS : la colonne des effets en cours
// et les avis des objets ramassés au HUD, les effets permanents dans la pause.
// Les chiffres viennent de `survivors_effect_desk.h` ; ici s'ajoutent les noms
// et les icônes des tables du jeu.

#include <cstddef>
#include <cstdint>
#include <optional>

#include "ro/sim/survivors_meta.h"

#include "game/survivors_views.h"

namespace moonclient {
namespace {

[[nodiscard]] const ro::sim::SurvivorsMeta& Meta() { return ro::sim::ShippedSurvivorsMeta(); }

// Le nœud de l'arbre d'un skill nommé par son nom RO (`AL_BLESSING`).
[[nodiscard]] const ro::sim::SkillNode* NodeNamed(const char* idname,
                                                  const SurvivorsTables& tables) {
  return tables.skill_tree != nullptr ? tables.skill_tree->FindByName(idname) : nullptr;
}

}  // namespace

void SurvivorsViews::ComposeHudEffects(const ro::sim::SurvivorsField& field,
                                       const ro::sim::SurvivorsRules& rules,
                                       std::uint64_t presentation_ms,
                                       const SurvivorsTables& tables) {
  TimedEffectsOf(field, rules, timed_effects_);
  hud_effects_.clear();
  for (const SurvivorsTimedEffect& effect : timed_effects_) {
    hud_effects_.push_back(
        ro::ui::SurvivorsHudEffect{.icon = SurvivorsItemIcon(effect.item_id, tables),
                                   .remaining_ms = effect.remaining_ms,
                                   .full_ms = effect.full_ms});
  }
  hud_.effects = hud_effects_;

  pickup_notices_.ShownAt(presentation_ms, notices_shown_);
  hud_notices_.clear();
  for (const PickupNoticeShown& shown : notices_shown_) {
    const ro::sim::PickupRule* rule = ro::sim::FindPickup(rules, shown.kind);
    if (rule == nullptr) continue;
    const std::optional<SurvivorsPickupFigure> figure = PickupFigureOf(*rule);
    if (!figure.has_value()) continue;
    hud_notices_.push_back(ro::ui::SurvivorsHudNotice{.name = CardName(shown.item_id, tables),
                                                      .figure = figure->figure,
                                                      .value = figure->value,
                                                      .duration_ms = rule->duration_ms,
                                                      .fraction = shown.fraction});
  }
  hud_.notices = hud_notices_;
}

void SurvivorsViews::ComposePauseEffects(const ro::sim::SurvivorsField& field,
                                         const ro::sim::SurvivorsRules& rules,
                                         const SurvivorsTables& tables) {
  PermanentEffectsOf(field, rules, permanent_effects_);
  pause_effects_.clear();
  for (const SurvivorsPermanentEffect& effect : permanent_effects_) {
    ro::ui::SurvivorsPauseEffectRow row{
        .level = effect.level, .figure = effect.figure, .value = effect.value};
    switch (effect.source) {
      case SurvivorsPermanentSource::kCard:
      case SurvivorsPermanentSource::kKafra:
        row.icon = SurvivorsItemIcon(effect.id, tables);
        row.name = CardName(effect.id, tables);
        break;
      case SurvivorsPermanentSource::kBlessing:
      case SurvivorsPermanentSource::kLife: {
        const ro::sim::BlessingRule* rule =
            effect.source == SurvivorsPermanentSource::kLife
                ? ro::sim::FindBlessing(Meta(), ro::sim::Blessing::kResurrection)
                : (effect.id < Meta().blessings.size() ? &Meta().blessings[effect.id] : nullptr);
        const ro::sim::SkillNode* node =
            rule != nullptr ? NodeNamed(rule->skill_name, tables) : nullptr;
        row.icon = node != nullptr ? SurvivorsSkillIcon(node->id, tables) : ImTextureID{0};
        row.name =
            node != nullptr ? SkillName(node->id, tables) : (rule != nullptr ? rule->key : "");
        break;
      }
    }
    pause_effects_.push_back(row);
  }
  pause_.effects = pause_effects_;
}

}  // namespace moonclient
