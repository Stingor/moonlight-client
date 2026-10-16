#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include <glm/glm.hpp>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LE PANNEAU DE L'ÉDITEUR DE CARTE : voir docs/adr/0118.
//
// 🔴 IL NE POSSÈDE RIEN. Il reçoit une vue en lecture seule, `MapEditorView`,
// et rend les INTENTIONS du staff, `MapEditorIntent` ; c'est l'appelant qui
// tient le document, l'historique et la sélection, et qui les change.

// UNE LIGNE DE L'ARBRE DE LA SCÈNE : un modèle de la carte.
struct MapEditorModelRow {
  // Son rang dans le RSW, et son fichier en UTF-8.
  std::size_t model_index = 0;
  std::string file;
  // Le document le retouche ; il le masque.
  bool edited = false;
  bool hidden = false;
};

// CE QUE LE MODE TRACE PAR-DESSUS LA VUE, sous les fenêtres : des segments
// déjà projetés, en pixels de fenêtre.
enum class MapEditorLineKind : std::uint8_t {
  // La boîte du modèle désigné, et le carré de l'échelle uniforme.
  kSelection = 0,
  // Les poignées des trois axes du RSW.
  kAxisX = 1,
  kAxisY = 2,
  kAxisZ = 3,
  // La poignée saisie.
  kActive = 4,
};

// L'OUTIL DES POIGNÉES : déplacer, tourner ou mettre à l'échelle.
enum class MapEditorTool : std::uint8_t { kMove = 0, kRotate = 1, kScale = 2 };
struct MapEditorLine {
  glm::vec2 from{0.0F, 0.0F};
  glm::vec2 to{0.0F, 0.0F};
  MapEditorLineKind kind = MapEditorLineKind::kSelection;
};

// CE QUE LE PANNEAU MONTRE, posé par l'appelant à chaque image.
struct MapEditorView {
  // Le mode éditeur est-il ouvert ? Fermé, le panneau ne se dessine pas.
  bool active = false;
  MapEditorTool tool = MapEditorTool::kMove;
  std::string map_name;
  // L'historique : ce qu'annuler et rétablir déferaient, vide sans geste.
  std::string undo_label;
  std::string redo_label;
  // Le modèle désigné, par son rang dans le RSW, son fichier en UTF-8 et sa
  // position d'origine ; et ce que l'édition en fait.
  std::optional<std::size_t> selected;
  std::string selected_file;
  std::array<float, 3> origin{};
  std::array<float, 3> position{};
  std::array<float, 3> rotation{};
  std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
  bool hidden = false;
  std::string why;
  // Les rangs que le document retouche sur cette carte.
  std::vector<std::size_t> edited;
  // L'ARBRE DE LA SCÈNE : tous les modèles de la carte, dans l'ordre du RSW.
  std::vector<MapEditorModelRow> models;
  // Ce que le mode trace par-dessus la vue.
  std::vector<MapEditorLine> lines;
  // Le document diffère de celui qui est enregistré.
  bool unsaved = false;
  // La dernière nouvelle : enregistré, refusé, et pourquoi.
  std::string status;
};

// LES INTENTIONS DU STAFF.
namespace map_editor {
// Entrer dans le mode, ou en sortir.
struct Toggle {};
struct Undo {};
struct Redo {};
// Désigner le modèle de ce rang.
struct Select {
  std::size_t model_index = 0;
};
// Le placement que le staff règle. `released` : le geste est fini, et il
// devient une entrée de l'historique ; avant, il ne fait que se voir.
struct Place {
  std::array<float, 3> position{};
  std::array<float, 3> rotation{};
  std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
  bool hidden = false;
  bool released = true;
};
struct SetWhy {
  std::string why;
};
// Rendre au modèle désigné son placement d'origine.
struct Revert {};
// Choisir l'outil des poignées.
struct SetTool {
  MapEditorTool tool = MapEditorTool::kMove;
};
struct Save {};
}  // namespace map_editor

using MapEditorIntent = std::variant<map_editor::Toggle, map_editor::Undo, map_editor::Redo,
                                     map_editor::Select, map_editor::Place, map_editor::SetWhy,
                                     map_editor::Revert, map_editor::Save, map_editor::SetTool>;

// CE QUE LE PANNEAU GARDE POUR LUI d'une image à la suivante : la saisie en
// cours, qui n'est pas encore une intention.
struct MapEditorPanelState {
  // La place réservée à la raison d'une retouche, zéro final compris.
  static constexpr std::size_t kWhyCapacity = 256;
  // Celle du filtre de l'arbre de la scène.
  static constexpr std::size_t kFilterCapacity = 64;

  // Le filtre de l'arbre : un morceau de nom de fichier, ou un rang.
  std::array<char, kFilterCapacity> filter{};
  // N'y montrer que les modèles retouchés.
  bool edited_only = false;

  std::array<char, kWhyCapacity> why{};
  // Le modèle dont `why` est la saisie : un autre modèle la recharge.
  std::optional<std::size_t> why_of;
};

// Dessine le panneau, et rend les intentions de l'image dans `intents`.
void DrawMapEditor(const MapEditorView& view, MapEditorPanelState& panel,
                   std::vector<MapEditorIntent>& intents, const SkinArt& art);

}  // namespace ro::ui
