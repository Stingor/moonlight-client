// Les annonces d'objet, écrites : la phrase du client, l'objet en lien, une
// ligne dans la conversation et une bulle en haut de l'écran.

#include "world/item_broadcast_report.h"

#include <string>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/world.h"
#include "ro/vfs/item_link.h"

using moonclient::GradeLetter;
using moonclient::RareObtainLine;
using moonclient::RefineAnnounceLine;

namespace {

constexpr std::uint32_t kElunium = 985;
constexpr std::uint32_t kOldBlueBox = 603;

[[nodiscard]] std::string Link(std::uint32_t item_id) {
  return ro::vfs::BuildItemLink(ro::vfs::ItemLink{.item_id = item_id}, 0);
}

}  // namespace

TEST_CASE("Annonces d'objet : les grades, de D à SS", "[app][items]") {
  CHECK(GradeLetter(1) == "D");
  CHECK(GradeLetter(4) == "A");
  CHECK(GradeLetter(7) == "SS");
  CHECK(GradeLetter(0).empty());
  CHECK(GradeLetter(8).empty());
}

TEST_CASE("Annonces d'objet : la boîte, le monstre et le PNJ", "[app][items]") {
  ro::sim::RareItemObtain obtained{
      .source = 0, .player_name = "Stingor", .item_id = kElunium, .box_item_id = kOldBlueBox};
  CHECK(RareObtainLine(obtained) ==
        "Stingor a ouvert [" + Link(kOldBlueBox) + "] et obtenu [" + Link(kElunium) + "].");
  obtained.source = 1;
  obtained.monster_name = "Baphomet";
  CHECK(RareObtainLine(obtained) ==
        "Stingor a ouvert [Baphomet] et obtenu [" + Link(kElunium) + "].");
  obtained.source = 2;
  CHECK(RareObtainLine(obtained) == "Félicitations Stingor, [" + Link(kElunium) + "] obtenu !");
}

TEST_CASE("Annonces d'objet : le raffinage réussi ou manqué", "[app][items]") {
  ro::sim::RefineAnnounce refined{
      .player_name = "Stingor", .item_id = kElunium, .refine_level = 9, .success = true};
  CHECK(RefineAnnounceLine(refined) ==
        "[Stingor] a raffiné un équipement avec succès : [+9 " + Link(kElunium) + "] obtenu.");
  refined.success = false;
  CHECK(RefineAnnounceLine(refined) ==
        "[Stingor] n'a pas réussi à raffiner [+9 " + Link(kElunium) + "].");
}

TEST_CASE("Annonces d'objet : une ligne dans la conversation, une bulle en haut", "[app][items]") {
  ro::sim::ItemNews news;
  news.NoteGrade(ro::sim::GradeAnnounce{
      .player_name = "Stingor", .item_id = kElunium, .grade = 2, .success = true, .heard_ms = 50});
  ro::sim::World world;
  moonclient::ReportItemBroadcasts(news, world);
  REQUIRE(world.chat_lines().size() == 1);
  const std::string said = world.chat_lines()[0].text;
  CHECK(said == "[Stingor] a amélioré le grade avec succès et obtenu l'item [C grade " +
                    Link(kElunium) + "].");
  CHECK(world.chat_lines()[0].color == moonclient::kItemBroadcastColor);
  CHECK(world.chat_lines()[0].said_ms == 50);
  const std::vector<ro::sim::BalloonNotice> balloons = world.balloon_news().Take();
  REQUIRE(balloons.size() == 1);
  CHECK(balloons[0].lines[0].text == said);
  CHECK(balloons[0].lines[0].color == moonclient::kItemBroadcastColor);
  CHECK(balloons[0].heard_ms == 50);
  // Prises, les nouvelles ne se redisent pas.
  moonclient::ReportItemBroadcasts(news, world);
  CHECK(world.chat_lines().size() == 1);
}
