// CE QUE LE CLIENT RETIENT DES QUÊTES ET DU PANNEAU DE SUIVI : un aller-retour
// fidèle, et un fichier abîmé qui retombe sur les défauts sans rien casser.

#include "quest/quest_memory.h"

#include <cstddef>
#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "quest/tracker_memory.h"
#include "support/temp_directory.h"

using moonclient::QuestHistoryEntry;
using moonclient::QuestMemory;
using moonclient::RememberedQuests;
using moonclient::TrackerMemory;
using ro::test_support::TempDirectory;

TEST_CASE("Mémoire des quêtes : ce qu'on écrit se relit tel quel", "[app][quetes]") {
  const TempDirectory folder;
  QuestMemory written;
  RememberedQuests kept{.character = "Stingor",
                        .tracked = {1001, 2045},
                        .untracked = {3000},
                        .seen = {1001},
                        .seen_recommended = {97, 99},
                        .tab = ro::ui::QuestTab::kWaiting,
                        .sort = ro::ui::QuestSort::kTimeLeft,
                        .category = "daily",
                        .group_by_category = false};
  kept.received_unix_s = {{1001, 1700000000}, {2045, 1700000500}};
  kept.history.push_back(QuestHistoryEntry{
      .quest_id = 7, .title = "« Rançon » : la tortue", .left_unix_s = 1700001000});
  written.Remember(kept);
  REQUIRE(moonclient::SaveQuestMemory(folder.path(), written));

  QuestMemory read;
  moonclient::LoadQuestMemory(folder.path(), read);
  const RememberedQuests* back = read.Of("Stingor");
  REQUIRE(back != nullptr);
  CHECK(back->tracked == kept.tracked);
  CHECK(back->untracked == kept.untracked);
  CHECK(back->seen == kept.seen);
  CHECK(back->seen_recommended == kept.seen_recommended);
  CHECK(back->received_unix_s == kept.received_unix_s);
  REQUIRE(back->history.size() == 1);
  CHECK(back->history.front().title == "« Rançon » : la tortue");
  CHECK(back->history.front().left_unix_s == 1700001000);
  CHECK(back->tab == ro::ui::QuestTab::kWaiting);
  CHECK(back->sort == ro::ui::QuestSort::kTimeLeft);
  CHECK(back->category == "daily");
  CHECK_FALSE(back->group_by_category);
  CHECK(read.Of("Autre") == nullptr);
}

TEST_CASE("Mémoire des quêtes : un fichier abîmé laisse les défauts", "[app][quetes]") {
  QuestMemory broken;
  // 🔴 ET LA LECTURE LE DIT : c'est ce qui retient l'appelant de réécrire, par-
  // dessus le fichier de tous les personnages, la seule part du sien.
  CHECK_FALSE(
      moonclient::ParseQuestMemory("characters: [ { character: A, tab: 9, sort: -1,", broken));
  CHECK(broken.characters.empty());
  QuestMemory scalar;
  CHECK_FALSE(moonclient::ParseQuestMemory("juste un mot", scalar));
  // Un fichier vide, lui, ne retient rien : il n'y a rien à perdre.
  QuestMemory empty;
  CHECK(moonclient::ParseQuestMemory("", empty));

  QuestMemory odd;
  moonclient::ParseQuestMemory(
      "characters:\n  - character: A\n    tab: 9\n    sort: 7\n    tracked: [0, 5, 5]\n", odd);
  const RememberedQuests* kept = odd.Of("A");
  REQUIRE(kept != nullptr);
  // Un onglet ou un ordre hors bornes retombe au premier ; zéro et les doublons
  // ne sont pas des quêtes.
  CHECK(kept->tab == ro::ui::QuestTab::kActive);
  CHECK(kept->sort == ro::ui::QuestSort::kRecent);
  CHECK(kept->tracked == std::vector<std::uint32_t>{5});
}

TEST_CASE("Mémoire des quêtes : l'historique ne garde que les plus récentes", "[app][quetes]") {
  RememberedQuests kept{.character = "A"};
  for (std::uint32_t quest_id = 1; quest_id <= moonclient::kMostQuestHistory + 5; ++quest_id) {
    kept.history.push_back(QuestHistoryEntry{.quest_id = quest_id, .left_unix_s = quest_id});
  }
  QuestMemory written;
  written.Remember(kept);
  QuestMemory read;
  moonclient::ParseQuestMemory(moonclient::QuestMemoryText(written), read);
  const RememberedQuests* back = read.Of("A");
  REQUIRE(back != nullptr);
  REQUIRE(back->history.size() == moonclient::kMostQuestHistory);
  CHECK(back->history.front().quest_id == 6);
}

TEST_CASE("Mémoire des quêtes : une quête n'a qu'une ligne d'historique, la dernière",
          "[app][quetes]") {
  QuestMemory read;
  moonclient::ParseQuestMemory(
      "characters:\n  - character: A\n    history:\n"
      "      - { quest: 9, title: Q, left: 100 }\n"
      "      - { quest: 8, title: R, left: 150 }\n"
      "      - { quest: 9, title: Q, left: 200 }\n",
      read);
  const RememberedQuests* kept = read.Of("A");
  REQUIRE(kept != nullptr);
  REQUIRE(kept->history.size() == 2);
  CHECK(kept->history[0].quest_id == 8);
  CHECK(kept->history[1].quest_id == 9);
  CHECK(kept->history[1].left_unix_s == 200);
}

TEST_CASE("Mémoire des quêtes : la fin d'une quête se relit, l'étape suivante avec elle",
          "[app][quetes]") {
  using ro::sim::QuestEndReason;
  constexpr std::uint32_t kNextQuest = 12;
  RememberedQuests kept{.character = "A"};
  kept.history.push_back(QuestHistoryEntry{.quest_id = 1});
  kept.history.push_back(
      QuestHistoryEntry{.quest_id = 2, .ending = ro::sim::QuestEnding{QuestEndReason::kCompleted}});
  kept.history.push_back(
      QuestHistoryEntry{.quest_id = 3, .ending = ro::sim::QuestEnding{QuestEndReason::kErased}});
  kept.history.push_back(QuestHistoryEntry{
      .quest_id = 4, .ending = ro::sim::QuestEnding{QuestEndReason::kReplaced, kNextQuest}});
  kept.history.push_back(
      QuestHistoryEntry{.quest_id = 5, .ending = ro::sim::QuestEnding{QuestEndReason::kRetired}});
  QuestMemory written;
  written.Remember(kept);
  QuestMemory read;
  moonclient::ParseQuestMemory(moonclient::QuestMemoryText(written), read);
  const RememberedQuests* back = read.Of("A");
  REQUIRE(back != nullptr);
  REQUIRE(back->history.size() == kept.history.size());
  CHECK_FALSE(back->history[0].ending.has_value());
  for (std::size_t rank = 1; rank < kept.history.size(); ++rank) {
    REQUIRE(back->history[rank].ending.has_value());
    CHECK(back->history[rank].ending->reason == kept.history[rank].ending->reason);
    CHECK(back->history[rank].ending->next_quest_id == kept.history[rank].ending->next_quest_id);
  }
}

TEST_CASE("Mémoire des quêtes : une ligne d'avant, ou une fin inconnue, ne dit pas de fin",
          "[app][quetes]") {
  QuestMemory read;
  moonclient::ParseQuestMemory(
      "characters:\n  - character: A\n    history:\n"
      "      - { quest: 9, title: Q, left: 100 }\n"
      "      - { quest: 8, title: R, left: 150, ending: vanished }\n"
      "      - { quest: 7, title: S, left: 200, ending: completed, next: 6 }\n",
      read);
  const RememberedQuests* kept = read.Of("A");
  REQUIRE(kept != nullptr);
  REQUIRE(kept->history.size() == 3);
  CHECK_FALSE(kept->history[0].ending.has_value());
  CHECK_FALSE(kept->history[1].ending.has_value());
  REQUIRE(kept->history[2].ending.has_value());
  CHECK(kept->history[2].ending->reason == ro::sim::QuestEndReason::kCompleted);
  // Seule une étape franchie a une suite.
  CHECK(kept->history[2].ending->next_quest_id == 0);
}

TEST_CASE("Mémoire du suivi : réglages et places se relisent tels quels", "[app][quetes]") {
  const TempDirectory folder;
  TrackerMemory written;
  written.shown = false;
  written.look = ro::ui::TrackerLook{.background = ro::ui::TrackerBackground::kDark,
                                     .locked = true,
                                     .click_through = true,
                                     .split = true,
                                     .most_quests = 7,
                                     .track_new_quests = false,
                                     .current_map_first = false};
  written.places[1] = ro::ui::TrackerPlace{.x = 1200.0F, .y = 340.5F};
  REQUIRE(moonclient::SaveTrackerMemory(folder.path(), written));

  TrackerMemory read;
  moonclient::LoadTrackerMemory(folder.path(), read);
  CHECK_FALSE(read.shown);
  CHECK(read.look == written.look);
  CHECK_FALSE(read.places[0].has_value());
  CHECK(read.places[1] == written.places[1]);

  // Une place à moitié écrite reste absente, un fond hors bornes n'en est
  // aucun, et un nombre de quêtes hors bornes se ramène dans les bornes.
  TrackerMemory partial;
  moonclient::ParseTrackerMemory("background: 7\nx: 10\nquests_shown: 500\n", partial);
  CHECK(partial.look.background == ro::ui::TrackerBackground::kNone);
  CHECK_FALSE(partial.places[0].has_value());
  CHECK(partial.look.most_quests == ro::ui::kTrackerMostQuests);
  CHECK(partial.shown);
}

TEST_CASE("Mémoire du suivi : elle s'écrit en anglais", "[app][quetes]") {
  const TempDirectory folder;
  TrackerMemory written;
  written.places[1] = ro::ui::TrackerPlace{.x = 5.0F, .y = 6.0F};
  REQUIRE(moonclient::SaveTrackerMemory(folder.path(), written));
  TrackerMemory read;
  moonclient::LoadTrackerMemory(folder.path(), read);
  CHECK(read.places[1] == written.places[1]);
  // Le fichier relu ne porte plus aucune clé française : une nouvelle lecture
  // sous les seules clés anglaises rend la même chose.
  TrackerMemory english;
  moonclient::ParseTrackerMemory("shown: false\nachievements_x: 5\nachievements_y: 6\n", english);
  CHECK_FALSE(english.shown);
  CHECK(english.places[1] == written.places[1]);
}
