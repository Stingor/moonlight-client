// Les vues de l'échange entre joueurs : les deux offres, le sac tel que
// l'échange le juge, les raisons qui retiennent chaque bouton, et la boîte de
// proposition.

#include <string>

#include "ro/core/i18n.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"
#include "ro/vfs/trade_words.h"

#include "game/deal_stage.h"

namespace moonclient {
namespace {

std::string NameWithLevel(std::string_view name, std::uint16_t base_level) {
  std::string out(name);
  if (base_level > 0) {
    out += " (";
    out += ro::core::Tr("Niv.");
    out += ' ';
    out += std::to_string(base_level);
    out += ')';
  }
  return out;
}

// CE QUI SE LIT AVANT DE VALIDER : un item non identifié ne dit pas ce qu'il
// est, un item cassé ne vaut pas l'intact.
const char* NoteOf(const ro::sim::InventoryItem& item) {
  if (!item.identified) {
    return ro::core::Tr("non identifié");
  }
  if (item.damaged) {
    return ro::core::Tr("cassé");
  }
  return "";
}

}  // namespace

void DealStage::Compose(const TradeContext& context) {
  if (!deal_.open()) {
    return;
  }
  const TradeItems& items = context.items;
  title_ = std::string(ro::core::Tr("Échange avec")) + ' ' +
           NameWithLevel(deal_.partner().name, deal_.partner().base_level);

  // ── LES DEUX OFFRES ──────────────────────────────────────────────────────
  // Les dépôts en route se montrent avec mon offre, grisés : ils en seront.
  mine_items_.clear();
  std::vector<bool> in_flight;
  for (const ro::sim::DealLine& line : deal_.my_offer()) {
    mine_items_.push_back(line.item);
    in_flight.push_back(false);
  }
  for (const ro::sim::DealPendingAdd& pending : deal_.pending_adds()) {
    ro::sim::InventoryItem shown = pending.item;
    shown.amount = pending.amount;
    mine_items_.push_back(shown);
    in_flight.push_back(true);
  }
  partner_items_.clear();
  std::vector<bool> changed;
  for (const ro::sim::DealLine& line : deal_.partner_offer()) {
    partner_items_.push_back(line.item);
    changed.push_back(line.changed_after_my_lock);
  }

  mine_names_.clear();
  for (const ro::sim::InventoryItem& item : mine_items_) {
    mine_names_.push_back(items.NameOf(item, context.creators));
  }
  partner_names_.clear();
  for (const ro::sim::InventoryItem& item : partner_items_) {
    partner_names_.push_back(items.NameOf(item, context.creators));
  }

  // ── LE SAC, TEL QUE L'ÉCHANGE LE JUGE ────────────────────────────────────
  const std::span<const ro::sim::InventoryItem> bag = context.world.inventory().items();
  bag_names_.clear();
  bag_refusals_.clear();
  std::vector<int> bag_most;
  for (const ro::sim::InventoryItem& item : bag) {
    bag_names_.push_back(items.NameOf(item, context.creators));
    const std::int32_t most = deal_.MostAddable(item);
    const ro::sim::DealAddFault fault = deal_.CheckAdd(item, std::max<std::int32_t>(most, 1));
    if (fault == ro::sim::DealAddFault::kNone) {
      bag_refusals_.emplace_back();
      bag_most.push_back(most);
    } else {
      bag_refusals_.emplace_back(DealAddFaultText(fault));
      bag_most.push_back(0);
    }
  }

  // ⚠ LES VUES SE PRENNENT ICI, une fois les chaînes rangées.
  mine_.clear();
  for (std::size_t rank = 0; rank < mine_items_.size(); ++rank) {
    const ro::sim::InventoryItem& item = mine_items_[rank];
    mine_.push_back(ro::ui::DealLineView{.item = {.item_id = item.item_id,
                                                  .name = mine_names_[rank],
                                                  .icon = items.IconOf(item.item_id),
                                                  .damaged = item.damaged},
                                         .refine_level = item.refine_level,
                                         .amount = item.amount,
                                         .in_flight = in_flight[rank],
                                         .note = NoteOf(item)});
  }
  partner_.clear();
  for (std::size_t rank = 0; rank < partner_items_.size(); ++rank) {
    const ro::sim::InventoryItem& item = partner_items_[rank];
    partner_.push_back(ro::ui::DealLineView{.item = {.item_id = item.item_id,
                                                     .name = partner_names_[rank],
                                                     .icon = items.IconOf(item.item_id),
                                                     .damaged = item.damaged},
                                            .refine_level = item.refine_level,
                                            .amount = item.amount,
                                            .changed_after_my_lock = changed[rank],
                                            .note = NoteOf(item)});
  }
  bag_.clear();
  for (std::size_t rank = 0; rank < bag.size(); ++rank) {
    const ro::sim::InventoryItem& item = bag[rank];
    bag_.push_back(ro::ui::DealBagView{.client_slot = item.client_slot,
                                       .item = {.item_id = item.item_id,
                                                .name = bag_names_[rank],
                                                .icon = items.IconOf(item.item_id),
                                                .damaged = item.damaged},
                                       .refine_level = item.refine_level,
                                       .amount = item.amount,
                                       .most = bag_most[rank],
                                       .refusal = bag_refusals_[rank]});
  }

  // ── CE QUI RETIENT CHAQUE BOUTON ─────────────────────────────────────────
  const std::int64_t zeny = context.world.player_status().zeny;
  const std::int64_t purse = zeny == ro::sim::kUnknownStat ? ro::ui::kTradeNoPurse : zeny;
  lock_blockers_.clear();
  if (!deal_.pending_adds().empty()) {
    lock_blockers_.emplace_back(ro::core::Tr("Un dépôt attend encore la réponse du serveur."));
  }
  execute_blockers_.clear();
  if (const std::string_view why =
          DealExecuteFaultText(deal_.CheckExecute(deal_.partner_revision(), purse));
      !why.empty()) {
    execute_blockers_.push_back(why);
  }

  view_ = ro::ui::DealView{
      .title = title_,
      .partner_name = deal_.partner().name,
      .opening_seq = deal_.opening_seq(),
      .mine = mine_,
      .partner = partner_,
      .bag = bag_,
      .my_zeny = deal_.my_zeny(),
      .partner_zeny = deal_.partner_zeny(),
      .purse = purse,
      .partner_zeny_changed_after_my_lock = deal_.partner_zeny_changed_after_my_lock(),
      .lock_sent = deal_.lock_sent(),
      .my_locked = deal_.my_locked(),
      .partner_locked = deal_.partner_locked(),
      .execute_sent = deal_.execute_sent(),
      .partner_changed_after_my_lock = deal_.partner_changed_after_my_lock(),
      .partner_revision = deal_.partner_revision(),
      .lock_blockers = lock_blockers_,
      .execute_blockers = execute_blockers_,
      .last_refusal = last_refusal_};
}

void DealStage::ComposeProposal(const TradeContext& context) {
  proposal_shown_ = false;
  switch (deal_.phase()) {
    case ro::sim::DealPhase::kProposed: {
      // La phrase du jeu se colle au nom ; sans elle, la nôtre.
      const std::string who = NameWithLevel(deal_.partner().name, deal_.partner().base_level);
      const std::string_view suffix =
          ro::vfs::TradeText(context.words, ro::vfs::TradeWord::kDealProposedSuffix);
      proposal_text_ = suffix.empty() ? who + ' ' + ro::core::Tr("vous propose un échange.")
                                      : who + std::string(suffix);
      proposal_view_ = ro::ui::DealProposalView{.text = proposal_text_,
                                                .incoming = true,
                                                .answered = deal_.answer_sent(),
                                                .seq = deal_.proposal_seq() * 2};
      proposal_shown_ = true;
      return;
    }
    case ro::sim::DealPhase::kProposing:
      proposal_text_ =
          std::string(ro::core::Tr("Échange proposé à")) + ' ' + deal_.partner().name + '.';
      break;
    case ro::sim::DealPhase::kNone:
      if (!approach_.has_value()) {
        return;
      }
      proposal_text_ = std::string(ro::core::Tr("En route vers")) + ' ' + approach_->name + ' ' +
                       ro::core::Tr("pour lui proposer un échange…");
      break;
    case ro::sim::DealPhase::kOpen:
      return;
  }
  // ⚠ LES RANGS PAIRS SONT AUX PROPOSITIONS REÇUES, les impairs aux miennes :
  // une neuve de l'une ou l'autre sorte rouvre la boîte.
  proposal_view_ = ro::ui::DealProposalView{
      .text = proposal_text_, .incoming = false, .answered = false, .seq = outgoing_seq_ * 2 + 1};
  proposal_shown_ = true;
}

}  // namespace moonclient
