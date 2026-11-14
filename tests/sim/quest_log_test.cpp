// LE JOURNAL DES QUÊTES : ajout, totaux, progression, retrait, activation.

#include "ro/sim/quest_log.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::sim::QuestHunt;
using ro::sim::QuestLog;
using ro::sim::QuestRecord;

namespace {

constexpr std::uint32_t kFirstQuest = 1001;
constexpr std::uint32_t kSecondQuest = 2002;
constexpr std::uint32_t kPoringId = 1002;

[[nodiscard]] QuestRecord Hunting(std::uint32_t quest_id) {
  QuestRecord quest{.quest_id = quest_id, .active = true};
  quest.hunts.push_back(QuestHunt{.hunt_id = quest_id * 1000, .mob_id = kPoringId});
  quest.hunts.push_back(QuestHunt{.hunt_id = quest_id * 1000 + 1, .mob_id = kPoringId});
  return quest;
}

}  // namespace

TEST_CASE("Chasse : achevée seulement avec un total connu", "[sim][quest]") {
  CHECK_FALSE(QuestHunt{.killed = 0, .total = 0}.done());
  CHECK_FALSE(QuestHunt{.killed = 3, .total = 0}.done());
  CHECK_FALSE(QuestHunt{.killed = 3, .total = 5}.done());
  CHECK(QuestHunt{.killed = 5, .total = 5}.done());
  CHECK(QuestHunt{.killed = 6, .total = 5}.done());
}

TEST_CASE("Journal : une quête ajoutée va en fin, et remplace la même sans changer de place",
          "[sim][quest]") {
  QuestLog log;
  log.Add(Hunting(kFirstQuest));
  log.Add(Hunting(kSecondQuest));
  REQUIRE(log.quests().size() == 2);
  CHECK(log.quests()[1].quest_id == kSecondQuest);

  QuestRecord again = Hunting(kFirstQuest);
  again.active = false;
  log.Add(again);
  REQUIRE(log.quests().size() == 2);
  CHECK(log.quests()[0].quest_id == kFirstQuest);
  CHECK_FALSE(log.quests()[0].active);
  CHECK(log.Find(kSecondQuest) != nullptr);
  CHECK(log.Find(9) == nullptr);
}

TEST_CASE("Journal : Replace remplace, Clear vide, la révision monte", "[sim][quest]") {
  QuestLog log;
  const std::uint64_t start = log.revision();
  log.Replace({Hunting(kFirstQuest)});
  CHECK(log.revision() > start);
  CHECK(log.quests().size() == 1);
  const std::uint64_t after_replace = log.revision();
  log.Clear();
  CHECK(log.quests().empty());
  CHECK(log.revision() > after_replace);
}

TEST_CASE("Totaux : posés par hunt_id, le monstre aussi, même à zéro", "[sim][quest]") {
  QuestLog log;
  log.Add(Hunting(kFirstQuest));
  const std::uint64_t before = log.revision();
  CHECK(log.SetHuntTotal(kFirstQuest * 1000 + 1, 0, 8, 2));
  const QuestHunt& hunt = log.Find(kFirstQuest)->hunts[1];
  CHECK(hunt.total == 8);
  CHECK(hunt.killed == 2);
  // Le monstre brut de la base, nul pour une chasse générique, écrase le
  // monstre de remplacement.
  CHECK(hunt.mob_id == 0);
  CHECK(log.Find(kFirstQuest)->hunts[0].mob_id == kPoringId);
  CHECK(log.revision() > before);

  CHECK(log.SetHuntTotal(kFirstQuest * 1000, 1003, 4, 0));
  CHECK(log.Find(kFirstQuest)->hunts[0].mob_id == 1003);
  CHECK_FALSE(log.SetHuntTotal(7'000'000, 1, 1, 0));
}

TEST_CASE("Progression : vraie seulement quand le compte augmente", "[sim][quest]") {
  QuestLog log;
  log.Add(Hunting(kFirstQuest));
  const std::uint32_t hunt = kFirstQuest * 1000;
  CHECK(log.UpdateHunt(kFirstQuest, hunt, 10, 1));
  CHECK(log.Find(kFirstQuest)->hunts[0].killed == 1);
  const std::uint64_t before = log.revision();
  // Un compte inchangé n'est pas un progrès, et ne touche pas à la révision.
  CHECK_FALSE(log.UpdateHunt(kFirstQuest, hunt, 10, 1));
  CHECK(log.revision() == before);
  // Un recul est posé, sans être un progrès.
  CHECK_FALSE(log.UpdateHunt(kFirstQuest, hunt, 10, 0));
  CHECK(log.Find(kFirstQuest)->hunts[0].killed == 0);
  // Une quête ou une chasse inconnue ne fait rien.
  CHECK_FALSE(log.UpdateHunt(99, hunt, 10, 5));
  CHECK_FALSE(log.UpdateHunt(kFirstQuest, 5, 10, 5));
}

TEST_CASE("Retrait et activation : faux pour une quête inconnue", "[sim][quest]") {
  QuestLog log;
  log.Add(Hunting(kFirstQuest));
  log.Add(Hunting(kSecondQuest));
  CHECK(log.SetActive(kFirstQuest, false));
  CHECK_FALSE(log.Find(kFirstQuest)->active);
  CHECK_FALSE(log.SetActive(99, true));

  CHECK(log.Remove(kFirstQuest));
  CHECK(log.Find(kFirstQuest) == nullptr);
  CHECK(log.quests().size() == 1);
  CHECK_FALSE(log.Remove(kFirstQuest));
}

TEST_CASE("Raison d'un retrait : la dernière dite, prise une fois, sans toucher au journal",
          "[sim][quest]") {
  using ro::sim::QuestEndReason;
  QuestLog log;
  log.Add(Hunting(kFirstQuest));
  const std::uint64_t before = log.revision();
  log.NoteEnding(kFirstQuest, {.reason = QuestEndReason::kErased});
  log.NoteEnding(kFirstQuest, {.reason = QuestEndReason::kReplaced, .next_quest_id = kSecondQuest});
  log.NoteEnding(kSecondQuest, {.reason = QuestEndReason::kCompleted});
  CHECK(log.revision() == before);

  const auto taken = log.TakeEnding(kFirstQuest);
  REQUIRE(taken.has_value());
  CHECK(taken->reason == QuestEndReason::kReplaced);
  CHECK(taken->next_quest_id == kSecondQuest);
  CHECK_FALSE(log.TakeEnding(kFirstQuest).has_value());
  CHECK(log.TakeEnding(kSecondQuest).has_value());
}

TEST_CASE("Raison d'un retrait : la liste entière et l'effacement l'oublient", "[sim][quest]") {
  using ro::sim::QuestEndReason;
  QuestLog log;
  log.NoteEnding(kFirstQuest, {.reason = QuestEndReason::kCompleted});
  log.Replace({Hunting(kFirstQuest)});
  CHECK_FALSE(log.TakeEnding(kFirstQuest).has_value());
  log.NoteEnding(kFirstQuest, {.reason = QuestEndReason::kRetired});
  log.Clear();
  CHECK_FALSE(log.TakeEnding(kFirstQuest).has_value());
}

TEST_CASE("Raison d'un retrait : seules les quatre valeurs du fil sont connues", "[sim][quest]") {
  constexpr std::uint8_t kNoReason = 0;
  constexpr std::uint8_t kFirstReason = 1;
  constexpr std::uint8_t kLastReason = 4;
  CHECK_FALSE(ro::sim::IsKnownQuestEndReason(kNoReason));
  for (std::uint8_t reason = kFirstReason; reason <= kLastReason; ++reason) {
    CHECK(ro::sim::IsKnownQuestEndReason(reason));
  }
  CHECK_FALSE(ro::sim::IsKnownQuestEndReason(kLastReason + 1));
}
