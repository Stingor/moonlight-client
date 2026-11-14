#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/sprite_catalog.h"
#include "ro/sim/survivors_match.h"
#include "ro/sim/survivors_rules.h"

#include "game/survivors_coop_mode.h"
#include "survivors/survivors_stage.h"

namespace moonclient {
namespace {

using ro::sim::SurvivorsStart;

constexpr std::uint32_t kFrameMs = 20;

[[nodiscard]] SurvivorsStart DuoStart(const ro::sim::SurvivorsRules& rules) {
  SurvivorsStart start;
  start.seed = 31;
  start.class_id = 1;
  start.first_skill_id = ro::sim::FindClass(rules, 1)->starter_skills.front();
  for (const ro::sim::ArenaRule& arena : rules.arenas) {
    if (arena.terrain_id != 0) {
      start.arena_id = arena.arena_id;
      break;
    }
  }
  start.allies.push_back(ro::sim::SurvivorsAllyStart{
      .class_id = 2, .first_skill_id = ro::sim::FindClass(rules, 2)->starter_skills.front()});
  return start;
}

// Deux étages en lockstep, chaque trame livrée à l'autre à l'image suivante.
struct Duo {
  SurvivorsStage first;
  SurvivorsStage second;

  explicit Duo(const ro::sim::SurvivorsRules& rules) {
    first.Open();
    second.Open();
    REQUIRE(first.BeginCoop(rules, std::nullopt, DuoStart(rules), 0, 4));
    REQUIRE(second.BeginCoop(rules, std::nullopt, DuoStart(rules), 1, 4));
    first.Loaded();
    second.Loaded();
  }
  void Frames(std::uint32_t count, const SurvivorsIntent& intent = {}) {
    for (std::uint32_t frame = 0; frame < count; ++frame) {
      first.Advance(kFrameMs, intent);
      second.Advance(kFrameMs, intent);
      while (auto payload = first.TakeCoopFrame()) second.CoopReceive(0, *payload);
      while (auto payload = second.TakeCoopFrame()) first.CoopReceive(1, *payload);
    }
  }
};

TEST_CASE("une partie coop descend, puis les deux étages jouent le même tic", "[survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  Duo duo(rules);
  CHECK(duo.first.coop());
  CHECK(duo.first.board().coop);
  CHECK(duo.first.screen() == SurvivorsScreen::kArriving);
  duo.Frames(kArrivalMs / kFrameMs + 1);
  CHECK(duo.first.screen() == SurvivorsScreen::kPlaying);
  duo.Frames(500);
  const ro::sim::SurvivorsMatch* first = duo.first.match();
  const ro::sim::SurvivorsMatch* second = duo.second.match();
  REQUIRE(first != nullptr);
  REQUIRE(second != nullptr);
  CHECK(first->confirmed().field().ticks > 400);
  CHECK_FALSE(first->diverged());
  CHECK_FALSE(second->diverged());
}

TEST_CASE("chaque étage met en scène le héros de son joueur", "[survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  Duo duo(rules);
  duo.Frames(kArrivalMs / kFrameMs + 20);
  REQUIRE(duo.first.play() != nullptr);
  CHECK(duo.first.play()->run().field().stage == 0);
  CHECK(duo.second.play()->run().field().stage == 1);
  CHECK(duo.second.play()->run().field().hero_class->class_id == 2);
}

TEST_CASE("la pause d'une coop ne fige rien", "[survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  Duo duo(rules);
  duo.Frames(kArrivalMs / kFrameMs + 1);
  const std::uint32_t before = duo.first.match()->confirmed().field().ticks;
  SurvivorsIntent escape;
  escape.escape = true;
  duo.first.Advance(kFrameMs, escape);
  CHECK(duo.first.screen() == SurvivorsScreen::kPaused);
  duo.Frames(100);
  CHECK(duo.first.match()->confirmed().field().ticks > before);
}

TEST_CASE("abandonner quitte l'équipe et donne le bilan", "[survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  Duo duo(rules);
  duo.Frames(kArrivalMs / kFrameMs + 1);
  SurvivorsIntent escape;
  escape.escape = true;
  duo.first.Advance(kFrameMs, escape);
  REQUIRE(duo.first.screen() == SurvivorsScreen::kPaused);
  SurvivorsIntent abandon;
  abandon.abandon = true;
  duo.first.Advance(kFrameMs, abandon);
  CHECK(duo.first.screen() == SurvivorsScreen::kSummary);
  CHECK_FALSE(duo.first.coop());
  REQUIRE(duo.first.ending() != nullptr);
  CHECK(duo.first.ending()->board.coop);
}

TEST_CASE("le temps d'attente des autres se compte, et abandonner reste possible partout",
          "[survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  Duo duo(rules);
  duo.Frames(kArrivalMs / kFrameMs + 1);
  CHECK(duo.first.coop_stalled_ms() == 0);
  // L'autre se tait : seul le premier avance.
  for (std::uint32_t frame = 0; frame < 200; ++frame) duo.first.Advance(kFrameMs, {});
  CHECK(duo.first.coop_stalled_ms() > 1000);
  duo.first.AbandonCoop();
  CHECK(duo.first.screen() == SurvivorsScreen::kSummary);
  CHECK_FALSE(duo.first.coop());
  CHECK(duo.first.coop_stalled_ms() > 0);
}

TEST_CASE("un client calé au plafond d'avance en redescend", "[survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  Duo duo(rules);
  duo.Frames(kArrivalMs / kFrameMs + 100);
  // L'autre fige le temps d'atteindre le plafond, puis tout repart à l'image.
  for (std::uint32_t frame = 0; frame < 60; ++frame) duo.first.Advance(kFrameMs, {});
  REQUIRE(duo.first.match()->lead_ticks() > ro::sim::kLockstepEaseLeadTicks);
  duo.Frames(800);
  CHECK(duo.first.match()->lead_ticks() <= ro::sim::kLockstepEaseLeadTicks + 1U);
  CHECK_FALSE(duo.first.match()->diverged());
}

TEST_CASE("une offre qui en suit une autre repasse par l'entrée de l'écran", "[survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  SurvivorsStage first;
  SurvivorsStage second;
  SurvivorsStart start = DuoStart(rules);
  start.loadout.rerolls = 3;
  first.Open();
  second.Open();
  REQUIRE(first.BeginCoop(rules, std::nullopt, start, 0, 4));
  REQUIRE(second.BeginCoop(rules, std::nullopt, start, 1, 4));
  first.Loaded();
  second.Loaded();
  const auto frame = [&](const SurvivorsIntent& intent) {
    first.Advance(kFrameMs, intent);
    second.Advance(kFrameMs, intent);
    while (auto payload = first.TakeCoopFrame()) second.CoopReceive(0, *payload);
    while (auto payload = second.TakeCoopFrame()) first.CoopReceive(1, *payload);
  };
  constexpr std::uint32_t kMostFrames = 60000;
  std::uint32_t frames = 0;
  while (first.screen() != SurvivorsScreen::kChoosing && first.coop() && !first.coop_over() &&
         frames < kMostFrames) {
    frame({});
    ++frames;
  }
  REQUIRE(first.screen() == SurvivorsScreen::kChoosing);
  // La garde laisse passer le clic, puis les deux relancent.
  for (std::uint32_t wait = 0; wait < 50; ++wait) frame({});
  REQUIRE(first.screen() == SurvivorsScreen::kChoosing);
  const std::vector<ro::sim::SurvivorsOffer> offers = first.match()->confirmed().field().offers;
  SurvivorsIntent reroll;
  reroll.answer = ro::sim::SurvivorsAnswer{.kind = ro::sim::AnswerKind::kReroll};
  bool entered_again = false;
  for (std::uint32_t wait = 0; wait < 100 && !entered_again; ++wait) {
    frame(wait == 0 ? reroll : SurvivorsIntent{});
    entered_again = first.entered() == SurvivorsScreen::kChoosing;
  }
  CHECK(entered_again);
  CHECK(first.match()->confirmed().field().offers != offers);
}

[[nodiscard]] std::vector<std::uint8_t> LeftFrame(std::uint32_t char_id) {
  std::vector<std::uint8_t> bytes{0x5D, 0x0F, 12, 0};
  for (std::size_t shift = 0; shift < 32; shift += 8) {
    bytes.push_back(static_cast<std::uint8_t>((char_id >> shift) & 0xFFU));
  }
  bytes.insert(bytes.end(), {0, 0, 0, 0});
  return bytes;
}

[[nodiscard]] ro::net::SurvivorsCoopStart RosterOf() {
  ro::net::SurvivorsCoopStart start;
  start.players.push_back({.char_id = 10, .part = {}});
  start.players.push_back({.char_id = 20, .part = {}});
  return start;
}

TEST_CASE("le client apprend que le serveur l'a sorti de la partie", "[survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  Duo duo(rules);
  SurvivorsCoopMode coop;
  coop.Roster(RosterOf(), 10);
  ro::net::Connection link;
  // Le départ d'un autre passe au robot, le sien ne se confond pas avec.
  REQUIRE(coop.ApplyFrame(ro::net::Frame{.opcode = ro::net::kSurvivorsCoopLeftOpcode,
                                         .bytes = LeftFrame(20)}) ==
          ro::net::FrameOutcome::kApplied);
  coop.Carry(duo.first, link, {});
  CHECK_FALSE(coop.TakeExcluded());
  CHECK(duo.first.match()->left(1));
  REQUIRE(coop.ApplyFrame(ro::net::Frame{.opcode = ro::net::kSurvivorsCoopLeftOpcode,
                                         .bytes = LeftFrame(10)}) ==
          ro::net::FrameOutcome::kApplied);
  coop.Carry(duo.first, link, {});
  CHECK(coop.TakeExcluded());
  CHECK_FALSE(coop.TakeExcluded());
}

TEST_CASE("un départ annoncé tard ne touche pas la partie suivante", "[survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  SurvivorsCoopMode coop;
  coop.Roster(RosterOf(), 10);
  REQUIRE(coop.ApplyFrame(ro::net::Frame{.opcode = ro::net::kSurvivorsCoopLeftOpcode,
                                         .bytes = LeftFrame(20)}) ==
          ro::net::FrameOutcome::kApplied);
  // Le départ suivant, sur le fil, efface ce qui le précède.
  const std::vector<std::uint8_t> start_bytes{0x5A, 0x0F, 14, 0, 1, 0, 0, 0, 0, 0, 0, 0, 4, 0};
  REQUIRE(coop.ApplyFrame(ro::net::Frame{.opcode = ro::net::kSurvivorsCoopStartOpcode,
                                         .bytes = start_bytes}) == ro::net::FrameOutcome::kApplied);
  Duo duo(rules);
  ro::net::Connection link;
  coop.Carry(duo.first, link, {});
  CHECK_FALSE(duo.first.match()->left(1));
}

TEST_CASE("le choix ne se redit que tant que le salon ne le montre pas", "[survivors]") {
  constexpr std::uint32_t kOwnCharId = 7;
  constexpr std::uint16_t kKnight = 2;
  constexpr std::uint16_t kBash = 5;
  SurvivorsCoopMode coop;
  CHECK_FALSE(coop.ChoiceOwed(kOwnCharId, kKnight, kBash));

  // Au salon, le serveur ne montre encore aucun choix pour ce joueur.
  std::vector<std::uint8_t> lobby_bytes{0x59, 0x0F, 43,         0, 1, 0, 0, 0,
                                        0,    1,    kOwnCharId, 0, 0, 0, 0};
  lobby_bytes.resize(43);
  REQUIRE(coop.ApplyFrame(ro::net::Frame{.opcode = ro::net::kSurvivorsCoopLobbyOpcode,
                                         .bytes = lobby_bytes}) == ro::net::FrameOutcome::kApplied);
  CHECK(coop.ChoiceOwed(kOwnCharId, kKnight, kBash));
  CHECK_FALSE(coop.ChoiceOwed(kOwnCharId + 1, kKnight, kBash));

  // Il le montre : plus rien à dire, sauf si le joueur change d'avis.
  lobby_bytes[39] = kKnight;
  lobby_bytes[41] = kBash;
  REQUIRE(coop.ApplyFrame(ro::net::Frame{.opcode = ro::net::kSurvivorsCoopLobbyOpcode,
                                         .bytes = lobby_bytes}) == ro::net::FrameOutcome::kApplied);
  CHECK_FALSE(coop.ChoiceOwed(kOwnCharId, kKnight, kBash));
  CHECK(coop.ChoiceOwed(kOwnCharId, kKnight, kBash + 1));
}

TEST_CASE("renoncer au salon efface le départ qui attendait", "[survivors]") {
  SurvivorsCoopMode coop;
  const std::vector<std::uint8_t> lobby_bytes{0x59, 0x0F, 10, 0, 1, 0, 0, 0, 0, 0};
  REQUIRE(coop.ApplyFrame(ro::net::Frame{.opcode = ro::net::kSurvivorsCoopLobbyOpcode,
                                         .bytes = lobby_bytes}) == ro::net::FrameOutcome::kApplied);
  REQUIRE(coop.lobby().state == ro::net::SurvivorsCoopState::kLobby);
  const std::vector<std::uint8_t> start_bytes{0x5A, 0x0F, 14, 0, 1, 0, 0, 0, 0, 0, 0, 0, 4, 0};
  REQUIRE(coop.ApplyFrame(ro::net::Frame{.opcode = ro::net::kSurvivorsCoopStartOpcode,
                                         .bytes = start_bytes}) == ro::net::FrameOutcome::kApplied);
  ro::net::Connection link;
  coop.Forsake(link);
  CHECK(coop.lobby().state == ro::net::SurvivorsCoopState::kClosed);
  CHECK_FALSE(coop.TakeStart());
}

TEST_CASE("le départ se lit des parts, chacun sa place", "[survivors]") {
  const ro::sim::SurvivorsRules& rules = ro::sim::ShippedSurvivorsRules();
  const SurvivorsStart duo = DuoStart(rules);
  const auto part_of = [&](std::uint16_t class_id, std::uint16_t first_skill_id,
                           std::uint16_t hair) {
    ro::sim::SurvivorsCoopPart part;
    part.ruleset_version = rules.ruleset_version;
    part.rules_digest = ro::sim::SurvivorsRulesDigest(rules);
    part.arena_id = duo.arena_id;
    part.player.class_id = class_id;
    part.player.first_skill_id = first_skill_id;
    std::vector<std::uint8_t> bytes = ro::sim::EncodeSurvivorsCoopPart(part);
    ro::render::ActorLook look;
    look.hair = hair;
    const std::vector<std::uint8_t> trailing = EncodeCoopLook(look);
    bytes.insert(bytes.end(), trailing.begin(), trailing.end());
    return bytes;
  };
  ro::net::SurvivorsCoopStart start;
  start.seed = 5;
  start.input_delay_ticks = 4;
  start.players.push_back({.char_id = 10, .part = part_of(1, duo.first_skill_id, 7)});
  start.players.push_back({.char_id = 20, .part = part_of(2, duo.allies[0].first_skill_id, 9)});
  const auto departure = DepartureOf(start, 20, rules);
  REQUIRE(departure);
  CHECK(departure->local_player == 1);
  CHECK(departure->start.seed == 5);
  CHECK(departure->start.class_id == 1);
  CHECK(departure->looks[1].hair == 9);
  CHECK_FALSE(DepartureOf(start, 99, rules));
}

}  // namespace
}  // namespace moonclient
