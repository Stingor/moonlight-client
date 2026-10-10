#pragma once

// LE TROC, DANS LA BOUCLE DE LA PARTIE : la session que le serveur ouvre et que
// le client doit fermer, ce que la fenêtre en montre, et ce que ses gestes
// deviennent sur le fil.

#include <cstdint>
#include <optional>

#include "ro/sim/barter.h"
#include "ro/ui/barter_window.h"

#include "game/barter_view.h"
#include "game/npc_shop_stage.h"
#include "game/trade_items.h"

namespace moonclient {

class BarterStage {
 public:
  [[nodiscard]] ro::sim::Barter& barter() noexcept { return barter_; }
  [[nodiscard]] const ro::sim::Barter& barter() const noexcept { return barter_; }

  // Une partie commence : rien de la précédente ne survit.
  void StartSession();

  // L'IMAGE : la réponse de la fenêtre à l'image précédente, l'échange à
  // émettre, les fermetures dues au serveur, l'ouverture qui arrive, le résultat
  // à dire, puis la vue de la prochaine.
  void Advance(ro::ui::BarterWindowState& state, const ro::ui::BarterWindowAction& answer,
               const TradeContext& context);

  // La vue que la fenêtre dessine, nulle quand le troc est fermé.
  [[nodiscard]] const ro::ui::BarterView* view() const noexcept {
    return barter_.open() ? view_ : nullptr;
  }

  // CE QUE LA SOURIS SURVOLAIT, et ce que le clic droit veut décrire — une fois.
  [[nodiscard]] TradeItemFocus hovered() const noexcept { return hovered_; }
  [[nodiscard]] std::optional<TradeItemFocus> TakeDescribed() noexcept;

 private:
  void Follow(ro::ui::BarterWindowState& state, const ro::ui::BarterWindowAction& answer,
              const TradeContext& context);
  void Edit(const ro::ui::BarterCartEdit& edit, const TradeContext& context);
  void Exchange(const ro::ui::BarterWindowAction& answer, const TradeContext& context);
  void SayFarewells(const TradeContext& context);
  void SayResult(const TradeContext& context);
  void Refuse(const ro::sim::BarterCartCheck& check, const TradeContext& context);

  ro::sim::Barter barter_;
  // Le rang du dernier résultat déjà dit dans la conversation.
  std::uint32_t result_said_ = 0;
  TradeItemFocus hovered_;
  std::optional<TradeItemFocus> described_;
  BarterViewComposer composer_;
  const ro::ui::BarterView* view_ = nullptr;
};

}  // namespace moonclient
