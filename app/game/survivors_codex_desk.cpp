#include "game/survivors_codex_desk.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_records.h"

#include "survivors/arena_night.h"
#include "survivors/survivors_book.h"
#include "survivors/survivors_codex_keys.h"
#include "survivors/survivors_gems.h"

namespace moonclient {
namespace {

using ro::ui::SurvivorsCodexEntry;
using ro::ui::SurvivorsCodexFigure;
using ro::ui::SurvivorsCodexFigureLabel;
using ro::ui::SurvivorsCodexSheet;
using ro::ui::SurvivorsCodexTab;
using ro::ui::SurvivorsCodexUnit;

// Les dixièmes d'une case.
constexpr std::int64_t kTenthsPerCell = 10;
// Un pourcentage en centièmes : le poids d'une plante sur la somme des poids.
constexpr std::int64_t kHundredthsOfWhole = 10000;

// UNE LIGNE DE LA FICHE D'UN SORT : son libellé, son unité, et le champ du
// niveau qu'elle lit.
struct SkillRow {
  SurvivorsCodexFigureLabel label;
  SurvivorsCodexUnit unit;
  std::int64_t (*read)(const ro::sim::SkillLevel&);
};
using Label = SurvivorsCodexFigureLabel;
using Unit = SurvivorsCodexUnit;
using Level = ro::sim::SkillLevel;
constexpr std::array<SkillRow, 14> kSkillRows{{
    {Label::kDamage, Unit::kPlain, [](const Level& level) -> std::int64_t { return level.damage; }},
    {Label::kCooldown, Unit::kMs,
     [](const Level& level) -> std::int64_t { return level.cooldown_ms; }},
    {Label::kAmount, Unit::kPlain, [](const Level& level) -> std::int64_t { return level.amount; }},
    {Label::kPierce, Unit::kPlain, [](const Level& level) -> std::int64_t { return level.pierce; }},
    {Label::kReach, Unit::kCellTenths,
     [](const Level& level) -> std::int64_t { return CellTenthsOf(level.reach_units); }},
    {Label::kArea, Unit::kCellTenths,
     [](const Level& level) -> std::int64_t { return CellTenthsOf(level.area_radius_units); }},
    {Label::kLength, Unit::kCellTenths,
     [](const Level& level) -> std::int64_t { return CellTenthsOf(level.length_units); }},
    {Label::kLife, Unit::kMs, [](const Level& level) -> std::int64_t { return level.life_ms; }},
    {Label::kZoneHits, Unit::kPlain,
     [](const Level& level) -> std::int64_t { return level.zone_hits; }},
    {Label::kChill, Unit::kMs, [](const Level& level) -> std::int64_t { return level.chill_ms; }},
    {Label::kFreeze, Unit::kMs, [](const Level& level) -> std::int64_t { return level.freeze_ms; }},
    {Label::kKnockback, Unit::kCellTenths,
     [](const Level& level) -> std::int64_t { return CellTenthsOf(level.knockback_units); }},
    {Label::kBounces, Unit::kPlain,
     [](const Level& level) -> std::int64_t { return level.bounces; }},
    {Label::kSplash, Unit::kPercent,
     [](const Level& level) -> std::int64_t { return level.splash_pct; }},
}};

[[nodiscard]] SurvivorsCodexFigure Single(Label label, Unit unit, std::int64_t value) {
  SurvivorsCodexFigure figure{.label = label, .unit = unit};
  figure.values[0] = value;
  return figure;
}

// Le lien vers un sort des règles.
[[nodiscard]] ro::ui::SurvivorsCodexLink SkillLink(std::uint16_t skill_id) {
  return ro::ui::SurvivorsCodexLink{.ro_id = skill_id};
}

// La clé de l'exploit qui verrouille ce sort, ou "".
[[nodiscard]] const char* UnlockFeatOf(const ro::sim::SurvivorsSkillRule& skill) {
  for (const SurvivorsUnlock& unlock : ShippedSurvivorsUnlocks()) {
    if (unlock.condition == UnlockCondition::kFeat && std::string_view(unlock.key) == skill.key) {
      return unlock.key;
    }
  }
  return "";
}

// Le niveau maximal d'un sort, borné à la fiche.
[[nodiscard]] std::uint8_t MostLevelOf(const ro::sim::SurvivorsRules& rules,
                                       std::uint16_t skill_id) {
  const ro::sim::SurvivorsSkillRule* skill = ro::sim::FindSkill(rules, skill_id);
  if (skill == nullptr) return 0;
  return static_cast<std::uint8_t>(
      std::min<std::size_t>(skill->levels.size(), ro::ui::kSurvivorsCodexMostLevels));
}

[[nodiscard]] ro::ui::SurvivorsCodexEvolution EvolutionOf(const ro::sim::SurvivorsRules& rules,
                                                          const ro::sim::EvolutionRule& rule) {
  return ro::ui::SurvivorsCodexEvolution{
      .card = ro::ui::SurvivorsCodexLink{.ro_id = rule.card_item_id},
      .result = SkillLink(rule.evolved_skill_id),
      .max_level = MostLevelOf(rules, rule.skill_id)};
}

// Ce qu'un objet au sol dit de lui : son effet selon sa sorte, sa durée, sa
// portée.
[[nodiscard]] std::vector<SurvivorsCodexFigure> PickupFiguresOf(const ro::sim::PickupRule& rule) {
  std::vector<SurvivorsCodexFigure> figures;
  if (rule.amount != 0) {
    switch (rule.kind) {
      case ro::sim::PickupKind::kMeat:
        figures.push_back(Single(Label::kHeal, Unit::kPlain, rule.amount));
        break;
      case ro::sim::PickupKind::kGold:
        figures.push_back(Single(Label::kZeny, Unit::kPlain, rule.amount));
        break;
      case ro::sim::PickupKind::kManual:
        figures.push_back(Single(Label::kExpBoost, Unit::kPercent, rule.amount));
        break;
      case ro::sim::PickupKind::kHaste:
        figures.push_back(Single(Label::kWalkBoost, Unit::kPercent, rule.amount));
        break;
      case ro::sim::PickupKind::kChest:
      case ro::sim::PickupKind::kHolyWater:
      case ro::sim::PickupKind::kMagnetAll:
      case ro::sim::PickupKind::kCard:
        break;
    }
  }
  if (rule.duration_ms != 0)
    figures.push_back(Single(Label::kDuration, Unit::kMs, rule.duration_ms));
  if (rule.reach_units != 0) {
    figures.push_back(Single(Label::kReach, Unit::kCellTenths, CellTenthsOf(rule.reach_units)));
  }
  return figures;
}

// LE COFFRE : le tirage teste le seuil des cinq gains avant celui des trois,
// si bien que la plage des trois est la part du second seuil au-delà du
// premier. Chaque ligne dit le compte de gains que la règle donne.
[[nodiscard]] std::vector<SurvivorsCodexFigure> ChestFiguresOf(const ro::sim::ChestRule& chest) {
  const std::uint32_t triple_band = chest.triple_per_10000 > chest.quintuple_per_10000
                                        ? chest.triple_per_10000 - chest.quintuple_per_10000
                                        : 0;
  SurvivorsCodexFigure triple = Single(Label::kGainsOdds, Unit::kPercentHundredths, triple_band);
  triple.gain_count = chest.triple;
  SurvivorsCodexFigure quintuple =
      Single(Label::kGainsOdds, Unit::kPercentHundredths, chest.quintuple_per_10000);
  quintuple.gain_count = chest.quintuple;
  return {triple, quintuple, Single(Label::kEvolutionsFrom, Unit::kMs, chest.evolution_from_ms)};
}

// CE QUI EST VU, POUR LES LIENS D'UNE FICHE : la même règle que les entrées,
// les cartes en plus, vues quand l'Album les a trouvées.
struct LinkSight {
  const ro::sim::SurvivorsRules& rules;
  const SurvivorsCodexSeen& seen;
  std::span<const std::uint16_t> found_cards;

  [[nodiscard]] bool Foe(std::uint16_t foe_id) const {
    return seen.Has(CodexCategory::kFoes, CodexFoeKey(foe_id));
  }
  [[nodiscard]] bool Skill(std::uint16_t skill_id) const {
    const ro::sim::SurvivorsSkillRule* skill = ro::sim::FindSkill(rules, skill_id);
    return skill != nullptr && seen.Has(CodexCategory::kSkills, CodexSkillKey(*skill));
  }
  [[nodiscard]] bool Card(std::uint16_t card_item_id) const {
    return std::ranges::find(found_cards, card_item_id) != found_cards.end();
  }
};

void MarkEvolution(ro::ui::SurvivorsCodexEvolution& evolution, const LinkSight& sight) {
  evolution.card.seen = sight.Card(evolution.card.ro_id);
  evolution.result.seen = sight.Skill(evolution.result.ro_id);
}

// Chaque lien d'une entrée reçoit `seen`.
void MarkEntryLinks(SurvivorsCodexEntry& entry, const LinkSight& sight) {
  MarkEvolution(entry.evolution, sight);
  MarkEvolution(entry.evolved_from, sight);
  entry.card.card.seen = sight.Card(entry.card.card.ro_id);
  entry.escort.seen = sight.Foe(entry.escort.ro_id);
  entry.event_foe.seen = sight.Foe(entry.event_foe.ro_id);
  entry.drop.seen =
      entry.drop_key[0] != '\0' && sight.seen.Has(CodexCategory::kItems, entry.drop_key);
}

// LES LIENS VERS D'AUTRES ENTRÉES, marqués vus ou non : ceux des entrées, les
// porteurs des sources d'arrivée et les espèces des coups de boss.
void MarkLinks(const LinkSight& sight, SurvivorsCodexRowsHeld& held) {
  for (std::vector<SurvivorsCodexEntry>& tab : held.entries) {
    for (SurvivorsCodexEntry& entry : tab) MarkEntryLinks(entry, sight);
  }
  for (std::vector<ro::ui::SurvivorsCodexSource>& sources : held.sources) {
    for (ro::ui::SurvivorsCodexSource& source : sources) {
      source.bringer.seen = sight.Foe(source.bringer.ro_id);
    }
  }
  for (std::vector<ro::ui::SurvivorsCodexMove>& moves : held.moves) {
    for (ro::ui::SurvivorsCodexMove& move : moves) move.foe.seen = sight.Foe(move.foe.ro_id);
  }
}

[[nodiscard]] std::vector<SurvivorsCodexEntry>& TabOf(SurvivorsCodexRowsHeld& held,
                                                      SurvivorsCodexTab tab) {
  return held.entries[static_cast<std::size_t>(tab)];
}

// Q2 : ce que le meilleur temps survécu a forcément montré.
void DeduceFromBestTime(const ro::sim::SurvivorsRules& rules, std::uint32_t best_ms,
                        SurvivorsCodexSeen& seen) {
  for (const ro::sim::WaveRule& wave : rules.waves) {
    const bool brings = wave.floor != 0 || wave.batch != 0;
    if (wave.from_ms >= best_ms || !brings) continue;
    for (std::size_t index = 0; index < wave.foe_ids.size(); ++index) {
      if (index < wave.weights.size() && wave.weights[index] != 0) {
        seen.Add(CodexCategory::kFoes, CodexFoeKey(wave.foe_ids[index]));
      }
    }
  }
  for (const ro::sim::BossRule& boss : rules.bosses) {
    if (boss.at_ms < best_ms) seen.Add(CodexCategory::kFoes, CodexFoeKey(boss.foe_id));
  }
  for (const ro::sim::EventRule& event : rules.events) {
    if (event.at_ms < best_ms) seen.Add(CodexCategory::kEvents, CodexEventKey(event));
  }
  if (rules.elites.interval_ms != 0 && rules.elites.first_ms < best_ms) {
    seen.Add(CodexCategory::kEvents, kCodexElitesKey);
  }
  if (kArenaNightFallsMs < best_ms) seen.Add(CodexCategory::kEvents, kCodexNightKey);
}

// Q2 : le monstre de chaque carte trouvée, et le premier sort de chaque record.
void DeduceFromAlbumAndRecords(const ro::sim::SurvivorsRules& rules, const SurvivorsSave& save,
                               SurvivorsCodexSeen& seen) {
  for (const SurvivorsAlbumCard& found : save.album.cards) {
    if (found.found == 0) continue;
    for (const ro::sim::CardRule& card : rules.cards) {
      if (found.key != card.key) continue;
      for (const ro::sim::FoeRule& foe : rules.foes) {
        if (foe.card_item_id == card.card_item_id) {
          seen.Add(CodexCategory::kFoes, CodexFoeKey(foe.foe_id));
        }
      }
    }
  }
  for (const SurvivorsBoard& board : save.records) {
    for (const ro::sim::SurvivorsRecord& record : board.runs) {
      for (const ro::sim::SurvivorsSkillRule& skill : rules.skills) {
        if (record.first_skill_key == skill.key) {
          seen.Add(CodexCategory::kSkills, CodexSkillKey(skill));
        }
      }
    }
  }
}

}  // namespace

void SurvivorsCodexRowsHeld::Clear() {
  for (std::vector<SurvivorsCodexEntry>& tab : entries) tab.clear();
  figures.clear();
  sources.clear();
  affinities.clear();
  moves.clear();
  keys.clear();
}

std::span<const SurvivorsCodexFigure> SurvivorsCodexRowsHeld::Hold(
    std::vector<SurvivorsCodexFigure> rows) {
  return figures.emplace_back(std::move(rows));
}

std::span<const ro::ui::SurvivorsCodexSource> SurvivorsCodexRowsHeld::Hold(
    std::vector<ro::ui::SurvivorsCodexSource> rows) {
  return sources.emplace_back(std::move(rows));
}

std::span<const ro::ui::SurvivorsCodexAffinity> SurvivorsCodexRowsHeld::Hold(
    std::vector<ro::ui::SurvivorsCodexAffinity> rows) {
  return affinities.emplace_back(std::move(rows));
}

std::span<const ro::ui::SurvivorsCodexMove> SurvivorsCodexRowsHeld::Hold(
    std::vector<ro::ui::SurvivorsCodexMove> rows) {
  return moves.emplace_back(std::move(rows));
}

const char* SurvivorsCodexRowsHeld::HoldKey(std::string key) {
  return keys.emplace_back(std::move(key)).c_str();
}

std::int64_t CellTenthsOf(std::int64_t units) noexcept {
  const std::int64_t half = ro::sim::kSubunitsPerCell / 2;
  const std::int64_t scaled = units * kTenthsPerCell;
  return (scaled >= 0 ? scaled + half : scaled - half) / ro::sim::kSubunitsPerCell;
}

std::vector<SurvivorsCodexFigure> SkillFiguresOf(std::span<const ro::sim::SkillLevel> levels) {
  const std::size_t count = std::min(levels.size(), ro::ui::kSurvivorsCodexMostLevels);
  std::vector<SurvivorsCodexFigure> figures;
  for (const SkillRow& row : kSkillRows) {
    SurvivorsCodexFigure figure{
        .label = row.label, .unit = row.unit, .count = static_cast<std::uint8_t>(count)};
    bool shown = false;
    for (std::size_t level = 0; level < count; ++level) {
      figure.values[level] = row.read(levels[level]);
      shown = shown || figure.values[level] != 0;
    }
    if (shown) figures.push_back(figure);
  }
  return figures;
}

void CodexSkillsOf(const ro::sim::SurvivorsRules& rules, const SurvivorsCodexSeen& seen,
                   SurvivorsCodexRowsHeld& held) {
  std::vector<SurvivorsCodexEntry>& tab = TabOf(held, SurvivorsCodexTab::kSkills);
  for (const ro::sim::SurvivorsSkillRule& skill : rules.skills) {
    const std::string key = CodexSkillKey(skill);
    SurvivorsCodexEntry entry{.sheet = SurvivorsCodexSheet::kSkill,
                              .seen = seen.Has(CodexCategory::kSkills, key),
                              .key = held.HoldKey(key),
                              .ro_id = skill.skill_id,
                              .description = skill.description,
                              .element = static_cast<std::uint8_t>(skill.element),
                              .figures = held.Hold(SkillFiguresOf(skill.levels)),
                              .unlock_feat = UnlockFeatOf(skill)};
    if (const ro::sim::EvolutionRule* evolution = ro::sim::FindEvolutionOf(rules, skill.skill_id)) {
      entry.has_evolution = true;
      entry.evolution = EvolutionOf(rules, *evolution);
    }
    if (const ro::sim::EvolutionRule* origin = ro::sim::FindEvolutionTo(rules, skill.skill_id)) {
      entry.evolved = true;
      entry.evolved_from = EvolutionOf(rules, *origin);
      entry.evolved_from.result = SkillLink(origin->skill_id);
    }
    tab.push_back(entry);
  }
}

void CodexItemsOf(const ro::sim::SurvivorsRules& rules, const SurvivorsCodexSeen& seen,
                  SurvivorsCodexRowsHeld& held) {
  std::vector<SurvivorsCodexEntry>& tab = TabOf(held, SurvivorsCodexTab::kItems);
  for (const ro::sim::PickupRule& pickup : rules.pickups) {
    const char* key = CodexItemKey(pickup.kind);
    const bool chest = pickup.kind == ro::sim::PickupKind::kChest;
    tab.push_back(SurvivorsCodexEntry{
        .sheet = chest ? SurvivorsCodexSheet::kChest : SurvivorsCodexSheet::kItem,
        .seen = seen.Has(CodexCategory::kItems, key),
        .key = key,
        .ro_id = pickup.item_id,
        .figures = held.Hold(chest ? ChestFiguresOf(rules.chests) : PickupFiguresOf(pickup))});
  }
  for (const GemColour& colour : kGemColours) {
    const char* key = CodexGemKey(colour.item_id);
    tab.push_back(SurvivorsCodexEntry{
        .sheet = SurvivorsCodexSheet::kGem,
        .seen = seen.Has(CodexCategory::kItems, key),
        .key = key,
        .ro_id = colour.item_id,
        .figures = held.Hold({Single(Label::kGemExp, Unit::kPlain, colour.from_exp)})});
  }
  const char* card_key = CodexItemKey(ro::sim::PickupKind::kCard);
  tab.push_back(SurvivorsCodexEntry{.sheet = SurvivorsCodexSheet::kGroundCard,
                                    .seen = seen.Has(CodexCategory::kItems, card_key),
                                    .key = card_key});
}

void CodexPlantsOf(const ro::sim::SurvivorsRules& rules, const SurvivorsCodexSeen& seen,
                   SurvivorsCodexRowsHeld& held) {
  std::vector<SurvivorsCodexEntry>& tab = TabOf(held, SurvivorsCodexTab::kPlants);
  std::int64_t weights = 0;
  for (const ro::sim::PlantRule& plant : rules.plants) weights += plant.weight;
  for (const ro::sim::PlantRule& plant : rules.plants) {
    const std::string key = CodexPlantKey(plant.foe_id);
    const ro::sim::PickupRule* drop = ro::sim::FindPickup(rules, plant.pickup);
    const std::int64_t share =
        weights > 0 ? std::int64_t{plant.weight} * kHundredthsOfWhole / weights : 0;
    tab.push_back(SurvivorsCodexEntry{
        .sheet = SurvivorsCodexSheet::kPlant,
        .seen = seen.Has(CodexCategory::kPlants, key),
        .key = held.HoldKey(key),
        .ro_id = plant.foe_id,
        .figures = held.Hold({Single(Label::kDrawWeight, Unit::kPercentHundredths, share)}),
        .drop =
            ro::ui::SurvivorsCodexLink{.ro_id = drop != nullptr ? drop->item_id : std::uint16_t{0}},
        .drop_key = CodexItemKey(plant.pickup)});
  }
}

SurvivorsCodexSeen CodexSeenOf(const ro::sim::SurvivorsRules& rules, const SurvivorsSave& save,
                               const SurvivorsCodexSeen* live) {
  SurvivorsCodexSeen seen = save.codex;
  if (live != nullptr) Merge(seen, *live);
  DeduceFromBestTime(rules, save.album.counters.best_survived_ms, seen);
  DeduceFromAlbumAndRecords(rules, save, seen);
  return seen;
}

std::vector<std::uint16_t> FoundCardItemsOf(const ro::sim::SurvivorsRules& rules,
                                            const SurvivorsSave& save) {
  std::vector<std::uint16_t> found_cards;
  for (const SurvivorsAlbumCard& found : save.album.cards) {
    if (found.found == 0) continue;
    for (const ro::sim::CardRule& card : rules.cards) {
      if (found.key == card.key) found_cards.push_back(card.card_item_id);
    }
  }
  return found_cards;
}

ro::ui::SurvivorsCodexView ComposeSurvivorsCodex(const ro::sim::SurvivorsRules& rules,
                                                 const SurvivorsCodexSeen& seen,
                                                 SurvivorsCodexRowsHeld& held,
                                                 std::span<const std::uint16_t> found_cards) {
  held.Clear();
  CodexSkillsOf(rules, seen, held);
  CodexItemsOf(rules, seen, held);
  CodexPlantsOf(rules, seen, held);
  CodexFoesOf(rules, seen, held);
  CodexBossesOf(rules, seen, held);
  MarkLinks(LinkSight{.rules = rules, .seen = seen, .found_cards = found_cards}, held);
  ro::ui::SurvivorsCodexView view;
  for (std::size_t tab = 0; tab < ro::ui::kSurvivorsCodexTabCount; ++tab) {
    view.entries[tab] = held.entries[tab];
  }
  return view;
}

bool FollowCodexOverPause(SurvivorsScreen screen, bool open_asked, SurvivorsGestures& gestures,
                          ro::ui::SurvivorsCodexState& codex) noexcept {
  if (screen != SurvivorsScreen::kPaused) {
    codex.open = false;
    return false;
  }
  if (codex.open) {
    gestures.wheel_notches = 0.0F;
    if (gestures.escape) {
      codex.open = false;
      gestures.escape = false;
      return true;
    }
    return false;
  }
  if (open_asked) codex.open = true;
  return false;
}

}  // namespace moonclient
