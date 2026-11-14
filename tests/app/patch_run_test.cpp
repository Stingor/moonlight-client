// La marche d'une chaîne de patchs : lister, télécharger, vérifier, appliquer,
// retenir — sur un réseau factice et une installation temporaire.

#include "patcher/patch_run.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <stop_token>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/sha256.h"
#include "ro/formats/grf.h"

#include "fake_patch_fetcher.h"
#include "patcher/install_path.h"
#include "patcher/patcher_state.h"
#include "support/grf_builder.h"
#include "support/temp_directory.h"
#include "support/thor_builder.h"

#if defined(_WIN32)
#include <array>

#include <windows.h>
#endif

using moonclient::patcher::PatchPhase;
using moonclient::patcher::PatchProgressBoard;
using moonclient::patcher::PatchRunSettings;
using moonclient::patcher::RunPatches;
using ro::test_support::Bytes;
using ro::test_support::TempDirectory;

namespace {

constexpr const char* kListUrl = "https://patchs.example/plist.txt";
constexpr const char* kArchiveUrl = "https://patchs.example/data/";

using FakeFetcher = moonclient::test_support::FakePatchFetcher;

[[nodiscard]] std::string AsText(const std::vector<std::uint8_t>& bytes) {
  return {bytes.begin(), bytes.end()};
}

[[nodiscard]] std::string Sha256Of(const std::string& bytes) {
  return ro::core::Sha256::ToHex(ro::core::Sha256::Hash(
      std::span(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size())));
}

[[nodiscard]] std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream all;
  all << file.rdbuf();
  return all.str();
}

[[nodiscard]] PatchRunSettings SettingsFor(const TempDirectory& install) {
  PatchRunSettings settings;
  settings.config.mirrors.push_back(
      {.name = "essai", .list_url = kListUrl, .archive_url = kArchiveUrl});
  settings.config.default_grf = "moonlight.grf";
  settings.install_root = install.path();
  return settings;
}

// Une installation avec sa GRF, et un réseau qui sert deux patchs : l'un
// enrichit la GRF, l'autre écrit et retire des fichiers du dossier du jeu.
struct Chain {
  TempDirectory install;
  FakeFetcher fetcher;
  std::string grf_patch;
  std::string game_patch;

  Chain() {
    install.WriteFile("moonlight.grf", ro::test_support::BuildGrf({{"data\\a.txt", Bytes("a")}}));
    install.WriteText("ancien.txt", "à retirer");
    grf_patch = AsText(ro::test_support::BuildThor({{"data\\b.txt", Bytes("b")}}));
    ro::test_support::ThorBuildOptions to_game;
    to_game.merge_into_grf = false;
    game_patch = AsText(ro::test_support::BuildThor(
        {{"SystemEN\\table.lua", Bytes("return {}")}, {"ancien.txt", {}, true}}, to_game));
    fetcher.served[std::string(kArchiveUrl) + "0-grf.thor"] = grf_patch;
    fetcher.served[std::string(kArchiveUrl) + "1-sys.thor"] = game_patch;
  }

  void Publish(const std::string& list) { fetcher.served[kListUrl] = list; }
};

}  // namespace

TEST_CASE("Patcher : une chaîne entière s'applique, et l'état retient le dernier index",
          "[app][patcher][run]") {
  Chain chain;
  chain.Publish("0 0-grf.thor " + Sha256Of(chain.grf_patch) + "\n1 1-sys.thor\n");
  PatchProgressBoard board;
  const auto outcome =
      RunPatches(SettingsFor(chain.install), chain.fetcher, board, std::stop_token());
  REQUIRE(outcome.has_value());
  CHECK(outcome->applied_patches == 2);
  CHECK_FALSE(outcome->restart_needed);
  CHECK(board.Snapshot().phase == PatchPhase::kDone);

  auto grf = ro::formats::GrfArchive::OpenFile(chain.install.path() / "moonlight.grf");
  REQUIRE(grf.has_value());
  CHECK(AsText(grf->Read("data\\a.txt").value()) == "a");
  CHECK(AsText(grf->Read("data\\b.txt").value()) == "b");
  CHECK(ReadFile(chain.install.path() / "SystemEN" / "table.lua") == "return {}");
  CHECK_FALSE(std::filesystem::exists(chain.install.path() / "ancien.txt"));
  CHECK(ReadFile(chain.install.path() / moonclient::patcher::kPatcherStateFileName) ==
        moonclient::patcher::FormatPatcherState(1));
  // Les archives téléchargées ne restent pas.
  CHECK(std::filesystem::is_empty(chain.install.path() / moonclient::patcher::kDownloadFolderName));
}

TEST_CASE("Patcher : une installation à jour ne télécharge que ce qui suit",
          "[app][patcher][run]") {
  Chain chain;
  chain.Publish("0 0-grf.thor\n1 1-sys.thor\n");
  chain.install.WriteText(std::string(moonclient::patcher::kPatcherStateFileName),
                          moonclient::patcher::FormatPatcherState(0));
  PatchProgressBoard board;
  const auto outcome =
      RunPatches(SettingsFor(chain.install), chain.fetcher, board, std::stop_token());
  REQUIRE(outcome.has_value());
  CHECK(outcome->applied_patches == 1);
  CHECK(std::ranges::count(chain.fetcher.requested, std::string(kArchiveUrl) + "0-grf.thor") == 0);
}

TEST_CASE("Patcher : une empreinte fausse arrête tout, sans rien écrire ni retenir",
          "[app][patcher][run]") {
  Chain chain;
  chain.Publish("0 0-grf.thor " + std::string(64, 'a') + "\n1 1-sys.thor\n");
  const std::string grf_before = ReadFile(chain.install.path() / "moonlight.grf");
  PatchProgressBoard board;
  const auto outcome =
      RunPatches(SettingsFor(chain.install), chain.fetcher, board, std::stop_token());
  REQUIRE_FALSE(outcome.has_value());
  CHECK(outcome.error().detail.find("empreinte") != std::string::npos);
  CHECK(board.Snapshot().phase == PatchPhase::kFailed);
  CHECK(ReadFile(chain.install.path() / "moonlight.grf") == grf_before);
  CHECK_FALSE(
      std::filesystem::exists(chain.install.path() / moonclient::patcher::kPatcherStateFileName));
}

TEST_CASE("Patcher : exiger les empreintes refuse un patch qui n'en a pas", "[app][patcher][run]") {
  Chain chain;
  chain.Publish("0 0-grf.thor\n");
  PatchRunSettings settings = SettingsFor(chain.install);
  settings.config.require_hashes = true;
  PatchProgressBoard board;
  const auto outcome = RunPatches(settings, chain.fetcher, board, std::stop_token());
  REQUIRE_FALSE(outcome.has_value());
  CHECK(chain.fetcher.requested.size() == 1);  // la liste, et rien d'autre
}

TEST_CASE("Patcher : le serveur suivant répond quand le premier se tait", "[app][patcher][run]") {
  Chain chain;
  chain.Publish("0 0-grf.thor\n");
  PatchRunSettings settings = SettingsFor(chain.install);
  settings.config.mirrors.insert(
      settings.config.mirrors.begin(),
      {"muet", "https://muet.example/plist.txt", "https://muet.example/"});
  PatchProgressBoard board;
  const auto outcome = RunPatches(settings, chain.fetcher, board, std::stop_token());
  REQUIRE(outcome.has_value());
  CHECK(outcome->applied_patches == 1);
}

TEST_CASE("Patcher : une GRF absente n'est pas recréée, sauf si on le demande",
          "[app][patcher][run]") {
  Chain chain;
  chain.Publish("0 0-grf.thor\n");
  std::filesystem::remove(chain.install.path() / "moonlight.grf");
  PatchProgressBoard board;
  PatchRunSettings settings = SettingsFor(chain.install);
  CHECK_FALSE(RunPatches(settings, chain.fetcher, board, std::stop_token()).has_value());

  settings.config.create_grf = true;
  REQUIRE(RunPatches(settings, chain.fetcher, board, std::stop_token()).has_value());
  auto grf = ro::formats::GrfArchive::OpenFile(chain.install.path() / "moonlight.grf");
  REQUIRE(grf.has_value());
  CHECK(grf->entry_count() == 1);
}

TEST_CASE("Patcher : l'exécutable en cours s'écarte, et deux patchs de suite le réécrivent",
          "[app][patcher][run]") {
  TempDirectory install;
  install.WriteText("Jeu.exe", "ancien binaire");
  ro::test_support::ThorBuildOptions to_game;
  to_game.merge_into_grf = false;
  FakeFetcher fetcher;
  fetcher.served[kListUrl] = "0 a.thor\n1 b.thor\n";
  fetcher.served[std::string(kArchiveUrl) + "a.thor"] =
      AsText(ro::test_support::BuildThor({{"Jeu.exe", Bytes("binaire 1")}}, to_game));
  fetcher.served[std::string(kArchiveUrl) + "b.thor"] =
      AsText(ro::test_support::BuildThor({{"Jeu.exe", Bytes("binaire 2")}}, to_game));

  PatchRunSettings settings = SettingsFor(install);
  settings.running_executable = install.path() / "Jeu.exe";
  PatchProgressBoard board;
  const auto outcome = RunPatches(settings, fetcher, board, std::stop_token());
  REQUIRE(outcome.has_value());
  CHECK(outcome->restart_needed);
  CHECK(ReadFile(install.path() / "Jeu.exe") == "binaire 2");
  // Celui qui tournait est à l'écart, intact, jusqu'au prochain lancement.
  CHECK(ReadFile(install.path() / "Jeu.exe.old") == "ancien binaire");

  moonclient::patcher::RemoveReplacedExecutable(settings.running_executable);
  CHECK_FALSE(std::filesystem::exists(install.path() / "Jeu.exe.old"));
}

TEST_CASE("Patcher : le journal n'efface que les fichiers écartés sous l'installation",
          "[app][patcher][run]") {
  TempDirectory install;
  install.WriteText("Lib.dll", "ancienne");
  install.WriteText("garder.txt", "à garder");
  const auto aside =
      moonclient::patcher::SetInUseFileAside(install.path(), install.path() / "Lib.dll");
  REQUIRE(aside.has_value());
  CHECK(ReadFile(*aside) == "ancienne");
  CHECK_FALSE(std::filesystem::exists(install.path() / "Lib.dll"));

  // Une ligne étrangère ne fait rien effacer, et le journal ne la garde pas.
  const std::filesystem::path journal =
      install.path() / moonclient::patcher::kSetAsideJournalFileName;
  {
    std::ofstream append(journal, std::ios::binary | std::ios::app);
    append << "garder.txt\n../hors.moonclient-old\n";
  }
  moonclient::patcher::RemoveSetAsideFiles(install.path());
  CHECK_FALSE(std::filesystem::exists(*aside));
  CHECK(std::filesystem::exists(install.path() / "garder.txt"));
  CHECK_FALSE(std::filesystem::exists(journal));
}

#if defined(_WIN32)
TEST_CASE("Patcher : rejouée sans état, la chaîne remplace une bibliothèque chargée",
          "[app][patcher][run]") {
  TempDirectory install;
  std::array<wchar_t, MAX_PATH> system_folder{};
  REQUIRE(GetSystemDirectoryW(system_folder.data(), static_cast<UINT>(system_folder.size())) > 0);
  const std::filesystem::path library = install.path() / "Lib.dll";
  std::filesystem::copy_file(std::filesystem::path(system_folder.data()) / "version.dll", library);
  HMODULE loaded = LoadLibraryExW(library.c_str(), nullptr, LOAD_LIBRARY_AS_IMAGE_RESOURCE);
  REQUIRE(loaded != nullptr);
  // Ce que l'essai suppose : une image chargée ne s'écrase pas.
  install.WriteText("autre.dll", "x");
  std::error_code overwrite_error;
  std::filesystem::rename(install.path() / "autre.dll", library, overwrite_error);
  REQUIRE(overwrite_error);

  ro::test_support::ThorBuildOptions to_game;
  to_game.merge_into_grf = false;
  FakeFetcher fetcher;
  fetcher.served[kListUrl] = "0 a.thor\n";
  fetcher.served[std::string(kArchiveUrl) + "a.thor"] =
      AsText(ro::test_support::BuildThor({{"Lib.dll", Bytes("nouvelle")}}, to_game));
  PatchProgressBoard board;
  const auto outcome = RunPatches(SettingsFor(install), fetcher, board, std::stop_token());
  REQUIRE(outcome.has_value());
  CHECK(outcome->restart_needed);
  CHECK(ReadFile(library) == "nouvelle");
  const std::filesystem::path aside =
      install.path() / ("Lib.dll" + std::string(moonclient::patcher::kInUseSetAsideSuffix));
  CHECK(std::filesystem::exists(aside));

  // Tant qu'elle est chargée, elle reste au journal ; relâchée, elle s'efface.
  moonclient::patcher::RemoveSetAsideFiles(install.path());
  CHECK(std::filesystem::exists(aside));
  FreeLibrary(loaded);
  moonclient::patcher::RemoveSetAsideFiles(install.path());
  CHECK_FALSE(std::filesystem::exists(aside));
}
#endif

TEST_CASE("Patcher : un arrêt demandé avant de commencer n'applique rien", "[app][patcher][run]") {
  Chain chain;
  chain.Publish("0 0-grf.thor\n");
  std::stop_source stop;
  stop.request_stop();
  PatchProgressBoard board;
  const auto outcome =
      RunPatches(SettingsFor(chain.install), chain.fetcher, board, stop.get_token());
  REQUIRE_FALSE(outcome.has_value());
  CHECK(board.Snapshot().phase == PatchPhase::kStopped);
}
