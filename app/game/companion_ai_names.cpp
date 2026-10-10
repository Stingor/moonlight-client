// Voir l'en-tête.

#include "game/companion_ai_names.h"

#include <array>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/sim/companion_ai.h"

#include "game/companion_ai_file.h"

namespace moonclient {
namespace {

using ro::sim::kGuildMercenaryCount;
using ro::sim::kGuildMercenaryRanks;
using ro::sim::kHomunculusBasicTypeCount;
using ro::sim::kHomunculusFirstMutatedType;
using ro::sim::kHomunculusJobBase;
using ro::sim::kHomunculusLastType;
using ro::sim::kHomunculusLastUnevolvedType;
using ro::sim::kHomunculusSpeciesPerLine;
using ro::sim::kMercenaryJobBase;

constexpr std::size_t kNameBytes = 64;
constexpr int kGuildMercenaryLines = kGuildMercenaryCount / kGuildMercenaryRanks;
constexpr int kMutatedHomunculusCount = kHomunculusLastType - kHomunculusFirstMutatedType + 1;

// Les noms du jeu, qui restent en anglais.
constexpr std::array<const char*, kHomunculusSpeciesPerLine> kSpecies = {"Lif", "Amistr", "Filir",
                                                                         "Vanilmirth"};
constexpr std::array<const char*, kMutatedHomunculusCount> kMutated = {"Eira", "Bayeri", "Sera",
                                                                       "Dieter", "Eleanor"};
constexpr std::array<const char*, kGuildMercenaryLines> kMercenaryLineNames = {"Archer", "Lancer",
                                                                               "Swordman"};

}  // namespace

std::string CompanionClassName(std::uint16_t job) {
  std::array<char, kNameBytes> name{};
  const int homunculus_type = static_cast<int>(job) - kHomunculusJobBase;
  const int mercenary_type = static_cast<int>(job) - kMercenaryJobBase;
  if (mercenary_type >= 1 && mercenary_type <= kGuildMercenaryCount) {
    std::snprintf(
        name.data(), name.size(), ro::core::Tr("%s, rang %d"),
        kMercenaryLineNames[static_cast<std::size_t>((mercenary_type - 1) / kGuildMercenaryRanks)],
        ((mercenary_type - 1) % kGuildMercenaryRanks) + 1);
  } else if (homunculus_type >= kHomunculusFirstMutatedType &&
             homunculus_type <= kHomunculusLastType) {
    std::snprintf(
        name.data(), name.size(), "%s",
        kMutated[static_cast<std::size_t>(homunculus_type - kHomunculusFirstMutatedType)]);
  } else if (homunculus_type >= 1 && homunculus_type <= kHomunculusBasicTypeCount) {
    const char* species =
        kSpecies[static_cast<std::size_t>((homunculus_type - 1) % kHomunculusSpeciesPerLine)];
    if (homunculus_type > kHomunculusLastUnevolvedType) {
      std::snprintf(name.data(), name.size(), ro::core::Tr("%s évolué"), species);
    } else {
      std::snprintf(name.data(), name.size(), "%s", species);
    }
  } else {
    std::snprintf(name.data(), name.size(), ro::core::Tr("Classe %u"), static_cast<unsigned>(job));
  }
  return name.data();
}

std::string ProfileLabelOf(std::string_view key) {
  if (key == kBasicProfileName) {
    return ro::core::Tr("IA basique");
  }
  if (key == kOwnProfileName) {
    return ro::core::Tr("Mon profil");
  }
  return std::string(key);
}

std::string ProfileRefusalText(ProfileRefusal refusal) {
  switch (refusal) {
    case ProfileRefusal::kNone:
      return std::string();
    case ProfileRefusal::kEmptyName:
      return ro::core::Tr("Tapez un nom de profil.");
    case ProfileRefusal::kNameTooLong:
      return ro::core::Tr("Ce nom de profil est trop long.");
    case ProfileRefusal::kBadCharacter:
      return ro::core::Tr("Ce nom de profil porte un caractère interdit.");
    case ProfileRefusal::kReservedName:
      return ro::core::Tr("Ce nom est réservé à un profil d'usine.");
    case ProfileRefusal::kNameTaken:
      return ro::core::Tr("Un profil de ce type porte déjà ce nom.");
    case ProfileRefusal::kTypeFull:
      return ro::core::Tr("Ce type a déjà tous ses profils.");
    case ProfileRefusal::kUnknownProfile:
      return ro::core::Tr("Ce profil n'existe plus.");
    case ProfileRefusal::kBasicLocked:
      return ro::core::Tr("L'IA basique ne se renomme ni ne se supprime.");
  }
  return std::string();
}

}  // namespace moonclient
