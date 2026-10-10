#pragma once

#include <string>
#include <vector>

namespace ro::core {

// Les arguments du programme, en UTF-8, sans le nom de l'exécutable.
//
// ⚠ Sous Windows, `argv` arrive dans la page de codes ANSI du système, qui ne
// sait pas représenter le coréen : un chemin comme data\sprite\몬스터\… y
// devient une file de points d'interrogation, et le fichier reste introuvable.
// Or les sprites du jeu portent presque tous un nom coréen. La ligne de
// commande est donc relue en UTF-16, seule forme non dégradée, et convertie en
// UTF-8 — l'encodage que le VFS attend de la part d'un humain.
//
// Ailleurs, `argv` est déjà en UTF-8 et rien n'est transformé.
[[nodiscard]] std::vector<std::string> ProgramArguments(int argc, char** argv);

}  // namespace ro::core
