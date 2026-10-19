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

#include "ro/render/model_pose.h"
#include "ro/ui/map_cells.h"
#include "ro/ui/map_editor_hours.h"
#include "ro/ui/map_editor_library.h"
#include "ro/ui/map_editor_sounds.h"
#include "ro/ui/map_editor_sprite_view.h"
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
  // Le document le retouche ; il le masque ; il l'a ajouté ; il le rend
  // translucide.
  bool edited = false;
  bool hidden = false;
  bool added = false;
  bool translucent = false;
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
  bool shadow = true;
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
  bool glow_shadow = true;
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
  bool glow_shadow = true;
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
  // Le cercle de la brosse de relief, tracé au sol sous la souris.
  kBrush = 11,
  // Un point de la grille des coins du sol, un point que le document retouche,
  // un point sélectionné et le point désigné : des points, en `from`.
  kGroundPoint = 12,
  kGroundPointEdited = 13,
  kGroundPointSelected = 14,
  kGroundPointDesignated = 15,
  // Un sprite de décor, et le sprite désigné : des carrés, en `from`.
  kSprite = 16,
  kSpriteSelected = 17,
  // Un son d'ambiance, et le son désigné : des carrés, en `from`.
  kSound = 18,
  kSoundSelected = 19,
};

// L'OUTIL DE LA VUE : les poignées qui déplacent, tournent ou mettent à
// l'échelle, les sommets d'un maillage, ou la brosse de relief.
enum class MapEditorTool : std::uint8_t {
  kMove = 0,
  kRotate = 1,
  kScale = 2,
  kVertices = 3,
  kRelief = 4,
};

// LA BROSSE DE RELIEF, telle que le panneau la montre et la règle.
enum class MapEditorBrushMode : std::uint8_t {
  // Lever ; Maj tenue, abaisser.
  kRaise = 0,
  // Tirer vers la hauteur sous le centre au premier appui.
  kFlatten = 1,
  // Tirer vers la moyenne des voisins.
  kSmooth = 2,
  // Désigner les points de la grille un à un, et les lever à la poignée.
  kPoints = 3,
};
struct MapEditorBrush {
  // Le rayon, en cases.
  float radius_cells = kDefaultRadiusCells;
  // Ce que la brosse déplace au centre, en unités du monde par seconde de
  // glissé.
  float units_per_second = kDefaultUnitsPerSecond;
  MapEditorBrushMode mode = MapEditorBrushMode::kRaise;

  static constexpr float kDefaultRadiusCells = 6.0F;
  static constexpr float kDefaultUnitsPerSecond = 10.0F;
  static constexpr float kLeastRadiusCells = 1.0F;
  static constexpr float kGreatestRadiusCells = 60.0F;
  static constexpr float kLeastUnitsPerSecond = 0.1F;
  static constexpr float kGreatestUnitsPerSecond = 200.0F;

  friend bool operator==(const MapEditorBrush&, const MapEditorBrush&) = default;
};

// LE POINT DÉSIGNÉ du mode Sommets : sa place en cases depuis le coin de la
// carte, sa hauteur en unités du monde, vers le haut, et si le document le
// retouche.
struct MapEditorGroundPoint {
  std::array<float, 2> cells{};
  float height = 0.0F;
  bool edited = false;
};

// LE RELIEF DE LA CARTE, tel que le panneau le montre : la brosse, ce que le
// document retouche, et ce que coûte un remaillage.
struct MapEditorGround {
  // Le relief retouché se voit : « Ombres en temps réel » est allumé. Sinon,
  // l'outil Relief est grisé.
  bool allowed = true;
  MapEditorBrush brush;
  // Les points de la grille des coins que le document lève ou abaisse.
  std::size_t edited_points = 0;
  std::string why;
  // Le dernier remaillage du sol, en millisecondes, et si un glissé remaille
  // en direct ou seulement au relâcher ; rien avant le premier remaillage.
  std::optional<double> remesh_ms;
  bool remesh_live = true;
  // LE MODE SOMMETS : le point désigné, le nombre de points sélectionnés — le
  // désigné compris —, et combien d'entre eux le document retouche.
  std::optional<MapEditorGroundPoint> designated;
  std::size_t selected_points = 0;
  std::size_t selected_edited = 0;
};
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
  // LE CENTRE DE LA BOÎTE du modèle désigné, dans le repère du RSW : là
  // qu'« Ajouter une lumière ici » ancre la sienne — au cœur d'un lampion, et
  // non à son pivot. Rien pour un modèle qui ne se dessine pas.
  std::optional<std::array<float, 3>> selected_center;
  bool hidden = false;
  // De 0 (invisible) à 1 (plein).
  float opacity = 1.0F;
  // Comment il joue, comme le .rsw l'écrit : ce que « Copier ce modèle » emporte.
  std::int32_t animation_type = render::kModelPlayLooped;
  float animation_speed = 1.0F;
  std::string why;
  // Les rangs que le document retouche sur cette carte.
  std::vector<std::size_t> edited;
  // L'ARBRE DE LA SCÈNE : tous les modèles de la carte, dans l'ordre du RSW.
  // Partagé et non recopié : une carte en compte des centaines, et il ne change
  // qu'avec le document. Jamais nul quand le mode est ouvert.
  std::shared_ptr<const std::vector<MapEditorModelRow>> models;
  // Ce que le mode trace par-dessus la vue.
  std::vector<MapEditorLine> lines;
  // LA BIBLIOTHÈQUE : les modèles de l'installation, leurs dossiers, les
  // favoris, les récents et les vignettes. Voir `ModelLibraryView`.
  ModelLibraryView library;
  // LES LUMIÈRES PONCTUELLES du document, et celle qu'on règle.
  std::vector<MapEditorLight> lights;
  std::optional<std::size_t> selected_light;
  // LES EFFETS DE CARTE du document, et celui qu'on règle.
  std::vector<MapEditorEffect> effects;
  std::optional<std::size_t> selected_effect;
  // LES EFFETS DE LA CARTE, dans l'ordre du RSW, et celui qu'on règle.
  std::vector<MapEditorMapEffect> map_effects;
  std::optional<std::size_t> selected_map_effect;
  // LES SPRITES DE DÉCOR du document, et celui qu'on règle.
  std::vector<MapEditorSprite> sprites;
  std::optional<std::size_t> selected_sprite;
  // LES SONS D'AMBIANCE : ceux du RSW dans son ordre, puis ceux que le document
  // ajoute ; celui qu'on règle ; et les `.wav` de l'installation, relatifs à
  // data\wav\, triés et partagés comme la bibliothèque.
  std::vector<MapEditorSound> sounds;
  std::optional<MapEditorSoundKey> selected_sound;
  std::shared_ptr<const std::vector<std::string>> sound_library;
  // LE RELIEF : la brosse, et ce que le document retouche.
  MapEditorGround ground;
  // L'aperçu du modèle survolé dans l'arbre, la liste ou la grille : la
  // vignette du fichier que le panneau survolait à l'image précédente.
  MapEditorPreview preview;
  // Le document diffère de celui qui est enregistré.
  bool unsaved = false;
  // La dernière nouvelle : enregistré, refusé, et pourquoi.
  std::string status;
  // Le dernier envoi au serveur a été devancé par une autre révision : il
  // attend qu'on écrase celle-ci, ou qu'on la recharge.
  bool server_conflict = false;
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
  // L'opacité, de 0 à 1 ; rien : celle que le document lui donne déjà.
  std::optional<float> opacity;
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
  // Où il se pose, dans le repère du RSW ; sans elle, là où regarde la caméra.
  std::optional<std::array<float, 3>> position;
  // Ce que porte un modèle collé ; sans eux, ceux d'un modèle neuf.
  std::array<float, 3> rotation{};
  std::array<float, 3> scale{1.0F, 1.0F, 1.0F};
  std::optional<float> opacity;
  std::int32_t animation_type = render::kModelPlayLooped;
  float animation_speed = 1.0F;
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
// Renvoyer le document au serveur, par-dessus la révision qui l'a devancé.
struct Overwrite {};
// Adopter la révision du serveur ; les retouches non enregistrées se perdent.
struct Reload {};
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
// Ajouter une lumière ponctuelle à `position`, dans le repère du RSW ; sans
// position, là où l'appelant pose ce qu'on ajoute.
struct AddLight {
  std::optional<std::array<float, 3>> position;
};
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
// Régler la brosse de relief.
struct SetBrush {
  MapEditorBrush brush;
};
// Le glissé de la brosse est fini : il devient UNE entrée de l'historique.
struct EndGroundStroke {};
// Retirer toutes les retouches du relief de la carte.
struct RevertGround {};
// Dire pourquoi le relief est retouché.
struct SetGroundWhy {
  std::string why;
};
// LE MODE SOMMETS : lever les points sélectionnés de `lift` unités du monde
// depuis le début du geste. `released`, comme pour `Place`.
struct LiftGroundPoints {
  float lift = 0.0F;
  bool released = true;
};
// Porter le point désigné à la hauteur `height`, en unités du monde ; les
// autres points sélectionnés bougent d'autant. `released`, comme pour `Place`.
struct SetGroundPointHeight {
  float height = 0.0F;
  bool released = true;
};
// Retirer les retouches des points sélectionnés.
struct RevertGroundPoints {};
}  // namespace map_editor

using MapEditorIntent = std::variant<
    map_editor::Toggle, map_editor::Undo, map_editor::Redo, map_editor::Select, map_editor::Place,
    map_editor::SetWhy, map_editor::Revert, map_editor::Save, map_editor::SetTool,
    map_editor::AddModel, map_editor::MoveVertex, map_editor::RevertVertex, map_editor::SetAllPoses,
    map_editor::AddLight, map_editor::SelectLight, map_editor::SetLight, map_editor::RemoveLight,
    map_editor::Duplicate, map_editor::MoveLight, map_editor::Focus, map_editor::Delete,
    map_editor::Nudge, map_editor::AddEffect, map_editor::SelectEffect, map_editor::SetEffect,
    map_editor::RemoveEffect, map_editor::MoveEffect, map_editor::SelectMapEffect,
    map_editor::SetMapEffect, map_editor::RevertMapEffect, map_editor::MoveMapEffect,
    map_editor::SetBrush, map_editor::EndGroundStroke, map_editor::RevertGround,
    map_editor::SetGroundWhy, map_editor::LiftGroundPoints, map_editor::SetGroundPointHeight,
    map_editor::RevertGroundPoints, map_editor::Overwrite, map_editor::Reload,
    map_editor::AddSprite, map_editor::SelectSprite, map_editor::SetSprite,
    map_editor::RemoveSprite, map_editor::MoveSprite, map_editor::SelectSound, map_editor::SetSound,
    map_editor::MoveSound, map_editor::RevertSound, map_editor::AddSound, map_editor::ListenSound>;

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
  // LE FICHIER SURVOLÉ dans l'arbre, la liste ou la grille, relatif à
  // data\model\ ; vide sans survol. Refait à chaque image : l'appelant en rend
  // l'aperçu.
  std::string hovered_model;
  // La fenêtre « Bibliothèque de modèles ».
  ModelLibraryWindowState library_window;

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
  // La raison du relief, comme celle d'un modèle.
  std::array<char, kWhyCapacity> ground_why{};
  bool ground_why_typing = false;
  // LES SPRITES DE DÉCOR : le nom ou le numéro qu'ajoute le bouton, et la
  // raison du sprite réglé, comme celle d'une lumière.
  std::array<char, kFilterCapacity> sprite_input{};
  std::array<char, kWhyCapacity> sprite_why{};
  std::optional<std::size_t> sprite_why_of;
  bool sprite_why_typing = false;
  // Le nom du sprite réglé, saisi comme sa raison.
  std::array<char, kFilterCapacity> sprite_name{};
  std::optional<std::size_t> sprite_name_of;
  bool sprite_name_typing = false;
  // La section « Sons ».
  MapEditorSoundPanelState sounds;
};

// Dessine le panneau, et rend les intentions de l'image dans `intents`.
void DrawMapEditor(const MapEditorView& view, MapEditorPanelState& panel,
                   std::vector<MapEditorIntent>& intents, const SkinArt& art);

}  // namespace ro::ui
