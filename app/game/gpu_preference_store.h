#pragma once

#include "ro/core/error.h"

#include "options/gpu_choice.h"

// LA PRÉFÉRENCE DE CARTE GRAPHIQUE DE WINDOWS POUR CET EXÉCUTABLE, lue et
// écrite là où Windows la range. Voir `gpu_choice.h`.
//
// ⚠ ELLE NE VAUT QU'AU LANCEMENT : le pilote choisit sa carte en ouvrant le
// contexte OpenGL, et un contexte ne change pas de carte. Écrire la préférence
// prépare la partie suivante, rien de plus.
//
// Hors de Windows, rien n'est rangé : la lecture rend l'automatique et
// l'écriture ne fait rien.
namespace moonclient {

// La préférence que Windows tient pour l'exécutable en cours.
[[nodiscard]] GpuPreference ReadWindowsGpuPreference();

// La pose. ⚠ SANS ÉCRITURE QUAND RIEN NE CHANGE : relancer le jeu repasse par
// ici, et réécrire la même valeur à chaque lancement toucherait le registre pour
// rien.
[[nodiscard]] ro::core::Result<void> WriteWindowsGpuPreference(GpuPreference wanted);

// LA DEMANDE AUX PILOTES NVIDIA ET AMD des portables à deux cartes, posée avant
// la création du contexte.
//
// ⚠ UN COMPLÉMENT, PAS LE MÉCANISME : ces pilotes lisent deux variables que
// l'exécutable exporte, et certains les lisent dès le démarrage du processus,
// avant qu'on ait pu les poser. Ceux-là n'y voient que zéro, et c'est la
// préférence de Windows qui décide.
void RequestHighPerformanceGpu(bool wanted);

}  // namespace moonclient
