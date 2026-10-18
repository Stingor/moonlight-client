#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "ro/ui/model_library_index.h"
#include "ro/ui/skin_art.h"

namespace ro::ui {

// ── LA BIBLIOTHÈQUE DE MODÈLES DE L'ÉDITEUR DE CARTE ────────────────────────
//
// Les modèles de l'installation, en liste dans le panneau et en GRILLE DE
// VIGNETTES dans leur propre fenêtre : l'arbre des dossiers de data\model\, un
// filtre, les favoris et les modèles posés récemment. Voir docs/adr/0118.
//
// 🔴 LA FENÊTRE NE REND AUCUNE VIGNETTE : elle dit lesquelles elle veut
// (`ModelLibraryWindowState::wanted`), les cases visibles d'abord, et
// l'appelant les rend à son rythme — quelques-unes par image — pour les lui
// donner à l'image suivante (`ModelLibraryView::vignettes`).

// LA VIGNETTE D'UN MODÈLE, telle que l'appelant l'a rendue.
struct MapEditorPreview {
  // Relatif à data\model\, en UTF-8 ; vide sans survol.
  std::string file;
  // La vignette, une fois rendue.
  SkinImage image;
  // Le fichier ne se lit pas, ou n'a rien à montrer.
  bool failed = false;
};

// CE QUE L'APPELANT DONNE DE LA BIBLIOTHÈQUE, à chaque image. Les listes sont
// partagées et non recopiées : elles ne changent qu'avec un geste.
struct ModelLibraryView {
  // Les fichiers de l'installation, relatifs à data\model\, triés.
  std::shared_ptr<const std::vector<std::string>> files;
  // L'arbre de leurs dossiers.
  std::shared_ptr<const ModelLibraryTree> tree;
  // Les favoris, triés ; les modèles posés récemment, le plus récent d'abord.
  std::shared_ptr<const std::vector<std::string>> favorites;
  std::shared_ptr<const std::vector<std::string>> recents;
  // Les vignettes prêtes — ou en échec — parmi celles que la fenêtre voulait à
  // l'image précédente.
  std::vector<MapEditorPreview> vignettes;
};

// Ce que la grille montre.
enum class ModelLibrarySource : std::uint8_t {
  // Un dossier de l'arbre et ses sous-dossiers.
  kFolder,
  kFavorites,
  kRecents,
};

// CE QUE LA FENÊTRE GARDE D'UNE IMAGE À LA SUIVANTE, et ce qu'elle rend à
// l'appelant.
struct ModelLibraryWindowState {
  // Le côté d'une case, en pixels : ses bornes, et celui du premier affichage.
  static constexpr float kSmallestCellPx = 48.0F;
  static constexpr float kLargestCellPx = 192.0F;
  static constexpr float kFirstCellPx = 96.0F;
  // Combien de vignettes la fenêtre demande au plus par image : l'appelant
  // doit pouvoir les garder toutes à la fois.
  static constexpr std::size_t kMostWanted = 160;
  // La place du filtre, zéro final compris.
  static constexpr std::size_t kFilterCapacity = 64;

  bool open = false;
  float cell_px = kFirstCellPx;
  ModelLibrarySource source = ModelLibrarySource::kFolder;
  // Le dossier choisi dans l'arbre : son chemin terminé par « \ », « » pour la
  // racine.
  std::string folder;
  std::array<char, kFilterCapacity> filter{};
  // Le côté a bougé depuis le dernier `cell_px_committed` : il se rend quand le
  // geste est fini, pas à chaque pixel d'un glissé.
  bool cell_px_moved = false;

  // ── RENDU À L'APPELANT, refait à chaque image ──
  // Les vignettes voulues, dans l'ordre où les rendre.
  std::vector<std::string> wanted;
  // Les modèles dont le staff vient de basculer le favori.
  std::vector<std::string> favorite_toggles;
  // Le côté des cases vient d'être réglé : l'appelant le retient.
  bool cell_px_committed = false;

  // ── LE TRI EN COURS, refait quand ce qui le fonde change ──
  // ⚠ LA LISTE EST TENUE, pas seulement désignée : une liste neuve à
  // l'adresse d'une ancienne passerait sinon pour la même.
  std::shared_ptr<const std::vector<std::string>> shown_list;
  ModelLibrarySource shown_source = ModelLibrarySource::kFolder;
  std::string shown_folder;
  std::string shown_filter;
  // Les rangs montrés dans `shown_list`.
  std::vector<std::size_t> shown;
};

}  // namespace ro::ui
