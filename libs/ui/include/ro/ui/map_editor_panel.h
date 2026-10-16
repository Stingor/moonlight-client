#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "ro/ui/skin_art.h"

namespace ro::ui {

// LE PANNEAU DE L'ÉDITEUR DE CARTE : voir docs/adr/0118.
//
// 🔴 IL NE POSSÈDE RIEN. Il reçoit une vue en lecture seule, `MapEditorView`,
// et rend les INTENTIONS du staff, `MapEditorIntent` ; c'est l'appelant qui
// tient le document, l'historique et la sélection, et qui les change.

// CE QUE LE PANNEAU MONTRE, posé par l'appelant à chaque image.
struct MapEditorView {
  // Le mode éditeur est-il ouvert ? Fermé, le panneau ne se dessine pas.
  bool active = false;
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
struct Save {};
}  // namespace map_editor

using MapEditorIntent =
    std::variant<map_editor::Toggle, map_editor::Undo, map_editor::Redo, map_editor::Select,
                 map_editor::Place, map_editor::SetWhy, map_editor::Revert, map_editor::Save>;

// CE QUE LE PANNEAU GARDE POUR LUI d'une image à la suivante : la saisie en
// cours, qui n'est pas encore une intention.
struct MapEditorPanelState {
  // La place réservée à la raison d'une retouche, zéro final compris.
  static constexpr std::size_t kWhyCapacity = 256;

  std::array<char, kWhyCapacity> why{};
  // Le modèle dont `why` est la saisie : un autre modèle la recharge.
  std::optional<std::size_t> why_of;
};

// Dessine le panneau, et rend les intentions de l'image dans `intents`.
void DrawMapEditor(const MapEditorView& view, MapEditorPanelState& panel,
                   std::vector<MapEditorIntent>& intents, const SkinArt& art);

}  // namespace ro::ui
