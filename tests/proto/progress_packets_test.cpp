// LA BARRE DE PROGRESSION D'UN SCRIPT : les quatre paquets, recoupés avec les
// longueurs que le serveur déclare.

#include "ro/proto/progress_packets.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_packets.h"
#include "ro/proto/packet_lengths.h"

using ro::proto::DecodeProgressStart;
using ro::proto::EncodeProgressStart;
using ro::proto::PacketLength;
using ro::proto::ProgressCancel;
using ro::proto::ProgressDone;
using ro::proto::ProgressStart;

namespace {

[[nodiscard]] bool DeclaredLength(std::uint16_t opcode, std::size_t wire_size) {
  const auto length = PacketLength(opcode);
  return length.has_value() && static_cast<std::size_t>(*length) == wire_size;
}

}  // namespace

TEST_CASE("Barre de progression : les longueurs sont celles du serveur", "[proto][progress]") {
  CHECK(DeclaredLength(ProgressStart::kOpcode, ProgressStart::kWireSize));
  CHECK(DeclaredLength(ProgressDone::kOpcode, ProgressDone::kWireSize));
  CHECK(DeclaredLength(ProgressCancel::kOpcode, ProgressCancel::kWireSize));
  CHECK(DeclaredLength(ro::proto::kProgressActorOpcode, ro::proto::ZcProgressActor::kWireSize));
}

TEST_CASE("Barre de progression : ZC_PROGRESS porte la couleur puis les SECONDES",
          "[proto][progress]") {
  const std::vector<std::uint8_t> frame =
      EncodeProgressStart(ProgressStart{.color = 0x00FF00, .seconds = 5});
  CHECK(frame == std::vector<std::uint8_t>{0xF0, 0x02, 0x00, 0xFF, 0x00, 0x00, 5, 0, 0, 0});
  const auto read = DecodeProgressStart(frame);
  REQUIRE(read.has_value());
  CHECK(read->color == 0x00FF00);
  CHECK(read->seconds == 5);

  std::vector<std::uint8_t> short_frame = frame;
  short_frame.pop_back();
  CHECK_FALSE(DecodeProgressStart(short_frame).has_value());
}

TEST_CASE("Barre de progression : CZ_PROGRESS n'est que son opcode", "[proto][progress]") {
  const auto done = ro::proto::EncodeProgressDone();
  REQUIRE(done.has_value());
  CHECK(*done == std::vector<std::uint8_t>{0xF1, 0x02});
  CHECK(ro::proto::EncodeProgressCancel() == std::vector<std::uint8_t>{0xF2, 0x02});
}

TEST_CASE("Barre de progression : ZC_PROGRESS_ACTOR se relit par la structure du serveur",
          "[proto][progress]") {
  ro::proto::ZcProgressActor sent;
  sent.GID = 110'012;
  sent.color = 0xFF0000;
  sent.time = 3;
  ro::proto::ZcProgressActor read;
  REQUIRE(ro::proto::Decode(ro::proto::EncodeProgressActor(sent), read));
  CHECK(read.GID == sent.GID);
  CHECK(read.color == sent.color);
  CHECK(read.time == sent.time);
}
