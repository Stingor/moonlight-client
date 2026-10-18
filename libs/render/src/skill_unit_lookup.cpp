#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <string_view>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_case_table.h"
#include "ro/render/effect_duration.h"
#include "ro/render/effect_frame.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/skill_unit_sound_table.h"
#include "ro/render/skill_unit_table.h"

namespace ro::render {
namespace {

// Une même recherche pour les deux tables de sons : elles ne diffèrent que par
// le moment où on les consulte.
const SkillUnitSoundEntry* Found(std::span<const SkillUnitSoundEntry> table,
                                 std::uint32_t unit_id) {
  return core::RowWithKey(table, unit_id, &SkillUnitSoundEntry::unit_id);
}

// LES SONS DE POSE CHOISIS PAR LE JOUEUR, à l'écart du client d'origine, qui
// n'en joue aucun pour ces unités. Ils ne passent qu'à défaut du relevé :
// l'unité de Demonstration crépite comme un Fire Wall.
constexpr std::array<SkillUnitSoundEntry, 1> kPlayerChosenPlacementSounds = {{
    {177, "effect\\EF_FireWall.wav", 1},
}};

}  // namespace

const SkillUnitSoundEntry* SkillUnitPlacementSound(std::uint32_t unit_id) {
  if (const SkillUnitSoundEntry* measured = Found(SkillUnitPlacementSoundTable(), unit_id)) {
    return measured;
  }
  return Found(kPlayerChosenPlacementSounds, unit_id);
}

const SkillUnitSoundEntry* SkillUnitSustainSound(std::uint32_t unit_id) {
  return Found(SkillUnitSustainSoundTable(), unit_id);
}

std::uint16_t SkillUnitEffect(std::uint32_t unit_id) {
  const SkillUnitEffectEntry* found =
      core::RowWithKey(SkillUnitEffectTable(), unit_id, &SkillUnitEffectEntry::unit_id);
  if (found == nullptr) {
    return 0;
  }
  return found->effect_id;
}

const SkillUnitVisualEntry* SkillUnitVisual(std::uint32_t unit_id) {
  return core::RowWithKey(SkillUnitVisualTable(), unit_id, &SkillUnitVisualEntry::unit_id);
}

std::uint16_t SkillUnitLevelVisual(std::uint32_t unit_id, std::uint8_t skill_level) {
  // Le NIVEAU DÉBORDE vers le dernier connu plutôt que vers rien : le serveur
  // Moonlight hausse des compétences au-delà de ce que le client relevé
  // connaît, et une unité muette serait pire qu'une image d'un rang trop bas.
  const std::span<const SkillUnitLevelVisualEntry> table = SkillUnitLevelVisualTable();
  std::uint16_t effect_id = 0;
  for (const SkillUnitLevelVisualEntry& entry : table) {
    if (entry.unit_id != unit_id) {
      continue;
    }
    if (entry.skill_level > skill_level && effect_id != 0) {
      break;
    }
    effect_id = entry.effect_id;
  }
  return effect_id;
}

std::uint32_t SustainedUnitVisualPeriodMs(std::uint32_t effect_id) {
  std::optional<std::int32_t> earliest_ticks;
  for (const EffectNodePattern& pattern : EffectNodePatternsFor(effect_id)) {
    const std::int32_t ticks = pattern.fade_out_tick > 0
                                   ? pattern.fade_out_tick
                                   : std::max(pattern.life_ticks, pattern.life_ticks_high);
    earliest_ticks = std::min(earliest_ticks.value_or(ticks), ticks);
  }
  if (!earliest_ticks.has_value() || *earliest_ticks <= 0) {
    return 0;
  }
  return static_cast<std::uint32_t>(*earliest_ticks) * static_cast<std::uint32_t>(kEffectTickMs);
}

bool SkillUnitVisualSownOnce(const SkillUnitVisualEntry& visual) {
  // La fabrique qui pose un sprite d'acteur, et le bâtit à la naissance seule.
  constexpr std::string_view kSpriteFactory = "Effect_Build_ActorSpriteEffectByType";
  const EffectCaseEntry* builder = EffectCaseFor(visual.effect_id);
  return visual.shape == SkillUnitVisualShape::kPermanent && builder != nullptr &&
         builder->client_function == kSpriteFactory;
}

std::uint32_t SkillUnitVisualPeriodMs(const SkillUnitVisualEntry& visual) {
  if (SkillUnitVisualLastsWithUnit(visual.unit_id)) {
    return SustainedUnitVisualPeriodMs(visual.effect_id);
  }
  return SkillUnitRespawnMs(visual.effect_id, visual.withhold_ticks);
}

std::string SkillUnitModelPath(std::uint32_t unit_id) {
  const SkillUnitEntry* found =
      core::RowWithKey(SkillUnitTable(), unit_id, &SkillUnitEntry::unit_id);
  if (found == nullptr) {
    return {};
  }
  return std::string(kModelFolder) + std::string(found->model);
}

std::span<const SkillUnitNamedEffectEntry> SkillUnitNamedEffects(std::uint32_t unit_id) {
  // Une PLAGE, et non une entrée : une fabrique en joue jusqu'à quatre par
  // moment, et leur ordre dans la table est celui de sa fonction.
  return core::RowsWithKey(SkillUnitNamedEffectTable(), static_cast<std::uint16_t>(unit_id),
                           &SkillUnitNamedEffectEntry::unit_id);
}

std::span<const SkillUnitNamedEffectEntry> SkillUnitNamedEffects(std::uint32_t unit_id,
                                                                 SkillUnitNamedMoment moment) {
  // La table range les moments d'une unité dans l'ordre où elle les joue, donc
  // ceux d'un même moment sont CONTIGUS et se prennent en une seconde borne.
  return core::RowsWithKey(SkillUnitNamedEffects(unit_id), moment,
                           &SkillUnitNamedEffectEntry::moment);
}

}  // namespace ro::render
