// LES MOTS DES COMMERCES : posés dans l'ordre de leur énumération, et imprimés sans
// la libc.
//
// 🔴 CE BANC PORTE LA SEULE CHOSE QUI SOIT DANGEREUSE ICI : un gabarit dont les
// conversions ne sont pas celles qu'on attend. Il doit rendre un texte VIDE,
// jamais une impression.

#include "ro/vfs/trade_words.h"

#include <cstddef>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/vfs/item_words.h"

using ro::core::Tr;
using ro::vfs::ItemWords;
using ro::vfs::TradeFormatShape;
using ro::vfs::TradeText;
using ro::vfs::TradeTextWith;
using ro::vfs::TradeWord;

namespace {

void Say(ItemWords& words, TradeWord word, std::string_view text) {
  words.trade.texts[static_cast<std::size_t>(word)] = std::string(text);
}

}  // namespace

TEST_CASE("Mots des commerces : le remplissage pose chaque rang à sa place", "[vfs][trade_words]") {
  // 🔴 UN RANG DÉCALÉ NE SE VERRAIT PAS : il afficherait un autre texte
  // plausible. Le premier, un du milieu et le dernier tiennent l'ordre.
  ItemWords words;
  ro::vfs::FillTradeWords(words.trade);
  CHECK(TradeText(words, TradeWord::kDealSucceeded) == Tr("L'échange a été effectué."));
  CHECK(TradeText(words, TradeWord::kBuyOneAtATime) ==
        Tr("Vous ne pouvez acheter qu'un item à la fois."));
  CHECK(TradeText(words, TradeWord::kSearchNextPage) == Tr("Suiv."));
  for (std::size_t rank = 0; rank < ro::vfs::kTradeWordCount; ++rank) {
    CHECK_FALSE(words.trade.texts[rank].empty());
  }
}

TEST_CASE("Mots des commerces : la forme d'un gabarit", "[vfs][trade_words]") {
  CHECK(TradeFormatShape("You have sold %s. Amount: %d. Total Zeny: %dz") == "sdd");
  CHECK(TradeFormatShape("100%% sure, %s") == "s");
  CHECK(TradeFormatShape("no conversion") == "");
  CHECK(TradeFormatShape("%5d") == "?");
  CHECK(TradeFormatShape("%n") == "?");
  CHECK(TradeFormatShape("dangling %") == "");
}

TEST_CASE("Mots des commerces : l'impression suit le gabarit, ou se tait", "[vfs][trade_words]") {
  ItemWords words;
  Say(words, TradeWord::kBuyingStoreSold, "You have sold %s. Amount: %d. Total Zeny: %dz");
  CHECK(TradeTextWith(words, TradeWord::kBuyingStoreSold, "Jellopy", 3, 150) ==
        "You have sold Jellopy. Amount: 3. Total Zeny: 150z");

  Say(words, TradeWord::kItemSold, "%s %d sold.");
  CHECK(TradeTextWith(words, TradeWord::kItemSold, "+7 Knife", 1) == "+7 Knife 1 sold.");
  // Les arguments dans le désordre : refusé, pas imprimé.
  Say(words, TradeWord::kItemSold, "%d %s sold.");
  CHECK(TradeTextWith(words, TradeWord::kItemSold, "+7 Knife", 1).empty());
  // Une conversion inconnue : refusé.
  Say(words, TradeWord::kLimitBuyItem, "up to %x pieces");
  CHECK(TradeTextWith(words, TradeWord::kLimitBuyItem, 30000).empty());
}
