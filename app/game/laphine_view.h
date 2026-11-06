#pragma once

// LE LAPHINE VU PAR LA FENÊTRE : ce que le modèle devient à l'écran, ce que chaque
// refus et chaque résultat disent.
//
// ⚠ À PART DE L'ÉTAGE, ET SANS FIL NI TEXTURE : les noms et les icônes arrivent par
// `LaphineLooks`, ce qui laisse tout ceci s'essayer sur un banc.

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/inventory.h"
#include "ro/sim/laphine.h"
#include "ro/ui/laphine_window.h"

namespace moonclient {

// Ce que le joueur lit quand un objet ou une demande est refusé.
[[nodiscard]] std::string LaphineRefusalText(ro::sim::LaphineRefusal refusal,
                                             const ro::sim::LaphineRecipe& recipe,
                                             std::uint16_t needed_amount);

// Le mot du serveur, une fois sa réponse reçue. Vide pour une réussite, qui ne se
// dit pas : les objets obtenus s'annoncent d'eux-mêmes, et la pièce améliorée
// s'ouvre sous les yeux du joueur.
[[nodiscard]] std::string LaphineResultText(ro::sim::LaphineKind kind, std::uint16_t code);

// D'où viennent les noms et les icônes.
struct LaphineLooks {
  std::function<std::string(const ro::sim::InventoryItem&)> name_of_held;
  std::function<std::string(std::uint32_t)> name_of_id;
  std::function<ro::ui::SkinImage(std::uint32_t)> icon_of;
};

struct LaphineScene {
  const ro::sim::Laphine& laphine;
  const ro::sim::LaphineRecipe& recipe;
  const ro::sim::Inventory& inventory;
  const LaphineLooks& looks;
  // Le dernier refus du serveur.
  std::string_view notice;
};

// Compose la vue, et garde ce vers quoi elle pointe jusqu'à la prochaine.
class LaphineViewComposer {
 public:
  [[nodiscard]] const ro::ui::LaphineView& Compose(const LaphineScene& scene);

 private:
  [[nodiscard]] ro::ui::LaphineItemView RowOf(const LaphineScene& scene,
                                              const ro::sim::InventoryItem& item, bool deposited);
  void ComposeTexts(const LaphineScene& scene);

  // ⚠ LES VUES POINTENT DANS CES TEXTES : ils se réservent avant qu'on en prenne un.
  std::vector<std::string> texts_;
  std::vector<ro::ui::LaphineItemView> candidates_;
  std::vector<ro::ui::LaphineItemView> deposited_;
  std::vector<ro::ui::LaphineAcceptedView> accepted_;
  std::vector<std::string_view> blockers_;
  ro::ui::LaphineView view_;
};

}  // namespace moonclient
