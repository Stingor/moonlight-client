#include "ro/render/game_images.h"

#include <catch2/catch_test_macros.hpp>

// LE CHEMIN D'UNE IMAGE QUE LE SERVEUR NOMME.
//
// 🔴 C'EST LE SEUL CHEMIN DE CE DÉPÔT QUI VIENNE DU DEHORS. Partout ailleurs il
// est composé ici, à partir d'une table de l'installation ; celui-ci arrive dans
// le texte d'un PNJ, et le VFS lit aussi le dossier `data` du disque.

using ro::render::SafeGamePath;

TEST_CASE("Image de script : un chemin ordinaire passe, et se normalise", "[render][npc]") {
  CHECK(SafeGamePath("ressource\\carte.bmp") == "ressource\\carte.bmp");
  // ⚠ LES DEUX ÉCRITURES DE SÉPARATEUR MÈNENT AU MÊME FICHIER : les archives du
  // client nomment à l'antislash, un script écrit volontiers l'autre.
  CHECK(SafeGamePath("ressource/carte.bmp") == "ressource\\carte.bmp");
}

TEST_CASE("Image de script : ce qui sort de l'installation est refusé", "[render][npc]") {
  // Une remontée, une racine, un lecteur : le chemin ne désigne plus une
  // ressource du jeu.
  CHECK(SafeGamePath("..\\..\\Windows\\win.ini").empty());
  CHECK(SafeGamePath("ressource/../../secret.txt").empty());
  CHECK(SafeGamePath("/etc/passwd").empty());
  CHECK(SafeGamePath("\\\\serveur\\partage\\a.bmp").empty());
  CHECK(SafeGamePath("C:\\Windows\\win.ini").empty());
  CHECK(SafeGamePath("").empty());
}
