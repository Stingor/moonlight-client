// LES MERCENAIRES HUMAINS SONT HABILLÉS PAR LE CLIENT.
//
// 🔴 LE SERVEUR N'ENVOIE QUE LEUR CLASSE. Sans ces pièces, un mercenaire se
// dessinait en corps sans tête : tout ce qui l'habille — sa tête, son arme,
// sa coiffe — se déduit de la classe seule, et c'est ce que ce banc garde.

#include <algorithm>
#include <cstdint>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/formats/lua_table.h"
#include "ro/render/sprite_catalog.h"

using ro::render::ActorFiles;
using ro::render::ActorLook;
using ro::render::ActorPart;
using ro::render::ActorPieceFiles;
using ro::render::SpriteCatalog;

namespace {

// ⚠ UN CATALOGUE VIDE SUFFIT : un mercenaire humain se nomme par une table que
// le client porte en dur, pas par ses tables Lua.
[[nodiscard]] std::vector<ActorPieceFiles> FilesOf(std::uint32_t job) {
  const SpriteCatalog catalog{};
  ActorLook look;
  look.job = job;
  return ActorFiles(catalog, look);
}

[[nodiscard]] std::vector<std::string> PathsOf(const std::vector<ActorPieceFiles>& files,
                                               ActorPart part) {
  std::vector<std::string> paths;
  for (const ActorPieceFiles& piece : files) {
    if (piece.part == part) {
      paths.insert(paths.end(), piece.candidates.begin(), piece.candidates.end());
    }
  }
  return paths;
}

}  // namespace

TEST_CASE("Mercenaire : l'archère porte sa tête, son arc et sa coiffe", "[render][catalog]") {
  const std::vector<ActorPieceFiles> files = FilesOf(6017);
  REQUIRE(!files.empty());
  CHECK(files.front().part == ActorPart::kBody);
  // LA TÊTE : classe modulo vingt-trois, plus un — 6017 donne quinze. Femme.
  CHECK(PathsOf(files, ActorPart::kHead) ==
        std::vector<std::string>{"data\\sprite\\인간족\\머리통\\여\\15_여"});
  CHECK(PathsOf(files, ActorPart::kWeapon) ==
        std::vector<std::string>{"data\\sprite\\인간족\\용병\\활용병_활"});
  // Une archère n'a pas de lueur d'arme.
  CHECK(PathsOf(files, ActorPart::kWeaponTrail).empty());
  CHECK(PathsOf(files, ActorPart::kHeadgearLow) ==
        std::vector<std::string>{"data\\sprite\\악세사리\\여\\여_털모자"});
  // Ni ornement : elle n'est pas la meilleure de sa lignée.
  CHECK(PathsOf(files, ActorPart::kHeadgearTop).empty());
}

TEST_CASE("Mercenaire : le meilleur épéiste, sa lueur et ses ornements", "[render][catalog]") {
  const std::vector<ActorPieceFiles> files = FilesOf(6046);
  // 6046 modulo 23 vaut 20 : la tête 21, d'homme.
  CHECK(PathsOf(files, ActorPart::kHead) ==
        std::vector<std::string>{"data\\sprite\\인간족\\머리통\\남\\21_남"});
  CHECK(PathsOf(files, ActorPart::kWeaponTrail) ==
        std::vector<std::string>{"data\\sprite\\인간족\\용병\\검용병_검_검광"});
  CHECK(PathsOf(files, ActorPart::kHeadgearLow) ==
        std::vector<std::string>{"data\\sprite\\악세사리\\남\\남_태양신의모자"});
  // ⚠ DEUX ORNEMENTS ET NON TROIS : son premier rang repeint sa coiffe.
  CHECK(PathsOf(files, ActorPart::kHeadgearTop) ==
        std::vector<std::string>{"data\\sprite\\악세사리\\남\\남_요정의귀",
                                 "data\\sprite\\악세사리\\남\\남_사이클롭스아이"});
}

TEST_CASE("Mercenaire : le lancier porte la lance, et rien hors de la plage", "[render][catalog]") {
  const std::vector<ActorPieceFiles> spearman = FilesOf(6030);
  CHECK(PathsOf(spearman, ActorPart::kWeapon) ==
        std::vector<std::string>{"data\\sprite\\인간족\\용병\\창용병_창"});
  CHECK(PathsOf(spearman, ActorPart::kHeadgearLow) ==
        std::vector<std::string>{"data\\sprite\\악세사리\\남\\남_양모자"});
  // Un homoncule, juste avant la plage, ne s'habille pas.
  CHECK(PathsOf(FilesOf(6016), ActorPart::kHead).empty());
}

TEST_CASE("Mercenaire : la plage des mercenaires humains, bornes comprises", "[render][catalog]") {
  // Elle décide de l'habillage ET du gel au repos : un rang de trop fige un
  // homoncule, un rang de moins fait tourner la tête du dernier épéiste.
  CHECK_FALSE(ro::render::IsHumanMercenaryClass(6016));
  CHECK(ro::render::IsHumanMercenaryClass(6017));
  CHECK(ro::render::IsHumanMercenaryClass(6046));
  CHECK_FALSE(ro::render::IsHumanMercenaryClass(6047));
}
