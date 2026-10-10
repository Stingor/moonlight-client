#pragma once

// LA BOUTIQUE D'UN PNJ, DANS LA BOUCLE DE LA PARTIE : la session que le serveur
// ouvre et que le client doit fermer, ce que la fenêtre en montre, et ce que ses
// gestes deviennent sur le fil.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/npc_shop.h"
#include "ro/ui/npc_shop_window.h"
#include "ro/vfs/item_link.h"

#include "game/trade_items.h"

namespace moonclient {

// L'OBJET QUE LA FENÊTRE VEUT VOIR DÉCRIT OU SURVOLE : un identifiant, et la
// case du sac quand l'objet est à nous — c'est elle qui dit raffinement et
// cartes.
struct TradeItemFocus {
  std::uint32_t item_id = 0;
  std::uint16_t bag_slot = 0;
  // UN OBJET QUI N'EST PAS DANS LE SAC — l'offre d'un autre joueur, une pile du
  // cart — se décrit par ce qu'il porte : raffinement, cartes, options voyagent
  // ici, comme dans un lien de conversation. Absent : l'identifiant et la case
  // du sac suffisent.
  std::optional<ro::vfs::ItemLink> link;
  // CE QUE LE GESTE DEMANDE de l'objet visé : sa description, son lien dans la
  // saisie, ou une autre entrée du menu des objets. Sans objet pour un survol.
  ro::ui::ChatLinkGesture gesture = ro::ui::ChatLinkGesture::kDescribe;
};

class NpcShopStage {
 public:
  [[nodiscard]] ro::sim::NpcShop& shop() noexcept { return shop_; }
  [[nodiscard]] const ro::sim::NpcShop& shop() const noexcept { return shop_; }

  // Une partie commence : rien de la précédente ne survit.
  void StartSession();

  // L'IMAGE : la réponse de la fenêtre à l'image précédente, la fermeture due
  // au serveur, l'ouverture qui arrive, le résultat à dire, puis la vue de la
  // prochaine.
  void Advance(ro::ui::NpcShopWindowState& state, const ro::ui::NpcShopWindowAction& answer,
               const TradeContext& context);

  // La vue que la fenêtre dessine, nulle quand la boutique est fermée.
  [[nodiscard]] const ro::ui::NpcShopView* view() const noexcept {
    return shop_.open() ? &view_ : nullptr;
  }

  // CE QUE LA SOURIS SURVOLAIT, et ce que le clic droit veut décrire — une fois.
  [[nodiscard]] TradeItemFocus hovered() const noexcept { return hovered_; }
  [[nodiscard]] std::optional<TradeItemFocus> TakeDescribed() noexcept;

 private:
  void Follow(ro::ui::NpcShopWindowState& state, const ro::ui::NpcShopWindowAction& answer,
              const TradeContext& context);
  void SayOutcome(ro::ui::NpcShopWindowState& state, const TradeContext& context);
  void Compose(const ro::ui::NpcShopWindowState& state, const TradeContext& context);
  void ComposeVerdict(const ro::ui::NpcShopWindowState& state, const TradeContext& context);
  void AskList(ro::sim::NpcShopSide side, const TradeContext& context);

  ro::sim::NpcShop shop_;
  // Le rang du dernier résultat déjà dit dans la conversation.
  std::uint32_t outcome_said_ = 0;
  // La liste demandée pour cette ouverture, pour ne pas la redemander à chaque
  // image : le rang d'ouverture, et le côté.
  std::uint32_t asked_opening_ = 0;
  ro::sim::NpcShopSide asked_side_ = ro::sim::NpcShopSide::kBuy;
  // LA DERNIÈRE TRANSACTION PARTIE VENAIT-ELLE DU PANIER ? Un achat immédiat
  // (Ctrl + clic) ne le vide pas au succès.
  bool sent_from_cart_ = false;
  bool close_after_sale_ = false;
  TradeItemFocus hovered_;
  std::optional<TradeItemFocus> described_;

  // ── CE QUI SE COMPOSE À CHAQUE IMAGE ──────────────────────────────────────
  // ⚠ LES VUES POINTENT DEDANS : elles se prennent une fois les vecteurs remplis.
  std::vector<std::string> offer_names_;
  std::vector<std::string> sell_names_;
  std::vector<ro::ui::NpcShopOfferView> offers_;
  std::vector<ro::ui::NpcShopSellView> sells_;
  std::vector<std::string> blocker_texts_;
  std::vector<std::string_view> blockers_;
  std::string title_;
  std::string purse_line_;
  std::string unit_;
  std::string outcome_text_;
  std::string cart_weight_;
  ro::ui::NpcShopView view_;
};

}  // namespace moonclient
