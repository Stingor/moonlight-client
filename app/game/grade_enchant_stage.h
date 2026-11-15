#pragma once

// L'AMÉLIORATION DE GRADE, DANS LA BOUCLE DE LA PARTIE : la session que le serveur
// ouvre et que le client doit fermer, ce que la fenêtre en montre, et ce que ses
// gestes deviennent sur le fil.

#include <cstdint>
#include <optional>
#include <string>

#include "ro/sim/grade_enchant.h"
#include "ro/ui/grade_enchant_window.h"

#include "game/grade_enchant_view.h"
#include "game/npc_shop_stage.h"
#include "game/trade_items.h"

namespace moonclient {

class GradeEnchantStage {
 public:
  [[nodiscard]] ro::sim::GradeEnchant& grade() noexcept { return grade_; }
  [[nodiscard]] const ro::sim::GradeEnchant& grade() const noexcept { return grade_; }

  // Une partie commence : rien de la précédente ne survit.
  void StartSession();

  // L'IMAGE : la réponse de la fenêtre à l'image précédente, la demande à émettre,
  // la proposition à demander, la fermeture due, l'ouverture qui arrive, le
  // résultat à dire, puis la vue de la prochaine.
  void Advance(ro::ui::GradeEnchantWindowState& state,
               const ro::ui::GradeEnchantWindowAction& answer, const TradeContext& context);

  // La vue que la fenêtre dessine, nulle quand elle est fermée.
  [[nodiscard]] const ro::ui::GradeEnchantView* view() const noexcept {
    return grade_.open() ? view_ : nullptr;
  }

  [[nodiscard]] TradeItemFocus hovered() const noexcept { return hovered_; }
  [[nodiscard]] std::optional<TradeItemFocus> TakeDescribed() noexcept;

 private:
  void Follow(ro::ui::GradeEnchantWindowState& state,
              const ro::ui::GradeEnchantWindowAction& answer, const TradeContext& context);
  void Enchant(const ro::ui::GradeEnchantWindowState& state,
               const ro::ui::GradeEnchantWindowAction& answer, const TradeContext& context);
  void ReadResult();

  ro::sim::GradeEnchant grade_;
  // Le rang du dernier résultat déjà lu, et le dernier mot montré.
  std::uint32_t result_read_ = 0;
  std::string notice_;
  bool notice_good_ = false;
  TradeItemFocus hovered_;
  std::optional<TradeItemFocus> described_;
  GradeEnchantViewComposer composer_;
  const ro::ui::GradeEnchantView* view_ = nullptr;
};

}  // namespace moonclient
