#pragma once

// L'AMÉLIORATION DE GRADE VUE PAR LA FENÊTRE : ce que le modèle devient à
// l'écran, ce que chaque refus et chaque résultat disent.
//
// ⚠ À PART DE L'ÉTAGE, ET SANS FIL NI TEXTURE : les noms et les icônes arrivent
// par `GradeEnchantLooks`, ce qui laisse tout ceci s'essayer sur un banc.

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/grade_enchant.h"
#include "ro/sim/inventory.h"
#include "ro/ui/grade_enchant_window.h"

namespace moonclient {

// Ce que le joueur lit quand une pièce ou une demande est refusée ; `item_name`
// nomme l'objet qui manque.
[[nodiscard]] std::string GradeEnchantRefusalText(ro::sim::GradeEnchantRefusal refusal,
                                                  std::string_view item_name = {});

// Le mot du serveur, une fois sa réponse reçue.
[[nodiscard]] std::string GradeEnchantResultText(ro::sim::GradeEnchantResult result);

// La lettre d'un grade : « D », « C »… ; « aucun » pour zéro.
[[nodiscard]] std::string GradeLetter(std::uint8_t grade);

// D'où viennent les noms et les icônes.
struct GradeEnchantLooks {
  std::function<std::string(const ro::sim::InventoryItem&)> name_of_held;
  std::function<std::string(std::uint32_t)> name_of_id;
  std::function<ro::ui::SkinImage(std::uint32_t)> icon_of;
};

struct GradeEnchantScene {
  const ro::sim::GradeEnchant& grade;
  const ro::sim::Inventory& inventory;
  const GradeEnchantLooks& looks;
  // La mise et les doses que la fenêtre règle.
  ro::sim::GradeEnchantAttempt attempt;
  // L'argent du joueur.
  std::int64_t zeny = 0;
  // Le dernier mot du serveur, et s'il dit une réussite.
  std::string_view notice;
  bool notice_good = false;
};

// Compose la vue, et garde ce vers quoi elle pointe jusqu'à la prochaine.
class GradeEnchantViewComposer {
 public:
  [[nodiscard]] const ro::ui::GradeEnchantView& Compose(const GradeEnchantScene& scene);

 private:
  [[nodiscard]] ro::ui::TradeItemView HeldOf(const GradeEnchantScene& scene,
                                             const ro::sim::InventoryItem& item);
  [[nodiscard]] ro::ui::TradeItemView CatalogOf(const GradeEnchantScene& scene,
                                                std::uint32_t item_id);
  void ComposeChoice(const GradeEnchantScene& scene);
  void ComposeTexts(const GradeEnchantScene& scene);

  // ⚠ LES VUES POINTENT DANS CES TEXTES : ils se réservent avant qu'on en prenne un.
  std::vector<std::string> texts_;
  std::vector<ro::ui::GradeEnchantPieceView> candidates_;
  std::vector<ro::ui::GradeEnchantStakeView> stakes_;
  std::vector<std::string_view> blockers_;
  ro::ui::GradeEnchantView view_;
};

}  // namespace moonclient
