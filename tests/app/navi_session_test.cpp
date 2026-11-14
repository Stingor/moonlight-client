// LA RECHERCHE DEMANDÉE AVANT LES TABLES : retenue, puis lancée à leur
// adoption — et une seule, la dernière.

#include "world/navi_session.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/vfs.h"

#include "game/save_folder.h"
#include "support/temp_directory.h"
#include "world/navi_file.h"

using moonclient::NaviSession;
using ro::formats::NaviData;
using ro::sim::NaviHit;
using ro::ui::NavigationRowRef;

namespace {

constexpr std::uint16_t kSimpleMapKind = 5001;
constexpr std::uint16_t kMapSideCells = 100;
constexpr std::uint32_t kProntera = 0;
constexpr std::uint32_t kGeffen = 1;

std::shared_ptr<NaviData> TwoMaps() {
  auto data = std::make_shared<NaviData>();
  data->maps.push_back(
      ro::formats::NaviMap{"prontera", "Prontera", kSimpleMapKind, kMapSideCells, kMapSideCells});
  data->maps.push_back(
      ro::formats::NaviMap{"geffen", "Geffen", kSimpleMapKind, kMapSideCells, kMapSideCells});
  data->BuildIndexes();
  return data;
}

// La carte du premier résultat, rien s'il n'y en a pas.
std::optional<std::uint32_t> FirstMapFound(const NaviSession& session) {
  const std::optional<NaviHit> hit = session.navigation_view.HitAt(NavigationRowRef{});
  if (!hit.has_value() || hit->kind != ro::sim::NaviHitKind::kMap) {
    return std::nullopt;
  }
  return hit->rank;
}

}  // namespace

TEST_CASE("Navigation : une recherche avant les tables part à leur adoption", "[app][navigation]") {
  NaviSession session;
  session.Search("geffen", /*only_map=*/{}, "prontera");
  CHECK(session.search_waiting());
  CHECK_FALSE(FirstMapFound(session).has_value());

  session.Adopt(TwoMaps(), "prontera");
  CHECK_FALSE(session.search_waiting());
  CHECK(FirstMapFound(session) == kGeffen);
}

TEST_CASE("Navigation : seule la dernière recherche retenue part", "[app][navigation]") {
  NaviSession session;
  session.Search("geffen", {}, "prontera");
  session.Search("prontera", {}, "prontera");
  session.Adopt(TwoMaps(), "prontera");
  CHECK(FirstMapFound(session) == kProntera);
}

TEST_CASE("Navigation : des tables illisibles oublient la recherche retenue", "[app][navigation]") {
  NaviSession session;
  session.Search("geffen", {}, "prontera");
  session.Adopt(nullptr, "prontera");
  CHECK_FALSE(session.search_waiting());
  CHECK_FALSE(FirstMapFound(session).has_value());
}

TEST_CASE("Navigation : tables lues, la recherche part tout de suite", "[app][navigation]") {
  NaviSession session;
  session.Adopt(TwoMaps(), "prontera");
  session.Search("geffen", {}, "prontera");
  CHECK_FALSE(session.search_waiting());
  CHECK(FirstMapFound(session) == kGeffen);
}

TEST_CASE("Session de navigation : les préférences vivent sous le dossier du client",
          "[app][navi]") {
  // 🔴 UN CLIENT D'ESSAI LIT LES ARCHIVES D'UNE INSTALLATION : il ne doit pas
  // y réécrire les réglages du joueur.
  const ro::test_support::TempDirectory client;
  const ro::test_support::TempDirectory installation;
  std::filesystem::create_directories(client.path() / moonclient::kSaveFolder);
  std::filesystem::create_directories(installation.path() / moonclient::kSaveFolder);

  NaviSession session;
  session.Open(client.path(), ro::vfs::MountOptions{.root = installation.path()});
  session.SavePrefs();

  CHECK(std::filesystem::exists(moonclient::NaviPrefsPath(client.path())));
  CHECK_FALSE(std::filesystem::exists(moonclient::NaviPrefsPath(installation.path())));
}
