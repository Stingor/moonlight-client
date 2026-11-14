// LA MÉTA À L'ACCUEIL : les intentions des fenêtres de la méta routées vers le
// carnet relu (achat, remboursement, tableau, Cursed Water, lieu ouvert), la
// copie d'un carnet abîmé dite au joueur, l'Église ouverte depuis l'écran de
// fin dont l'achat fait le départ de « Rejouer », et les lignes que le carnet
// donne aux fenêtres.

#include "game/survivors_meta_desk.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/minigame_trophies.h"
#include "ro/sim/survivors_meta.h"
#include "ro/sim/survivors_records.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_week.h"

#include "game/survivors_book_mode.h"
#include "game/ui_stage_survivors.h"
#include "support/temp_directory.h"
#include "survivors/survivors_book.h"
#include "survivors/survivors_save.h"
#include "survivors/survivors_stage.h"

using moonclient::ApplyShopAsk;
using moonclient::ChallengeFirstSkill;
using moonclient::ChooseCursedWater;
using moonclient::CreditRowsOf;
using moonclient::PrepareMetaOf;
using moonclient::RecordsViewOf;
using moonclient::ShopRowsOf;
using moonclient::SurvivorsAsk;
using moonclient::SurvivorsBoard;
using moonclient::SurvivorsBoardKind;
using moonclient::SurvivorsBookMode;
using moonclient::SurvivorsCredit;
using moonclient::SurvivorsCreditRowsHeld;
using moonclient::SurvivorsEnding;
using moonclient::SurvivorsMetaDesk;
using moonclient::SurvivorsMetaShop;
using moonclient::SurvivorsMetaWindows;
using moonclient::SurvivorsRecordRowsHeld;
using moonclient::SurvivorsSave;
using moonclient::SurvivorsSaveFile;
using moonclient::SurvivorsSay;
using ro::sim::IsoWeek;
using ro::test_support::TempDirectory;

namespace {

[[nodiscard]] const ro::sim::SurvivorsRules& Rules() { return ro::sim::ShippedSurvivorsRules(); }

// La prairie, l'arène par défaut : celle du défi et de la partie classée.
[[nodiscard]] const char* PlainsKey() { return moonclient::DefaultArenaOf(Rules())->key; }
[[nodiscard]] const ro::sim::SurvivorsMeta& Meta() { return ro::sim::ShippedSurvivorsMeta(); }

constexpr IsoWeek kWeek42{.year = 2026, .week = 42};
constexpr IsoWeek kWeek43{.year = 2026, .week = 43};
constexpr std::uint64_t kSeed = 77;
constexpr std::uint16_t kGolemCard = 4072;
// La première ligne de l'Église : Blessing.
constexpr std::size_t kBlessingRow = 0;
constexpr std::uint64_t kRichZeny = 100000;

const std::filesystem::path kSavePath = std::filesystem::path("savedata") / "survivors.yml";

// Un instant du mercredi d'une semaine, à midi UTC.
[[nodiscard]] std::int64_t NoonOf(const IsoWeek& week) {
  constexpr std::int64_t kWednesday = 2;
  constexpr std::int64_t kNoonS = 12 * ro::sim::kSecondsPerHour;
  return (ro::sim::MondayOfIsoWeek(week) + kWednesday) * ro::sim::kSecondsPerDay + kNoonS;
}

[[nodiscard]] SurvivorsSave Reread(const TempDirectory& root) {
  return SurvivorsSaveFile().Load(root.path()).save;
}

// Les lignes dites au joueur.
struct Heard {
  std::vector<std::string> lines;
  [[nodiscard]] SurvivorsSay Say() {
    return [this](std::string text) { lines.push_back(std::move(text)); };
  }
};

}  // namespace

TEST_CASE("Méta : un achat depuis la vue de l'Église mute le carnet relu", "[app][survivors]") {
  const TempDirectory root;
  root.WriteText(kSavePath, "meta: {zeny: 100000}\n");
  SurvivorsMetaDesk desk;
  Heard heard;
  desk.Reload(root.path(), heard.Say());
  std::vector<ro::ui::SurvivorsShopRow> rows;
  ShopRowsOf(desk.save(), SurvivorsMetaShop::kChurch, rows);
  REQUIRE(rows.size() == Meta().blessings.size());
  const std::uint64_t price = rows[kBlessingRow].next_price;
  REQUIRE(price > 0);
  CHECK(rows[kBlessingRow].rank == 0);

  SurvivorsMetaWindows windows;
  windows.church_answer.buy_row = kBlessingRow;
  desk.FollowShops(windows, root.path(), heard.Say());
  CHECK(windows.church_answer == ro::ui::SurvivorsShopAsk{});
  const SurvivorsSave written = Reread(root);
  CHECK(written.meta.ranks.Of(ro::sim::Blessing::kBlessing) == 1);
  CHECK(written.meta.zeny == kRichZeny - price);
  CHECK(desk.save() == written);
  // La vue suivante montre le rang acheté et le prix monté.
  ShopRowsOf(desk.save(), SurvivorsMetaShop::kChurch, rows);
  CHECK(rows[kBlessingRow].rank == 1);
  CHECK(rows[kBlessingRow].next_price > price);
  CHECK(heard.lines.empty());

  // UN AUTRE CLIENT A TOUT DÉPENSÉ ENTRE-TEMPS : l'achat se juge sur le carnet
  // relu, et se refuse sans rien écrire.
  root.WriteText(kSavePath, "meta: {zeny: 0}\n");
  windows.church_answer.buy_row = kBlessingRow;
  desk.FollowShops(windows, root.path(), heard.Say());
  CHECK(Reread(root).meta.ranks.Of(ro::sim::Blessing::kBlessing) == 0);
  CHECK(desk.save().meta.zeny == 0);
}

TEST_CASE("Méta : « Tout rembourser » rend ce que la Kafra a reçu", "[app][survivors]") {
  SurvivorsSave save;
  save.meta.zeny = kRichZeny;
  REQUIRE(ApplyShopAsk(save, SurvivorsMetaShop::kKafra, {.buy_row = 0}));
  REQUIRE(ApplyShopAsk(save, SurvivorsMetaShop::kKafra, {.buy_row = 0}));
  CHECK(save.meta.ranks.Of(ro::sim::KafraService::kMagnifier) == 2);
  CHECK(save.meta.zeny < kRichZeny);
  CHECK(ApplyShopAsk(save, SurvivorsMetaShop::kKafra, {.refund_all = true}));
  CHECK(save.meta.zeny == kRichZeny);
  CHECK(save.meta.ranks.Of(ro::sim::KafraService::kMagnifier) == 0);
  // Rien à rendre, rien d'écrit ; une ligne hors du catalogue non plus.
  CHECK_FALSE(ApplyShopAsk(save, SurvivorsMetaShop::kKafra, {.refund_all = true}));
  CHECK_FALSE(ApplyShopAsk(save, SurvivorsMetaShop::kKafra, {.buy_row = Meta().kafra.size()}));
}

TEST_CASE("Méta : l'accueil choisit le tableau, la Cursed Water et le lieu ouvert",
          "[app][survivors]") {
  const TempDirectory root;
  SurvivorsMetaDesk desk;
  SurvivorsBookMode book([] { return NoonOf(kWeek43); });
  SurvivorsMetaWindows windows;
  Heard heard;
  desk.FollowPrepare({.choose_board = ro::ui::SurvivorsBoardTab::kChallenge,
                      .choose_cursed_water = 99,
                      .open_place = ro::ui::SurvivorsMetaPlace::kCodex},
                     windows, book, Rules(), root.path(), heard.Say());
  CHECK(book.chosen() == SurvivorsBoardKind::kChallenge);
  const int most = static_cast<int>(Rules().cursed_water.size()) - 1;
  CHECK(Reread(root).meta.cursed_water == most);
  CHECK(windows.codex.open);
  CHECK_FALSE(windows.church.open);
  CHECK_FALSE(windows.records.open);
  desk.FollowPrepare({.choose_board = ro::ui::SurvivorsBoardTab::kFree}, windows, book, Rules(),
                     root.path(), heard.Say());
  CHECK(book.chosen() == SurvivorsBoardKind::kFree);
  // L'arène de la partie libre : retenue au carnet du mode, et seulement une
  // arène des règles.
  constexpr std::uint16_t kUnknownArena = 999;
  const std::uint16_t field_id = Rules().arenas[1].arena_id;
  desk.FollowPrepare({.choose_arena_id = field_id}, windows, book, Rules(), root.path(),
                     heard.Say());
  CHECK(book.free_arena_id() == field_id);
  desk.FollowPrepare({.choose_arena_id = kUnknownArena}, windows, book, Rules(), root.path(),
                     heard.Say());
  CHECK(book.free_arena_id() == field_id);

  SurvivorsSave save;
  CHECK_FALSE(ChooseCursedWater(save, Rules(), -1));
  CHECK(ChooseCursedWater(save, Rules(), 2));
  CHECK(save.meta.cursed_water == 2);
}

TEST_CASE("Méta : l'Église ouverte depuis l'écran de fin fait le départ de « Rejouer »",
          "[app][survivors]") {
  const TempDirectory root;
  root.WriteText(kSavePath, "meta: {zeny: 100000}\n");
  SurvivorsMetaDesk desk;
  SurvivorsBookMode book([] { return NoonOf(kWeek43); });
  SurvivorsMetaWindows windows;
  Heard heard;
  const SurvivorsBookMode::Begun before = book.StartOf(Rules(), root.path(), kSeed);
  CHECK(before.start.loadout == ro::sim::SurvivorsLoadout{});

  SurvivorsAsk church;
  church.summary.church = true;
  desk.FollowSummary(church, windows, root.path(), heard.Say());
  CHECK(windows.church.open);
  SurvivorsAsk buy;
  buy.church.buy_row = kBlessingRow;
  desk.FollowSummary(buy, windows, root.path(), heard.Say());

  // « Rejouer » relit le carnet : la partie part de la Bénédiction achetée.
  const SurvivorsBookMode::Begun replay = book.StartOf(Rules(), root.path(), kSeed);
  ro::sim::SurvivorsMetaRanks ranks;
  ranks.church[static_cast<std::size_t>(ro::sim::Blessing::kBlessing)] = 1;
  CHECK(replay.start.loadout == ro::sim::LoadoutOf(Meta(), ranks, Rules().classes.front()).loadout);
  CHECK(replay.start.loadout != ro::sim::SurvivorsLoadout{});
}

TEST_CASE("Méta : un carnet abîmé copié de côté se dit dans la conversation", "[app][survivors]") {
  const TempDirectory root;
  root.WriteText(kSavePath, "meta: [zeny: {\n");
  SurvivorsMetaDesk desk;
  Heard heard;
  desk.Reload(root.path(), heard.Say());
  REQUIRE(heard.lines.size() == 1);
  CHECK(heard.lines.front().find("survivors.yml.damaged-") != std::string::npos);
  CHECK(desk.save() == SurvivorsSave{});
  // Remplacé par un carnet neuf : la relecture suivante ne dit plus rien.
  desk.Reload(root.path(), heard.Say());
  CHECK(heard.lines.size() == 1);

  // La copie que le départ d'une partie a faite se dit une fois.
  root.WriteText(kSavePath, "meta: [zeny: {\n");
  SurvivorsBookMode book([] { return NoonOf(kWeek43); });
  static_cast<void>(book.StartOf(Rules(), root.path(), kSeed));
  moonclient::TellDamagedCopy(book.TakeDamagedCopy(), heard.Say());
  CHECK(heard.lines.size() == 2);
  CHECK_FALSE(book.TakeDamagedCopy().has_value());
}

TEST_CASE("Méta : l'accueil montre le départ acheté et la semaine du défi", "[app][survivors]") {
  SurvivorsSave save;
  save.meta.zeny = kRichZeny;
  save.meta.cursed_water = 3;
  save.meta.ranks.church[static_cast<std::size_t>(ro::sim::Blessing::kBlessing)] = 2;
  save.meta.ranks.church[static_cast<std::size_t>(ro::sim::Blessing::kResurrection)] = 1;
  save.meta.ranks.kafra[static_cast<std::size_t>(ro::sim::KafraService::kMagnifier)] = 2;
  save.records.push_back(SurvivorsBoard{
      .class_key = Rules().classes.front().key,
      .arena_key = PlainsKey(),
      .board_key = std::string(moonclient::kChallengeBoardKey),
      .week = kWeek43,
      .runs = {ro::sim::SurvivorsRecord{.score = 4321, .ruleset = Rules().ruleset_version}}});
  const ro::ui::SurvivorsPrepareMeta meta =
      PrepareMetaOf(Rules(), save, SurvivorsBoardKind::kChallenge, NoonOf(kWeek43));
  CHECK(meta.board == ro::ui::SurvivorsBoardTab::kChallenge);
  CHECK(meta.zeny == kRichZeny);
  CHECK(meta.cursed_water == 3);
  CHECK(meta.cursed_water_bonus_pct == 30);
  CHECK(meta.blessing_ranks == 3);
  CHECK(meta.lives == 1);
  CHECK(meta.charges == 2);
  CHECK(meta.week_year == 2026);
  CHECK(meta.week_number == 43);
  // Du mercredi midi au lundi 00:00 UTC : quatre jours et demi.
  constexpr std::int64_t kFourDaysAndAHalfS =
      4 * ro::sim::kSecondsPerDay + ro::sim::kSecondsPerDay / 2;
  CHECK(meta.week_left_s == kFourDaysAndAHalfS);
  CHECK(meta.challenge_bolt.skill_id == ChallengeFirstSkill(Rules(), kWeek43));
  CHECK(meta.week_best_score == std::optional<std::uint64_t>(4321));
}

TEST_CASE("Méta : les records rangent la semaine, les semaines passées et la dernière partie",
          "[app][survivors]") {
  SurvivorsSave save;
  const auto board = [](std::string_view key, IsoWeek week, std::uint64_t score,
                        std::int64_t played_at_utc_s) {
    return SurvivorsBoard{.class_key = Rules().classes.front().key,
                          .arena_key = PlainsKey(),
                          .board_key = std::string(key),
                          .week = week,
                          .runs = {ro::sim::SurvivorsRecord{.score = score,
                                                            .ruleset = Rules().ruleset_version,
                                                            .played_at_utc_s = played_at_utc_s,
                                                            .week = week}}};
  };
  constexpr std::int64_t kLatestS = 1'800'000'000;
  save.records.push_back(board(moonclient::kFreeBoardKey, IsoWeek{}, 10, kLatestS));
  save.records.push_back(board(moonclient::kChallengeBoardKey, kWeek42, 20, 1));
  save.records.push_back(
      board(moonclient::kChallengeBoardKey, IsoWeek{.year = 2026, .week = 40}, 30, 2));
  save.records.push_back(board(moonclient::kChallengeBoardKey, kWeek43, 40, 3));
  SurvivorsRecordRowsHeld held;
  const ro::ui::SurvivorsRecordsView view =
      RecordsViewOf(Rules(), save, NoonOf(kWeek43), kLatestS, held);
  REQUIRE(view.free_runs.size() == 1);
  CHECK(view.free_runs.front().latest);
  REQUIRE(view.week_runs.size() == 1);
  CHECK(view.week_runs.front().score == 40);
  CHECK_FALSE(view.week_runs.front().latest);
  REQUIRE(view.past_weeks.size() == 2);
  CHECK(view.past_weeks[0].week_number == 42);
  CHECK(view.past_weeks[1].week_number == 40);
  CHECK(view.week_number == 43);
}

TEST_CASE("Méta : les records d'anciennes règles restent, marqués, sous ceux des règles en vigueur",
          "[app][survivors]") {
  const std::uint16_t current = Rules().ruleset_version;
  const auto older = static_cast<std::uint16_t>(current - 1);
  const auto run = [](std::uint64_t score, std::uint16_t ruleset) {
    return ro::sim::SurvivorsRecord{.score = score, .ruleset = ruleset};
  };
  SurvivorsSave save;
  save.records.push_back(SurvivorsBoard{.class_key = Rules().classes.front().key,
                                        .arena_key = PlainsKey(),
                                        .board_key = std::string(moonclient::kFreeBoardKey),
                                        .week = IsoWeek{},
                                        .runs = {}});
  // La partie ancienne la mieux notée se range pourtant sous les neuves.
  for (const ro::sim::SurvivorsRecord& played :
       {run(900, older), run(500, current), run(400, older), run(300, current)}) {
    REQUIRE(ro::sim::PlaceRecord(save.records.front().runs, played).has_value());
  }
  SurvivorsRecordRowsHeld held;
  const ro::ui::SurvivorsRecordsView view =
      RecordsViewOf(Rules(), save, NoonOf(kWeek43), std::nullopt, held);
  REQUIRE(view.free_runs.size() == 4);
  CHECK(view.free_runs[0].score == 500);
  CHECK(view.free_runs[1].score == 300);
  CHECK(view.free_runs[2].score == 900);
  CHECK(view.free_runs[3].score == 400);
  CHECK_FALSE(view.free_runs[0].older_rules);
  CHECK_FALSE(view.free_runs[1].older_rules);
  CHECK(view.free_runs[2].older_rules);
  CHECK(view.free_runs[3].older_rules);
}

TEST_CASE("Méta : les tableaux sont ceux de la prairie, ceux du ruleset 3 restent dessous (Q10)",
          "[app][survivors]") {
  const std::uint16_t current = Rules().ruleset_version;
  const auto older = static_cast<std::uint16_t>(current - 1);
  const ro::sim::ArenaRule* plains = moonclient::DefaultArenaOf(Rules());
  const ro::sim::ArenaRule* field = nullptr;
  for (const ro::sim::ArenaRule& arena : Rules().arenas) {
    if (!arena.ranked) field = &arena;
  }
  REQUIRE(plains != nullptr);
  REQUIRE(field != nullptr);
  const auto board = [](const char* arena_key, std::string_view board_key, IsoWeek week,
                        std::vector<ro::sim::SurvivorsRecord> runs) {
    return SurvivorsBoard{.class_key = Rules().classes.front().key,
                          .arena_key = arena_key,
                          .board_key = std::string(board_key),
                          .week = week,
                          .runs = std::move(runs)};
  };
  const auto run = [](std::uint64_t score, std::uint16_t ruleset) {
    return ro::sim::SurvivorsRecord{.score = score, .ruleset = ruleset};
  };
  SurvivorsSave save;
  // Le carnet d'avant le ruleset 4, tout sur une autre arène, et des parties neuves
  // sur chaque arène.
  save.records.push_back(board(field->key, moonclient::kFreeBoardKey, IsoWeek{},
                               {run(800, current), run(900, older), run(700, older)}));
  save.records.push_back(
      board(plains->key, moonclient::kFreeBoardKey, IsoWeek{}, {run(100, current)}));
  save.records.push_back(
      board(field->key, moonclient::kChallengeBoardKey, kWeek43, {run(600, older)}));
  save.records.push_back(
      board(plains->key, moonclient::kChallengeBoardKey, kWeek43, {run(50, current)}));
  save.records.push_back(
      board(field->key, moonclient::kChallengeBoardKey, kWeek42, {run(650, older)}));
  save.records.push_back(
      board(field->key, moonclient::kRankedBoardKey, IsoWeek{}, {run(500, older)}));

  // LA PRAIRIE PAR DÉFAUT : ses parties, puis celles du ruleset 3, marquées ;
  // la partie neuve de l'autre arène reste au tableau de celle-ci.
  SurvivorsRecordRowsHeld held;
  const ro::ui::SurvivorsRecordsView view =
      RecordsViewOf(Rules(), save, NoonOf(kWeek43), std::nullopt, held);
  REQUIRE(view.free_runs.size() == 3);
  CHECK(view.free_runs[0].score == 100);
  CHECK_FALSE(view.free_runs[0].older_rules);
  CHECK(view.free_runs[1].score == 900);
  CHECK(view.free_runs[1].older_rules);
  CHECK(view.free_runs[2].score == 700);
  CHECK(view.free_runs[2].older_rules);
  REQUIRE(view.week_runs.size() == 2);
  CHECK(view.week_runs[0].score == 50);
  CHECK(view.week_runs[1].score == 600);
  CHECK(view.week_runs[1].older_rules);
  REQUIRE(view.past_weeks.size() == 1);
  CHECK(view.past_weeks[0].score == 650);
  CHECK(view.past_weeks[0].older_rules);
  REQUIRE(view.ranked_runs.size() == 1);
  CHECK(view.ranked_runs[0].older_rules);

  // PRT_FILD08 RETENUE POUR LA PARTIE LIBRE : son tableau, anciennes règles
  // comprises ; le défi et les parties classées restent ceux de la prairie.
  const ro::ui::SurvivorsRecordsView field_view =
      RecordsViewOf(Rules(), save, NoonOf(kWeek43), std::nullopt, held, field->arena_id);
  REQUIRE(field_view.free_runs.size() == 3);
  CHECK(field_view.free_runs[0].score == 800);
  CHECK_FALSE(field_view.free_runs[0].older_rules);
  CHECK(field_view.free_runs[1].score == 900);
  CHECK(field_view.free_runs[2].score == 700);
  REQUIRE(field_view.week_runs.size() == 2);
  CHECK(field_view.week_runs[0].score == 50);

  // LE MEILLEUR DE LA SEMAINE, à l'accueil, est celui du défi de la prairie.
  const ro::ui::SurvivorsPrepareMeta meta =
      PrepareMetaOf(Rules(), save, SurvivorsBoardKind::kChallenge, NoonOf(kWeek43));
  CHECK(meta.week_best_score == std::optional<std::uint64_t>(50));
}

TEST_CASE("Méta : l'écran de fin dit le crédit, une carte neuve une fois, et les succès",
          "[app][survivors]") {
  SurvivorsCredit credit;
  credit.zeny = 120;
  credit.zeny_balance = 900;
  credit.new_cards = {kGolemCard};
  credit.record = moonclient::RecordVerdict::kPlaced;
  credit.record_rank = 2;
  SurvivorsEnding ending;
  ending.board.kind = SurvivorsBoardKind::kChallenge;
  ending.recap.cards_found = {kGolemCard, kGolemCard};
  SurvivorsSave save;
  save.meta.zeny = 900;
  SurvivorsCreditRowsHeld held;
  const ro::ui::SurvivorsCreditRows rows = CreditRowsOf(credit, ending, save, held);
  CHECK(rows.zeny_gained == 120);
  CHECK(rows.zeny_balance == 900);
  CHECK(rows.board == ro::ui::SurvivorsBoardTab::kChallenge);
  CHECK(rows.record == ro::ui::SurvivorsRecordNews::kPlaced);
  CHECK(rows.record_rank == 3);
  REQUIRE(rows.dropped.size() == 2);
  CHECK(rows.dropped[0].first_time);
  CHECK_FALSE(rows.dropped[1].first_time);
  CHECK(rows.feats.size() == moonclient::ShippedSurvivorsFeats().size());

  // LES SUCCÈS NE DÉPENDENT PAS DU CRÉDIT : leurs lignes se font sans lui.
  const std::vector<ro::sim::MinigameTrophy> trophies = {
      ro::sim::MinigameTrophy::kSurvivorsVictory};
  std::vector<ro::ui::SurvivorsTrophyRow> trophy_rows;
  moonclient::TrophyRowsOf(trophies, trophy_rows);
  REQUIRE(trophy_rows.size() == 1);
  CHECK(std::string_view(trophy_rows.front().name) ==
        ro::sim::MinigameTrophies()[static_cast<std::size_t>(
                                        ro::sim::MinigameTrophy::kSurvivorsVictory)]
            .name);
  moonclient::TrophyRowsOf({}, trophy_rows);
  CHECK(trophy_rows.empty());

  // Un abandon n'entre pas aux records ; un défi refusé se dit refusé.
  credit.record = moonclient::RecordVerdict::kAbandoned;
  CHECK(CreditRowsOf(credit, ending, SurvivorsSave{}, held).record ==
        ro::ui::SurvivorsRecordNews::kNotRanked);
  credit.record = moonclient::RecordVerdict::kNotNeutral;
  CHECK(CreditRowsOf(credit, ending, SurvivorsSave{}, held).record ==
        ro::ui::SurvivorsRecordNews::kRefused);
}

TEST_CASE("Méta : seul l'exploit que la partie a atteint s'annonce neuf", "[app][survivors]") {
  SurvivorsSave save;
  save.album.feats = {"sight"};
  SurvivorsCredit credit;
  const SurvivorsEnding ending;
  SurvivorsCreditRowsHeld held;
  const auto sight = [&held]() -> const ro::ui::SurvivorsFeatEntry& {
    const auto found = std::ranges::find(
        held.feats, std::string_view("sight"),
        [](const ro::ui::SurvivorsFeatEntry& feat) { return std::string_view(feat.key); });
    REQUIRE(found != held.feats.end());
    return *found;
  };
  // Atteint par une partie passée : atteint, pas neuf.
  static_cast<void>(CreditRowsOf(credit, ending, save, held));
  CHECK(sight().reached);
  CHECK_FALSE(sight().fresh);
  // Atteint par celle-ci : neuf.
  credit.new_feats = {"sight"};
  static_cast<void>(CreditRowsOf(credit, ending, save, held));
  CHECK(sight().reached);
  CHECK(sight().fresh);
}

TEST_CASE("Méta : le solde de l'écran de fin suit un achat fait à l'Église", "[app][survivors]") {
  const TempDirectory root;
  root.WriteText(kSavePath, "meta: {zeny: 100000}\n");
  SurvivorsMetaDesk desk;
  Heard heard;
  desk.Reload(root.path(), heard.Say());
  SurvivorsCredit credit;
  credit.zeny_balance = kRichZeny;
  const SurvivorsEnding ending;
  SurvivorsCreditRowsHeld held;
  CHECK(CreditRowsOf(credit, ending, desk.save(), held).zeny_balance == kRichZeny);

  SurvivorsMetaWindows windows;
  SurvivorsAsk buy;
  buy.church.buy_row = kBlessingRow;
  desk.FollowSummary(buy, windows, root.path(), heard.Say());
  REQUIRE(desk.save().meta.zeny < kRichZeny);
  CHECK(CreditRowsOf(credit, ending, desk.save(), held).zeny_balance == desk.save().meta.zeny);
}

TEST_CASE("Méta : un achat refusé par le carnet relu se dit au joueur", "[app][survivors]") {
  const TempDirectory root;
  root.WriteText(kSavePath, "meta: {zeny: 0}\n");
  SurvivorsMetaDesk desk;
  SurvivorsMetaWindows windows;
  Heard heard;
  windows.church_answer.buy_row = kBlessingRow;
  desk.FollowShops(windows, root.path(), heard.Say());
  REQUIRE(heard.lines.size() == 1);
  CHECK(heard.lines.front().find("Zeny insuffisants") != std::string::npos);

  SurvivorsSave save;
  std::optional<ro::sim::MetaRefusal> refused;
  CHECK_FALSE(ApplyShopAsk(save, SurvivorsMetaShop::kChurch, {.buy_row = kBlessingRow}, &refused));
  CHECK(refused == ro::sim::MetaRefusal::kNotEnoughZeny);
  // Ce qui s'achète, comme ce qui n'est pas un achat, ne pose aucun refus.
  save.meta.zeny = kRichZeny;
  CHECK(ApplyShopAsk(save, SurvivorsMetaShop::kChurch, {.buy_row = kBlessingRow}, &refused));
  CHECK_FALSE(refused.has_value());
  CHECK_FALSE(ApplyShopAsk(save, SurvivorsMetaShop::kChurch, {.buy_row = Meta().blessings.size()},
                           &refused));
  CHECK_FALSE(refused.has_value());
}

TEST_CASE("Méta : une boutique rouverte oublie la confirmation de remboursement",
          "[app][survivors]") {
  const TempDirectory root;
  SurvivorsMetaDesk desk;
  SurvivorsBookMode book([] { return NoonOf(kWeek43); });
  SurvivorsMetaWindows windows;
  Heard heard;
  // Fermée par Échap, confirmation en suspens : la pile ne touche que `open`.
  windows.church.confirming_refund = true;
  windows.kafra.confirming_refund = true;
  desk.FollowPrepare({.open_place = ro::ui::SurvivorsMetaPlace::kChurch}, windows, book, Rules(),
                     root.path(), heard.Say());
  CHECK(windows.church.open);
  CHECK_FALSE(windows.church.confirming_refund);
  desk.FollowPrepare({.open_place = ro::ui::SurvivorsMetaPlace::kKafra}, windows, book, Rules(),
                     root.path(), heard.Say());
  CHECK(windows.kafra.open);
  CHECK_FALSE(windows.kafra.confirming_refund);
  // Depuis l'écran de fin aussi.
  windows.church.open = false;
  windows.church.confirming_refund = true;
  SurvivorsAsk church;
  church.summary.church = true;
  desk.FollowSummary(church, windows, root.path(), heard.Say());
  CHECK(windows.church.open);
  CHECK_FALSE(windows.church.confirming_refund);
}

TEST_CASE("Records survivors : la partie libre montre la classe retenue à l'accueil",
          "[app][survivors]") {
  constexpr std::uint16_t kKnightClass = 2;
  const ro::sim::ClassRule* knight = ro::sim::FindClass(Rules(), kKnightClass);
  REQUIRE(knight != nullptr);
  const auto board = [](const char* class_key, std::uint64_t score) {
    SurvivorsBoard kept{.class_key = class_key,
                        .arena_key = PlainsKey(),
                        .board_key = std::string(moonclient::kFreeBoardKey),
                        .week = IsoWeek{},
                        .runs = {}};
    kept.runs.push_back(
        ro::sim::SurvivorsRecord{.score = score, .ruleset = Rules().ruleset_version});
    return kept;
  };
  SurvivorsSave save;
  save.records.push_back(board(Rules().classes.front().key, 700));
  save.records.push_back(board(knight->key, 900));
  SurvivorsRecordRowsHeld held;

  // Le Chevalier encore verrouillé : les parties du Magicien.
  const ro::ui::SurvivorsRecordsView mage =
      RecordsViewOf(Rules(), save, NoonOf(kWeek43), std::nullopt, held, 0, kKnightClass);
  REQUIRE(mage.free_runs.size() == 1);
  CHECK(mage.free_runs[0].score == 700);

  // Débloqué et retenu : les siennes seules.
  save.album.feats = {"ice_wall"};
  const ro::ui::SurvivorsRecordsView chosen =
      RecordsViewOf(Rules(), save, NoonOf(kWeek43), std::nullopt, held, 0, kKnightClass);
  REQUIRE(chosen.free_runs.size() == 1);
  CHECK(chosen.free_runs[0].score == 900);
}

TEST_CASE("Records et accueil survivors : le défi et la partie classée suivent la classe retenue",
          "[app][survivors]") {
  constexpr std::uint16_t kKnightClass = 2;
  const ro::sim::ClassRule* knight = ro::sim::FindClass(Rules(), kKnightClass);
  REQUIRE(knight != nullptr);
  const ro::sim::ClassRule& mage = Rules().classes.front();
  const auto board = [](const char* class_key, std::string_view board_key, const IsoWeek& week,
                        std::uint64_t score) {
    SurvivorsBoard kept{.class_key = class_key,
                        .arena_key = PlainsKey(),
                        .board_key = std::string(board_key),
                        .week = week,
                        .runs = {}};
    kept.runs.push_back(
        ro::sim::SurvivorsRecord{.score = score, .ruleset = Rules().ruleset_version});
    return kept;
  };
  SurvivorsSave save;
  save.records.push_back(board(mage.key, moonclient::kRankedBoardKey, IsoWeek{}, 700));
  save.records.push_back(board(knight->key, moonclient::kRankedBoardKey, IsoWeek{}, 900));
  save.records.push_back(board(mage.key, moonclient::kChallengeBoardKey, kWeek43, 71));
  save.records.push_back(board(knight->key, moonclient::kChallengeBoardKey, kWeek43, 91));
  SurvivorsRecordRowsHeld held;

  // Le Chevalier encore verrouillé : tout est au Magicien, l'accueil le dit.
  const ro::ui::SurvivorsRecordsView mage_view =
      RecordsViewOf(Rules(), save, NoonOf(kWeek43), std::nullopt, held, 0, kKnightClass);
  REQUIRE(mage_view.ranked_runs.size() == 1);
  CHECK(mage_view.ranked_runs[0].score == 700);
  REQUIRE(mage_view.week_runs.size() == 1);
  CHECK(mage_view.week_runs[0].score == 71);
  const ro::ui::SurvivorsPrepareMeta mage_meta =
      PrepareMetaOf(Rules(), save, SurvivorsBoardKind::kChallenge, NoonOf(kWeek43), kKnightClass);
  CHECK(mage_meta.challenge_bolt.skill_id == ChallengeFirstSkill(Rules(), kWeek43));
  CHECK(mage_meta.week_best_score == 71);

  // Débloqué et retenu : ses tableaux, et le bolt que la semaine lui impose.
  save.album.feats = {std::string(moonclient::ClassUnlockFeat(*knight))};
  const ro::ui::SurvivorsRecordsView knight_view =
      RecordsViewOf(Rules(), save, NoonOf(kWeek43), std::nullopt, held, 0, kKnightClass);
  REQUIRE(knight_view.ranked_runs.size() == 1);
  CHECK(knight_view.ranked_runs[0].score == 900);
  REQUIRE(knight_view.week_runs.size() == 1);
  CHECK(knight_view.week_runs[0].score == 91);
  const ro::ui::SurvivorsPrepareMeta knight_meta =
      PrepareMetaOf(Rules(), save, SurvivorsBoardKind::kChallenge, NoonOf(kWeek43), kKnightClass);
  CHECK(knight_meta.challenge_bolt.skill_id == ChallengeFirstSkill(Rules(), kWeek43));
  CHECK(knight_meta.week_best_score == 91);
}

TEST_CASE("Méta : le meilleur de la semaine ne se lit que sous les règles en vigueur",
          "[app][survivors]") {
  const std::uint16_t current = Rules().ruleset_version;
  const auto older = static_cast<std::uint16_t>(current - 1);
  SurvivorsSave save;
  save.records.push_back(SurvivorsBoard{.class_key = Rules().classes.front().key,
                                        .arena_key = PlainsKey(),
                                        .board_key = std::string(moonclient::kChallengeBoardKey),
                                        .week = kWeek43,
                                        .runs = {}});
  const auto best = [&save] {
    return PrepareMetaOf(Rules(), save, SurvivorsBoardKind::kChallenge, NoonOf(kWeek43))
        .week_best_score;
  };
  REQUIRE(ro::sim::PlaceRecord(save.records.front().runs,
                               ro::sim::SurvivorsRecord{.score = 900, .ruleset = older})
              .has_value());
  CHECK_FALSE(best().has_value());
  REQUIRE(ro::sim::PlaceRecord(save.records.front().runs,
                               ro::sim::SurvivorsRecord{.score = 400, .ruleset = current})
              .has_value());
  CHECK(best() == std::optional<std::uint64_t>(400));
}

TEST_CASE("survivors_meta_desk : l'Album montre la réserve de la classe, Peco Peco au Chevalier",
          "[app][survivors]") {
  constexpr std::uint16_t kKnightClass = 2;
  constexpr std::string_view kPecoPecoKey = "peco_peco_card";
  constexpr std::uint32_t kFoundTwice = 2;
  const ro::sim::ClassRule* knight = ro::sim::FindClass(Rules(), kKnightClass);
  REQUIRE(knight != nullptr);
  const ro::sim::CardRule* mount = ro::sim::FindCard(Rules(), knight->mount_card_item_id);
  REQUIRE(mount != nullptr);
  REQUIRE(std::string_view(mount->key) == kPecoPecoKey);

  SurvivorsSave save;
  save.album.cards.push_back(
      moonclient::SurvivorsAlbumCard{.key = std::string(kPecoPecoKey), .found = kFoundTwice});

  std::vector<ro::ui::SurvivorsAlbumEntry> knight_cards;
  moonclient::AlbumCardsOf(Rules(), save, knight->card_pool, knight_cards);
  REQUIRE(knight_cards.size() == knight->card_pool.size());
  const auto mount_rank = std::ranges::find(knight->card_pool, knight->mount_card_item_id);
  REQUIRE(mount_rank != knight->card_pool.end());
  const ro::ui::SurvivorsAlbumEntry& shown =
      knight_cards[static_cast<std::size_t>(std::distance(knight->card_pool.begin(), mount_rank))];
  CHECK(shown.found == kFoundTwice);
  CHECK(std::string_view(shown.description) == mount->description);

  // Le Magicien ne l'a pas dans sa réserve.
  const ro::sim::ClassRule& mage = Rules().classes.front();
  CHECK(std::ranges::find(mage.card_pool, knight->mount_card_item_id) == mage.card_pool.end());
  std::vector<ro::ui::SurvivorsAlbumEntry> mage_cards;
  moonclient::AlbumCardsOf(Rules(), save, mage.card_pool, mage_cards);
  CHECK(mage_cards.size() == mage.card_pool.size());
  CHECK(std::ranges::none_of(
      mage_cards, [](const ro::ui::SurvivorsAlbumEntry& card) { return card.found != 0; }));
}
