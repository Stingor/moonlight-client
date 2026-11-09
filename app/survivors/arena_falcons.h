#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/act.h"
#include "ro/formats/spr.h"
#include "ro/render/actor_sprite.h"
#include "ro/render/companion_falcon.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_run.h"
#include "ro/vfs/vfs.h"

#include "survivors/horde_poses.h"

// LE FAUCON DU MODE SURVIVORS : l'oiseau qui plane au-dessus d'un héros qui
// tient Blitz Beat ou Falcon Assault, et qui pique vers le point que le coup
// vise. C'est le faucon du jeu (`ro/render/companion_falcon.h`) : sa naissance,
// son vol plané, son piqué et sa remontée, au rythme de ses tics de 16 ms.
//
// 🔴 RIEN N'ENTRE DANS LA SIMULATION : le coup y existe déjà, et l'oiseau n'en
// est que le dessin. Le faucon compte dans le repère du client ; on convertit
// à l'entrée et à la sortie, nulle part ailleurs.
namespace moonclient {

class ArenaFalcons {
 public:
  // Le sol sous une position en cases continues, dans notre repère.
  using GroundOfCell = std::function<glm::vec3(glm::vec2)>;
  // La hauteur du sol sous un point (x, z) de notre monde.
  using HeightAt = std::function<float(float x, float z)>;

  // ⚠ LE VFS DOIT SURVIVRE AUX FAUCONS.
  explicit ArenaFalcons(ro::vfs::Vfs& vfs) : vfs_(&vfs) {}

  // LES FAUCONS DE CETTE IMAGE : un par héros posé qui tient un skill de
  // faucon, lancé vers chaque coup de faucon de `casts`, avancé jusqu'à
  // `now_ms`.
  void Compose(const ro::sim::SurvivorsField& field, std::span<const ro::sim::SurvivorsCast> casts,
               std::span<const HordePose> poses, const GroundOfCell& ground_of,
               const HeightAt& height_at, std::uint32_t now_ms);

  [[nodiscard]] std::span<const ro::render::DrawnActor> drawn() const noexcept { return drawn_; }

  // Une partie neuve : aucun faucon.
  void Reset();
  // Le lacet de la caméra de l'arène, que les faces suivent.
  void set_camera_yaw_degrees(float yaw_degrees) noexcept { camera_yaw_degrees_ = yaw_degrees; }

 private:
  struct Art {
    std::optional<ro::formats::SprSprite> sprite;
    std::optional<ro::formats::ActAnimation> animation;
  };
  struct Kept {
    ro::render::Falcon falcon;
    const Art* art = nullptr;
    ro::render::FalconMaster master;
    std::unique_ptr<ro::render::ComposedActor> frame;
  };

  [[nodiscard]] const Art* ArtFor(const ro::render::FalconArt& wanted);

  ro::vfs::Vfs* vfs_;
  std::map<std::string, std::unique_ptr<Art>> arts_;
  std::array<std::optional<Kept>, ro::sim::kMostHeroes> falcons_;
  std::vector<ro::render::DrawnActor> drawn_;
  float camera_yaw_degrees_ = 0.0F;
  std::optional<std::uint32_t> last_tick_ms_;
};

}  // namespace moonclient
