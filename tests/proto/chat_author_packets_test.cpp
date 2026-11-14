// L'auteur d'une ligne de parole (`ZC_MOONLIGHT_CHAT_AUTHOR 0x0F3A`) : le bloc
// fixe engendré, et la queue — le nom d'un canal — lue à la main.

#include "ro/proto/chat_author_packets.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::proto::ChatAuthorNotice;
using ro::proto::ChatAuthorScope;

namespace {

constexpr std::uint16_t kGuildChatOpcode = 0x017F;
constexpr std::uint16_t kChannelOpcode = 0x02C1;

[[nodiscard]] ChatAuthorNotice SomeAuthor(ChatAuthorScope scope) {
  ChatAuthorNotice notice;
  notice.fixed.next_opcode = kGuildChatOpcode;
  notice.fixed.scope = static_cast<std::uint8_t>(scope);
  notice.fixed.channel_type = ro::proto::kChatAuthorNoChannelType;
  notice.fixed.account_id = 2'000'001;
  notice.fixed.char_id = 150'001;
  notice.fixed.header_len = 6;
  notice.fixed.name = "Bob";
  return notice;
}

}  // namespace

TEST_CASE("Auteur de parole : le bloc fixe fait 44 octets, et se relit", "[proto][chat_author]") {
  const std::vector<std::uint8_t> bytes = Encode(SomeAuthor(ChatAuthorScope::kGuild));
  REQUIRE(bytes.size() == ro::proto::ZcMoonlightChatAuthor::kWireSize);
  CHECK(bytes.size() == 44);

  ChatAuthorNotice read;
  REQUIRE(Decode(bytes, read));
  CHECK(read.fixed.next_opcode == kGuildChatOpcode);
  CHECK(read.fixed.account_id == 2'000'001);
  CHECK(read.fixed.char_id == 150'001);
  CHECK(read.fixed.header_len == 6);
  CHECK(read.fixed.name == "Bob");
  CHECK(read.channel.empty());
}

TEST_CASE("Auteur de parole : un canal porte son nom en queue", "[proto][chat_author]") {
  ChatAuthorNotice channel = SomeAuthor(ChatAuthorScope::kChannel);
  channel.fixed.next_opcode = kChannelOpcode;
  channel.fixed.channel_type = ro::proto::kChatAuthorAllyChannelType;
  channel.channel = "ally";
  const std::vector<std::uint8_t> bytes = Encode(channel);
  REQUIRE(bytes.size() == 48);
  // La longueur annoncée compte la queue.
  CHECK(bytes[2] == 48);

  ChatAuthorNotice read;
  REQUIRE(Decode(bytes, read));
  CHECK(read.channel == "ally");
  CHECK(read.fixed.channel_type == ro::proto::kChatAuthorAllyChannelType);
}

TEST_CASE("Auteur de parole : les bornes", "[proto][chat_author]") {
  ChatAuthorNotice read;
  const std::vector<std::uint8_t> whole = Encode(SomeAuthor(ChatAuthorScope::kGuild));

  // Plus court que le bloc fixe.
  const std::vector<std::uint8_t> cut(whole.begin(), whole.end() - 1);
  CHECK_FALSE(Decode(cut, read));

  // Une queue hors d'un canal : la disposition a changé sans nous.
  std::vector<std::uint8_t> tailed = whole;
  tailed.push_back('x');
  CHECK_FALSE(Decode(tailed, read));

  // Un nom de canal qui porte un NUL.
  ChatAuthorNotice channel = SomeAuthor(ChatAuthorScope::kChannel);
  channel.channel = "al";
  std::vector<std::uint8_t> with_nul = Encode(channel);
  with_nul.back() = 0;
  CHECK_FALSE(Decode(with_nul, read));

  // Un nom vide, une portée inconnue.
  ChatAuthorNotice nameless = SomeAuthor(ChatAuthorScope::kGuild);
  nameless.fixed.name.clear();
  CHECK_FALSE(Decode(Encode(nameless), read));
  ChatAuthorNotice stray = SomeAuthor(ChatAuthorScope::kGuild);
  stray.fixed.scope = 0;
  CHECK_FALSE(Decode(Encode(stray), read));
  stray.fixed.scope = static_cast<std::uint8_t>(ChatAuthorScope::kChannel) + 1;
  CHECK_FALSE(Decode(Encode(stray), read));
}
