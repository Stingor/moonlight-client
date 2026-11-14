#include "chat/chat_layer_link.h"

#include <cstddef>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/i18n.h"
#include "ro/proto/chat_limits.h"
#include "ro/sim/style_layer_code.h"
#include "ro/ui/chat_link.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "chat/chat_item_link.h"

using moonclient::BuildLayerLink;
using moonclient::DecorateChatLine;
using moonclient::DecoratedChatLine;
using moonclient::LayerLink;
using moonclient::ParseLayerLink;
using ro::ui::ChatLinkKind;

namespace {

// Le code d'un chapeau dont la première pièce est réglée.
[[nodiscard]] std::string HatCode() {
  ro::sim::StyleLayerCode code;
  code.layer = ro::sim::StyleLayerKind::kHeadgear;
  code.profile = ro::sim::kStyleLayerCodeRampProfile;
  code.adjusts[0].hue = 40;
  return ro::sim::EncodeStyleLayerCode(code);
}

}  // namespace

TEST_CASE("Lien de calque : la balise s'écrit et se relit, le code tel quel", "[app][lien]") {
  const std::string code = HatCode();
  const std::string wire = BuildLayerLink("Weissbier Hat", code);
  CHECK(wire == "<LAYC>Weissbier Hat:" + code + "</LAYC>");
  LayerLink back;
  std::size_t after = 0;
  REQUIRE(ParseLayerLink(wire, &back, &after));
  CHECK(after == wire.size());
  CHECK(back.label == "Weissbier Hat");
  CHECK(back.code == code);
}

TEST_CASE("Lien de calque : un code illisible ne part ni ne se lit", "[app][lien]") {
  CHECK(BuildLayerLink("Weissbier Hat", "pas un code").empty());
  LayerLink back;
  std::size_t after = 0;
  const std::string forged = "<LAYC>Weissbier Hat:L9:zz</LAYC>";
  CHECK_FALSE(ParseLayerLink(forged, &back, &after));
  CHECK(after == forged.size());
}

TEST_CASE("Lien de calque : une balise trop longue pour une ligne ne part pas", "[app][lien]") {
  const std::string label(ro::proto::kChatTextMax, 'a');
  CHECK(BuildLayerLink(label, HatCode()).empty());
}

TEST_CASE("Lien de calque : la ligne reçue en fait un lien cliquable", "[app][lien]") {
  ro::core::UseFrench();
  const std::string code = HatCode();
  DecoratedChatLine out;
  REQUIRE(DecorateChatLine("vois <LAYC>Weissbier Hat:" + code + "</LAYC> !", ro::vfs::ItemTable{},
                           ro::vfs::ItemWords{}, &out, nullptr, nullptr, nullptr, nullptr));
  CHECK(out.text == "vois [Personnalisation : Weissbier Hat] !");
  REQUIRE(out.links.size() == 1);
  CHECK(out.links[0].kind == ChatLinkKind::kLayer);
  REQUIRE(out.carried.size() == 1);
  CHECK(out.carried[0].code == code);
}
