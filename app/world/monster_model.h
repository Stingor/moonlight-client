#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/formats/gr2_model.h"
#include "ro/render/actor_model_pose.h"
#include "ro/render/icon_texture.h"
#include "ro/vfs/vfs.h"

namespace moonclient {

// ── LE MODÈLE 3D D'UN MONSTRE, MONTRÉ COMME UN SPRITE ───────────────────────
//
// L'Emperium, les gardiens de forteresse, le drapeau de guilde et les coffres
// au trésor n'ont pas de `.spr` : c'est un modèle que le jeu peint. La fiche,
// elle, pose des images. Son animation se RE-REND donc en une série d'images,
// à la taille du cadre et dans l'orientation choisie, et l'affichage redevient
// celui d'un sprite.
//
// ⚠ LES IMAGES SE RENDENT À LA DEMANDE, une par image de jeu au pire, et se
// gardent : tout rendre d'un coup coûtait une centaine de rendus à l'ouverture
// et à chaque cran de molette. Le premier tour remplit la série, les suivants
// ne coûtent plus rien.
class MonsterModel {
 public:
  explicit MonsterModel(ro::vfs::Vfs& vfs) : vfs_(&vfs) {}

  struct Framing {
    // Le fichier que la table des classes donne, extension comprise :
    // « Empelium90_0.gr2 ».
    std::string_view resource_name;
    int box_width = 0;
    int box_height = 0;
    // L'orientation, en radians autour de la verticale du modèle.
    float yaw_radians = 0.0F;
  };

  // L'image de l'instant `clock_ms`, ou nulle : le modèle manque, ou ne porte
  // rien à peindre. Zéro est la première image, pour un modèle figé.
  [[nodiscard]] const ro::render::IconTexture* FrameAt(const Framing& framing,
                                                       std::uint32_t clock_ms);

  void Forget();

 private:
  [[nodiscard]] bool Load(std::string_view resource_name);
  void ResetSeries();
  [[nodiscard]] const ro::render::IconTexture* EnsureFrame(std::size_t index);

  ro::vfs::Vfs* vfs_;
  std::string name_;
  std::optional<ro::formats::GrannyScene> scene_;
  // L'animation que le fichier embarque, jouée en boucle ; absente d'un modèle
  // qui n'en porte pas, qui se montre alors figé.
  std::optional<ro::render::ActorModelAnimation> animation_;
  std::vector<std::vector<std::int32_t>> bone_maps_;

  int width_ = 0;
  int height_ = 0;
  float yaw_radians_ = 0.0F;
  float ms_per_frame_ = 0.0F;
  std::vector<ro::render::IconTexture> frames_;
  std::vector<bool> rendered_;
};

}  // namespace moonclient
