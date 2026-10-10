// L'échange entre joueurs, dans la boucle. Voir l'en-tête.

#include "game/deal_stage.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/movement.h"
#include "ro/sim/movement.h"
#include "ro/sim/world.h"
#include "ro/vfs/trade_words.h"

#include "items/inventory_view.h"

namespace moonclient {
namespace {

using ro::vfs::TradeWord;

// Les couleurs des lignes de conversation, celles des commerces : cyan pour ce
// qui a réussi, rouge pour ce qui a échoué ; l'orangé des mises en garde du
// client lui-même.
constexpr std::uint32_t kSucceededColor = 0x00FFFF;
constexpr std::uint32_t kFailedColor = 0xFF0000;
constexpr std::uint32_t kWarningColor = 0xFFAA00;

// Le gabarit de nos propres phrases : chaque « {} » reçoit la valeur suivante.
std::string Fill(std::string_view pattern, std::initializer_list<std::string_view> values) {
  std::string out;
  out.reserve(pattern.size());
  auto next = values.begin();
  for (std::size_t at = 0; at < pattern.size(); ++at) {
    if (pattern[at] == '{' && at + 1 < pattern.size() && pattern[at + 1] == '}' &&
        next != values.end()) {
      out.append(*next++);
      ++at;
      continue;
    }
    out.push_back(pattern[at]);
  }
  return out;
}

// LE MOT DU JEU, sinon le nôtre : une installation sans la clé ne laisse pas le
// joueur sans réponse.
std::string GameOr(const ro::vfs::ItemWords& words, TradeWord word, const char* ours) {
  const std::string_view said = ro::vfs::TradeText(words, word);
  return said.empty() ? std::string(ours) : std::string(said);
}

void Say(const TradeContext& context, std::string text, std::uint32_t color) {
  if (!text.empty()) {
    context.world.RecordChat(ro::sim::ChatKind::kServer, 0, std::move(text), context.now_ms, color);
  }
}

std::string AnswerText(std::uint8_t code, std::string_view partner,
                       const ro::vfs::ItemWords& words) {
  switch (code) {
    case ro::sim::kDealAnswerTooFar:
      return GameOr(words, TradeWord::kDealTooFar,
                    ro::core::Tr("Vous êtes trop loin de ce joueur pour échanger."));
    case ro::sim::kDealAnswerNoCharacter:
      return GameOr(words, TradeWord::kDealNoCharacter, ro::core::Tr("Ce joueur n'est pas là."));
    case ro::sim::kDealAnswerCancelled:
      return GameOr(words, TradeWord::kDealRejected,
                    Fill(ro::core::Tr("{} a refusé l'échange."), {partner}).c_str());
    case ro::sim::kDealAnswerFailed:
    case ro::sim::kDealAnswerBusy:
    default:
      return GameOr(words, TradeWord::kDealPartnerBusy,
                    ro::core::Tr("Ce joueur ne peut pas échanger en ce moment."));
  }
}

std::string AddRefusalText(std::uint8_t code, std::string_view item, std::string_view partner,
                           const ro::vfs::ItemWords& words) {
  switch (code) {
    case ro::sim::kDealAddOverWeight:
      return GameOr(
          words, TradeWord::kDealPartnerOverWeight,
          Fill(ro::core::Tr("{} ne peut pas porter autant de poids."), {partner}).c_str());
    case ro::sim::kDealAddBagFull:
      return Fill(ro::core::Tr("L'inventory de {} n'a plus de place pour {}."), {partner, item});
    case ro::sim::kDealAddStackOverflow:
      return Fill(ro::core::Tr("{} ne peut pas recevoir autant de {} : la pile déborderait."),
                  {partner, item});
    case ro::sim::kDealAddSilent:
      return Fill(ro::core::Tr("Le serveur n'a pas répondu au dépôt de {} : il est tenu pour "
                               "refusé."),
                  {item});
    case ro::sim::kDealAddRefused:
    default:
      return Fill(ro::core::Tr("{} ne peut pas être échangé."), {item});
  }
}

}  // namespace

const char* DealAddFaultText(ro::sim::DealAddFault fault) {
  switch (fault) {
    case ro::sim::DealAddFault::kNone:
      return "";
    case ro::sim::DealAddFault::kNotOpen:
      return ro::core::Tr("Aucun échange n'est ouvert.");
    case ro::sim::DealAddFault::kLocked:
      return ro::core::Tr("Votre offre est verrouillée.");
    case ro::sim::DealAddFault::kBadAmount:
      return ro::core::Tr("Quantité hors de la pile.");
    case ro::sim::DealAddFault::kEquipped:
      return ro::core::Tr(
          "Porté : déséquipez-le d'abord. Le serveur tiendrait l'offre d'un item "
          "porté pour de la triche.");
    case ro::sim::DealAddFault::kRented:
      return ro::core::Tr("Un item loué ne s'échange pas.");
    case ro::sim::DealAddFault::kHatchedEgg:
      return ro::core::Tr("Un œuf éclos ne s'échange pas.");
    case ro::sim::DealAddFault::kAllOffered:
      return ro::core::Tr("Toute la pile est déjà offerte.");
    case ro::sim::DealAddFault::kOfferFull:
      return ro::core::Tr("Dix items différents au plus par échange.");
  }
  return "";
}

const char* DealExecuteFaultText(ro::sim::DealExecuteFault fault) {
  switch (fault) {
    case ro::sim::DealExecuteFault::kNone:
    case ro::sim::DealExecuteFault::kNotOpen:
    case ro::sim::DealExecuteFault::kAlreadySent:
      return "";
    case ro::sim::DealExecuteFault::kNotBothLocked:
      return ro::core::Tr("Les deux offres doivent être verrouillées.");
    case ro::sim::DealExecuteFault::kAddInFlight:
      return ro::core::Tr("Un dépôt attend encore la réponse du serveur.");
    case ro::sim::DealExecuteFault::kPartnerChanged:
      return ro::core::Tr("L'offre de l'autre joueur a changé : relisez-la.");
    case ro::sim::DealExecuteFault::kZenyMelted:
      return ro::core::Tr("Votre bourse ne couvre plus le zeny offert : annulez l'échange.");
  }
  return "";
}

namespace {

std::int64_t PurseOf(const ro::sim::World& world) {
  const std::int64_t zeny = world.player_status().zeny;
  return zeny == ro::sim::kUnknownStat ? ro::ui::kTradeNoPurse : zeny;
}

}  // namespace

void DealStage::StartSession() {
  deal_ = ro::sim::Deal();
  approach_.reset();
  news_said_ = deal_.last_news_seq();
  partner_name_.clear();
  refused_frames_.clear();
  last_refusal_.clear();
  hovered_ = {};
  described_.reset();
  proposal_shown_ = false;
}

void DealStage::Watch(ro::net::Connection& link) {
  link.set_send_gate([this](std::span<const std::uint8_t> frame) { return PermitsFrame(frame); });
}

ro::net::FrameOutcome DealStage::ApplyFrame(const ro::net::Frame& frame, ro::sim::Inventory& bag) {
  return ro::net::ApplyDealFrame(frame, deal_, bag);
}

void DealStage::Propose(std::uint32_t account_id, std::string name, std::uint16_t base_level) {
  if (account_id == 0) {
    return;
  }
  approach_ = Approach{.account_id = account_id, .name = std::move(name), .base_level = base_level};
  ++outgoing_seq_;
}

bool DealStage::PermitsFrame(std::span<const std::uint8_t> frame) {
  const ro::net::DealFrameRefusal refusal = ro::net::DealRefusesFrame(deal_, frame);
  if (refusal == ro::net::DealFrameRefusal::kNone) {
    return true;
  }
  if (std::ranges::find(refused_frames_, refusal) == refused_frames_.end()) {
    refused_frames_.push_back(refusal);
  }
  ro::core::LogWarning("échange : trame 0x{:04X} retenue pendant l'échange",
                       static_cast<unsigned>(frame[0] | (frame[1] << 8)));
  return false;
}

bool DealStage::HoldsBagGesture() {
  if (!deal_.open()) {
    return false;
  }
  if (std::ranges::find(refused_frames_, ro::net::DealFrameRefusal::kItemGesture) ==
      refused_frames_.end()) {
    refused_frames_.push_back(ro::net::DealFrameRefusal::kItemGesture);
  }
  return true;
}

std::optional<TradeItemFocus> DealStage::TakeDescribed() noexcept {
  return std::exchange(described_, std::nullopt);
}

void DealStage::Note(const ro::ui::DealLineRef& line, bool hovered, bool described) {
  if (!line.valid()) {
    return;
  }
  const std::vector<ro::sim::InventoryItem>& items = line.partner ? partner_items_ : mine_items_;
  if (static_cast<std::size_t>(line.index) >= items.size()) {
    return;
  }
  const ro::sim::InventoryItem& item = items[static_cast<std::size_t>(line.index)];
  // L'OBJET SE DÉCRIT PAR CE QU'IL PORTE : offert, il a quitté le sac ; celui du
  // partenaire n'y a jamais été.
  const TradeItemFocus focus{.item_id = item.item_id, .link = ItemLinkOf(item)};
  if (hovered) {
    hovered_ = focus;
  }
  if (described) {
    described_ = focus;
  }
}

// ── LA PROPOSITION ─────────────────────────────────────────────────────────

void DealStage::FollowProposal(DealWindows& windows, const TradeContext& context) {
  const ro::ui::DealProposalAction answer = std::exchange(windows.proposal_answer, {});
  // ÉCHAP BAISSE L'INTERRUPTEUR SANS RIEN ÉMETTRE : une boîte montrée puis
  // fermée est un refus, ou le retrait de ma proposition.
  const bool closed = proposal_shown_ && !windows.proposal.open;
  const bool refused = answer.refuse || closed;

  if (approach_.has_value() && refused) {
    approach_.reset();
    return;
  }
  switch (deal_.phase()) {
    case ro::sim::DealPhase::kProposed:
      if (deal_.answer_sent()) {
        return;
      }
      if (answer.accept || refused) {
        if (auto sent = ro::net::AnswerDealProposal(context.link, deal_, answer.accept && !closed);
            !sent) {
          ro::core::LogError("échange : {}", ro::core::Describe(sent.error()));
        }
      }
      return;
    case ro::sim::DealPhase::kProposing:
      if (refused) {
        if (auto sent = ro::net::CancelDeal(context.link, deal_); !sent) {
          ro::core::LogError("échange : {}", ro::core::Describe(sent.error()));
        }
      }
      return;
    case ro::sim::DealPhase::kNone:
    case ro::sim::DealPhase::kOpen:
      return;
  }
}

void DealStage::AdvanceApproach(const TradeContext& context) {
  if (!approach_.has_value()) {
    return;
  }
  Approach& approach = *approach_;
  if (!approach.started) {
    approach.started = true;
    approach.since_ms = context.now_ms;
  }
  if (deal_.open()) {
    Say(context, ro::core::Tr("Terminez d'abord l'échange en cours."), kFailedColor);
    approach_.reset();
    return;
  }
  const ro::sim::Actor* self = context.world.player();
  const ro::sim::Actor* partner = context.world.Find(approach.account_id);
  if (self == nullptr || partner == nullptr) {
    Say(context, AnswerText(ro::sim::kDealAnswerNoCharacter, approach.name, context.words),
        kFailedColor);
    approach_.reset();
    return;
  }
  const ro::sim::CellPosition mine = ro::sim::StandingCell(*self);
  const ro::sim::CellPosition theirs = ro::sim::StandingCell(*partner);
  if (ro::sim::DealReachable(mine.x, mine.y, theirs.x, theirs.y)) {
    ro::sim::DealPartner asked{.account_id = approach.account_id,
                               .name = approach.name,
                               .base_level = approach.base_level};
    if (auto sent = ro::net::ProposeDeal(context.link, deal_, std::move(asked), context.now_ms);
        !sent) {
      ro::core::LogError("échange : {}", ro::core::Describe(sent.error()));
    }
    approach_.reset();
    return;
  }
  if (context.now_ms - approach.since_ms >= kDealApproachGiveUpMs) {
    Say(context,
        Fill(ro::core::Tr("Impossible de rejoindre {} pour lui proposer un échange."),
             {approach.name}),
        kFailedColor);
    approach_.reset();
    return;
  }
  // UNE MARCHE PAR CASE DU PARTENAIRE : il bouge, on repart vers lui ; il reste,
  // on ne redemande rien. La porte commune de toutes les marches s'applique.
  const bool moved = !approach.walked_to.has_value() || approach.walked_to->x != theirs.x ||
                     approach.walked_to->y != theirs.y;
  if (moved && approach_clock_ != nullptr &&
      ro::sim::AdmitWalkRequest(context.world, *approach_clock_, context.now_ms)) {
    if (auto sent = ro::net::RequestMove(context.link, theirs); !sent) {
      ro::core::LogError("échange : {}", ro::core::Describe(sent.error()));
    }
    approach.walked_to = theirs;
  }
}

// ── LA FENÊTRE ─────────────────────────────────────────────────────────────

void DealStage::FollowWindow(DealWindows& windows, const TradeContext& context) {
  const ro::ui::DealWindowAction answer = std::exchange(windows.answer, {});
  const bool shown = windows.view != nullptr;
  hovered_ = {};
  if (!deal_.open()) {
    return;
  }
  Note(answer.hovered, true, false);
  Note(answer.described, false, true);
  // LA CROIX, ÉCHAP OU « ANNULER » : l'échange s'annule chez le serveur.
  if (answer.cancel || (shown && !windows.state.open)) {
    if (auto sent = ro::net::CancelDeal(context.link, deal_); !sent) {
      ro::core::LogError("échange : {}", ro::core::Describe(sent.error()));
    }
    return;
  }
  const std::int64_t purse = PurseOf(context.world);

  // ⚠ CHAQUE GESTE SE REJUGE : le verdict dessiné a une image de retard.
  if (answer.offer.has_value()) {
    if (const ro::sim::InventoryItem* held =
            context.world.inventory().Find(answer.offer->client_slot);
        held != nullptr) {
      const std::int32_t amount = std::min(answer.offer->amount, deal_.MostAddable(*held));
      if (const ro::sim::DealAddFault fault = deal_.CheckAdd(*held, amount);
          fault != ro::sim::DealAddFault::kNone) {
        last_refusal_ = DealAddFaultText(fault);
      } else if (auto sent =
                     ro::net::OfferDealItem(context.link, deal_, *held, amount, context.now_ms);
                 !sent) {
        ro::core::LogError("échange : {}", ro::core::Describe(sent.error()));
      } else {
        last_refusal_.clear();
      }
    }
  }
  bool zeny_held = false;
  if (answer.zeny.has_value() && *answer.zeny != deal_.my_zeny()) {
    if (auto sent = ro::net::OfferDealZeny(context.link, deal_, *answer.zeny, purse); !sent) {
      ro::core::LogError("échange : {}", ro::core::Describe(sent.error()));
      zeny_held = true;
    }
  }
  // UN ZENY REFUSÉ RETIENT LE VERROU : on ne verrouille pas autre chose que ce
  // que le joueur a tapé.
  if (answer.lock && !zeny_held) {
    if (auto sent = ro::net::LockDeal(context.link, deal_); !sent) {
      ro::core::LogError("échange : {}", ro::core::Describe(sent.error()));
    }
  }
  if (answer.execute) {
    if (auto sent = ro::net::ExecuteDeal(context.link, deal_, answer.execute_revision, purse);
        !sent) {
      Say(context, DealExecuteFaultText(deal_.CheckExecute(answer.execute_revision, purse)),
          kFailedColor);
    }
  }
}

// ── LES NOUVELLES ──────────────────────────────────────────────────────────

void DealStage::SayNews(const TradeContext& context) {
  const ro::vfs::ItemWords& words = context.words;
  for (const ro::sim::DealNews& news : deal_.news()) {
    if (news.seq <= news_said_) {
      continue;
    }
    news_said_ = news.seq;
    switch (news.kind) {
      case ro::sim::DealNewsKind::kAnswered:
        Say(context, AnswerText(news.code, partner_name_, words), kFailedColor);
        break;
      case ro::sim::DealNewsKind::kAddRefused: {
        const std::string item = context.items.NameOf(news.item, context.creators);
        last_refusal_ = AddRefusalText(news.code, item, partner_name_, words);
        Say(context, last_refusal_, kFailedColor);
        break;
      }
      case ro::sim::DealNewsKind::kPartnerLocked:
        Say(context, Fill(ro::core::Tr("{} a verrouillé son offre."), {partner_name_}),
            kSucceededColor);
        break;
      case ro::sim::DealNewsKind::kPartnerChangedAfterMyLock:
        Say(context,
            Fill(ro::core::Tr("{} a changé son offre après votre verrou : relisez-la avant "
                              "d'échanger."),
                 {partner_name_}),
            kWarningColor);
        break;
      case ro::sim::DealNewsKind::kCancelled:
        Say(context,
            GameOr(words, TradeWord::kDealCancelled, ro::core::Tr("L'échange a été annulé.")),
            kFailedColor);
        break;
      case ro::sim::DealNewsKind::kRefusedAtExecution:
        Say(context, GameOr(words, TradeWord::kDealNotDone, ro::core::Tr("L'échange a échoué.")),
            kFailedColor);
        Say(context,
            ro::core::Tr("Le serveur a refusé de l'exécuter : place, poids ou zeny insuffisants "
                         "chez l'un des deux. Rien n'a changé de main."),
            kWarningColor);
        break;
      case ro::sim::DealNewsKind::kExecuted:
        Say(context, GameOr(words, TradeWord::kDealDone, ro::core::Tr("L'échange a été effectué.")),
            kSucceededColor);
        break;
    }
  }
}

void DealStage::SayRefusedFrames(const TradeContext& context) {
  for (const ro::net::DealFrameRefusal refusal : std::exchange(refused_frames_, {})) {
    switch (refusal) {
      case ro::net::DealFrameRefusal::kNone:
        break;
      case ro::net::DealFrameRefusal::kItemGesture:
        Say(context,
            GameOr(context.words, TradeWord::kDealHoldsItems,
                   ro::core::Tr("Impossible d'utiliser, d'équiper, d'ôter, de jeter ou de ranger "
                                "un item pendant un échange.")),
            kFailedColor);
        break;
      case ro::net::DealFrameRefusal::kSkillWithStake:
        Say(context,
            ro::core::Tr("Pas de skill pendant un échange où vous offrez quelque chose : il "
                         "pourrait consommer un item ou du zeny offert, ce que le serveur "
                         "tiendrait pour de la triche."),
            kWarningColor);
        break;
      case ro::net::DealFrameRefusal::kZenyWithStake:
        Say(context,
            ro::core::Tr("Pas de dépense de zeny pendant que vous en offrez dans un échange : le "
                         "serveur tiendrait l'offre pour de la triche."),
            kWarningColor);
        break;
    }
  }
}

// ── L'IMAGE ────────────────────────────────────────────────────────────────

void DealStage::Advance(DealWindows& windows, const TradeContext& context) {
  if (deal_.phase() != ro::sim::DealPhase::kNone && !deal_.partner().name.empty()) {
    partner_name_ = deal_.partner().name;
  }
  FollowProposal(windows, context);
  FollowWindow(windows, context);
  AdvanceApproach(context);
  if (deal_.open()) {
    deal_.ExpireSilentAdds(context.now_ms);
  }
  SayNews(context);
  SayRefusedFrames(context);
  if (!deal_.open()) {
    last_refusal_.clear();
  }

  Compose(context);
  ComposeProposal(context);
  windows.state.open = deal_.open();
  windows.view = deal_.open() ? &view_ : nullptr;
  windows.proposal_view = proposal_shown_ ? &proposal_view_ : nullptr;
  windows.proposal.open = proposal_shown_;
}

}  // namespace moonclient
