// LE CARNET DU MODE SURVIVORS : `savedata\survivors.yml`, lu avec tolérance
// champ par champ, réécrit sans rien perdre de ce qu'un client plus récent y
// range, muté par relecture, et copié de côté quand il est abîmé.

#include "survivors/survivors_save.h"

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/sim/survivors_meta.h"
#include "ro/sim/survivors_records.h"
#include "ro/sim/survivors_week.h"

#include "support/temp_directory.h"

using moonclient::ApplySurvivorsSaveDelta;
using moonclient::DamagedCopyName;
using moonclient::LoadSurvivorsOptions;
using moonclient::ReadSurvivorsSave;
using moonclient::SaveSurvivorsOptions;
using moonclient::SurvivorsAlbumCard;
using moonclient::SurvivorsBoard;
using moonclient::SurvivorsForeignEntry;
using moonclient::SurvivorsOptions;
using moonclient::SurvivorsPendingResult;
using moonclient::SurvivorsSave;
using moonclient::SurvivorsSaveApplied;
using moonclient::SurvivorsSaveFile;
using moonclient::SurvivorsSaveRead;
using moonclient::WriteSurvivorsSave;
using ro::sim::Blessing;
using ro::sim::KafraService;
using ro::test_support::TempDirectory;

namespace {

constexpr std::uint16_t kColdBolt = 14;
constexpr std::uint16_t kKnightClass = 2;

[[nodiscard]] SurvivorsOptions AllSet() {
  return SurvivorsOptions{.reduced_effects = true,
                          .show_measures = true,
                          .horde_sun_shadows = true,
                          .pause_while_typing = false,
                          .presence_shown = false,
                          .class_id = kKnightClass,
                          .objective_skill_id = kColdBolt};
}

[[nodiscard]] SurvivorsSave Read(std::string_view text) {
  ro::core::Result<SurvivorsSave> read = ReadSurvivorsSave(text);
  REQUIRE(read.has_value());
  return *read;
}

[[nodiscard]] std::int64_t Day(std::int32_t year, std::uint8_t month, std::uint8_t day) {
  return ro::sim::DayOfCivilDate(ro::sim::CivilDate{.year = year, .month = month, .day = day});
}

// Une partie classée en attente de son verdict, chaque champ posé.
[[nodiscard]] SurvivorsPendingResult RankedPending() {
  constexpr std::int64_t kIssuedAtS = 1'793'000'000;
  constexpr std::int64_t kRunS = 1'000;
  constexpr std::int64_t kRetryS = 60;
  return SurvivorsPendingResult{.server_key = "127.0.0.1:5121",
                                .character_id = 150'001,
                                .character_name = "Élise",
                                .ticket = 0xA1B2'C3D4,
                                .issued_at_utc_s = kIssuedAtS,
                                .ruleset = 3,
                                .arena_key = "prt_fild08",
                                .class_key = "mage",
                                .board_key = "ranked",
                                .seed = 0xFEDC'BA98'7654'3210ULL,
                                .components = {.survived_steps = 48'300,
                                               .kill_points = 279'966,
                                               .kills = 2'345,
                                               .boss_kills = 2,
                                               .level = 49,
                                               .outcome = ro::sim::SurvivorsOutcome::kVictory,
                                               .journal_bytes = 6'144},
                                .local_score = 319'526,
                                .ended_at_utc_s = kIssuedAtS + kRunS,
                                .send_count = 2,
                                .last_sent_utc_s = kIssuedAtS + kRunS + kRetryS};
}

// Un carnet dont chaque section porte quelque chose.
[[nodiscard]] SurvivorsSave FullSave() {
  constexpr std::int64_t kAfternoonS = 50'602;  // 14:03:22
  SurvivorsSave save;
  save.options = AllSet();
  save.meta.zeny = 12'340;
  save.meta.zeny_earned = 98'765;
  save.meta.ranks.church[static_cast<std::size_t>(Blessing::kBlessing)] = 3;
  save.meta.ranks.church[static_cast<std::size_t>(Blessing::kResurrection)] = 1;
  save.meta.ranks.kafra[static_cast<std::size_t>(KafraService::kFlyWing)] = 2;
  save.meta.ranks.church_spent = 4'321;
  save.meta.ranks.kafra_spent = 900;
  save.meta.foreign_church_ranks.push_back({.key = "lex_aeterna", .yaml = "2"});
  save.meta.cursed_water = 3;
  save.meta.without_minimap = true;
  save.meta.without_recipe_hints = true;
  save.album.cards.push_back(
      {.key = "hornet_card", .found = 3, .first_found_day = Day(2026, 10, 30)});
  save.album.cards.push_back({.key = "golem_card", .found = 0, .first_found_day = 0});
  save.album.feats = {"sight"};
  save.album.counters = {
      .chills = 312, .wind_kills = 45, .best_level = 31, .best_survived_ms = 912'000, .runs = 17};
  ro::sim::SurvivorsRecord record;
  record.score = 45'678;
  record.survived_ms = 900'000;
  record.level = 31;
  record.outcome = ro::sim::SurvivorsOutcome::kVictory;
  record.kills = 2'345;
  record.cursed_water = 2;
  record.ruleset = 2;
  record.first_skill_key = "cold_bolt";
  record.seed = 0xFEDC'BA98'7654'3210ULL;
  record.played_at_utc_s = Day(2026, 10, 30) * ro::sim::kSecondsPerDay + kAfternoonS;
  SurvivorsBoard free_board{.class_key = "mage", .arena_key = "prt_fild08", .board_key = "free"};
  free_board.runs.push_back(record);
  record.week = ro::sim::IsoWeek{.year = 2026, .week = 44};
  record.cursed_water = 0;
  SurvivorsBoard challenge{.class_key = "mage",
                           .arena_key = "prt_fild08",
                           .board_key = "challenge",
                           .week = record.week};
  challenge.runs.push_back(record);
  save.records = {free_board, challenge};
  save.foreign_pending = {"{token: abc, score: 12}"};
  save.pending_results = {RankedPending()};
  save.foreign_sections.push_back({.key = "arena_unlocks", .yaml = "{prt_fild09: true}"});
  return save;
}

[[nodiscard]] std::string FileText(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream text;
  text << file.rdbuf();
  return text.str();
}

// 2026-10-30 14:03:22 UTC.
[[nodiscard]] std::chrono::system_clock::time_point FixedNow() {
  constexpr std::int64_t kAfternoonS = 50'602;
  return std::chrono::system_clock::time_point(
      std::chrono::seconds(Day(2026, 10, 30) * ro::sim::kSecondsPerDay + kAfternoonS));
}

}  // namespace

TEST_CASE("Carnet survivors : un texte vide est un carnet vierge", "[app][survivors]") {
  CHECK(Read("") == SurvivorsSave{});
  CHECK(Read("# rien\n") == SurvivorsSave{});
  // Une section d'un mauvais type garde ses défauts sans abîmer le reste.
  CHECK(Read("options: 3\n").options == SurvivorsOptions{});
  CHECK(Read("options: 3\nmeta: {zeny: 7}\n").meta.zeny == 7);
}

TEST_CASE(
    "Carnet survivors : un texte que le YAML refuse ou une racine qui n'est pas une table "
    "sont abîmés",
    "[app][survivors]") {
  CHECK_FALSE(ReadSurvivorsSave("{{{ pas du yaml").has_value());
  CHECK_FALSE(ReadSurvivorsSave("- une\n- liste\n").has_value());
  CHECK_FALSE(ReadSurvivorsSave("juste un mot\n").has_value());
}

TEST_CASE("Carnet survivors : l'aller-retour complet garde chaque champ", "[app][survivors]") {
  const SurvivorsSave full = FullSave();
  const std::string text = WriteSurvivorsSave(full);
  CHECK(Read(text) == full);
  CHECK(text.find("date: 2026-10-30T14:03:22Z") != std::string::npos);
  CHECK(text.find("week: 2026-W44") != std::string::npos);
  CHECK(text.find("first_found: 2026-10-30") != std::string::npos);
  // Un carnet vierge s'écrit avec ses sections, et se relit vierge.
  const std::string blank = WriteSurvivorsSave(SurvivorsSave{});
  CHECK(blank.find("pending_results: []") != std::string::npos);
  CHECK(Read(blank) == SurvivorsSave{});
}

TEST_CASE("Carnet survivors : les réglages, un champ illisible garde son défaut",
          "[app][survivors]") {
  const SurvivorsOptions read = Read(
                                    "options:\n"
                                    "  reduced_effects: peut-etre\n"
                                    "  show_measures: true\n"
                                    "  horde_sun_shadows: [1, 2]\n"
                                    "  pause_while_typing: [1]\n"
                                    "  presence_shown: oui\n"
                                    "  first_skill_id: 70000\n"
                                    "  inconnue: 1\n")
                                    .options;
  CHECK_FALSE(read.reduced_effects);
  CHECK(read.show_measures);
  CHECK_FALSE(read.horde_sun_shadows);
  // La pause pendant la saisie est mise par défaut.
  CHECK(read.pause_while_typing);
  // La partie se montre aux autres par défaut.
  CHECK(read.presence_shown);
  CHECK(read.objective_skill_id == 0);
  CHECK(Read("options:\n  objective_skill_id: -4\n").options.objective_skill_id == 0);
  SurvivorsSave set;
  set.options = AllSet();
  CHECK(Read(WriteSurvivorsSave(set)).options == AllSet());
}

TEST_CASE("Carnet survivors : une clé ou une section inconnue se recopie", "[app][survivors]") {
  const SurvivorsSave read = Read(
      "meta:\n"
      "  church: {spent: 50, ranks: {blessing: 1, lex_aeterna: 4}}\n"
      "  kafra: {ranks: {giant_fly_wing: 2}}\n"
      "album:\n"
      "  cards: {angeling_card: {found: 1, first_found: 2027-01-02}}\n"
      "  feats: {thunderstorm: true, sight: false}\n"
      "pending_results:\n"
      "  - {token: abc}\n"
      "arena_unlocks:\n"
      "  prt_fild09: true\n");
  CHECK(read.meta.ranks.Of(Blessing::kBlessing) == 1);
  CHECK(read.meta.foreign_church_ranks ==
        std::vector<SurvivorsForeignEntry>{{.key = "lex_aeterna", .yaml = "4"}});
  CHECK(read.meta.foreign_kafra_ranks ==
        std::vector<SurvivorsForeignEntry>{{.key = "giant_fly_wing", .yaml = "2"}});
  REQUIRE(read.album.cards.size() == 1);
  CHECK(read.album.cards.front() ==
        SurvivorsAlbumCard{.key = "angeling_card", .found = 1, .first_found_day = Day(2027, 1, 2)});
  CHECK(read.album.feats == std::vector<std::string>{"thunderstorm"});
  REQUIRE(read.foreign_pending.size() == 1);
  REQUIRE(read.foreign_sections.size() == 1);
  CHECK(read.foreign_sections.front().key == "arena_unlocks");

  const std::string text = WriteSurvivorsSave(read);
  CHECK(text.find("lex_aeterna: 4") != std::string::npos);
  CHECK(text.find("giant_fly_wing: 2") != std::string::npos);
  CHECK(text.find("arena_unlocks:") != std::string::npos);
  CHECK(text.find("prt_fild09: true") != std::string::npos);
  CHECK(Read(text) == read);
}

TEST_CASE("Carnet survivors : un rang hors bornes se ramène, un solde négatif se lit zéro",
          "[app][survivors]") {
  const SurvivorsSave read = Read(
      "meta:\n"
      "  zeny: -300\n"
      "  zeny_earned: beaucoup\n"
      "  church: {spent: -1, ranks: {blessing: 9, resurrection: 4, gloria: -2}}\n"
      "  kafra: {ranks: {magnifier: 99}}\n"
      "  cursed_water: 9\n"
      "records:\n"
      "  - {class: mage, arena: prt_fild08, board: free,\n"
      "     runs: [{score: 10, outcome: perdu, date: hier, week: 2026-W60}]}\n");
  const ro::sim::SurvivorsMeta& meta = ro::sim::ShippedSurvivorsMeta();
  CHECK(read.meta.zeny == 0);
  CHECK(read.meta.zeny_earned == 0);
  CHECK(read.meta.ranks.church_spent == 0);
  CHECK(read.meta.ranks.Of(Blessing::kBlessing) == ro::sim::MostRank(meta, Blessing::kBlessing));
  CHECK(read.meta.ranks.Of(Blessing::kResurrection) ==
        ro::sim::MostRank(meta, Blessing::kResurrection));
  CHECK(read.meta.ranks.Of(Blessing::kGloria) == 0);
  CHECK(read.meta.ranks.Of(KafraService::kMagnifier) ==
        ro::sim::MostRank(meta, KafraService::kMagnifier));
  CHECK(read.meta.cursed_water == ro::sim::kMostCursedWater);
  REQUIRE(read.records.size() == 1);
  REQUIRE(read.records.front().runs.size() == 1);
  const ro::sim::SurvivorsRecord& run = read.records.front().runs.front();
  CHECK(run.score == 10);
  CHECK(run.level == 1);
  CHECK(run.outcome == ro::sim::SurvivorsOutcome::kRunning);
  CHECK(run.played_at_utc_s == 0);
  CHECK(run.week == ro::sim::IsoWeek{});
}

TEST_CASE("Carnet survivors : un tableau corrigé à la main se relit rangé", "[app][survivors]") {
  std::string text =
      "records:\n  - class: mage\n    arena: prt_fild08\n    board: free\n    runs:\n";
  constexpr int kHandRuns = 12;
  for (int run = 1; run <= kHandRuns; ++run) {
    text += "      - {score: " + std::to_string(run) + "}\n";
  }
  const SurvivorsSave read = Read(text);
  REQUIRE(read.records.size() == 1);
  const std::vector<ro::sim::SurvivorsRecord>& runs = read.records.front().runs;
  REQUIRE(runs.size() == ro::sim::kRecordsPerBoard);
  CHECK(runs.front().score == kHandRuns);
  CHECK(runs.back().score == kHandRuns - ro::sim::kRecordsPerBoard + 1);
}

TEST_CASE("Carnet survivors : deux écrivains successifs gardent chacun leur delta",
          "[app][survivors]") {
  constexpr std::uint64_t kCredit = 5'000;
  // Le premier client crédite une partie ; le second, ouvert avant, achète
  // ensuite : son achat se juge sur le solde RELU.
  const auto credit = [](SurvivorsSave& save) {
    save.meta.zeny += kCredit;
    save.meta.zeny_earned += kCredit;
    return true;
  };
  std::optional<ro::sim::MetaRefusal> refused;
  const auto buy = [&refused](SurvivorsSave& save) {
    const ro::sim::MetaPurchase bought = ro::sim::BuyBlessing(
        ro::sim::ShippedSurvivorsMeta(), save.meta.ranks, save.meta.zeny, Blessing::kBlessing);
    if (!bought) refused = bought.error();
    return bought.has_value();
  };
  const std::string opened = WriteSurvivorsSave(SurvivorsSave{});
  // Sur le texte de l'ouverture, l'achat est refusé et rien ne s'écrit.
  const ro::core::Result<SurvivorsSaveApplied> stale = ApplySurvivorsSaveDelta(opened, buy);
  REQUIRE(stale.has_value());
  CHECK_FALSE(stale->text.has_value());
  CHECK(refused == ro::sim::MetaRefusal::kNotEnoughZeny);

  const ro::core::Result<SurvivorsSaveApplied> first = ApplySurvivorsSaveDelta(opened, credit);
  REQUIRE(first.has_value());
  REQUIRE(first->text.has_value());
  const ro::core::Result<SurvivorsSaveApplied> second = ApplySurvivorsSaveDelta(*first->text, buy);
  REQUIRE(second.has_value());
  REQUIRE(second->text.has_value());
  const SurvivorsSave both = Read(*second->text);
  CHECK(both == second->save);
  CHECK(both.meta.zeny_earned == kCredit);
  CHECK(both.meta.ranks.Of(Blessing::kBlessing) == 1);
  CHECK(both.meta.zeny == kCredit - both.meta.ranks.church_spent);
  CHECK_FALSE(ApplySurvivorsSaveDelta("{{{", credit).has_value());

  // Le même scénario sur le disque, par deux mutations successives.
  const TempDirectory root;
  const SurvivorsSaveFile file;
  refused.reset();
  const ro::core::Result<SurvivorsSaveRead> credited = file.Mutate(root.path(), credit);
  REQUIRE(credited.has_value());
  CHECK(credited->save.meta.zeny == kCredit);
  const ro::core::Result<SurvivorsSaveRead> bought = file.Mutate(root.path(), buy);
  REQUIRE(bought.has_value());
  CHECK_FALSE(refused.has_value());
  const SurvivorsSave on_disk = file.Load(root.path()).save;
  CHECK(on_disk == bought->save);
  CHECK(on_disk.meta.zeny_earned == kCredit);
  CHECK(on_disk.meta.ranks.Of(Blessing::kBlessing) == 1);
  CHECK(on_disk.meta.zeny == kCredit - on_disk.meta.ranks.church_spent);
}

TEST_CASE("Carnet survivors : sans fichier les défauts, puis l'écriture se relit",
          "[app][survivors]") {
  const TempDirectory root;
  CHECK(LoadSurvivorsOptions(root.path()) == SurvivorsOptions{});
  REQUIRE(SaveSurvivorsOptions(root.path(), SurvivorsOptions{}, AllSet()).has_value());
  CHECK(std::filesystem::exists(root.path() / "savedata" / "survivors.yml"));
  CHECK(LoadSurvivorsOptions(root.path()) == AllSet());
}

TEST_CASE("Carnet survivors : un réglage écrit ne pose que le champ changé", "[app][survivors]") {
  const TempDirectory root;
  // Le mode a gardé les défauts (fichier tenu à l'ouverture, ou autre client
  // qui a écrit depuis) ; le fichier porte deux réglages vrais.
  root.WriteText("savedata/survivors.yml",
                 "options:\n  show_measures: true\n  reduced_effects: true\n");
  const SurvivorsOptions before;
  SurvivorsOptions after = before;
  after.objective_skill_id = kColdBolt;
  REQUIRE(SaveSurvivorsOptions(root.path(), before, after).has_value());
  const SurvivorsOptions reread = LoadSurvivorsOptions(root.path());
  CHECK(reread.objective_skill_id == kColdBolt);
  // Un carnet écrit avant que la classe ne se retienne n'en retient aucune.
  CHECK(reread.class_id == 0);
  CHECK(reread.show_measures);
  CHECK(reread.reduced_effects);
  CHECK_FALSE(reread.horde_sun_shadows);

  // Un réglage remis à faux se pose aussi.
  SurvivorsOptions cleared = reread;
  cleared.show_measures = false;
  REQUIRE(SaveSurvivorsOptions(root.path(), reread, cleared).has_value());
  CHECK(LoadSurvivorsOptions(root.path()) == cleared);
}

TEST_CASE("Carnet survivors : l'identifiant de skill suit la conversion entière de yaml-cpp",
          "[app][survivors]") {
  CHECK(Read("options:\n  objective_skill_id: 0x0E\n").options.objective_skill_id == kColdBolt);
  CHECK(Read("options:\n  objective_skill_id: +14\n").options.objective_skill_id == kColdBolt);
  CHECK(Read("options:\n  objective_skill_id: 14\n").options.objective_skill_id == kColdBolt);
}

TEST_CASE("Carnet survivors : les réglages écrits laissent les autres sections",
          "[app][survivors]") {
  const TempDirectory root;
  root.WriteText("savedata/survivors.yml",
                 "meta:\n  zeny: 1200\noptions:\n  show_measures: false\nfuture: {x: 1}\n");
  REQUIRE(SaveSurvivorsOptions(root.path(), SurvivorsOptions{}, AllSet()).has_value());
  const SurvivorsSaveRead reread = SurvivorsSaveFile().Load(root.path());
  CHECK(reread.save.options == AllSet());
  CHECK(reread.save.meta.zeny == 1200);
  REQUIRE(reread.save.foreign_sections.size() == 1);
  CHECK(reread.save.foreign_sections.front().key == "future");
}

TEST_CASE("Carnet survivors : un fichier abîmé est copié de côté, puis remplacé",
          "[app][survivors]") {
  CHECK(DamagedCopyName(FixedNow()) == "survivors.yml.damaged-20261030-140322");
  const TempDirectory root;
  const std::filesystem::path path = root.path() / "savedata" / "survivors.yml";
  const std::string broken = "meta: {zeny: 1200\n  cassé: [\n";
  root.WriteText("savedata/survivors.yml", broken);
  const SurvivorsSaveFile file(FixedNow);

  const SurvivorsSaveRead first = file.Load(root.path());
  CHECK_FALSE(first.file_unread);
  REQUIRE(first.damaged_copy.has_value());
  CHECK(*first.damaged_copy == root.path() / "savedata" / "survivors.yml.damaged-20261030-140322");
  CHECK(FileText(*first.damaged_copy) == broken);
  CHECK(first.save == SurvivorsSave{});
  // Remplacé par un carnet vierge : la lecture suivante ne dit plus rien.
  const SurvivorsSaveRead second = file.Load(root.path());
  CHECK_FALSE(second.damaged_copy.has_value());
  CHECK(second.save == SurvivorsSave{});

  // Un second abîmé dans la même seconde ne remplace pas la première copie.
  root.WriteText("savedata/survivors.yml", "- une\n- liste\n");
  const ro::core::Result<SurvivorsSaveRead> mutated =
      file.Mutate(root.path(), [](SurvivorsSave& save) {
        save.meta.zeny = 77;
        return true;
      });
  REQUIRE(mutated.has_value());
  REQUIRE(mutated->damaged_copy.has_value());
  CHECK(mutated->damaged_copy->filename() == "survivors.yml.damaged-20261030-140322-2");
  CHECK(FileText(*first.damaged_copy) == broken);
  CHECK(file.Load(root.path()).save.meta.zeny == 77);
}

TEST_CASE("Carnet survivors : un abîmé dont la copie échoue reste intact", "[app][survivors]") {
  const TempDirectory root;
  const std::filesystem::path path = root.path() / "savedata" / "survivors.yml";
  const std::string broken = "- une\n- liste\n";
  root.WriteText("savedata/survivors.yml", broken);
  // Les cent noms de copie de cette seconde sont pris.
  constexpr int kMostCopies = 100;
  const std::string name = DamagedCopyName(FixedNow());
  for (int attempt = 1; attempt <= kMostCopies; ++attempt) {
    root.WriteText(std::filesystem::path("savedata") /
                       (attempt == 1 ? name : name + "-" + std::to_string(attempt)),
                   "occupé");
  }
  const SurvivorsSaveFile file(FixedNow);
  const SurvivorsSaveRead loaded = file.Load(root.path());
  CHECK(loaded.file_unread);
  CHECK_FALSE(loaded.damaged_copy.has_value());
  CHECK(FileText(path) == broken);
  const ro::core::Result<SurvivorsSaveRead> mutated =
      file.Mutate(root.path(), [](SurvivorsSave& save) {
        save.meta.zeny = 1;
        return true;
      });
  REQUIRE_FALSE(mutated.has_value());
  CHECK(mutated.error().code == ro::core::ErrorCode::kIoFailure);
  CHECK(FileText(path) == broken);
}

#ifdef _WIN32
// Sous Linux, un dossier s'ouvre en lecture et se lit vide : l'essai n'y
// fabriquerait pas un fichier qui ne s'ouvre pas.
TEST_CASE("Carnet survivors : un fichier qui ne s'ouvre pas ne s'écrase jamais",
          "[app][survivors]") {
  const TempDirectory root;
  const std::filesystem::path path = root.path() / "savedata" / "survivors.yml";
  std::filesystem::create_directories(path);
  const SurvivorsSaveFile file;
  const SurvivorsSaveRead loaded = file.Load(root.path());
  CHECK(loaded.file_unread);
  CHECK(loaded.save == SurvivorsSave{});
  const ro::core::Result<SurvivorsSaveRead> mutated =
      file.Mutate(root.path(), [](SurvivorsSave& save) {
        save.meta.zeny = 1;
        return true;
      });
  REQUIRE_FALSE(mutated.has_value());
  CHECK(mutated.error().code == ro::core::ErrorCode::kIoFailure);
  CHECK(std::filesystem::is_directory(path));
  const ro::core::Result<moonclient::SurvivorsSaveRead> saved =
      SaveSurvivorsOptions(root.path(), SurvivorsOptions{}, AllSet());
  REQUIRE_FALSE(saved.has_value());
  CHECK(saved.error().code == ro::core::ErrorCode::kIoFailure);
}
#endif

TEST_CASE("Carnet survivors : une section connue d'une autre forme se recopie",
          "[app][survivors]") {
  const SurvivorsSave read = Read(
      "records: {mage: 1}\n"
      "pending_results: {queue: [1, 2], last_sent: 7}\n"
      "album:\n"
      "  cards: {hornet_card: {found: 1}, golem_card: 3}\n"
      "  feats: {sight: true, wind: {at: 2027-01-01}}\n"
      "meta:\n"
      "  church: {ranks: {lex_aeterna: {rank: 2, since: 2027-01-01}}}\n");
  CHECK(read.pending_results.empty());
  CHECK(read.foreign_pending.empty());
  CHECK(read.records.empty());
  REQUIRE(read.album.cards.size() == 1);
  CHECK(read.album.foreign_cards ==
        std::vector<SurvivorsForeignEntry>{{.key = "golem_card", .yaml = "3"}});
  CHECK(read.album.feats == std::vector<std::string>{"sight"});
  REQUIRE(read.album.foreign_feats.size() == 1);
  REQUIRE(read.meta.foreign_church_ranks.size() == 1);

  const SurvivorsSave reread = Read(WriteSurvivorsSave(read));
  CHECK(reread == read);
  const std::string text = WriteSurvivorsSave(reread);
  CHECK(text.find("last_sent: 7") != std::string::npos);
  CHECK(text.find("pending_results: []") == std::string::npos);
  CHECK(text.find("since: 2027-01-01") != std::string::npos);
  CHECK(text.find("golem_card: 3") != std::string::npos);

  // Une fois que ce client y écrit, sa section remplace l'autre forme.
  SurvivorsSave written = read;
  written.foreign_pending = {"{token: abc}"};
  written.album.cards.push_back({.key = "golem_card", .found = 1, .first_found_day = 0});
  const SurvivorsSave mine = Read(WriteSurvivorsSave(written));
  REQUIRE(mine.foreign_pending.size() == 1);
  CHECK(mine.album.foreign_cards.empty());
  CHECK(mine.album.cards.size() == 2);
}

TEST_CASE("Carnet survivors : le défi a un tableau par semaine", "[app][survivors]") {
  const SurvivorsSave save = Read(
      "records:\n"
      "  - {class: mage, arena: prt_fild08, board: challenge, week: 2026-W43,\n"
      "     runs: [{score: 10}]}\n"
      "  - {class: mage, arena: prt_fild08, board: challenge, week: 2026-W44,\n"
      "     runs: [{score: 20}]}\n"
      "  - {class: mage, arena: prt_fild08, board: challenge, week: 2026-W43,\n"
      "     runs: [{score: 30}]}\n");
  REQUIRE(save.records.size() == 2);
  CHECK(save.records[0].week == ro::sim::IsoWeek{.year = 2026, .week = 43});
  CHECK(save.records[0].runs.size() == 2);
  CHECK(save.records[1].week == ro::sim::IsoWeek{.year = 2026, .week = 44});
  CHECK(Read(WriteSurvivorsSave(save)) == save);
}

TEST_CASE("Carnet survivors : une entrée en attente fait l'aller-retour", "[app][survivors]") {
  const SurvivorsPendingResult entry = RankedPending();
  const std::string text = moonclient::WriteSurvivorsPendingResult(entry);
  CHECK(moonclient::ReadSurvivorsPendingResult(text) == entry);
  CHECK(text.find("issued_at: 2026-10-26T") != std::string::npos);
  CHECK(text.find("outcome: victory") != std::string::npos);

  SurvivorsSave save;
  save.pending_results = {entry};
  const SurvivorsSave read = Read(WriteSurvivorsSave(save));
  CHECK(read == save);
  CHECK(read.foreign_pending.empty());

  // Jamais envoyée : les champs facultatifs gardent leur défaut.
  SurvivorsPendingResult fresh = entry;
  fresh.character_name.clear();
  fresh.send_count = 0;
  fresh.last_sent_utc_s = 0;
  const std::string fresh_text = moonclient::WriteSurvivorsPendingResult(fresh);
  CHECK(fresh_text.find("last_sent") == std::string::npos);
  CHECK(moonclient::ReadSurvivorsPendingResult(fresh_text) == fresh);
}

TEST_CASE("Carnet survivors : une entrée d'une autre forme ou hors bornes se recopie",
          "[app][survivors]") {
  const std::string mine = moonclient::WriteSurvivorsPendingResult(RankedPending());
  SurvivorsPendingResult zero_seed = RankedPending();
  zero_seed.seed = 0;
  SurvivorsPendingResult free_board = RankedPending();
  free_board.board_key = "free";
  SurvivorsPendingResult abandoned = RankedPending();
  abandoned.components.outcome = ro::sim::SurvivorsOutcome::kAbandoned;
  const SurvivorsSave read =
      Read("pending_results:\n  - {token: abc, score: 12}\n  - " + mine + "\n  - " +
           moonclient::WriteSurvivorsPendingResult(zero_seed) + "\n  - " +
           moonclient::WriteSurvivorsPendingResult(free_board) + "\n  - " +
           moonclient::WriteSurvivorsPendingResult(abandoned) + "\n  - {server: a, ticket: 1}\n");
  REQUIRE(read.pending_results.size() == 1);
  CHECK(read.pending_results.front() == RankedPending());
  CHECK(read.foreign_pending.size() == 5);
  CHECK_FALSE(moonclient::IsSendablePendingResult(zero_seed));
  CHECK_FALSE(moonclient::IsSendablePendingResult(free_board));
  CHECK_FALSE(moonclient::IsSendablePendingResult(abandoned));

  const std::string text = WriteSurvivorsSave(read);
  CHECK(text.find("token: abc") != std::string::npos);
  CHECK(Read(text) == read);
  // Une mutation n'ajoute pas une entrée qui ne peut pas partir.
  const auto refused = ApplySurvivorsSaveDelta(text, moonclient::AddPendingResult(zero_seed));
  REQUIRE(refused.has_value());
  CHECK_FALSE(refused->text.has_value());
}

TEST_CASE("Carnet survivors : deux ajouts successifs sur le même texte gardent les deux",
          "[app][survivors]") {
  SurvivorsPendingResult second = RankedPending();
  second.ticket += 1;
  second.board_key = "challenge";
  const auto first = ApplySurvivorsSaveDelta("", moonclient::AddPendingResult(RankedPending()));
  REQUIRE(first.has_value());
  REQUIRE(first->text.has_value());
  const auto both = ApplySurvivorsSaveDelta(*first->text, moonclient::AddPendingResult(second));
  REQUIRE(both.has_value());
  REQUIRE(both->text.has_value());
  const SurvivorsSave read = Read(*both->text);
  CHECK(read.pending_results == std::vector<SurvivorsPendingResult>{RankedPending(), second});

  // La même entrée redite ne change rien ; un envoi de plus la remplace.
  const auto again = ApplySurvivorsSaveDelta(*both->text, moonclient::AddPendingResult(second));
  REQUIRE(again.has_value());
  CHECK_FALSE(again->text.has_value());
  SurvivorsPendingResult resent = second;
  resent.send_count += 1;
  const auto replaced = ApplySurvivorsSaveDelta(*both->text, moonclient::AddPendingResult(resent));
  REQUIRE(replaced.has_value());
  REQUIRE(replaced->text.has_value());
  CHECK(Read(*replaced->text).pending_results ==
        std::vector<SurvivorsPendingResult>{RankedPending(), resent});
}

TEST_CASE("Carnet survivors : le retrait par jeton laisse l'autre personnage", "[app][survivors]") {
  SurvivorsPendingResult other_character = RankedPending();
  other_character.character_id += 1;
  other_character.character_name = "Brune";
  SurvivorsSave save;
  save.pending_results = {RankedPending(), other_character};
  save.foreign_pending = {"{token: abc}"};
  const std::string text = WriteSurvivorsSave(save);

  const SurvivorsPendingResult& gone = save.pending_results.front();
  const auto removed = ApplySurvivorsSaveDelta(
      text, moonclient::RemovePendingResult(gone.server_key, gone.character_id, gone.ticket));
  REQUIRE(removed.has_value());
  REQUIRE(removed->text.has_value());
  const SurvivorsSave read = Read(*removed->text);
  CHECK(read.pending_results == std::vector<SurvivorsPendingResult>{other_character});
  CHECK(read.foreign_pending == save.foreign_pending);

  // Un autre serveur, ou un jeton inconnu : rien ne change.
  const auto elsewhere = ApplySurvivorsSaveDelta(
      text, moonclient::RemovePendingResult("10.0.0.1:5121", gone.character_id, gone.ticket));
  REQUIRE(elsewhere.has_value());
  CHECK_FALSE(elsewhere->text.has_value());
  const auto unknown = ApplySurvivorsSaveDelta(
      text, moonclient::RemovePendingResult(gone.server_key, gone.character_id, gone.ticket + 1));
  REQUIRE(unknown.has_value());
  CHECK_FALSE(unknown->text.has_value());
}

TEST_CASE("Carnet survivors : les mesures rangées ne se montrent qu'au client de développement",
          "[app][survivors]") {
  const SurvivorsOptions read = Read("options:\n  show_measures: true\n").options;
  REQUIRE(read.show_measures);
  CHECK_FALSE(moonclient::MeasuresShown(read, /*dev_tools=*/false));
  CHECK(moonclient::MeasuresShown(read, /*dev_tools=*/true));
  CHECK_FALSE(moonclient::MeasuresShown(SurvivorsOptions{}, /*dev_tools=*/true));
}
