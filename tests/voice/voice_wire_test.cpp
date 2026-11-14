// LE FIL DU SERVEUR VOIX : les trames TCP découpées quel que soit le morceau
// reçu, l'audio entendu lu depuis des octets écrits à la main, et les
// datagrammes épinglés à l'octet.

#include "ro/voice/voice_wire.h"

#include <bit>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::voice::DecodeHeardVoice;
using ro::voice::DecodeUdp;
using ro::voice::EncodeFrame;
using ro::voice::EncodeTcpVoice;
using ro::voice::EncodeUdpHello;
using ro::voice::EncodeUdpVoice;
using ro::voice::FrameKind;
using ro::voice::FrameReader;
using ro::voice::UdpIdentity;
using ro::voice::UdpKind;
using ro::voice::VoiceChannel;

namespace {

void PutBe32(std::vector<std::uint8_t>& out, std::uint32_t value) {
  out.push_back(static_cast<std::uint8_t>(value >> 24));
  out.push_back(static_cast<std::uint8_t>(value >> 16));
  out.push_back(static_cast<std::uint8_t>(value >> 8));
  out.push_back(static_cast<std::uint8_t>(value));
}

// Un audio entendu tel que le serveur l'écrit : 42 octets d'en-tête, puis l'Opus.
[[nodiscard]] std::vector<std::uint8_t> HeardBytes(float volume) {
  std::vector<std::uint8_t> bytes;
  PutBe32(bytes, 150001);
  PutBe32(bytes, std::bit_cast<std::uint32_t>(volume));
  PutBe32(bytes, 156);
  PutBe32(bytes, 191);
  const char name[] = "Sting";
  for (std::size_t at = 0; at < 24; ++at) {
    bytes.push_back(at < sizeof(name) - 1 ? static_cast<std::uint8_t>(name[at]) : 0);
  }
  bytes.push_back(0x12);
  bytes.push_back(0x34);
  bytes.push_back(0xF8);  // un octet TOC d'Opus, puis un corps
  bytes.push_back(0x01);
  return bytes;
}

}  // namespace

TEST_CASE("Voix : une trame se découpe, octet par octet comme d'un bloc", "[voice][wire]") {
  const std::vector<std::uint8_t> text = {'{', '}'};
  const std::vector<std::uint8_t> first = EncodeFrame(FrameKind::kText, text);
  REQUIRE(first == std::vector<std::uint8_t>{0x56, 0x54, 0x01, 0x01, 0, 0, 0, 2, '{', '}'});
  const std::vector<std::uint8_t> second =
      EncodeFrame(FrameKind::kBinary, std::vector<std::uint8_t>{7});

  FrameReader reader;
  std::vector<std::uint8_t> stream = first;
  stream.insert(stream.end(), second.begin(), second.end());
  std::vector<ro::voice::StreamFrame> frames;
  for (const std::uint8_t byte : stream) {
    reader.Feed(std::vector<std::uint8_t>{byte});
    auto next = reader.Next();
    REQUIRE(next.has_value());
    if (next->has_value()) frames.push_back(std::move(**next));
  }
  REQUIRE(frames.size() == 2);
  CHECK(frames[0].kind == FrameKind::kText);
  CHECK(frames[0].payload == text);
  CHECK(frames[1].kind == FrameKind::kBinary);
  CHECK(frames[1].payload == std::vector<std::uint8_t>{7});
}

TEST_CASE("Voix : un flux sans signature est perdu", "[voice][wire]") {
  FrameReader reader;
  reader.Feed(std::vector<std::uint8_t>{0x47, 0x45, 0x54, 0x20, 0, 0, 0, 0});
  CHECK_FALSE(reader.Next().has_value());
}

TEST_CASE("Voix : l'audio entendu se lit, son volume se borne", "[voice][wire]") {
  const std::optional<ro::voice::HeardVoice> heard = DecodeHeardVoice(HeardBytes(0.5F));
  REQUIRE(heard.has_value());
  CHECK(heard->speaker_id == 150001);
  CHECK(heard->volume == 0.5F);
  CHECK(heard->x == 156);
  CHECK(heard->y == 191);
  CHECK(heard->name == "Sting");
  CHECK(heard->sequence == 0x1234);
  CHECK(heard->opus == std::vector<std::uint8_t>{0xF8, 0x01});

  CHECK_FALSE(DecodeHeardVoice(HeardBytes(2.0F)).has_value());
  std::vector<std::uint8_t> bare = HeardBytes(0.5F);
  bare.resize(42);
  CHECK_FALSE(DecodeHeardVoice(bare).has_value());
}

TEST_CASE("Voix : l'audio envoyé par le TCP porte canal et numéro", "[voice][wire]") {
  const std::vector<std::uint8_t> opus = {0xF8, 0x02};
  CHECK(EncodeTcpVoice(VoiceChannel::kParty, 0x0102, opus) ==
        std::vector<std::uint8_t>{1, 0, 0, 0, 0, 0x01, 0x02, 0xF8, 0x02});
}

TEST_CASE("Voix : les datagrammes du client, à l'octet", "[voice][wire]") {
  const UdpIdentity identity{
      .session_id = 0x0102030405060708, .character_id = 0x0A0B0C0D, .token = 0x1112131415161718};
  const std::vector<std::uint8_t> hello = EncodeUdpHello(identity);
  CHECK(hello == std::vector<std::uint8_t>{0x56, 0x55, 0x44, 0x50, 1,    1,    1,    2,    3,
                                           4,    5,    6,    7,    8,    0x0A, 0x0B, 0x0C, 0x0D,
                                           0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18});

  const std::vector<std::uint8_t> voice =
      EncodeUdpVoice(identity, VoiceChannel::kGuild, 0x0304, std::vector<std::uint8_t>{0xF8});
  REQUIRE(voice.size() == ro::voice::kUdpVoiceHeaderBytes + 1);
  CHECK(voice[5] == 2);
  CHECK(voice[26] == 0x03);
  CHECK(voice[27] == 0x04);
  CHECK(voice[28] == 2);
  CHECK(voice[33] == 0xF8);
}

TEST_CASE("Voix : les datagrammes du serveur", "[voice][wire]") {
  const std::optional<ro::voice::UdpReceived> ack =
      DecodeUdp(std::vector<std::uint8_t>{0x56, 0x55, 0x44, 0x50, 1, 1});
  REQUIRE(ack.has_value());
  CHECK(ack->kind == UdpKind::kHello);

  std::vector<std::uint8_t> relayed = {0x56, 0x55, 0x44, 0x50, 1, 3};
  const std::vector<std::uint8_t> heard = HeardBytes(1.0F);
  relayed.insert(relayed.end(), heard.begin(), heard.end());
  const std::optional<ro::voice::UdpReceived> voice = DecodeUdp(relayed);
  REQUIRE(voice.has_value());
  CHECK(voice->kind == UdpKind::kHeard);
  REQUIRE(voice->heard.has_value());
  CHECK(voice->heard->name == "Sting");

  CHECK_FALSE(DecodeUdp(std::vector<std::uint8_t>{0x56, 0x55, 0x44, 0x51, 1, 1}).has_value());
}
