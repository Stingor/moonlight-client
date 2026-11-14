// LA LISTE DES DERNIERS CORRESPONDANTS SOUS LA BOX « PSEUDO » : son filtre et
// son clavier, sans fenêtre.

#include "ro/ui/chat_whisper_list.h"

#include <array>
#include <cstddef>
#include <vector>

#include <catch2/catch_test_macros.hpp>

namespace {

using ro::ui::ChatRecentWhisper;
using ro::ui::ChatWhisperKey;
using ro::ui::ChatWhisperListState;
using ro::ui::ChatWhisperListView;
using ro::ui::ChatWhisperWay;
using ro::ui::FollowWhisperList;
using ro::ui::MatchRecentWhispers;
using ro::ui::PressWhisperListKey;

const std::array<ChatRecentWhisper, 4> kNames = {
    ChatRecentWhisper{.name = "Bob", .way = ChatWhisperWay::kIncoming},
    ChatRecentWhisper{.name = "Élodie", .way = ChatWhisperWay::kOutgoing},
    ChatRecentWhisper{.name = "김철수", .way = ChatWhisperWay::kIncoming},
    ChatRecentWhisper{.name = "Robert", .way = ChatWhisperWay::kOutgoing}};

using Rows = std::vector<std::size_t>;

// La liste ouverte sur une box vide, comme au premier clic.
[[nodiscard]] ChatWhisperListView OpenOnEmpty(ChatWhisperListState& state) {
  ro::ui::OpenWhisperList(state, "");
  return FollowWhisperList(state, kNames, "");
}

}  // namespace

TEST_CASE("Filtre : les noms qui CONTIENNENT les lettres, sans la casse", "[ui][chuchotement]") {
  CHECK(MatchRecentWhispers(kNames, "") == Rows{0, 1, 2, 3});
  CHECK(MatchRecentWhispers(kNames, "OB") == Rows{0, 3});
  CHECK(MatchRecentWhispers(kNames, "bert") == Rows{3});
}

TEST_CASE("Filtre : accents et coréen se cherchent sans couper un caractère",
          "[ui][chuchotement]") {
  CHECK(MatchRecentWhispers(kNames, "ÉLO") == Rows{1});
  CHECK(MatchRecentWhispers(kNames, "elo") == Rows{1});
  CHECK(MatchRecentWhispers(kNames, "철") == Rows{2});
  // ⚠ UN OCTET DE SUITE SEUL ne répond à rien : il ne forme pas un caractère,
  // et ne doit pas trouver le milieu d'un autre.
  CHECK(MatchRecentWhispers(kNames, "\xB2").empty());
}

TEST_CASE("Filtre : aucun résultat, la liste ne s'ouvre pas", "[ui][chuchotement]") {
  ChatWhisperListState state;
  ro::ui::OpenWhisperList(state, "");
  const ChatWhisperListView view = FollowWhisperList(state, kNames, "zz");
  CHECK(view.rows.empty());
  CHECK_FALSE(view.open);
  // Et une liste vide ne s'ouvre pas non plus.
  CHECK_FALSE(FollowWhisperList(state, {}, "").open);
}

TEST_CASE("Filtre : le nom déjà dans la box ne filtre pas", "[ui][chuchotement]") {
  // ⚠ LA BOX GARDE LE DERNIER DESTINATAIRE : filtrer sur lui à la prise du
  // clavier ne laisserait que lui. Le filtre part à la première frappe.
  ChatWhisperListState state;
  ro::ui::OpenWhisperList(state, "Bob");
  CHECK(FollowWhisperList(state, kNames, "Bob").rows == Rows{0, 1, 2, 3});
  CHECK(FollowWhisperList(state, kNames, "Bobe").rows.empty());
  CHECK(FollowWhisperList(state, kNames, "Ro").rows == Rows{3});
}

TEST_CASE("Clavier : les flèches surlignent, en boucle", "[ui][chuchotement]") {
  ChatWhisperListState state;
  const ChatWhisperListView view = OpenOnEmpty(state);
  REQUIRE(view.open);
  CHECK(state.chosen == ro::ui::kNoWhisperChosen);
  CHECK(PressWhisperListKey(state, view, "", ChatWhisperKey::kDown).consumed);
  CHECK(state.chosen == 0);
  CHECK(PressWhisperListKey(state, view, "", ChatWhisperKey::kUp).consumed);
  CHECK(state.chosen == 3);
  CHECK(PressWhisperListKey(state, view, "", ChatWhisperKey::kDown).consumed);
  CHECK(state.chosen == 0);
}

TEST_CASE("Clavier : Entrée et Tab ne prennent qu'un nom surligné", "[ui][chuchotement]") {
  ChatWhisperListState state;
  ChatWhisperListView view = OpenOnEmpty(state);
  // 🔴 SANS NOM SURLIGNÉ, LA TOUCHE N'EST PAS À LA LISTE : Entrée doit envoyer
  // la ligne, Tab passer à la phrase.
  CHECK_FALSE(PressWhisperListKey(state, view, "", ChatWhisperKey::kEnter).consumed);
  CHECK_FALSE(PressWhisperListKey(state, view, "", ChatWhisperKey::kTab).consumed);
  CHECK(state.open);

  static_cast<void>(PressWhisperListKey(state, view, "", ChatWhisperKey::kDown));
  static_cast<void>(PressWhisperListKey(state, view, "", ChatWhisperKey::kDown));
  const ro::ui::ChatWhisperKeyAnswer entered =
      PressWhisperListKey(state, view, "", ChatWhisperKey::kEnter);
  CHECK(entered.consumed);
  CHECK(entered.picked == 1U);
  CHECK_FALSE(state.open);

  // Tab, sur une liste filtrée : le rang rendu est celui du nom, pas de la rangée.
  ro::ui::OpenWhisperList(state, "");
  view = FollowWhisperList(state, kNames, "rob");
  static_cast<void>(PressWhisperListKey(state, view, "rob", ChatWhisperKey::kDown));
  const ro::ui::ChatWhisperKeyAnswer tabbed =
      PressWhisperListKey(state, view, "rob", ChatWhisperKey::kTab);
  CHECK(tabbed.picked == 3U);
}

TEST_CASE("Clavier : une frappe efface le surlignage", "[ui][chuchotement]") {
  ChatWhisperListState state;
  const ChatWhisperListView view = OpenOnEmpty(state);
  static_cast<void>(PressWhisperListKey(state, view, "", ChatWhisperKey::kDown));
  REQUIRE(state.chosen == 0);
  static_cast<void>(FollowWhisperList(state, kNames, "o"));
  CHECK(state.chosen == ro::ui::kNoWhisperChosen);
}

TEST_CASE("Clavier : Échap ferme, la frappe suivante rouvre", "[ui][chuchotement]") {
  ChatWhisperListState state;
  const ChatWhisperListView view = OpenOnEmpty(state);
  CHECK(PressWhisperListKey(state, view, "", ChatWhisperKey::kEscape).consumed);
  CHECK_FALSE(FollowWhisperList(state, kNames, "").open);
  CHECK(FollowWhisperList(state, kNames, "b").open);
}

TEST_CASE("Ouverture : la box qui reprend le clavier après un envoi reste close",
          "[ui][chuchotement]") {
  ChatWhisperListState state;
  state.quiet_next = true;
  ro::ui::OpenWhisperList(state, "Bob");
  CHECK_FALSE(state.open);
  // La prise suivante, elle, déroule.
  ro::ui::OpenWhisperList(state, "Bob");
  CHECK(state.open);
}
