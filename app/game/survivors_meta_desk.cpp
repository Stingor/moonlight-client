#include "game/survivors_meta_desk.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/error.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/utf8_path.h"
#include "ro/sim/survivors_meta.h"
#include "ro/sim/survivors_records.h"
#include "ro/sim/survivors_week.h"

namespace moonclient {
namespace {

using ro::sim::Blessing;
using ro::sim::KafraService;

constexpr std::int64_t kDaysPerWeek = 7;
// La ligne de la conversation : assez pour le nom de la copie, jamais un chemin.
constexpr std::size_t kSayTextSize = 256;

[[nodiscard]] const ro::sim::SurvivorsMeta& Meta() { return ro::sim::ShippedSurvivorsMeta(); }

[[nodiscard]] int Clamped(std::uint64_t value) noexcept {
  return static_cast<int>(std::min<std::uint64_t>(value, std::numeric_limits<int>::max()));
}

// Le plus haut rang de Cursed Water que les règles connaissent.
[[nodiscard]] int MostCursedWater(const ro::sim::SurvivorsRules& rules) noexcept {
  return rules.cursed_water.empty() ? 0 : static_cast<int>(rules.cursed_water.size() - 1);
}

[[nodiscard]] const char* ClassKey(const ro::sim::SurvivorsRules& rules) {
  return rules.classes.empty() ? "" : rules.classes.front().key;
}

[[nodiscard]] const char* ArenaKey(const ro::sim::SurvivorsRules& rules) {
  return rules.arenas.empty() ? "" : rules.arenas.front().key;
}

[[nodiscard]] ro::ui::SurvivorsRecordRow RecordRowOf(const ro::sim::SurvivorsRecord& record,
                                                     const ro::sim::SurvivorsRules& rules,
                                                     std::optional<std::int64_t> latest_utc_s) {
  return ro::ui::SurvivorsRecordRow{
      .score = record.score,
      .survived_ms = record.survived_ms,
      .level = record.level,
      .outcome = EndOutcomeOf(record.outcome),
      .kills = record.kills,
      .cursed_water = record.cursed_water,
      .played_at_utc_s = record.played_at_utc_s,
      .latest = latest_utc_s.has_value() && record.played_at_utc_s == *latest_utc_s,
      .older_rules = record.ruleset < rules.ruleset_version,
      .week_year = record.week.year,
      .week_number = record.week.week};
}

[[nodiscard]] ro::ui::SurvivorsRecordNews RecordNewsOf(RecordVerdict verdict) noexcept {
  switch (verdict) {
    case RecordVerdict::kPlaced:
      return ro::ui::SurvivorsRecordNews::kPlaced;
    case RecordVerdict::kBelowBoard:
      return ro::ui::SurvivorsRecordNews::kBelowBoard;
    case RecordVerdict::kAbandoned:
      return ro::ui::SurvivorsRecordNews::kNotRanked;
    case RecordVerdict::kNotNeutral:
    case RecordVerdict::kNotThisWeek:
      return ro::ui::SurvivorsRecordNews::kRefused;
  }
  return ro::ui::SurvivorsRecordNews::kBelowBoard;
}

// Exhaustifs : un tableau neuf doit se voir donner son onglet, et l'inverse.
[[nodiscard]] ro::ui::SurvivorsBoardTab TabOf(SurvivorsBoardKind kind) noexcept {
  switch (kind) {
    case SurvivorsBoardKind::kChallenge:
      return ro::ui::SurvivorsBoardTab::kChallenge;
    case SurvivorsBoardKind::kRanked:
      return ro::ui::SurvivorsBoardTab::kRanked;
    case SurvivorsBoardKind::kFree:
      break;
  }
  return ro::ui::SurvivorsBoardTab::kFree;
}

[[nodiscard]] SurvivorsBoardKind BoardKindOf(ro::ui::SurvivorsBoardTab tab) noexcept {
  switch (tab) {
    case ro::ui::SurvivorsBoardTab::kChallenge:
      return SurvivorsBoardKind::kChallenge;
    case ro::ui::SurvivorsBoardTab::kRanked:
      return SurvivorsBoardKind::kRanked;
    case ro::ui::SurvivorsBoardTab::kFree:
      break;
  }
  return SurvivorsBoardKind::kFree;
}

}  // namespace

void TellDamagedCopy(const std::optional<std::filesystem::path>& copy, const SurvivorsSay& say) {
  if (!copy.has_value() || !say) return;
  std::array<char, kSayTextSize> text = {};
  std::snprintf(text.data(), text.size(),
                ro::core::Tr("Le carnet des Survivants était abîmé : copié sous %s, un carnet "
                             "neuf le remplace."),
                ro::core::Utf8FromPath(copy->filename()).c_str());
  say(std::string(text.data()));
}

bool ApplyShopAsk(SurvivorsSave& save, SurvivorsMetaShop shop, const ro::ui::SurvivorsShopAsk& ask,
                  std::optional<ro::sim::MetaRefusal>* refused) {
  SurvivorsSaveMeta& meta = save.meta;
  const SurvivorsSaveMeta before = meta;
  if (refused != nullptr) refused->reset();
  if (ask.refund_all) {
    if (shop == SurvivorsMetaShop::kChurch) {
      static_cast<void>(ro::sim::RefundChurch(meta.ranks, meta.zeny));
    } else {
      static_cast<void>(ro::sim::RefundKafra(meta.ranks, meta.zeny));
    }
  } else if (ask.buy_row.has_value()) {
    const std::size_t row = *ask.buy_row;
    ro::sim::MetaPurchase bought = ro::sim::MetaPurchase(0);
    if (shop == SurvivorsMetaShop::kChurch && row < Meta().blessings.size()) {
      bought = ro::sim::BuyBlessing(Meta(), meta.ranks, meta.zeny, Meta().blessings[row].blessing);
    } else if (shop == SurvivorsMetaShop::kKafra && row < Meta().kafra.size()) {
      bought = ro::sim::BuyKafra(Meta(), meta.ranks, meta.zeny, Meta().kafra[row].service);
    }
    if (!bought.has_value() && refused != nullptr) *refused = bought.error();
  }
  return meta != before;
}

bool ChooseCursedWater(SurvivorsSave& save, const ro::sim::SurvivorsRules& rules, int rank) {
  const auto chosen = static_cast<std::uint8_t>(std::clamp(rank, 0, MostCursedWater(rules)));
  if (save.meta.cursed_water == chosen) return false;
  save.meta.cursed_water = chosen;
  return true;
}

ro::ui::SurvivorsEndOutcome EndOutcomeOf(ro::sim::SurvivorsOutcome outcome) noexcept {
  switch (outcome) {
    case ro::sim::SurvivorsOutcome::kFell:
      return ro::ui::SurvivorsEndOutcome::kDied;
    case ro::sim::SurvivorsOutcome::kAbandoned:
      return ro::ui::SurvivorsEndOutcome::kAbandoned;
    case ro::sim::SurvivorsOutcome::kRunning:
    case ro::sim::SurvivorsOutcome::kSurvived:
    case ro::sim::SurvivorsOutcome::kVictory:
      return ro::ui::SurvivorsEndOutcome::kVictory;
  }
  return ro::ui::SurvivorsEndOutcome::kVictory;
}

void ShopRowsOf(const SurvivorsSave& save, SurvivorsMetaShop shop,
                std::vector<ro::ui::SurvivorsShopRow>& rows) {
  rows.clear();
  const ro::sim::SurvivorsMetaRanks& ranks = save.meta.ranks;
  if (shop == SurvivorsMetaShop::kChurch) {
    for (const ro::sim::BlessingRule& rule : Meta().blessings) {
      const int most = ro::sim::MostRank(Meta(), rule.blessing);
      const int rank = std::min<int>(ranks.Of(rule.blessing), most);
      const auto total_at = [&rule](int at) {
        return at > 0 ? rule.totals[static_cast<std::size_t>(at - 1)] : 0;
      };
      rows.push_back(ro::ui::SurvivorsShopRow{
          .key = rule.key,
          .description = rule.description,
          .rank = rank,
          .most_rank = most,
          .effect_now = total_at(rank),
          .effect_next = total_at(std::min(rank + 1, most)),
          .next_price = ro::sim::BlessingCost(Meta(), ranks, rule.blessing)});
    }
    return;
  }
  for (const ro::sim::KafraRule& rule : Meta().kafra) {
    const int most = ro::sim::MostRank(Meta(), rule.service);
    const int rank = std::min<int>(ranks.Of(rule.service), most);
    // Q3 : le rang r donne r charges à chaque partie libre.
    rows.push_back(
        ro::ui::SurvivorsShopRow{.key = rule.key,
                                 .rank = rank,
                                 .most_rank = most,
                                 .effect_now = rank,
                                 .effect_next = std::min(rank + 1, most),
                                 .next_price = ro::sim::KafraCost(Meta(), ranks, rule.service)});
  }
}

ro::ui::SurvivorsShopView ShopViewOf(const SurvivorsSave& save, SurvivorsMetaShop shop,
                                     std::span<const ro::ui::SurvivorsShopRow> rows) {
  const ro::sim::SurvivorsMetaRanks& ranks = save.meta.ranks;
  return ro::ui::SurvivorsShopView{.rows = rows,
                                   .zeny = save.meta.zeny,
                                   .refundable_zeny = shop == SurvivorsMetaShop::kChurch
                                                          ? ranks.church_spent
                                                          : ranks.kafra_spent};
}

ro::ui::SurvivorsPrepareMeta PrepareMetaOf(const ro::sim::SurvivorsRules& rules,
                                           const SurvivorsSave& save, SurvivorsBoardKind board,
                                           std::int64_t now_utc_s) {
  ro::ui::SurvivorsPrepareMeta meta;
  meta.board = TabOf(board);
  meta.zeny = save.meta.zeny;
  meta.most_cursed_water = MostCursedWater(rules);
  meta.cursed_water = std::min<int>(save.meta.cursed_water, meta.most_cursed_water);
  meta.cursed_water_bonus_pct =
      Clamped(ro::sim::kCursedWaterZenyPctPerRank * static_cast<std::uint64_t>(meta.cursed_water));
  for (const std::uint8_t rank : save.meta.ranks.church) meta.blessing_ranks += rank;
  const ro::sim::IsoWeek week = IsoWeekOfUtc(now_utc_s);
  meta.week_year = week.year;
  meta.week_number = week.week;
  meta.week_left_s =
      (ro::sim::MondayOfIsoWeek(week) + kDaysPerWeek) * ro::sim::kSecondsPerDay - now_utc_s;
  if (rules.classes.empty()) return meta;
  const ro::sim::ClassRule& class_rule = rules.classes.front();
  const ro::sim::SurvivorsMetaStart start = ro::sim::LoadoutOf(Meta(), save.meta.ranks, class_rule);
  meta.lives = start.lives;
  meta.charges = start.loadout.rerolls + start.loadout.skips + start.loadout.banishes;
  const std::uint16_t bolt = ChallengeFirstSkill(class_rule, week);
  const ro::sim::SurvivorsSkillRule* bolt_rule = ro::sim::FindSkill(rules, bolt);
  meta.challenge_bolt = ro::ui::SurvivorsBoltChoice{
      .skill_id = bolt, .description = bolt_rule != nullptr ? bolt_rule->description : ""};
  if (const SurvivorsBoard* best =
          FindBoard(save, class_rule.key, ArenaKey(rules), kChallengeBoardKey, week);
      best != nullptr && !best->runs.empty()) {
    meta.week_best_score = best->runs.front().score;
  }
  return meta;
}

void AlbumCardsOf(const ro::sim::SurvivorsRules& rules, const SurvivorsSave& save,
                  std::vector<ro::ui::SurvivorsAlbumEntry>& cards) {
  cards.clear();
  if (rules.classes.empty()) return;
  for (const std::uint16_t card_item_id : rules.classes.front().card_pool) {
    const ro::sim::CardRule* rule = ro::sim::FindCard(rules, card_item_id);
    ro::ui::SurvivorsAlbumEntry entry;
    if (rule != nullptr) {
      entry.description = rule->description;
      const std::string_view key = rule->key;
      const auto kept = std::ranges::find_if(
          save.album.cards, [key](const SurvivorsAlbumCard& card) { return card.key == key; });
      if (kept != save.album.cards.end()) {
        entry.found = kept->found;
        entry.first_found_day = kept->first_found_day;
      }
    }
    cards.push_back(entry);
  }
}

void FeatEntriesOf(const SurvivorsSave& save, std::vector<ro::ui::SurvivorsFeatEntry>& feats) {
  feats.clear();
  for (const SurvivorsFeat& feat : ShippedSurvivorsFeats()) {
    feats.push_back(
        ro::ui::SurvivorsFeatEntry{.key = feat.key,
                                   .measure = feat.counter == FeatCounter::kBestSurvivedMs
                                                  ? ro::ui::SurvivorsFeatMeasure::kSurvivedMs
                                                  : ro::ui::SurvivorsFeatMeasure::kCount,
                                   .progress = FeatProgressOf(save, feat),
                                   .threshold = feat.threshold,
                                   .reached = FeatReached(save, feat)});
  }
}

ro::ui::SurvivorsRecordsView RecordsViewOf(const ro::sim::SurvivorsRules& rules,
                                           const SurvivorsSave& save, std::int64_t now_utc_s,
                                           std::optional<std::int64_t> latest_utc_s,
                                           SurvivorsRecordRowsHeld& held) {
  held = SurvivorsRecordRowsHeld{};
  const ro::sim::IsoWeek week = IsoWeekOfUtc(now_utc_s);
  const std::string_view class_key = ClassKey(rules);
  const std::string_view arena_key = ArenaKey(rules);
  std::vector<const SurvivorsBoard*> past;
  for (const SurvivorsBoard& board : save.records) {
    if (board.class_key != class_key || board.arena_key != arena_key) continue;
    if (board.board_key == kFreeBoardKey) {
      for (const ro::sim::SurvivorsRecord& run : board.runs) {
        held.free_runs.push_back(RecordRowOf(run, rules, latest_utc_s));
      }
    } else if (board.board_key == kChallengeBoardKey && board.week == week) {
      for (const ro::sim::SurvivorsRecord& run : board.runs) {
        held.week_runs.push_back(RecordRowOf(run, rules, latest_utc_s));
      }
    } else if (board.board_key == kChallengeBoardKey && !board.runs.empty()) {
      past.push_back(&board);
    } else if (board.board_key == kRankedBoardKey) {
      for (const ro::sim::SurvivorsRecord& run : board.runs) {
        held.ranked_runs.push_back(RecordRowOf(run, rules, latest_utc_s));
      }
    }
  }
  // Les parties d'anciennes règles restent montrées, mais SOUS celles des
  // règles en vigueur, chacune à son rang parmi les siennes.
  for (std::vector<ro::ui::SurvivorsRecordRow>* rows :
       {&held.free_runs, &held.week_runs, &held.ranked_runs}) {
    std::ranges::stable_partition(
        *rows, [](const ro::ui::SurvivorsRecordRow& row) { return !row.older_rules; });
  }
  std::ranges::sort(past, [](const SurvivorsBoard* first, const SurvivorsBoard* second) {
    return ro::sim::IsoWeekOrdinal(first->week) > ro::sim::IsoWeekOrdinal(second->week);
  });
  for (const SurvivorsBoard* board : past) {
    ro::ui::SurvivorsRecordRow row = RecordRowOf(board->runs.front(), rules, latest_utc_s);
    row.week_year = board->week.year;
    row.week_number = board->week.week;
    held.past_weeks.push_back(row);
  }
  return ro::ui::SurvivorsRecordsView{.free_runs = held.free_runs,
                                      .week_runs = held.week_runs,
                                      .ranked_runs = held.ranked_runs,
                                      .past_weeks = held.past_weeks,
                                      .week_year = week.year,
                                      .week_number = week.week};
}

ro::ui::SurvivorsCreditRows CreditRowsOf(const SurvivorsCredit& credit,
                                         const SurvivorsEnding& ending, const SurvivorsSave& save,
                                         SurvivorsCreditRowsHeld& held) {
  held.dropped.clear();
  std::vector<std::uint16_t> unseen = credit.new_cards;
  for (const std::uint16_t card_item_id : ending.recap.cards_found) {
    // La première prise d'une carte neuve est la seule « NOUVELLE ».
    const auto fresh = std::ranges::find(unseen, card_item_id);
    held.dropped.push_back(ro::ui::SurvivorsDroppedCardRow{.first_time = fresh != unseen.end()});
    if (fresh != unseen.end()) unseen.erase(fresh);
  }
  FeatEntriesOf(save, held.feats);
  for (ro::ui::SurvivorsFeatEntry& feat : held.feats) {
    feat.fresh =
        std::ranges::find(credit.new_feats, std::string_view(feat.key)) != credit.new_feats.end();
  }
  ro::ui::SurvivorsCreditRows rows{
      .zeny_gained = credit.zeny,
      .zeny_balance = save.meta.zeny,
      .board = TabOf(ending.board.kind),
      .record = RecordNewsOf(credit.record),
      .record_rank = credit.record_rank.has_value() ? Clamped(*credit.record_rank + 1) : 0,
      .score_to_beat = credit.score_to_beat,
      .dropped = held.dropped,
      .feats = held.feats};
  return rows;
}

void TrophyRowsOf(std::span<const ro::sim::MinigameTrophy> trophies,
                  std::vector<ro::ui::SurvivorsTrophyRow>& rows) {
  rows.clear();
  for (const ro::sim::MinigameTrophy trophy : trophies) {
    rows.push_back(ro::ui::SurvivorsTrophyRow{
        .name = ro::sim::MinigameTrophies()[static_cast<std::size_t>(trophy)].name});
  }
}

SurvivorsMetaDesk::SurvivorsMetaDesk(SurvivorsSaveFile file) : file_(std::move(file)) {}

void SurvivorsMetaDesk::Reload(const std::filesystem::path& client_root, const SurvivorsSay& say) {
  SurvivorsSaveRead read = file_.Load(client_root);
  TellDamagedCopy(read.damaged_copy, say);
  if (!read.file_unread) save_ = std::move(read.save);
}

void SurvivorsMetaDesk::Mutate(const std::filesystem::path& client_root,
                               const SurvivorsSaveDelta& delta, const SurvivorsSay& say) {
  ro::core::Result<SurvivorsSaveRead> written = file_.Mutate(client_root, delta);
  if (!written) {
    ro::core::LogWarning("survivors : carnet non écrit — {}", ro::core::Describe(written.error()));
    if (say) say(ro::core::Tr("Le carnet des Survivants ne s'écrit pas : rien n'a changé."));
    return;
  }
  TellDamagedCopy(written->damaged_copy, say);
  save_ = std::move(written->save);
}

void SurvivorsMetaDesk::FollowPrepare(const ro::ui::SurvivorsPrepareAsk& asked,
                                      SurvivorsMetaWindows& windows, SurvivorsBookMode& book,
                                      const ro::sim::SurvivorsRules& rules,
                                      const std::filesystem::path& client_root,
                                      const SurvivorsSay& say) {
  if (asked.choose_board.has_value()) book.Choose(BoardKindOf(*asked.choose_board));
  if (asked.choose_cursed_water.has_value()) {
    const int rank = *asked.choose_cursed_water;
    Mutate(
        client_root,
        [&rules, rank](SurvivorsSave& save) { return ChooseCursedWater(save, rules, rank); }, say);
  }
  if (!asked.open_place.has_value()) return;
  switch (*asked.open_place) {
    case ro::ui::SurvivorsMetaPlace::kChurch:
      windows.church.Open();
      return;
    case ro::ui::SurvivorsMetaPlace::kKafra:
      windows.kafra.Open();
      return;
    case ro::ui::SurvivorsMetaPlace::kCodex:
      windows.codex.open = true;
      return;
    case ro::ui::SurvivorsMetaPlace::kRecords:
      windows.records.open = true;
      return;
  }
}

void SurvivorsMetaDesk::FollowShop(SurvivorsMetaShop shop, const ro::ui::SurvivorsShopAsk& asked,
                                   const std::filesystem::path& client_root,
                                   const SurvivorsSay& say) {
  if (asked == ro::ui::SurvivorsShopAsk{}) return;
  std::optional<ro::sim::MetaRefusal> refused;
  Mutate(
      client_root,
      [shop, &asked, &refused](SurvivorsSave& save) {
        return ApplyShopAsk(save, shop, asked, &refused);
      },
      say);
  // La fenêtre ne demande que ce qu'elle montrait achetable : un refus dit que
  // le carnet a changé depuis l'image qui l'a montré.
  if (!refused.has_value() || !say) return;
  say(*refused == ro::sim::MetaRefusal::kNotEnoughZeny
          ? ro::core::Tr("Achat refusé : Zeny insuffisants, le carnet a changé.")
          : ro::core::Tr("Achat refusé : rang maximal déjà atteint, le carnet a changé."));
}

void SurvivorsMetaDesk::FollowShops(SurvivorsMetaWindows& windows,
                                    const std::filesystem::path& client_root,
                                    const SurvivorsSay& say) {
  FollowShop(SurvivorsMetaShop::kChurch, std::exchange(windows.church_answer, {}), client_root,
             say);
  FollowShop(SurvivorsMetaShop::kKafra, std::exchange(windows.kafra_answer, {}), client_root, say);
}

void SurvivorsMetaDesk::FollowSummary(const SurvivorsAsk& asked, SurvivorsMetaWindows& windows,
                                      const std::filesystem::path& client_root,
                                      const SurvivorsSay& say) {
  if (asked.summary.church) windows.church.Open();
  FollowShop(SurvivorsMetaShop::kChurch, asked.church, client_root, say);
}

}  // namespace moonclient
