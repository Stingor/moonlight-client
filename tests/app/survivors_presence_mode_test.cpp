// LA PRÉSENCE EN ARÈNE : muette sans service, un envoi par changement d'état
// et par seconde au plus, oubliée à la perte de liaison ; la table des autres
// suit ce que le serveur dit d'eux.

#include "game/survivors_presence_mode.h"

#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/net/survivors_presence.h"
#include "ro/net/ui_caps.h"
#include "ro/proto/packet_lengths.h"

#include "support/wire_bytes.h"
#include "survivors/survivors_stage.h"

namespace rn = ro::net;
using moonclient::SurvivorsPresenceFacts;
using moonclient::SurvivorsPresenceMode;
using ro::test_support::PutU16;
using ro::test_support::PutU32;

namespace {

constexpr std::uint32_t kAccountId = 2000123;
constexpr std::uint32_t kOtherAccountId = 2000042;
constexpr std::uint16_t kArenaId = 3;
constexpr std::uint16_t kClassId = 2;

// Une table des longueurs qui connaît `ZC 0x0F57`, ou non.
[[nodiscard]] moonclient::PacketLengthOf Table(bool knows_presence) {
  return [knows_presence](std::uint16_t opcode) -> std::optional<std::int16_t> {
    if (opcode == rn::kSurvivorsPresenceOpcode && knows_presence) {
      return static_cast<std::int16_t>(rn::kSurvivorsPresenceSize);
    }
    return std::nullopt;
  };
}

[[nodiscard]] rn::Frame PresenceFrame(std::uint32_t account_id, rn::SurvivorsPresenceState state,
                                      std::uint32_t elapsed_s) {
  rn::Frame frame{.opcode = rn::kSurvivorsPresenceOpcode};
  PutU16(frame.bytes, rn::kSurvivorsPresenceOpcode);
  PutU16(frame.bytes, static_cast<std::uint16_t>(rn::kSurvivorsPresenceSize));
  PutU32(frame.bytes, account_id);
  PutU32(frame.bytes, account_id == 0 ? 0 : 150000);
  frame.bytes.push_back(static_cast<std::uint8_t>(state));
  PutU16(frame.bytes, kArenaId);
  PutU16(frame.bytes, kClassId);
  PutU32(frame.bytes, elapsed_s);
  return frame;
}

[[nodiscard]] rn::Frame Announcement() {
  return PresenceFrame(0, rn::SurvivorsPresenceState::kOut, 0);
}

constexpr rn::SurvivorsPresenceAsk kPlaying{
    .state = rn::SurvivorsPresenceState::kPlaying, .arena_id = kArenaId, .class_id = kClassId};

[[nodiscard]] SurvivorsPresenceFacts Facts(rn::SurvivorsPresenceAsk own, std::int64_t now_ms) {
  return SurvivorsPresenceFacts{
      .link_open = true, .verified = true, .shown = true, .own = own, .now_ms = now_ms};
}

}  // namespace

TEST_CASE("Présence survivors : le bit n'entre au masque que si la table connaît le ZC",
          "[app][survivors]") {
  CHECK(SurvivorsPresenceMode(Table(true)).caps() == rn::kUiCapSurvivorsPresence);
  CHECK(SurvivorsPresenceMode(Table(false)).caps() == 0);
  // La table livrée, engendrée du serveur, le connaît : le bit y est.
  CHECK(ro::proto::PacketLength(rn::kSurvivorsPresenceOpcode).has_value());
  CHECK(SurvivorsPresenceMode().caps() == rn::kUiCapSurvivorsPresence);
}

TEST_CASE("Présence survivors : rien ne part avant l'annonce du serveur", "[app][survivors]") {
  SurvivorsPresenceMode presence(Table(true));
  CHECK_FALSE(presence.Due(Facts(kPlaying, 0)).has_value());
  CHECK_FALSE(presence.announced());

  REQUIRE(presence.ApplyFrame(Announcement(), 0) == rn::FrameOutcome::kApplied);
  CHECK(presence.announced());
  // Dehors pour le serveur, dehors pour nous : rien à dire.
  CHECK_FALSE(presence.Due(Facts(rn::SurvivorsPresenceAsk{}, 0)).has_value());
  const std::optional<std::vector<std::uint8_t>> entered = presence.Due(Facts(kPlaying, 10));
  REQUIRE(entered.has_value());
  CHECK(*entered == rn::EncodeSurvivorsPresenceAsk(kPlaying));
  // Une fois dit, il ne se redit pas.
  CHECK_FALSE(presence.Due(Facts(kPlaying, 5000)).has_value());
}

TEST_CASE("Présence survivors : un envoi par seconde au plus, l'état final gagne",
          "[app][survivors]") {
  SurvivorsPresenceMode presence(Table(true));
  REQUIRE(presence.ApplyFrame(Announcement(), 0) == rn::FrameOutcome::kApplied);
  REQUIRE(presence.Due(Facts(kPlaying, 1000)).has_value());
  rn::SurvivorsPresenceAsk paused = kPlaying;
  paused.state = rn::SurvivorsPresenceState::kPaused;
  // Une pause prise et lâchée dans la seconde n'est jamais dite.
  CHECK_FALSE(presence.Due(Facts(paused, 1200)).has_value());
  CHECK_FALSE(presence.Due(Facts(kPlaying, 1999)).has_value());
  // Partie finie : la sortie part, à la seconde.
  CHECK_FALSE(presence.Due(Facts(rn::SurvivorsPresenceAsk{}, 1500)).has_value());
  const std::optional<std::vector<std::uint8_t>> left =
      presence.Due(Facts(rn::SurvivorsPresenceAsk{}, 2000));
  REQUIRE(left.has_value());
  CHECK(*left == rn::EncodeSurvivorsPresenceAsk(rn::SurvivorsPresenceAsk{}));
}

TEST_CASE("Présence survivors : caché par le réglage, le serveur le protège sans le dire",
          "[app][survivors]") {
  SurvivorsPresenceMode presence(Table(true));
  REQUIRE(presence.ApplyFrame(Announcement(), 0) == rn::FrameOutcome::kApplied);
  SurvivorsPresenceFacts hidden = Facts(kPlaying, 0);
  hidden.shown = false;
  const std::optional<std::vector<std::uint8_t>> first = presence.Due(hidden);
  REQUIRE(first.has_value());
  rn::SurvivorsPresenceAsk asked = hidden.own;
  asked.flags |= rn::kSurvivorsPresenceHidden;
  CHECK(*first == rn::EncodeSurvivorsPresenceAsk(asked));
  CHECK_FALSE(presence.Due(hidden).has_value());
}

TEST_CASE("Présence survivors : elle se redit, pour ne pas expirer", "[app][survivors]") {
  SurvivorsPresenceMode presence(Table(true));
  REQUIRE(presence.ApplyFrame(Announcement(), 0) == rn::FrameOutcome::kApplied);
  REQUIRE(presence.Due(Facts(kPlaying, 0)).has_value());
  CHECK_FALSE(presence.Due(Facts(kPlaying, 60'000)).has_value());
  CHECK(presence.Due(Facts(kPlaying, 300'000)).has_value());
  CHECK_FALSE(presence.Due(Facts(kPlaying, 301'000)).has_value());
}

TEST_CASE("Présence survivors : la liaison perdue oublie tout, l'annonce suivante redit l'état",
          "[app][survivors]") {
  SurvivorsPresenceMode presence(Table(true));
  REQUIRE(presence.ApplyFrame(Announcement(), 0) == rn::FrameOutcome::kApplied);
  REQUIRE(presence.ApplyFrame(
              PresenceFrame(kOtherAccountId, rn::SurvivorsPresenceState::kPlaying, 60), 100'000) ==
          rn::FrameOutcome::kApplied);
  REQUIRE(presence.Due(Facts(kPlaying, 0)).has_value());

  SurvivorsPresenceFacts closed = Facts(kPlaying, 3000);
  closed.link_open = false;
  CHECK_FALSE(presence.Due(closed).has_value());
  CHECK_FALSE(presence.announced());
  CHECK(presence.seen().empty());

  // Le serveur suivant annonce : la partie en cours se redit aussitôt.
  REQUIRE(presence.ApplyFrame(Announcement(), 4000) == rn::FrameOutcome::kApplied);
  CHECK(presence.Due(Facts(kPlaying, 4000)).has_value());
}

TEST_CASE("Présence survivors : la table suit ce que le serveur dit des autres",
          "[app][survivors]") {
  SurvivorsPresenceMode presence(Table(true));
  constexpr std::int64_t kReceivedMs = 500'000;
  REQUIRE(presence.ApplyFrame(PresenceFrame(kAccountId, rn::SurvivorsPresenceState::kPlaying, 462),
                              kReceivedMs) == rn::FrameOutcome::kApplied);
  REQUIRE(
      presence.ApplyFrame(PresenceFrame(kOtherAccountId, rn::SurvivorsPresenceState::kPaused, 10),
                          kReceivedMs) == rn::FrameOutcome::kApplied);
  REQUIRE(presence.seen().size() == 2);
  // Rangés par compte.
  CHECK(presence.seen()[0].account_id == kOtherAccountId);
  const moonclient::SurvivorsPresenceSeen* seen = presence.Of(kAccountId);
  REQUIRE(seen != nullptr);
  CHECK(seen->arena_id == kArenaId);
  CHECK(seen->class_id == kClassId);
  // La durée suit l'horloge du client, depuis l'entrée que le serveur a dite.
  CHECK(moonclient::PresenceElapsedS(*seen, kReceivedMs) == 462);
  CHECK(moonclient::PresenceElapsedS(*seen, kReceivedMs + 38'000) == 500);

  // Sorti : il quitte la table.
  REQUIRE(presence.ApplyFrame(PresenceFrame(kAccountId, rn::SurvivorsPresenceState::kOut, 0),
                              kReceivedMs) == rn::FrameOutcome::kApplied);
  CHECK(presence.Of(kAccountId) == nullptr);
  CHECK(presence.seen().size() == 1);

  // Une trame abîmée ne touche à rien ; une autre trame n'est pas la sienne.
  rn::Frame broken = PresenceFrame(kAccountId, rn::SurvivorsPresenceState::kPlaying, 1);
  broken.bytes.pop_back();
  CHECK(presence.ApplyFrame(broken, kReceivedMs) == rn::FrameOutcome::kUndecodable);
  CHECK(presence.ApplyFrame(rn::Frame{.opcode = 0x0080}, kReceivedMs) ==
        rn::FrameOutcome::kUnhandled);
}

TEST_CASE("Présence survivors : hors d'une partie, l'étage dit dehors", "[app][survivors]") {
  const moonclient::SurvivorsStage closed;
  CHECK(moonclient::PresenceOfStage(closed) == rn::SurvivorsPresenceAsk{});
}
