#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace ro::ui {

// L'OUTIL CASES DE L'ÉDITEUR DE CARTE : les cases que nul ne traverse, serveur
// compris. Voir docs/adr/0131.

// Ce qu'une case peinte arrête.
enum class MapEditorCellBlock : std::uint8_t {
  // Ni la marche ni les tirs.
  kWall = 0,
  // La marche seulement : les tirs passent.
  kFence = 1,
};

// LE PINCEAU DE CASES, tel que le panneau le montre et le règle.
struct MapEditorCellBrush {
  // Le rayon, en cases : 0 ne peint que la case sous la souris.
  float radius_cells = kDefaultRadiusCells;
  MapEditorCellBlock block = MapEditorCellBlock::kWall;

  static constexpr float kDefaultRadiusCells = 0.0F;
  static constexpr float kLeastRadiusCells = 0.0F;
  static constexpr float kGreatestRadiusCells = 20.0F;

  friend bool operator==(const MapEditorCellBrush&, const MapEditorCellBrush&) = default;
};

// LES CASES DE LA CARTE, telles que le panneau les montre : le pinceau, ce que
// le document bloque, et si un modèle est désigné pour en bloquer l'emprise.
struct MapEditorCells {
  MapEditorCellBrush brush;
  std::size_t blocked = 0;
  std::string why;
  bool model_selected = false;
  // La case sous la souris, en cases ; rien hors du sol.
  std::optional<std::array<std::int32_t, 2>> pointer;
};

// CE QUE LA SECTION GARDE d'une image à la suivante : la raison qu'on tape.
struct MapEditorCellsPanelState {
  static constexpr std::size_t kWhyCapacity = 256;
  std::array<char, kWhyCapacity> why{};
  bool why_typing = false;
};

namespace map_editor {
// Régler le pinceau de cases.
struct SetCellBrush {
  MapEditorCellBrush brush;
};
// Le glissé du pinceau est fini : il devient UNE entrée de l'historique.
struct EndCellStroke {};
// Libérer toutes les cases que le document bloque.
struct ClearCells {};
// Dire pourquoi les cases sont bloquées.
struct SetCellsWhy {
  std::string why;
};
// Bloquer les cases sous l'emprise du modèle désigné.
struct BlockModelFootprint {};
}  // namespace map_editor

}  // namespace ro::ui
