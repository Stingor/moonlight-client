#include "ro/core/byte_writer.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/byte_reader.h"

using ro::core::ByteReader;
using ro::core::ByteWriter;

TEST_CASE("Écrivain : les entiers sortent en petit-boutiste", "[core][writer]") {
  // L'ordre est celui des formats et des paquets RO. L'inverser ne se voit pas
  // sur un octet, et change tout au-delà.
  std::vector<std::uint8_t> bytes;
  ByteWriter writer(bytes);
  writer.U16(0x1234);
  writer.U32(0xDEADBEEF);

  REQUIRE(bytes.size() == 6);
  CHECK(bytes[0] == 0x34);
  CHECK(bytes[1] == 0x12);
  CHECK(bytes[2] == 0xEF);
  CHECK(bytes[3] == 0xBE);
  CHECK(bytes[4] == 0xAD);
  CHECK(bytes[5] == 0xDE);
}

TEST_CASE("Écrivain : ce qu'il écrit, le lecteur le relit", "[core][writer]") {
  // Le vrai contrôle : les deux moitiés doivent être exactement inverses. Les
  // vérifier séparément laisserait passer une erreur commune aux deux.
  std::vector<std::uint8_t> bytes;
  ByteWriter writer(bytes);
  writer.U8(0x7F);
  writer.I16(-1234);
  writer.I32(-70000);
  writer.I64(-9'000'000'000LL);
  writer.U64(0xFEDCBA9876543210ULL);
  writer.F32(3.5F);

  ByteReader reader(bytes);
  CHECK(reader.U8() == 0x7F);
  CHECK(reader.I16() == -1234);
  CHECK(reader.I32() == -70000);
  CHECK(reader.I64() == -9'000'000'000LL);
  CHECK(reader.U64() == 0xFEDCBA9876543210ULL);
  CHECK(reader.F32() == 3.5F);
  CHECK(reader.ok());
  CHECK(reader.remaining() == 0);
}

TEST_CASE("Écrivain : un champ de largeur fixe est rempli JUSQU'AU BORD", "[core][writer]") {
  // Tous les champs de largeur fixe ne sont pas des chaînes. Le code PIN fait
  // exactement quatre chiffres, que le serveur exige tous les quatre : réserver
  // le dernier octet pour un terminateur en enverrait trois, et la connexion
  // serait fermée sans un mot.
  //
  // Rien n'est perdu pour les vrais textes : le pair tronque déjà de son côté.
  constexpr std::size_t kWidth = 8;
  std::vector<std::uint8_t> bytes;
  ByteWriter writer(bytes);
  writer.FixedString("abcdefghijkl", kWidth);

  REQUIRE(bytes.size() == kWidth);
  CHECK(bytes[kWidth - 1] == 'h');

  ByteReader reader(bytes);
  CHECK(reader.FixedString(kWidth) == "abcdefgh");
}

TEST_CASE("Écrivain : un champ plus court est complété de zéros", "[core][writer]") {
  constexpr std::size_t kWidth = 8;
  std::vector<std::uint8_t> bytes;
  ByteWriter writer(bytes);
  writer.FixedString("ab", kWidth);

  REQUIRE(bytes.size() == kWidth);
  for (std::size_t index = 2; index < kWidth; ++index) {
    CHECK(bytes[index] == 0);
  }
}

TEST_CASE("Écrivain : une queue de texte s'écrit sans terminateur", "[core][writer]") {
  // Sa longueur est celle de la trame ; y ajouter un zéro l'allongerait d'un
  // octet que le pair compterait comme du contenu.
  std::vector<std::uint8_t> bytes;
  ByteWriter writer(bytes);
  writer.Text("bonjour");
  CHECK(bytes.size() == 7);
}

TEST_CASE("Écrivain : la longueur réservée se remplit après coup", "[core][writer]") {
  // Elle n'est connue qu'une fois la queue écrite. La compter d'avance
  // reviendrait à la compter deux fois, donc à risquer de la compter
  // différemment.
  std::vector<std::uint8_t> bytes;
  ByteWriter writer(bytes);
  writer.U16(0x0069);
  writer.U16(0);
  writer.Text("charge utile");
  REQUIRE(writer.PatchU16(2, static_cast<std::uint16_t>(writer.size())));

  ByteReader reader(bytes);
  CHECK(reader.U16() == 0x0069);
  CHECK(reader.U16() == bytes.size());
}

TEST_CASE("Écrivain : une correction hors du tampon est refusée", "[core][writer]") {
  // Écrire au-delà de ce qui existe corromprait la mémoire voisine ; le dire
  // vaut mieux que d'agrandir le tampon en silence, ce qui décalerait la trame.
  std::vector<std::uint8_t> bytes;
  ByteWriter writer(bytes);
  writer.U8(1);
  CHECK_FALSE(writer.PatchU16(0, 0x1234));
  CHECK(bytes.size() == 1);
}
