#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
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
  // Le document le retouche ; il le masque ; il l'a ajouté.
  bool edited = false;
  bool hidden = false;
  bool added = false;
};

// UNE LUMIÈRE PONCTUELLE DU DOCUMENT, telle que le panneau la montre et la
// rend : position dans le repère du RSW, couleur qui porte l'intensité, rayon
// où elle s'éteint, en unités du RSW.
struct MapEditorLight {
  std::array<float, 3> position{};
  std::array<float, 3> color{1.0F, 1.0F, 1.0F};
  float range = 0.0F;
  bool night_only = false;
  bool flicker = false;
  std::string why;
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
  // Un sommet du modèle désigné, et le sommet désigné : des points, tracés en
  // `from` seul.
  kVertex = 5,
  kVertexSelected = 6,
  // Une lumière ponctuelle, et la lumière désignée : des cercles, en `from`.
  kLight = 7,
  kLightSelected = 8,
};

// L'OUTIL DES POIGNÉES : déplacer, tourner ou mettre à l'échelle.
enum class MapEditorTool : std::uint8_t { kMove = 0, kRotate = 1, kScale = 2, kVertices = 3 };
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
  // Le désigné est un modèle que le document ajoute : il se retire au lieu de
  // se rétablir.
  bool selected_added = false;
  // L'OUTIL SOMMETS : le sommet désigné — son nœud et son rang —, le nombre de
  // sommets que le document déplace dans ce modèle, et s'il les déplace pour
  // toutes les poses du fichier.
  std::string selected_vertex;
  std::size_t moved_vertices = 0;
  bool all_poses = false;
  std::array<float, 3> origin{};
  std::array<float, 3> position{};
  std::array<float, 3> rotation{};
  std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
  bool hidden = false;
  std::string why;
  // Les rangs que le document retouche sur cette carte.
  std::vector<std::size_t> edited;
  // L'ARBRE DE LA SCÈNE : tous les modèles de la carte, dans l'ordre du RSW.
  // Partagé et non recopié : une carte en compte des centaines, et il ne change
  // qu'avec le document. Jamais nul quand le mode est ouvert.
  std::shared_ptr<const std::vector<MapEditorModelRow>> models;
  // Ce que le mode trace par-dessus la vue.
  std::vector<MapEditorLine> lines;
  // LA BIBLIOTHÈQUE : les fichiers de modèles de l'installation, relatifs à
  // data\model\, triés. Partagée et non recopiée : elle en compte des milliers.
  std::shared_ptr<const std::vector<std::string>> library;
  // LES LUMIÈRES PONCTUELLES du document, et celle qu'on règle.
  std::vector<MapEditorLight> lights;
  std::optional<std::size_t> selected_light;
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
// Ajouter à la carte un modèle de la bibliothèque, là où l'appelant le pose.
struct AddModel {
  std::string file;
};
// Déplacer le sommet `vertex` du nœud `node`, dans le repère du RSW — comme
// une pose. `released`, comme pour `Place`.
struct MoveVertex {
  std::size_t node = 0;
  std::size_t vertex = 0;
  std::array<float, 3> native{};
  bool released = true;
};
// Rendre au sommet désigné sa position d'origine.
struct RevertVertex {};
// Déplacer les sommets pour toutes les poses du fichier, ou pour la seule
// pose désignée.
struct SetAllPoses {
  bool all = false;
};
// Choisir l'outil des poignées.
struct SetTool {
  MapEditorTool tool = MapEditorTool::kMove;
};
struct Save {};
// Ajouter une copie du modèle désigné, un peu à côté de lui.
struct Duplicate {};
// Amener la caméra sur ce qui est désigné : le modèle, ou la lumière.
struct Focus {};
// Le geste de la touche Suppr sur ce qui est désigné : une lumière ou un modèle
// ajouté se retirent, un modèle de la carte se masque, ou se démasque.
struct Delete {};
// Ajouter une lumière ponctuelle, là où l'appelant pose ce qu'on ajoute.
struct AddLight {};
// Régler la lumière de rang `slot`, ou plus aucune.
struct SelectLight {
  std::optional<std::size_t> slot;
};
// La lumière de rang `slot` devient `light`. `released`, comme pour `Place`.
struct SetLight {
  std::size_t slot = 0;
  MapEditorLight light;
  bool released = true;
};
struct RemoveLight {
  std::size_t slot = 0;
};
// Déplacer la lumière de rang `slot` en `native`, dans le repère du RSW — à la
// flèche, comme une pose. `released`, comme pour `Place`.
struct MoveLight {
  std::size_t slot = 0;
  std::array<float, 3> native{};
  bool released = true;
};
}  // namespace map_editor

using MapEditorIntent = std::variant<
    map_editor::Toggle, map_editor::Undo, map_editor::Redo, map_editor::Select, map_editor::Place,
    map_editor::SetWhy, map_editor::Revert, map_editor::Save, map_editor::SetTool,
    map_editor::AddModel, map_editor::MoveVertex, map_editor::RevertVertex, map_editor::SetAllPoses,
    map_editor::AddLight, map_editor::SelectLight, map_editor::SetLight, map_editor::RemoveLight,
    map_editor::Duplicate, map_editor::MoveLight, map_editor::Focus, map_editor::Delete>;

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
  // Le filtre de la bibliothèque, et le fichier qu'on y a choisi.
  std::array<char, kFilterCapacity> library_filter{};
  std::string library_choice;

  std::array<char, kWhyCapacity> why{};
  // Le modèle dont `why` est la saisie : un autre modèle la recharge.
  std::optional<std::size_t> why_of;
  // On tape dans ce champ : le document ne le recharge pas sous les doigts.
  bool why_typing = false;
  // La raison de la lumière réglée, et la lumière dont c'est la saisie.
  std::array<char, kWhyCapacity> light_why{};
  std::optional<std::size_t> light_why_of;
  // On tape dans ce champ : le document ne le recharge pas sous les doigts.
  bool light_why_typing = false;
};

// Dessine le panneau, et rend les intentions de l'image dans `intents`.
void DrawMapEditor(const MapEditorView& view, MapEditorPanelState& panel,
                   std::vector<MapEditorIntent>& intents, const SkinArt& art);

}  // namespace ro::ui
