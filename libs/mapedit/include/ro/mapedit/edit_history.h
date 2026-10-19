#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "ro/mapedit/map_edit.h"

namespace ro::mapedit {

// L'HISTORIQUE DE L'ÉDITEUR : annuler et rétablir, geste par geste.
//
// Chaque geste se retient par le document d'AVANT et celui d'APRÈS : il sait
// donc se défaire et se refaire, quelle que soit l'opération qu'il a posée. Un
// document pèse quelques centaines d'octets par retouche ; les garder entiers
// coûte moins qu'une inversion opération par opération, et ne se trompe pas.
//
// Un geste qui glisse — une poignée, une brosse — ne devient UNE entrée qu'au
// relâcher : c'est à l'appelant de ne l'enregistrer qu'alors.
class EditHistory {
 public:
  // Le nombre de gestes retenus ; au-delà, les plus anciens s'oublient.
  static constexpr std::size_t kMaxSteps = 200;

  // Retient un geste. Ce qui avait été annulé ne se rétablit plus.
  // `document` : le document qu'il change — `kModelLightsDocument` —, vide
  // pour celui de la carte.
  void Record(std::string label, MapEdit before, MapEdit after, std::string document = {});

  [[nodiscard]] bool CanUndo() const noexcept { return !past_.empty(); }
  [[nodiscard]] bool CanRedo() const noexcept { return !future_.empty(); }
  // Le nom du geste que `Undo` défera, ou que `Redo` refera ; vide sinon.
  [[nodiscard]] std::string UndoLabel() const;
  [[nodiscard]] std::string RedoLabel() const;
  // Le document que `Undo` ou `Redo` rendra, comme `Record` l'a reçu.
  [[nodiscard]] std::string UndoDocument() const;
  [[nodiscard]] std::string RedoDocument() const;

  // Le document d'avant le dernier geste, qui passe au futur ; rien sans geste.
  [[nodiscard]] std::optional<MapEdit> Undo();
  // Le document d'après le dernier geste défait, qui repasse au passé.
  [[nodiscard]] std::optional<MapEdit> Redo();

  // Oublie tout : une autre carte, un autre document.
  void Clear() noexcept;

 private:
  struct Step {
    std::string label;
    MapEdit before;
    MapEdit after;
    std::string document;
  };
  std::vector<Step> past_;
  std::vector<Step> future_;
};

// CE QUE L'ÉDITEUR DÉSIGNE. Un modèle, pour l'instant : par son rang dans le
// RSW de la carte.
struct EditSelection {
  std::optional<std::size_t> model_index;

  [[nodiscard]] bool empty() const noexcept { return !model_index.has_value(); }
  void Clear() noexcept { model_index.reset(); }

  friend bool operator==(const EditSelection&, const EditSelection&) = default;
};

}  // namespace ro::mapedit
