#include "ro/sim/survivors_match.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_play.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/survivors_run.h"
#include "ro/sim/walk_map.h"

namespace ro::sim {
namespace {

constexpr std::uint32_t kInputDelayTicks = 4;
constexpr std::uint8_t kEast = 0;
constexpr std::uint8_t kNorth = 16;

[[nodiscard]] SurvivorsStart DuoStart(const SurvivorsRules& rules) {
  SurvivorsStart start;
  start.seed = 4242;
  start.class_id = 1;
  start.first_skill_id = FindClass(rules, 1)->starter_skills.front();
  for (const ArenaRule& arena : rules.arenas) {
    if (arena.terrain_id != 0) {
      start.arena_id = arena.arena_id;
      break;
    }
  }
  start.allies.push_back(SurvivorsAllyStart{
      .class_id = 2, .first_skill_id = FindClass(rules, 2)->starter_skills.front()});
  return start;
}

[[nodiscard]] SurvivorsMatch MatchOf(const SurvivorsRules& rules, std::size_t local) {
  auto run = SurvivorsRun::Start(rules, DuoStart(rules));
  REQUIRE(run);
  return SurvivorsMatch(std::move(*run), local, kInputDelayTicks);
}

// Fait jouer deux clients, chacun son cap, chaque trame livrée après `lag` tics.
void PlayBoth(SurvivorsMatch& first, SurvivorsMatch& second, std::uint32_t ticks,
              std::uint32_t lag) {
  std::vector<std::pair<std::uint32_t, std::vector<std::uint8_t>>> to_first;
  std::vector<std::pair<std::uint32_t, std::vector<std::uint8_t>>> to_second;
  for (std::uint32_t tick = 0; tick < ticks; ++tick) {
    (void)first.PushLocal(SurvivorsTeamInput{.heading = kEast});
    (void)second.PushLocal(SurvivorsTeamInput{.heading = kNorth});
    if (auto frame = first.TakeFrame()) to_second.emplace_back(tick + lag, std::move(*frame));
    if (auto frame = second.TakeFrame()) to_first.emplace_back(tick + lag, std::move(*frame));
    std::erase_if(to_first, [&](const auto& due) {
      if (due.first > tick) return false;
      CHECK(first.Receive(1, due.second));
      return true;
    });
    std::erase_if(to_second, [&](const auto& due) {
      if (due.first > tick) return false;
      CHECK(second.Receive(0, due.second));
      return true;
    });
    first.Advance();
    second.Advance();
  }
}

TEST_CASE("deux clients lockstep calculent la même partie", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsMatch first = MatchOf(rules, 0);
  SurvivorsMatch second = MatchOf(rules, 1);
  PlayBoth(first, second, 1500, 3);
  CHECK(first.confirmed().field().ticks > 0);
  // Le plus avancé a fait les tics de l'autre : on compare à un même tic.
  const std::uint32_t common =
      std::min(first.confirmed().field().ticks, second.confirmed().field().ticks);
  CHECK(common > 1000);
  CHECK_FALSE(first.diverged());
  CHECK_FALSE(second.diverged());
}

TEST_CASE("le tic attend l'entrée de l'autre", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsMatch match = MatchOf(rules, 0);
  for (std::uint32_t tick = 0; tick < 20; ++tick) {
    (void)match.PushLocal(SurvivorsTeamInput{.heading = kEast});
    match.Advance();
  }
  // Seuls les tics d'avant la première entrée s'exécutent.
  CHECK(match.confirmed().field().ticks == kInputDelayTicks);
}

TEST_CASE("le présent répond dès la touche, sans attendre l'autre", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsMatch match = MatchOf(rules, 0);
  const FieldPoint start = match.confirmed().field().hero.position;
  for (std::uint32_t tick = 0; tick < 20; ++tick) {
    (void)match.PushLocal(SurvivorsTeamInput{.heading = kEast});
    match.Advance();
  }
  CHECK(match.confirmed().field().hero.position == start);
  CHECK_FALSE(match.present().field().hero.position == start);
}

TEST_CASE("le client sans nouvelles s'arrête à son avance maximale", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsMatch match = MatchOf(rules, 0);
  std::uint32_t accepted = 0;
  for (std::uint32_t tick = 0; tick < 200; ++tick) {
    if (match.PushLocal(SurvivorsTeamInput{})) ++accepted;
    match.Advance();
  }
  // Rien n'est encore confirmé : personne ne joue seul.
  CHECK(accepted <= kLockstepStartLeadTicks + 2U);
}

TEST_CASE("une entrée redite ne compte qu'une fois", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsMatch first = MatchOf(rules, 0);
  SurvivorsMatch second = MatchOf(rules, 1);
  for (std::uint32_t tick = 0; tick < 6; ++tick) {
    (void)second.PushLocal(SurvivorsTeamInput{.heading = kNorth});
    (void)first.PushLocal(SurvivorsTeamInput{.heading = kEast});
    first.Advance();
    if (auto frame = second.TakeFrame()) {
      CHECK(first.Receive(1, *frame));
      CHECK(first.Receive(1, *frame));
    }
    first.Advance();
  }
  CHECK(first.confirmed().field().ticks >= kInputDelayTicks);
}

TEST_CASE("la part d'un joueur se code et se relit, l'apparence à sa suite", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsCoopPart part;
  part.ruleset_version = rules.ruleset_version;
  part.rules_digest = SurvivorsRulesDigest(rules);
  part.arena_id = DuoStart(rules).arena_id;
  part.cursed_water = 2;
  part.player = DuoStart(rules).allies.front();
  part.player.leaves = 3;
  part.grounds = {{.arena_id = 1, .digest = 77}, {.arena_id = 5, .digest = 88}};
  std::vector<std::uint8_t> bytes = EncodeSurvivorsCoopPart(part);
  bytes.insert(bytes.end(), {9, 9});
  std::size_t trailing = 0;
  const auto read = DecodeSurvivorsCoopPart(bytes, &trailing);
  REQUIRE(read);
  CHECK(*read == part);
  CHECK(trailing == 2);
  // Une part qui s'ouvre sur la version des règles est celle de l'ancien format.
  std::vector<std::uint8_t> former = bytes;
  former[0] = static_cast<std::uint8_t>(rules.ruleset_version);
  former[1] = 0;
  CHECK_FALSE(DecodeSurvivorsCoopPart(former, &trailing));
  // Des sols annoncés et absents : tronquée, refusée.
  constexpr std::size_t kGroundsAnnouncedAt = 2 + 2 + 8 + 2 + 1;
  std::vector<std::uint8_t> overdrawn = bytes;
  overdrawn[kGroundsAnnouncedAt] = 0xFF;
  CHECK_FALSE(DecodeSurvivorsCoopPart(overdrawn, &trailing));
}

TEST_CASE("l'équipe ne part pas sur deux sols différents de son arène", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  constexpr std::uint16_t kMapArenaId = 1;
  constexpr std::uint16_t kOtherArenaId = 2;
  constexpr std::uint64_t kGround = 111;
  constexpr std::uint64_t kRetouchedGround = 222;
  SurvivorsCoopPart first;
  first.ruleset_version = rules.ruleset_version;
  first.rules_digest = SurvivorsRulesDigest(rules);
  first.arena_id = kMapArenaId;
  first.player.class_id = 1;
  first.player.first_skill_id = FindClass(rules, 1)->starter_skills.front();
  first.grounds = {{.arena_id = kMapArenaId, .digest = kGround}};
  SurvivorsCoopPart second = first;
  // L'ARÈNE DE L'ÉQUIPE EST CELLE DU PREMIER : ce que le second a choisi n'y change rien.
  second.arena_id = kOtherArenaId;
  CHECK(TeamStartOf(99, rules, std::array{first, second}));
  second.grounds.front().digest = kRetouchedGround;
  CHECK_FALSE(TeamStartOf(99, rules, std::array{first, second}));
  // Une carte que l'un n'a pas su lire ne vaut pas celle de l'autre.
  second.grounds.clear();
  CHECK_FALSE(TeamStartOf(99, rules, std::array{first, second}));
  // UNE ARÈNE GÉNÉRÉE NE LIT AUCUNE CARTE : un sol retouché ne l'empêche pas.
  first.arena_id = kOtherArenaId;
  second.grounds = {{.arena_id = kMapArenaId, .digest = kRetouchedGround}};
  CHECK(TeamStartOf(99, rules, std::array{first, second}));
}

TEST_CASE("l'empreinte d'un sol voit une case, et sa taille", "[survivors]") {
  constexpr std::array<std::string_view, 2> kOpen = {"..", ".."};
  constexpr std::array<std::string_view, 2> kWalled = {"..", ".#"};
  constexpr std::array<std::string_view, 1> kLong = {"...."};
  const std::uint64_t open = SurvivorsGroundDigest(WalkMap::FromPicture(kOpen));
  CHECK(open == SurvivorsGroundDigest(WalkMap::FromPicture(kOpen)));
  CHECK(open != SurvivorsGroundDigest(WalkMap::FromPicture(kWalled)));
  CHECK(open != SurvivorsGroundDigest(WalkMap::FromPicture(kLong)));
}

TEST_CASE("des images en plus ne changent pas le présent", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsMatch many_frames = MatchOf(rules, 0);
  SurvivorsMatch one_frame = MatchOf(rules, 0);
  SurvivorsMatch other = MatchOf(rules, 1);
  constexpr std::uint32_t kExtraFrames = 3;
  constexpr std::uint32_t kTicks = 40;
  for (std::uint32_t tick = 0; tick < kTicks; ++tick) {
    (void)other.PushLocal(SurvivorsTeamInput{.heading = kNorth});
    (void)many_frames.PushLocal(SurvivorsTeamInput{.heading = kEast});
    (void)one_frame.PushLocal(SurvivorsTeamInput{.heading = kEast});
    if (auto frame = other.TakeFrame()) {
      CHECK(many_frames.Receive(1, *frame));
      CHECK(one_frame.Receive(1, *frame));
    }
    if (auto frame = many_frames.TakeFrame()) CHECK(other.Receive(0, *frame));
    (void)one_frame.TakeFrame();
    other.Advance();
    one_frame.Advance();
    for (std::uint32_t extra = 0; extra < kExtraFrames; ++extra) many_frames.Advance();
    REQUIRE(SurvivorsDigest(many_frames.present().field()) ==
            SurvivorsDigest(one_frame.present().field()));
  }
  CHECK(one_frame.confirmed().field().ticks > kInputDelayTicks);
}

TEST_CASE("une trame reçue refait le présent, même sans tic neuf", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  // TROIS JOUEURS : le troisième reste muet, et aucun tic ne se confirme.
  const auto match_of = [&rules](std::size_t local) {
    SurvivorsStart start = DuoStart(rules);
    start.allies.push_back(start.allies.front());
    auto run = SurvivorsRun::Start(rules, start);
    REQUIRE(run);
    return SurvivorsMatch(std::move(*run), local, kInputDelayTicks);
  };
  SurvivorsMatch match = match_of(0);
  SurvivorsMatch other = match_of(1);
  REQUIRE(match.PushLocal(SurvivorsTeamInput{.heading = kEast}));
  match.Advance();
  const FieldPoint predicted = HeroAt(match.present().field(), 1).position;
  REQUIRE(other.PushLocal(SurvivorsTeamInput{.heading = kNorth}));
  const auto frame = other.TakeFrame();
  REQUIRE(frame);
  REQUIRE(match.Receive(1, *frame));
  match.Advance();
  CHECK(match.confirmed().field().ticks == kInputDelayTicks);
  CHECK_FALSE(HeroAt(match.present().field(), 1).position == predicted);
}

TEST_CASE("un cap entendu refait le présent, même sans tic neuf", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsMatch match = MatchOf(rules, 0);
  SurvivorsMatch other = MatchOf(rules, 1);
  REQUIRE(match.PushLocal(SurvivorsTeamInput{.heading = kEast}));
  match.Advance();
  const FieldPoint predicted = HeroAt(match.present().field(), 1).position;
  REQUIRE(other.PushLocal(SurvivorsTeamInput{.heading = kNorth}));
  const auto frame = other.TakeFrame();
  REQUIRE(frame);
  match.Hear(1, *frame);
  match.Advance();
  CHECK_FALSE(HeroAt(match.present().field(), 1).position == predicted);
}

TEST_CASE("le dessin suit le présent sans le copier, jusqu'à ce qu'il le modifie", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsMatch match = MatchOf(rules, 0);
  auto run = SurvivorsRun::Start(rules, DuoStart(rules));
  REQUIRE(run);
  SurvivorsPlay play(std::move(*run));
  play.Watch(match.present(), 0);
  CHECK(&play.run() == &match.present());
  play.Abandon();
  CHECK(&play.run() != &match.present());
  CHECK(play.run().field().phase == SurvivorsPhase::kOver);
  CHECK(match.present().field().phase != SurvivorsPhase::kOver);
}

TEST_CASE("le départ d'une équipe prend la Cursed Water la plus basse", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsCoopPart first;
  first.ruleset_version = rules.ruleset_version;
  first.rules_digest = SurvivorsRulesDigest(rules);
  first.arena_id = DuoStart(rules).arena_id;
  first.cursed_water = 3;
  first.player.class_id = 1;
  first.player.first_skill_id = FindClass(rules, 1)->starter_skills.front();
  SurvivorsCoopPart second = first;
  second.cursed_water = 1;
  const std::array<SurvivorsCoopPart, 2> parts{first, second};
  const auto start = TeamStartOf(99, rules, parts);
  REQUIRE(start);
  CHECK(start->cursed_water == 1);
  CHECK(start->allies.size() == 1);
  CHECK(SurvivorsRun::Start(rules, *start).has_value());
  second.rules_digest ^= 1U;
  const std::array<SurvivorsCoopPart, 2> mixed{first, second};
  CHECK_FALSE(TeamStartOf(99, rules, mixed));
}

TEST_CASE("une trame mal formée se refuse", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsMatch match = MatchOf(rules, 0);
  const std::array<std::uint8_t, 3> short_bytes{1, 2, 3};
  CHECK_FALSE(match.Receive(1, short_bytes));
  // Un cap hors de ses bornes.
  const std::array<std::uint8_t, 8> bad_heading{0, 0, 0, 0, 1, 200, kLockstepNoBallot, 0};
  CHECK_FALSE(match.Receive(1, bad_heading));
  // Le joueur local ne se reçoit pas lui-même.
  const std::array<std::uint8_t, 8> fine{0, 0, 0, 0, 1, 0, kLockstepNoBallot, 0};
  CHECK_FALSE(match.Receive(0, fine));
}

TEST_CASE("le robot reprend un joueur parti, au même tic partout", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsMatch match = MatchOf(rules, 0);
  // Sa dernière entrée connue fixe le tic du robot : sans elle, le client attend.
  const std::array<std::uint8_t, 8> last{
      static_cast<std::uint8_t>(kInputDelayTicks), 0, 0, 0, 1, kNoHeading, kLockstepNoBallot, 0};
  REQUIRE(match.Receive(1, last));
  match.Leave(1);
  CHECK(match.left(1));
  for (std::uint32_t tick = 0; tick < 100; ++tick) {
    (void)match.PushLocal(SurvivorsTeamInput{.heading = kEast});
    match.Advance();
  }
  CHECK(match.confirmed().field().ticks > kInputDelayTicks + 1U);
}

// Toutes les trames en attente de `from`, livrées à `to` par le chemin fiable.
void Deliver(SurvivorsMatch& from, SurvivorsMatch& to, std::size_t from_player) {
  while (auto frame = from.TakeFrame()) CHECK(to.Receive(from_player, *frame));
}

TEST_CASE("une image longue ne laisse aucun tic manquer aux autres", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsMatch first = MatchOf(rules, 0);
  SurvivorsMatch second = MatchOf(rules, 1);
  // Une image de 260 ms pousse 13 tics d'un coup, plus que la redondance n'en redit.
  constexpr std::uint32_t kLongFrameTicks = 13;
  REQUIRE(kLongFrameTicks > kLockstepRedundantTicks);
  for (std::uint32_t round = 0; round < 40; ++round) {
    const std::uint32_t ticks = round % 2 == 0 ? kLongFrameTicks : 1;
    for (std::uint32_t tick = 0; tick < ticks; ++tick) {
      (void)first.PushLocal(SurvivorsTeamInput{.heading = kEast});
      (void)second.PushLocal(SurvivorsTeamInput{.heading = kNorth});
    }
    Deliver(first, second, 0);
    Deliver(second, first, 1);
    first.Advance();
    second.Advance();
  }
  CHECK(first.confirmed().field().ticks > 200);
  CHECK(first.confirmed().field().ticks == second.confirmed().field().ticks);
  CHECK(SurvivorsDigest(first.confirmed().field()) == SurvivorsDigest(second.confirmed().field()));
}

TEST_CASE("la partie finie, la dernière entrée part sans attendre son tour", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  auto run = SurvivorsRun::Start(rules, DuoStart(rules));
  REQUIRE(run);
  run->Abandon();
  REQUIRE(run->field().phase == SurvivorsPhase::kOver);
  SurvivorsMatch match(std::move(*run), 0, 0);
  REQUIRE(match.PushLocal(SurvivorsTeamInput{}));
  REQUIRE(match.TakeFrame());
  REQUIRE(match.PushLocal(SurvivorsTeamInput{}));
  // Une trame part tous les deux tics, sauf quand plus aucune ne suivra.
  const auto last = match.TakeFrame();
  REQUIRE(last);
  CHECK(DecodeLockstepFrame(*last)->through_tick == 1U);
  CHECK_FALSE(match.TakeFrame());
}

TEST_CASE("le chemin rapide prédit le présent et ne confirme rien", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsMatch match = MatchOf(rules, 0);
  SurvivorsMatch other = MatchOf(rules, 1);
  REQUIRE(other.PushLocal(SurvivorsTeamInput{.heading = kNorth}));
  const auto frame = other.TakeFrame();
  REQUIRE(frame);
  REQUIRE(match.PushLocal(SurvivorsTeamInput{.heading = kEast}));
  match.Hear(1, *frame);
  match.Advance();
  // Sans le chemin fiable, ni tic confirmé, ni entrée connue.
  CHECK(match.confirmed().field().ticks == kInputDelayTicks);
  const FieldPoint ally_before = HeroAt(match.confirmed().field(), 1).position;
  CHECK_FALSE(HeroAt(match.present().field(), 1).position == ally_before);
}

TEST_CASE("une empreinte venue du chemin rapide ne juge personne", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsMatch first = MatchOf(rules, 0);
  SurvivorsMatch second = MatchOf(rules, 1);
  PlayBoth(first, second, 200, 1);
  const std::uint32_t tick =
      first.confirmed().field().ticks / kLockstepDigestEveryTicks * kLockstepDigestEveryTicks;
  std::vector<std::uint8_t> stale{0, 0, 0, 0, 0};
  for (std::size_t shift = 0; shift < 32; shift += 8) {
    stale.push_back(static_cast<std::uint8_t>((tick >> shift) & 0xFFU));
  }
  stale.insert(stale.end(), {1, 2, 3, 4, 5, 6, 7, 8});
  first.Hear(1, stale);
  first.Advance();
  CHECK_FALSE(first.diverged());
}

TEST_CASE("un départ brutal donne le même tic au robot, quoi qu'ait apporté l'UDP", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  // Le joueur 2 part : A a tout entendu par l'UDP, B rien. Les deux n'ont du TCP que
  // ses trames relayées, les mêmes.
  auto start3 = DuoStart(rules);
  start3.allies.push_back(start3.allies.front());
  auto three = [&](std::size_t local) {
    auto run = SurvivorsRun::Start(rules, start3);
    REQUIRE(run);
    return SurvivorsMatch(std::move(*run), local, kInputDelayTicks);
  };
  SurvivorsMatch hears_all = three(0);
  SurvivorsMatch hears_none = three(1);
  SurvivorsMatch leaver = three(2);
  const auto cross = [](SurvivorsMatch& from, std::size_t player, SurvivorsMatch& one,
                        SurvivorsMatch& other) {
    while (auto frame = from.TakeFrame()) {
      CHECK(one.Receive(player, *frame));
      CHECK(other.Receive(player, *frame));
    }
  };
  for (std::uint32_t tick = 0; tick < 40; ++tick) {
    (void)hears_all.PushLocal(SurvivorsTeamInput{.heading = kEast});
    (void)hears_none.PushLocal(SurvivorsTeamInput{.heading = kEast});
    (void)leaver.PushLocal(SurvivorsTeamInput{.heading = kNorth});
    cross(hears_all, 0, hears_none, leaver);
    cross(hears_none, 1, hears_all, leaver);
    // Ses vingt premiers tics passent partout ; les suivants, par l'UDP, chez un seul.
    while (auto frame = leaver.TakeFrame()) {
      if (tick < 20) {
        CHECK(hears_all.Receive(2, *frame));
        CHECK(hears_none.Receive(2, *frame));
      } else {
        hears_all.Hear(2, *frame);
      }
    }
    hears_all.Advance();
    hears_none.Advance();
    leaver.Advance();
  }
  hears_all.Leave(2);
  hears_none.Leave(2);
  for (std::uint32_t tick = 0; tick < 200; ++tick) {
    (void)hears_all.PushLocal(SurvivorsTeamInput{.heading = kEast});
    (void)hears_none.PushLocal(SurvivorsTeamInput{.heading = kEast});
    Deliver(hears_all, hears_none, 0);
    Deliver(hears_none, hears_all, 1);
    hears_all.Advance();
    hears_none.Advance();
  }
  REQUIRE(hears_all.confirmed().field().ticks > 100);
  REQUIRE(hears_all.confirmed().field().ticks == hears_none.confirmed().field().ticks);
  CHECK(SurvivorsDigest(hears_all.confirmed().field()) ==
        SurvivorsDigest(hears_none.confirmed().field()));
}

TEST_CASE("le premier client arrivé n'avance pas seul", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsMatch early = MatchOf(rules, 0);
  for (std::uint32_t tick = 0; tick < 200; ++tick) {
    (void)early.PushLocal(SurvivorsTeamInput{.heading = kEast});
    early.Advance();
  }
  CHECK(early.lead_ticks() <= kLockstepStartLeadTicks + 1U);
}

TEST_CASE("un bulletin déposé pour un autre vote ne compte pas", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsMatch first = MatchOf(rules, 0);
  SurvivorsMatch second = MatchOf(rules, 1);
  const auto both = [&](std::optional<SurvivorsAnswer> ballot, std::uint8_t vote) {
    SurvivorsTeamInput input;
    input.ballot = ballot;
    input.ballot_vote = vote;
    (void)first.PushLocal(input);
    (void)second.PushLocal(input);
    Deliver(first, second, 0);
    Deliver(second, first, 1);
    first.Advance();
    second.Advance();
  };
  constexpr std::uint32_t kMostTicks = 60000;
  std::uint32_t ticks = 0;
  while (first.confirmed().field().phase == SurvivorsPhase::kRunning && ticks < kMostTicks) {
    both(std::nullopt, 0);
    ++ticks;
  }
  REQUIRE(first.confirmed().field().phase == SurvivorsPhase::kChoosing);
  while (second.confirmed().field().phase != SurvivorsPhase::kChoosing && ticks < kMostTicks) {
    both(std::nullopt, 0);
    ++ticks;
  }
  const std::uint8_t vote = first.confirmed().field().vote_serial;
  const std::uint32_t vote_started = first.confirmed().field().ticks;
  const SurvivorsAnswer pick{.kind = AnswerKind::kPick};
  // Le numéro du vote d'avant : le vote reste ouvert, malgré l'unanimité.
  for (std::uint32_t tick = 0; tick < 40; ++tick) {
    both(pick, static_cast<std::uint8_t>(vote - 1U));
  }
  CHECK(first.confirmed().field().phase == SurvivorsPhase::kChoosing);
  CHECK(first.confirmed().field().vote_serial == vote);
  CHECK(first.confirmed().field().ticks > vote_started);
  for (std::uint32_t tick = 0; tick < 40; ++tick) both(pick, vote);
  CHECK((first.confirmed().field().phase != SurvivorsPhase::kChoosing ||
         first.confirmed().field().vote_serial != vote));
}

TEST_CASE("un robot ne vote pas : seul l'humain resté décide, et tout de suite", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsMatch first = MatchOf(rules, 0);
  SurvivorsMatch second = MatchOf(rules, 1);
  PlayBoth(first, second, 50, 0);
  first.Leave(1);
  constexpr std::uint32_t kMostTicks = 60000;
  std::uint32_t ticks = 0;
  while (first.confirmed().field().phase == SurvivorsPhase::kRunning && ticks < kMostTicks) {
    (void)first.PushLocal(SurvivorsTeamInput{.heading = kEast});
    first.Advance();
    ++ticks;
  }
  REQUIRE(first.confirmed().field().phase == SurvivorsPhase::kChoosing);
  const std::uint8_t vote = first.confirmed().field().vote_serial;
  // Sans bulletin de l'humain, le robot n'a pas tranché à sa place.
  for (std::uint32_t tick = 0; tick < 5; ++tick) {
    (void)first.PushLocal(SurvivorsTeamInput{.heading = kEast});
    first.Advance();
  }
  REQUIRE(first.confirmed().field().phase == SurvivorsPhase::kChoosing);
  CHECK(first.confirmed().field().vote_serial == vote);
  CHECK_FALSE(first.confirmed().field().ballots[1].has_value());
  // Son bulletin seul fait la majorité.
  SurvivorsTeamInput ballot{.heading = kEast};
  ballot.ballot = SurvivorsAnswer{.kind = AnswerKind::kPick};
  ballot.ballot_vote = vote;
  for (std::uint32_t tick = 0; tick < 30; ++tick) {
    (void)first.PushLocal(ballot);
    first.Advance();
  }
  CHECK((first.confirmed().field().phase != SurvivorsPhase::kChoosing ||
         first.confirmed().field().vote_serial != vote));
}

TEST_CASE("un joueur hostile n'entasse pas d'entrées sans borne", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsMatch match = MatchOf(rules, 0);
  // Des trames contiguës sans fin, sans que le joueur local n'avance.
  std::uint32_t through = kInputDelayTicks;
  for (std::uint32_t frame_index = 0; frame_index < 100; ++frame_index) {
    constexpr std::uint8_t kCount = 100;
    through = kInputDelayTicks + (frame_index + 1U) * kCount - 1U;
    std::vector<std::uint8_t> frame;
    for (std::size_t shift = 0; shift < 32; shift += 8) {
      frame.push_back(static_cast<std::uint8_t>((through >> shift) & 0xFFU));
    }
    frame.push_back(kCount);
    for (std::uint8_t input = 0; input < kCount; ++input) {
      frame.push_back(kEast);
      frame.push_back(kLockstepNoBallot);
      frame.push_back(0);
    }
    CHECK(match.Receive(1, frame));
  }
  // Le robot prend la suite au bout de ce qui a été gardé, pas au tic annoncé.
  match.Leave(1);
  for (std::uint32_t tick = 0; tick < 60; ++tick) {
    (void)match.PushLocal(SurvivorsTeamInput{.heading = kEast});
    match.Advance();
  }
  CHECK(match.confirmed().field().ticks > kInputDelayTicks);
}

TEST_CASE("l'empreinte des règles voit une valeur d'un sort", "[survivors]") {
  const SurvivorsRules& shipped = ShippedSurvivorsRules();
  std::vector<SurvivorsSkillRule> skills(shipped.skills.begin(), shipped.skills.end());
  std::vector<SkillLevel> levels(skills.front().levels.begin(), skills.front().levels.end());
  levels.front().damage += 1;
  skills.front().levels = levels;
  SurvivorsRules changed = shipped;
  changed.skills = skills;
  CHECK(SurvivorsRulesDigest(changed) != SurvivorsRulesDigest(shipped));
  SurvivorsRules tuned = shipped;
  tuned.tuning.gem_pull_accel_units += 1;
  CHECK(SurvivorsRulesDigest(tuned) != SurvivorsRulesDigest(shipped));
  CHECK(SurvivorsRulesDigest(shipped) == SurvivorsRulesDigest(ShippedSurvivorsRules()));
  std::vector<ClassRule> classes(shipped.classes.begin(), shipped.classes.end());
  classes.front().final_boss_hp_bonus_pct += 1;
  SurvivorsRules stronger_boss = shipped;
  stronger_boss.classes = classes;
  CHECK(SurvivorsRulesDigest(stronger_boss) != SurvivorsRulesDigest(shipped));
}

TEST_CASE("une empreinte différente dit que la partie a divergé", "[survivors]") {
  const SurvivorsRules& rules = ShippedSurvivorsRules();
  SurvivorsMatch first = MatchOf(rules, 0);
  SurvivorsMatch second = MatchOf(rules, 1);
  PlayBoth(first, second, 200, 1);
  REQUIRE_FALSE(first.diverged());
  // Une empreinte fausse à un tic que `first` a gardé.
  const std::uint32_t tick =
      first.confirmed().field().ticks / kLockstepDigestEveryTicks * kLockstepDigestEveryTicks;
  std::vector<std::uint8_t> frame{0, 0, 0, 0, 0};
  for (std::size_t shift = 0; shift < 32; shift += 8) {
    frame.push_back(static_cast<std::uint8_t>((tick >> shift) & 0xFFU));
  }
  frame.insert(frame.end(), {1, 2, 3, 4, 5, 6, 7, 8});
  REQUIRE(first.Receive(1, frame));
  CHECK(first.diverged());
}

}  // namespace
}  // namespace ro::sim
