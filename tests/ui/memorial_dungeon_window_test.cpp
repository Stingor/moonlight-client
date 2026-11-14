// La fenêtre d'instance, sans dessin : la durée lisible, le nom des
// difficultés, et le titre de la fenêtre repliée.

#include "ro/ui/memorial_dungeon_window.h"

#include <cstring>

#include <catch2/catch_test_macros.hpp>

using ro::ui::MemorialCollapsedTitle;
using ro::ui::MemorialDungeonView;
using ro::ui::MemorialDuration;
using ro::ui::MemorialHardModeName;

TEST_CASE("Instance : une durée se lit d'un coup d'œil", "[ui][instance]") {
  CHECK(MemorialDuration(3723) == "1 h 02 min 03 s");
  CHECK(MemorialDuration(245) == "4 min 05 s");
  CHECK(MemorialDuration(12) == "12 s");
  // Une échéance dépassée ne se lit pas en négatif.
  CHECK(MemorialDuration(-5) == "0 s");
}

TEST_CASE("Instance : seuls les niveaux du menu ont un nom", "[ui][instance]") {
  REQUIRE(MemorialHardModeName(5) != nullptr);
  CHECK(std::strcmp(MemorialHardModeName(5), "Difficile") == 0);
  CHECK(MemorialHardModeName(4) == nullptr);
}

TEST_CASE("Instance : repliée, la barre dit le temps restant", "[ui][instance]") {
  MemorialDungeonView view;
  CHECK(MemorialCollapsedTitle(view) == "Instance");
  view.present = true;
  view.seconds_left = 245;
  CHECK(MemorialCollapsedTitle(view) == "Instance · 4 min 05 s");
  view.waiting = true;
  view.wait_position = 2;
  CHECK(MemorialCollapsedTitle(view) == "Instance · file : 2");
  // Ouverte sans limite, le nom seul.
  view.waiting = false;
  view.seconds_left.reset();
  CHECK(MemorialCollapsedTitle(view) == "Instance");
}
