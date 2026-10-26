// LES FICHES DES MONSTRES, DES BOSS ET DES ÉVÉNEMENTS DU CODEX, tirées des
// règles : l'arrivée d'un monstre, ses affinités, la chronologie.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "ro/sim/survivors_content.h"
#include "ro/sim/survivors_rules.h"
#include "ro/ui/database_window.h"
#include "ro/ui/survivors_codex_window.h"

#include "game/survivors_codex_desk.h"
#include "survivors/arena_night.h"
#include "survivors/survivors_codex_keys.h"

namespace moonclient {
namespace {

using ro::ui::SurvivorsCodexEntry;
using ro::ui::SurvivorsCodexFigure;
using ro::ui::SurvivorsCodexSheet;
using ro::ui::SurvivorsCodexSource;
using ro::ui::SurvivorsCodexSourceKind;
using Label = ro::ui::SurvivorsCodexFigureLabel;
using Unit = ro::ui::SurvivorsCodexUnit;

// LES MIROIRS DE `ui` : les rangs des éléments et des tailles sont ceux des
// tables de la base de données, et les énumérations se suivent valeur à valeur.
static_assert(ro::sim::kSurvivorsElementCount == ro::ui::kDatabaseElements.size());
static_assert(ro::sim::kFoeSizeCount == ro::ui::kDatabaseSizes.size());
static_assert(static_cast<int>(ro::sim::FoeMove::kBlinker) ==
              static_cast<int>(ro::ui::SurvivorsCodexGait::kBlinker));
static_assert(static_cast<int>(ro::sim::BossMoveKind::kFlee) ==
              static_cast<int>(ro::ui::SurvivorsCodexMoveKind::kFlee));
static_assert(static_cast<int>(ro::sim::EventKind::kGroup) ==
              static_cast<int>(ro::ui::SurvivorsCodexEventKind::kGroup));
static_assert(ro::sim::kFoeKnockbackImmune == ro::ui::kCodexTraitKnockbackImmune);
static_assert(ro::sim::kFoeFreezeImmune == ro::ui::kCodexTraitFreezeImmune);
static_assert(ro::sim::kFoePassesWalls == ro::ui::kCodexTraitPassesWalls);
static_assert(ro::sim::kFoeHides == ro::ui::kCodexTraitHides);
static_assert(ro::sim::kFoeOverHorde == ro::ui::kCodexTraitOverHorde);

// L'efficacité ordinaire d'un élément, en %.
constexpr std::int32_t kPlainElementPct = 100;
constexpr std::uint32_t kWholePct = 100;

[[nodiscard]] SurvivorsCodexFigure Single(Label label, Unit unit, std::int64_t value) {
  SurvivorsCodexFigure figure{.label = label, .unit = unit};
  figure.values[0] = value;
  return figure;
}

[[nodiscard]] ro::ui::SurvivorsCodexLink FoeLink(std::uint16_t foe_id) {
  return ro::ui::SurvivorsCodexLink{.ro_id = foe_id};
}

[[nodiscard]] bool WaveBrings(const ro::sim::WaveRule& wave, std::uint16_t foe_id) {
  if (wave.floor == 0 && wave.batch == 0) return false;
  for (std::size_t index = 0; index < wave.foe_ids.size(); ++index) {
    if (wave.foe_ids[index] == foe_id && index < wave.weights.size() && wave.weights[index] != 0) {
      return true;
    }
  }
  return false;
}

// Une source de plus, ou une plus tôt pour la même source.
void NoteSource(std::vector<SurvivorsCodexSource>& sources, SurvivorsCodexSourceKind kind,
                std::uint32_t at_ms, std::uint16_t bringer_id) {
  for (SurvivorsCodexSource& known : sources) {
    if (known.kind == kind && known.bringer.ro_id == bringer_id) {
      known.at_ms = std::min(known.at_ms, at_ms);
      return;
    }
  }
  sources.push_back(
      SurvivorsCodexSource{.kind = kind, .at_ms = at_ms, .bringer = FoeLink(bringer_id)});
}

[[nodiscard]] std::uint32_t ScaledWalkMs(std::uint32_t walk_ms, std::int32_t scale_pct) {
  const auto scale = static_cast<std::uint64_t>(std::max(scale_pct, 0));
  return static_cast<std::uint32_t>(std::min<std::uint64_t>(
      std::uint64_t{walk_ms} * scale / kWholePct, std::numeric_limits<std::uint32_t>::max()));
}

// Les nombres d'un monstre ou d'un boss : PV et leur croissance, contact,
// marche, XP.
[[nodiscard]] std::vector<SurvivorsCodexFigure> FoeFiguresOf(const ro::sim::SurvivorsRules& rules,
                                                             const ro::sim::FoeRule& foe,
                                                             const ro::sim::BossRule* boss) {
  const ro::sim::SurvivorsTuning& tuning = rules.tuning;
  std::vector<SurvivorsCodexFigure> figures{Single(Label::kHp, Unit::kPlain, foe.hp)};
  const std::int32_t growth_pct =
      boss != nullptr ? tuning.boss_hp_growth_pct_per_minute : tuning.foe_hp_growth_pct_per_minute;
  if (growth_pct != 0)
    figures.push_back(Single(Label::kHpGrowthPerMinute, Unit::kPercent, growth_pct));
  if (foe.touch_damage != 0) {
    figures.push_back(Single(Label::kTouchDamage, Unit::kPlain, foe.touch_damage));
    figures.push_back(Single(Label::kTouchInterval, Unit::kMs, foe.touch_interval_ms));
  }
  std::uint32_t walk_ms = ScaledWalkMs(foe.ro_walk_ms_per_cell, tuning.foe_walk_scale_pct);
  if (boss != nullptr) {
    walk_ms = boss->walk_ms_per_cell != 0
                  ? boss->walk_ms_per_cell
                  : ScaledWalkMs(foe.ro_walk_ms_per_cell, tuning.boss_walk_scale_pct);
  }
  if (walk_ms != 0) figures.push_back(Single(Label::kWalkPerCell, Unit::kMs, walk_ms));
  figures.push_back(Single(Label::kExp, Unit::kPlain, foe.exp));
  return figures;
}

[[nodiscard]] ro::ui::SurvivorsCodexBehaviour BehaviourOf(const ro::sim::FoeBehaviour& behaviour) {
  return ro::ui::SurvivorsCodexBehaviour{
      .gait = static_cast<ro::ui::SurvivorsCodexGait>(behaviour.move),
      .traits = behaviour.traits,
      .pack = behaviour.pack};
}

// L'entrée commune d'un monstre et d'un boss : élément, taille, nombres,
// affinités, comportement, carte.
[[nodiscard]] SurvivorsCodexEntry FoeEntryOf(const ro::sim::SurvivorsRules& rules,
                                             const SurvivorsCodexSeen& seen,
                                             const ro::sim::FoeRule& foe,
                                             const ro::sim::BossRule* boss,
                                             SurvivorsCodexRowsHeld& held) {
  const std::string key = CodexFoeKey(foe.foe_id);
  FoeAffinities affinities = AffinitiesOf(rules, foe.element);
  SurvivorsCodexEntry entry{
      .sheet = boss != nullptr ? SurvivorsCodexSheet::kBoss : SurvivorsCodexSheet::kFoe,
      .seen = seen.Has(CodexCategory::kFoes, key),
      .key = held.HoldKey(key),
      .ro_id = foe.foe_id,
      .element = static_cast<std::uint8_t>(foe.element),
      .size = static_cast<std::uint8_t>(foe.size),
      .figures = held.Hold(FoeFiguresOf(rules, foe, boss)),
      .weaknesses = held.Hold(std::move(affinities.weaknesses)),
      .resistances = held.Hold(std::move(affinities.resistances)),
      .behaviour = BehaviourOf(foe.behaviour)};
  if (foe.card_item_id != 0) {
    entry.drops_card = true;
    entry.card = ro::ui::SurvivorsCodexCardDrop{
        .card = ro::ui::SurvivorsCodexLink{.ro_id = foe.card_item_id},
        .odds = rules.card_drops.common_odds,
        .guaranteed = foe.rank != ro::sim::FoeRank::kCommon};
  }
  return entry;
}

[[nodiscard]] std::vector<ro::ui::SurvivorsCodexMove> MovesOf(const ro::sim::BossRule& boss) {
  std::vector<ro::ui::SurvivorsCodexMove> moves;
  for (const ro::sim::BossMoveRule& move : boss.moves) {
    moves.push_back(ro::ui::SurvivorsCodexMove{
        .kind = static_cast<ro::ui::SurvivorsCodexMoveKind>(move.kind),
        .from_hp_pct = move.from_hp_pct,
        .to_hp_pct = move.to_hp_pct,
        .period_ms = move.period_ms,
        .telegraph_ms = move.telegraph_ms,
        .effect_ms = move.effect_ms,
        .count = move.count,
        .foe = FoeLink(move.foe_id),
        .reach_tenths = static_cast<std::int32_t>(CellTenthsOf(move.reach_units)),
        .radius_tenths = static_cast<std::int32_t>(CellTenthsOf(move.radius_units)),
        .damage = move.damage,
        .boost_pct = move.boost_pct});
  }
  return moves;
}

// LES ÉLITES : les règles portent des FACTEURS en % (1000 : dix fois les PV),
// et la fiche dit ce qui s'ajoute à un monstre ordinaire, le facteur moins
// cent. Le contact suit la borne que la partie lui applique.
[[nodiscard]] std::vector<SurvivorsCodexFigure> EliteFiguresOf(const ro::sim::EliteRule& elites) {
  constexpr std::int64_t kWhole = kWholePct;
  const std::int64_t touch = std::clamp(elites.touch_pct, 0, ro::sim::kMostStatBonus);
  return {Single(Label::kHpBoost, Unit::kPercent, std::int64_t{elites.hp_pct} - kWhole),
          Single(Label::kTouchBoost, Unit::kPercent, touch - kWhole),
          Single(Label::kExpBoost, Unit::kPercent, std::int64_t{elites.exp_pct} - kWhole)};
}

// Une ligne de la chronologie, et son heure.
struct TimedEntry {
  std::uint32_t at_ms = 0;
  SurvivorsCodexEntry entry;
};

}  // namespace

FoeArrival FoeArrivalOf(const ro::sim::SurvivorsRules& rules, std::uint16_t foe_id) {
  FoeArrival arrival;
  constexpr std::uint16_t kNoBringer = 0;
  for (const ro::sim::WaveRule& wave : rules.waves) {
    if (WaveBrings(wave, foe_id)) {
      NoteSource(arrival.sources, SurvivorsCodexSourceKind::kWave, wave.from_ms, kNoBringer);
    }
  }
  for (const ro::sim::EventRule& event : rules.events) {
    if (event.foe_id == foe_id && event.count != 0) {
      NoteSource(arrival.sources, SurvivorsCodexSourceKind::kEvent, event.at_ms, kNoBringer);
    }
  }
  for (const ro::sim::BossRule& boss : rules.bosses) {
    if (boss.escort_foe_id == foe_id && boss.escort_count != 0) {
      NoteSource(arrival.sources, SurvivorsCodexSourceKind::kEscort, boss.at_ms, boss.foe_id);
    }
    for (const ro::sim::BossMoveRule& move : boss.moves) {
      if (move.kind == ro::sim::BossMoveKind::kSummon && move.foe_id == foe_id) {
        NoteSource(arrival.sources, SurvivorsCodexSourceKind::kSummon, boss.at_ms + move.first_ms,
                   boss.foe_id);
      }
    }
  }
  std::ranges::stable_sort(arrival.sources, {}, &SurvivorsCodexSource::at_ms);
  if (!arrival.sources.empty()) arrival.arrives_ms = arrival.sources.front().at_ms;
  return arrival;
}

FoeAffinities AffinitiesOf(const ro::sim::SurvivorsRules& rules,
                           ro::sim::SurvivorsElement defence) {
  FoeAffinities affinities;
  for (std::size_t attack = 0; attack < ro::sim::kSurvivorsElementCount; ++attack) {
    const std::int32_t pct =
        ro::sim::ElementPct(rules, static_cast<ro::sim::SurvivorsElement>(attack), defence);
    const ro::ui::SurvivorsCodexAffinity affinity{.element = static_cast<std::uint8_t>(attack),
                                                  .pct = pct};
    if (pct > kPlainElementPct) affinities.weaknesses.push_back(affinity);
    if (pct < kPlainElementPct) affinities.resistances.push_back(affinity);
  }
  const auto gap = [](const ro::ui::SurvivorsCodexAffinity& affinity) {
    return std::abs(affinity.pct - kPlainElementPct);
  };
  std::ranges::stable_sort(affinities.weaknesses, std::ranges::greater{}, gap);
  std::ranges::stable_sort(affinities.resistances, std::ranges::greater{}, gap);
  return affinities;
}

void CodexFoesOf(const ro::sim::SurvivorsRules& rules, const SurvivorsCodexSeen& seen,
                 SurvivorsCodexRowsHeld& held) {
  std::vector<TimedEntry> rows;
  for (const ro::sim::FoeRule& foe : rules.foes) {
    if (foe.rank != ro::sim::FoeRank::kCommon) continue;
    FoeArrival arrival = FoeArrivalOf(rules, foe.foe_id);
    SurvivorsCodexEntry entry = FoeEntryOf(rules, seen, foe, nullptr, held);
    entry.arrives = arrival.arrives_ms.has_value();
    entry.arrives_ms = arrival.arrives_ms.value_or(0);
    entry.sources = held.Hold(std::move(arrival.sources));
    rows.push_back(
        TimedEntry{.at_ms = arrival.arrives_ms.value_or(std::numeric_limits<std::uint32_t>::max()),
                   .entry = entry});
  }
  std::ranges::stable_sort(rows, {}, &TimedEntry::at_ms);
  auto& tab = held.entries[static_cast<std::size_t>(ro::ui::SurvivorsCodexTab::kFoes)];
  for (const TimedEntry& row : rows) tab.push_back(row.entry);
}

void CodexBossesOf(const ro::sim::SurvivorsRules& rules, const SurvivorsCodexSeen& seen,
                   SurvivorsCodexRowsHeld& held) {
  std::vector<TimedEntry> rows;
  for (const ro::sim::BossRule& boss : rules.bosses) {
    const ro::sim::FoeRule* foe = ro::sim::FindFoe(rules, boss.foe_id);
    if (foe == nullptr) continue;
    SurvivorsCodexEntry entry = FoeEntryOf(rules, seen, *foe, &boss, held);
    entry.arrives = true;
    entry.arrives_ms = boss.at_ms;
    entry.at_ms = boss.at_ms;
    entry.final = boss.final;
    entry.escort = FoeLink(boss.escort_foe_id);
    entry.escort_count = boss.escort_count;
    entry.moves = held.Hold(MovesOf(boss));
    rows.push_back(TimedEntry{.at_ms = boss.at_ms, .entry = entry});
  }
  for (const ro::sim::EventRule& event : rules.events) {
    const std::string key = CodexEventKey(event);
    SurvivorsCodexEntry entry{.sheet = SurvivorsCodexSheet::kEvent,
                              .seen = seen.Has(CodexCategory::kEvents, key),
                              .key = held.HoldKey(key)};
    entry.at_ms = event.at_ms;
    entry.event_kind = static_cast<ro::ui::SurvivorsCodexEventKind>(event.kind);
    entry.event_foe = FoeLink(event.foe_id);
    entry.event_count = event.count;
    rows.push_back(TimedEntry{.at_ms = event.at_ms, .entry = entry});
  }
  const ro::sim::EliteRule& elites = rules.elites;
  if (elites.interval_ms != 0) {
    SurvivorsCodexEntry entry{.sheet = SurvivorsCodexSheet::kEvent,
                              .seen = seen.Has(CodexCategory::kEvents, kCodexElitesKey),
                              .key = kCodexElitesKey.data(),
                              .figures = held.Hold(EliteFiguresOf(elites))};
    entry.at_ms = elites.first_ms;
    entry.event_kind = ro::ui::SurvivorsCodexEventKind::kElites;
    entry.interval_ms = elites.interval_ms;
    entry.until_ms = elites.last_ms;
    rows.push_back(TimedEntry{.at_ms = elites.first_ms, .entry = entry});
  }
  SurvivorsCodexEntry night{.sheet = SurvivorsCodexSheet::kEvent,
                            .seen = seen.Has(CodexCategory::kEvents, kCodexNightKey),
                            .key = kCodexNightKey.data()};
  night.at_ms = static_cast<std::uint32_t>(kArenaNightFallsMs);
  night.event_kind = ro::ui::SurvivorsCodexEventKind::kNight;
  night.until_ms = static_cast<std::uint32_t>(kArenaMidnightMs);
  rows.push_back(TimedEntry{.at_ms = night.at_ms, .entry = night});
  std::ranges::stable_sort(rows, {}, &TimedEntry::at_ms);
  auto& tab = held.entries[static_cast<std::size_t>(ro::ui::SurvivorsCodexTab::kBosses)];
  for (const TimedEntry& row : rows) tab.push_back(row.entry);
}

}  // namespace moonclient
