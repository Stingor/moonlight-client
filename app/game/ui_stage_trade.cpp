// LES FENÊTRES DES COMMERCES, dans la trame de l'interface : leur dessin, et
// leur place dans la pile d'Échap. À part de `ui_stage.cpp`, qui n'a plus la
// place de les porter toutes.

#include <string>

#include "ro/core/i18n.h"

#include "game/ui_stage.h"

namespace moonclient {

void UiStage::DrawTradeWindows(TradeWindows& trade,
                               std::span<const ro::ui::VoteShopOffer> vote_shop) {
  // LE TITRE ENTIER D'UNE ENSEIGNE COUPÉE, sous la souris.
  ro::ui::DrawSignboardTip(trade.signboard_tips);
  // LA BOUTIQUE D'UN PNJ, avant la description qu'on ouvre depuis ses lignes.
  if (trade.npc_shop.view != nullptr) {
    trade.npc_shop.answer =
        ro::ui::DrawNpcShopWindow(trade.npc_shop.state, *trade.npc_shop.view, skin_, vote_shop);
  }
  // LE TROC, au même rang : c'est une boutique de PNJ qui se paie en items.
  if (trade.barter.view != nullptr) {
    trade.barter.answer =
        ro::ui::DrawBarterWindow(trade.barter.state, *trade.barter.view, skin_, vote_shop);
  }
  // L'ÉCHOPPE D'ACHAT : ses quatre fenêtres, chacune seulement quand l'étage
  // lui a composé une vue.
  BuyingStoreWindows& buying = trade.buying_store;
  if (buying.setup.view != nullptr) {
    buying.setup.answer = ro::ui::DrawBuyingStoreSetupWindow(buying.setup.state, *buying.setup.view,
                                                             skin_, vote_shop);
  }
  if (buying.own.view != nullptr) {
    buying.own.answer =
        ro::ui::DrawBuyingStoreOwnWindow(buying.own.state, *buying.own.view, skin_, vote_shop);
  }
  if (buying.log.view != nullptr) {
    buying.log.answer =
        ro::ui::DrawBuyingStoreLogWindow(buying.log.state, *buying.log.view, skin_, vote_shop);
  }
  if (buying.sell.view != nullptr) {
    buying.sell.answer =
        ro::ui::DrawBuyingStoreSellWindow(buying.sell.state, *buying.sell.view, skin_, vote_shop);
  }
  // L'ÉCHOPPE DE VENTE : ses quatre fenêtres, chacune seulement quand l'étage
  // lui a composé une vue, puis la boîte d'un avis du serveur.
  VendingWindows& vending = trade.vending;
  if (vending.setup_view != nullptr) {
    vending.setup_answer =
        ro::ui::DrawVendingSetupWindow(vending.setup, *vending.setup_view, skin_, vote_shop);
  }
  if (vending.shop_view != nullptr) {
    vending.shop_answer =
        ro::ui::DrawVendingShopWindow(vending.shop, *vending.shop_view, skin_, vote_shop);
  }
  if (vending.log_view != nullptr) {
    vending.log_answer =
        ro::ui::DrawVendingLogWindow(vending.log, *vending.log_view, skin_, vote_shop);
  }
  if (vending.visit_view != nullptr) {
    vending.visit_answer =
        ro::ui::DrawVendingVisitWindow(vending.visit, *vending.visit_view, skin_, vote_shop);
  }
  const std::string notice_id = std::string(ro::core::Tr("Shop")) + "###vending_notice";
  ro::ui::DrawTradeNotice(vending.notice, notice_id.c_str(), skin_);
  // LA RECHERCHE D'ÉCHOPPES, puis la boîte de ses refus.
  StoreSearchWindows& search = trade.store_search;
  if (search.view != nullptr) {
    search.answer = ro::ui::DrawStoreSearchWindow(search.state, *search.view, skin_, vote_shop);
  }
  const std::string search_notice_id = std::string(ro::core::Tr("Shop")) + "###store_search_notice";
  ro::ui::DrawTradeNotice(search.notice, search_notice_id.c_str(), skin_);
  // L'ÉCHANGE ENTRE JOUEURS : la proposition, puis la fenêtre des deux offres.
  DealWindows& deal = trade.deal;
  if (deal.proposal_view != nullptr) {
    deal.proposal_answer = ro::ui::DrawDealProposal(deal.proposal, *deal.proposal_view, skin_);
  }
  if (deal.view != nullptr) {
    deal.answer = ro::ui::DrawDealWindow(deal.state, *deal.view, skin_, vote_shop);
  }
}

void UiStage::RegisterTradeEscapes(TradeWindows& trade) {
  // LA BOUTIQUE D'UN PNJ EN EST : son interrupteur recopie la session du
  // serveur, mais la boucle lit un interrupteur BAISSÉ comme une fermeture, et
  // la dit au serveur — voir `NpcShopStage::Advance`.
  escape_.Register(ro::ui::kNpcShopWindowId, &trade.npc_shop.state.open);
  // LE TROC AUSSI, et pour la même raison : Échap baisse l'interrupteur, et
  // `BarterStage::Advance` dit la fermeture au serveur.
  escape_.Register(ro::ui::kBarterWindowId, &trade.barter.state.open);
  // L'ÉCHOPPE D'ACHAT. La préparation, l'historique et la vente se ferment sur
  // Échap ; MON ÉCHOPPE, elle, relève son interrupteur et demande confirmation
  // — voir `DrawBuyingStoreOwnWindow`. ⚠ LE NOM INSCRIT EST L'IDENTITÉ « ### »
  // de la fenêtre : c'est elle qu'ImGui retrouve.
  BuyingStoreWindows& buying = trade.buying_store;
  if (buying.setup.view != nullptr) {
    escape_.Register(ro::ui::kBuyingStoreSetupWindowName, &buying.setup.state.open);
  }
  if (buying.own.view != nullptr) {
    escape_.Register(ro::ui::kBuyingStoreOwnWindowName, &buying.own.state.open);
  }
  if (buying.log.view != nullptr) {
    escape_.Register(ro::ui::kBuyingStoreLogWindowName, &buying.log.state.open);
  }
  if (buying.sell.view != nullptr) {
    escape_.Register(ro::ui::kBuyingStoreSellWindowName, &buying.sell.state.open);
  }
  // L'ÉCHOPPE DE VENTE, même règle : la préparation baissée est un abandon que
  // `VendingStage` dit au serveur ; MON ÉCHOPPE relève son interrupteur et
  // demande confirmation — voir `DrawVendingShopWindow`.
  VendingWindows& vending = trade.vending;
  if (vending.setup_view != nullptr) {
    escape_.Register(ro::ui::kVendingSetupWindowId, &vending.setup.open);
  }
  if (vending.shop_view != nullptr) {
    escape_.Register(ro::ui::kVendingShopWindowId, &vending.shop.open);
  }
  if (vending.log_view != nullptr) {
    escape_.Register(ro::ui::kVendingLogWindowId, &vending.log.open);
  }
  if (vending.visit_view != nullptr) {
    escape_.Register(ro::ui::kVendingVisitWindowId, &vending.visit.open);
  }
  // LA RECHERCHE D'ÉCHOPPES : Échap baisse l'interrupteur, et
  // `StoreSearchStage` dit la fermeture au serveur.
  if (trade.store_search.view != nullptr) {
    escape_.Register(ro::ui::kStoreSearchWindowId, &trade.store_search.state.open);
  }
  // L'ÉCHANGE : Échap baisse l'interrupteur, et `DealStage` lit une fenêtre
  // baissée comme une annulation, une proposition baissée comme un refus.
  DealWindows& deal = trade.deal;
  if (deal.proposal_view != nullptr) {
    escape_.Register(ro::ui::kDealProposalWindowId, &deal.proposal.open);
  }
  if (deal.view != nullptr) {
    escape_.Register(ro::ui::kDealWindowId, &deal.state.open);
  }
}

}  // namespace moonclient
