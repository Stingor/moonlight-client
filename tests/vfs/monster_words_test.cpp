// LES MOTS DE LA FICHE D'UN MONSTRE, des textes de l'interface.
//
// 🔴 UNE VALEUR HORS BORNES S'ÉCRIT « ? », et l'élément s'indexe modulo vingt,
// comme le client.

#include "ro/vfs/monster_words.h"

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/vfs/item_words.h"

using ro::core::Tr;
using ro::vfs::ItemWords;
using ro::vfs::MonsterElementWord;
using ro::vfs::MonsterLabel;
using ro::vfs::MonsterLabelWord;
using ro::vfs::MonsterRaceWord;
using ro::vfs::MonsterSizeWord;

namespace {

// La race « humanoïde », l'élément sacré, la grande taille.
constexpr std::uint8_t kDemiHuman = 7;
constexpr std::uint8_t kHoly = 6;
constexpr std::uint8_t kLarge = 2;
// Le codage historique « élément + 20 × niveau » : le sacré de niveau deux.
constexpr std::uint8_t kHolyCodedLevelTwo = kHoly + 40;

}  // namespace

TEST_CASE("Mots d'un monstre : les mots de la fiche sont des textes de l'interface",
          "[vfs][mots][monster_sheet]") {
  ItemWords words;
  ro::vfs::FillMessageWords(words);
  CHECK(MonsterRaceWord(words, kDemiHuman) == Tr("Demi-humain"));
  CHECK(MonsterElementWord(words, kHoly) == Tr("Sacré"));
  CHECK(MonsterSizeWord(words, kLarge) == Tr("Grande"));
  CHECK(MonsterLabelWord(words, MonsterLabel::kLevel) == Tr("Niveau"));
}

TEST_CASE("Mots d'un monstre : vide sans la clé, et « ? » hors bornes",
          "[vfs][mots][monster_sheet]") {
  // Aucun libellé en dur : des mots qu'aucun remplissage n'a posés sont vides.
  const ItemWords none;
  CHECK(MonsterRaceWord(none, kDemiHuman).empty());
  CHECK(MonsterElementWord(none, kHoly).empty());
  CHECK(MonsterSizeWord(none, kLarge).empty());
  CHECK(MonsterLabelWord(none, MonsterLabel::kMdef).empty());
  CHECK(MonsterLabelWord(none, MonsterLabel::kLevel).empty());
  CHECK(MonsterRaceWord(none, ro::vfs::kMonsterRaceCount) == "?");
  CHECK(MonsterSizeWord(none, ro::vfs::kMonsterSizeCount) == "?");
}

TEST_CASE("Mots d'un monstre : l'élément s'indexe modulo vingt", "[vfs][mots][monster_sheet]") {
  ItemWords words;
  words.monster.element[kHoly] = "Holy";
  CHECK(MonsterElementWord(words, kHolyCodedLevelTwo) == "Holy");
}
