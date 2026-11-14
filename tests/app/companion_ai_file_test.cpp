// LE FICHIER DE L'IA DES COMPAGNONS : aller-retour, ligne écartée, fichier
// absent, version inconnue, fusion, et les champs de la fenêtre « Avancé… ».

#include "game/companion_ai_file.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_tactics.h"
#include "ro/ui/companion_tactic_window.h"

#include "game/companion_tactic_fields.h"

using moonclient::CompanionAiFileRead;
using moonclient::CompanionProfiles;
using ro::sim::CompanionTactic;
using ro::sim::TacticKey;
using ro::sim::TacticKeyKind;

namespace {

constexpr std::uint16_t kFilirJob = 6003;
constexpr std::uint16_t kMercenaryJob = 6017;
constexpr std::uint16_t kPoringJob = 1002;

}  // namespace

TEST_CASE("Fichier de l'IA des compagnons : l'aller-retour garde tout",
          "[app][companion_ai_file]") {
  CompanionProfiles profiles;
  CompanionTactic poring{.key = TacticKey{.kind = TacticKeyKind::kMonster, .job = kPoringJob},
                         .rank = ro::sim::TacticRank::kHigh,
                         .trigger = ro::sim::TacticTrigger::kIfAttacks,
                         .avoid = ro::sim::TacticAvoid::kAlways,
                         .avoid_threshold_cells = 4,
                         .avoid_step_cells = 6,
                         .skill_use = ro::sim::TacticSkillUse::kOnceAtLevel,
                         .skill_use_count = 3,
                         .opening_skill_id = 8019,
                         .harass = true,
                         .sp_reserve_points = 25,
                         .weight = 0.5F,
                         .share = ro::sim::TacticShare::kIfStill,
                         .chase = ro::sim::TacticChase::kNever};
  REQUIRE(profiles.At(kFilirJob, moonclient::kOwnProfileName).tactics.Set(poring));
  REQUIRE(profiles.At(kFilirJob, moonclient::kOwnProfileName)
              .tactics.Set(CompanionTactic{.key = TacticKey{.kind = TacticKeyKind::kChest},
                                           .rank = ro::sim::TacticRank::kIgnore}));
  REQUIRE(profiles.At(kMercenaryJob, moonclient::kOwnProfileName)
              .tactics.Set(CompanionTactic{.key = TacticKey{.kind = TacticKeyKind::kStill},
                                           .rank = ro::sim::TacticRank::kLast}));
  REQUIRE(profiles.At(kMercenaryJob, moonclient::kOwnProfileName)
              .tactics.Set(CompanionTactic{.key = TacticKey{.kind = TacticKeyKind::kMinion},
                                           .rank = ro::sim::TacticRank::kHigh}));

  const CompanionAiFileRead read =
      moonclient::ParseCompanionAi(moonclient::FormatCompanionAi(profiles));
  CHECK_FALSE(read.unknown_version);
  CHECK_FALSE(read.unreadable);
  CHECK(read.rejected.empty());
  CHECK(read.profiles == profiles);
}

TEST_CASE("Fichier de l'IA des compagnons : une ligne hors du cadre est écartée",
          "[app][companion_ai_file]") {
  const CompanionAiFileRead read = moonclient::ParseCompanionAi(
      "version: 1\n"
      "types:\n"
      "  - class: 6003\n"
      "    profiles:\n"
      "      - name: My profile\n"
      "        tactics:\n"
      "          - monster: 1002\n"
      "            rank: high\n"
      "          - monster: 1003\n"
      "            rank: furious\n"
      "          - monster: 1004\n"
      "            avoid_step: 0\n"
      "          - generic: dragons\n");
  REQUIRE(read.profiles.Find(kFilirJob, moonclient::kOwnProfileName) != nullptr);
  const ro::sim::CompanionTactics& kept =
      read.profiles.Find(kFilirJob, moonclient::kOwnProfileName)->tactics;
  CHECK(kept.lines().size() == 1);
  CHECK(kept.Find(TacticKey{.kind = TacticKeyKind::kMonster, .job = kPoringJob})->rank ==
        ro::sim::TacticRank::kHigh);
  CHECK(read.rejected.size() == 3);
}

TEST_CASE("Fichier de l'IA des compagnons : un évitement au-delà du plafond y est ramené",
          "[app][companion_ai_file]") {
  const CompanionAiFileRead read = moonclient::ParseCompanionAi(
      "version: 1\n"
      "types:\n"
      "  - class: 6003\n"
      "    profiles:\n"
      "      - name: My profile\n"
      "        tactics:\n"
      "          - monster: 1002\n"
      "            avoid: always\n"
      "            avoid_threshold: 40\n"
      "            avoid_step: 19\n");
  REQUIRE(read.profiles.Find(kFilirJob, moonclient::kOwnProfileName) != nullptr);
  const CompanionTactic* poring =
      read.profiles.Find(kFilirJob, moonclient::kOwnProfileName)
          ->tactics.Find(TacticKey{.kind = TacticKeyKind::kMonster, .job = kPoringJob});
  REQUIRE(poring != nullptr);
  CHECK(poring->avoid_threshold_cells == ro::sim::kTacticAvoidMostCells);
  // EN DEÇÀ DU PLAFOND, le fichier garde la valeur du joueur, même au-delà du
  // rayon de vue du serveur : c'est l'IA qui la borne en jeu.
  CHECK(poring->avoid_step_cells == 19);
  CHECK(read.rejected.empty());
}

TEST_CASE("Fichier de l'IA des compagnons : version inconnue et fichier absent",
          "[app][companion_ai_file]") {
  const CompanionAiFileRead newer = moonclient::ParseCompanionAi("version: 99\ntypes: []\n");
  CHECK(newer.unknown_version);
  CHECK(newer.profiles.by_job.empty());

  const CompanionAiFileRead broken = moonclient::ParseCompanionAi("version: [1\n");
  CHECK(broken.unreadable);

  const std::filesystem::path nowhere =
      std::filesystem::temp_directory_path() / "moonclient_companion_ai_absent";
  const CompanionAiFileRead absent = moonclient::LoadCompanionAi(nowhere);
  CHECK_FALSE(absent.unreadable);
  CHECK_FALSE(absent.unknown_version);
  CHECK(absent.profiles.by_job.empty());
}

TEST_CASE("Fichier de l'IA des compagnons : la fusion ne touche que les lignes modifiées",
          "[app][companion_ai_file]") {
  // Un nœud inconnu à chaque étage, un autre profil, un autre type, et une
  // ligne écartée à la lecture : tout survit.
  const std::string current =
      "version: 1\n"
      "from_elsewhere: keep-me\n"
      "types:\n"
      "  - class: 6003\n"
      "    note: of the type\n"
      "    profiles:\n"
      "      - name: Hunting\n"
      "        tactics:\n"
      "          - monster: 1031\n"
      "            rank: low\n"
      "      - name: My profile\n"
      "        tactics:\n"
      "          - monster: 1002\n"
      "            rank: high\n"
      "          - monster: 1003\n"
      "            rank: furious\n"
      "          - monster: 1004\n"
      "            rank: low\n"
      "          - monster: 1005\n"
      "            rank: hold\n"
      "  - class: 6017\n"
      "    profiles:\n"
      "      - name: My profile\n"
      "        tactics:\n"
      "          - generic: still\n"
      "            rank: last\n";
  constexpr std::uint16_t kRemovedJob = 1004;
  constexpr std::uint16_t kAddedJob = 1006;
  constexpr std::uint16_t kHeldJob = 1005;
  moonclient::CompanionProfileEdits edits;
  edits.Set(kFilirJob, moonclient::kOwnProfileName,
            CompanionTactic{.key = TacticKey{.kind = TacticKeyKind::kMonster, .job = kPoringJob},
                            .rank = ro::sim::TacticRank::kIgnore});
  edits.Remove(kFilirJob, moonclient::kOwnProfileName,
               TacticKey{.kind = TacticKeyKind::kMonster, .job = kRemovedJob});
  edits.Set(kFilirJob, moonclient::kOwnProfileName,
            CompanionTactic{.key = TacticKey{.kind = TacticKeyKind::kMonster, .job = kAddedJob},
                            .rank = ro::sim::TacticRank::kAbsolute});

  const std::optional<std::string> merged = moonclient::MergeCompanionAi(current, edits);
  REQUIRE(merged.has_value());
  CHECK(merged->find("from_elsewhere: keep-me") != std::string::npos);
  CHECK(merged->find("note: of the type") != std::string::npos);
  CHECK(merged->find("name: Hunting") != std::string::npos);
  CHECK(merged->find("rank: furious") != std::string::npos);

  const CompanionAiFileRead read = moonclient::ParseCompanionAi(*merged);
  REQUIRE(read.profiles.Find(kFilirJob, moonclient::kOwnProfileName) != nullptr);
  const ro::sim::CompanionTactics& filir =
      read.profiles.Find(kFilirJob, moonclient::kOwnProfileName)->tactics;
  const auto rank_of = [&filir](std::uint16_t monster_job) {
    const CompanionTactic* line =
        filir.Find(TacticKey{.kind = TacticKeyKind::kMonster, .job = monster_job});
    return line != nullptr ? std::optional(line->rank) : std::nullopt;
  };
  CHECK(rank_of(kPoringJob) == ro::sim::TacticRank::kIgnore);
  CHECK_FALSE(rank_of(kRemovedJob).has_value());
  CHECK(rank_of(kAddedJob) == ro::sim::TacticRank::kAbsolute);
  // Intouchée.
  CHECK(rank_of(kHeldJob) == ro::sim::TacticRank::kHold);
  // La ligne modifiée reste à sa place : avant celle qui la suivait.
  CHECK(merged->find("monster: 1002") < merged->find("monster: 1005"));
  CHECK(merged->find("version: 1") != std::string::npos);
  CHECK(read.profiles.Find(kMercenaryJob, moonclient::kOwnProfileName)->tactics.lines().size() ==
        1);
  // La ligne écartée l'est toujours, et rien d'autre.
  CHECK(read.rejected.size() == 1);
}

TEST_CASE("Fichier de l'IA des compagnons : un fichier qu'on ne comprend pas ne se réécrit pas",
          "[app][companion_ai_file]") {
  moonclient::CompanionProfileEdits edits;
  edits.Set(kFilirJob, moonclient::kOwnProfileName,
            CompanionTactic{.key = TacticKey{.kind = TacticKeyKind::kMonster, .job = kPoringJob}});
  CHECK_FALSE(moonclient::MergeCompanionAi("version: 99\ntypes: []\n", edits).has_value());
  CHECK_FALSE(moonclient::MergeCompanionAi("version: [1\n", edits).has_value());
  CHECK_FALSE(moonclient::MergeCompanionAi("- une\n- liste\n", edits).has_value());
  // Une forme qu'on ne sait pas compléter : `types` qui n'est pas une liste.
  CHECK_FALSE(moonclient::MergeCompanionAi("version: 1\ntypes: 3\n", edits).has_value());
  // Un fichier vide se fusionne.
  CHECK(moonclient::MergeCompanionAi("", edits).has_value());

  // ET SUR LE DISQUE : le fichier illisible reste tel quel.
  const std::filesystem::path root =
      std::filesystem::temp_directory_path() / "moonclient_companion_ai_unreadable";
  const std::filesystem::path path = moonclient::CompanionAiFilePath(root);
  std::filesystem::create_directories(path.parent_path());
  const std::string broken = "version: [1\n";
  {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file << broken;
  }
  CHECK_FALSE(moonclient::SaveCompanionAiEdits(root, edits).has_value());
  std::ifstream file(path, std::ios::binary);
  std::ostringstream text;
  text << file.rdbuf();
  CHECK(text.str() == broken);
  file.close();
  std::error_code ignored;
  std::filesystem::remove_all(root, ignored);
}

TEST_CASE("Fichier de l'IA des compagnons : les champs de la fenêtre font l'aller-retour",
          "[app][companion_ai_file]") {
  const CompanionTactic line{.key = TacticKey{.kind = TacticKeyKind::kMonster, .job = kPoringJob},
                             .rank = ro::sim::TacticRank::kLow,
                             .trigger = ro::sim::TacticTrigger::kIfAttacksCompanion,
                             .avoid = ro::sim::TacticAvoid::kIfAttacks,
                             .avoid_threshold_cells = 2,
                             .avoid_step_cells = 7,
                             .skill_use = ro::sim::TacticSkillUse::kTimes,
                             .skill_use_count = 4,
                             .opening_skill_id = 8013,
                             .harass = true,
                             .sp_reserve_points = 30,
                             .weight = 2.5F,
                             .share = ro::sim::TacticShare::kAlways,
                             .chase = ro::sim::TacticChase::kAlways};
  CHECK(moonclient::WithFields(
            CompanionTactic{.key = line.key, .rank = line.rank, .trigger = line.trigger},
            moonclient::FieldsOf(line)) == line);

  // La réserve propre décochée : celle du réglage général.
  ro::ui::TacticAdvancedFields fields = moonclient::FieldsOf(line);
  fields.sp_reserve_points = -1;
  CHECK(moonclient::WithFields(line, fields).sp_reserve_points ==
        ro::sim::kTacticSpReserveFromBase);
  // La clé, le rang et le déclencheur ne sont pas des champs de la fenêtre.
  const CompanionTactic other = moonclient::WithFields(line, ro::ui::TacticAdvancedFields{});
  CHECK(other.key == line.key);
  CHECK(other.rank == line.rank);
  CHECK(other.trigger == line.trigger);
}
