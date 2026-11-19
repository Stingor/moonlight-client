// Le titre de la fenêtre : le hash du commit bâti, et ce qui l'entoure.

#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/build_stamp.h"
#include "ro/sim/world.h"

#include "world/map_stage.h"

namespace {

using moonclient::kModifiedTreeTitleMark;
using moonclient::kTestServerTitleMark;
using moonclient::kWindowTitle;
using moonclient::WindowTitle;
using moonclient::WindowTitleMarks;

[[nodiscard]] std::string ExpectedRevision() {
  std::string revision(ro::core::kBuildCommit);
  if (ro::core::kBuildTree != ro::core::kBuildTreeClean) {
    revision += kModifiedTreeTitleMark;
  }
  return revision;
}

TEST_CASE("le titre porte le hash du commit, pas la version", "[app][window_title]") {
  CHECK(WindowTitle(WindowTitleMarks{}) == std::string(kWindowTitle) + " " + ExpectedRevision());
}

TEST_CASE("masquer la révision laisse le nom du jeu seul", "[app][window_title]") {
  CHECK(WindowTitle(WindowTitleMarks{.revision_shown = false}) == kWindowTitle);
}

TEST_CASE("un serveur test garde sa marque en tête, révision masquée ou non",
          "[app][window_title]") {
  CHECK(WindowTitle(WindowTitleMarks{.test_server = true}) ==
        std::string(kTestServerTitleMark) + std::string(kWindowTitle) + " " + ExpectedRevision());
  CHECK(WindowTitle(WindowTitleMarks{.revision_shown = false, .test_server = true}) ==
        std::string(kTestServerTitleMark) + std::string(kWindowTitle));
}

TEST_CASE("une mise à jour disponible se dit en queue, pseudo et carte en tête ou non",
          "[app][window_title]") {
  const std::string notice =
      std::string(moonclient::kUpdateTitleSeparator) + "mise à jour disponible";
  CHECK(WindowTitle(WindowTitleMarks{.revision_shown = false, .update_available = true}) ==
        std::string(kWindowTitle) + notice);
  ro::sim::World world;
  world.set_player_name("Stingor");
  world.EnterMap("prontera", 0, 0);
  CHECK(WindowTitle(true, world,
                    WindowTitleMarks{.revision_shown = false, .update_available = true}) ==
        "Stingor - prontera - " + std::string(kWindowTitle) + notice);
}

TEST_CASE("le pseudo et la carte et la révision se masquent séparément", "[app][window_title]") {
  ro::sim::World world;
  world.set_player_name("Stingor");
  world.EnterMap("prontera", 0, 0);
  const std::string names = "Stingor - prontera - ";

  CHECK(WindowTitle(true, world, WindowTitleMarks{}) ==
        names + std::string(kWindowTitle) + " " + ExpectedRevision());
  CHECK(WindowTitle(true, world, WindowTitleMarks{.revision_shown = false}) ==
        names + std::string(kWindowTitle));
  CHECK(WindowTitle(false, world, WindowTitleMarks{.revision_shown = false}) == kWindowTitle);
}

}  // namespace
