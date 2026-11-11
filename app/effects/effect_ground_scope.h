#pragma once

#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <vector>

#include <glm/glm.hpp>

#include "ro/formats/image.h"
#include "ro/render/effect_magic_target.h"
#include "ro/vfs/vfs.h"

#include "effects/drawn_ground_cells.h"

namespace moonclient {

// LES CERCLES DE VISÉE VIVANTS : un par incantation posée sur une case, jusqu'à
// la fin annoncée. Voir `ro/render/ground_scope.h`.
class GroundScopes {
 public:
  explicit GroundScopes(ro::vfs::Vfs& vfs) : vfs_(&vfs) {}

  // Pose le cercle d'une incantation sur la case dont `ground_position` est le
  // centre. Rien pour une compétence dont le cercle n'a pas de côté.
  void Place(std::uint16_t skill_id, glm::vec3 ground_position, std::uint32_t started_ms,
             std::uint32_t duration_ms, bool minimal_effects);

  // CE QUI SE PEINT CETTE IMAGE : un lot par cercle. Les cercles échus s'en vont
  // d'abord. Le résultat vit jusqu'au prochain appel.
  [[nodiscard]] std::span<const DrawnGroundCells> Compose(std::uint32_t now_ms);

  // Une nouvelle carte : plus aucun cercle.
  void Forget() { scopes_.clear(); }

 private:
  struct Scope {
    std::uint16_t skill_id = 0;
    std::int32_t side = 0;
    glm::vec3 ground_position{0.0F};
    std::uint32_t started_ms = 0;
    std::uint32_t ends_ms = 0;
    std::string texture_path;
  };

  // L'image d'un chemin, lue une fois puis retenue — l'échec comme le succès.
  const ro::formats::Image& TextureFor(const std::string& path);

  ro::vfs::Vfs* vfs_ = nullptr;
  std::vector<Scope> scopes_;
  std::map<std::string, ro::formats::Image, std::less<>> textures_;
  std::vector<std::vector<ro::render::EffectGroundCellPaint>> cell_lots_;
  std::vector<DrawnGroundCells> drawn_;
};

}  // namespace moonclient
