#include "ro/core/sha256.h"

#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::core::Sha256;

namespace {

std::string HashOf(std::string_view text) {
  const std::span<const std::uint8_t> bytes(reinterpret_cast<const std::uint8_t*>(text.data()),
                                            text.size());
  return Sha256::ToHex(Sha256::Hash(bytes));
}

}  // namespace

TEST_CASE("SHA-256 sur les vecteurs de référence", "[core][sha256]") {
  // Vecteurs publiés de FIPS 180-4. Ils couvrent les trois cas de remplissage :
  // message vide, message court, et message de 56 octets — la longueur exacte
  // qui force un second bloc, là où un remplissage mal écrit se trahit.
  CHECK(HashOf("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  CHECK(HashOf("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  CHECK(HashOf("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
        "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST_CASE("SHA-256 sur un message d'un million d'octets", "[core][sha256]") {
  // Le vecteur « un million de 'a' » de FIPS 180-4 : il exerce la boucle de
  // blocs sur plus de 15 000 itérations, ce qu'aucun message court ne fait.
  const std::vector<std::uint8_t> million(1'000'000, static_cast<std::uint8_t>('a'));
  CHECK(Sha256::ToHex(Sha256::Hash(million)) ==
        "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE("SHA-256 en flux découpé donne la même empreinte", "[core][sha256]") {
  // La propriété qui compte réellement pour l'usage prévu : roc-cli hache un
  // fichier par blocs d'un mébioctet, et l'intégrité du client hachera une DLL
  // de la même façon. Une erreur de gestion du bloc partiel ne se voit QUE là.
  std::vector<std::uint8_t> payload;
  payload.reserve(5000);
  for (std::size_t index = 0; index < 5000; ++index) {
    payload.push_back(static_cast<std::uint8_t>(index * 31 + 7));
  }
  const std::string one_shot = Sha256::ToHex(Sha256::Hash(payload));

  // Des tailles de tranche volontairement premières entre elles avec 64, pour
  // que le bloc partiel tombe à un endroit différent à chaque tour.
  for (const std::size_t chunk_size : {1U, 7U, 63U, 64U, 65U, 127U, 1000U}) {
    Sha256 hasher;
    for (std::size_t offset = 0; offset < payload.size(); offset += chunk_size) {
      const std::size_t take = std::min(chunk_size, payload.size() - offset);
      hasher.Update(std::span<const std::uint8_t>(payload.data() + offset, take));
    }
    INFO("tranches de " << chunk_size << " octets");
    CHECK(Sha256::ToHex(hasher.Finish()) == one_shot);
  }
}

TEST_CASE("SHA-256 : le remplissage traverse toutes les frontières de bloc", "[core][sha256]") {
  // La frontière qui casse les remplissages mal écrits est à 56 octets modulo
  // 64 : au-delà, la longueur encodée ne tient plus dans le bloc courant et il
  // en faut un second. Le vecteur FIPS de 56 octets ci-dessus l'atteste sur une
  // valeur publiée ; ce test-ci balaie TOUTES les longueurs de 0 à 200.
  //
  // Il ne vérifie pas une empreinte connue — il vérifie que le chemin « un seul
  // Update » et le chemin « un Update par octet » convergent, à chaque longueur.
  // Un remplissage qui se déclenche au mauvais moment fait diverger les deux.
  for (std::size_t length = 0; length <= 200; ++length) {
    std::vector<std::uint8_t> payload(length);
    for (std::size_t index = 0; index < length; ++index) {
      payload[index] = static_cast<std::uint8_t>(index);
    }

    Sha256 byte_by_byte;
    for (const std::uint8_t byte : payload) {
      byte_by_byte.Update(std::span<const std::uint8_t>(&byte, 1));
    }

    INFO("longueur " << length);
    CHECK(Sha256::ToHex(byte_by_byte.Finish()) == Sha256::ToHex(Sha256::Hash(payload)));
  }
}

TEST_CASE("SHA-256 : l'encodage hexadécimal", "[core][sha256]") {
  const Sha256::Digest digest = Sha256::Hash(std::span<const std::uint8_t>{});
  const std::string hex = Sha256::ToHex(digest);
  CHECK(hex.size() == Sha256::kDigestSize * 2);
  // Minuscules, jamais de majuscules : c'est la forme que le serveur compare
  // dans son contrôle d'intégrité.
  CHECK(hex.find_first_not_of("0123456789abcdef") == std::string::npos);
}
