#include "chat/emotion_catalog.h"

#include <array>

namespace moonclient {
namespace {

// ── EXCLURE UNE ÉMOTICÔNE ────────────────────────────────────────────────────
// Lui donner un nom VIDE — ne pas supprimer sa ligne, ce qui décalerait tout ce
// qui suit. L'entrée garde sa place et l'émoticône disparaît d'un coup : absente
// de la grille, et son nom ne la désigne plus.
//
// ⚠ LES SEPT TROUS SONT VOULUS : le mutisme, que le serveur refuse de relayer,
// et les six dés, que le jeu se réserve et qu'aucune commande ne nomme. Le
// client ne les propose pas non plus.
//
// 🔴 LE RANG EST LE NUMÉRO DU PROTOCOLE, PAS L'ACTION DU SPRITE : les deux
// divergent, voir `ro::render::EmotionAction`. Rangée dans l'ordre du sprite,
// la table nommait chaque émoticône du nom de sa voisine.
constexpr std::array<std::string_view, kKnownEmotions> kNames = {
    "surprise",
    "question",
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
    "bigthrob",
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
    "",
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
    "whisp",
};

// Garde-fou contre le seul geste qui casse tout en silence : retirer une ligne
// au lieu d'y mettre un nom vide. Le compilateur le dit tout de suite, là où le
// jeu se serait contenté d'envoyer chaque émoticône à la place de sa voisine.
static_assert(kNames.size() == kKnownEmotions,
              "Table d'emoticones desalignee : pour en EXCLURE une, lui donner un nom VIDE, "
              "ne pas supprimer la ligne");

}  // namespace

std::string_view EmotionName(std::uint16_t emotion) {
  return emotion < kNames.size() ? kNames[emotion] : std::string_view();
}

bool EmotionIsOffered(std::uint16_t emotion) { return !EmotionName(emotion).empty(); }

}  // namespace moonclient
