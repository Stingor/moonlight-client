// Les quatre aiguillages. Voir l'en-tête pour ce qui ne se traduit pas.

#include "world/entity_traits.h"

#include <cstddef>

#include "ro/core/i18n.h"
#include "ro/vfs/monster_words.h"

namespace moonclient {

namespace {

// Les indices que le serveur range après les mots de la fiche d'un monstre.
constexpr std::int16_t kElementAll = 10;
constexpr std::int16_t kRacePlayer = 10;
constexpr std::int16_t kRaceDoramPlayer = 11;
constexpr std::int16_t kRaceAll = 12;
constexpr std::int16_t kSizeAll = 3;

}  // namespace

// `e_element` : du neutre au mort-vivant (les mots de la table), puis « tous
// éléments ».
std::string_view ElementName(const ro::vfs::ItemWords& words, std::int16_t index) {
  if (index >= 0 && static_cast<std::size_t>(index) < ro::vfs::kMonsterElementCount) {
    return ro::vfs::MonsterElementWord(words, static_cast<std::uint8_t>(index));
  }
  if (index == kElementAll) {
    return ro::core::Tr("all elements");
  }
  return kUnnamedTarget;
}
// `e_race`. ⚠ PAS DE « BOSS » ICI : le serveur range Boss dans la CLASSE de
// monstre, qui est une autre famille — un boss a par ailleurs sa race.
std::string_view RaceName(const ro::vfs::ItemWords& words, std::int16_t index) {
  if (index >= 0 && static_cast<std::size_t>(index) < ro::vfs::kMonsterRaceCount) {
    return ro::vfs::MonsterRaceWord(words, static_cast<std::uint8_t>(index));
  }
  switch (index) {
    case kRacePlayer:
      return ro::core::Tr("Player");
    case kRaceDoramPlayer:
      return ro::core::Tr("Doram Player");
    case kRaceAll:
      return ro::core::Tr("all races");
    default:
      return kUnnamedTarget;
  }
}
// `e_size`.
std::string_view SizeName(const ro::vfs::ItemWords& words, std::int16_t index) {
  if (index >= 0 && static_cast<std::size_t>(index) < ro::vfs::kMonsterSizeCount) {
    return ro::vfs::MonsterSizeWord(words, static_cast<std::uint8_t>(index));
  }
  if (index == kSizeAll) {
    return ro::core::Tr("all sizes");
  }
  return kUnnamedTarget;
}
// `e_aegis_monsterclass`. ⚠ L'INDEX 3 EST UN TROU dans l'énumération du
// serveur : il n'a pas de nom, et se lit donc « ? » plutôt que de décaler les
// suivants.
std::string_view MonsterClassName(std::int16_t index) {
  switch (index) {
    case 0:
      return ro::core::Tr("Normal");
    case 1:
      return ro::core::Tr("Boss");
    case 2:
      return ro::core::Tr("Guardian");
    case 3:
      return ro::core::Tr("?");
    case 4:
      return ro::core::Tr("Battlefield");
    case 5:
      return ro::core::Tr("Event");
    case 6:
      return ro::core::Tr("all classes");
    default:
      return kUnnamedTarget;
  }
}

}  // namespace moonclient
