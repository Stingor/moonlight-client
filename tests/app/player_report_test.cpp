// « SIGNALER CE JOUEUR » : l'entrée du menu, la fenêtre qu'elle ouvre, ce qui
// part, et la phrase que chaque verdict écrit dans la conversation.

#include "world/player_report.h"

#include <algorithm>
#include <cstdint>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/map_packets.h"
#include "ro/sim/world.h"
#include "ro/ui/entity_menu.h"

#include "support/net_frames.h"
#include "world/entity_menu.h"

using moonclient::BuildEntityMenu;
using moonclient::EntityFamily;
using moonclient::EntityMenuFacts;
using moonclient::EntityVerb;
using moonclient::PlayerReportDesk;
using moonclient::PlayerReportLine;
using ro::net::PlayerReportAck;
using ro::net::PlayerReportResult;

namespace {

constexpr std::uint32_t kSelf = 2000001;
constexpr std::uint32_t kBob = 2000002;
constexpr std::uint32_t kCarol = 2000003;
constexpr std::uint32_t kNowMs = 50000;
// Au-delà du dernier verdict connu.
constexpr std::int32_t kUnknownResult = 42;

[[nodiscard]] std::size_t RankOf(const ro::ui::EntityMenuView& view, EntityVerb verb) {
  const auto found = std::ranges::find_if(view.items, [verb](const ro::ui::EntityMenuItem& item) {
    return item.verb == static_cast<int>(verb);
  });
  return static_cast<std::size_t>(found - view.items.begin());
}

[[nodiscard]] PlayerReportAck AckOf(PlayerReportResult result) {
  return PlayerReportAck{
      .target_aid = kBob, .target_name = "Bob", .result = static_cast<std::int32_t>(result)};
}

[[nodiscard]] ro::sim::World WorldWithSelf() {
  ro::sim::World world;
  ro::sim::Actor self;
  self.id = kSelf;
  self.kind = ro::sim::ActorKind::kPlayer;
  world.Spawn(self);
  world.set_player_id(kSelf);
  return world;
}

[[nodiscard]] bool Glows(const ro::sim::World& world) {
  const ro::sim::Actor* self = world.Find(kSelf);
  return self != nullptr &&
         std::ranges::any_of(self->effects, [](const ro::sim::PlayedEffect& effect) {
           return effect.id == moonclient::kPlayerReportRecordedEffect;
         });
}

}  // namespace

TEST_CASE("Signalement : l'entrée suit le blocage des chuchotements, jamais sur soi",
          "[app][player_report]") {
  EntityMenuFacts facts;
  facts.family = EntityFamily::kPlayer;
  facts.id = kBob;
  facts.name = "Bob";
  const ro::ui::EntityMenuView view = BuildEntityMenu(facts, {});
  const std::size_t rank = RankOf(view, EntityVerb::kReportPlayer);
  REQUIRE(rank < view.items.size());
  CHECK(view.items[rank].label == "Signaler ce joueur");
  CHECK_FALSE(view.items[rank].disabled);
  CHECK(RankOf(view, EntityVerb::kWhisperBlock) + 1 == rank);
  CHECK(rank + 1 == RankOf(view, EntityVerb::kCopyName));

  EntityMenuFacts self;
  self.family = EntityFamily::kSelf;
  self.id = kSelf;
  self.name = "Moi";
  const ro::ui::EntityMenuView own = BuildEntityMenu(self, {});
  CHECK(RankOf(own, EntityVerb::kReportPlayer) == own.items.size());
}

TEST_CASE("Signalement : la fenêtre s'ouvre à neuf sur un autre joueur", "[app][player_report]") {
  PlayerReportDesk desk;
  ro::ui::PlayerReportWindowState state;
  desk.Open(state, kBob, "Bob");
  CHECK(state.open);
  CHECK(state.raise);
  CHECK(state.target_name == "Bob");
  CHECK(desk.target_aid() == kBob);

  // Le texte et le motif survivent à une seconde ouverture sur le même joueur...
  state.motive = ro::ui::PlayerReportMotive::kBugAbuse;
  state.message[0] = 'x';
  desk.Open(state, kBob, "Bob");
  CHECK(state.motive == ro::ui::PlayerReportMotive::kBugAbuse);
  CHECK(state.message[0] == 'x');

  // ... mais pas au changement de joueur.
  desk.Open(state, kCarol, "Carol");
  CHECK(state.motive == ro::ui::PlayerReportMotive::kIllegalProgram);
  CHECK(state.message[0] == '\0');
  CHECK(desk.target_aid() == kCarol);
}

TEST_CASE("Signalement : ce qui part porte le joueur, le motif et le texte",
          "[app][player_report]") {
  PlayerReportDesk desk;
  ro::ui::PlayerReportWindowState state;
  desk.Open(state, kBob, "Bob");
  state.motive = ro::ui::PlayerReportMotive::kBugAbuse;
  const std::string text = "Il traverse les murs.";
  std::ranges::copy(text, state.message.begin());
  const ro::net::PlayerReport report = desk.ReportOf(state, kSelf);
  CHECK(report.reporter_aid == kSelf);
  CHECK(report.target_aid == kBob);
  CHECK(report.target_name == "Bob");
  CHECK(report.reason == ro::net::PlayerReportReason::kBugAbuse);
  CHECK(report.message == text);
}

TEST_CASE("Signalement : chaque verdict connu a sa phrase, un inconnu se tait",
          "[app][player_report]") {
  CHECK(PlayerReportLine(AckOf(PlayerReportResult::kRecorded)) ==
        std::string("Signalement de Bob enregistré."));
  CHECK(PlayerReportLine(AckOf(PlayerReportResult::kCoolTime)) ==
        std::string("Vous avez déjà signalé ce joueur récemment."));
  for (const PlayerReportResult result :
       {PlayerReportResult::kCountLimit, PlayerReportResult::kTargetProtected,
        PlayerReportResult::kDailyMaxReached, PlayerReportResult::kReportedByOther,
        PlayerReportResult::kAreaForbidden, PlayerReportResult::kTargetNotFound,
        PlayerReportResult::kFailed}) {
    const auto line = PlayerReportLine(AckOf(result));
    REQUIRE(line.has_value());
    CHECK_FALSE(line->empty());
  }
  PlayerReportAck unknown = AckOf(PlayerReportResult::kRecorded);
  unknown.result = kUnknownResult;
  CHECK_FALSE(PlayerReportLine(unknown).has_value());
}

TEST_CASE("Signalement : l'accusé s'écrit dans la conversation, l'effet suit l'enregistrement",
          "[app][player_report]") {
  ro::sim::World world = WorldWithSelf();
  ro::proto::ZcAckReportUser packet;
  packet.targetAID = kBob;
  packet.targetName = "Bob";
  packet.result = static_cast<std::int32_t>(PlayerReportResult::kCoolTime);
  CHECK(PlayerReportDesk::ApplyFrame(ro::test_support::FrameOf(ro::proto::Encode(packet)), world,
                                     kNowMs) == ro::net::FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 1);
  CHECK(world.chat_lines().back().text == "Vous avez déjà signalé ce joueur récemment.");
  CHECK_FALSE(Glows(world));

  packet.result = static_cast<std::int32_t>(PlayerReportResult::kRecorded);
  CHECK(PlayerReportDesk::ApplyFrame(ro::test_support::FrameOf(ro::proto::Encode(packet)), world,
                                     kNowMs) == ro::net::FrameOutcome::kApplied);
  REQUIRE(world.chat_lines().size() == 2);
  CHECK(world.chat_lines().back().text == "Signalement de Bob enregistré.");
  CHECK(Glows(world));

  // Un verdict inconnu n'écrit rien.
  packet.result = kUnknownResult;
  CHECK(PlayerReportDesk::ApplyFrame(ro::test_support::FrameOf(ro::proto::Encode(packet)), world,
                                     kNowMs) == ro::net::FrameOutcome::kApplied);
  CHECK(world.chat_lines().size() == 2);

  // Une autre trame ne le concerne pas.
  ro::proto::ZcMoonlightChatBlocks other;
  CHECK(PlayerReportDesk::ApplyFrame(ro::test_support::FrameOf(ro::proto::Encode(other)), world,
                                     kNowMs) == ro::net::FrameOutcome::kUnhandled);
}
