// Le miroir de fichiers : ce que le serveur publie arrive dans le dossier du
// jeu, et seulement ce qui diffère — sur un réseau factice et une installation
// temporaire.

#include "patcher/mirror_sync.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stop_token>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/sha256.h"

#include "fake_patch_fetcher.h"
#include "patcher/mirror_cache.h"
#include "patcher/mirror_manifest.h"
#include "support/temp_directory.h"

using moonclient::patcher::MirrorSyncOutcome;
using moonclient::patcher::PatchProgress;
using moonclient::patcher::PatchProgressBoard;
using moonclient::patcher::PatchRunSettings;
using moonclient::patcher::RunPatches;
using moonclient::patcher::SyncMirror;
using ro::test_support::TempDirectory;

namespace {

constexpr const char* kListUrl = "https://patchs.example/plist.txt";
constexpr const char* kArchiveUrl = "https://patchs.example/data/";
constexpr const char* kManifestUrl = "https://patchs.example/manifest.php";
constexpr const char* kFilesUrl = "https://patchs.example/files/";

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

// Une installation, et un miroir dont on choisit ce qu'il publie.
struct Mirror {
  TempDirectory install;
  moonclient::test_support::FakePatchFetcher fetcher;
  std::string manifest = "moonclient-mirror 1\n";

  void Publish(const std::string& relative_path, const std::string& content) {
    manifest +=
        Sha256Of(content) + " " + std::to_string(content.size()) + " " + relative_path + "\n";
    fetcher.served[moonclient::patcher::MirrorFileUrl(kFilesUrl, relative_path)] = content;
  }

  [[nodiscard]] PatchRunSettings Settings() const {
    PatchRunSettings settings;
    settings.config.mirrors.push_back({.name = "essai",
                                       .list_url = kListUrl,
                                       .archive_url = kArchiveUrl,
                                       .manifest_url = kManifestUrl,
                                       .files_url = kFilesUrl});
    settings.config.default_grf = "moonlight.grf";
    settings.install_root = install.path();
    return settings;
  }

  [[nodiscard]] ro::core::Result<MirrorSyncOutcome> Sync(
      const std::filesystem::path& running_executable = {}) {
    fetcher.served[kManifestUrl] = manifest;
    fetcher.requested.clear();
    PatchProgressBoard board;
    PatchProgress progress;
    return SyncMirror(Settings(), running_executable, fetcher, board, progress, std::stop_token());
  }

  [[nodiscard]] bool Requested(const std::string& relative_path) const {
    return std::ranges::count(fetcher.requested,
                              moonclient::patcher::MirrorFileUrl(kFilesUrl, relative_path)) > 0;
  }
};

}  // namespace

TEST_CASE("Miroir : ce qui manque ou diffère se télécharge, ce qui est à jour ne bouge pas",
          "[app][patcher][mirror]") {
  Mirror mirror;
  mirror.install.WriteText("identique.txt", "pareil");
  mirror.install.WriteText("ancien.txt", "vieux");
  mirror.install.WriteText("autre taille.txt", "123");
  mirror.Publish("identique.txt", "pareil");
  mirror.Publish("ancien.txt", "neuf!");
  mirror.Publish("autre taille.txt", "1234");
  mirror.Publish("SystemEN/sous/nouveau.lua", "return {}");

  const auto outcome = mirror.Sync();
  REQUIRE(outcome.has_value());
  CHECK(outcome->written_files == 3);
  CHECK_FALSE(outcome->restart_needed);
  CHECK_FALSE(mirror.Requested("identique.txt"));
  CHECK(ReadFile(mirror.install.path() / "ancien.txt") == "neuf!");
  CHECK(ReadFile(mirror.install.path() / "autre taille.txt") == "1234");
  CHECK(ReadFile(mirror.install.path() / "SystemEN" / "sous" / "nouveau.lua") == "return {}");
  CHECK(std::filesystem::exists(mirror.install.path() / moonclient::patcher::kMirrorCacheFileName));

  // Une seconde fois, il n'y a plus rien à prendre.
  const auto again = mirror.Sync();
  REQUIRE(again.has_value());
  CHECK(again->written_files == 0);
  CHECK(mirror.fetcher.requested.size() == 1);  // le manifeste, et rien d'autre
}

TEST_CASE("Miroir : l'empreinte retenue vaut tant que taille et date n'ont pas bougé",
          "[app][patcher][mirror]") {
  Mirror mirror;
  mirror.Publish("table.lua", "AAAA");
  REQUIRE(mirror.Sync().has_value());

  // Même taille, même date, autre contenu : le cache le croit à jour. C'est
  // l'économie qu'il achète, et le prix qu'on accepte.
  const std::filesystem::path table = mirror.install.path() / "table.lua";
  const auto written = std::filesystem::last_write_time(table);
  mirror.install.WriteText("table.lua", "BBBB");
  std::filesystem::last_write_time(table, written);
  const auto trusted = mirror.Sync();
  REQUIRE(trusted.has_value());
  CHECK(trusted->written_files == 0);

  // La date bouge : le fichier se rehache, et se reprend.
  std::filesystem::last_write_time(table, written + std::chrono::seconds(10));
  const auto rehashed = mirror.Sync();
  REQUIRE(rehashed.has_value());
  CHECK(rehashed->written_files == 1);
  CHECK(ReadFile(table) == "AAAA");
}

TEST_CASE("Miroir : un fichier reçu qui ne correspond pas au manifeste n'est pas posé",
          "[app][patcher][mirror]") {
  Mirror mirror;
  mirror.install.WriteText("table.lua", "ancien");
  mirror.Publish("table.lua", "nouveau");
  mirror.fetcher.served[std::string(kFilesUrl) + "table.lua"] = "nouveaX";

  const auto outcome = mirror.Sync();
  REQUIRE_FALSE(outcome.has_value());
  CHECK(outcome.error().detail.find("table.lua") != std::string::npos);
  CHECK(ReadFile(mirror.install.path() / "table.lua") == "ancien");
  CHECK_FALSE(std::filesystem::exists(
      mirror.install.path() / moonclient::patcher::kDownloadFolderName / "mirror.download"));
}

TEST_CASE("Miroir : un fichier plus long que le manifeste ne l'annonce est refusé",
          "[app][patcher][mirror]") {
  Mirror mirror;
  mirror.Publish("table.lua", "court");
  mirror.fetcher.served[std::string(kFilesUrl) + "table.lua"] = "beaucoup plus long";
  REQUIRE_FALSE(mirror.Sync().has_value());
  CHECK_FALSE(std::filesystem::exists(mirror.install.path() / "table.lua"));
}

TEST_CASE(
    "Miroir : un manifeste qui vise un fichier du patcher est refusé avant tout téléchargement",
    "[app][patcher][mirror]") {
  for (const char* reserved : {"moonclient-patch.dat", "Moonclient-Patch-Cache/x.thor",
                               "moonclient-patch-mirror.dat", "SDL3.dll.moonclient-old"}) {
    INFO(reserved);
    Mirror mirror;
    mirror.Publish("a.txt", "a");
    mirror.Publish(reserved, "piège");
    const auto outcome = mirror.Sync();
    REQUIRE_FALSE(outcome.has_value());
    CHECK(outcome.error().detail.find("patcher") != std::string::npos);
    CHECK_FALSE(mirror.Requested("a.txt"));
  }
}

TEST_CASE("Miroir : l'exécutable qui tourne s'écarte, et le client doit se relancer",
          "[app][patcher][mirror]") {
  Mirror mirror;
  mirror.install.WriteText("Moonlight-Destiny.exe", "ancien exe");
  mirror.Publish("Moonlight-Destiny.exe", "nouvel exe");
  const std::filesystem::path running = mirror.install.path() / "Moonlight-Destiny.exe";

  const auto outcome = mirror.Sync(running);
  REQUIRE(outcome.has_value());
  CHECK(outcome->restart_needed);
  CHECK(ReadFile(running) == "nouvel exe");
  CHECK(ReadFile(mirror.install.path() / "Moonlight-Destiny.exe.old") == "ancien exe");
}

TEST_CASE("Miroir : le nouvel exécutable garde le droit d'exécution de l'ancien",
          "[app][patcher][mirror]") {
  Mirror mirror;
  mirror.install.WriteText("MoonClient", "ancien binaire");
  mirror.Publish("MoonClient", "nouveau binaire");
  const std::filesystem::path running = mirror.install.path() / "MoonClient";
  std::filesystem::permissions(running, std::filesystem::perms::owner_exec,
                               std::filesystem::perm_options::add);
  const std::filesystem::perms before = std::filesystem::status(running).permissions();

  const auto outcome = mirror.Sync(running);
  REQUIRE(outcome.has_value());
  CHECK(ReadFile(running) == "nouveau binaire");
  CHECK(std::filesystem::status(running).permissions() == before);
}

TEST_CASE("Miroir : un serveur sans manifeste ne demande rien de plus", "[app][patcher][mirror]") {
  Mirror mirror;
  PatchRunSettings settings = mirror.Settings();
  settings.config.mirrors[0].manifest_url.clear();
  settings.config.mirrors[0].files_url.clear();
  PatchProgressBoard board;
  PatchProgress progress;
  const auto outcome = SyncMirror(settings, {}, mirror.fetcher, board, progress, std::stop_token());
  REQUIRE(outcome.has_value());
  CHECK(outcome->written_files == 0);
  CHECK(mirror.fetcher.requested.empty());
}

TEST_CASE("Miroir : la chaîne passe d'abord, puis le miroir, et le compte rendu dit les deux",
          "[app][patcher][mirror]") {
  Mirror mirror;
  mirror.fetcher.served[kListUrl] = "";
  mirror.Publish("table.lua", "return {}");
  mirror.fetcher.served[kManifestUrl] = mirror.manifest;
  PatchProgressBoard board;
  const auto outcome = RunPatches(mirror.Settings(), mirror.fetcher, board, std::stop_token());
  REQUIRE(outcome.has_value());
  CHECK(outcome->applied_patches == 0);
  CHECK(outcome->mirrored_files == 1);
  CHECK(board.Snapshot().phase == moonclient::patcher::PatchPhase::kDone);
  REQUIRE(mirror.fetcher.requested.size() == 3);
  CHECK(mirror.fetcher.requested[0] == kListUrl);
  CHECK(mirror.fetcher.requested[1] == kManifestUrl);
}
