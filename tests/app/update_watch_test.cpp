// Le guet des mises à jour : le serveur publie-t-il plus neuf que
// l'installation ? Sur un réseau factice et une installation temporaire.

#include "patcher/update_watch.h"

#include <chrono>
#include <filesystem>
#include <memory>
#include <stop_token>
#include <string>
#include <thread>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/file_write.h"
#include "ro/core/sha256.h"

#include "fake_patch_fetcher.h"
#include "patcher/mirror_sync.h"
#include "patcher/patcher_state.h"
#include "support/temp_directory.h"

using moonclient::patcher::PatchProgress;
using moonclient::patcher::PatchProgressBoard;
using moonclient::patcher::PatchRunSettings;
using moonclient::patcher::ServerHasNewer;
using moonclient::patcher::UpdateWatch;
using moonclient::test_support::FakePatchFetcher;
using ro::test_support::TempDirectory;

namespace {

constexpr const char* kListUrl = "https://patchs.example/plist.txt";
constexpr const char* kArchiveUrl = "https://patchs.example/data/";
constexpr const char* kManifestUrl = "https://patchs.example/manifest.php";
constexpr const char* kFilesUrl = "https://patchs.example/files/";
// Ce qu'un essai du fil attend au plus avant de conclure qu'il n'a rien vu.
constexpr std::chrono::seconds kWatchPatience{10};

[[nodiscard]] std::string Sha256Of(const std::string& bytes) {
  return ro::core::Sha256::ToHex(ro::core::Sha256::Hash(
      std::span(reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size())));
}

// Une installation à jour d'une chaîne d'un patch, et un miroir qu'on fait
// avancer sous ses pieds.
struct Server {
  TempDirectory install;
  FakePatchFetcher fetcher;
  std::string manifest = "moonclient-mirror 1\n";

  Server() {
    fetcher.served[kListUrl] = "1 premier.thor\n";
    REQUIRE(
        ro::core::WriteFileAtomically(install.path() / moonclient::patcher::kPatcherStateFileName,
                                      moonclient::patcher::FormatPatcherState(1))
            .has_value());
  }

  void Publish(const std::string& relative_path, const std::string& content) {
    manifest +=
        Sha256Of(content) + " " + std::to_string(content.size()) + " " + relative_path + "\n";
    fetcher.served[moonclient::patcher::MirrorFileUrl(kFilesUrl, relative_path)] = content;
    fetcher.served[kManifestUrl] = manifest;
  }

  // Le miroir repart de zéro : la publication suivante le remplace.
  void Unpublish() {
    manifest = "moonclient-mirror 1\n";
    fetcher.served[kManifestUrl] = manifest;
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

  // Ce que fait le lancement : l'installation prend ce que le miroir publie.
  void Launch() {
    PatchProgressBoard board;
    PatchProgress progress;
    REQUIRE(
        moonclient::patcher::SyncMirror(Settings(), {}, fetcher, board, progress, std::stop_token())
            .has_value());
  }

  [[nodiscard]] ro::core::Result<bool> Check() {
    return ServerHasNewer(Settings(), fetcher, std::stop_token());
  }

  // Un réseau à part pour le fil, qui sert la même chose : le fil seul y touche.
  [[nodiscard]] std::unique_ptr<FakePatchFetcher> SameNetwork() const {
    auto copy = std::make_unique<FakePatchFetcher>();
    copy->served = fetcher.served;
    return copy;
  }
};

}  // namespace

TEST_CASE("Guet : une installation tout juste lancée est à jour", "[app][patcher][update_watch]") {
  Server server;
  server.Publish("table.lua", "AAAA");
  server.Launch();
  const auto newer = server.Check();
  REQUIRE(newer.has_value());
  CHECK_FALSE(*newer);
}

TEST_CASE("Guet : un patch de plus dans la chaîne est une mise à jour",
          "[app][patcher][update_watch]") {
  Server server;
  server.fetcher.served[kListUrl] = "1 premier.thor\n2 second.thor\n";
  const auto newer = server.Check();
  REQUIRE(newer.has_value());
  CHECK(*newer);
}

TEST_CASE("Guet : un fichier du miroir nouveau, ou d'une autre taille, est une mise à jour",
          "[app][patcher][update_watch]") {
  Server server;
  server.Publish("table.lua", "AAAA");
  server.Launch();

  server.Publish("SystemEN/nouveau.lua", "return {}");
  auto newer = server.Check();
  REQUIRE(newer.has_value());
  CHECK(*newer);

  server.Unpublish();
  server.Publish("table.lua", "AAAAA");
  newer = server.Check();
  REQUIRE(newer.has_value());
  CHECK(*newer);
}

TEST_CASE("Guet : même taille, autre empreinte que celle du cache, est une mise à jour",
          "[app][patcher][update_watch]") {
  Server server;
  server.Publish("table.lua", "AAAA");
  server.Launch();
  server.Unpublish();
  server.Publish("table.lua", "BBBB");
  const auto newer = server.Check();
  REQUIRE(newer.has_value());
  CHECK(*newer);
  // Rien ne s'est téléchargé : seuls la liste et le manifeste ont été demandés.
  CHECK(server.fetcher.requested.back() == kManifestUrl);
}

TEST_CASE("Guet : un fichier dont le cache ne dit plus rien ne se tranche pas",
          "[app][patcher][update_watch]") {
  Server server;
  server.Publish("table.lua", "AAAA");
  server.Launch();
  // Retouché sur place, à la même taille : la date a bougé, le cache ne vaut
  // plus, et le guet ne hache pas pour savoir.
  const std::filesystem::path table = server.install.path() / "table.lua";
  const auto written = std::filesystem::last_write_time(table);
  server.install.WriteText("table.lua", "CCCC");
  std::filesystem::last_write_time(table, written + std::chrono::seconds(1));
  const auto newer = server.Check();
  REQUIRE(newer.has_value());
  CHECK_FALSE(*newer);
}

TEST_CASE("Guet : un serveur muet est une erreur, pas une installation à jour",
          "[app][patcher][update_watch]") {
  Server server;
  server.fetcher.served.clear();
  CHECK_FALSE(server.Check().has_value());
}

TEST_CASE("Guet : le fil apprend la nouvelle et la garde", "[app][patcher][update_watch]") {
  Server server;
  server.fetcher.served[kListUrl] = "1 premier.thor\n2 second.thor\n";
  const UpdateWatch watch(server.Settings(), server.SameNetwork(), std::chrono::milliseconds(1));
  const auto deadline = std::chrono::steady_clock::now() + kWatchPatience;
  while (!watch.update_available() && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(watch.update_available());
}

TEST_CASE("Guet : le fil s'arrête avec son propriétaire, même en pleine attente",
          "[app][patcher][update_watch]") {
  Server server;
  const auto started = std::chrono::steady_clock::now();
  {
    const UpdateWatch watch(server.Settings(), server.SameNetwork(), std::chrono::hours(1));
    CHECK_FALSE(watch.update_available());
  }
  CHECK(std::chrono::steady_clock::now() - started < kWatchPatience);
}
