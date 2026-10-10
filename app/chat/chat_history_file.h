#pragma once

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/chat_line.h"

#include "game/save_folder.h"

namespace moonclient {

// CE QUI S'EST DIT LA PARTIE D'AVANT.
//
// ⚠ CES LIGNES-LÀ N'APPARTIENNENT PAS AU MONDE, et c'est pour cela qu'elles
// vivent ici plutôt que dans `sim`. La conversation du monde est ce qui s'est
// dit DANS CETTE SESSION ; ce fichier est une aide à la lecture, qu'on pose
// au-dessus. Les mêler reviendrait à faire croire à la simulation qu'elle a
// entendu des choses qui ne lui sont jamais parvenues.
//
// ⚠ ET LEUR HEURE EST ÉCRITE, pas déduite. Celle d'une ligne vivante se calcule
// depuis l'horloge de la boucle ; une ligne d'hier soir n'a pas d'horloge de
// boucle, et la recalculer lui donnerait une heure d'aujourd'hui.
//
// 🔴 CE FICHIER CONTIENT LES CHUCHOTEMENTS EN CLAIR. C'est la raison pour
// laquelle son option est décochée par défaut : garder l'historique est un
// service, mais il écrit sur le disque des conversations privées, et personne ne
// doit le découvrir après coup.

inline constexpr std::string_view kChatHistoryFileName = "chat_history.yaml";

// Combien de lignes on garde, et les bornes du réglage. En deçà de vingt, la
// relecture n'apprend rien ; au-delà de mille, le fichier devient un journal,
// ce qu'il n'est pas — le journal est à côté, dans `chat_log_file.h`, et il
// n'a pas de borne parce qu'il ne se relit pas dans la fenêtre.
inline constexpr int kChatHistoryKeptLines = 200;
inline constexpr int kSmallestChatHistory = 20;
inline constexpr int kLargestChatHistory = 1000;

// Une ligne de la partie d'avant, avec l'heure qu'il était.
struct PastChatLine {
  // Sa case de filtre : c'est elle qui dit dans quel onglet elle se relit.
  ro::sim::ChatType type = ro::sim::ChatType::kPublic;
  std::string text;
  std::uint8_t hour = 0;
  std::uint8_t minute = 0;
  std::uint8_t second = 0;
  // LA COULEUR QU'ELLE AVAIT À L'ÉCRAN, en `0xRRGGBB`, ou zéro pour la couleur
  // ordinaire du fil — voir `ShownRgbOfChatLine`.
  //
  // ⚠ RÉSOLUE, ET NON CELLE QUE LE SERVEUR A DEMANDÉE : le genre, qui donnait
  // sa teinte à une ligne sans couleur demandée, ne s'écrit pas. Sans elle, une
  // ligne de groupe d'hier soir reviendrait de la teinte ordinaire, et ce qui
  // se relit ne ressemblerait plus à ce qu'on a vu.
  std::uint32_t color = 0;
};

// Ce qu'un fichier d'historique rend : les lignes, et QUAND il a été écrit.
//
// ⚠ LA DATE EST MONTRÉE AU JOUEUR, et elle n'est pas décorative : l'heure d'une
// ligne ment sur son jour. « 21:12 » se lit comme ce soir, alors que c'était
// hier — le repère qu'on pose entre les deux sessions le dit.
struct PastChat {
  std::vector<PastChatLine> lines;
  std::string saved_at;
  // ── LES DERNIERS DESTINATAIRES ─────────────────────────
  // Ceux que la box « Pseudo » propose, le plus récent en tête.
  //
  // 🔴 DANS CE FICHIER-CI, ET SOUS LA MÊME OPTION, parce que c'est la
  // MÊME confidence : il porte déjà les chuchotements en clair, et savoir À QUI
  // l'on parle en privé n'est pas moins sensible que ce qu'on leur dit. Un
  // second fichier sous une seconde option donnerait deux cases à décocher pour
  // une seule question, et une chance de plus d'en oublier une à l'effacement.
  //
  // ⚠ LE CLIENT D'ORIGINE NE LES GARDE PAS : son JSON n'écrit que la liste de
  // blocage, les macros d'émotes et les raccourcis. Ce n'est donc pas une
  // régression qu'on rattrape, c'est un service qu'on ajoute — raison de plus
  // pour qu'il suive l'option que le joueur a déjà refusée ou acceptée.
  std::vector<std::string> whispered;
  // ── LES LIGNES TAPÉES ──────────────────────────────────
  // Ce que la flèche ↑ rappelle, de la plus ancienne à la plus récente.
  //
  // 🔴 DANS CE FICHIER-CI, ET C'EST LA MÊME CONFIDENCE : une ligne tapée est
  // une ligne tapée, `/w Alice …` comprise. Un second fichier donnerait un
  // second endroit à effacer, et une chance de plus d'en oublier un.
  //
  // ⚠ MAIS SOUS SA PROPRE CASE, et cochée celle-là : ce qu'on retape le plus
  // sont les `@commandes`, et un service qu'il faut d'abord aller cocher
  // n'existe pas pour qui ignore qu'il existe. Les deux cases répondent à deux
  // questions ; le fichier, lui, reste unique.
  std::vector<std::string> typed;
};

[[nodiscard]] std::filesystem::path ChatHistoryPath(const std::filesystem::path& client_root);

// Lit l'historique. Un fichier absent, illisible ou vide rend une liste vide :
// on joue sans, et rien ne le signale au joueur.
[[nodiscard]] PastChat LoadChatHistory(const std::filesystem::path& client_root);

// Écrit les `keep` dernières lignes. Rend faux si le fichier n'a pas pu être
// écrit. `saved_at` est composé par l'appelant, qui seul connaît l'heure.
//
// ⚠ `typed` ARRIVE DÉJÀ BORNÉ : la barre de saisie tient sa propre limite à
// chaque envoi — c'est elle qui la connaît —, et la retrancher ici donnerait un
// second endroit où le réglage se ferait appliquer.
[[nodiscard]] bool SaveChatHistory(const std::filesystem::path& client_root,
                                   const std::vector<PastChatLine>& lines, int keep,
                                   std::string_view saved_at,
                                   std::span<const std::string> whispered,
                                   std::span<const std::string> typed = {});

// Efface le fichier. C'est le geste de qui décoche l'option : garder sur le
// disque ce qu'on vient de refuser serait exactement l'inverse de ce qu'il
// demande.
void ForgetChatHistory(const std::filesystem::path& client_root);

}  // namespace moonclient
