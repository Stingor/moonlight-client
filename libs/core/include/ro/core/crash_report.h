#pragma once

#include <filesystem>
#include <string>
#include <string_view>

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
// CE QU'IL VOIT, sous Windows : les fautes matérielles, le tas corrompu, les
// paramètres que le runtime C refuse, l'appel d'une fonction virtuelle pure,
// `abort` et l'exception C++ non rattrapée. Ce qu'il NE VOIT PAS : un processus
// tué de l'extérieur — c'est la ligne de fin du journal qui le trahit, voir
// `EndLogFile`.
//
// À appeler UNE FOIS, et AVANT `InitLogging` : ouvrir le journal peut déjà
// échouer, et le rapport n'a besoin que de son dossier. Sans journal, il le dit
// sur la console.
void InstallCrashReporter(std::filesystem::path folder);

// REPOSE le filtre, et dit au journal si quelqu'un l'avait remplacé.
//
// ⚠ UN MODULE CHARGÉ APRÈS COUP PEUT PRENDRE LA PLACE DU FILET — un pilote
// graphique, une surcouche injectée — et une faute ne passe plus alors par
// lui. `moment` dit ce qui venait d'être chargé.
void RearmCrashReporter(std::string_view moment);

// Le nom d'une faute, tel que le système la numérote.
//
// Exposé parce que c'est la seule partie qui se vérifie sans planter : le reste
// est du câblage vers le système d'exploitation.
[[nodiscard]] std::string DescribeFault(unsigned long code);

}  // namespace ro::core
