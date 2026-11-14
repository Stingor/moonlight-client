// LES CHOIX D'ARGUMENT DANS LA LISTE DES COMMANDES : quand la liste les ouvre,
// ce que les lettres tapées en retiennent, et ce que chaque touche en fait.

#include <array>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/core/search_key.h"
#include "ro/ui/chat_command_hints.h"

using ro::ui::ChatArgumentChoice;
using ro::ui::ChatArgumentQueryOf;
using ro::ui::ChatCommandHint;
using ro::ui::ChatCommandKey;
using ro::ui::ChatCommandListState;
using ro::ui::ChatCommandListView;
using ro::ui::ChatCommandRowCount;
using ro::ui::FollowChatCommandLine;
using ro::ui::MatchChatArguments;
using ro::ui::PickChatCommand;
using ro::ui::PressChatCommandKey;

namespace {

// Les clés de recherche, calculées comme `app` les calcule.
const std::array<std::string, 6> kKeys = {
    ro::core::SearchKey("Novice"),      ro::core::SearchKey("Knight"),
    ro::core::SearchKey("Lord Knight"), ro::core::SearchKey("White Smith Whitesmith"),
    ro::core::SearchKey("Rune Knight"), ro::core::SearchKey("Élémentaliste"),
};

const std::array<ChatArgumentChoice, 6> kJobs = {{
    {.value = "0", .label = "Novice", .search_key = kKeys[0]},
    {.value = "7", .label = "Knight", .search_key = kKeys[1]},
    {.value = "4008", .label = "Lord Knight", .search_key = kKeys[2]},
    {.value = "4011", .label = "White Smith", .detail = "Whitesmith", .search_key = kKeys[3]},
    {.value = "4054", .label = "Rune Knight", .search_key = kKeys[4]},
    {.value = "4261", .label = "Élémentaliste", .search_key = kKeys[5]},
}};

const std::array<ChatCommandHint, 3> kHints = {{
    {.sigil = '@',
     .name = "jobchange",
     .aliases = "job",
     .params = "<job name|ID>",
     .takes_argument = true,
     .argument_choices = kJobs},
    {.sigil = '@', .name = "joblevel", .params = "<n>", .takes_argument = true},
    {.sigil = '@', .name = "warp", .params = "<carte>", .takes_argument = true},
}};

[[nodiscard]] std::string ValueAt(const ChatCommandListView& view, std::size_t row) {
  return std::string(kJobs[view.choice_rows[row]].value);
}

}  // namespace

TEST_CASE("Les choix s'ouvrent après le nom ou l'alias suivi d'un blanc, casse indifférente") {
  for (const std::string_view line : {"@job ", "@jobchange ", "@JOB ", "@JobChange "}) {
    INFO(line);
    ChatCommandListState state;
    const ChatCommandListView view = FollowChatCommandLine(state, kHints, line);
    CHECK(view.open);
    REQUIRE(view.argument_of.has_value());
    CHECK(*view.argument_of == 0);
    CHECK(view.matches.empty());
    CHECK(ChatCommandRowCount(view) == kJobs.size());
  }
  // Sans blanc, c'est encore le nom qu'on écrit : la liste des commandes.
  ChatCommandListState state;
  const ChatCommandListView naming = FollowChatCommandLine(state, kHints, "@job");
  CHECK_FALSE(naming.argument_of.has_value());
  CHECK_FALSE(naming.matches.empty());
  // Un autre sigil ne mène pas à la commande du serveur.
  CHECK_FALSE(ChatArgumentQueryOf(kHints, "/job ").has_value());
  CHECK_FALSE(ChatArgumentQueryOf(kHints, "%job ").has_value());
  // Un préfixe du nom n'est pas le nom.
  CHECK_FALSE(ChatArgumentQueryOf(kHints, "@jo ").has_value());
}

TEST_CASE("Une commande sans source de choix se tait après le blanc") {
  ChatCommandListState state;
  CHECK_FALSE(FollowChatCommandLine(state, kHints, "@warp ").open);
  CHECK_FALSE(FollowChatCommandLine(state, kHints, "@joblevel 5").open);
  CHECK_FALSE(ChatArgumentQueryOf(kHints, "@warp prontera").has_value());
}

TEST_CASE("Les lettres tapées filtrent sur le nom comme sur le numéro") {
  const auto values = [](std::string_view typed) {
    std::string joined;
    for (const std::size_t rank : MatchChatArguments(kJobs, typed)) {
      joined += std::string(kJobs[rank].value) + ' ';
    }
    return joined;
  };
  // Le nom, n'importe où, casse indifférente.
  CHECK(values("kni") == "7 4008 4054 ");
  CHECK(values("KNI") == "7 4008 4054 ");
  CHECK(values("lord k") == "4008 ");
  // Le second nom compte aussi.
  CHECK(values("whitesm") == "4011 ");
  // Le numéro, par son début.
  CHECK(values("40") == "4008 4011 4054 ");
  CHECK(values("4008") == "4008 ");
  CHECK(values("08").empty());
  // UTF-8 : accents et capitales accentuées se replient.
  CHECK(values("élé") == "4261 ");
  CHECK(values("ELEM") == "4261 ");
  CHECK(values("zzz").empty());
  // Rien de tapé : tout.
  CHECK(MatchChatArguments(kJobs, "").size() == kJobs.size());

  ChatCommandListState state;
  const ChatCommandListView view = FollowChatCommandLine(state, kHints, "@job kni");
  REQUIRE(view.choice_rows.size() == 3);
  CHECK(ValueAt(view, 1) == "4008");
  // Rien ne répond : rien à montrer.
  CHECK_FALSE(FollowChatCommandLine(state, kHints, "@job zzz").open);
}

TEST_CASE("Tab insère le numéro à la place de l'argument tapé") {
  ChatCommandListState state;
  const std::string line = "@JOB lord";
  const ChatCommandListView view = FollowChatCommandLine(state, kHints, line);
  const auto answer = PressChatCommandKey(state, view, kHints, line, ChatCommandKey::kTab);
  CHECK(answer.consumed);
  REQUIRE(answer.line.has_value());
  CHECK(*answer.line == "@JOB 4008");
  // La ligne complétée ne rouvre pas la liste.
  CHECK_FALSE(FollowChatCommandLine(state, kHints, *answer.line).open);
}

TEST_CASE("Entrée insère le métier choisi aux flèches, sans envoyer la ligne") {
  ChatCommandListState state;
  const std::string line = "@jobchange 40";
  ChatCommandListView view = FollowChatCommandLine(state, kHints, line);
  // Sans choix aux flèches, Entrée envoie la ligne telle quelle.
  CHECK_FALSE(PressChatCommandKey(state, view, kHints, line, ChatCommandKey::kEnter).consumed);

  // 4008, 4011, 4054 : ↓ passe de la première rangée à la deuxième.
  CHECK(PressChatCommandKey(state, view, kHints, line, ChatCommandKey::kDown).consumed);
  view = FollowChatCommandLine(state, kHints, line);
  const auto answer = PressChatCommandKey(state, view, kHints, line, ChatCommandKey::kEnter);
  // ⚠ LA TOUCHE EST PRISE : le champ ne la voit pas, la ligne ne part pas.
  CHECK(answer.consumed);
  REQUIRE(answer.line.has_value());
  CHECK(*answer.line == "@jobchange 4011");
}

TEST_CASE("Échap ferme les choix sans toucher à la ligne") {
  ChatCommandListState state;
  const std::string line = "@job kn";
  const ChatCommandListView view = FollowChatCommandLine(state, kHints, line);
  REQUIRE(view.open);
  const auto answer = PressChatCommandKey(state, view, kHints, line, ChatCommandKey::kEscape);
  CHECK(answer.consumed);
  CHECK_FALSE(answer.line.has_value());
  CHECK_FALSE(FollowChatCommandLine(state, kHints, line).open);
  // La frappe suivante rouvre.
  CHECK(FollowChatCommandLine(state, kHints, "@job kni").open);
}

TEST_CASE("Compléter le nom de la commande ouvre aussitôt ses choix") {
  ChatCommandListState state;
  const ChatCommandListView naming = FollowChatCommandLine(state, kHints, "@jobc");
  REQUIRE(naming.matches.size() == 1);
  const std::string line = PickChatCommand(state, naming, kHints, 0);
  CHECK(line == "@jobchange ");
  const ChatCommandListView view = FollowChatCommandLine(state, kHints, line);
  CHECK(view.open);
  CHECK(view.argument_of.has_value());
  // Un clic sur un choix l'insère.
  CHECK(PickChatCommand(state, view, kHints, 2) == "@jobchange 4008");
}
