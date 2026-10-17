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

#include "ro/ui/map_cells.h"
#include "ro/ui/skin_art.h"

struct ImFont;

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

// LA PLAGE D'HEURES d'un objet « la nuit seulement », telle que le panneau la
// montre et la rend : choisie ou non, et ses bornes en heures du serveur.
struct MapEditorHours {
  bool chosen = false;
  float from_hour = 0.0F;
  float to_hour = 0.0F;
};

// UNE LUMIÈRE PONCTUELLE DU DOCUMENT, telle que le panneau la montre et la
// rend : position dans le repère du RSW, couleur qui porte l'intensité, rayon
// où elle s'éteint, en unités du RSW.
struct MapEditorLight {
  std::array<float, 3> position{};
  std::array<float, 3> color{1.0F, 1.0F, 1.0F};
  float range = 0.0F;
  bool night_only = false;
  MapEditorHours hours;
  bool flicker = false;
  std::string why;
};

// UN EFFET DE CARTE DU DOCUMENT, tel que le panneau le montre et le rend :
// numéro, position dans le repère du RSW, cadence et paramètres comme le RSW
// les porte ; et sa lueur, quand il en a une.
struct MapEditorEffect {
  std::int32_t number = 0;
  std::array<float, 3> position{};
  float emit_speed = 0.0F;
  std::array<float, 4> param{};
  bool night_only = false;
  MapEditorHours hours;
  bool glows = false;
  std::array<float, 3> glow_color{2.0F, 1.4F, 0.7F};
  float glow_range = 50.0F;
  bool glow_flicker = true;
  std::string why;
};

// UN EFFET DE LA CARTE — un objet d'effet du RSW —, tel que le panneau le
// montre et le rend : son rang et son numéro, sa place d'origine et celle que
// l'édition lui donne, et ce qu'elle en fait.
struct MapEditorMapEffect {
  std::size_t effect_index = 0;
  std::int32_t number = 0;
  std::array<float, 3> origin{};
  std::array<float, 3> position{};
  bool edited = false;
  bool hidden = false;
  bool night_only = false;
  MapEditorHours hours;
  bool glows = false;
  std::array<float, 3> glow_color{2.0F, 1.4F, 0.7F};
  float glow_range = 50.0F;
  bool glow_flicker = true;
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
  // Un effet de carte, et l'effet désigné : des losanges, en `from`.
  kEffect = 9,
  kEffectSelected = 10,
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
  // La taille de la carte : les positions se montrent en cases depuis son
  // coin — voir `CellsOfNative`.
  MapCubes map_cubes;
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
  // LES EFFETS DE CARTE du document, et celui qu'on règle.
  std::vector<MapEditorEffect> effects;
  std::optional<std::size_t> selected_effect;
  // LES EFFETS DE LA CARTE, dans l'ordre du RSW, et celui qu'on règle.
  std::vector<MapEditorMapEffect> map_effects;
  std::optional<std::size_t> selected_map_effect;
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
// Ajouter l'effet de numéro `number`, là où l'appelant pose ce qu'on ajoute.
struct AddEffect {
  std::int32_t number = 0;
};
// Régler l'effet de rang `slot`, ou plus aucun.
struct SelectEffect {
  std::optional<std::size_t> slot;
};
// L'effet de rang `slot` devient `effect`. `released`, comme pour `Place`.
struct SetEffect {
  std::size_t slot = 0;
  MapEditorEffect effect;
  bool released = true;
};
struct RemoveEffect {
  std::size_t slot = 0;
};
// Régler l'effet de la carte de rang `index`, ou plus aucun.
struct SelectMapEffect {
  std::optional<std::size_t> index;
};
// L'effet de la carte de rang `index` devient `effect`. `released`, comme pour
// `Place`.
struct SetMapEffect {
  std::size_t index = 0;
  MapEditorMapEffect effect;
  bool released = true;
};
// Rendre à l'effet de la carte de rang `index` ce que le RSW en dit.
struct RevertMapEffect {
  std::size_t index = 0;
};
// Déplacer l'effet de la carte de rang `index` en `native`, à la flèche.
struct MoveMapEffect {
  std::size_t index = 0;
  std::array<float, 3> native{};
  bool released = true;
};
// Déplacer l'effet de rang `slot` en `native`, à la flèche.
struct MoveEffect {
  std::size_t slot = 0;
  std::array<float, 3> native{};
  bool released = true;
};
// Pousser ce qui est désigné de `delta`, dans le repère du RSW.
struct Nudge {
  std::array<float, 3> delta{};
};
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
    map_editor::Duplicate, map_editor::MoveLight, map_editor::Focus, map_editor::Delete,
    map_editor::Nudge, map_editor::AddEffect, map_editor::SelectEffect, map_editor::SetEffect,
    map_editor::RemoveEffect, map_editor::MoveEffect, map_editor::SelectMapEffect,
    map_editor::SetMapEffect, map_editor::RevertMapEffect, map_editor::MoveMapEffect>;

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
  // LA POLICE DES NOMS DE FICHIERS, posée par l'appelant : les modèles portent
  // des noms coréens, que la famille choisie par le joueur ne dessine pas
  // toujours — Verdana n'a aucun hangeul. Nulle : la police courante.
  ImFont* names_font = nullptr;
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
  // LES EFFETS : le numéro qu'ajoute le bouton — les flammes de pvp_n_5-5 —,
  // et la raison de l'effet réglé, comme celle d'une lumière.
  static constexpr std::int32_t kFlameEffect = 47;
  std::int32_t effect_number = kFlameEffect;
  std::array<char, kWhyCapacity> effect_why{};
  std::optional<std::size_t> effect_why_of;
  bool effect_why_typing = false;
  // La raison de l'effet de la carte réglé, comme celle d'un effet ajouté.
  std::array<char, kWhyCapacity> map_effect_why{};
  std::optional<std::size_t> map_effect_why_of;
  bool map_effect_why_typing = false;
};

// Dessine le panneau, et rend les intentions de l'image dans `intents`.
void DrawMapEditor(const MapEditorView& view, MapEditorPanelState& panel,
                   std::vector<MapEditorIntent>& intents, const SkinArt& art);

}  // namespace ro::ui
