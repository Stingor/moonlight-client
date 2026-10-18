#pragma once

#include <cstddef>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/render/icon_texture.h"
#include "ro/ui/map_editor_panel.h"
#include "ro/vfs/vfs.h"

#include "world/preview_cache.h"

namespace moonclient {

// LES VIGNETTES DES MODÈLES DU DÉCOR que l'éditeur de carte montre — au survol,
// et dans la grille de la bibliothèque : relues dans le cache disque du client,
// sous le dossier de cache de l'utilisateur, ou rendues hors écran,
// téléversées, puis gardées. Voir `PreviewCache` pour le budget par image, et
// `ro::render::LoadCachedDecorVignette` pour le disque.
class ModelPreviews {
 public:
  // Combien de vignettes restent en mémoire, 192 × 192 pixels chacune : toutes
  // celles que la grille demande, plus celle du survol, plus de quoi revenir
  // en arrière sans tout refaire.
  static constexpr std::size_t kKept = 256;
  static_assert(kKept > ro::ui::ModelLibraryWindowState::kMostWanted + 1,
                "les vignettes d'une même image ne doivent pas se chasser l'une l'autre");
  // Le temps qu'une image consacre aux vignettes neuves, en millisecondes. Une
  // vignette relue du disque en coûte une fraction : elles arrivent alors par
  // dizaines ; une vignette à rendre en coûte plus que tout le budget en
  // Debug, et la grille s'emplit d'une case par image.
  static constexpr double kFrameBudgetMs = 6.0;

  // 🔴 À APPELER UNE FOIS PAR IMAGE, avant `Show` et `ShowMany`.
  void BeginFrame() noexcept { cache_.BeginFrame(); }

  // L'aperçu de `file`, relatif à data\model\, en UTF-8 ; un aperçu vide pour
  // un nom vide, ou qui attend. Un contexte OpenGL doit être courant.
  [[nodiscard]] ro::ui::MapEditorPreview Show(ro::vfs::Vfs& vfs, const std::string& file);

  // Les aperçus prêts — ou en échec — parmi `files`, demandés dans leur ordre :
  // ceux qui attendent n'y sont pas.
  [[nodiscard]] std::vector<ro::ui::MapEditorPreview> ShowMany(ro::vfs::Vfs& vfs,
                                                               std::span<const std::string> files);

  // Les vignettes faites depuis `BeginFrame` : relues ou rendues.
  [[nodiscard]] std::size_t made_this_frame() const noexcept { return cache_.made_this_frame(); }

  // Jette tout, à la sortie de l'éditeur. ⚠ ENTRE DEUX IMAGES.
  void Clear() { cache_.Clear(); }

 private:
  // Le dossier des vignettes gardées, cherché au premier aperçu : vide quand le
  // dossier de cache de l'utilisateur ne se trouve pas, et rien ne se garde.
  const std::filesystem::path& CacheRoot();

  PreviewCache<ro::render::IconTexture> cache_{kKept, kFrameBudgetMs};
  std::optional<std::filesystem::path> cache_root_;
};

}  // namespace moonclient
