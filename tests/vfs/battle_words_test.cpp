// LES LIGNES DE COMBAT : les phrases de la table de messages du client, et
// rien d'autre — une clé absente donne un texte vide, jamais un libellé anglais.

#include "ro/vfs/battle_words.h"

#include <catch2/catch_test_macros.hpp>

#include "ro/vfs/item_words.h"

using ro::vfs::BattleLine;
using ro::vfs::BattleLineText;
using ro::vfs::FormatTakesExactly;
using ro::vfs::ItemWords;

TEST_CASE("Lignes de combat : sans table, rien — et surtout pas de texte anglais en dur",
          "[vfs][battle]") {
  const ItemWords words;
  CHECK(ro::vfs::SomeoneText(words).empty());
  constexpr int kDamage = 123;
  CHECK(BattleLineText(words, BattleLine::kIReceived, "Poring", {}, kDamage).empty());
  CHECK(BattleLineText(words, BattleLine::kPartyReceived, "Ami", "Poring", kDamage).empty());
  CHECK(BattleLineText(words, BattleLine::kIGave, "Poring", {}, kDamage).empty());
  CHECK(BattleLineText(words, BattleLine::kPartyGave, "Ami", "Poring", kDamage).empty());
}

TEST_CASE("Lignes de combat : les phrases de la table s'impriment avec leurs noms",
          "[vfs][battle]") {
  ItemWords words;
  words.battle.someone = "Quelqu'un";
  words.battle.i_received = " [%s] vous inflige '%d'.";
  words.battle.party_received = " [%s] subit de [%s] '%d'.";
  words.battle.i_gave = " [%s] reçoit '%d'.";
  words.battle.party_gave = " [%s] frappe [%s] de '%d'.";
  constexpr int kDamage = 123;
  CHECK(ro::vfs::SomeoneText(words) == "Quelqu'un");
  CHECK(BattleLineText(words, BattleLine::kIReceived, "Poring", {}, kDamage) ==
        " [Poring] vous inflige '123'.");
  CHECK(BattleLineText(words, BattleLine::kPartyReceived, "Ami", "Poring", kDamage) ==
        " [Ami] subit de [Poring] '123'.");
  CHECK(BattleLineText(words, BattleLine::kIGave, "Poring", {}, kDamage) ==
        " [Poring] reçoit '123'.");
  CHECK(BattleLineText(words, BattleLine::kPartyGave, "Ami", "Poring", kDamage) ==
        " [Ami] frappe [Poring] de '123'.");
}

TEST_CASE("Lignes de combat : la table de l'installation passe, si sa forme est sûre",
          "[vfs][battle]") {
  ItemWords words;
  constexpr int kDamage = 7;
  words.battle.i_gave = "%s prend %d.";
  CHECK(BattleLineText(words, BattleLine::kIGave, "Poring", {}, kDamage) == "Poring prend 7.");
  // Une forme qui réclamerait autre chose ne s'imprime pas : la ligne est vide.
  words.battle.i_gave = "%d prend %s.";
  CHECK(BattleLineText(words, BattleLine::kIGave, "Poring", {}, kDamage).empty());
  CHECK(FormatTakesExactly("[%s] et [%s] : %i %%", "ssd"));
  CHECK_FALSE(FormatTakesExactly("%s %d %d", "sd"));
  CHECK_FALSE(FormatTakesExactly("%s %x", "sd"));
  CHECK_FALSE(FormatTakesExactly("%s %", "s"));
}
