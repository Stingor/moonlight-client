#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ro::ui {

// ── LA NAVIGATION DE LA BIBLIOTHÈQUE DE MODÈLES ─────────────────────────────
//
// Ce que la fenêtre « Bibliothèque de modèles » calcule sans dessiner : l'arbre
// des dossiers de data\model\, le filtre, et les cases de la grille qui se
// voient. Pur, donc éprouvé hors de toute fenêtre.

// Un dossier de l'arbre.
struct ModelFolder {
  // Son dernier segment, en UTF-8 ; vide pour la racine.
  std::string name;
  // Son chemin relatif à data\model\, terminé par « \ » ; vide pour la racine.
  std::string prefix;
  // Ses sous-dossiers, rangs dans `ModelLibraryTree::folders`, par nom.
  std::vector<std::size_t> children;
  // Les modèles qu'il porte, sous-dossiers compris.
  std::size_t model_count = 0;
};

// L'arbre des dossiers ; la racine est au rang zéro.
struct ModelLibraryTree {
  std::vector<ModelFolder> folders;
};

// L'arbre des dossiers de `files` (relatifs à data\model\, séparés par « \ »).
[[nodiscard]] ModelLibraryTree BuildModelLibraryTree(std::span<const std::string> files);

// LE FILTRE : chaque mot de `filter` (séparés par des espaces) doit se trouver
// dans le chemin, sans égard à la casse ASCII. Un filtre vide laisse tout.
[[nodiscard]] bool ModelPassesFilter(std::string_view file, std::string_view filter);

// Les rangs des `files` qui sont sous `folder_prefix` (sans égard à la casse
// ASCII, « » pour tous) et passent le filtre, dans leur ordre.
[[nodiscard]] std::vector<std::size_t> FilterModelFiles(std::span<const std::string> files,
                                                        std::string_view folder_prefix,
                                                        std::string_view filter);

// ── LA GRILLE ────────────────────────────────────────────────────────────────

// Combien de colonnes de `cell_px` pixels, séparées de `gap_px`, tiennent dans
// `width_px` ; une au moins.
[[nodiscard]] int GridColumns(float width_px, float cell_px, float gap_px);

// Une suite de cases, `[first, end)`, dans l'ordre de la grille.
struct GridSpan {
  std::size_t first = 0;
  std::size_t end = 0;

  [[nodiscard]] bool empty() const noexcept { return first >= end; }
  [[nodiscard]] std::size_t size() const noexcept { return empty() ? 0 : end - first; }
};

// LES CASES QUI SE VOIENT parmi `count`, rangées par `columns` en rangées de
// `row_px` pixels, pour une vue de `view_px` pixels défilée de `scroll_px` :
// une rangée à moitié visible compte.
[[nodiscard]] GridSpan VisibleGridCells(std::size_t count, int columns, float row_px,
                                        float scroll_px, float view_px);

// L'ORDRE DES DEMANDES DE VIGNETTES : les cases visibles d'abord, puis
// `ahead_rows` rangées au-dessous — où l'on défile d'ordinaire —, puis
// `behind_rows` au-dessus ; `most` au plus.
[[nodiscard]] std::vector<std::size_t> VignetteRequestOrder(GridSpan visible, std::size_t count,
                                                            int columns, int ahead_rows,
                                                            int behind_rows, std::size_t most);

// Le dernier segment de `file`, son extension ôtée : ce que la case écrit.
[[nodiscard]] std::string_view ModelStem(std::string_view file);

}  // namespace ro::ui
