#pragma once

#include "ro/ui/storage_window.h"

// UNE LIGNE DU COFFRE : son icône et son étoile de favori, ses gestes de
// retrait, son menu, et ses colonnes.
//
// En-tête PRIVÉ : seule la fenêtre du coffre dessine ses lignes.
namespace ro::ui {

// Le rang de déclaration d'une colonne, tel qu'ImGui le veut.
[[nodiscard]] constexpr int StorageColumnRank(StorageColumn column) {
  return static_cast<int>(column);
}

// Une ligne, dans la table ouverte : chaque cellule vise sa colonne par son
// rang, l'ordre d'affichage appartenant au joueur.
void DrawStorageRow(const InventoryCellView& cell, StorageWindowState& state,
                    const StorageWindowView& view, StorageWindowAction& action);

}  // namespace ro::ui
