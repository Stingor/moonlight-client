#pragma once

#include <cstddef>
#include <string>

#include "ro/render/icon_texture.h"
#include "ro/ui/map_editor_panel.h"
#include "ro/vfs/vfs.h"

#include "world/preview_cache.h"

namespace moonclient {

// LES VIGNETTES DES MODÈLES DU DÉCOR que l'éditeur de carte montre au survol :
// lues dans l'installation, rendues hors écran, téléversées, puis gardées.
// Voir `PreviewCache` pour la règle d'une neuve par image.
class ModelPreviews {
 public:
  // Combien de vignettes restent en mémoire : 192 × 192 pixels chacune.
  static constexpr std::size_t kKept = 32;

  // 🔴 À APPELER UNE FOIS PAR IMAGE, avant `Show`.
  void BeginFrame() noexcept { cache_.BeginFrame(); }

  // L'aperçu de `file`, relatif à data\model\, en UTF-8 ; un aperçu vide pour
  // un nom vide. Un contexte OpenGL doit être courant.
  [[nodiscard]] ro::ui::MapEditorPreview Show(ro::vfs::Vfs& vfs, const std::string& file);

  // Jette tout, à la sortie de l'éditeur. ⚠ ENTRE DEUX IMAGES.
  void Clear() { cache_.Clear(); }

 private:
  PreviewCache<ro::render::IconTexture> cache_{kKept};
};

}  // namespace moonclient
