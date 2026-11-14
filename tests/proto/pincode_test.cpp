#include "ro/proto/pincode.h"

#include <array>
#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

using ro::proto::kPincodeLength;
using ro::proto::ObfuscatePincode;
using ro::proto::PincodeTable;

namespace {

// char_pincode_decrypt, du serveur, transcrite ligne à ligne depuis
// src/char/char.cpp. C'est le SEUL juge qui compte : brouiller n'a de sens que
// si la fonction d'en face rend le code d'origine.
//
// Elle est recopiée plutôt qu'appelée depuis le module : une paire
// chiffrement/déchiffrement écrite deux fois par la même main partagerait ses
// erreurs, et le test passerait sur du faux.
std::string ServerDecrypt(std::uint32_t user_seed, const std::string& pin) {
  char tab[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
  for (int i = 1; i < 10; i++) {
    const std::uint32_t multiplier = 0x3498;
    const std::uint32_t base_seed = 0x881234;
    user_seed = base_seed + user_seed * multiplier;
    const int pos = static_cast<int>(user_seed % static_cast<std::uint32_t>(i + 1));
    if (i != pos) {
      const char swap = tab[i];
      tab[i] = tab[pos];
      tab[pos] = swap;
    }
  }
  std::string clear;
  for (std::size_t i = 0; i < kPincodeLength; i++) {
    clear.push_back(static_cast<char>('0' + tab[pin[i] - '0']));
  }
  return clear;
}

}  // namespace

TEST_CASE("PIN : le serveur retrouve le code d'origine", "[proto][pincode]") {
  // Sur toutes les graines que le serveur peut tirer (rnd() % 0xFFFF) et tous
  // les codes de quatre chiffres, un balayage complet serait long ; un pas
  // premier traverse les deux domaines sans les parcourir.
  for (std::uint32_t seed = 0; seed < 0xFFFF; seed += 137) {
    for (int code = 0; code < 10000; code += 397) {
      std::string digits = std::to_string(code);
      digits.insert(0, kPincodeLength - digits.size(), '0');

      const auto sent = ObfuscatePincode(seed, digits);
      REQUIRE(sent.has_value());
      REQUIRE(sent->size() == kPincodeLength);
      CHECK(ServerDecrypt(seed, *sent) == digits);
    }
  }
}

TEST_CASE("PIN : le code ne voyage pas en clair", "[proto][pincode]") {
  // Un brouillage qui rendrait l'entrée telle quelle passerait le test
  // précédent — il serait sa propre inverse. Il existe des graines pour
  // lesquelles la permutation est l'identité ; ce qui compte est qu'elles ne
  // soient pas la règle.
  int changed = 0;
  for (std::uint32_t seed = 0; seed < 200; ++seed) {
    const auto sent = ObfuscatePincode(seed, "1234");
    REQUIRE(sent.has_value());
    if (*sent != "1234") {
      ++changed;
    }
  }
  CHECK(changed > 190);
}

TEST_CASE("PIN : la table est une permutation des dix chiffres", "[proto][pincode]") {
  // Si elle ne l'était pas, un chiffre n'aurait pas d'antécédent et le
  // brouillage en inventerait un.
  for (std::uint32_t seed = 0; seed < 500; ++seed) {
    const std::array<std::uint8_t, 10> table = PincodeTable(seed);
    std::array<bool, 10> seen{};
    for (const std::uint8_t digit : table) {
      REQUIRE(digit < 10);
      CHECK_FALSE(seen[digit]);
      seen[digit] = true;
    }
  }
}

TEST_CASE("PIN : une graine différente donne une trame différente", "[proto][pincode]") {
  // C'est tout l'objet du brouillage : rejouer une trame capturée ne rejoue
  // pas le code, parce que la graine a changé entre-temps.
  const auto first = ObfuscatePincode(1234, "9876");
  const auto second = ObfuscatePincode(5678, "9876");
  REQUIRE(first.has_value());
  REQUIRE(second.has_value());
  CHECK(*first != *second);
}

TEST_CASE("PIN : un code mal formé est refusé ici, pas sur le fil", "[proto][pincode]") {
  // Le serveur, lui, coupe la liaison sans un mot. Refuser avant d'émettre
  // permet de le dire à l'utilisateur au lieu de lui montrer une déconnexion.
  CHECK_FALSE(ObfuscatePincode(1, "123").has_value());
  CHECK_FALSE(ObfuscatePincode(1, "12345").has_value());
  CHECK_FALSE(ObfuscatePincode(1, "12a4").has_value());
  CHECK_FALSE(ObfuscatePincode(1, "").has_value());
}
