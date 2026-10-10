#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "ro/formats/image.h"
#include "ro/render/effect_magic_target.h"
#include "ro/render/ground_quad_renderer.h"
#include "ro/sim/packed_position.h"
#include "ro/sim/world_fwd.h"
#include "ro/vfs/vfs.h"

#include "guild/emblem_plates.h"
#include "world/map_stage.h"

namespace moonclient {

// LES GRAFFITIS AU SOL : le message de chaque unité qui en porte un, et
// l'emblème de chaque Flag Graffiti, couchés sur le relief autour d'elle.
//
// La toile d'un message se compose UNE fois, quand l'unité paraît, et garde le
// regard qu'avait alors le joueur de la caméra : se retourner ensuite ne fait
// pas tourner le message. Voir `ro::render::ComposeGraffitiCanvas`.
class GraffitiOnGround {
 public:
  explicit GraffitiOnGround(ro::vfs::Vfs& vfs) : vfs_(&vfs) {}

  // CE QUI SE PEINT CETTE IMAGE : un lot par graffiti. `emblems` peut être nul :
  // les Flag Graffiti ne se peignent alors pas. Le résultat vit jusqu'au
  // prochain appel.
  [[nodiscard]] std::span<const ro::render::DrawnGroundQuads> Compose(const ro::sim::World& world,
                                                                      const MapStage& stage,
                                                                      EmblemPlates* emblems,
                                                                      std::uint32_t now_ms);

 private:
  struct Scribble {
    std::string message;
    ro::sim::CellPosition cell;
    ro::formats::Image canvas;
    // Le nom sous lequel le renderer retient la toile téléversée : il doit
    // survivre à l'appel, et changer avec ce qu'elle montre.
    std::string texture_path;
    float body_degrees = 0.0F;
  };

  // Un lot en attente : son premier quad, et l'image qui le peint.
  struct Pending {
    std::size_t first = 0;
    const std::string* texture_path = nullptr;
    const ro::formats::Image* texture = nullptr;
  };

  // La planche des lettres, lue une fois. ⚠ L'ÉCHEC SE RETIENT AUSSI.
  [[nodiscard]] const ro::formats::Image* Alphabet();

  // Pose les cases d'un lot autour de `centre`.
  void Paint(const MapStage& stage, ro::sim::CellPosition centre,
             std::span<const ro::render::EffectGroundCellPaint> cells,
             const std::string* texture_path, const ro::formats::Image* texture);

  ro::vfs::Vfs* vfs_ = nullptr;
  std::optional<ro::formats::Image> alphabet_;
  bool alphabet_read_ = false;
  std::map<std::uint32_t, Scribble> scribbles_;
  // Le nom de téléversement de chaque emblème peint, par guilde et version.
  std::map<std::pair<std::int32_t, std::int32_t>, std::string> emblem_paths_;
  std::vector<ro::render::GroundQuad> quads_;
  std::vector<Pending> pending_;
  std::vector<ro::render::DrawnGroundQuads> lots_;
};

}  // namespace moonclient
