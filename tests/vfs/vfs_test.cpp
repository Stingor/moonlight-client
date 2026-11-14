#include "ro/vfs/vfs.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/error.h"
#include "ro/vfs/encoding.h"

#include "support/grf_builder.h"
#include "support/temp_directory.h"

using ro::core::ErrorCode;
using ro::test_support::BuildGrf;
using ro::test_support::Bytes;
using ro::test_support::GrfFileSpec;
using ro::test_support::TempDirectory;
using ro::vfs::DiskSpellings;
using ro::vfs::MountOptions;
using ro::vfs::Vfs;

namespace {

std::string TextOf(const std::vector<std::uint8_t>& bytes) { return {bytes.begin(), bytes.end()}; }

// Un client minimal : deux archives, et un dossier data\.
//
// Les trois portent « data\partage.txt » avec un contenu différent : c'est ce
// qui rend l'ordre de résolution observable, et c'est tout l'intérêt du VFS.
void MakeClient(const TempDirectory& client) {
  client.WriteFile(
      "prioritaire.grf",
      BuildGrf({{"data\\partage.txt", Bytes("archive prioritaire")},
                {"data\\uniquement_prioritaire.txt", Bytes("seul dans la première")}}));
  client.WriteFile("secondaire.grf",
                   BuildGrf({{"data\\partage.txt", Bytes("archive secondaire")},
                             {"data\\uniquement_secondaire.txt", Bytes("seul dans la seconde")}}));
}

// Le client minimal, monté dans l'ordre de ses archives : la prioritaire d'abord.
ro::core::Result<Vfs> MountMadeClient(const TempDirectory& client) {
  return Vfs::Mount({.root = client.path(), .archives = {"prioritaire.grf", "secondaire.grf"}});
}

}  // namespace

TEST_CASE("VFS : la première archive l'emporte", "[vfs]") {
  TempDirectory client;
  MakeClient(client);

  auto vfs = MountMadeClient(client);
  REQUIRE(vfs.has_value());
  REQUIRE(vfs->archives().size() == 2);

  auto shared = vfs->Read("data\\partage.txt");
  REQUIRE(shared.has_value());
  CHECK(TextOf(*shared) == "archive prioritaire");

  // Chaque archive reste atteignable pour ce qu'elle est seule à porter.
  auto only_second = vfs->Read("data\\uniquement_secondaire.txt");
  REQUIRE(only_second.has_value());
  CHECK(TextOf(*only_second) == "seul dans la seconde");
}

TEST_CASE("VFS : le dossier data\\ passe avant toutes les archives", "[vfs]") {
  // C'est ce qui permet de remplacer un fichier sans repacker un GRF, et le
  // client officiel se comporte pareil.
  TempDirectory client;
  MakeClient(client);
  client.WriteText("data/partage.txt", "disque");

  auto vfs = MountMadeClient(client);
  REQUIRE(vfs.has_value());
  CHECK_FALSE(vfs->data_directory().empty());

  auto shared = vfs->Read("data\\partage.txt");
  REQUIRE(shared.has_value());
  CHECK(TextOf(*shared) == "disque");
}

TEST_CASE("VFS : un dossier de surcharge passe avant tout, pour son préfixe seul", "[vfs]") {
  // La règle des skins : le dossier ne porte que ce qu'il change, et le reste
  // retombe sur la chaîne ordinaire.
  TempDirectory client;
  client.WriteFile("prioritaire.grf", BuildGrf({{"data\\texture\\ui\\change.bmp", Bytes("archive")},
                                                {"data\\texture\\ui\\garde.bmp", Bytes("archive")},
                                                {"data\\autre.txt", Bytes("archive")}}));
  client.WriteText("skin/sombre/change.bmp", "skin");
  client.WriteText("skin/sombre/ajout.bmp", "ajout");
  client.WriteText("skin/sombre/autre.txt", "hors du préfixe");

  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"prioritaire.grf"}});
  REQUIRE(vfs.has_value());
  const std::uint64_t before = vfs->overlay_revision();
  vfs->OverlayFolder("data\\texture\\ui\\", client.path() / "skin" / "sombre");
  // Qui a lu sous la surcharge apprend qu'il doit relire.
  CHECK(vfs->overlay_revision() != before);

  auto changed = vfs->Read("data/Texture/UI/Change.bmp");
  REQUIRE(changed.has_value());
  CHECK(TextOf(*changed) == "skin");
  auto kept = vfs->Read("data\\texture\\ui\\garde.bmp");
  REQUIRE(kept.has_value());
  CHECK(TextOf(*kept) == "archive");
  // Une pièce que seul le dossier porte existe.
  CHECK(vfs->Contains("data\\texture\\ui\\ajout.bmp"));
  // Hors du préfixe, le dossier ne compte pas.
  auto outside = vfs->Read("data\\autre.txt");
  REQUIRE(outside.has_value());
  CHECK(TextOf(*outside) == "archive");

  // Un dossier vide retire la surcharge.
  const std::uint64_t posed = vfs->overlay_revision();
  vfs->OverlayFolder("data\\texture\\ui\\", {});
  CHECK(vfs->overlay_revision() != posed);
  auto restored = vfs->Read("data\\texture\\ui\\change.bmp");
  REQUIRE(restored.has_value());
  CHECK(TextOf(*restored) == "archive");
  CHECK_FALSE(vfs->Contains("data\\texture\\ui\\ajout.bmp"));
}

TEST_CASE("VFS : le disque peut être désactivé", "[vfs]") {
  TempDirectory client;
  MakeClient(client);
  client.WriteText("data/partage.txt", "disque");

  auto vfs = Vfs::Mount(MountOptions{.root = client.path(),
                                     .archives = {"prioritaire.grf", "secondaire.grf"},
                                     .prefer_data_directory = false});
  REQUIRE(vfs.has_value());
  CHECK(vfs->data_directory().empty());

  auto shared = vfs->Read("data\\partage.txt");
  REQUIRE(shared.has_value());
  CHECK(TextOf(*shared) == "archive prioritaire");
}

TEST_CASE("VFS : SystemEN\\ se lit sur le disque, même sans dossier data\\", "[vfs]") {
  // 🔴 L'INSTALLATION D'UN JOUEUR N'A PAS DE DOSSIER `data\` : il ne sert qu'à
  // remplacer un fichier des archives. Ce qui vit hors de lui — la table des
  // repères des villes, les recettes — ne doit pas se perdre avec lui.
  TempDirectory client;
  MakeClient(client);
  client.WriteText("SystemEN/Towninfo.lub", "tbl = {}");

  auto vfs = MountMadeClient(client);
  REQUIRE(vfs.has_value());
  REQUIRE(vfs->data_directory().empty());
  auto table = vfs->Read("SystemEN\\Towninfo.lub");
  REQUIRE(table.has_value());
  CHECK(TextOf(*table) == "tbl = {}");

  // Et même le disque « désactivé » : l'option ne gouverne que `data\`.
  auto without_disk = Vfs::Mount(MountOptions{
      .root = client.path(), .archives = {"prioritaire.grf"}, .prefer_data_directory = false});
  REQUIRE(without_disk.has_value());
  CHECK(without_disk->Contains("SystemEN\\Towninfo.lub"));
}

TEST_CASE("VFS : SystemEN\\ se lit dans une archive, hors de data\\", "[vfs]") {
  // L'installation livre `SystemEN\` dans `moonlight.grf`, sans dossier sur le
  // disque : la clé hors de `data\` doit s'y trouver comme une autre.
  TempDirectory client;
  client.WriteFile("moonlight.grf", BuildGrf({{"SystemEN\\some_table.yml", Bytes("recipes: []")}}));

  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"moonlight.grf"}});
  REQUIRE(vfs.has_value());
  CHECK(vfs->Contains("SystemEN\\some_table.yml"));
  auto book = vfs->Read("systemen/Some_Table.yml");
  REQUIRE(book.has_value());
  CHECK(TextOf(*book) == "recipes: []");
}

TEST_CASE("VFS : recherche insensible à la casse et au séparateur", "[vfs]") {
  TempDirectory client;
  MakeClient(client);

  auto vfs = MountMadeClient(client);
  REQUIRE(vfs.has_value());

  CHECK(vfs->Contains("data\\partage.txt"));
  CHECK(vfs->Contains("DATA/Partage.TXT"));
  CHECK_FALSE(vfs->Contains("data\\absent.txt"));

  auto missing = vfs->Read("data\\absent.txt");
  REQUIRE_FALSE(missing.has_value());
  CHECK(missing.error().code == ErrorCode::kNotFound);
}

TEST_CASE("VFS : List dédoublonne entre archives", "[vfs]") {
  TempDirectory client;
  MakeClient(client);

  auto vfs = MountMadeClient(client);
  REQUIRE(vfs.has_value());

  const std::vector<std::string> listed = vfs->List("data\\");
  // partage.txt est dans les DEUX archives et ne doit apparaître qu'une fois.
  REQUIRE(listed.size() == 3);
  CHECK(listed[0] == "data\\partage.txt");

  const std::vector<std::string> filtered = vfs->List("data\\uniquement", ".txt");
  CHECK(filtered.size() == 2);
}

TEST_CASE("VFS : List voit aussi le disque", "[vfs]") {
  // 🔴 L'EN-TÊTE PROMETTAIT « DU DISQUE COMME DES ARCHIVES », et seul le
  // second se listait : une carte posée dans `data\\` se lisait mais
  // n'apparaissait dans aucun relevé.
  TempDirectory client;
  MakeClient(client);
  client.WriteText("data/Cartes/Nouvelle.RSW", "carte");
  client.WriteText("data/partage.txt", "disque");

  auto vfs = MountMadeClient(client);
  REQUIRE(vfs.has_value());
  const std::vector<std::string> maps = vfs->List("data\\", ".rsw");
  REQUIRE(maps.size() == 1);
  CHECK(maps[0] == "data\\cartes\\nouvelle.rsw");
  // Présent sur le disque ET dans les deux archives : une seule fois.
  CHECK(vfs->List("data\\partage").size() == 1);
}

TEST_CASE("VFS : un nom que le CP949 n'écrit pas se liste et se relit", "[vfs]") {
  // UN DÉCOR DÉPOSÉ PAR LE JOUEUR : son nom vient du disque, pas d'une archive.
  TempDirectory client;
  MakeClient(client);
  const std::string name =
      "data/model/d\xC3\xA9"
      "cor.rsm";
  client.WriteText(std::filesystem::path(
                       std::u8string(reinterpret_cast<const char8_t*>(name.data()), name.size())),
                   "maquette");

  auto vfs = MountMadeClient(client);
  REQUIRE(vfs.has_value());
  const std::vector<std::string> listed = vfs->List("data\\model\\", ".rsm");
  REQUIRE(listed.size() == 1);
  CHECK(listed[0] ==
        "data\\model\\d\xC3\xA9"
        "cor.rsm");
  CHECK(vfs->Contains(listed[0]));
  auto read = vfs->Read(listed[0]);
  REQUIRE(read.has_value());
  CHECK(std::string(read->begin(), read->end()) == "maquette");
  // La casse d'un tel nom se plie comme celle des autres.
  CHECK(
      vfs->Contains("DATA/MODEL/D\xC3\xA9"
                    "COR.RSM"));
}

TEST_CASE("VFS : la clé d'un nom que le CP949 n'écrit pas minuscule toutes ses lettres", "[vfs]") {
  // « ⚠Z » : relus par paires CP949, ces octets feraient du Z une queue.
  TempDirectory client;
  MakeClient(client);
  const std::string name = "data/model/\xE2\x9A\xA0Z.rsm";
  client.WriteText(std::filesystem::path(
                       std::u8string(reinterpret_cast<const char8_t*>(name.data()), name.size())),
                   "maquette");

  auto vfs = MountMadeClient(client);
  REQUIRE(vfs.has_value());
  const std::vector<std::string> listed = vfs->List("data\\model\\", ".rsm");
  REQUIRE(listed.size() == 1);
  CHECK(listed[0] == "data\\model\\\xE2\x9A\xA0z.rsm");
}

TEST_CASE("VFS : un nom ASCII ne s'essaie qu'une fois sur le disque", "[vfs]") {
  // 🔴 CHAQUE LECTURE INTERROGE LE DISQUE AVANT LES ARCHIVES, et un nom ASCII
  // s'écrit pareil dans les deux orthographes : l'essayer deux fois coûtait un
  // second `stat` pour chaque fichier qu'une archive sert.
  const std::filesystem::path root = "client";
  const std::vector<std::filesystem::path> ascii = DiskSpellings(root, "data\\wav\\bruit.wav");
  REQUIRE(ascii.size() == 1);
  CHECK(ascii.front() == root / "data" / "wav" / "bruit.wav");

  // « 인간족 », en CP949 : un dossier des archives officielles.
  const std::string human_cp949 = {static_cast<char>(0xC0), static_cast<char>(0xCE),
                                   static_cast<char>(0xB0), static_cast<char>(0xA3),
                                   static_cast<char>(0xC1), static_cast<char>(0xB7)};
  const std::string korean_key = "data\\sprite\\" + human_cp949 + "\\body.spr";
  const std::vector<std::filesystem::path> korean = DiskSpellings(root, korean_key);
  REQUIRE_FALSE(korean.empty());
#ifdef _WIN32
  // La première orthographe est le vrai nom Unicode. La seconde relit les octets
  // dans la page ANSI, et ne s'ajoute que si elle en diffère : sur une machine
  // en coréen, les deux se confondent.
  CHECK(korean.front() == root / std::filesystem::path(u8"data/sprite/인간족/body.spr"));
  std::string relative = korean_key;
  std::ranges::replace(relative, '\\', '/');
  const bool ansi_differs = root / std::filesystem::path(relative) != korean.front();
  CHECK(korean.size() == (ansi_differs ? 2U : 1U));
#else
  CHECK(korean.size() == 1);
#endif
}

#ifdef _WIN32
TEST_CASE("VFS : un nom coréen invalide dans la page ANSI ne lève rien", "[vfs]") {
  // Windows réglé sur « UTF-8 mondial » : ces octets CP949 n'y sont pas valides.
  constexpr unsigned int kUtf8CodePage = 65001;
  const std::filesystem::path root = "client";
  const std::string human_cp949 = {static_cast<char>(0xC0), static_cast<char>(0xCE),
                                   static_cast<char>(0xB0), static_cast<char>(0xA3),
                                   static_cast<char>(0xC1), static_cast<char>(0xB7)};
  const std::string korean_key = "data\\sprite\\" + human_cp949 + "\\body.spr";

  CHECK_FALSE(ro::vfs::WideFromCodePage(korean_key, kUtf8CodePage).has_value());

  const std::vector<std::filesystem::path> korean = DiskSpellings(root, korean_key, kUtf8CodePage);
  REQUIRE(korean.size() == 1);
  CHECK(korean.front() == root / std::filesystem::path(u8"data/sprite/인간족/body.spr"));
}
#endif

TEST_CASE("VFS : une archive illisible est ignorée, pas fatale", "[vfs]") {
  // Un client installé n'a pas forcément toutes les archives de sa liste.
  TempDirectory client;
  MakeClient(client);

  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"fantome.grf", "prioritaire.grf"}});
  REQUIRE(vfs.has_value());
  CHECK(vfs->archives().size() == 1);
  CHECK(vfs->Contains("data\\partage.txt"));
}

TEST_CASE("VFS : racine sans archive ni dossier data", "[vfs]") {
  TempDirectory client;

  auto vfs = Vfs::Mount({.root = client.path(), .archives = {"inexistante.grf"}});
  REQUIRE_FALSE(vfs.has_value());
  CHECK(vfs.error().code == ErrorCode::kNotFound);
}

TEST_CASE("VFS : racine inexistante", "[vfs]") {
  auto vfs = Vfs::Mount({.root = "ce/chemin/n/existe/pas", .archives = {"data.grf"}});
  REQUIRE_FALSE(vfs.has_value());
  CHECK(vfs.error().code == ErrorCode::kNotFound);
}

TEST_CASE("VFS : un nom du jeu se retrouve sur le disque, séparateurs et casse compris", "[vfs]") {
  // Le nom vient de la table du jeu, « bgm\131.mp3 » ; l'installation porte
  // `BGM/131.mp3`. Sous Linux, ni l'antislash ni la casse ne s'y retrouvent
  // d'eux-mêmes.
  const TempDirectory root;
  root.WriteText("BGM/131.mp3", "mp3");

  const std::filesystem::path found = ro::vfs::FindOnDisk(root.path(), "bgm\\131.mp3");
  REQUIRE_FALSE(found.empty());
  CHECK(std::filesystem::equivalent(found, root.path() / "BGM" / "131.mp3"));
  CHECK(ro::vfs::FindOnDisk(root.path(), "bgm\\absent.mp3").empty());
}
