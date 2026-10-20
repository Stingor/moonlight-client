#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <vector>

#include "ro/core/load_progress.h"
#include "ro/render/game_images.h"
#include "ro/render/icon_texture.h"
#include "ro/vfs/vfs.h"

#include "game/ui_stage.h"

namespace moonclient {

// CE QUE L'ÉCRAN DE CHARGEMENT GARDE D'UN CHARGEMENT À L'AUTRE : l'illustration
// tirée, l'animation d'attente téléversée, et de quoi tirer la suivante.
//
// ⚠ TOUT SE LIT À LA PREMIÈRE IMAGE MONTRÉE, et non à la construction : un
// chargement court ne montre rien, et ne doit rien payer. Ce qui manque —
// archives pas encore montées, installation sans l'image — laisse le fond noir
// et des points qui tournent.
class LoadingScreens {
 public:
  LoadingScreens();

  // UNE IMAGE DE L'ÉCRAN, ouverte et fermée dans `interface`. Sans
  // `progress`, la barre est indéterminée et l'étape dit « Chargement… ».
  // `vfs` et `images` peuvent être nuls avant le montage des archives.
  void Draw(UiStage& interface, ro::vfs::Vfs* vfs, ro::render::GameImages* images,
            const ro::core::LoadProgress* progress, float opacity, std::uint32_t now_ms);

 private:
  // L'illustration : la même tant que les écrans se suivent, une autre après.
  void FollowArt(ro::render::GameImages* images, std::uint32_t now_ms);
  // L'animation d'attente, composée et téléversée une fois pour toutes.
  void EnsureSpinner(ro::vfs::Vfs* vfs);
  [[nodiscard]] ro::ui::SkinImage SpinnerAt(std::uint32_t elapsed_ms) const;

  std::mt19937 draw_;
  // Le rang tiré, et la texture que le cache d'images tient pour lui.
  std::optional<std::size_t> art_rank_;
  const ro::render::IconTexture* art_ = nullptr;
  // ⚠ LE CACHE SE VIDE QUAND LE SKIN CHANGE : sa génération dit si `art_`
  // désigne encore une texture vivante.
  std::uint64_t art_generation_ = 0;
  bool art_asked_ = false;
  // La dernière image montrée, et le début de la suite d'écrans en cours.
  std::optional<std::uint32_t> last_shown_ms_;
  std::uint32_t shown_since_ms_ = 0;
  bool spinner_tried_ = false;
  std::vector<ro::render::IconTexture> spinner_frames_;
  float spinner_interval_ms_ = 0.0F;
  // LA DERNIÈRE ÉTAPE DITE, gardée entre deux étapes : la ligne ne clignote pas
  // à chaque couture.
  std::string step_line_;
  std::string percent_text_;
};

}  // namespace moonclient
