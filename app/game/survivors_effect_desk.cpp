#include "game/survivors_effect_desk.h"

#include <algorithm>
#include <utility>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_meta.h"

namespace moonclient {
namespace {

using Figure = ro::ui::SurvivorsEffectFigure;

// LE TERME D'UN EFFET À DURÉE dans le héros : le pas où il s'éteint. Un
// nouvel objet à durée ajoute ici son terme, sans quoi il ne paraît pas au
// HUD.
[[nodiscard]] std::optional<std::uint32_t> UntilStepOf(const ro::sim::SurvivorsHero& hero,
                                                       ro::sim::PickupKind kind) noexcept {
  switch (kind) {
    case ro::sim::PickupKind::kManual:
      return hero.exp_boost_until_step;
    case ro::sim::PickupKind::kHaste:
      return hero.haste_until_step;
    case ro::sim::PickupKind::kChest:
    case ro::sim::PickupKind::kMeat:
    case ro::sim::PickupKind::kGold:
    case ro::sim::PickupKind::kHolyWater:
    case ro::sim::PickupKind::kMagnetAll:
    case ro::sim::PickupKind::kCard:
      return std::nullopt;
  }
  return std::nullopt;
}

// Des sous-unités en cases entières, arrondies au plus proche.
[[nodiscard]] std::int32_t CellsOf(std::int32_t units) noexcept {
  return (units + ro::sim::kSubunitsPerCell / 2) / ro::sim::kSubunitsPerCell;
}

[[nodiscard]] const ro::sim::SurvivorsMeta& Meta() { return ro::sim::ShippedSurvivorsMeta(); }

// La valeur d'une carte à son niveau ; rien hors de sa table.
[[nodiscard]] std::optional<std::int32_t> CardValueAt(const ro::sim::CardRule& rule,
                                                      std::uint8_t level) noexcept {
  if (level == 0 || level > rule.per_level.size()) return std::nullopt;
  return rule.per_level[level - 1U];
}

// LA STATISTIQUE QU'UNE BÉNÉDICTION MAJORE au départ : la sienne, ou les PV
// maximum que Kyrie Eleison verse en PV. Rien pour celle qui donne des vies.
[[nodiscard]] std::optional<ro::sim::SurvivorsStat> BlessedStat(const ro::sim::BlessingRule& rule) {
  switch (rule.effect) {
    case ro::sim::BlessingEffect::kStat:
      return rule.stat;
    case ro::sim::BlessingEffect::kMaxHpPct:
      return ro::sim::SurvivorsStat::kMaxHp;
    case ro::sim::BlessingEffect::kLife:
      return std::nullopt;
  }
  return std::nullopt;
}

[[nodiscard]] Figure FigureOfCharge(ro::sim::KafraCharge charge) noexcept {
  switch (charge) {
    case ro::sim::KafraCharge::kReroll:
      return Figure::kRerollsLeft;
    case ro::sim::KafraCharge::kSkip:
      return Figure::kSkipsLeft;
    case ro::sim::KafraCharge::kBanish:
      return Figure::kBanishesLeft;
  }
  return Figure::kRerollsLeft;
}

// Les charges d'un service : données au départ, puis restantes.
[[nodiscard]] std::pair<std::uint8_t, std::uint8_t> ChargesOf(
    const ro::sim::SurvivorsField& field, ro::sim::KafraCharge charge) noexcept {
  switch (charge) {
    case ro::sim::KafraCharge::kReroll:
      return {field.start.loadout.rerolls, field.rerolls_left};
    case ro::sim::KafraCharge::kSkip:
      return {field.start.loadout.skips, field.skips_left};
    case ro::sim::KafraCharge::kBanish:
      return {field.start.loadout.banishes, field.banishes_left};
  }
  return {std::uint8_t{0}, std::uint8_t{0}};
}

}  // namespace

void TimedEffectsOf(const ro::sim::SurvivorsField& field, const ro::sim::SurvivorsRules& rules,
                    std::vector<SurvivorsTimedEffect>& out) {
  out.clear();
  for (const ro::sim::PickupRule& rule : rules.pickups) {
    if (rule.duration_ms == 0) continue;
    const std::optional<std::uint32_t> until = UntilStepOf(field.hero, rule.kind);
    if (!until.has_value() || field.step >= *until) continue;
    const std::uint64_t left_ms = std::uint64_t{*until - field.step} * ro::sim::kSurvivorsStepMs;
    // Le terme compte le pas du ramassage : le reste ne passe jamais la
    // durée pleine.
    out.push_back(SurvivorsTimedEffect{.kind = rule.kind,
                                       .item_id = rule.item_id,
                                       .remaining_ms = static_cast<std::uint32_t>(
                                           std::min<std::uint64_t>(left_ms, rule.duration_ms)),
                                       .full_ms = rule.duration_ms});
  }
}

ro::ui::SurvivorsEffectFigure FigureOfStat(ro::sim::SurvivorsStat stat) noexcept {
  switch (stat) {
    case ro::sim::SurvivorsStat::kDamagePct:
      return Figure::kDamagePct;
    case ro::sim::SurvivorsStat::kMaxHp:
      return Figure::kMaxHp;
    case ro::sim::SurvivorsStat::kArmor:
      return Figure::kArmor;
    case ro::sim::SurvivorsStat::kRegenPerMinute:
      return Figure::kRegenPerMinute;
    case ro::sim::SurvivorsStat::kWalkPct:
      return Figure::kWalkPct;
    case ro::sim::SurvivorsStat::kCooldownPct:
      return Figure::kCooldownPct;
    case ro::sim::SurvivorsStat::kAmount:
      return Figure::kAmount;
    case ro::sim::SurvivorsStat::kAreaPct:
      return Figure::kAreaPct;
    case ro::sim::SurvivorsStat::kDurationPct:
      return Figure::kDurationPct;
    case ro::sim::SurvivorsStat::kLuckPct:
      return Figure::kLuckPct;
    case ro::sim::SurvivorsStat::kMagnetPct:
      return Figure::kMagnetPct;
    case ro::sim::SurvivorsStat::kExpPct:
      return Figure::kExpPct;
  }
  return Figure::kDamagePct;
}

std::optional<SurvivorsPickupFigure> PickupFigureOf(const ro::sim::PickupRule& rule) {
  switch (rule.kind) {
    case ro::sim::PickupKind::kMeat:
      return SurvivorsPickupFigure{.figure = Figure::kHeal, .value = rule.amount};
    case ro::sim::PickupKind::kGold:
      return SurvivorsPickupFigure{.figure = Figure::kZeny, .value = rule.amount};
    case ro::sim::PickupKind::kManual:
      return SurvivorsPickupFigure{.figure = Figure::kExpPct, .value = rule.amount};
    case ro::sim::PickupKind::kHaste:
      return SurvivorsPickupFigure{.figure = Figure::kWalkPct, .value = rule.amount};
    case ro::sim::PickupKind::kHolyWater:
      return SurvivorsPickupFigure{.figure = Figure::kPurifyCells,
                                   .value = CellsOf(rule.reach_units)};
    case ro::sim::PickupKind::kMagnetAll:
      return SurvivorsPickupFigure{.figure = Figure::kGatherGems};
    case ro::sim::PickupKind::kChest:
    case ro::sim::PickupKind::kCard:
      return std::nullopt;
  }
  return std::nullopt;
}

bool NoticedPickup(ro::sim::PickupKind kind) noexcept {
  switch (kind) {
    case ro::sim::PickupKind::kMeat:
    case ro::sim::PickupKind::kManual:
    case ro::sim::PickupKind::kHaste:
    case ro::sim::PickupKind::kHolyWater:
    case ro::sim::PickupKind::kMagnetAll:
      return true;
    case ro::sim::PickupKind::kGold:
    case ro::sim::PickupKind::kChest:
    case ro::sim::PickupKind::kCard:
      return false;
  }
  return false;
}

void PickupNotices::Follow(std::uint64_t now_ms) noexcept {
  if (now_ms < last_now_ms_) living_.clear();
  last_now_ms_ = now_ms;
}

void PickupNotices::Notice(std::span<const ro::sim::SurvivorsPickupNews> taken,
                           std::uint64_t now_ms) {
  Follow(now_ms);
  for (const ro::sim::SurvivorsPickupNews& pickup : taken) {
    if (!NoticedPickup(pickup.kind)) continue;
    if (living_.size() >= kMostPickupNotices) living_.erase(living_.begin());
    living_.push_back(Living{.kind = pickup.kind, .item_id = pickup.item_id, .since_ms = now_ms});
  }
}

void PickupNotices::ShownAt(std::uint64_t now_ms, std::vector<PickupNoticeShown>& out) {
  Follow(now_ms);
  out.clear();
  std::erase_if(living_, [now_ms](const Living& notice) {
    return now_ms >= notice.since_ms + kPickupNoticeMs;
  });
  for (const Living& notice : living_) {
    out.push_back(PickupNoticeShown{.kind = notice.kind,
                                    .item_id = notice.item_id,
                                    .fraction = static_cast<float>(now_ms - notice.since_ms) /
                                                static_cast<float>(kPickupNoticeMs)});
  }
}

void PermanentEffectsOf(const ro::sim::SurvivorsField& field, const ro::sim::SurvivorsRules& rules,
                        std::vector<SurvivorsPermanentEffect>& out) {
  out.clear();
  for (const ro::sim::OwnedCard& card : field.hero.cards) {
    const ro::sim::CardRule* rule = ro::sim::FindCard(rules, card.card_item_id);
    if (rule == nullptr) continue;
    const std::optional<std::int32_t> value = CardValueAt(*rule, card.level);
    if (!value.has_value()) continue;
    out.push_back(SurvivorsPermanentEffect{.source = SurvivorsPermanentSource::kCard,
                                           .id = card.card_item_id,
                                           .level = card.level,
                                           .figure = FigureOfStat(rule->stat),
                                           .value = *value});
  }
  // Les Bénédictions se lisent dans le bonus que le départ porte : en défi et
  // en partie classée, il est nul, et aucune ne paraît.
  const ro::sim::SurvivorsStats& bonus = field.start.loadout.bonus;
  for (std::size_t rank = 0; rank < Meta().blessings.size(); ++rank) {
    const std::optional<ro::sim::SurvivorsStat> stat = BlessedStat(Meta().blessings[rank]);
    if (!stat.has_value() || bonus.Of(*stat) == 0) continue;
    out.push_back(SurvivorsPermanentEffect{.source = SurvivorsPermanentSource::kBlessing,
                                           .id = static_cast<std::uint16_t>(rank),
                                           .figure = FigureOfStat(*stat),
                                           .value = bonus.Of(*stat)});
  }
  for (const ro::sim::KafraRule& rule : Meta().kafra) {
    const auto [given, left] = ChargesOf(field, rule.charge);
    if (given == 0) continue;
    out.push_back(SurvivorsPermanentEffect{.source = SurvivorsPermanentSource::kKafra,
                                           .id = rule.item_id,
                                           .figure = FigureOfCharge(rule.charge),
                                           .value = left});
  }
  if (field.start.lives > 0) {
    out.push_back(SurvivorsPermanentEffect{.source = SurvivorsPermanentSource::kLife,
                                           .figure = Figure::kLivesLeft,
                                           .value = field.lives_left});
  }
}

}  // namespace moonclient
