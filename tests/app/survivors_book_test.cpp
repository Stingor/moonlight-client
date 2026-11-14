// LE CARNET EN JEU : le départ d'une partie libre et du défi lu dans le carnet,
// et le crédit qu'une partie y verse — Zeny, Album, exploits, records.

#include "survivors/survivors_book.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_geometry.h"
#include "ro/sim/survivors_meta.h"
#include "ro/sim/survivors_ranked.h"
#include "ro/sim/survivors_records.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/survivors_score.h"
#include "ro/sim/survivors_state.h"
#include "ro/sim/survivors_week.h"
#include "ro/ui/survivors_album_window.h"

#include "game/survivors_book_mode.h"
#include "support/survivors_fixture.h"
#include "support/survivors_golden.h"
#include "support/temp_directory.h"
#include "survivors/survivors_codex_keys.h"
#include "survivors/survivors_save.h"
#include "survivors/survivors_sightings.h"
#include "survivors/survivors_stage.h"

using moonclient::ChallengeFirstSkill;
using moonclient::ChallengeStartOf;
using moonclient::CreditRun;
using moonclient::FreeStartOf;
using moonclient::RecordVerdict;
using moonclient::SurvivorsAlbumCard;
using moonclient::SurvivorsBoardKind;
using moonclient::SurvivorsBookMode;
using moonclient::SurvivorsCredit;
using moonclient::SurvivorsEnding;
using moonclient::SurvivorsRunBoard;
using moonclient::SurvivorsSave;
using ro::sim::IsoWeek;
using ro::sim::SurvivorsOutcome;
using ro::sim::SurvivorsStart;
using ro::test_support::TempDirectory;

namespace {

[[nodiscard]] const ro::sim::SurvivorsRules& Rules() { return ro::sim::ShippedSurvivorsRules(); }

constexpr std::uint16_t kMageClassId = 1;
constexpr IsoWeek kWeek43{.year = 2026, .week = 43};
constexpr IsoWeek kWeek44{.year = 2026, .week = 44};
constexpr std::uint64_t kFreeSeed = 77;
constexpr std::uint16_t kColdBolt = 14;
constexpr std::uint16_t kGolemCard = 4072;
constexpr std::uint16_t kMasteringCard = 4197;

// Un instant du mercredi d'une semaine, à midi UTC.
[[nodiscard]] std::int64_t NoonOf(const IsoWeek& week) {
  constexpr std::int64_t kWednesday = 2;
  constexpr std::int64_t kNoonS = 12 * ro::sim::kSecondsPerHour;
  return (ro::sim::MondayOfIsoWeek(week) + kWednesday) * ro::sim::kSecondsPerDay + kNoonS;
}

[[nodiscard]] std::uint16_t IdOf(std::string_view key) {
  for (const ro::sim::SurvivorsSkillRule& skill : Rules().skills) {
    if (key == skill.key) return skill.skill_id;
  }
  for (const ro::sim::CardRule& card : Rules().cards) {
    if (key == card.key) return card.card_item_id;
  }
  return 0;
}

// Un carnet qui a tout débloqué.
[[nodiscard]] SurvivorsSave OpenedSave() {
  SurvivorsSave save;
  save.album.cards = {SurvivorsAlbumCard{.key = "golem_card", .found = 1},
                      SurvivorsAlbumCard{.key = "mastering_card", .found = 2},
                      SurvivorsAlbumCard{.key = "eclipse_card", .found = 1},
                      SurvivorsAlbumCard{.key = "dragon_fly_card", .found = 1},
                      SurvivorsAlbumCard{.key = "vagabond_wolf_card", .found = 1},
                      SurvivorsAlbumCard{.key = "baphomet_card", .found = 1}};
  save.album.feats = {"sight",
                      "frost_diver",
                      "earth_spike",
                      "thunderstorm",
                      "water_ball",
                      "ice_wall",
                      std::string(moonclient::kMeetingFeat)};
  return save;
}

// Une partie de `steps` pas, `kills` tués, `zeny` ramassés.
[[nodiscard]] SurvivorsEnding EndingOf(SurvivorsOutcome outcome, std::uint32_t steps,
                                       std::uint32_t kills, std::uint32_t zeny,
                                       SurvivorsRunBoard board = {}) {
  SurvivorsEnding ending;
  ending.class_id = Rules().classes.front().class_id;
  ending.tally.survived_steps = steps;
  ending.tally.kills = kills;
  ending.tally.level = 12;
  ending.tally.outcome = outcome;
  ending.zeny = zeny;
  ending.board = board;
  return ending;
}

[[nodiscard]] SurvivorsStart NeutralFreeStart() {
  return FreeStartOf(Rules(), OpenedSave(), kFreeSeed);
}

}  // namespace

TEST_CASE("Carnet en jeu : un carnet vierge part sans méta, douze réserves verrouillées",
          "[app][survivors]") {
  const SurvivorsStart start = FreeStartOf(Rules(), SurvivorsSave{}, kFreeSeed);
  CHECK(start.seed == kFreeSeed);
  // Le héros part de son auto-attaque seule.
  CHECK(start.first_skill_id == 0);
  CHECK(start.loadout == ro::sim::SurvivorsLoadout{});
  CHECK(start.lives == 0);
  CHECK(start.cursed_water == 0);
  std::vector<std::uint16_t> expected{kGolemCard,
                                      kMasteringCard,
                                      IdOf("eclipse_card"),
                                      IdOf("dragon_fly_card"),
                                      IdOf("vagabond_wolf_card"),
                                      IdOf("baphomet_card"),
                                      IdOf("sight"),
                                      IdOf("frost_diver"),
                                      IdOf("earth_spike"),
                                      IdOf("thunderstorm"),
                                      IdOf("water_ball"),
                                      IdOf("ice_wall")};
  std::ranges::sort(expected);
  CHECK(start.locked_ids == expected);
  // La partie accepte ce départ.
  const ro::sim::SurvivorsRules& golden = ro::test_support::GoldenRules();
  CHECK(ro::sim::SurvivorsRun::Start(
            golden, ro::test_support::PillarArena(ro::test_support::kGoldenArenaCells),
            FreeStartOf(golden, SurvivorsSave{}, kFreeSeed))
            .has_value());
  // Tout débloqué : rien de verrouillé, et le départ est neutre.
  CHECK(ro::sim::IsNeutralStart(NeutralFreeStart()));
}

TEST_CASE("Carnet en jeu : la partie libre prend les rangs et la Cursed Water du carnet",
          "[app][survivors]") {
  SurvivorsSave save = OpenedSave();
  save.meta.ranks = ro::sim::MostMetaRanks(ro::sim::ShippedSurvivorsMeta());
  save.meta.cursed_water = 3;
  const SurvivorsStart start = FreeStartOf(Rules(), save, kFreeSeed);
  const ro::sim::SurvivorsMetaStart meta =
      ro::sim::LoadoutOf(ro::sim::ShippedSurvivorsMeta(), save.meta.ranks, Rules().classes.front());
  CHECK(start.loadout == meta.loadout);
  CHECK(start.lives == meta.lives);
  CHECK(start.lives == 1);
  CHECK(start.cursed_water == 3);
  CHECK(start.locked_ids.empty());
  // Une carte trouvée débloque la sienne seulement.
  SurvivorsSave golem_only;
  golem_only.album.cards = {SurvivorsAlbumCard{.key = "golem_card", .found = 1}};
  const SurvivorsStart partial = FreeStartOf(Rules(), golem_only, kFreeSeed);
  CHECK(std::ranges::find(partial.locked_ids, kGolemCard) == partial.locked_ids.end());
  CHECK(std::ranges::find(partial.locked_ids, kMasteringCard) != partial.locked_ids.end());
}

TEST_CASE("Carnet en jeu : le défi part neutre, le même pour deux carnets", "[app][survivors]") {
  const SurvivorsStart start = ChallengeStartOf(Rules(), kWeek43, kMageClassId);
  CHECK(ro::sim::IsNeutralStart(start));
  CHECK(start.seed == ro::sim::SeedOfWeek(2026, 43));
  // LE SKILL IMPOSÉ vient de n'importe quelle classe : chaque semaine en tire un
  // parmi les skills d'ouverture, et une année en montre de plusieurs classes.
  const std::vector<std::uint16_t> opening = ro::sim::OpeningSkills(Rules());
  std::vector<std::uint16_t> imposed;
  constexpr std::uint8_t kWeeksInYear = 52;
  for (std::uint8_t week = 1; week <= kWeeksInYear; ++week) {
    const std::uint16_t skill_id =
        ChallengeFirstSkill(Rules(), IsoWeek{.year = 2026, .week = week});
    REQUIRE(std::ranges::find(opening, skill_id) != opening.end());
    if (std::ranges::find(imposed, skill_id) == imposed.end()) imposed.push_back(skill_id);
  }
  const ro::sim::ClassRule& mage = Rules().classes.front();
  CHECK(std::ranges::any_of(imposed, [&](std::uint16_t skill_id) {
    return std::ranges::find(mage.skill_pool, skill_id) == mage.skill_pool.end();
  }));
  CHECK(imposed.size() > 1);
  CHECK(ChallengeFirstSkill(Rules(), IsoWeek{}) == 0);
  CHECK(start.first_skill_id == ChallengeFirstSkill(Rules(), kWeek43));

  // Deux carnets que tout sépare rendent le même départ de défi.
  const TempDirectory poor;
  const TempDirectory rich;
  rich.WriteText(std::filesystem::path("savedata") / "survivors.yml",
                 "meta: {zeny: 9000, cursed_water: 4,\n"
                 "       church: {ranks: {blessing: 5, resurrection: 1}},\n"
                 "       kafra: {ranks: {magnifier: 3}}}\n");
  SurvivorsBookMode book([] { return NoonOf(kWeek43); });
  book.Choose(SurvivorsBoardKind::kChallenge);
  const SurvivorsBookMode::Begun from_poor = book.StartOf(Rules(), poor.path(), kFreeSeed);
  const SurvivorsBookMode::Begun from_rich = book.StartOf(Rules(), rich.path(), kFreeSeed);
  CHECK(from_poor.start == start);
  CHECK(from_rich.start == start);
  CHECK(from_rich.board ==
        SurvivorsRunBoard{.kind = SurvivorsBoardKind::kChallenge, .week = kWeek43});
  // La même partie libre, elle, prend la méta du carnet.
  book.Choose(SurvivorsBoardKind::kFree);
  const SurvivorsBookMode::Begun free_rich = book.StartOf(Rules(), rich.path(), kFreeSeed);
  CHECK(free_rich.start.cursed_water == 4);
  CHECK(free_rich.start.lives == 1);
  CHECK(free_rich.start.loadout.rerolls == 3);
  CHECK(free_rich.board == SurvivorsRunBoard{});
}

TEST_CASE("Carnet en jeu : toute partie se joue sur l'arène retenue, la prairie par défaut",
          "[app][survivors]") {
  // LA PRAIRIE GÉNÉRÉE est l'arène par défaut du défi et de la partie classée.
  const ro::sim::ArenaRule* plains = ro::sim::FirstRankedArena(Rules());
  REQUIRE(plains != nullptr);
  CHECK(std::string_view(plains->key) == "midgard_plains");
  CHECK(plains->terrain_id != 0);
  CHECK(ChallengeStartOf(Rules(), kWeek43, kMageClassId).arena_id == plains->arena_id);
  CHECK(ro::sim::RankedStartOf(Rules(), kFreeSeed, kMageClassId, 0).arena_id == plains->arena_id);
  // LA PARTIE LIBRE part sur la prairie par défaut, sur le désert quand
  // l'accueil le retient ; un Id inconnu reprend la prairie.
  REQUIRE(moonclient::DefaultArenaOf(Rules()) == plains);
  const ro::sim::ArenaRule* field = nullptr;
  for (const ro::sim::ArenaRule& arena : Rules().arenas) {
    if (std::string_view(arena.key) == "morroc_desert") field = &arena;
  }
  REQUIRE(field != nullptr);
  REQUIRE(field->ranked);
  constexpr std::uint16_t kUnknownArena = 999;
  CHECK(FreeStartOf(Rules(), SurvivorsSave{}, kFreeSeed).arena_id == plains->arena_id);
  CHECK(FreeStartOf(Rules(), SurvivorsSave{}, kFreeSeed, field->arena_id).arena_id ==
        field->arena_id);
  CHECK(FreeStartOf(Rules(), SurvivorsSave{}, kFreeSeed, kUnknownArena).arena_id ==
        plains->arena_id);

  // LE CHOIX DE L'ACCUEIL vaut pour les trois tableaux, chaque arène ayant
  // son défi et son classement ; une classée sans jeton repart libre sur
  // l'arène qu'elle demandait.
  const TempDirectory root;
  SurvivorsBookMode book([] { return NoonOf(kWeek43); });
  CHECK(book.arena_id() == 0);
  book.ChooseArena(Rules(), field->arena_id);
  CHECK(book.arena_id() == field->arena_id);
  CHECK(book.StartOf(Rules(), root.path(), kFreeSeed).start.arena_id == field->arena_id);
  book.Choose(SurvivorsBoardKind::kChallenge);
  CHECK(book.StartOf(Rules(), root.path(), kFreeSeed).start.arena_id == field->arena_id);
  book.Choose(SurvivorsBoardKind::kRanked);
  CHECK(book.StartOf(Rules(), root.path(), kFreeSeed).start.arena_id == field->arena_id);
  const SurvivorsBookMode::Begun fallback =
      book.StartRankedAsFree(Rules(), root.path(), kFreeSeed, kMageClassId, field->arena_id);
  CHECK(fallback.board.kind == SurvivorsBoardKind::kFree);
  CHECK(fallback.start.arena_id == field->arena_id);
  // Le défi d'une semaine a la même graine sur toutes les arènes.
  CHECK(ChallengeStartOf(Rules(), kWeek43, kMageClassId, field->arena_id).seed ==
        ChallengeStartOf(Rules(), kWeek43, kMageClassId).seed);
  // Un Id que les règles ne connaissent pas ne se retient pas.
  book.ChooseArena(Rules(), kUnknownArena);
  CHECK(book.arena_id() == field->arena_id);
}

TEST_CASE("Carnet en jeu : une victoire verse ses Zeny et prend la tête du tableau",
          "[app][survivors]") {
  SurvivorsSave save;
  save.meta.zeny = 100;
  const SurvivorsStart start = NeutralFreeStart();
  constexpr std::uint32_t kSteps = 45000;
  constexpr std::uint32_t kKills = 4321;
  constexpr std::uint32_t kGold = 600;
  SurvivorsEnding ending = EndingOf(SurvivorsOutcome::kVictory, kSteps, kKills, kGold);
  const SurvivorsCredit credit = CreditRun(Rules(), save, ending, start, NoonOf(kWeek43));
  const std::uint64_t zeny = ro::sim::RunZeny(ending.tally, kGold, SurvivorsOutcome::kVictory);
  CHECK(credit.zeny == zeny);
  CHECK(credit.zeny_balance == 100 + zeny);
  CHECK(save.meta.zeny == 100 + zeny);
  CHECK(save.meta.zeny_earned == zeny);
  CHECK(credit.record == RecordVerdict::kPlaced);
  CHECK(credit.record_rank == std::size_t{0});
  REQUIRE(save.records.size() == 1);
  const moonclient::SurvivorsBoard& board = save.records.front();
  CHECK(board.class_key == "mage");
  CHECK(board.arena_key == "midgard_plains");
  CHECK(board.board_key == moonclient::kFreeBoardKey);
  REQUIRE(board.runs.size() == 1);
  const ro::sim::SurvivorsRecord& run = board.runs.front();
  CHECK(run.score == ro::sim::SurvivorsScore(ending.tally));
  CHECK(run.survived_ms == kSteps * ro::sim::kSurvivorsStepMs);
  // Une partie libre part de l'auto-attaque : aucun premier skill au record.
  CHECK(run.first_skill_key.empty());
  CHECK(run.seed == kFreeSeed);
  CHECK(run.ruleset == Rules().ruleset_version);
  CHECK(run.played_at_utc_s == NoonOf(kWeek43));
  CHECK(run.week == IsoWeek{});
  CHECK(save.album.counters.runs == 1);
  CHECK(save.album.counters.best_level == 12);
}

TEST_CASE("Carnet en jeu : la Cursed Water majore les Zeny et le score rangé", "[app][survivors]") {
  SurvivorsSave save = OpenedSave();
  save.meta.cursed_water = 2;
  const SurvivorsStart start = FreeStartOf(Rules(), save, kFreeSeed);
  const SurvivorsEnding ending = EndingOf(SurvivorsOutcome::kFell, 9000, 1000, 1000);
  const SurvivorsCredit credit = CreditRun(Rules(), save, ending, start, NoonOf(kWeek43));
  // (1 000 + 1 000 / 10) × 120 / 100.
  CHECK(credit.zeny == 1320);
  CHECK(credit.score == ro::sim::FreeRunScore(ending.tally, 2));
  CHECK(save.records.front().runs.front().cursed_water == 2);
}

TEST_CASE("Carnet en jeu : partir sans minimap majore les Zeny de la partie libre seule",
          "[app][survivors]") {
  SurvivorsSave save = OpenedSave();
  save.meta.cursed_water = 2;
  const SurvivorsStart start = FreeStartOf(Rules(), save, kFreeSeed);
  const SurvivorsRunBoard blind{.without_minimap = true};
  const SurvivorsCredit credit =
      CreditRun(Rules(), save, EndingOf(SurvivorsOutcome::kFell, 9000, 1000, 1000, blind), start,
                NoonOf(kWeek43));
  // 1 320 de la Cursed Water 2, puis × 115 / 100.
  CHECK(credit.zeny == 1518);
  // Le score rangé n'en sait rien.
  CHECK(credit.score ==
        ro::sim::FreeRunScore(EndingOf(SurvivorsOutcome::kFell, 9000, 1000, 1000).tally, 2));
  // Un défi qui porterait le drapeau ne gagne rien.
  const SurvivorsRunBoard challenge{
      .kind = SurvivorsBoardKind::kChallenge, .week = kWeek43, .without_minimap = true};
  const SurvivorsCredit refused =
      CreditRun(Rules(), save, EndingOf(SurvivorsOutcome::kFell, 9000, 1000, 1000, challenge),
                ChallengeStartOf(Rules(), kWeek43, kMageClassId), NoonOf(kWeek43));
  CHECK(refused.zeny == 1100);
  CHECK(moonclient::WithoutMinimapZeny(std::numeric_limits<std::uint64_t>::max(), true) ==
        std::numeric_limits<std::uint64_t>::max());
}

TEST_CASE("Carnet en jeu : la onzième partie d'un tableau plein n'y entre pas",
          "[app][survivors]") {
  SurvivorsSave save;
  const SurvivorsStart start = NeutralFreeStart();
  constexpr std::uint32_t kLongSteps = 30000;
  constexpr std::uint32_t kStepGap = 1000;
  for (std::size_t run = 0; run < ro::sim::kRecordsPerBoard; ++run) {
    const auto steps = static_cast<std::uint32_t>(kLongSteps + run * kStepGap);
    const SurvivorsCredit credit = CreditRun(
        Rules(), save, EndingOf(SurvivorsOutcome::kFell, steps, 0, 0), start, NoonOf(kWeek43));
    CHECK(credit.record == RecordVerdict::kPlaced);
  }
  const SurvivorsCredit eleventh = CreditRun(
      Rules(), save, EndingOf(SurvivorsOutcome::kFell, kStepGap, 0, 0), start, NoonOf(kWeek43));
  CHECK(eleventh.record == RecordVerdict::kBelowBoard);
  CHECK_FALSE(eleventh.record_rank.has_value());
  // Le score à battre est celui de la dernière ligne du tableau.
  CHECK(eleventh.score_to_beat == save.records.front().runs.back().score);
  CHECK(eleventh.played_at_utc_s == NoonOf(kWeek43));
  CHECK(save.records.front().runs.size() == ro::sim::kRecordsPerBoard);
  CHECK(save.album.counters.runs == ro::sim::kRecordsPerBoard + 1);
}

TEST_CASE("Carnet en jeu : dix parties d'anciennes règles n'arrêtent pas une partie neuve",
          "[app][survivors]") {
  SurvivorsSave save;
  const SurvivorsStart start = NeutralFreeStart();
  constexpr std::uint32_t kLongSteps = 30000;
  constexpr std::uint32_t kStepGap = 1000;
  for (std::size_t run = 0; run < ro::sim::kRecordsPerBoard; ++run) {
    const auto steps = static_cast<std::uint32_t>(kLongSteps + run * kStepGap);
    (void)CreditRun(Rules(), save, EndingOf(SurvivorsOutcome::kFell, steps, 0, 0), start,
                    NoonOf(kWeek43));
  }
  REQUIRE(save.records.size() == 1);
  const auto older_rules = static_cast<std::uint16_t>(Rules().ruleset_version - 1);
  for (ro::sim::SurvivorsRecord& kept : save.records.front().runs) kept.ruleset = older_rules;
  // Moins bien notée que les dix, elle entre pourtant en tête : elle est des
  // règles en vigueur.
  const SurvivorsCredit fresh = CreditRun(
      Rules(), save, EndingOf(SurvivorsOutcome::kFell, kStepGap, 0, 0), start, NoonOf(kWeek43));
  CHECK(fresh.record == RecordVerdict::kPlaced);
  CHECK(fresh.record_rank == std::optional<std::size_t>(0));
  CHECK_FALSE(fresh.score_to_beat.has_value());
  const std::vector<ro::sim::SurvivorsRecord>& runs = save.records.front().runs;
  REQUIRE(runs.size() == ro::sim::kRecordsPerBoard);
  CHECK(runs.front().ruleset == Rules().ruleset_version);
  CHECK(runs.back().ruleset == older_rules);
}

TEST_CASE("Carnet en jeu : le tableau du défi refuse un départ chargé ou une autre semaine",
          "[app][survivors]") {
  const SurvivorsRunBoard week43{.kind = SurvivorsBoardKind::kChallenge, .week = kWeek43};
  const SurvivorsEnding ending = EndingOf(SurvivorsOutcome::kFell, 20000, 500, 50, week43);
  SurvivorsStart loaded = ChallengeStartOf(Rules(), kWeek43, kMageClassId);
  loaded.loadout.rerolls = 1;
  SurvivorsSave save;
  const SurvivorsCredit refused = CreditRun(Rules(), save, ending, loaded, NoonOf(kWeek43));
  CHECK(refused.record == RecordVerdict::kNotNeutral);
  CHECK(save.records.empty());
  // Les Zeny se versent quand même (Q10).
  CHECK(refused.zeny == 100);
  // La graine d'une autre semaine, ou un autre bolt : refusés aussi.
  const SurvivorsStart other_week = ChallengeStartOf(Rules(), kWeek44, kMageClassId);
  CHECK(CreditRun(Rules(), save, ending, other_week, NoonOf(kWeek43)).record ==
        RecordVerdict::kNotThisWeek);
  SurvivorsStart other_bolt = ChallengeStartOf(Rules(), kWeek43, kMageClassId);
  other_bolt.first_skill_id = kColdBolt;
  CHECK(CreditRun(Rules(), save, ending, other_bolt, NoonOf(kWeek43)).record ==
        RecordVerdict::kNotThisWeek);
  CHECK(save.records.empty());
  // Le bon départ entre au tableau de sa semaine.
  const SurvivorsCredit placed = CreditRun(
      Rules(), save, ending, ChallengeStartOf(Rules(), kWeek43, kMageClassId), NoonOf(kWeek43));
  CHECK(placed.record == RecordVerdict::kPlaced);
  REQUIRE(save.records.size() == 1);
  CHECK(save.records.front().board_key == moonclient::kChallengeBoardKey);
  CHECK(save.records.front().week == kWeek43);
  CHECK(save.records.front().runs.front().week == kWeek43);
}

TEST_CASE("Carnet en jeu : une semaine de défi passée ne garde que sa meilleure partie",
          "[app][survivors]") {
  const SurvivorsRunBoard week43{.kind = SurvivorsBoardKind::kChallenge, .week = kWeek43};
  const SurvivorsStart start = ChallengeStartOf(Rules(), kWeek43, kMageClassId);
  SurvivorsSave save;
  constexpr std::array<std::uint32_t, 3> kSteps{20000, 40000, 30000};
  for (const std::uint32_t steps : kSteps) {
    (void)CreditRun(Rules(), save, EndingOf(SurvivorsOutcome::kFell, steps, 0, 0, week43), start,
                    NoonOf(kWeek43));
  }
  REQUIRE(save.records.front().runs.size() == 3);
  // Une partie de la semaine suivante ramène la 43 à sa meilleure.
  const SurvivorsRunBoard week44{.kind = SurvivorsBoardKind::kChallenge, .week = kWeek44};
  (void)CreditRun(Rules(), save, EndingOf(SurvivorsOutcome::kFell, 10000, 0, 0, week44),
                  ChallengeStartOf(Rules(), kWeek44, kMageClassId), NoonOf(kWeek44));
  REQUIRE(save.records.size() == 2);
  REQUIRE(save.records[0].runs.size() == 1);
  CHECK(save.records[0].runs.front().survived_ms == 40000 * ro::sim::kSurvivorsStepMs);
  CHECK(save.records[1].week == kWeek44);
  // Une partie de la 43 finie en semaine 44 derrière sa meilleure n'y reste pas.
  const SurvivorsCredit late = CreditRun(
      Rules(), save, EndingOf(SurvivorsOutcome::kFell, 5000, 0, 0, week43), start, NoonOf(kWeek44));
  CHECK(late.record == RecordVerdict::kBelowBoard);
  CHECK(save.records[0].runs.size() == 1);
}

TEST_CASE("Carnet en jeu : l'exploit Sight tombe à 5:00 pile", "[app][survivors]") {
  constexpr std::uint32_t kFiveMinutesSteps = 5 * 60 * 1000 / ro::sim::kSurvivorsStepMs;
  SurvivorsSave save;
  const SurvivorsStart start = FreeStartOf(Rules(), save, kFreeSeed);
  const SurvivorsCredit short_of =
      CreditRun(Rules(), save, EndingOf(SurvivorsOutcome::kFell, kFiveMinutesSteps - 1, 0, 0),
                start, NoonOf(kWeek43));
  CHECK(short_of.new_feats.empty());
  const SurvivorsCredit reached =
      CreditRun(Rules(), save, EndingOf(SurvivorsOutcome::kFell, kFiveMinutesSteps, 0, 0), start,
                NoonOf(kWeek43));
  CHECK(reached.new_feats == std::vector<std::string>{"sight"});
  CHECK(save.album.feats == std::vector<std::string>{"sight"});
  // Sight sort des verrous de la partie suivante ; il ne s'atteint qu'une fois.
  const SurvivorsStart next = FreeStartOf(Rules(), save, kFreeSeed);
  CHECK(std::ranges::find(next.locked_ids, IdOf("sight")) == next.locked_ids.end());
  CHECK(CreditRun(Rules(), save, EndingOf(SurvivorsOutcome::kFell, kFiveMinutesSteps, 0, 0), start,
                  NoonOf(kWeek43))
            .new_feats.empty());
  // Frost Diver : 500 glaçages au cumul, sur plusieurs parties.
  SurvivorsEnding chilly = EndingOf(SurvivorsOutcome::kFell, 100, 0, 0);
  chilly.recap.chills = 250;
  CHECK(CreditRun(Rules(), save, chilly, start, NoonOf(kWeek43)).new_feats.empty());
  CHECK(CreditRun(Rules(), save, chilly, start, NoonOf(kWeek43)).new_feats ==
        std::vector<std::string>{"frost_diver"});
  CHECK(save.album.counters.chills == 500);
}

TEST_CASE("Carnet en jeu : une partie d'équipe verse ses Zeny, sans record", "[app][survivors]") {
  SurvivorsSave save;
  const SurvivorsStart start = FreeStartOf(Rules(), save, kFreeSeed);
  SurvivorsEnding ending = EndingOf(SurvivorsOutcome::kFell, 6000, 250, 75);
  ending.board.coop = true;
  const SurvivorsCredit credit = CreditRun(Rules(), save, ending, start, NoonOf(kWeek43));
  CHECK(credit.zeny > 0);
  CHECK(credit.record == RecordVerdict::kAbandoned);
  CHECK(save.records.empty());
}

TEST_CASE("Carnet en jeu : un abandon verse ses Zeny et ses cartes, sans record",
          "[app][survivors]") {
  SurvivorsSave save;
  const SurvivorsStart start = FreeStartOf(Rules(), save, kFreeSeed);
  SurvivorsEnding ending = EndingOf(SurvivorsOutcome::kAbandoned, 6000, 250, 75);
  ending.recap.cards_found = {kGolemCard, IdOf("hornet_card"), kGolemCard};
  // Un monstre Wind compte pour Thunderstorm.
  constexpr std::uint64_t kWindKills = 9;
  for (const ro::sim::FoeRule& foe : Rules().foes) {
    if (foe.element == ro::sim::SurvivorsElement::kWind) {
      ending.recap.kills_by_foe.push_back({.id = foe.foe_id, .value = kWindKills});
      break;
    }
  }
  REQUIRE_FALSE(ending.recap.kills_by_foe.empty());
  const SurvivorsCredit credit = CreditRun(Rules(), save, ending, start, NoonOf(kWeek43));
  CHECK(credit.zeny == 75 + 25);
  CHECK(credit.record == RecordVerdict::kAbandoned);
  CHECK(save.records.empty());
  CHECK(credit.new_cards == std::vector<std::uint16_t>{kGolemCard, IdOf("hornet_card")});
  REQUIRE(save.album.cards.size() == 2);
  CHECK(save.album.cards[0].key == "golem_card");
  CHECK(save.album.cards[0].found == 2);
  CHECK(save.album.cards[0].first_found_day == ro::sim::MondayOfIsoWeek(kWeek43) + 2);
  CHECK(save.album.counters.wind_kills == kWindKills);
  // La Golem Card tombée se débloque pour la partie suivante.
  const SurvivorsStart next = FreeStartOf(Rules(), save, kFreeSeed);
  CHECK(std::ranges::find(next.locked_ids, kGolemCard) == next.locked_ids.end());
}

TEST_CASE("Carnet en jeu : le mode crédite une partie une seule fois", "[app][survivors]") {
  const TempDirectory root;
  SurvivorsBookMode book([] { return NoonOf(kWeek43); });
  moonclient::SurvivorsStage stage;
  stage.Open();
  const ro::sim::SurvivorsRules& golden = ro::test_support::GoldenRules();
  const SurvivorsBookMode::Begun begun = book.StartOf(golden, root.path(), kFreeSeed);
  REQUIRE(stage
              .Begin(golden, ro::test_support::PillarArena(ro::test_support::kGoldenArenaCells),
                     begun.start, begun.board)
              .has_value());
  stage.Loaded();
  // Une partie figée tout de suite : abandonnée au premier pas.
  REQUIRE(stage.Freeze());
  book.Credit(stage, golden, root.path());
  REQUIRE(book.credit().has_value());
  CHECK(book.credit()->record == RecordVerdict::kAbandoned);
  book.Credit(stage, golden, root.path());
  book.FollowEntered(stage, golden, root.path());
  const moonclient::SurvivorsSaveRead read = moonclient::SurvivorsSaveFile().Load(root.path());
  CHECK(read.save.album.counters.runs == 1);
  // Une partie neuve se crédite à son tour.
  book.Restart();
  CHECK_FALSE(book.credit().has_value());
}

// ── LE CODEX DU CARNET ─────────────────────────────────────────────────────

TEST_CASE("Carnet en jeu : ce qui a été vu s'ajoute sans doublon et dans l'ordre",
          "[app][survivors][codex]") {
  using moonclient::CodexCategory;
  SurvivorsSave save;
  save.codex.Add(CodexCategory::kFoes, "1063");
  moonclient::SurvivorsCodexSeen sightings;
  sightings.Add(CodexCategory::kFoes, "1007");
  sightings.Add(CodexCategory::kFoes, "1063");
  sightings.Add(CodexCategory::kFoes, "1002");
  CHECK(moonclient::CreditSightings(save, sightings));
  const auto& foes = save.codex.lists[static_cast<std::size_t>(CodexCategory::kFoes)];
  REQUIRE(foes.size() == 3);
  CHECK(foes[0].text == "1063");
  CHECK(foes[1].text == "1007");
  CHECK(foes[2].text == "1002");
  // Le même relevé une seconde fois ne change rien.
  CHECK_FALSE(moonclient::CreditSightings(save, sightings));
  CHECK(foes.size() == 3);
}

TEST_CASE("Carnet en jeu : une partie abandonnée crédite aussi ce qu'elle a vu",
          "[app][survivors][codex]") {
  const TempDirectory root;
  SurvivorsBookMode book([] { return NoonOf(kWeek43); });
  moonclient::SurvivorsStage stage;
  stage.Open();
  const ro::sim::SurvivorsRules& golden = ro::test_support::GoldenRules();
  const SurvivorsBookMode::Begun begun = book.StartOf(golden, root.path(), kFreeSeed);
  REQUIRE(stage
              .Begin(golden, ro::test_support::PillarArena(ro::test_support::kGoldenArenaCells),
                     begun.start, begun.board)
              .has_value());
  stage.Loaded();
  REQUIRE(stage.Freeze());
  REQUIRE_FALSE(stage.sightings() == moonclient::SurvivorsCodexSeen{});
  book.Credit(stage, golden, root.path());
  REQUIRE(book.credit().has_value());
  CHECK(book.credit()->record == RecordVerdict::kAbandoned);
  const moonclient::SurvivorsSaveRead read = moonclient::SurvivorsSaveFile().Load(root.path());
  CHECK(read.save.codex == stage.sightings());
}

TEST_CASE("Carnet survivors : le Chevalier se débloque par une partie tenue 10:00",
          "[app][survivors]") {
  constexpr std::uint16_t kKnightClass = 2;
  constexpr std::uint16_t kThiefClass = 3;
  const ro::sim::ClassRule* knight = ro::sim::FindClass(Rules(), kKnightClass);
  REQUIRE(knight != nullptr);
  const std::uint16_t mage_class = Rules().classes.front().class_id;

  // VERROUILLÉ : la partie libre part en Magicien, le bolt choisi gardé.
  const SurvivorsSave fresh;
  CHECK(moonclient::ClassUnlocked(Rules(), Rules().classes.front(), fresh));
  CHECK_FALSE(moonclient::ClassUnlocked(Rules(), *knight, fresh));
  const SurvivorsStart refused = FreeStartOf(Rules(), fresh, kFreeSeed, 0, kKnightClass);
  CHECK(refused.class_id == mage_class);
  CHECK(refused.first_skill_id == 0);

  // DÉBLOQUÉ par l'exploit d'Ice Wall : la partie libre part en Chevalier, et
  // un bolt du Magicien laisse la place à son premier skill de départ.
  SurvivorsSave held = fresh;
  held.album.feats = {"ice_wall"};
  CHECK(moonclient::ClassUnlocked(Rules(), *knight, held));
  const SurvivorsStart knight_start = FreeStartOf(Rules(), held, kFreeSeed, 0, kKnightClass);
  CHECK(knight_start.class_id == kKnightClass);
  CHECK(knight_start.first_skill_id == 0);
  CHECK(ro::sim::SurvivorsRun::Start(Rules(), knight_start).has_value());

  // LE VOLEUR, par sa rencontre : verrouillé sans elle, ouvert dès qu'elle est
  // au carnet, et le Chevalier garde son exploit à lui.
  const ro::sim::ClassRule* thief = ro::sim::FindClass(Rules(), kThiefClass);
  REQUIRE(thief != nullptr);
  CHECK(moonclient::ClassUnlockFeat(*thief) == moonclient::kMeetingFeat);
  CHECK(moonclient::ClassUnlockFeat(*knight) == "ice_wall");
  CHECK_FALSE(moonclient::ClassUnlocked(Rules(), *thief, fresh));
  CHECK_FALSE(moonclient::ClassUnlocked(Rules(), *thief, held));
  SurvivorsSave met = fresh;
  CHECK(moonclient::InscribeFeat(met, moonclient::kMeetingFeat));
  CHECK_FALSE(moonclient::InscribeFeat(met, moonclient::kMeetingFeat));
  CHECK(met.album.feats.size() == 1);
  CHECK(moonclient::ClassUnlocked(Rules(), *thief, met));
  CHECK_FALSE(moonclient::ClassUnlocked(Rules(), *knight, met));
  CHECK(FreeStartOf(Rules(), met, kFreeSeed, 0, kThiefClass).class_id == kThiefClass);
  CHECK(std::string_view(moonclient::ClassTitleOf(*thief)) == "Assassin");
  // Le carnet le garde, et aucun compteur ne l'atteint ni ne le retire.
  auto reread = moonclient::ReadSurvivorsSave(moonclient::WriteSurvivorsSave(met));
  REQUIRE(reread.has_value());
  CHECK(moonclient::ClassUnlocked(Rules(), *thief, *reread));
  const auto meeting_feat =
      std::ranges::find(moonclient::ShippedSurvivorsFeats(), moonclient::kMeetingFeat,
                        [](const auto& feat) { return std::string_view(feat.key); });
  REQUIRE(meeting_feat != moonclient::ShippedSurvivorsFeats().end());
  CHECK(moonclient::FeatProgressOf(fresh, *meeting_feat) == 0);
  CHECK(moonclient::FeatProgressOf(met, *meeting_feat) == 1);

  // Chaque classe livrée porte son nom, distinct de celui de son métier.
  CHECK(std::string_view(moonclient::ClassTitleOf(*knight)) == "Knight");
  CHECK(std::string_view(moonclient::ClassTitleOf(Rules().classes.front())) == "Wizard");

  // Le défi et la partie classée jouent la classe qu'on leur donne, chacune son
  // premier skill, la même graine de semaine pour toutes.
  const SurvivorsStart mage_week = ChallengeStartOf(Rules(), kWeek44, mage_class);
  const SurvivorsStart knight_week = ChallengeStartOf(Rules(), kWeek44, kKnightClass);
  CHECK(mage_week.class_id == mage_class);
  CHECK(knight_week.class_id == kKnightClass);
  CHECK(knight_week.seed == mage_week.seed);
  // Le skill imposé est le même pour toutes les classes, même hors de leur réserve.
  CHECK(knight_week.first_skill_id == ChallengeFirstSkill(Rules(), kWeek44));
  CHECK(mage_week.first_skill_id == knight_week.first_skill_id);
  CHECK(ro::sim::IsNeutralStart(knight_week));
  CHECK(ro::sim::SurvivorsRun::Start(Rules(), knight_week).has_value());
}

// `ui` désigne le but d'un exploit par la clé du carnet : une clé renommée d'un seul
// côté laisserait l'exploit sans but, en silence.
TEST_CASE("Carnet survivors : chaque exploit a son but dans l'interface", "[app][survivors]") {
  for (const moonclient::SurvivorsFeat& feat : moonclient::ShippedSurvivorsFeats()) {
    INFO("exploit " << feat.key);
    CHECK_FALSE(std::string_view(ro::ui::SurvivorsFeatGoal(feat.key)).empty());
  }
  CHECK_FALSE(std::string_view(ro::ui::SurvivorsFeatGoal(moonclient::kMeetingFeat)).empty());
}
