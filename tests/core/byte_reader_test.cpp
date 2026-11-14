#include "ro/core/byte_reader.h"

#include <array>
#include <cstdint>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::core::ByteReader;

namespace {

std::vector<std::uint8_t> Sample() { return {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08}; }

}  // namespace

TEST_CASE("ByteReader lit en petit-boutiste", "[core][byte_reader]") {
  const std::vector<std::uint8_t> data = Sample();

  SECTION("octet par octet") {
    ByteReader reader(data);
    CHECK(reader.U8() == 0x01);
    CHECK(reader.U8() == 0x02);
    CHECK(reader.offset() == 2);
    CHECK(reader.ok());
  }

  SECTION("largeurs multiples") {
    ByteReader reader(data);
    CHECK(reader.U16() == 0x0201);
    CHECK(reader.U32() == 0x06050403);
    CHECK(reader.ok());
  }

  SECTION("u64") {
    ByteReader reader(data);
    CHECK(reader.U64() == 0x0807060504030201ULL);
    CHECK(reader.remaining() == 0);
  }
}

TEST_CASE("ByteReader lit un flottant", "[core][byte_reader]") {
  // 1.0f == 0x3F800000 en IEEE 754, donc 00 00 80 3F en petit-boutiste.
  const std::vector<std::uint8_t> data = {0x00, 0x00, 0x80, 0x3F};
  ByteReader reader(data);
  CHECK(reader.F32() == 1.0F);
  CHECK(reader.ok());
}

TEST_CASE("ByteReader lit les entiers signés", "[core][byte_reader]") {
  const std::vector<std::uint8_t> data = {0xFF, 0xFF, 0xFF, 0xFF};
  ByteReader reader(data);
  CHECK(reader.I8() == -1);
  reader.Seek(0);
  CHECK(reader.I16() == -1);
  reader.Seek(0);
  CHECK(reader.I32() == -1);
}

TEST_CASE("ByteReader échoue au lieu de déborder", "[core][byte_reader]") {
  const std::vector<std::uint8_t> data = {0x01, 0x02};

  SECTION("lecture plus large que le tampon") {
    ByteReader reader(data);
    CHECK(reader.U32() == 0);
    CHECK_FALSE(reader.ok());
  }

  SECTION("tampon vide") {
    ByteReader reader(std::span<const std::uint8_t>{});
    CHECK(reader.U8() == 0);
    CHECK_FALSE(reader.ok());
  }

  SECTION("Bytes au-delà de la fin rend une vue vide") {
    ByteReader reader(data);
    CHECK(reader.Bytes(5).empty());
    CHECK_FALSE(reader.ok());
  }
}

TEST_CASE("L'échec de ByteReader est rémanent", "[core][byte_reader]") {
  // C'est la propriété qui autorise à enchaîner trente lectures d'en-tête et à
  // ne tester qu'une fois à la fin. Si elle tombe, tous les parseurs deviennent
  // faux en silence.
  const std::vector<std::uint8_t> data = Sample();
  ByteReader reader(data);

  reader.Seek(6);
  CHECK(reader.U32() == 0);  // il ne reste que 2 octets
  CHECK_FALSE(reader.ok());

  // Une lecture qui TIENDRAIT dans le tampon doit malgré tout échouer.
  reader.Seek(0);
  CHECK(reader.U8() == 0);
  CHECK_FALSE(reader.ok());
  CHECK(reader.remaining() == 0);
}

TEST_CASE("ByteReader::Seek refuse de sortir du tampon", "[core][byte_reader]") {
  const std::vector<std::uint8_t> data = Sample();
  ByteReader reader(data);

  CHECK(reader.Seek(8));  // pile à la fin : légal
  CHECK(reader.ok());
  CHECK_FALSE(reader.Seek(9));
  CHECK_FALSE(reader.ok());
}

TEST_CASE("ByteReader::CString", "[core][byte_reader]") {
  SECTION("consomme le NUL") {
    const std::vector<std::uint8_t> data = {'d', 'a', 't', 'a', 0x00, 0x42};
    ByteReader reader(data);
    CHECK(reader.CString() == "data");
    CHECK(reader.offset() == 5);
    CHECK(reader.U8() == 0x42);
    CHECK(reader.ok());
  }

  SECTION("chaîne vide") {
    const std::vector<std::uint8_t> data = {0x00};
    ByteReader reader(data);
    CHECK(reader.CString().empty());
    CHECK(reader.ok());
  }

  SECTION("absence de NUL est un échec") {
    // Une table d'entrées tronquée en plein nom ne doit pas rendre un nom
    // valide : c'est ce qui transformerait une archive corrompue en entrée
    // silencieusement fausse.
    const std::vector<std::uint8_t> data = {'d', 'a', 't', 'a'};
    ByteReader reader(data);
    CHECK(reader.CString().empty());
    CHECK_FALSE(reader.ok());
  }
}

TEST_CASE("ByteReader::FixedString", "[core][byte_reader]") {
  SECTION("tronque au NUL mais consomme toute la largeur") {
    const std::vector<std::uint8_t> data = {'a', 'b', 0x00, 0x00, 0x00, 0x7F};
    ByteReader reader(data);
    CHECK(reader.FixedString(5) == "ab");
    CHECK(reader.offset() == 5);
    CHECK(reader.U8() == 0x7F);
  }

  SECTION("champ plein, sans NUL") {
    const std::vector<std::uint8_t> data = {'a', 'b', 'c'};
    ByteReader reader(data);
    CHECK(reader.FixedString(3) == "abc");
    CHECK(reader.remaining() == 0);
  }

  SECTION("champ plus large que le tampon") {
    const std::vector<std::uint8_t> data = {'a'};
    ByteReader reader(data);
    CHECK(reader.FixedString(4).empty());
    CHECK_FALSE(reader.ok());
  }
}

TEST_CASE("ByteReader::Invalidate lève le drapeau sans lire", "[core][byte_reader]") {
  const std::vector<std::uint8_t> data = Sample();
  ByteReader reader(data);
  CHECK(reader.ok());
  reader.Invalidate();
  CHECK_FALSE(reader.ok());
  CHECK(reader.offset() == 0);
}

TEST_CASE("ByteReader::TailString", "[core][byte_reader]") {
  SECTION("le zéro terminal ne fait pas partie du texte") {
    // ⚠ C'EST CE QUE LE SERVEUR ENVOIE : `clif_broadcast` annonce
    // `strlen(mes) + 1`, donc le zéro est DANS la longueur. Le garder, c'est le
    // transporter jusqu'au dessin, où il sort en glyphe « caractère manquant ».
    const std::array<std::uint8_t, 6> bytes = {'s', 'a', 'l', 'u', 't', 0};
    ro::core::ByteReader reader(bytes);
    CHECK(reader.TailString() == "salut");
    CHECK(reader.ok());
  }

  SECTION("sans zéro, tout est du texte") {
    const std::array<std::uint8_t, 3> bytes = {'o', 'u', 'i'};
    ro::core::ByteReader reader(bytes);
    CHECK(reader.TailString() == "oui");
  }

  SECTION("ce qui suit le zéro est du remplissage") {
    const std::array<std::uint8_t, 6> bytes = {'a', 0, 'z', 'z', 'z', 'z'};
    ro::core::ByteReader reader(bytes);
    CHECK(reader.TailString() == "a");
  }

  SECTION("une queue vide rend une chaîne vide, pas une panne") {
    const std::array<std::uint8_t, 1> bytes = {0};
    ro::core::ByteReader reader(bytes);
    reader.Skip(1);
    CHECK(reader.TailString().empty());
    CHECK(reader.ok());
  }
}

TEST_CASE("Lecteur : un compte à réserver ne dépasse pas les octets restants", "[core]") {
  // Un fichier de quarante octets qui annonce un million d'éléments ne doit
  // pas en faire réserver un million.
  constexpr std::size_t kAnnounced = 1'000'000;
  const std::array<std::uint8_t, 40> bytes{};
  ByteReader reader(bytes);
  reader.Skip(8);
  CHECK(reader.AtMostRemaining(kAnnounced) == 32);
  CHECK(reader.AtMostRemaining(5) == 5);
  // Un lecteur en échec n'a plus rien à offrir.
  reader.Skip(100);
  CHECK(reader.AtMostRemaining(kAnnounced) == 0);
}
