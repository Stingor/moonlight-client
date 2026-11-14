// LA LISTE DES COMMANDES AU-DESSUS DE LA BARRE : ce qu'elle propose pour les
// lettres tapées, quand elle s'ouvre, et ce que chaque touche en fait.

#include "ro/ui/chat_command_hints.h"

#include <array>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using ro::ui::ChatCommandHint;
using ro::ui::ChatCommandKey;
using ro::ui::ChatCommandListState;
using ro::ui::ChatCommandListView;
using ro::ui::ChatCommandQuery;
using ro::ui::DismissChatCommandList;
using ro::ui::FollowChatCommandLine;
using ro::ui::MatchChatCommands;
using ro::ui::PickChatCommand;
using ro::ui::PressChatCommandKey;

namespace {

constexpr auto kHints = std::to_array<ChatCommandHint>({
    {.sigil = '/', .name = "sit", .aliases = "stand dkswrl", .help = "S'assied."},
    {.sigil = '/', .name = "savechat", .help = "Enregistre."},
    {.sigil = '/', .name = "deal", .aliases = "rjfo", .params = "<nom>", .takes_argument = true},
    {.sigil = '@', .name = "autolootmvp", .aliases = "mvp alootmvp", .help = "Autoloot."},
    {.sigil = '@', .name = "storeall", .params = "[<n>]", .takes_argument = true},
    {.sigil = '@',
     .name = "mapmove",
     .aliases = "rura warp",
     .params = "<carte>",
     .takes_argument = true},
});

[[nodiscard]] std::string Spelled(const ChatCommandListView& view, std::size_t row) {
  const ChatCommandHint& hint = kHints[view.matches[row].hint];
  return std::string(1, hint.sigil) + std::string(view.matches[row].spelled);
}

}  // namespace

TEST_CASE("La liste ne s'ouvre qu'en tête de ligne, sur le premier mot") {
  CHECK(ChatCommandQuery("/") == "/");
  CHECK(ChatCommandQuery("@sto") == "@sto");
  CHECK_FALSE(ChatCommandQuery("").has_value());
  CHECK_FALSE(ChatCommandQuery("salut /sit").has_value());
  CHECK_FALSE(ChatCommandQuery("bonjour @storeall").has_value());
  CHECK_FALSE(ChatCommandQuery("%/sit").has_value());
  // Dès qu'on écrit les arguments, elle se tait.
  CHECK_FALSE(ChatCommandQuery("/deal Bob").has_value());
  CHECK_FALSE(ChatCommandQuery("@storeall ").has_value());

  ChatCommandListState state;
  CHECK(FollowChatCommandLine(state, kHints, "/s").open);
  CHECK_FALSE(FollowChatCommandLine(state, kHints, "on dit /s").open);
  CHECK_FALSE(FollowChatCommandLine(state, kHints, "/sit ").open);
  // Rien qui commence ainsi : rien à montrer.
  CHECK_FALSE(FollowChatCommandLine(state, kHints, "/zzz").open);
}

TEST_CASE("Le filtre suit le début du nom, puis celui des alias, sans la casse") {
  const auto by_name = MatchChatCommands(kHints, "/S");
  REQUIRE(by_name.size() == 2);
  CHECK(by_name[0].spelled == "sit");
  CHECK(by_name[1].spelled == "savechat");
  CHECK_FALSE(by_name[0].by_alias);

  // « /st » : aucun nom, mais l'alias « stand » de /sit.
  const auto by_alias = MatchChatCommands(kHints, "/st");
  REQUIRE(by_alias.size() == 1);
  CHECK(by_alias[0].spelled == "stand");
  CHECK(by_alias[0].by_alias);
  CHECK(kHints[by_alias[0].hint].name == "sit");

  // Le sigil sépare les deux familles.
  const auto at = MatchChatCommands(kHints, "@");
  CHECK(at.size() == 3);
  // Sur un sigil seul, aucune commande ne paraît deux fois par ses alias.
  CHECK(MatchChatCommands(kHints, "/").size() == 3);

  // Les noms d'abord, les alias ensuite : « @m » trouve mapmove par son nom,
  // autolootmvp par son alias.
  const auto mixed = MatchChatCommands(kHints, "@m");
  REQUIRE(mixed.size() == 2);
  CHECK(mixed[0].spelled == "mapmove");
  CHECK(mixed[1].spelled == "mvp");
}

TEST_CASE("Tab complète la première rangée, ou celle qu'on a choisie") {
  ChatCommandListState state;
  ChatCommandListView view = FollowChatCommandLine(state, kHints, "/s");
  REQUIRE(view.open);
  auto answer = PressChatCommandKey(state, view, kHints, "/s", ChatCommandKey::kTab);
  CHECK(answer.consumed);
  CHECK(answer.line == "/sit");
  // Complétée, la liste ne se rouvre pas sur la ligne qu'elle a posée.
  CHECK_FALSE(FollowChatCommandLine(state, kHints, "/sit").open);
  // La première lettre de plus la rouvre.
  CHECK(FollowChatCommandLine(state, kHints, "/sa").open);

  // Une commande qui attend un argument prend une espace.
  ChatCommandListState other;
  view = FollowChatCommandLine(other, kHints, "@sto");
  answer = PressChatCommandKey(other, view, kHints, "@sto", ChatCommandKey::kTab);
  CHECK(answer.line == "@storeall ");
  // Par un alias, c'est l'alias qui se complète.
  ChatCommandListState third;
  view = FollowChatCommandLine(third, kHints, "@ru");
  answer = PressChatCommandKey(third, view, kHints, "@ru", ChatCommandKey::kTab);
  CHECK(answer.line == "@rura ");
}

TEST_CASE("Les flèches choisissent, et le choix revient en tête quand les lettres changent") {
  ChatCommandListState state;
  ChatCommandListView view = FollowChatCommandLine(state, kHints, "/");
  REQUIRE(view.matches.size() == 3);
  CHECK(PressChatCommandKey(state, view, kHints, "/", ChatCommandKey::kDown).consumed);
  CHECK(state.chosen == 1);
  CHECK(state.navigated);
  CHECK(PressChatCommandKey(state, view, kHints, "/", ChatCommandKey::kUp).consumed);
  CHECK(PressChatCommandKey(state, view, kHints, "/", ChatCommandKey::kUp).consumed);
  // ↑ depuis la première fait le tour par la fin.
  CHECK(state.chosen == 2);
  const auto answer = PressChatCommandKey(state, view, kHints, "/", ChatCommandKey::kTab);
  CHECK(answer.line == Spelled(view, 2) + " ");

  ChatCommandListState fresh;
  view = FollowChatCommandLine(fresh, kHints, "/");
  static_cast<void>(PressChatCommandKey(fresh, view, kHints, "/", ChatCommandKey::kDown));
  view = FollowChatCommandLine(fresh, kHints, "/s");
  CHECK(fresh.chosen == 0);
  CHECK_FALSE(fresh.navigated);
}

TEST_CASE("Entrée n'envoie la complétion qu'après un choix aux flèches") {
  ChatCommandListState state;
  ChatCommandListView view = FollowChatCommandLine(state, kHints, "/si");
  // Sans choix, Entrée appartient au champ : la ligne part telle quelle.
  auto answer = PressChatCommandKey(state, view, kHints, "/si", ChatCommandKey::kEnter);
  CHECK_FALSE(answer.consumed);
  CHECK_FALSE(answer.line.has_value());

  view = FollowChatCommandLine(state, kHints, "/s");
  static_cast<void>(PressChatCommandKey(state, view, kHints, "/s", ChatCommandKey::kDown));
  answer = PressChatCommandKey(state, view, kHints, "/s", ChatCommandKey::kEnter);
  CHECK(answer.consumed);
  CHECK(answer.line == "/savechat");
}

TEST_CASE("Échap ferme la liste, la ligne reste, et une frappe la rouvre") {
  ChatCommandListState state;
  ChatCommandListView view = FollowChatCommandLine(state, kHints, "/s");
  const auto answer = PressChatCommandKey(state, view, kHints, "/s", ChatCommandKey::kEscape);
  CHECK(answer.consumed);
  CHECK_FALSE(answer.line.has_value());
  CHECK_FALSE(FollowChatCommandLine(state, kHints, "/s").open);
  CHECK(FollowChatCommandLine(state, kHints, "/si").open);

  // Liste fermée, aucune touche ne lui appartient : Échap revient au champ.
  ChatCommandListState closed;
  view = FollowChatCommandLine(closed, kHints, "bonjour");
  CHECK_FALSE(
      PressChatCommandKey(closed, view, kHints, "bonjour", ChatCommandKey::kEscape).consumed);
  CHECK_FALSE(PressChatCommandKey(closed, view, kHints, "bonjour", ChatCommandKey::kTab).consumed);
  CHECK_FALSE(PressChatCommandKey(closed, view, kHints, "bonjour", ChatCommandKey::kUp).consumed);
}

TEST_CASE("Une ligne rappelée de l'historique n'ouvre pas la liste") {
  ChatCommandListState state;
  DismissChatCommandList(state, "/sit");
  const ChatCommandListView view = FollowChatCommandLine(state, kHints, "/sit");
  CHECK_FALSE(view.open);
  // ↑ suivante n'appartient donc pas à la liste : l'historique continue.
  CHECK_FALSE(PressChatCommandKey(state, view, kHints, "/sit", ChatCommandKey::kUp).consumed);
}

TEST_CASE("Le clic complète la rangée cliquée") {
  ChatCommandListState state;
  const ChatCommandListView view = FollowChatCommandLine(state, kHints, "@");
  CHECK(PickChatCommand(state, view, kHints, 1) == "@storeall ");
  CHECK(PickChatCommand(state, view, kHints, view.matches.size()).empty());
}

TEST_CASE("Huit rangées se voient, et le choix reste dans la fenêtre") {
  std::array<ChatCommandHint, 12> many{};
  std::array<std::string, 12> names{};
  for (std::size_t rank = 0; rank < many.size(); ++rank) {
    names[rank] = "cmd" + std::to_string(rank + 10);
    many[rank] = ChatCommandHint{.sigil = '/', .name = names[rank]};
  }
  ChatCommandListState state;
  const ChatCommandListView view = FollowChatCommandLine(state, many, "/c");
  for (int step = 0; step < 9; ++step) {
    static_cast<void>(PressChatCommandKey(state, view, many, "/c", ChatCommandKey::kDown));
  }
  CHECK(state.chosen == 9);
  CHECK(state.first_shown == 2);
  static_cast<void>(PressChatCommandKey(state, view, many, "/c", ChatCommandKey::kUp));
  static_cast<void>(PressChatCommandKey(state, view, many, "/c", ChatCommandKey::kUp));
  static_cast<void>(PressChatCommandKey(state, view, many, "/c", ChatCommandKey::kUp));
  CHECK(state.chosen == 6);
  CHECK(state.first_shown == 2);
}
