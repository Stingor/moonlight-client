// OÙ JOINDRE LE SERVEUR VOIX (`ZC 0x0F55`) : décodé depuis des octets écrits à
// la main, et refusé quand sa taille, son `len` ou son contenu ne vont pas.

#include "ro/net/voice_service.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/packet_lengths.h"

using ro::net::DecodeVoiceService;
using ro::net::kVoiceServiceSize;

namespace {

[[nodiscard]] std::vector<std::uint8_t> Service(const std::string& host, std::uint16_t port) {
  std::vector<std::uint8_t> bytes = {0x55,
                                     0x0F,
                                     static_cast<std::uint8_t>(kVoiceServiceSize),
                                     0,
                                     static_cast<std::uint8_t>(port),
                                     static_cast<std::uint8_t>(port >> 8)};
  bytes.insert(bytes.end(), host.begin(), host.end());
  bytes.resize(kVoiceServiceSize, 0);
  return bytes;
}

}  // namespace

TEST_CASE("Service voix : l'hôte et le port se lisent", "[net][voice]") {
  const std::optional<ro::net::VoiceService> service =
      DecodeVoiceService(Service("voix.moonlight.fr", 7000));
  REQUIRE(service.has_value());
  CHECK(service->host == "voix.moonlight.fr");
  CHECK(service->port == 7000);
}

TEST_CASE("Service voix : une trame qui ne va pas se refuse", "[net][voice]") {
  CHECK_FALSE(DecodeVoiceService(Service("", 7000)).has_value());
  CHECK_FALSE(DecodeVoiceService(Service("hote", 0)).has_value());
  std::vector<std::uint8_t> short_frame = Service("hote", 7000);
  short_frame.pop_back();
  CHECK_FALSE(DecodeVoiceService(short_frame).has_value());
  std::vector<std::uint8_t> lying = Service("hote", 7000);
  lying[2] = 69;
  CHECK_FALSE(DecodeVoiceService(lying).has_value());
}

TEST_CASE("Service voix : la table des longueurs connaît le ZC", "[net][voice]") {
  CHECK(ro::proto::PacketLength(ro::net::kVoiceServiceOpcode).has_value());
}
