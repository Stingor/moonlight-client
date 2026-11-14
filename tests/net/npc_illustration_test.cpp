// ZC_SHOW_IMAGE : le serveur montre une illustration de PNJ, ou l'efface.

#include "ro/net/npc_illustration.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_packets.h"

#include "support/net_frames.h"

using ro::net::ApplyNpcIllustrationFrame;
using ro::net::FrameOutcome;
using ro::sim::IllustrationPlace;
using ro::sim::NpcIllustrationSlot;
using ro::test_support::FrameOf;

namespace {

FrameOutcome Show(NpcIllustrationSlot& slot, const char* image, std::uint8_t type) {
  ro::proto::ZcShowImage packet;
  packet.image = image;
  packet.type = type;
  return ApplyNpcIllustrationFrame(FrameOf(Encode(packet)), slot);
}

}  // namespace

TEST_CASE("ZC_SHOW_IMAGE : le nom, tel quel, et la place", "[net][illustration]") {
  NpcIllustrationSlot slot;
  REQUIRE(Show(slot, "ep18_maram_01.png", 2) == FrameOutcome::kApplied);
  REQUIRE(slot.shown().has_value());
  CHECK(slot.shown()->image == "ep18_maram_01.png");
  CHECK(slot.shown()->place == IllustrationPlace::kBottomRight);
}

TEST_CASE("ZC_SHOW_IMAGE : 255 efface, et un nom vide aussi", "[net][illustration]") {
  NpcIllustrationSlot slot;
  REQUIRE(Show(slot, "16lei_01", 1) == FrameOutcome::kApplied);
  REQUIRE(Show(slot, "", 255) == FrameOutcome::kApplied);
  CHECK_FALSE(slot.shown().has_value());

  REQUIRE(Show(slot, "16lei_01", 1) == FrameOutcome::kApplied);
  REQUIRE(Show(slot, "", 1) == FrameOutcome::kApplied);
  CHECK_FALSE(slot.shown().has_value());
}

TEST_CASE("ZC_SHOW_IMAGE : les autres trames ne la regardent pas", "[net][illustration]") {
  NpcIllustrationSlot slot;
  ro::proto::ZcCloseDialog close;
  CHECK(ApplyNpcIllustrationFrame(FrameOf(Encode(close)), slot) == FrameOutcome::kUnhandled);
}
