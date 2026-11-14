#pragma once

// LA RÉFORME, DANS LA BOUCLE DE LA PARTIE : la session que le serveur ouvre et que
// le client doit fermer, ce que la fenêtre en montre, et ce que ses gestes
// deviennent sur le fil.

#include <cstdint>
#include <optional>
#include <string>

#include "ro/sim/item_reform.h"
#include "ro/ui/item_reform_window.h"

#include "game/item_reform_view.h"
#include "game/npc_shop_stage.h"
#include "game/trade_items.h"

namespace moonclient {

class ItemReformStage {
 public:
  [[nodiscard]] ro::sim::ItemReform& reform() noexcept { return reform_; }
  [[nodiscard]] const ro::sim::ItemReform& reform() const noexcept { return reform_; }

  // Une partie commence : rien de la précédente ne survit.
  void StartSession();

  // L'IMAGE : la réponse de la fenêtre à l'image précédente, la demande à émettre,
  // la fermeture due au serveur, l'ouverture qui arrive, le résultat à dire, puis la
  // vue de la prochaine.
  void Advance(ro::ui::ItemReformWindowState& state, const ro::ui::ItemReformWindowAction& answer,
               const TradeContext& context);

  // La vue que la fenêtre dessine, nulle quand elle est fermée.
  [[nodiscard]] const ro::ui::ItemReformView* view() const noexcept {
    return reform_.open() ? view_ : nullptr;
  }

  [[nodiscard]] TradeItemFocus hovered() const noexcept { return hovered_; }
  [[nodiscard]] std::optional<TradeItemFocus> TakeDescribed() noexcept;

 private:
  void Follow(ro::ui::ItemReformWindowState& state, const ro::ui::ItemReformWindowAction& answer,
              const TradeContext& context);
  void Reform(const ro::ui::ItemReformWindowAction& answer, const TradeContext& context);
  void ReadResult(const TradeContext& context);
  [[nodiscard]] const ro::sim::ItemReformTrigger* TriggerOf(const TradeContext& context) const;

  ro::sim::ItemReform reform_;
  // Le rang du dernier résultat déjà lu, et le dernier refus montré.
  std::uint32_t result_read_ = 0;
  std::string notice_;
  TradeItemFocus hovered_;
  std::optional<TradeItemFocus> described_;
  ItemReformViewComposer composer_;
  const ro::ui::ItemReformView* view_ = nullptr;
};

}  // namespace moonclient
