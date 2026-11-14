// Le dossier des skins : où se range une pièce d'interface, et ce que le skin
// d'origine reçoit sans jamais perdre ce que le joueur y a mis.

#include "shell/skin_folder.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/ui/login_window.h"

#include "support/temp_directory.h"

using moonclient::SkinFilePath;
using ro::test_support::TempDirectory;

namespace {

[[nodiscard]] std::string ReadFile(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  std::ostringstream all;
  all << file.rdbuf();
  return all.str();
}

// Une installation dont le dossier data\ porte l'illustration : le VFS la sert
// sans archive.
[[nodiscard]] ro::vfs::Vfs MountWithArt(const TempDirectory& install) {
  install.WriteText(std::filesystem::path(u8"data/texture/유저인터페이스/t_login.jpg"),
                    "illustration");
  auto vfs = ro::vfs::Vfs::Mount(ro::vfs::MountOptions{.root = install.path()});
  REQUIRE(vfs.has_value());
  return std::move(*vfs);
}

}  // namespace

TEST_CASE("Skin : une pièce d'interface se range sous skin\\<nom>, sans sa racine", "[app][skin]") {
  const std::filesystem::path root = "C:/jeu";
  const auto art = SkinFilePath(root, "default", ro::ui::kLoginArtPath);
  REQUIRE(art.has_value());
  CHECK(*art == root / "skin" / "default" / "t_login.jpg");

  const auto nested = SkinFilePath(
      root, "sombre", "data\\texture\\유저인터페이스\\basic_interface\\btnbar_left.bmp");
  REQUIRE(nested.has_value());
  CHECK(*nested == root / "skin" / "sombre" / "basic_interface" / "btnbar_left.bmp");
}

TEST_CASE("Skin : hors de la racine d'interface, ou hors de l'installation, rien", "[app][skin]") {
  const std::filesystem::path root = "C:/jeu";
  CHECK_FALSE(SkinFilePath(root, "default", "data\\sprite\\a.spr").has_value());
  CHECK_FALSE(SkinFilePath(root, "default", "data\\texture\\유저인터페이스\\..\\..\\..\\x.exe")
                  .has_value());
  CHECK_FALSE(SkinFilePath(root, "", ro::ui::kLoginArtPath).has_value());
  CHECK_FALSE(SkinFilePath(root, "default", "data\\texture\\유저인터페이스\\").has_value());
}

TEST_CASE("Skin : le skin d'origine reçoit l'illustration qui lui manque", "[app][skin]") {
  const TempDirectory install;
  ro::vfs::Vfs vfs = MountWithArt(install);
  moonclient::SeedDefaultSkin(vfs, install.path());
  CHECK(ReadFile(install.path() / "skin" / "default" / "t_login.jpg") == "illustration");
}

TEST_CASE("Skin : ce que le joueur a mis dans son skin n'est jamais écrasé", "[app][skin]") {
  const TempDirectory install;
  ro::vfs::Vfs vfs = MountWithArt(install);
  install.WriteText("skin/default/t_login.jpg", "la sienne");
  moonclient::SeedDefaultSkin(vfs, install.path());
  CHECK(ReadFile(install.path() / "skin" / "default" / "t_login.jpg") == "la sienne");
}

TEST_CASE("Skin : une pièce que les archives n'ont pas est passée", "[app][skin]") {
  const TempDirectory install;
  // Un dossier data\ sans l'illustration : le montage a de quoi se faire.
  install.WriteText("data/autre.txt", "rien");
  auto vfs = ro::vfs::Vfs::Mount(ro::vfs::MountOptions{.root = install.path()});
  REQUIRE(vfs.has_value());
  moonclient::SeedDefaultSkin(*vfs, install.path());
  CHECK_FALSE(std::filesystem::exists(install.path() / "skin"));
}

TEST_CASE("Skin : une pièce du skin passe avant celle de data\\, les autres non", "[app][skin]") {
  const TempDirectory install;
  ro::vfs::Vfs vfs = MountWithArt(install);
  install.WriteText(std::filesystem::path(u8"data/texture/유저인터페이스/basic_interface/a.bmp"),
                    "data");
  install.WriteText(std::filesystem::path(u8"skin/sombre/t_login.jpg"), "skin");
  moonclient::UseSkin(vfs, install.path(), "sombre");

  auto changed = vfs.Read(ro::ui::kLoginArtPath);
  REQUIRE(changed.has_value());
  CHECK(std::string(changed->begin(), changed->end()) == "skin");
  // Le skin ne porte pas cette pièce : elle vient d'ailleurs.
  auto kept = vfs.Read("data\\texture\\유저인터페이스\\basic_interface\\a.bmp");
  REQUIRE(kept.has_value());
  CHECK(std::string(kept->begin(), kept->end()) == "data");
}

TEST_CASE("Skin : les skins installés, le skin d'origine en tête même absent", "[app][skin]") {
  const TempDirectory install;
  CHECK(moonclient::InstalledSkins(install.path()) == std::vector<std::string>{"default"});
  install.WriteText(std::filesystem::path(u8"skin/zénith/t_login.jpg"), "z");
  install.WriteText(std::filesystem::path(u8"skin/aube/t_login.jpg"), "a");
  install.WriteText(std::filesystem::path(u8"skin/default/t_login.jpg"), "d");
  install.WriteText(std::filesystem::path(u8"skin/lisez-moi.txt"), "pas un skin");
  CHECK(moonclient::InstalledSkins(install.path()) ==
        std::vector<std::string>{"default", "aube", "zénith"});
}

TEST_CASE("Skin : une pièce lue sans archive retombe sur le skin d'origine", "[app][skin]") {
  const TempDirectory install;
  CHECK_FALSE(
      moonclient::SkinPieceOnDisk(install.path(), "aube", ro::ui::kLoginArtPath).has_value());
  install.WriteText(std::filesystem::path(u8"skin/default/t_login.jpg"), "d");
  const auto fallback = moonclient::SkinPieceOnDisk(install.path(), "aube", ro::ui::kLoginArtPath);
  REQUIRE(fallback.has_value());
  CHECK(ReadFile(*fallback) == "d");
  install.WriteText(std::filesystem::path(u8"skin/aube/t_login.jpg"), "a");
  const auto own = moonclient::SkinPieceOnDisk(install.path(), "aube", ro::ui::kLoginArtPath);
  REQUIRE(own.has_value());
  CHECK(ReadFile(*own) == "a");
}
