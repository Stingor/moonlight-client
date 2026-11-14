// Le relai du canal Discord (`ZC_MOONLIGHT_DISCORD_MSG 0x0F08`).
//
// Ce qui se vérifie ici, c'est une lecture que le décodeur engendré fait
// FAUSSE — il ne prend qu'un octet du message —, et un filtre que le serveur
// ne fait pas : il envoie à toute la carte, réglage coché ou non.

#include "ro/net/discord_relay.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/world.h"

#include "support/wire_bytes.h"

using ro::net::ApplyDiscordRelayFrame;
using ro::net::FindDiscordRelayAuthor;
using ro::net::FrameOutcome;
using ro::net::IsDiscordRelayMap;
using ro::net::ReadDiscordRelayText;
using ro::test_support::PutU16;
using ro::test_support::PutU64;

namespace {

constexpr std::uint16_t kOpcode = 0x0F08;
constexpr std::uint32_t kSaidMs = 1234;
constexpr std::uint16_t kGonryunX = 100;
constexpr std::uint16_t kGonryunY = 120;

// La trame telle que le serveur l'écrit : [op][len = 4 + n + 1][n octets][NUL].
[[nodiscard]] std::vector<std::uint8_t> RelayBytes(std::string_view text) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, kOpcode);
  PutU16(bytes, static_cast<std::uint16_t>(ro::net::kDiscordRelayHeader + text.size() + 1));
  bytes.insert(bytes.end(), text.begin(), text.end());
  bytes.push_back(0);
  return bytes;
}

[[nodiscard]] ro::net::Frame RelayFrame(std::string_view text) {
  return ro::net::Frame{kOpcode, RelayBytes(text)};
}

}  // namespace

TEST_CASE("Relai Discord : le texte se lit jusqu'au NUL", "[net][discord]") {
  std::vector<std::uint8_t> bytes = RelayBytes("abc");
  // Ce qui suit le NUL n'est pas du message, même compté dans la longueur.
  bytes.insert(bytes.end() - 1, {0, 'x', 'y'});
  bytes[2] = static_cast<std::uint8_t>(bytes.size());
  const auto text = ReadDiscordRelayText(bytes);
  REQUIRE(text.has_value());
  CHECK(*text == "abc");
}

TEST_CASE("Relai Discord : une trame de 250 octets est coupée à 243", "[net][discord]") {
  const std::string long_text(250, 'a');
  const auto text = ReadDiscordRelayText(RelayBytes(long_text));
  REQUIRE(text.has_value());
  CHECK(text->size() == ro::net::kDiscordRelayTextMax);
}

TEST_CASE("Relai Discord : une séquence coupée à la borne se perd seule", "[net][discord]") {
  // 242 octets ASCII, puis un émoji de quatre octets que la borne coupe après
  // son premier : le message reste de l'ASCII, sans « ð » en queue.
  const std::string cut_emoji = std::string(242, 'a') + "\xF0\x9F\x98\x80";
  const auto text = ReadDiscordRelayText(RelayBytes(cut_emoji));
  REQUIRE(text.has_value());
  CHECK(*text == std::string(242, 'a'));
}

TEST_CASE("Relai Discord : l'UTF-8 se garde, le 1252 se convertit", "[net][discord]") {
  const auto utf8 = ReadDiscordRelayText(RelayBytes("caf\xC3\xA9 \xF0\x9F\x98\x80"));
  REQUIRE(utf8.has_value());
  CHECK(*utf8 == "caf\xC3\xA9 \xF0\x9F\x98\x80");

  const auto cp1252 = ReadDiscordRelayText(RelayBytes("caf\xE9"));
  REQUIRE(cp1252.has_value());
  CHECK(*cp1252 == "caf\xC3\xA9");
}

TEST_CASE("Relai Discord : une longueur mensongère ne fait rien lire de plus", "[net][discord]") {
  // Plus courte que l'en-tête : la trame est illisible.
  std::vector<std::uint8_t> short_header = {0x08, 0x0F, 0x02, 0x00};
  CHECK_FALSE(ReadDiscordRelayText(short_header).has_value());
  CHECK_FALSE(ReadDiscordRelayText(std::vector<std::uint8_t>{0x08, 0x0F}).has_value());

  // Plus longue que ce qui est arrivé : on lit ce qui est là.
  std::vector<std::uint8_t> claims_more = RelayBytes("hi");
  claims_more.pop_back();
  claims_more[2] = 0xFF;
  const auto text = ReadDiscordRelayText(claims_more);
  REQUIRE(text.has_value());
  CHECK(*text == "hi");

  // Plus courte que le message : on s'arrête où elle dit.
  std::vector<std::uint8_t> claims_less = RelayBytes("hello");
  claims_less[2] = static_cast<std::uint8_t>(ro::net::kDiscordRelayHeader + 2);
  const auto cut = ReadDiscordRelayText(claims_less);
  REQUIRE(cut.has_value());
  CHECK(*cut == "he");
}

TEST_CASE("Relai Discord : seules les cartes de Gonryun le reçoivent", "[net][discord]") {
  CHECK(IsDiscordRelayMap("gonryun"));
  CHECK(IsDiscordRelayMap("Gonryun"));
  CHECK(IsDiscordRelayMap("gonryun_in"));
  CHECK_FALSE(IsDiscordRelayMap("prontera"));
  CHECK_FALSE(IsDiscordRelayMap("gonr"));
  CHECK_FALSE(IsDiscordRelayMap(""));
}

TEST_CASE("Relai Discord : l'auteur entre les seconds crochets", "[net][discord]") {
  const auto ana = FindDiscordRelayAuthor("[#gonryun][Ana] hi");
  REQUIRE(ana.has_value());
  CHECK(ana->offset == 11);
  CHECK(ana->length == 3);

  // Le canal entre les premiers crochets n'importe pas.
  const auto other = FindDiscordRelayAuthor("[Discord][X] salut");
  REQUIRE(other.has_value());
  CHECK(other->offset == 10);
  CHECK(other->length == 1);

  CHECK_FALSE(FindDiscordRelayAuthor("Ana : hi").has_value());
  CHECK_FALSE(FindDiscordRelayAuthor("[a] [b] c").has_value());
  CHECK_FALSE(FindDiscordRelayAuthor("[a][] c").has_value());
  CHECK_FALSE(FindDiscordRelayAuthor("[a][b").has_value());
  CHECK_FALSE(FindDiscordRelayAuthor("").has_value());
}

TEST_CASE("Relai Discord : à Gonryun, la ligne entre au fil, en bleu Discord", "[net][discord]") {
  ro::sim::World world;
  world.EnterMap("gonryun", kGonryunX, kGonryunY);
  // Par le relai général, pour prouver que la trame y est branchée.
  CHECK(ro::net::ApplyFrame(RelayFrame("[#gonryun][Ana] salut"), world, kSaidMs) ==
        FrameOutcome::kApplied);

  REQUIRE(world.chat_lines().size() == 1);
  const ro::sim::ChatLine& line = world.chat_lines().front();
  CHECK(line.text == "[#gonryun][Ana] salut");
  CHECK(line.kind == ro::sim::ChatKind::kPublic);
  CHECK(line.type == ro::sim::ChatType::kDiscord);
  CHECK(line.color == ro::net::kDiscordRelayRgb);
  CHECK(line.color == 0x7289DAU);
  CHECK(line.speaker_id == 0);
  CHECK(line.said_ms == kSaidMs);
}

TEST_CASE("Relai Discord : hors de Gonryun, rien", "[net][discord]") {
  ro::sim::World world;
  world.EnterMap("prontera", kGonryunX, kGonryunY);
  CHECK(ApplyDiscordRelayFrame(RelayFrame("[#gonryun][Ana] salut"), world, kSaidMs) ==
        FrameOutcome::kIgnored);
  CHECK(world.chat_lines().empty());
}

TEST_CASE("Relai Discord : le relai décoché le tait", "[net][discord]") {
  ro::sim::World world;
  world.EnterMap("gonryun", kGonryunX, kGonryunY);
  REQUIRE(world.session().discord_chat);

  world.session().discord_chat = false;
  CHECK(ApplyDiscordRelayFrame(RelayFrame("[#gonryun][Ana] salut"), world, kSaidMs) ==
        FrameOutcome::kIgnored);
  CHECK(world.chat_lines().empty());

  // Recoché, il reprend — sans rendre ce qui a été jeté.
  world.session().discord_chat = true;
  CHECK(ApplyDiscordRelayFrame(RelayFrame("[#gonryun][Ana] encore"), world, kSaidMs) ==
        FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines().front().text == "[#gonryun][Ana] encore");
}

TEST_CASE("Relai Discord : une trame illisible ou un autre opcode", "[net][discord]") {
  ro::sim::World world;
  world.EnterMap("gonryun", kGonryunX, kGonryunY);
  ro::net::Frame broken{kOpcode, {0x08, 0x0F}};
  CHECK(ApplyDiscordRelayFrame(broken, world, kSaidMs) == FrameOutcome::kUndecodable);

  ro::net::Frame other = RelayFrame("x");
  other.opcode = 0x0F07;
  CHECK(ApplyDiscordRelayFrame(other, world, kSaidMs) == FrameOutcome::kUnhandled);
  CHECK(world.chat_lines().empty());
}

// ── LA FORME RICHE (`ZC_MOONLIGHT_DISCORD_RICH 0x0F3C`) ─────────────────────

namespace {

constexpr std::uint16_t kRichOpcode = 0x0F3C;
constexpr std::size_t kRichFixedBytes = 18;
constexpr std::uint64_t kSomeDiscordUser = 123456789012345678ULL;

// La trame telle que le serveur l'écrit : le bloc fixe, puis les quatre
// morceaux, sans NUL.
[[nodiscard]] std::vector<std::uint8_t> RichBytes(std::string_view channel, std::string_view name,
                                                  std::string_view avatar, std::string_view text) {
  std::vector<std::uint8_t> bytes;
  PutU16(bytes, kRichOpcode);
  PutU16(bytes, static_cast<std::uint16_t>(kRichFixedBytes + channel.size() + name.size() +
                                           avatar.size() + text.size()));
  PutU64(bytes, kSomeDiscordUser);
  bytes.push_back(static_cast<std::uint8_t>(channel.size()));
  bytes.push_back(static_cast<std::uint8_t>(name.size()));
  bytes.push_back(static_cast<std::uint8_t>(avatar.size()));
  bytes.push_back(0);
  PutU16(bytes, static_cast<std::uint16_t>(text.size()));
  for (const std::string_view piece : {channel, name, avatar, text}) {
    bytes.insert(bytes.end(), piece.begin(), piece.end());
  }
  return bytes;
}

[[nodiscard]] ro::net::Frame RichFrame(std::string_view channel, std::string_view name,
                                       std::string_view avatar, std::string_view text) {
  return ro::net::Frame{kRichOpcode, RichBytes(channel, name, avatar, text)};
}

}  // namespace

TEST_CASE("Relai Discord riche : la ligne se recompose comme l'ancienne", "[net][discord]") {
  const auto line = ro::net::ReadDiscordRichLine(
      RichBytes("gonryun", "Ana", "avatars/1/abc.png", "salut <:pepe:42>"));
  REQUIRE(line.has_value());
  CHECK(line->text == "[#gonryun][Ana] salut <:pepe:42>");
  CHECK(line->author.name == "Ana");
  CHECK(line->author.discord_user_id == kSomeDiscordUser);
  CHECK(line->author.avatar_path == "avatars/1/abc.png");
  CHECK(line->text.substr(line->author.said_offset) == "salut <:pepe:42>");
  // Le pseudo est là où le lien de l'ancienne forme le trouve.
  const auto found = FindDiscordRelayAuthor(line->text);
  REQUIRE(found.has_value());
  CHECK(found->offset == line->author.name_offset);
  CHECK(found->length == line->author.name_length);
}

TEST_CASE("Relai Discord riche : les décalages se comptent après la conversion", "[net][discord]") {
  // Un nom en 1252 : « é » y tient un octet, deux en UTF-8.
  const auto line = ro::net::ReadDiscordRichLine(RichBytes("gonryun", "Jos\xE9", "", "ok"));
  REQUIRE(line.has_value());
  CHECK(line->author.name == "Jos\xC3\xA9");
  CHECK(line->author.name_length == 5);
  CHECK(line->text.substr(line->author.name_offset, line->author.name_length) == "Jos\xC3\xA9");
  CHECK(line->text.substr(line->author.said_offset) == "ok");
  CHECK(line->author.avatar_path.empty());
}

TEST_CASE("Relai Discord riche : un message entier, au-delà de 243 octets", "[net][discord]") {
  const std::string long_text(1000, 'a');
  const auto line = ro::net::ReadDiscordRichLine(RichBytes("gonryun", "Ana", "", long_text));
  REQUIRE(line.has_value());
  CHECK(line->text.substr(line->author.said_offset) == long_text);
}

TEST_CASE("Relai Discord riche : un avatar qui sortirait du CDN s'efface", "[net][discord]") {
  CHECK(ro::net::IsDiscordAvatarPath("avatars/1/abc.png"));
  CHECK(ro::net::IsDiscordAvatarPath("embed/avatars/3.png"));
  CHECK(ro::net::IsDiscordAvatarPath("guilds/9/users/1/avatars/a_b-c.png"));
  CHECK_FALSE(ro::net::IsDiscordAvatarPath(""));
  CHECK_FALSE(ro::net::IsDiscordAvatarPath("../evil.png"));
  CHECK_FALSE(ro::net::IsDiscordAvatarPath("avatars/../../x.png"));
  CHECK_FALSE(ro::net::IsDiscordAvatarPath("//evil.tld/x.png"));
  CHECK_FALSE(ro::net::IsDiscordAvatarPath("avatars//x.png"));
  CHECK_FALSE(ro::net::IsDiscordAvatarPath("/avatars/x.png"));
  CHECK_FALSE(ro::net::IsDiscordAvatarPath("avatars/x.png?y=1"));
  CHECK_FALSE(ro::net::IsDiscordAvatarPath("@evil.tld/x.png"));
  CHECK(ro::net::DiscordAvatarUrl("embed/avatars/3.png") ==
        "https://cdn.discordapp.com/embed/avatars/3.png");
  CHECK(ro::net::DiscordAvatarUrl("../x.png").empty());

  // Refusé, il n'emporte pas le message.
  const auto line = ro::net::ReadDiscordRichLine(RichBytes("gonryun", "Ana", "//evil", "hi"));
  REQUIRE(line.has_value());
  CHECK(line->author.avatar_path.empty());
  CHECK(line->text == "[#gonryun][Ana] hi");
}

TEST_CASE("Relai Discord riche : des longueurs qui ne remplissent pas la trame", "[net][discord]") {
  std::vector<std::uint8_t> longer = RichBytes("gonryun", "Ana", "", "hi");
  longer.push_back('x');
  CHECK_FALSE(ro::net::ReadDiscordRichLine(longer).has_value());

  std::vector<std::uint8_t> shorter = RichBytes("gonryun", "Ana", "", "hi");
  shorter.pop_back();
  CHECK_FALSE(ro::net::ReadDiscordRichLine(shorter).has_value());

  CHECK_FALSE(ro::net::ReadDiscordRichLine(RichBytes("gonryun", "", "", "hi")).has_value());
  CHECK_FALSE(ro::net::ReadDiscordRichLine(RichBytes("", "Ana", "", "hi")).has_value());
  const std::string_view with_nul("A\0a", 3);
  CHECK_FALSE(ro::net::ReadDiscordRichLine(RichBytes("gonryun", with_nul, "", "hi")).has_value());
  const std::string too_long(ro::net::kDiscordRichTextMax + 1, 'a');
  CHECK_FALSE(ro::net::ReadDiscordRichLine(RichBytes("gonryun", "Ana", "", too_long)).has_value());
}

TEST_CASE("Relai Discord riche : mêmes filtres, et l'auteur entre au fil", "[net][discord]") {
  ro::sim::World world;
  world.EnterMap("prontera", kGonryunX, kGonryunY);
  // Par le relai général, pour prouver que la trame y est branchée.
  CHECK(ro::net::ApplyFrame(RichFrame("gonryun", "Ana", "", "hi"), world, kSaidMs) ==
        FrameOutcome::kIgnored);
  CHECK(world.chat_lines().empty());

  world.EnterMap("gonryun", kGonryunX, kGonryunY);
  world.session().discord_chat = false;
  CHECK(ApplyDiscordRelayFrame(RichFrame("gonryun", "Ana", "", "hi"), world, kSaidMs) ==
        FrameOutcome::kIgnored);
  CHECK(world.chat_lines().empty());

  world.session().discord_chat = true;
  CHECK(ApplyDiscordRelayFrame(RichFrame("gonryun", "Ana", "embed/avatars/1.png", "hi"), world,
                               kSaidMs) == FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  const ro::sim::ChatLine& line = world.chat_lines().front();
  CHECK(line.text == "[#gonryun][Ana] hi");
  CHECK(line.type == ro::sim::ChatType::kDiscord);
  CHECK(line.color == ro::net::kDiscordRelayRgb);
  // ⚠ L'ORATEUR RESTE ZÉRO : un message de Discord ne met pas de bulle.
  CHECK(line.speaker_id == 0);
  REQUIRE(line.author.has_value());
  CHECK(line.author->avatar_path == "embed/avatars/1.png");

  ro::net::Frame broken{kRichOpcode, {0x3C, 0x0F, 0x04, 0x00}};
  CHECK(ApplyDiscordRelayFrame(broken, world, kSaidMs) == FrameOutcome::kUndecodable);
}
