// Les commerces dans la boucle. Voir l'en-tête.

#include "game/trade_stages.h"

#include <string>
#include <utility>

#include "ro/core/log.h"
#include "ro/net/barter.h"
#include "ro/net/buying_store.h"
#include "ro/net/npc_shop.h"
#include "ro/net/store_search.h"
#include "ro/net/vending.h"
#include "ro/sim/buying_store.h"
#include "ro/sim/vending.h"
#include "ro/sim/world.h"
#include "ro/vfs/trade_words.h"

namespace moonclient {

void TradeStages::See(const ro::vfs::ItemTable* items, ro::render::ItemIcons* icons,
                      const ro::vfs::ItemWords* words) noexcept {
  items_.See(items, icons, words);
}

void TradeStages::StartSession() {
  npc_shop_.StartSession();
  barter_.StartSession();
  buying_store_.StartSession();
  vending_.StartSession();
  deal_.StartSession();
  store_search_.StartSession();
  signboards_.clear();
  pending_visit_.reset();
  last_visit_ms_.reset();
  close_visits_ = false;
  visits_closed_ = false;
}

ro::net::FrameOutcome TradeStages::ApplyFrame(const ro::net::Frame& frame,
                                              ro::sim::Inventory& bag) {
  // L'ÉCHANGE ENTRE JOUEURS : ses opcodes ne sont qu'à lui ; il lit sans les
  // prendre les changements de carte.
  if (const ro::net::FrameOutcome dealt = deal_.ApplyFrame(frame, bag);
      dealt != ro::net::FrameOutcome::kUnhandled) {
    return dealt;
  }
  // LA RECHERCHE D'ÉCHOPPES : ses opcodes ne sont qu'à elle ; elle lit sans les
  // prendre les changements de carte.
  if (const ro::net::FrameOutcome searched =
          ro::net::ApplyStoreSearchFrame(frame, store_search_.search());
      searched != ro::net::FrameOutcome::kUnhandled) {
    return searched;
  }
  // L'ÉCHOPPE D'ACHAT : ses opcodes ne sont qu'à elle, et les changements de
  // carte, qu'elle lit aussi, ressortent `kUnhandled` pour les suivants. ⚠ LE
  // SAC PERD CE QUI EST VENDU PLUS TÔT, avec le monde (`RelayFrame`).
  if (const ro::net::FrameOutcome bought =
          ro::net::ApplyBuyingStoreFrame(frame, buying_store_.store());
      bought != ro::net::FrameOutcome::kUnhandled) {
    return bought;
  }
  // L'ÉCHOPPE DE VENTE : ses opcodes aussi ne sont qu'à elle. Elle lit sans les
  // prendre la disparition d'une enseigne et les changements de carte.
  if (const ro::net::FrameOutcome vended = ro::net::ApplyVendingFrame(frame, vending_.vending());
      vended != ro::net::FrameOutcome::kUnhandled) {
    return vended;
  }
  // LE TROC D'ABORD : il ne prend le résultat partagé que s'il l'attend.
  if (const ro::net::FrameOutcome bartered = ro::net::ApplyBarterFrame(frame, barter_.barter());
      bartered != ro::net::FrameOutcome::kUnhandled) {
    return bartered;
  }
  return ro::net::ApplyNpcShopFrame(frame, npc_shop_.shop());
}

void TradeStages::Advance(TradeWindows& windows, ro::net::Connection& link, ro::sim::World& world,
                          const ro::vfs::ItemWords& words, const CreatorNames& creators,
                          bool sell_locked, bool in_chat_room, std::uint32_t now_ms) {
  const TradeContext context{.link = link,
                             .world = world,
                             .words = words,
                             .creators = creators,
                             .items = items_,
                             .sell_locked = sell_locked,
                             .now_ms = now_ms,
                             .commands = commands_,
                             .item_properties = item_properties_,
                             .banned = banned_};
  if (in_chat_room) {
    RefuseStoresInChatRoom(windows, context);
  }
  // LE CLIC DROIT SUR UNE ENSEIGNE : les fenêtres se ferment comme par leur
  // croix, et chaque étage le voit à son tour juste en dessous.
  if (std::exchange(close_visits_, false)) {
    windows.vending.visit.open = false;
    windows.vending.visit.basket.clear();
    windows.buying_store.sell.state.open = false;
    visits_closed_ = true;
  }
  npc_shop_.Advance(windows.npc_shop.state, std::exchange(windows.npc_shop.answer, {}), context);
  windows.npc_shop.view = npc_shop_.view();
  barter_.Advance(windows.barter.state, std::exchange(windows.barter.answer, {}), context);
  windows.barter.view = barter_.view();
  buying_store_.Advance(windows.buying_store, context);
  vending_.Advance(windows.vending, context);
  deal_.Advance(windows.deal, context);
  store_search_.Advance(windows.store_search, context);
  // LES ENSEIGNES PEINTES, pour la bulle d'un titre coupé.
  windows.signboard_tips.clear();
  for (const SignboardHit& board : signboards_) {
    windows.signboard_tips.push_back(ro::ui::SignboardTipView{
        .board_min = ImVec2(board.rect.min.x, board.rect.min.y),
        .board_max = ImVec2(board.rect.max.x, board.rect.max.y),
        .title_min = ImVec2(board.title_rect.min.x, board.title_rect.min.y),
        .title_max = ImVec2(board.title_rect.max.x, board.title_rect.max.y),
        .cut_title = board.cut_title,
    });
  }
  // LA VISITE DEMANDÉE PAR UN CLIC : la liste d'un vendeur, ou ce qu'un
  // acheteur veut. C'est la réponse qui ouvre la fenêtre.
  //
  // ⚠ ELLE NE PART QUE SI RIEN NE S'Y OPPOSE : voir `StoreVisitMayLeave`. Une
  // demande refusée est perdue, pas remise à plus tard.
  //
  // ⚠ L'INSTANT SE RETIENT AVANT L'ENVOI, même si celui-ci échoue : c'est la
  // demande, et non sa réussite, qui ouvre l'écart.
  const ro::sim::Actor* player = world.player();
  if (const std::optional<SignboardHit> asked = std::exchange(pending_visit_, std::nullopt);
      asked.has_value() &&
      StoreVisitMayLeave(StoreVisitGate{
          .owner_id = asked->owner_id,
          .player_id = world.player_id(),
          .player_dead = player != nullptr && player->posture == ro::sim::Posture::kDead,
          .visiting = windows.vending.visit.open || windows.buying_store.sell.state.open,
          .last_sent_ms = last_visit_ms_,
          .now_ms = now_ms})) {
    last_visit_ms_ = now_ms;
    const ro::core::Result<void> sent = asked->kind == ro::sim::SignboardKind::kBuyingStore
                                            ? ro::net::RequestBuyingStoreList(link, asked->owner_id)
                                            : ro::net::RequestVendingList(link, asked->owner_id);
    if (!sent) {
      ro::core::LogError("échoppe : {}", ro::core::Describe(sent.error()));
    }
  }
}

void TradeStages::RefuseStoresInChatRoom(TradeWindows& windows, const TradeContext& context) {
  ro::sim::Vending& vending = vending_.vending();
  ro::sim::BuyingStore& buying = buying_store_.store();
  // ⚠ SEULEMENT UNE PRÉPARATION QUE SA FENÊTRE N'A PAS ENCORE MONTRÉE : celle
  // qu'on a déjà ouverte avant d'entrer au salon n'est pas une arrivée.
  const bool vending_arrives =
      vending.preparing() && vending.preparation_seq() != windows.vending.setup.preparation_seen;
  const bool buying_arrives =
      buying.phase() == ro::sim::BuyingStorePhase::kPreparing &&
      buying.preparation_seq() != windows.buying_store.setup.state.preparation_seen;
  if (!vending_arrives && !buying_arrives) {
    return;
  }
  // L'abandon de la vente part avec l'adieu que l'étage dit à son tour.
  if (vending_arrives) {
    vending.CancelPreparation();
  }
  if (buying_arrives) {
    if (auto said = ro::net::RequestBuyingStoreCancel(context.link, buying); !said) {
      ro::core::LogError("échoppe d'achat : {}", ro::core::Describe(said.error()));
    }
  }
  windows.vending.notice.text =
      std::string(ro::vfs::TradeText(context.words, ro::vfs::TradeWord::kNoChatWhileVending));
  windows.vending.notice.asked = false;
}

bool TradeStages::PreparingStore() const noexcept {
  const ro::sim::BuyingStorePhase buying = buying_store_.store().phase();
  return vending_.vending().preparing() || buying == ro::sim::BuyingStorePhase::kPreparing ||
         buying == ro::sim::BuyingStorePhase::kRequested;
}

std::optional<SignboardHit> TradeStages::SignboardAt(glm::vec2 point_px) const {
  return StoreSignboardAt(signboards_, point_px);
}

void TradeStages::VisitStore(std::uint32_t owner_id, ro::sim::SignboardKind kind) noexcept {
  if (owner_id != 0 && SignboardOpensStore(kind)) {
    pending_visit_ = SignboardHit{.owner_id = owner_id, .kind = kind};
  }
}

bool TradeStages::HoldsWorld() const noexcept {
  return npc_shop_.shop().open() || barter_.barter().open() || buying_store_.HoldsWorld() ||
         vending_.HoldsWorld() || deal_.HoldsWorld();
}

TradeItemFocus TradeStages::hovered() const noexcept {
  if (deal_.hovered().item_id != 0) {
    return deal_.hovered();
  }
  if (vending_.hovered().item_id != 0) {
    return vending_.hovered();
  }
  if (store_search_.hovered().item_id != 0) {
    return store_search_.hovered();
  }
  if (buying_store_.hovered().item_id != 0) {
    return buying_store_.hovered();
  }
  if (barter_.hovered().item_id != 0) {
    return barter_.hovered();
  }
  return npc_shop_.hovered();
}

std::optional<TradeItemFocus> TradeStages::TakeDescribed() noexcept {
  if (std::optional<TradeItemFocus> dealt = deal_.TakeDescribed(); dealt.has_value()) {
    return dealt;
  }
  if (std::optional<TradeItemFocus> vended = vending_.TakeDescribed(); vended.has_value()) {
    return vended;
  }
  if (std::optional<TradeItemFocus> searched = store_search_.TakeDescribed();
      searched.has_value()) {
    return searched;
  }
  if (std::optional<TradeItemFocus> bought = buying_store_.TakeDescribed(); bought.has_value()) {
    return bought;
  }
  if (std::optional<TradeItemFocus> bartered = barter_.TakeDescribed(); bartered.has_value()) {
    return bartered;
  }
  return npc_shop_.TakeDescribed();
}

}  // namespace moonclient
