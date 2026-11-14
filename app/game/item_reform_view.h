#pragma once

// LA RÉFORME VUE PAR LA FENÊTRE : ce que le modèle devient à l'écran, ce que chaque
// refus et chaque résultat disent.
//
// ⚠ À PART DE L'ÉTAGE, ET SANS FIL NI TEXTURE : les noms et les icônes arrivent par
// `ItemReformLooks`, ce qui laisse tout ceci s'essayer sur un banc.

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/inventory.h"
#include "ro/sim/item_reform.h"
#include "ro/ui/item_reform_window.h"

namespace moonclient {

// Ce que le joueur lit quand une pièce ou une demande est refusée. `recipe` peut
// être nul quand la pièce n'en a pas ; `material_name` nomme le matériau manquant.
[[nodiscard]] std::string ItemReformRefusalText(ro::sim::ItemReformRefusal refusal,
                                                const ro::sim::ItemReformRecipe* recipe,
                                                std::string_view material_name = {});

// Le mot du serveur, une fois sa réponse reçue. Vide pour une réussite : la pièce
// réformée s'ouvre sous les yeux du joueur.
[[nodiscard]] std::string ItemReformResultText(std::uint8_t code);

// D'où viennent les noms et les icônes.
struct ItemReformLooks {
  std::function<std::string(const ro::sim::InventoryItem&)> name_of_held;
  std::function<std::string(std::uint32_t)> name_of_id;
  std::function<ro::ui::SkinImage(std::uint32_t)> icon_of;
};

struct ItemReformScene {
  const ro::sim::ItemReform& reform;
  const ro::sim::ItemReformTrigger& trigger;
  const ro::sim::Inventory& inventory;
  const ItemReformLooks& looks;
  // Le dernier refus du serveur.
  std::string_view notice;
};

// Compose la vue, et garde ce vers quoi elle pointe jusqu'à la prochaine.
class ItemReformViewComposer {
 public:
  [[nodiscard]] const ro::ui::ItemReformView& Compose(const ItemReformScene& scene);

 private:
  [[nodiscard]] ro::ui::TradeItemView HeldOf(const ItemReformScene& scene,
                                             const ro::sim::InventoryItem& item);
  void ComposeChoice(const ItemReformScene& scene);
  void ComposeTexts(const ItemReformScene& scene);

  // ⚠ LES VUES POINTENT DANS CES TEXTES : ils se réservent avant qu'on en prenne un.
  std::vector<std::string> texts_;
  std::vector<ro::ui::ItemReformPieceView> candidates_;
  std::vector<ro::ui::ItemReformMaterialView> materials_;
  std::vector<std::string_view> blockers_;
  ro::ui::ItemReformView view_;
};

}  // namespace moonclient
