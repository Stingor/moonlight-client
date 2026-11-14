// LES DEUX INSIGNES DE BOSS, lus depuis un client synthétique.
//
// 🔴 CE BANC GARDE LE CHEMIN, et c'est ce qu'il a de plus utile. Le dossier
// d'interface s'écrit en coréen : nos sources le portent en UTF-8, l'archive le
// range en CP949, et le VFS transcode entre les deux. Une seule de ces trois
// formes qui dérive, et l'insigne disparaît sans autre signe qu'une ligne
// d'avertissement que personne ne lit.

#include "ro/render/boss_badge.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/encoding.h"

#include "support/grf_builder.h"
#include "support/image_builder.h"
#include "support/temp_directory.h"

using ro::render::BossBadges;
using ro::render::LoadBossBadges;
using ro::test_support::BmpPixel;
using ro::test_support::BuildBmp;
using ro::test_support::GrfFileSpec;
using ro::test_support::TempDirectory;
using ro::vfs::Vfs;

namespace {

// Le fond de ces deux bitmaps est le magenta du client, pas un canal alpha.
constexpr BmpPixel kMagenta{.red = 255, .green = 0, .blue = 255};
constexpr BmpPixel kBone{.red = 220, .green = 220, .blue = 230};

[[nodiscard]] std::vector<std::uint8_t> TinyBadge() {
  return BuildBmp(2, 2, {kBone, kMagenta, kMagenta, kMagenta});
}

// Le chemin tel que L'ARCHIVE le range. ⚠ Un essai qui y mettrait les octets
// UTF-8 ne retrouverait rien, et accuserait le chargeur.
[[nodiscard]] std::string ArchivePathOf(std::string_view utf8) {
  auto cp949 = ro::vfs::Utf8ToCp949(utf8);
  REQUIRE(cp949.has_value());
  return *cp949;
}

}  // namespace

TEST_CASE("Insignes : les deux se lisent, et le magenta disparaît", "[render][boss]") {
  const TempDirectory client;
  client.WriteFile("data.grf",
                   ro::test_support::BuildGrf({
                       GrfFileSpec{ArchivePathOf(ro::render::kMiniBossBadgePath), TinyBadge()},
                       GrfFileSpec{ArchivePathOf(ro::render::kMvpBadgePath), TinyBadge()},
                   }));

  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"data.grf"}});
  REQUIRE(vfs.has_value());
  const BossBadges badges = LoadBossBadges(*vfs);

  REQUIRE_FALSE(badges.mini_boss.empty());
  REQUIRE_FALSE(badges.mvp.empty());
  CHECK(badges.mini_boss.width == 2);
  CHECK(badges.mini_boss.height == 2);
  // Le premier pixel est de l'os, le second était magenta : il est transparent.
  CHECK(badges.mini_boss.pixels[3] == 255);
  CHECK(badges.mini_boss.pixels[7] == 0);
}

// ⚠ UNE INSTALLATION SANS CES FICHIERS NE DOIT RIEN COÛTER D'AUTRE que
// l'absence d'insigne : le nom s'écrit, et le jeu continue.
TEST_CASE("Insignes : absents, ils ne rendent que du vide", "[render][boss]") {
  const TempDirectory client;
  client.WriteFile("data.grf", ro::test_support::BuildGrf({}));

  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"data.grf"}});
  REQUIRE(vfs.has_value());
  const BossBadges badges = LoadBossBadges(*vfs);
  CHECK(badges.mini_boss.empty());
  CHECK(badges.mvp.empty());
}

// 🔴 LE MINI-BOSS ET LE MVP NE LISENT PAS LE MÊME FICHIER, et c'est tout le
// sujet du rang : `montype_boss.bmp` porte une tête de mort ARGENT,
// `montype_mvp.bmp` une DORÉE. Les deux chemins partagent leur dossier et huit
// de leurs onze derniers caractères ; une faute de copie mettrait le même
// insigne partout, et seule une capture côte à côte le verrait.
TEST_CASE("Insignes : chaque rang a son fichier", "[render][boss]") {
  CHECK(ro::render::kMiniBossBadgePath != ro::render::kMvpBadgePath);
  CHECK(ro::render::kMiniBossBadgePath.ends_with("montype_boss.bmp"));
  CHECK(ro::render::kMvpBadgePath.ends_with("montype_mvp.bmp"));

  const TempDirectory client;
  // SEUL celui du MVP est là : si les deux chemins s'étaient confondus, le
  // mini-boss aurait un insigne qu'il n'a pas.
  client.WriteFile("data.grf",
                   ro::test_support::BuildGrf({
                       GrfFileSpec{ArchivePathOf(ro::render::kMvpBadgePath), TinyBadge()},
                   }));

  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"data.grf"}});
  REQUIRE(vfs.has_value());
  const BossBadges badges = LoadBossBadges(*vfs);
  CHECK(badges.mini_boss.empty());
  CHECK_FALSE(badges.mvp.empty());
}
