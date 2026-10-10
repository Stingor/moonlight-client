#pragma once

#include <filesystem>
#include <string>

namespace ro::core {

// LE FILET : un plantage écrit un RAPPORT avant que la fenêtre ne se ferme.
//
// POURQUOI IL EXISTE. Un client qui tombe sur une violation d'accès disparaît
// sans un mot : la fenêtre se ferme, le journal s'arrête sur une ligne
// parfaitement normale, et il ne reste qu'une impression — « ça s'est fermé
// tout seul ». Rien ne distingue un plantage d'une fermeture demandée, et l'on
// cherche la panne dans ce qu'on faisait au lieu de la lire.
//
// ⚠ CE N'EST PAS UN GESTIONNAIRE D'ERREURS. Il ne rattrape rien et ne reprend
// rien : le programme meurt de toute façon. Il ne fait qu'écrire ce que le
// système sait de la faute, et c'est précisément ce qui manquait.
//
// CE QU'IL DÉPOSE, dans le dossier donné :
//
//   plantage-AAAAMMJJ-HHMMSS.txt   la faute, les registres, la pile, les
//                                  modules, et LES DERNIÈRES LIGNES DU JOURNAL
//   plantage-AAAAMMJJ-HHMMSS.dmp   le vidage, qu'un débogueur rouvre plus tard
//
// Les dernières lignes du journal sont ce qu'un rapport du client officiel n'a
// pas, et c'est souvent ce qui explique le reste : la faute dit OÙ, le journal
// dit CE QU'ON FAISAIT.
//
// À appeler UNE FOIS, aussitôt après `InitLogging` : un plantage survenu entre
// les deux serait muet.
void InstallCrashReporter(std::filesystem::path folder);

// Le nom d'une faute, tel que le système la numérote.
//
// Exposé parce que c'est la seule partie qui se vérifie sans planter : le reste
// est du câblage vers le système d'exploitation.
[[nodiscard]] std::string DescribeFault(unsigned long code);

}  // namespace ro::core
