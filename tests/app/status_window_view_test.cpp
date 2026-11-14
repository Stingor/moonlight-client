// LA VUE DE LA PETITE FENÊTRE DE STATUT : les nombres de la fiche, la MATK
// remise à l'endroit, et la flèche traduite en montée de la fiche.

#include "character/status_window_view.h"

#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/guild.h"

namespace {

using moonclient::SheetActionOf;
using moonclient::StatusWindowViewOf;

}  // namespace

TEST_CASE("Statut : la MATK s'écrit minimum puis maximum", "[app][statut]") {
  // 🔴 LE SERVEUR ENVOIE LE MAXIMUM DANS `matk1`, à l'inverse des autres
  // paires : recopiée telle quelle, la fenêtre écrirait « 900 ~ 400 ».
  ro::ui::CharacterStatsView stats;
  stats.matk1 = 900;
  stats.matk2 = 400;
  const ro::ui::StatusWindowView view = StatusWindowViewOf(stats, {});
  CHECK(view.matk_min == 400);
  CHECK(view.matk_max == 900);
}

TEST_CASE("Statut : les autres paires gardent l'ordre du serveur", "[app][statut]") {
  ro::ui::CharacterStatsView stats;
  stats.atk1 = 120;
  stats.atk2 = 35;
  stats.def1 = 40;
  stats.def2 = 12;
  stats.flee1 = 210;
  stats.flee2 = 7;
  const ro::ui::StatusWindowView view = StatusWindowViewOf(stats, {});
  CHECK(view.atk1 == 120);
  CHECK(view.atk2 == 35);
  CHECK(view.def1 == 40);
  CHECK(view.def2 == 12);
  // L'esquive et l'esquive parfaite ne se confondent pas.
  CHECK(view.flee == 210);
  CHECK(view.perfect_dodge == 7);
}

TEST_CASE("Statut : le nom de guilde passe tel quel, vide sans guilde", "[app][statut]") {
  const ro::ui::CharacterStatsView stats;
  constexpr std::string_view kGuild = "Moonlight";
  CHECK(StatusWindowViewOf(stats, kGuild).guild_name == kGuild);
  CHECK(StatusWindowViewOf(stats, {}).guild_name.empty());
}

TEST_CASE("Statut : sans guilde, le nom est vide", "[app][statut]") {
  const ro::sim::GuildBook book;
  CHECK(moonclient::GuildNameOf(book).empty());
}

TEST_CASE("Statut : le nom vient du résumé, sinon de l'appartenance", "[app][statut]") {
  ro::sim::GuildBook book;
  book.TellBelonging(ro::sim::GuildBelonging{.guild_id = 7, .name = "Appartenance"});
  CHECK(moonclient::GuildNameOf(book) == "Appartenance");
  book.TellSummary(ro::sim::GuildSummary{.guild_id = 7, .name = "Résumé"});
  CHECK(moonclient::GuildNameOf(book) == "Résumé");
}

TEST_CASE("Statut : le nom est une COPIE, qui survit au livre réécrit", "[app][statut]") {
  // 🔴 LE RÉSEAU RÉÉCRIT LE LIVRE ENTRE DEUX IMAGES : quitter la guilde efface
  // le nom. Une vue gardée sur lui lirait une chaîne libérée ; la copie, elle,
  // garde ce qui a été composé.
  ro::sim::GuildBook book;
  book.TellBelonging(ro::sim::GuildBelonging{.guild_id = 7, .name = "Une guilde au long nom"});
  const std::string kept = moonclient::GuildNameOf(book);
  book.Leave();
  CHECK(kept == "Une guilde au long nom");
  CHECK(moonclient::GuildNameOf(book).empty());
}

TEST_CASE("Statut : la flèche monte par la route du « + » de la fiche", "[app][statut]") {
  const ro::ui::StatusWindowAction asked{
      .verb = ro::ui::StatusVerb::kRaiseAttribute, .attribute = 2, .amount = 1};
  const ro::ui::CharacterWindowAction sheet = SheetActionOf(asked);
  CHECK(sheet.verb == ro::ui::CharacterVerb::kRaiseAttribute);
  CHECK(sheet.attribute == 2);
  CHECK(sheet.amount == 1);
}

TEST_CASE("Statut : une fenêtre qui n'a rien demandé ne demande rien", "[app][statut]") {
  CHECK(SheetActionOf(ro::ui::StatusWindowAction{}).verb == ro::ui::CharacterVerb::kNothing);
}
