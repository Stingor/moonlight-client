#pragma once

#include <cstdint>
#include <optional>
#include <string_view>

#include "ro/render/camera.h"
#include "ro/ui/camera_hud.h"
#include "ro/vfs/vfs.h"

#include "input/camera_rules.h"
#include "world/map_stage.h"

// Les tables par carte, les bornes courantes et la mémoire dehors / dedans.
namespace moonclient {

class CameraRig {
 public:
  void LoadTables(ro::vfs::Vfs& vfs);

  // Retient la vue de la carte quittée, puis pose celle d'arrivée.
  void EnterMap(ro::render::Camera& camera, std::string_view map_name);

  void SetBounded(bool bounded);

  void SetExpandedZoom(bool expanded);

  [[nodiscard]] const CameraLimits& limits() const noexcept { return limits_; }

  // Faux pour un cran arrivé moins de seize millisecondes après le précédent.
  [[nodiscard]] bool TakeWheel(std::uint32_t now_ms);

  // Empêche l'œil de passer sous le sol, sauf sans bornes.
  void FloorEye(ro::render::Camera& camera, const MapStage& stage) const;

  // Avant la première carte : c'est l'arrivée qui lit la mémoire.
  void Restore(const CameraMemory& kept) { memory_ = kept; }

  // La mémoire si l'on quittait la carte maintenant ; rien avant la première.
  [[nodiscard]] std::optional<CameraMemory> MemoryNow(const ro::render::Camera& camera) const;

  [[nodiscard]] ro::ui::CameraHudView HudOf(const ro::render::Camera& camera,
                                            std::string_view map_name, bool ctrl_held) const;

 private:
  void Recompute();

  MapCameraTables tables_;
  MapCameraRules rules_;
  CameraLimits limits_ = LimitsFor(MapCameraRules{}, false, true);
  CameraMemory memory_;
  bool bounded_ = true;
  bool expanded_zoom_ = false;
  bool entered_ = false;
  std::uint32_t last_wheel_ms_ = 0;
  bool wheeled_ = false;
};

}  // namespace moonclient
