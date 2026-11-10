#pragma once

// LE LAPHINE, DANS LA BOUCLE DE LA PARTIE : la session que le serveur ouvre et que
// le client doit fermer, ce que la fenêtre en montre, et ce que ses gestes
// deviennent sur le fil.

#include <cstdint>
#include <optional>
#include <string>

#include "ro/sim/laphine.h"
#include "ro/ui/laphine_window.h"

#include "game/laphine_view.h"
#include "game/npc_shop_stage.h"
#include "game/trade_items.h"

namespace moonclient {

class LaphineStage {
 public:
  [[nodiscard]] ro::sim::Laphine& laphine() noexcept { return laphine_; }
  [[nodiscard]] const ro::sim::Laphine& laphine() const noexcept { return laphine_; }

  // Une partie commence : rien de la précédente ne survit.
  void StartSession();

  // L'IMAGE : la réponse de la fenêtre à l'image précédente, la demande à émettre,
  // les fermetures dues au serveur, l'ouverture qui arrive, le résultat à dire, puis
  // la vue de la prochaine.
  void Advance(ro::ui::LaphineWindowState& state, const ro::ui::LaphineWindowAction& answer,
               const TradeContext& context);

  // La vue que la fenêtre dessine, nulle quand elle est fermée.
  [[nodiscard]] const ro::ui::LaphineView* view() const noexcept {
    return laphine_.open() ? view_ : nullptr;
  }

  [[nodiscard]] TradeItemFocus hovered() const noexcept { return hovered_; }
  [[nodiscard]] std::optional<TradeItemFocus> TakeDescribed() noexcept;

  // LE JOUEUR VA UTILISER `item_id`. Si c'est un déclencheur que `book` connaît
  // et qu'un Laphine est ouvert, la fenêtre se ferme et sa fermeture part
  // aussitôt, devant l'usage : le serveur ouvre alors celle du nouvel objet. Faux
  // quand l'usage doit se retenir, une demande attendant sa réponse.
  [[nodiscard]] bool YieldItemUse(std::uint32_t item_id, const LaphineBook* book,
                                  ro::net::Connection& link);

 private:
  void Follow(ro::ui::LaphineWindowState& state, const ro::ui::LaphineWindowAction& answer,
              const TradeContext& context);
  void Make(const ro::ui::LaphineWindowAction& answer, const TradeContext& context);
  void ReadResult(const TradeContext& context);
  [[nodiscard]] const ro::sim::LaphineRecipe* RecipeOf(const TradeContext& context) const;

  ro::sim::Laphine laphine_;
  // Le rang du dernier résultat déjà lu, et le dernier refus montré.
  std::uint32_t result_read_ = 0;
  std::string notice_;
  TradeItemFocus hovered_;
  std::optional<TradeItemFocus> described_;
  LaphineViewComposer composer_;
  const ro::ui::LaphineView* view_ = nullptr;
};

}  // namespace moonclient
