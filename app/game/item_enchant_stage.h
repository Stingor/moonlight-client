#pragma once

// L'ENCHANTEMENT, DANS LA BOUCLE DE LA PARTIE : la séance que le serveur ouvre et que
// le client doit fermer, ce que la fenêtre en montre, et ce que ses gestes
// deviennent sur le fil.

#include <cstdint>
#include <optional>
#include <string>

#include "ro/sim/item_enchant.h"
#include "ro/ui/item_enchant_window.h"

#include "game/item_enchant_view.h"
#include "game/npc_shop_stage.h"
#include "game/trade_items.h"

namespace moonclient {

class ItemEnchantStage {
 public:
  [[nodiscard]] ro::sim::ItemEnchant& enchant() noexcept { return enchant_; }
  [[nodiscard]] const ro::sim::ItemEnchant& enchant() const noexcept { return enchant_; }

  // Une partie commence : rien de la précédente ne survit.
  void StartSession();

  // L'IMAGE : la réponse de la fenêtre à l'image précédente, la demande à émettre,
  // le résultat à dire, la fermeture due, l'ouverture qui arrive, puis la vue de la
  // prochaine.
  void Advance(ro::ui::ItemEnchantWindowState& state, const ro::ui::ItemEnchantWindowAction& answer,
               const TradeContext& context);

  // La vue que la fenêtre dessine, nulle quand elle est fermée.
  [[nodiscard]] const ro::ui::ItemEnchantView* view() const noexcept { return view_; }

  // La séance est ouverte, ou la fenêtre montre encore ce qu'elle a donné.
  [[nodiscard]] bool showing() const noexcept { return enchant_.open() || concluded_; }

  [[nodiscard]] TradeItemFocus hovered() const noexcept { return hovered_; }
  [[nodiscard]] std::optional<TradeItemFocus> TakeDescribed() noexcept;

 private:
  [[nodiscard]] const ro::sim::ItemEnchantGroup* GroupOf(const TradeContext& context) const;
  void Follow(ro::ui::ItemEnchantWindowState& state, const ro::ui::ItemEnchantWindowAction& answer,
              const TradeContext& context);
  void Enchant(const ro::ui::ItemEnchantWindowState& state,
               const ro::ui::ItemEnchantWindowAction& answer, const TradeContext& context);
  void ReadResult(const TradeContext& context);

  ro::sim::ItemEnchant enchant_;
  // Le groupe de la séance close, que la fenêtre montre encore.
  std::uint64_t shown_group_ = 0;
  // Le rang du dernier résultat déjà lu, et le dernier mot montré.
  std::uint32_t result_read_ = 0;
  std::string notice_;
  bool notice_good_ = false;
  // 🔴 LA RÉPONSE CLÔT LA SÉANCE, PAS LA FENÊTRE : elle reste pour dire ce qui est
  // arrivé, tout retenu, jusqu'à ce que le joueur la ferme ou que le PNJ la rouvre.
  bool concluded_ = false;
  TradeItemFocus hovered_;
  std::optional<TradeItemFocus> described_;
  ItemEnchantViewComposer composer_;
  const ro::ui::ItemEnchantView* view_ = nullptr;
};

}  // namespace moonclient
