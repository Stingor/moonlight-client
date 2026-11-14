// OÙ L'ON CHERCHE L'ILLUSTRATION QU'UN SCRIPT NOMME.

#include "ro/vfs/npc_illustration_paths.h"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/encoding.h"

using ro::vfs::NpcIllustrationPaths;

namespace {

// Les deux dossiers, en CP949 comme les archives les nomment.
std::string Folder(const char* utf8) {
  auto cp949 = ro::vfs::Utf8ToCp949(utf8);
  REQUIRE(cp949.has_value());
  return *cp949;
}

}  // namespace

TEST_CASE("Illustration : un nom avec extension se cherche dans les deux dossiers",
          "[vfs][illustration]") {
  const std::string illust = Folder("data\\texture\\유저인터페이스\\illust\\");
  const std::string cardbmp = Folder("data\\texture\\유저인터페이스\\cardbmp\\");
  CHECK(NpcIllustrationPaths("ep18_maram_01.png") ==
        std::vector<std::string>{illust + "ep18_maram_01.png", cardbmp + "ep18_maram_01.png"});
}

TEST_CASE("Illustration : un nom SANS extension essaie le BMP puis le PNG", "[vfs][illustration]") {
  const std::string illust = Folder("data\\texture\\유저인터페이스\\illust\\");
  const std::string cardbmp = Folder("data\\texture\\유저인터페이스\\cardbmp\\");
  CHECK(NpcIllustrationPaths("16lei_01") ==
        std::vector<std::string>{illust + "16lei_01.bmp", illust + "16lei_01.png",
                                 cardbmp + "16lei_01.bmp", cardbmp + "16lei_01.png"});
}

TEST_CASE("Illustration : un nom vide ne mène nulle part", "[vfs][illustration]") {
  CHECK(NpcIllustrationPaths("").empty());
}
