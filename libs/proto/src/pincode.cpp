#include "ro/proto/pincode.h"

#include <algorithm>
#include <utility>

namespace ro::proto {
namespace {

// Les deux constantes du brouillage, telles que char_pincode_decrypt les pose
// dans src/char/char.cpp. Elles n'ont pas de sens arithmétique : ce sont les
// paramètres d'un générateur congruentiel qui ne sert qu'à mélanger dix cases.
constexpr std::uint32_t kSeedMultiplier = 0x3498;
constexpr std::uint32_t kSeedBase = 0x881234;

constexpr std::size_t kDigitCount = 10;

}  // namespace

std::array<std::uint8_t, kDigitCount> PincodeTable(std::uint32_t seed) {
  std::array<std::uint8_t, kDigitCount> table{};
  for (std::size_t digit = 0; digit < kDigitCount; ++digit) {
    table[digit] = static_cast<std::uint8_t>(digit);
  }

  // Mélange de Fisher-Yates, mais en montant et sans la case zéro : c'est la
  // boucle du serveur, et s'en écarter d'un cran donnerait une autre
  // permutation, donc un code refusé.
  //
  // Le débordement de la multiplication est VOULU — le serveur calcule sur un
  // uint32, et l'arithmétique non signée boucle des deux côtés à l'identique.
  for (std::size_t index = 1; index < kDigitCount; ++index) {
    seed = kSeedBase + seed * kSeedMultiplier;
    const std::size_t position = seed % (index + 1);
    if (index != position) {
      std::swap(table[index], table[position]);
    }
  }
  return table;
}

core::Result<std::string> ObfuscatePincode(std::uint32_t seed, std::string_view digits) {
  if (digits.size() != kPincodeLength) {
    return std::unexpected(core::Error{core::ErrorCode::kInvalidArgument,
                                       "un code PIN fait exactement quatre chiffres"});
  }
  if (!std::all_of(digits.begin(), digits.end(),
                   [](char sign) { return sign >= '0' && sign <= '9'; })) {
    return std::unexpected(
        core::Error{core::ErrorCode::kInvalidArgument, "un code PIN ne contient que des chiffres"});
  }

  const std::array<std::uint8_t, kDigitCount> table = PincodeTable(seed);

  // Le serveur lit table[reçu] et attend le vrai chiffre. On envoie donc
  // l'ANTÉCÉDENT : la position où ce chiffre a atterri dans la table. La
  // recherche aboutit toujours — la table est une permutation des dix chiffres.
  std::string obfuscated;
  obfuscated.reserve(kPincodeLength);
  for (const char sign : digits) {
    const auto found = std::find(table.begin(), table.end(), static_cast<std::uint8_t>(sign - '0'));
    obfuscated.push_back(static_cast<char>('0' + (found - table.begin())));
  }
  return obfuscated;
}

}  // namespace ro::proto
