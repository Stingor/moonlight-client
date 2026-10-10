// LES NOMS `:nom:` DES ÉMOTICONES DU JEU. Voir l'en-tête.

#include "ro/ui/chat_emote_names.h"

#include <array>

namespace ro::ui {
namespace {

// Les noms, dans l'ordre des ACTIONS du sprite des émoticones.
//
// 🔴 CE N'EST PAS L'ORDRE DU PROTOCOLE. Le sprite place un sifflement en 2 là
// où le serveur numérote déjà le ravissement, et il n'a aucune case pour
// l'icône « muet » que le protocole compte : tout ce qui la suit occupe ici
// l'action précédente. L'index ne sert qu'à DESSINER ; ce qui voyage, c'est le
// nom.
//
// 🔴 LES NOMS SONT UN CONTRAT : les GIF du relai Discord portent ces noms-là,
// et une ligne écrite par un autre client doit désigner la même image ici.
// Renommer une entrée casse le pont.
//
// ⚠ EXCLURE UNE ÉMOTICONE, c'est vider son nom — jamais retirer la ligne, ce
// qui décalerait toutes les suivantes d'une action. Vide, elle n'est plus
// reconnue en `:nom:` et ne se propose plus.
constexpr std::array<std::string_view, kChatEmoteActions> kNames = {
    "surprise",         // 0
    "question",         // 1
    "whistle",          // 2
    "delight",          // 3
    "throb",            // 4
    "sweat",            // 5
    "aha",              // 6
    "fret",             // 7
    "anger",            // 8
    "money",            // 9
    "think",            // 10
    "scissor",          // 11
    "rock",             // 12
    "wrap",             // 13
    "flag",             // 14
    "thanks",           // 15
    "kek",              // 16
    "sorry",            // 17
    "smile",            // 18
    "profusely_sweat",  // 19
    "scratch",          // 20
    "best",             // 21
    "stare_about",      // 22
    "huk",              // 23
    "o",                // 24
    "x",                // 25
    "help",             // 26
    "go",               // 27
    "cry",              // 28
    "kik",              // 29
    "chup",             // 30
    "chupchup",         // 31
    "hng",              // 32
    "ok",               // 33
    "indonesia_flag",   // 34
    "stare",            // 35
    "hungry",           // 36
    "cool",             // 37
    "merong",           // 38
    "shy",              // 39
    "goodboy",          // 40
    "sptime",           // 41
    "sexy",             // 42
    "comeon",           // 43
    "sleepy",           // 44
    "congratulation",   // 45
    "hptime",           // 46
    "ph_flag",          // 47
    "my_flag",          // 48
    "si_flag",          // 49
    "br_flag",          // 50
    "spark",            // 51
    "confuse",          // 52
    "ohno",             // 53
    "hum",              // 54
    "blabla",           // 55
    "otl",              // 56
    "",                 // 57
    "",                 // 58
    "",                 // 59
    "",                 // 60
    "",                 // 61
    "",                 // 62
    "india_flag",       // 63
    "luv",              // 64
    "flag8",            // 65
    "flag9",            // 66
    "mobile",           // 67
    "mail",             // 68
    "antenna0",         // 69
    "antenna1",         // 70
    "antenna2",         // 71
    "antenna3",         // 72
    "hum2",             // 73
    "abs",              // 74
    "oops",             // 75
    "spit",             // 76
    "ene",              // 77
    "panic",            // 78
};

// LE GARDE-FOU DU SEUL GESTE QUI CASSE TOUT EN SILENCE : une ligne retirée au
// lieu d'être vidée. `std::array` refuse déjà un initialiseur TROP long ; un
// initialiseur trop COURT, lui, compléterait la table de noms vides, et chaque
// émoticone afficherait sa voisine. La dernière case doit donc être pleine.
static_assert(!kNames.back().empty(),
              "Table d'émoticones désalignée : pour exclure une émoticone, vider son nom, "
              "ne pas retirer la ligne");

}  // namespace

std::string_view ChatEmoteName(std::size_t action) {
  if (action >= kNames.size()) {
    return {};
  }
  return kNames[action];
}

std::optional<std::uint16_t> ChatEmoteAction(std::string_view name) {
  // Un nom vide désignerait la première émoticone exclue.
  if (name.empty()) {
    return std::nullopt;
  }
  for (std::size_t action = 0; action < kNames.size(); ++action) {
    if (kNames[action] == name) {
      return static_cast<std::uint16_t>(action);
    }
  }
  return std::nullopt;
}

}  // namespace ro::ui
