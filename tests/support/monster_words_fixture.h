#pragma once

#include <array>
#include <cstddef>
#include <string_view>

#include "ro/vfs/item_words.h"

namespace ro::test_support {

// Une table de messages synthétique : les mots de race, d'élément et de taille
// d'une langue, dans l'ordre du serveur. Aucun texte du jeu n'est versionné, et
// le code testé ne porte aucun repli : sans table, ces mots sont vides.
struct MonsterWordSet {
  std::array<std::string_view, ro::vfs::kMonsterRaceCount> races;
  std::array<std::string_view, ro::vfs::kMonsterElementCount> elements;
  std::array<std::string_view, ro::vfs::kMonsterSizeCount> sizes;
};

[[nodiscard]] inline ro::vfs::ItemWords MonsterWordsOf(const MonsterWordSet& set) {
  ro::vfs::ItemWords words;
  for (std::size_t rank = 0; rank < set.races.size(); ++rank) {
    words.monster.race[rank] = set.races[rank];
  }
  for (std::size_t rank = 0; rank < set.elements.size(); ++rank) {
    words.monster.element[rank] = set.elements[rank];
  }
  for (std::size_t rank = 0; rank < set.sizes.size(); ++rank) {
    words.monster.size[rank] = set.sizes[rank];
  }
  return words;
}

// Les mots de l'installation anglaise.
[[nodiscard]] inline ro::vfs::ItemWords EnglishMonsterWords() {
  return MonsterWordsOf(MonsterWordSet{.races = {"Formless", "Undead", "Brute", "Plant", "Insect",
                                                 "Fish", "Demon", "Demi-Human", "Angel", "Dragon"},
                                       .elements = {"Neutral", "Water", "Earth", "Fire", "Wind",
                                                    "Poison", "Holy", "Shadow", "Ghost", "Undead"},
                                       .sizes = {"Small", "Medium", "Large"}});
}

// Les mots d'une installation française.
[[nodiscard]] inline ro::vfs::ItemWords FrenchMonsterWords() {
  return MonsterWordsOf(
      MonsterWordSet{.races = {"Amorphe", "Mort-vivant", "Brute", "Plante", "Insecte", "Poisson",
                               "Demon", "Demi-Humain", "Ange", "Dragon"},
                     .elements = {"Neutre", "Eau", "Terre", "Feu", "Vent", "Poison", "Saint",
                                  "Tenebres", "Fantome", "Mort-vivant"},
                     .sizes = {"Petit", "Moyen", "Grand"}});
}

}  // namespace ro::test_support
