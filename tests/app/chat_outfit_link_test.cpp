#include "chat/chat_outfit_link.h"

#include <cstddef>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/ui/chat_link.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "chat/chat_item_link.h"

using moonclient::BuildOutfitLink;
using moonclient::DecorateChatLine;
using moonclient::DecoratedChatLine;
using moonclient::OutfitLink;
using moonclient::ParseOutfitLink;
using ro::ui::ChatLinkKind;

TEST_CASE("Lien de tenue : la balise s'écrit et se relit, code normalisé", "[app][lien]") {
  const std::string wire = BuildOutfitLink("Stingor (Soirée)", "k7qm-3xpa");
  CHECK(wire == "<OUTF>Stingor (Soirée):K7QM3XPA</OUTF>");
  OutfitLink back;
  std::size_t after = 0;
  REQUIRE(ParseOutfitLink(wire, &back, &after));
  CHECK(after == wire.size());
  CHECK(back.label == "Stingor (Soirée)");
  CHECK(back.code == "K7QM3XPA");
}

TEST_CASE("Lien de tenue : un code qui n'en est pas un ne part ni ne se lit", "[app][lien]") {
  CHECK(BuildOutfitLink("Stingor", "trop-court").empty());
  OutfitLink back;
  std::size_t after = 0;
  CHECK_FALSE(ParseOutfitLink("<OUTF>Stingor:abc</OUTF>", &back, &after));
  // ⚠ LA BALISE ENTIÈRE SE SAUTE quand même : l'appelant ne repart pas dessus.
  CHECK(after == std::string("<OUTF>Stingor:abc</OUTF>").size());
  // Tronquée : rien à sauter.
  CHECK_FALSE(ParseOutfitLink("<OUTF>Stingor:K7QM3XPA", &back, &after));
  CHECK(after == 0);
}

TEST_CASE("Lien de tenue : une étiquette qui couperait la balise est nettoyée", "[app][lien]") {
  const std::string wire = BuildOutfitLink("a:b<c", "K7QM3XPA");
  OutfitLink back;
  REQUIRE(ParseOutfitLink(wire, &back, nullptr));
  CHECK(back.label == "a b c");
  CHECK(back.code == "K7QM3XPA");
}

TEST_CASE("Lien de tenue : la ligne reçue en fait un lien cliquable", "[app][lien]") {
  ro::core::UseFrench();
  DecoratedChatLine out;
  REQUIRE(DecorateChatLine("vois <OUTF>Stingor (Soirée):K7QM3XPA</OUTF> !", ro::vfs::ItemTable{},
                           ro::vfs::ItemWords{}, &out, nullptr, nullptr, nullptr, nullptr));
  CHECK(out.text == "vois [Tenue : Stingor (Soirée)] !");
  REQUIRE(out.links.size() == 1);
  CHECK(out.links[0].kind == ChatLinkKind::kOutfit);
  REQUIRE(out.carried.size() == 1);
  CHECK(out.carried[0].code == "K7QM3XPA");
  CHECK(out.carried[0].owner == "Stingor (Soirée)");
}
