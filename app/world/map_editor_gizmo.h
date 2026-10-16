#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/map_effect_math.h"
#include "ro/ui/map_editor_panel.h"

namespace moonclient {

// LES POIGNÉES DE L'ÉDITEUR DE CARTE : voir docs/adr/0118.
//
// Elles vivent à l'ÉCRAN. Chaque poignée se projette en une suite de points ;
// un appui assez près d'elle la saisit, et le glissé de la souris se projette
// sur l'axe que la poignée montre à l'écran. Rien n'en passe par le moteur de
// rendu : le panneau trace les segments par-dessus la vue.
//
// Les axes sont ceux du RSW : un pas d'une unité le long de chacun, traduit
// dans le monde, donne la direction de la poignée — le repère du RSW retourne
// deux de ses axes, et c'est ainsi qu'on ne se trompe pas de signe.

// L'axe que tient une poignée ; `kAll` est la poignée d'échelle uniforme.
enum class GizmoAxis : std::uint8_t { kX = 0, kY = 1, kZ = 2, kAll = 3 };

// Où se tiennent les poignées : le pivot du modèle dans le monde, les trois
// axes du RSW dans le monde, et leur longueur, en unités du monde.
struct GizmoFrame {
  glm::vec3 origin{0.0F};
  std::array<glm::vec3, 3> axes{};
  float length = 1.0F;
};

// La part de la distance de la caméra que mesure une poignée : elle garde ainsi
// la même taille à l'écran, quel que soit le recul.
inline constexpr float kGizmoLengthShare = 0.18F;
// À combien de pixels d'une poignée un appui la saisit.
inline constexpr float kGizmoGrabPixels = 8.0F;
// Les degrés que tourne un modèle par pixel de glissé.
inline constexpr float kGizmoDegreesPerPixel = 0.5F;
// Le facteur d'échelle par pixel de glissé de la poignée uniforme.
inline constexpr float kGizmoUniformScalePerPixel = 0.01F;
// L'échelle la plus faible qu'un glissé laisse : un modèle à l'échelle nulle
// ne se verrait plus, et ne se saisirait plus.
inline constexpr float kGizmoSmallestScale = 0.01F;

[[nodiscard]] GizmoFrame GizmoFrameOf(const ro::render::MapEffectFrame& map_frame,
                                      const std::array<float, 3>& rsw_position,
                                      const ro::render::Camera& camera);

// UNE POIGNÉE À L'ÉCRAN : ses points, en pixels de fenêtre, et ce qu'un pixel
// de glissé le long de son axe vaut en unités du monde.
struct GizmoHandle {
  GizmoAxis axis = GizmoAxis::kX;
  std::vector<glm::vec2> points;
  // Vrai pour un anneau, dont le dernier point rejoint le premier.
  bool closed = false;
  // L'axe à l'écran : où se projette un pas de `GizmoFrame::length`. Nul pour
  // un anneau et pour la poignée uniforme.
  glm::vec2 screen_axis{0.0F, 0.0F};
};

// Les poignées de l'outil `tool` autour de `frame`, projetées à l'écran. Une
// poignée dont un point passe derrière l'œil ne se montre pas.
[[nodiscard]] std::vector<GizmoHandle> GizmoHandles(ro::ui::MapEditorTool tool,
                                                    const GizmoFrame& frame,
                                                    const ro::render::Camera& camera,
                                                    float aspect_ratio, float viewport_width,
                                                    float viewport_height);

// La poignée qu'un appui en `pointer` saisit : la plus proche, à moins de
// `kGizmoGrabPixels`.
[[nodiscard]] std::optional<std::size_t> GrabbedHandle(const std::vector<GizmoHandle>& handles,
                                                       glm::vec2 pointer);

// LE PLACEMENT QU'UN GLISSÉ DONNE : `start` déplacé, tourné ou mis à l'échelle
// selon l'outil et la poignée, pour un glissé de `drag` pixels depuis l'appui.
[[nodiscard]] ro::ui::map_editor::Place Dragged(ro::ui::MapEditorTool tool,
                                                const GizmoHandle& handle, const GizmoFrame& frame,
                                                const ro::ui::map_editor::Place& start,
                                                glm::vec2 drag);

// L'AIMANTATION, Maj tenue : `placed` arrondi au pas de son outil — la
// position au demi-carreau, la rotation à quinze degrés, l'échelle au
// dixième. Seules les composantes que le glissé a changées depuis `start`
// s'arrondissent : un axe qu'on ne tire pas ne saute pas sur la grille.
inline constexpr float kSnapPositionUnits = 5.0F;
inline constexpr float kSnapRotationDegrees = 15.0F;
inline constexpr float kSnapScale = 0.1F;
[[nodiscard]] ro::ui::map_editor::Place Snapped(ro::ui::MapEditorTool tool,
                                                const ro::ui::map_editor::Place& start,
                                                ro::ui::map_editor::Place placed);

// Les traits des poignées, à tracer par-dessus la vue ; celle de rang
// `active`, saisie, se distingue.
[[nodiscard]] std::vector<ro::ui::MapEditorLine> GizmoLines(const std::vector<GizmoHandle>& handles,
                                                            std::optional<std::size_t> active);

}  // namespace moonclient
