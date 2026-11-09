#pragma once

#include <array>
#include <cstddef>
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
#include "ro/render/sprite_renderer.h"
#include "ro/sim/survivors_run.h"
#include "ro/vfs/vfs.h"

#include "survivors/horde_poses.h"

// LE CHARIOT DU MODE SURVIVORS : celui qu'un héros de classe à chariot traîne
// au bout d'une case, et qui devient l'orbe de Cart Revolution et de Cart
// Tornado. Il tourne alors autour du héros en roulant, tangent à son cercle,
// suivi de deux copies pâles qui disent sa vitesse, et le héros pivote pour
// le regarder, comme s'il le faisait tournoyer.
//
// 🔴 RIEN N'ENTRE DANS LA SIMULATION : l'orbite y existe déjà, et ce n'en est
// que le dessin. Tout se compte en cases continues, comme les poses.
namespace moonclient {

// La longe : au-delà, le chariot qui suit est ramené à cette distance du héros.
inline constexpr float kArenaCartLeashCells = 1.0F;
// Les copies de la traînée, à tant d'images derrière le chariot qui tourne, et
// leur opacité.
inline constexpr std::array<std::size_t, 2> kArenaCartTrailFrames = {2, 4};
inline constexpr std::array<std::uint8_t, 2> kArenaCartTrailAlpha = {110, 55};

// LES CHARIOTS D'UNE PARTIE, image après image.
class ArenaCarts {
 public:
  // Le sol sous une position en cases continues.
  using GroundOfCell = std::function<glm::vec3(glm::vec2)>;

  // ⚠ LE VFS DOIT SURVIVRE AUX CHARIOTS.
  explicit ArenaCarts(ro::vfs::Vfs& vfs) : vfs_(&vfs) {}

  // LES CHARIOTS DE CETTE IMAGE, d'après les poses (`poses`, retouchées : le
  // héros qui fait tourner le sien le regarde) et les orbes du dernier pas.
  // `spinning` : les skills dont l'orbe est le chariot. Sans « Effets
  // affichés », aucune traînée.
  void Compose(const ro::sim::SurvivorsField& field, std::span<const ro::sim::SurvivorsOrb> orbs,
               std::span<const std::uint16_t> spinning, std::vector<HordePose>& poses,
               const GroundOfCell& ground_of, bool trails);

  [[nodiscard]] std::span<const ro::render::DrawnActor> drawn() const noexcept { return drawn_; }

  // Une partie neuve : aucun chariot.
  void Reset();
  // Le lacet de la caméra de l'arène, que les faces suivent.
  void set_camera_yaw_degrees(float yaw_degrees) noexcept { camera_yaw_degrees_ = yaw_degrees; }

 private:
  // Un .spr et son .act, lus une fois ; vides s'ils ne se lisent pas.
  struct Art {
    std::optional<ro::formats::SprSprite> sprite;
    std::optional<ro::formats::ActAnimation> animation;
    float frame_delay = 1.0F;
  };
  // Une image dessinée du chariot : où, face à quelle direction, quelle image.
  struct Shown {
    glm::vec2 cell{0.0F};
    std::size_t action = 0;
    std::size_t frame = 0;
  };
  // Le chariot d'un héros.
  struct Cart {
    const Art* art = nullptr;
    glm::vec2 cell{0.0F};
    std::uint8_t heading = 0;  // cap de la simulation, 64 par tour
    float travelled_cells = 0.0F;
    bool spinning = false;
    // Les dernières images, la plus récente en tête : la traînée les reprend.
    std::vector<Shown> history;
  };

  [[nodiscard]] const Art* ArtFor(const std::string& spr_path, const std::string& act_path);
  // L'image du chariot `cart` dans l'action `action`.
  [[nodiscard]] std::size_t FrameOf(const Cart& cart, std::size_t action) const;
  void Draw(const Art& art, const Shown& shown, std::uint8_t alpha, const GroundOfCell& ground_of);

  ro::vfs::Vfs* vfs_;
  std::map<std::string, std::unique_ptr<Art>> arts_;
  std::array<std::optional<Cart>, ro::sim::kMostHeroes> carts_;
  // Les images composées de cette image : stables tant que `drawn_` les nomme.
  std::vector<std::unique_ptr<ro::render::ComposedActor>> composed_;
  std::vector<ro::render::DrawnActor> drawn_;
  float camera_yaw_degrees_ = 0.0F;
};

}  // namespace moonclient
