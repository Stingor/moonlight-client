// La ligne qu'un état écrit dans la conversation : sur soi en case 8, sur un
// membre connecté du groupe en case 10, rien sur les autres ; une table par
// forme de paquet, et une pose rafraîchie qui se réécrit.

#include "ro/net/status_chat.h"

#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_packets.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/world.h"

#include "support/net_frames.h"

using ro::net::ApplyStatusChatFrame;
using ro::net::Frame;
using ro::net::FrameOutcome;
using ro::net::StatusChatFacts;
using ro::net::StatusChatForm;
using ro::net::StatusChatPhraseOf;
using ro::sim::ChatType;
using ro::sim::PartyMember;
using ro::sim::PartyRoster;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kMe = 2'000'001;
constexpr std::uint32_t kAlice = 2'000'002;
constexpr std::uint32_t kBob = 2'000'003;
constexpr std::uint32_t kStranger = 2'000'999;
constexpr std::uint32_t kNowMs = 500'000;
constexpr std::uint32_t kPoseCyan = 0x00FFFF;
constexpr std::uint32_t kLiftRed = 0xFF0000;
constexpr std::uint32_t kMsPerMinute = 60'000;

// Les EFST du serveur que ces essais posent.
constexpr std::uint16_t kBlessing = 10;
constexpr std::uint16_t kWeaponProperty = 64;
constexpr std::uint16_t kCashPlusExp = 250;
constexpr std::uint16_t kCashReceiveItem = 252;
constexpr std::uint16_t kAllStatDown = 1013;
constexpr std::uint16_t kPowerfulFaith = 1160;
constexpr std::uint16_t kMusicalInterlude = 1261;
constexpr std::uint16_t kSilentEffect = 3;

constexpr std::int32_t kFiftyPercent = 50;

[[nodiscard]] Frame Timed(std::uint32_t account_id, std::uint16_t effect, bool active,
                          std::uint32_t left_ms = kMsPerMinute, std::int32_t val1 = 0) {
  ro::proto::PacketStatusChange packet;
  packet.index = static_cast<std::int16_t>(effect);
  packet.AID = account_id;
  packet.state = active ? 1 : 0;
  packet.Total = left_ms;
  packet.Left = left_ms;
  packet.val1 = val1;
  return FrameOf(ro::proto::Encode(packet));
}

[[nodiscard]] Frame OldTimed(std::uint32_t account_id, std::uint16_t effect) {
  ro::proto::PacketStatusChange2 packet;
  packet.index = static_cast<std::int16_t>(effect);
  packet.AID = account_id;
  packet.state = 1;
  packet.Left = kMsPerMinute;
  return FrameOf(ro::proto::Encode(packet));
}

[[nodiscard]] Frame Bare(std::uint32_t account_id, std::uint16_t effect, bool active) {
  ro::proto::PacketStatusChangeEnd packet;
  packet.index = static_cast<std::int16_t>(effect);
  packet.AID = account_id;
  packet.state = active ? 1 : 0;
  return FrameOf(ro::proto::Encode(packet));
}

struct Scene {
  ro::sim::World world;
  PartyRoster party;

  Scene() {
    world.set_player_id(kMe);
    party.Upsert(PartyMember{.account_id = kAlice, .name = "Alice"});
    party.Upsert(PartyMember{.account_id = kBob, .name = "Bob", .offline = true});
  }

  FrameOutcome Apply(const Frame& frame) {
    return ApplyStatusChatFrame(frame, world, party, kNowMs);
  }
};

[[nodiscard]] std::string PhraseText(StatusChatForm form, std::uint16_t effect, bool active,
                                     std::uint32_t remaining_ms = 0, std::int32_t val1 = 0) {
  const auto phrase = StatusChatPhraseOf(StatusChatFacts{.form = form,
                                                         .effect = effect,
                                                         .active = active,
                                                         .remaining_ms = remaining_ms,
                                                         .val1 = val1});
  return phrase.has_value() ? phrase->text : std::string();
}

}  // namespace

TEST_CASE("Altérations d'état (net) : sur soi, la pose en cyan et la levée en rouge, case 8",
          "[net][status_chat]") {
  Scene scene;
  CHECK(scene.Apply(Timed(kMe, kBlessing, true)) == FrameOutcome::kApplied);
  CHECK(scene.Apply(Bare(kMe, kBlessing, false)) == FrameOutcome::kApplied);
  const auto lines = scene.world.chat_lines();
  REQUIRE(lines.size() == 2);
  CHECK(lines[0].type == ChatType::kAbnormalStatus);
  CHECK(lines[0].text == "La STR, l'INT, la DEX et le HIT augmentent.");
  CHECK(lines[0].color == kPoseCyan);
  CHECK(lines[1].type == ChatType::kAbnormalStatus);
  CHECK(lines[1].text == "Blessing est annulé.");
  CHECK(lines[1].color == kLiftRed);
}

TEST_CASE("Altérations d'état (net) : une pose rafraîchie se réécrit", "[net][status_chat]") {
  Scene scene;
  CHECK(scene.Apply(Timed(kMe, kBlessing, true)) == FrameOutcome::kApplied);
  CHECK(scene.Apply(Timed(kMe, kBlessing, true)) == FrameOutcome::kApplied);
  CHECK(scene.world.chat_lines().size() == 2);
}

TEST_CASE("Altérations d'état (net) : un membre connecté en case 10, son nom en tête",
          "[net][status_chat]") {
  Scene scene;
  CHECK(scene.Apply(Bare(kAlice, kBlessing, false)) == FrameOutcome::kApplied);
  // Hors ligne, ou hors du groupe : rien.
  CHECK(scene.Apply(Bare(kBob, kBlessing, false)) == FrameOutcome::kIgnored);
  CHECK(scene.Apply(Bare(kStranger, kBlessing, false)) == FrameOutcome::kIgnored);
  const auto lines = scene.world.chat_lines();
  REQUIRE(lines.size() == 1);
  CHECK(lines[0].type == ChatType::kPartyStatus);
  CHECK(lines[0].text == "Sur Alice : Blessing est annulé.");
  CHECK(lines[0].color == kLiftRed);
}

TEST_CASE("Altérations d'état (net) : un état sans phrase et un paquet étranger se taisent",
          "[net][status_chat]") {
  Scene scene;
  CHECK(scene.Apply(Timed(kMe, kSilentEffect, true)) == FrameOutcome::kIgnored);
  ro::proto::PacketStatusChange packet;
  packet.AID = kMe;
  Frame other = FrameOf(ro::proto::Encode(packet));
  other.opcode = 0;
  CHECK(scene.Apply(other) == FrameOutcome::kUnhandled);
  CHECK(scene.world.chat_lines().empty());
}

TEST_CASE(
    "Altérations d'état (net) : les classes 4 posent par la forme à durée et lèvent par "
    "la forme nue",
    "[net][status_chat]") {
  CHECK(PhraseText(StatusChatForm::kTimed, kPowerfulFaith, true) ==
        "L'ATK et la P.ATK augmentent.");
  CHECK(PhraseText(StatusChatForm::kBare, kPowerfulFaith, true).empty());
  CHECK(PhraseText(StatusChatForm::kBare, kPowerfulFaith, false) ==
        "L'effet de Powerful Faith est annulé.");
  CHECK(PhraseText(StatusChatForm::kTimed, kPowerfulFaith, false).empty());
}

TEST_CASE("Altérations d'état (net) : la forme ancienne tait quatre états", "[net][status_chat]") {
  Scene scene;
  CHECK(scene.Apply(OldTimed(kMe, kMusicalInterlude)) == FrameOutcome::kIgnored);
  CHECK(scene.Apply(Timed(kMe, kMusicalInterlude, true)) == FrameOutcome::kApplied);
  CHECK(scene.world.chat_lines().size() == 1);
}

TEST_CASE("Altérations d'état (net) : les gains de la boutique se chiffrent en minutes restantes",
          "[net][status_chat]") {
  constexpr std::uint32_t kNinetyMinutes = 90 * kMsPerMinute;
  constexpr std::uint32_t kTwoHoursFive = 125 * kMsPerMinute;
  CHECK(PhraseText(StatusChatForm::kTimed, kCashPlusExp, true, kNinetyMinutes, kFiftyPercent) ==
        "Pendant 90 minutes, votre EXP augmente de 50 %.");
  CHECK(PhraseText(StatusChatForm::kTimed, kCashReceiveItem, true, kTwoHoursFive, kFiftyPercent) ==
        "Pendant 2 heures 05 minutes, le drop d'items des monstres augmente de 50 %.");
  // Moins d'une minute : rien. La forme nue n'a pas de durée.
  CHECK(PhraseText(StatusChatForm::kTimed, kCashPlusExp, true, kMsPerMinute - 1, kFiftyPercent)
            .empty());
  CHECK(PhraseText(StatusChatForm::kBare, kCashPlusExp, true).empty());
  CHECK(PhraseText(StatusChatForm::kBare, kCashReceiveItem, true) ==
        "Le drop des items ordinaires des monstres est doublé pendant les 30 prochaines "
        "minutes.");
}

TEST_CASE("Altérations d'état (net) : l'enchantement d'arme se renomme dans la forme à durée",
          "[net][status_chat]") {
  CHECK(PhraseText(StatusChatForm::kTimed, kWeaponProperty, true).empty());
  CHECK(PhraseText(StatusChatForm::kBare, kWeaponProperty, true) ==
        "L'arme est temporairement enchantée de l'élément Sacré.");
  CHECK(PhraseText(StatusChatForm::kBare, kWeaponProperty, false) ==
        "L'arme retrouve son état normal.");
}

TEST_CASE(
    "Altérations d'état (net) : la baisse de toutes les caractéristiques inverse ses "
    "teintes",
    "[net][status_chat]") {
  const auto pose = StatusChatPhraseOf(
      StatusChatFacts{.form = StatusChatForm::kTimed, .effect = kAllStatDown, .active = true});
  const auto lift = StatusChatPhraseOf(
      StatusChatFacts{.form = StatusChatForm::kBare, .effect = kAllStatDown, .active = false});
  REQUIRE(pose.has_value());
  REQUIRE(lift.has_value());
  CHECK(pose->color_rgb == kLiftRed);
  CHECK(lift->color_rgb == kPoseCyan);
  CHECK(lift->text == "La baisse de toutes les caractéristiques est levée.");
}
