#include "ro/vfs/monster_words.h"

#include <array>
#include <cstddef>

#include "ro/core/i18n.h"

namespace ro::vfs {
namespace {

// Ce que la fiche écrit d'une valeur qu'elle ne sait pas nommer.
constexpr std::string_view kUnnamedValue = "?";

// Le modulo du codage historique de l'élément.
constexpr std::uint8_t kElementCodingModulo = 20;

template <std::size_t kCount>
[[nodiscard]] std::string_view Indexed(const std::array<std::string, kCount>& read,
                                       std::size_t index) {
  if (index >= kCount) {
    return kUnnamedValue;
  }
  return read[index];
}

}  // namespace

std::string_view MonsterRaceWord(const ItemWords& words, std::uint8_t race) {
  return Indexed(words.monster.race, race);
}

std::string_view MonsterElementWord(const ItemWords& words, std::uint8_t element) {
  return Indexed(words.monster.element, element % kElementCodingModulo);
}

std::string_view MonsterSizeWord(const ItemWords& words, std::uint8_t size) {
  return Indexed(words.monster.size, size);
}

std::string_view MonsterLabelWord(const ItemWords& words, MonsterLabel label) {
  const MonsterWords& said = words.monster;
  switch (label) {
    case MonsterLabel::kLevel:
      return said.level_label;
    case MonsterLabel::kRace:
      return said.race_label;
    case MonsterLabel::kElement:
      return said.element_label;
    case MonsterLabel::kSize:
      return said.size_label;
    case MonsterLabel::kDef:
      return said.def_label;
    case MonsterLabel::kMdef:
      return said.mdef_label;
  }
  return kUnnamedValue;
}

void FillMonsterWords(MonsterWords& words) {
  // DANS L'ORDRE DU SERVEUR : c'est le rang qui indexe.
  words.race = {core::Tr("Informe"), core::Tr("Mort-vivant"), core::Tr("Bête"),
                core::Tr("Plante"),  core::Tr("Insecte"),     core::Tr("Poisson"),
                core::Tr("Démon"),   core::Tr("Demi-humain"), core::Tr("Ange"),
                core::Tr("Dragon")};
  // L'ordre des constantes `ELE_*`, que les résistances suivent aussi.
  words.element = {core::Tr("Neutre"),     core::Tr("Eau"),   core::Tr("Terre"),
                   core::Tr("Feu"),        core::Tr("Vent"),  core::Tr("Poison"),
                   core::Tr("Sacré"),      core::Tr("Ombre"), core::Tr("Fantôme"),
                   core::Tr("Mort-vivant")};
  words.size = {core::Tr("Petite"), core::Tr("Moyenne"), core::Tr("Grande")};
  words.level_label = core::Tr("Niveau");
  words.race_label = core::Tr("Race");
  words.element_label = core::Tr("Élément");
  words.size_label = core::Tr("Taille");
  words.def_label = core::Tr("DEF");
  words.mdef_label = core::Tr("MDEF");
}

}  // namespace ro::vfs
