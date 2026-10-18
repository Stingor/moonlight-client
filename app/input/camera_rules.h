#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>

#include "ro/render/camera.h"

// Bornes et pas des gestes de la caméra.
//
// Les calculs se font en unités du jeu (recul, altitude négative en l'air,
// lacet), puis se reconvertissent : l'ordre plancher-puis-plafond des bornes ne
// survivrait pas au changement de signe de l'élévation.
namespace moonclient {

inline constexpr float kNativeDefaultZoom = 300.0F;

// Le recul du jeu est la distance œil–visé, dans les mêmes unités que la nôtre.
struct NativeView {
  float zoom = kNativeDefaultZoom;
  float altitude_degrees = 0.0F;
  float yaw_degrees = 0.0F;
};

// Élévation = −altitude, lacet = −lacet du jeu.
[[nodiscard]] NativeView NativeViewOf(const ro::render::Camera& camera);
void ApplyNativeView(ro::render::Camera& camera, const NativeView& view);

// Une ligne de `data\viewpointtable.txt`, dans l'ordre du fichier.
struct ViewpointRow {
  std::int16_t range = 0;
  std::int16_t scope = 0;
  std::int16_t range_on_entry = 0;
  std::int16_t rotation_from = 0;
  std::int16_t rotation_to = 0;
  std::int16_t rotation_on_entry = 0;
  // `from` est le plafond, `to` le plancher : les altitudes sont négatives.
  std::int16_t altitude_from = 0;
  std::int16_t altitude_to = 0;
  std::int16_t altitude_on_entry = 0;

  friend bool operator==(const ViewpointRow&, const ViewpointRow&) = default;
};

struct MapCameraRules {
  bool indoor = false;
  std::optional<ViewpointRow> viewpoint;
};

// Les intérieurs de `vfs::ReadIndoorMaps`, et le texte de
// `data\viewpointtable.txt`. Une table absente laisse toutes les cartes aux
// bornes ordinaires.
class MapCameraTables {
 public:
  // `indoor_maps` : les clés de table des intérieurs, sans leur extension,
  // telles que la table les donne.
  [[nodiscard]] static MapCameraTables Parse(std::span<const std::string> indoor_maps,
                                             std::string_view viewpoint_text);

  [[nodiscard]] MapCameraRules RulesFor(std::string_view map_name) const;

  [[nodiscard]] std::size_t indoor_count() const noexcept { return indoor_.size(); }
  [[nodiscard]] std::size_t viewpoint_count() const noexcept { return viewpoints_.size(); }

  // Les intérieurs, par leur clé de table : minuscules, extension comprise.
  [[nodiscard]] const std::set<std::string, std::less<>>& indoor_maps() const noexcept {
    return indoor_;
  }

 private:
  std::set<std::string, std::less<>> indoor_;
  std::map<std::string, ViewpointRow, std::less<>> viewpoints_;
};

// Minuscules et extension `.rsw`. Un `@` au rang quatre ou plus retire les trois
// caractères qui commencent quatre rangs avant lui.
[[nodiscard]] std::string CameraTableKey(std::string_view map_name);

// Les bornes de la carte courante, en unités du jeu.
struct CameraLimits {
  // Faux avec la case du staff : les gestes ne s'arrêtent plus.
  bool bounded = true;
  MapCameraRules rules;
  float zoom_nearest = 0.0F;
  float zoom_farthest = 0.0F;
  float altitude_floor = 0.0F;
  float altitude_ceiling = 0.0F;
  float altitude_ceiling_ctrl = 0.0F;
  // Au-delà de ce recul, Ctrl n'élargit plus le plafond d'altitude.
  float ctrl_zoom_limit = 0.0F;
};

[[nodiscard]] CameraLimits LimitsFor(const MapCameraRules& rules, bool expanded_zoom, bool bounded);

struct CameraDrag {
  float dx_px = 0.0F;
  float dy_px = 0.0F;
  bool orbit = false;
  bool tilt = false;
  bool zoom = false;
};

// `ctrl_held` est la touche physique : elle élargit le plafond d'altitude quel
// que soit le raccourci qui incline.
void DragCamera(ro::render::Camera& camera, const CameraDrag& drag, const CameraLimits& limits,
                bool ctrl_held);

// `forward` : un cran loin du joueur, qui recule la caméra.
void WheelZoomCamera(ro::render::Camera& camera, bool forward, const CameraLimits& limits);
void WheelTiltCamera(ro::render::Camera& camera, bool forward, const CameraLimits& limits,
                     bool ctrl_held);

// Refusées sur une carte à bornes propres ; rendent vrai si la caméra a bougé.
bool ResetCameraZoom(ro::render::Camera& camera, const CameraLimits& limits);
bool ResetCameraTilt(ro::render::Camera& camera, const CameraLimits& limits);
bool ResetCameraYaw(ro::render::Camera& camera, const CameraLimits& limits);

// Recul, altitude et lacet de départ ; la vue de sa table sur une carte à bornes
// propres.
void ResetCamera(ro::render::Camera& camera, const CameraLimits& limits);

// Un recul et une altitude pour le dehors, un autre couple pour le dedans.
struct CameraMemory {
  NativeView outdoor;
  NativeView indoor;
  // LA CARTE QUITTÉE ÉTAIT-ELLE UN INTÉRIEUR ? Son lacet imposé ne doit pas
  // sortir avec le joueur : dehors, la vue reprend celui d'avant l'entrée.
  bool left_indoor = false;

  CameraMemory();
};

void RememberCamera(CameraMemory& memory, const ro::render::Camera& camera,
                    const CameraLimits& leaving);
void ArriveWithCamera(ro::render::Camera& camera, const CameraMemory& memory,
                      const CameraLimits& arriving);

}  // namespace moonclient
