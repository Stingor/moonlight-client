// Les noms `:nom:` des émoticones du jeu.
//
// Ce qui se vérifie ici, c'est un CONTRAT : le nom qui voyage dans une ligne
// doit désigner la même image chez tous les clients et dans les GIF du relai
// Discord. La table se recopie donc ici en entier, et un nom changé, une ligne
// retirée ou un décalage d'une case font tomber l'essai.

#include "ro/ui/chat_emote_names.h"

#include <array>
#include <cstddef>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

using ro::ui::ChatEmoteAction;
using ro::ui::ChatEmoteName;
using ro::ui::kChatEmoteActions;

namespace {

// La table attendue, action par action ; vide pour une émoticone exclue.
constexpr std::array<std::string_view, 79> kExpected = {
    "surprise",
    "question",
    "whistle",
    "delight",
    "throb",
    "sweat",
    "aha",
    "fret",
    "anger",
    "money",
    "think",
    "scissor",
    "rock",
    "wrap",
    "flag",
    "thanks",
    "kek",
    "sorry",
    "smile",
    "profusely_sweat",
    "scratch",
    "best",
    "stare_about",
    "huk",
    "o",
    "x",
    "help",
    "go",
    "cry",
    "kik",
    "chup",
    "chupchup",
    "hng",
    "ok",
    "indonesia_flag",
    "stare",
    "hungry",
    "cool",
    "merong",
    "shy",
    "goodboy",
    "sptime",
    "sexy",
    "comeon",
    "sleepy",
    "congratulation",
    "hptime",
    "ph_flag",
    "my_flag",
    "si_flag",
    "br_flag",
    "spark",
    "confuse",
    "ohno",
    "hum",
    "blabla",
    "otl",
    "",
    "",
    "",
    "",
    "",
    "",
    "india_flag",
    "luv",
    "flag8",
    "flag9",
    "mobile",
    "mail",
    "antenna0",
    "antenna1",
    "antenna2",
    "antenna3",
    "hum2",
    "abs",
    "oops",
    "spit",
    "ene",
    "panic",
};

}  // namespace

TEST_CASE("Noms d'émoticones : la table entière, action par action", "[ui][chat][emote]") {
  STATIC_REQUIRE(kChatEmoteActions == kExpected.size());
  for (std::size_t action = 0; action < kExpected.size(); ++action) {
    INFO("action " << action);
    CHECK(ChatEmoteName(action) == kExpected[action]);
  }
}

TEST_CASE("Noms d'émoticones : le sprite, pas le protocole", "[ui][chat][emote]") {
  // Le sprite met un sifflement en 2, là où le protocole numérote déjà le
  // ravissement, et n'a pas de case pour l'icône « muet » : tout se décale.
  CHECK(ChatEmoteName(2) == "whistle");
  CHECK(ChatEmoteName(3) == "delight");
  CHECK(ChatEmoteName(4) == "throb");
  CHECK(ChatEmoteName(33) == "ok");
  CHECK(ChatEmoteName(34) == "indonesia_flag");
}

TEST_CASE("Noms d'émoticones : le garde-fou de longueur", "[ui][chat][emote]") {
  // Au-delà de la table : aucun nom, jamais un débordement.
  CHECK(ChatEmoteName(kChatEmoteActions).empty());
  CHECK(ChatEmoteName(static_cast<std::size_t>(-1)).empty());
  // La dernière case est pleine : une ligne retirée l'aurait vidée.
  CHECK_FALSE(ChatEmoteName(kChatEmoteActions - 1).empty());
}

TEST_CASE("Noms d'émoticones : un nom rend son action, l'aller-retour tient", "[ui][chat][emote]") {
  for (std::size_t action = 0; action < kChatEmoteActions; ++action) {
    const std::string_view name = ChatEmoteName(action);
    if (name.empty()) {
      continue;
    }
    INFO(name);
    REQUIRE(ChatEmoteAction(name).has_value());
    CHECK(*ChatEmoteAction(name) == action);
  }
}

TEST_CASE("Noms d'émoticones : une exclue, un inconnu ou une casse fausse ne rendent rien",
          "[ui][chat][emote]") {
  CHECK(ChatEmoteName(57).empty());
  CHECK_FALSE(ChatEmoteAction("").has_value());
  CHECK_FALSE(ChatEmoteAction("pogchamp").has_value());
  CHECK_FALSE(ChatEmoteAction("Smile").has_value());
  CHECK_FALSE(ChatEmoteAction("smile ").has_value());
}
