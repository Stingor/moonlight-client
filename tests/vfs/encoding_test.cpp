#include "ro/vfs/encoding.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

using ro::vfs::Cp949ToUtf8;
using ro::vfs::Cp949ToUtf8Lossy;
using ro::vfs::Utf8ToCp949;

TEST_CASE("L'ASCII traverse sans changer", "[vfs][encoding]") {
  const std::string ascii = "data\\sprite\\poring.spr";

  auto to_cp949 = Utf8ToCp949(ascii);
  REQUIRE(to_cp949.has_value());
  CHECK(*to_cp949 == ascii);

  auto back = Cp949ToUtf8(ascii);
  REQUIRE(back.has_value());
  CHECK(*back == ascii);
}

TEST_CASE("Une chaîne vide reste vide", "[vfs][encoding]") {
  auto to_cp949 = Utf8ToCp949("");
  REQUIRE(to_cp949.has_value());
  CHECK(to_cp949->empty());

  auto to_utf8 = Cp949ToUtf8("");
  REQUIRE(to_utf8.has_value());
  CHECK(to_utf8->empty());
}

TEST_CASE("Un caractère absent du CP949 fait échouer la conversion", "[vfs][encoding]") {
  CHECK_FALSE(Utf8ToCp949("data\\model\\d\xC3\xA9"
                          "cor.rsm")
                  .has_value());
  // Un remplacement ne doit pas passer pour une réussite, ni un UTF-8 invalide.
  CHECK_FALSE(Utf8ToCp949("\xF0\x9F\x94\xB4").has_value());
  CHECK_FALSE(Utf8ToCp949("\xC3").has_value());
}

TEST_CASE("Le CP949 fait l'aller-retour", "[vfs][encoding]") {
  // « 인간족 » — le nom du dossier des sprites de personnage dans les archives
  // officielles. Six octets CP949 qui doivent revenir identiques.
  const std::string korean_cp949 = "\xC0\xCE\xB0\xA3\xC1\xB7";

  auto utf8 = Cp949ToUtf8(korean_cp949);
  REQUIRE(utf8.has_value());
  // En UTF-8 chaque idéogramme prend trois octets.
  CHECK(utf8->size() == 9);

  auto back = Utf8ToCp949(*utf8);
  REQUIRE(back.has_value());
  CHECK(*back == korean_cp949);
}

TEST_CASE("Un chemin mixte fait l'aller-retour", "[vfs][encoding]") {
  // Le cas réel : de l'ASCII, un dossier coréen, puis de l'ASCII à nouveau.
  const std::string path_cp949 = "data\\sprite\\\xC0\xCE\xB0\xA3\xC1\xB7\\body.spr";

  auto utf8 = Cp949ToUtf8(path_cp949);
  REQUIRE(utf8.has_value());

  auto back = Utf8ToCp949(*utf8);
  REQUIRE(back.has_value());
  CHECK(*back == path_cp949);
}

TEST_CASE("Le nom d'un fichier se relit en CP949 ou reste tel quel", "[vfs][encoding]") {
  CHECK(ro::vfs::FileNameOfRaw("model.rsm") == "model.rsm");
  const auto raw_korean = ro::vfs::Utf8ToCp949("\xEB\xAA\xA8\xEB\x8D\xB8.rsm");
  REQUIRE(raw_korean.has_value());
  CHECK(ro::vfs::FileNameOfRaw(*raw_korean) == "\xEB\xAA\xA8\xEB\x8D\xB8.rsm");
  // Un caractère absent de CP949, dont l'UTF-8 ne forme pas de paire valide.
  const std::string warning_sign = "\xE2\x9A\xA0x.rsm";
  CHECK(ro::vfs::FileNameOfRaw(warning_sign) == warning_sign);
}

TEST_CASE("La variante tolérante ne rend jamais d'erreur", "[vfs][encoding]") {
  // Un octet isolé qui ne peut pas commencer une paire CP949 valide. Utile pour
  // journaliser un nom abîmé sans faire échouer la commande en cours.
  const std::string broken = "abc\xFF\xFE";
  const std::string lossy = Cp949ToUtf8Lossy(broken);
  CHECK(lossy.starts_with("abc"));
  CHECK_FALSE(lossy.empty());
}
