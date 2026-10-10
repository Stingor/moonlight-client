// Mon échoppe d'achat, dans la boucle. Voir l'en-tête ; la vente à un acheteur
// est dans `buying_store_visit.cpp`.

#include "game/buying_store_stage.h"

#include <algorithm>
#include <array>
#include <cstdio>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"
#include "ro/core/text.h"
#include "ro/net/buying_store.h"
#include "ro/proto/buying_store_packets.h"
#include "ro/sim/buying_store_checks.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"
#include "ro/vfs/trade_words.h"

#include "game/buying_store_rules.h"

namespace moonclient {
namespace {

using ro::sim::BuyingStorePhase;
using ro::vfs::TradeWord;

// Ce que le serveur garde du nom : le champ, moins son terminateur.
constexpr int kTitleBytesKept = static_cast<int>(ro::proto::BuyingStoreOpenRequest::kTitleSize) - 1;

// Les poids se comptent en dixièmes, comme le serveur les tient.
constexpr std::uint32_t kWeightTenths = 10;

std::vector<ro::sim::BuyingStoreWish> WishesOf(
    std::span<const ro::ui::BuyingStoreSetupLine> lines) {
  std::vector<ro::sim::BuyingStoreWish> wishes;
  wishes.reserve(lines.size());
  for (const ro::ui::BuyingStoreSetupLine& line : lines) {
    wishes.push_back(
        ro::sim::BuyingStoreWish{.item_id = line.item_id,
                                 .amount = static_cast<std::uint32_t>(std::max(line.amount, 0)),
                                 .price = static_cast<std::uint32_t>(std::max(line.price, 0))});
  }
  return wishes;
}

// Ce que le joueur possède ; le plafond quand le serveur ne l'a pas dit, pour
// que la limite ne se juge que sur ce qu'on sait.
std::int64_t KnownZeny(const ro::sim::PlayerStatus& status) {
  return status.zeny == ro::sim::kUnknownStat ? ro::sim::kBuyingStoreMostZeny : status.zeny;
}

bool Preparing(const ro::sim::BuyingStore& store) {
  return store.phase() == BuyingStorePhase::kPreparing ||
         store.phase() == BuyingStorePhase::kRequested;
}

}  // namespace

void BuyingStoreStage::StartSession() {
  store_.CloseSilently();
  store_.ClearJournal();
  (void)store_.TakeFarewell();
  open_refusal_said_ = store_.open_refusal_seq();
  end_said_ = store_.end_seq();
  sale_seq_said_ = store_.sale_seq();
  sales_said_ = store_.sales().size();
  sale_refusal_said_ = store_.sale_refusal_seq();
  visit_seen_ = store_.visit_seq();
  was_open_ = false;
  draft_refused_ = false;
  buyer_had_board_ = false;
  sale_sent_ = false;
  refresh_asked_ = false;
  sold_names_.clear();
  sale_outcome_.clear();
  hovered_ = {};
  described_.reset();
}

std::optional<TradeItemFocus> BuyingStoreStage::TakeDescribed() noexcept {
  return std::exchange(described_, std::nullopt);
}

bool BuyingStoreStage::HoldsWorld() const noexcept {
  return store_.phase() != BuyingStorePhase::kClosed || store_.visiting();
}

void BuyingStoreStage::Note(std::uint32_t item_id, std::uint16_t slot, bool hovered,
                            ro::ui::ChatLinkGesture gesture) {
  if (item_id == 0) {
    return;
  }
  const TradeItemFocus focus{.item_id = item_id, .bag_slot = slot, .gesture = gesture};
  if (hovered) {
    hovered_ = focus;
  } else {
    described_ = focus;
  }
}

void BuyingStoreStage::Say(const TradeContext& context, std::string text, std::uint32_t color) {
  if (!text.empty()) {
    context.world.RecordChat(ro::sim::ChatKind::kServer, 0, std::move(text), context.now_ms, color);
  }
}

void BuyingStoreStage::FollowSetup(ro::ui::BuyingStoreSetupState& state,
                                   const ro::ui::BuyingStoreSetupAction& answer,
                                   const TradeContext& context) {
  Note(answer.hovered_id, 0, true);
  Note(answer.describe_id, 0, false, answer.item_gesture);
  // ⚠ SEULEMENT UNE FENÊTRE QUI MONTRAIT DÉJÀ CETTE PRÉPARATION : à l'image où
  // elle arrive, la fenêtre est encore baissée.
  if (!Preparing(store_) || state.preparation_seen != store_.preparation_seq()) {
    return;
  }
  if (answer.cancel || !state.open) {
    draft_refused_ = false;
    if (store_.phase() == BuyingStorePhase::kPreparing) {
      if (auto said = ro::net::RequestBuyingStoreCancel(context.link, store_); !said) {
        ro::core::LogError("échoppe d'achat : {}", ro::core::Describe(said.error()));
      }
    } else {
      // DEMANDÉE, PAS ENCORE OUVERTE : la fermeture défera l'ouverture que le
      // serveur est en train d'accorder.
      store_.Close();
    }
    return;
  }
  if (!answer.open_store || store_.phase() != BuyingStorePhase::kPreparing) {
    return;
  }
  // ⚠ REJUGÉ ICI : la fenêtre a pu dessiner un verdict d'une image de retard.
  const std::string wire = ro::net::BuyingStoreTitleOnWire(state.title.data());
  const std::vector<ro::sim::BuyingStoreWish> wishes = WishesOf(state.lines);
  for (const ro::sim::BuyingStoreDraftProblem& problem : ro::sim::CheckBuyingStoreDraft(
           wire, state.zeny_limit, wishes, store_.slots(), context.world.inventory().items(),
           KnownZeny(context.world.player_status()))) {
    if (ro::sim::BlocksOpening(problem.fault)) {
      Say(context, DraftFaultText(problem.fault), kBuyingStoreFailedColor);
      return;
    }
  }
  if (auto sent = ro::net::RequestBuyingStoreOpen(context.link, store_, state.title.data(),
                                                  state.zeny_limit, wishes);
      !sent) {
    ro::core::LogError("échoppe d'achat : {}", ro::core::Describe(sent.error()));
  }
}

void BuyingStoreStage::FollowOwn(const ro::ui::BuyingStoreOwnState& state,
                                 const ro::ui::BuyingStoreOwnAction& answer) {
  Note(answer.hovered_id, 0, true);
  Note(answer.describe_id, 0, false, answer.item_gesture);
  // LA FERMETURE EST CONFIRMÉE PAR LA FENÊTRE : un interrupteur baissé ne vaut
  // rien ici, seule la réponse compte.
  if (answer.close && store_.open() && state.opening_seen == store_.opening_seq()) {
    store_.Close();
  }
}

void BuyingStoreStage::FollowLog(const ro::ui::BuyingStoreLogState& state,
                                 const ro::ui::BuyingStoreLogAction& answer) {
  Note(answer.hovered_id, 0, true);
  Note(answer.describe_id, 0, false, answer.item_gesture);
  // L'historique refermé est oublié : il ne parlait que de l'échoppe finie.
  // ⚠ SEULEMENT S'IL ÉTAIT MONTRÉ : pendant la vente, l'interrupteur est
  // baissé et le journal se remplit.
  if (log_shown_ && (answer.close || !state.open)) {
    store_.ClearJournal();
  }
}

void BuyingStoreStage::OpenWindows(BuyingStoreWindows& windows) {
  // LA PRÉPARATION. Une préparation qui suit un refus garde les lignes.
  ro::ui::BuyingStoreSetupState& setup = windows.setup.state;
  if (!Preparing(store_)) {
    setup.open = false;
  } else if (store_.preparation_seq() != setup.preparation_seen) {
    const std::vector<ro::ui::BuyingStoreSetupLine> kept = setup.lines;
    const std::int64_t kept_limit = setup.zeny_limit;
    ro::ui::OpenBuyingStoreSetupOn(setup, store_.preparation_seq());
    if (draft_refused_) {
      setup.lines = kept;
      setup.zeny_limit = kept_limit;
    }
  }

  // MON ÉCHOPPE.
  ro::ui::BuyingStoreOwnState& own = windows.own.state;
  if (!store_.open()) {
    own.open = false;
  } else if (store_.opening_seq() != own.opening_seen) {
    ro::ui::OpenBuyingStoreOwnOn(own, store_.opening_seq());
    draft_refused_ = false;
  }

  // L'HISTORIQUE, quand l'échoppe se ferme sur des achats.
  if (was_open_ && !store_.open() && !store_.journal().empty()) {
    windows.log.state.open = true;
  }
  if (store_.journal().empty() || store_.open()) {
    windows.log.state.open = false;
  }
  was_open_ = store_.open();
}

void BuyingStoreStage::SayOwnOutcomes(const TradeContext& context) {
  if (store_.open_refusal_seq() != open_refusal_said_) {
    open_refusal_said_ = store_.open_refusal_seq();
    draft_refused_ = true;
    const std::uint16_t code = store_.open_refusal();
    std::string text;
    if (const std::optional<TradeWord> word = WordOfOpenRefusal(code); word.has_value()) {
      text = PrintedWord(context.words, *word, store_.open_refusal_weight() / kWeightTenths);
    }
    if (text.empty()) {
      // Un code que le client d'origine ne dit pas : le nôtre, avec le code.
      text = ro::core::Printed(ro::core::Tr("Le buying store n'a pas ouvert (code %d)."),
                               static_cast<int>(code));
    }
    Say(context, std::move(text), kBuyingStoreFailedColor);
  }
  if (store_.end_seq() != end_said_) {
    end_said_ = store_.end_seq();
    const std::uint16_t code = store_.end_result();
    std::string text;
    if (const std::optional<TradeWord> word = WordOfEnd(code); word.has_value()) {
      text = PrintedWord(context.words, *word, 0);
    }
    if (text.empty()) {
      text = ro::core::Printed(ro::core::Tr("Le serveur a fermé le buying store (code %d)."),
                               static_cast<int>(code));
    }
    Say(context, std::move(text), kBuyingStoreFailedColor);
  }
}

void BuyingStoreStage::Advance(BuyingStoreWindows& windows, const TradeContext& context) {
  hovered_ = {};
  FollowSetup(windows.setup.state, std::exchange(windows.setup.answer, {}), context);
  FollowOwn(windows.own.state, std::exchange(windows.own.answer, {}));
  FollowLog(windows.log.state, std::exchange(windows.log.answer, {}));
  FollowSell(windows.sell.state, std::exchange(windows.sell.answer, {}), context);

  if (store_.TakeFarewell()) {
    if (auto said = ro::net::SayBuyingStoreClosed(context.link); !said) {
      ro::core::LogError("échoppe d'achat : {}", ro::core::Describe(said.error()));
    }
  }

  SayOwnOutcomes(context);
  OpenWindows(windows);
  // LA VISITE S'OUVRE AVANT D'ÊTRE SURVEILLÉE : un nouvel acheteur n'a pas
  // encore été vu avec son enseigne.
  OpenSellWindow(windows.sell.state);
  WatchBuyer(context);
  if (!store_.visiting()) {
    windows.sell.state.open = false;
  }
  SaySaleOutcomes(windows.sell.state, context);

  windows.setup.view = nullptr;
  windows.own.view = nullptr;
  windows.log.view = nullptr;
  windows.sell.view = nullptr;
  if (Preparing(store_)) {
    ComposeSetup(windows.setup.state, context);
    windows.setup.view = &setup_view_;
  }
  if (store_.open()) {
    ComposeOwn(context);
    windows.own.view = &own_view_;
  }
  if (windows.log.state.open) {
    ComposeLog(context);
    windows.log.view = &log_view_;
  }
  log_shown_ = windows.log.view != nullptr;
  if (store_.visiting()) {
    ComposeSell(windows.sell.state, context);
    windows.sell.view = &sell_view_;
  }
}

void BuyingStoreStage::ComposeSetup(const ro::ui::BuyingStoreSetupState& state,
                                    const TradeContext& context) {
  const ro::sim::World& world = context.world;
  const std::span<const ro::sim::InventoryItem> bag = world.inventory().items();

  setup_title_ = PrintedWord(context.words, TradeWord::kBuyingStoreTitle, 0);
  setup_view_.window_title = setup_title_;
  setup_view_.slots = store_.slots();
  setup_view_.most_amount = static_cast<int>(ro::sim::kBuyingStoreMostOwned);

  const std::vector<BuyingStoreDraftCandidate> candidates = BuyingStoreDraftCandidates(bag);
  candidate_names_.clear();
  candidates_.clear();
  candidate_names_.reserve(candidates.size());
  for (const BuyingStoreDraftCandidate& candidate : candidates) {
    candidate_names_.push_back(context.items.NameOf(candidate.item_id));
    ro::ui::BuyingStoreCandidateView shown;
    shown.item.item_id = candidate.item_id;
    shown.item.icon = context.items.IconOf(candidate.item_id);
    shown.owned = candidate.owned;
    candidates_.push_back(shown);
  }
  for (std::size_t index = 0; index < candidates_.size(); ++index) {
    candidates_[index].item.name = candidate_names_[index];
  }
  setup_view_.candidates = candidates_;

  // LE NOM TEL QUE LE FIL LE PORTERA : ce que les autres liront, et ce qu'il
  // reste d'octets avant la coupe.
  const std::string_view typed(state.title.data());
  const std::string wire = ro::net::BuyingStoreTitleOnWire(typed);
  title_on_wire_ = ro::core::Utf8FromWire(wire);
  setup_view_.title_on_wire = title_on_wire_;
  setup_view_.title_bytes_left =
      kTitleBytesKept - static_cast<int>(ro::core::WireFromUtf8Text(typed).size());

  const ro::sim::PlayerStatus& status = world.player_status();
  setup_view_.zeny = status.zeny == ro::sim::kUnknownStat ? ro::ui::kTradeNoPurse : status.zeny;
  const std::vector<ro::sim::BuyingStoreWish> wishes = WishesOf(state.lines);
  setup_view_.total_cost = ro::sim::BuyingStoreDraftCost(wishes);

  blocker_texts_.clear();
  warning_texts_.clear();
  note_texts_.assign(state.lines.size(), std::string());
  for (const ro::sim::BuyingStoreDraftProblem& problem : ro::sim::CheckBuyingStoreDraft(
           wire, state.zeny_limit, wishes, store_.slots(), bag, KnownZeny(status))) {
    const std::string text = DraftFaultText(problem.fault);
    std::vector<std::string>& into =
        ro::sim::BlocksOpening(problem.fault) ? blocker_texts_ : warning_texts_;
    if (std::ranges::find(into, text) == into.end()) {
      into.push_back(text);
    }
    if (problem.line < note_texts_.size()) {
      std::string& note = note_texts_[problem.line];
      if (!note.empty()) {
        note.push_back('\n');
      }
      note.append(text);
    }
  }
  blockers_.assign(blocker_texts_.begin(), blocker_texts_.end());
  warnings_.assign(warning_texts_.begin(), warning_texts_.end());
  notes_.assign(note_texts_.begin(), note_texts_.end());
  setup_view_.blockers = blockers_;
  setup_view_.warnings = warnings_;
  setup_view_.line_notes = notes_;
  setup_view_.awaiting = store_.phase() == BuyingStorePhase::kRequested;
}

void BuyingStoreStage::ComposeOwn(const TradeContext& context) {
  own_title_ = PrintedWord(context.words, TradeWord::kBuyingStoreList, 0);
  own_view_.window_title = own_title_;
  own_view_.store_title = store_.title();
  own_names_.clear();
  own_lines_.clear();
  for (const ro::sim::BuyingStoreLine& line : store_.lines()) {
    own_names_.push_back(context.items.NameOf(line.item_id));
    ro::ui::BuyingStoreOwnLineView shown;
    shown.item.item_id = line.item_id;
    shown.item.icon = context.items.IconOf(line.item_id);
    shown.wanted = line.wanted;
    shown.price = line.price;
    own_lines_.push_back(shown);
  }
  for (std::size_t index = 0; index < own_lines_.size(); ++index) {
    own_lines_[index].item.name = own_names_[index];
  }
  own_view_.lines = own_lines_;
  own_view_.zeny_left = store_.zeny_limit();
  own_view_.zeny_at_opening = store_.opening_zeny_limit();
  own_view_.spent = store_.spent_zeny();
  own_view_.purchases = static_cast<int>(store_.journal().size());
}

void BuyingStoreStage::ComposeLog(const TradeContext& context) {
  const std::span<const ro::sim::BuyingStorePurchase> journal = store_.journal();
  log_names_.clear();
  log_clocks_.clear();
  log_lines_.clear();
  for (const ro::sim::BuyingStorePurchase& purchase : journal) {
    log_names_.push_back(context.items.NameOf(purchase.item_id));
    log_clocks_.push_back(LocalClockText(purchase.unix_s));
    ro::ui::BuyingStoreLogLineView shown;
    shown.item.item_id = purchase.item_id;
    shown.item.icon = context.items.IconOf(purchase.item_id);
    shown.seller_char_id = purchase.seller_char_id;
    shown.amount = purchase.amount;
    shown.zeny = purchase.zeny;
    log_lines_.push_back(shown);
  }
  for (std::size_t index = 0; index < log_lines_.size(); ++index) {
    log_lines_[index].item.name = log_names_[index];
    log_lines_[index].clock = log_clocks_[index];
  }
  log_view_.lines = log_lines_;
  log_view_.total = store_.spent_zeny();
}

}  // namespace moonclient
