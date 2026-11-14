// LES QUÊTES CÔTÉ RÉSEAU : chaque paquet que le serveur envoie, et la demande
// d'activation que le client émet.

#include "ro/net/quest_log.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/proto/ledger_packets.h"
#include "ro/proto/map_packets.h"
#include "ro/proto/quest_packets.h"
#include "ro/sim/quest_log.h"

#include "support/net_frames.h"

using ro::net::ApplyQuestFrame;
using ro::net::FrameOutcome;
using ro::sim::QuestLog;
using ro::test_support::FrameOf;

namespace {

constexpr std::uint32_t kQuestId = 1001;
constexpr std::uint32_t kHuntId = kQuestId * 1000;
constexpr std::uint32_t kPoringId = 1002;

[[nodiscard]] ro::proto::QuestList TwoQuests() {
  ro::proto::QuestList list;
  list.quests.push_back(ro::proto::QuestEntry{.quest_id = 1000,
                                              .active = true,
                                              .started_unix_s = 1'700'000'000,
                                              .expires_unix_s = 1'700'086'400});
  ro::proto::QuestEntry hunting{.quest_id = 2000, .active = false};
  hunting.hunts.push_back(ro::proto::QuestHuntEntry{.hunt_id = 2'000'000,
                                                    .mob_id = kPoringId,
                                                    .level_min = 1,
                                                    .level_max = 99,
                                                    .killed = 3,
                                                    .total = 10,
                                                    .mob_name = "Poring"});
  hunting.hunts.push_back(ro::proto::QuestHuntEntry{
      .hunt_id = 2'000'001, .mob_type = 4, .mob_id = kPoringId, .total = 5, .mob_name = "Plant"});
  list.quests.push_back(hunting);
  return list;
}

// Une quête à deux chasses, telle que 0x09F9 la dit : sans total.
void AddHunting(QuestLog& log) {
  ro::proto::QuestAdded added{.quest_id = kQuestId, .active = true, .expires_unix_s = 77};
  added.hunts.push_back(ro::proto::QuestAddedHunt{
      .hunt_id = kHuntId, .mob_id = kPoringId, .level_max = 99, .mob_name = "Poring"});
  added.hunts.push_back(
      ro::proto::QuestAddedHunt{.hunt_id = kHuntId + 1, .mob_id = kPoringId, .mob_name = "Plant"});
  REQUIRE(ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestAdded(added)), log).outcome ==
          FrameOutcome::kApplied);
}

// Les raisons de 0x0F3B, telles que le serveur les écrit.
constexpr std::uint8_t kCompletedReason = 1;
constexpr std::uint8_t kErasedReason = 2;
constexpr std::uint8_t kReplacedReason = 3;
constexpr std::uint32_t kNextQuestId = 1002;

[[nodiscard]] std::vector<std::uint8_t> EndingFrame(std::uint32_t quest_id, std::uint8_t reason,
                                                    std::uint32_t next_quest_id) {
  return ro::proto::Encode(
      ro::proto::ZcMoonlightQuestEnd{.quest_id = static_cast<std::int32_t>(quest_id),
                                     .reason = reason,
                                     .next_quest_id = static_cast<std::int32_t>(next_quest_id)});
}

}  // namespace

TEST_CASE("Quêtes : la liste se lit, chasses comprises, et remplace la précédente",
          "[net][quest]") {
  const std::vector<std::uint8_t> bytes = ro::proto::EncodeQuestList(TwoQuests());
  QuestLog log;
  const ro::net::QuestFrameResult result = ApplyQuestFrame(FrameOf(bytes), log);
  CHECK(result.outcome == FrameOutcome::kApplied);
  CHECK(result.replaced);
  REQUIRE(log.quests().size() == 2);
  CHECK(log.quests()[0].quest_id == 1000);
  CHECK(log.quests()[0].active);
  CHECK(log.quests()[0].expires_unix_s == 1'700'086'400);
  CHECK(log.quests()[0].hunts.empty());
  const ro::sim::QuestRecord& hunting = log.quests()[1];
  CHECK_FALSE(hunting.active);
  REQUIRE(hunting.hunts.size() == 2);
  CHECK(hunting.hunts[0].mob_id == kPoringId);
  CHECK(hunting.hunts[0].killed == 3);
  CHECK(hunting.hunts[0].total == 10);
  CHECK(hunting.hunts[0].level_max == 99);
  CHECK(hunting.hunts[0].mob_name == "Poring");
  CHECK(hunting.hunts[1].mob_type == 4);

  REQUIRE(ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestList({})), log).outcome ==
          FrameOutcome::kApplied);
  CHECK(log.quests().empty());
}

TEST_CASE("Quêtes : une chasse générique de la liste n'est pas une chasse au Poring",
          "[net][quest]") {
  // Le serveur la dit sous le Poring, sa description à la place du nom.
  ro::proto::QuestList list;
  ro::proto::QuestEntry hunting{.quest_id = kQuestId, .active = true};
  hunting.hunts.push_back(ro::proto::QuestHuntEntry{
      .hunt_id = kHuntId, .mob_id = kPoringId, .total = 10, .mob_name = "Poring"});
  hunting.hunts.push_back(ro::proto::QuestHuntEntry{.hunt_id = kHuntId + 1,
                                                    .mob_type = 2,
                                                    .mob_id = kPoringId,
                                                    .total = 5,
                                                    .mob_name = "Brute, Small"});
  hunting.hunts.push_back(ro::proto::QuestHuntEntry{
      .hunt_id = kHuntId + 2, .mob_id = kPoringId, .total = 3, .mob_name = ""});
  list.quests.push_back(hunting);
  QuestLog log;
  REQUIRE(ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestList(list)), log).outcome ==
          FrameOutcome::kApplied);
  const ro::sim::QuestRecord* quest = log.Find(kQuestId);
  REQUIRE(quest != nullptr);
  REQUIRE(quest->hunts.size() == 3);
  CHECK(quest->hunts[0].mob_id == kPoringId);
  CHECK(quest->hunts[0].mob_name == "Poring");
  CHECK(quest->hunts[1].mob_id == 0);
  CHECK(quest->hunts[1].mob_name == "Brute, Small");
  CHECK(quest->hunts[1].total == 5);
  CHECK(quest->hunts[2].mob_id == 0);

  // La quête neuve se lit de même, avant que ses totaux ne le disent.
  ro::proto::QuestAdded added{.quest_id = kQuestId + 1, .active = true};
  added.hunts.push_back(ro::proto::QuestAddedHunt{
      .hunt_id = kHuntId + 1000, .mob_id = kPoringId, .mob_name = "Fire Element"});
  REQUIRE(ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestAdded(added)), log).outcome ==
          FrameOutcome::kApplied);
  CHECK(log.Find(kQuestId + 1)->hunts[0].mob_id == 0);
}

TEST_CASE("Quêtes : une queue qui ne se découpe pas est indéchiffrable", "[net][quest]") {
  std::vector<std::uint8_t> bytes = ro::proto::EncodeQuestList(TwoQuests());
  bytes.pop_back();
  QuestLog log;
  CHECK(ApplyQuestFrame(FrameOf(bytes), log).outcome == FrameOutcome::kUndecodable);
  CHECK(log.quests().empty());
}

TEST_CASE("Quêtes : une quête neuve s'ajoute, puis ses totaux se posent", "[net][quest]") {
  QuestLog log;
  ro::proto::QuestAdded added{.quest_id = kQuestId, .active = true, .expires_unix_s = 77};
  added.hunts.push_back(
      ro::proto::QuestAddedHunt{.hunt_id = kHuntId, .mob_id = kPoringId, .mob_name = "Poring"});
  const ro::net::QuestFrameResult add_result =
      ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestAdded(added)), log);
  CHECK(add_result.outcome == FrameOutcome::kApplied);
  CHECK(add_result.added == kQuestId);
  REQUIRE(log.Find(kQuestId) != nullptr);
  CHECK(log.Find(kQuestId)->expires_unix_s == 77);
  CHECK(log.Find(kQuestId)->hunts[0].mob_name == "Poring");
  CHECK(log.Find(kQuestId)->hunts[0].total == 0);

  ro::proto::QuestHuntTotals totals;
  totals.entries = {{.quest_or_hunt_id = kHuntId, .mob_id = 0, .total = 10, .killed = 2}};
  CHECK(ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestHuntTotals(totals)), log).outcome ==
        FrameOutcome::kApplied);
  CHECK(log.Find(kQuestId)->hunts[0].total == 10);
  CHECK(log.Find(kQuestId)->hunts[0].killed == 2);
  CHECK(log.Find(kQuestId)->hunts[0].mob_id == 0);

  // Des totaux qu'aucune quête ne porte ne changent rien.
  totals.entries = {{.quest_or_hunt_id = 9'999'000, .total = 1}};
  CHECK(ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestHuntTotals(totals)), log).outcome ==
        FrameOutcome::kIgnored);
}

TEST_CASE("Quêtes : des totaux sous l'identifiant de la quête se rangent par rang",
          "[net][quest]") {
  QuestLog log;
  ro::proto::QuestAdded added{.quest_id = kQuestId, .active = true};
  added.hunts.push_back(
      ro::proto::QuestAddedHunt{.hunt_id = kHuntId, .mob_id = kPoringId, .mob_name = "Poring"});
  added.hunts.push_back(ro::proto::QuestAddedHunt{
      .hunt_id = kHuntId + 1, .mob_id = kPoringId, .mob_name = "Small, Fire Element"});
  REQUIRE(ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestAdded(added)), log).outcome ==
          FrameOutcome::kApplied);

  // LA FORME QUE LE CLIENT D'ORIGINE LIT : la quête en tête, et le monstre
  // brut — nul pour la seconde, générique.
  ro::proto::QuestHuntTotals totals;
  totals.entries = {{.quest_or_hunt_id = kQuestId, .mob_id = kPoringId, .total = 10, .killed = 1},
                    {.quest_or_hunt_id = kQuestId, .mob_id = 0, .total = 5, .killed = 3}};
  CHECK(ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestHuntTotals(totals)), log).outcome ==
        FrameOutcome::kApplied);
  const ro::sim::QuestRecord* quest = log.Find(kQuestId);
  REQUIRE(quest != nullptr);
  CHECK(quest->hunts[0].total == 10);
  CHECK(quest->hunts[0].mob_id == kPoringId);
  CHECK(quest->hunts[1].total == 5);
  CHECK(quest->hunts[1].killed == 3);
  CHECK(quest->hunts[1].mob_id == 0);
}

TEST_CASE("Quêtes : la progression ne rend que les comptes qui ont augmenté", "[net][quest]") {
  QuestLog log;
  AddHunting(log);
  ro::proto::QuestHuntProgress progress;
  progress.entries = {{.quest_id = kQuestId, .hunt_id = kHuntId, .total = 10, .killed = 4},
                      {.quest_id = kQuestId, .hunt_id = kHuntId + 1, .total = 5, .killed = 0}};
  ro::net::QuestFrameResult result =
      ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestHuntProgress(progress)), log);
  CHECK(result.outcome == FrameOutcome::kApplied);
  REQUIRE(result.progressed.size() == 1);
  CHECK(result.progressed[0].quest_id == kQuestId);
  CHECK(result.progressed[0].hunt_rank == 0);
  CHECK(result.progressed[0].killed == 4);
  CHECK(result.progressed[0].total == 10);
  CHECK(log.Find(kQuestId)->hunts[1].total == 5);

  // CHAQUE CHASSE REDITE SE RAPPORTE, qu'elle ait bougé ou non.
  REQUIRE(result.reported.size() == 2);
  CHECK(result.reported[1].hunt_rank == 1);
  CHECK(result.reported[1].killed == 0);
  CHECK(result.reported[1].total == 5);

  // La même trame redite : rien n'augmente, mais tout se rapporte encore.
  result = ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestHuntProgress(progress)), log);
  CHECK(result.outcome == FrameOutcome::kApplied);
  CHECK(result.progressed.empty());
  CHECK(result.reported.size() == 2);

  // Une chasse ou une quête que le journal ne connaît pas ne se rapporte pas.
  ro::proto::QuestHuntProgress strangers;
  strangers.entries = {{.quest_id = kQuestId, .hunt_id = kHuntId + 7, .total = 3, .killed = 1},
                       {.quest_id = 99, .hunt_id = 99'000, .total = 3, .killed = 1}};
  result = ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestHuntProgress(strangers)), log);
  CHECK(result.reported.empty());

  // Le rang suit la chasse, non l'ordre de la trame.
  progress.entries = {{.quest_id = kQuestId, .hunt_id = kHuntId + 1, .total = 5, .killed = 2}};
  result = ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestHuntProgress(progress)), log);
  REQUIRE(result.progressed.size() == 1);
  CHECK(result.progressed[0].hunt_rank == 1);

  // Sans entrée, la trame est lue et ne montre rien.
  result = ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestHuntProgress({})), log);
  CHECK(result.outcome == FrameOutcome::kApplied);
  CHECK(result.progressed.empty());
}

TEST_CASE("Quêtes : retrait et activation", "[net][quest]") {
  QuestLog log;
  AddHunting(log);

  ro::net::QuestFrameResult result = ApplyQuestFrame(
      FrameOf(ro::proto::EncodeQuestActiveNotice({.quest_id = kQuestId, .active = false})), log);
  CHECK(result.outcome == FrameOutcome::kApplied);
  CHECK(result.toggled == kQuestId);
  CHECK_FALSE(log.Find(kQuestId)->active);
  result = ApplyQuestFrame(
      FrameOf(ro::proto::EncodeQuestActiveNotice({.quest_id = 99, .active = true})), log);
  CHECK(result.outcome == FrameOutcome::kIgnored);
  CHECK_FALSE(result.toggled.has_value());

  result = ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestRemoved({.quest_id = kQuestId})), log);
  CHECK(result.outcome == FrameOutcome::kApplied);
  CHECK(result.removed == kQuestId);
  CHECK(result.deleted == kQuestId);
  CHECK(log.quests().empty());
  // Hors du journal, le retrait ne change rien mais se nomme encore : le client
  // d'origine l'annonce.
  result = ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestRemoved({.quest_id = kQuestId})), log);
  CHECK(result.outcome == FrameOutcome::kIgnored);
  CHECK_FALSE(result.removed.has_value());
  CHECK(result.deleted == kQuestId);
}

TEST_CASE("Quêtes : un ajout d'une quête déjà au journal le dit", "[net][quest]") {
  QuestLog log;
  ro::proto::QuestAdded added{.quest_id = kQuestId, .active = true};
  ro::net::QuestFrameResult result =
      ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestAdded(added)), log);
  CHECK(result.added == kQuestId);
  CHECK_FALSE(result.added_was_listed);
  added.active = false;
  result = ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestAdded(added)), log);
  CHECK(result.added == kQuestId);
  CHECK(result.added_was_listed);
}

TEST_CASE("Quêtes : un opcode étranger n'est pas traité, une trame tronquée est indéchiffrable",
          "[net][quest]") {
  QuestLog log;
  CHECK(ApplyQuestFrame(FrameOf(ro::proto::EncodeExpNotice({})), log).outcome ==
        FrameOutcome::kUnhandled);

  std::vector<std::uint8_t> removed = ro::proto::EncodeQuestRemoved({.quest_id = kQuestId});
  removed.pop_back();
  CHECK(ApplyQuestFrame(FrameOf(removed), log).outcome == FrameOutcome::kUndecodable);
  std::vector<std::uint8_t> notice = ro::proto::EncodeQuestActiveNotice({.quest_id = kQuestId});
  notice.pop_back();
  CHECK(ApplyQuestFrame(FrameOf(notice), log).outcome == FrameOutcome::kUndecodable);
  std::vector<std::uint8_t> added = ro::proto::EncodeQuestAdded({.quest_id = kQuestId});
  added.pop_back();
  CHECK(ApplyQuestFrame(FrameOf(added), log).outcome == FrameOutcome::kUndecodable);
  std::vector<std::uint8_t> progress = ro::proto::EncodeQuestHuntProgress({});
  progress.pop_back();
  CHECK(ApplyQuestFrame(FrameOf(progress), log).outcome == FrameOutcome::kUndecodable);
  std::vector<std::uint8_t> totals = ro::proto::EncodeQuestHuntTotals({});
  totals.pop_back();
  CHECK(ApplyQuestFrame(FrameOf(totals), log).outcome == FrameOutcome::kUndecodable);
  std::vector<std::uint8_t> ending = EndingFrame(kQuestId, kCompletedReason, 0);
  ending.pop_back();
  CHECK(ApplyQuestFrame(FrameOf(ending), log).outcome == FrameOutcome::kUndecodable);
}

TEST_CASE("Quêtes : la raison dite par 0x0F3B habille le retrait qui suit", "[net][quest]") {
  QuestLog log;
  AddHunting(log);
  REQUIRE(
      ApplyQuestFrame(FrameOf(EndingFrame(kQuestId, kReplacedReason, kNextQuestId)), log).outcome ==
      FrameOutcome::kApplied);
  // La raison seule ne retire rien.
  CHECK(log.Find(kQuestId) != nullptr);

  ro::net::QuestFrameResult result =
      ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestRemoved({.quest_id = kQuestId})), log);
  CHECK(result.removed == kQuestId);
  REQUIRE(result.ending.has_value());
  CHECK(result.ending->reason == ro::sim::QuestEndReason::kReplaced);
  CHECK(result.ending->next_quest_id == kNextQuestId);

  // Prise une fois : un second retrait n'a plus de raison.
  AddHunting(log);
  result = ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestRemoved({.quest_id = kQuestId})), log);
  CHECK_FALSE(result.ending.has_value());
}

TEST_CASE("Quêtes : la raison se prend même quand la quête n'est plus au journal", "[net][quest]") {
  QuestLog log;
  REQUIRE(ApplyQuestFrame(FrameOf(EndingFrame(kQuestId, kCompletedReason, 0)), log).outcome ==
          FrameOutcome::kApplied);
  ro::net::QuestFrameResult result =
      ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestRemoved({.quest_id = kQuestId})), log);
  CHECK(result.outcome == FrameOutcome::kIgnored);
  CHECK(result.ending.has_value());
  // Elle ne reste pas pour habiller la quête quand elle reviendra.
  AddHunting(log);
  result = ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestRemoved({.quest_id = kQuestId})), log);
  CHECK(result.removed == kQuestId);
  CHECK_FALSE(result.ending.has_value());
}

TEST_CASE(
    "Quêtes : une raison inconnue ne dit rien, une étape suivante n'est lue que pour un "
    "remplacement",
    "[net][quest]") {
  QuestLog log;
  AddHunting(log);
  constexpr std::uint8_t kFutureReason = 9;
  CHECK(ApplyQuestFrame(FrameOf(EndingFrame(kQuestId, kFutureReason, 0)), log).outcome ==
        FrameOutcome::kIgnored);
  CHECK(ApplyQuestFrame(FrameOf(EndingFrame(0, kCompletedReason, 0)), log).outcome ==
        FrameOutcome::kIgnored);
  REQUIRE(ApplyQuestFrame(FrameOf(EndingFrame(kQuestId, kCompletedReason, kNextQuestId)), log)
              .outcome == FrameOutcome::kApplied);
  const ro::net::QuestFrameResult result =
      ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestRemoved({.quest_id = kQuestId})), log);
  REQUIRE(result.ending.has_value());
  CHECK(result.ending->reason == ro::sim::QuestEndReason::kCompleted);
  CHECK(result.ending->next_quest_id == 0);
}

TEST_CASE("Quêtes : la liste entière oublie les raisons en attente", "[net][quest]") {
  QuestLog log;
  AddHunting(log);
  REQUIRE(ApplyQuestFrame(FrameOf(EndingFrame(kQuestId, kErasedReason, 0)), log).outcome ==
          FrameOutcome::kApplied);
  REQUIRE(ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestList(TwoQuests())), log).outcome ==
          FrameOutcome::kApplied);
  AddHunting(log);
  const ro::net::QuestFrameResult result =
      ApplyQuestFrame(FrameOf(ro::proto::EncodeQuestRemoved({.quest_id = kQuestId})), log);
  CHECK_FALSE(result.ending.has_value());
}
