#include <cstdint>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/world_update.h"
#include "ro/proto/show_script_packets.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"
#include "support/wire_bytes.h"

// CE QU'UN SCRIPT POSE AU-DESSUS D'UN ACTEUR (`showscript`) : une bulle, sans
// ligne dans le fil.

using ro::net::ApplyFrame;
using ro::net::FrameOutcome;
using ro::sim::CellPosition;
using ro::sim::World;
using ro::test_support::FrameOf;
using ro::test_support::IdleUnit;
using ro::test_support::PutU16;
using ro::test_support::PutU32;

namespace {

// Une trame écrite à la main : le client n'émet jamais ce paquet.
[[nodiscard]] std::vector<std::uint8_t> ShowScriptFrame(std::uint32_t actor_id,
                                                        std::string_view message) {
  std::vector<std::uint8_t> frame;
  PutU16(frame, ro::proto::ShowScriptNotice::kOpcode);
  PutU16(frame, 0);
  PutU32(frame, actor_id);
  for (const char character : message) {
    frame.push_back(static_cast<std::uint8_t>(character));
  }
  frame.push_back(0);
  const auto length = static_cast<std::uint16_t>(frame.size());
  frame[2] = static_cast<std::uint8_t>(length & 0xFF);
  frame[3] = static_cast<std::uint8_t>(length >> 8);
  return frame;
}

}  // namespace

TEST_CASE("Traduction : un texte de script se pose en bulle, hors du fil", "[net][notice]") {
  // Une bulle au-dessus de son porteur, tel quel, et pas une ligne de chat.
  World world;
  REQUIRE(ApplyFrame(FrameOf(Encode(IdleUnit(150'001, CellPosition{1, 1, 0}))), world, 0) ==
          FrameOutcome::kApplied);

  CHECK(ApplyFrame(FrameOf(ShowScriptFrame(150'001, "50 <- Unpurified Illusions -> 50")), world,
                   4000) == FrameOutcome::kApplied);
  CHECK(world.Find(150'001)->script_text == "50 <- Unpurified Illusions -> 50");
  CHECK(world.Find(150'001)->script_since_ms == 4000);
  CHECK(world.chat_lines().empty());

  // UNE BULLE À PART : la parole qui suit ne l'écrase pas, et elle n'écrase
  // pas la parole.
  world.ShowSpeech(150'001, "Bob : salut", 4500);
  CHECK(world.Find(150'001)->chat_text == "Bob : salut");
  CHECK(world.Find(150'001)->script_text == "50 <- Unpurified Illusions -> 50");

  // Un texte vide ne montre rien.
  CHECK(ApplyFrame(FrameOf(ShowScriptFrame(150'001, "")), world, 5000) == FrameOutcome::kIgnored);
}
