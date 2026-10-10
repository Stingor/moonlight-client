// La boutique d'un PNJ, dans la boucle. Voir l'en-tête.

#include "game/npc_shop_stage.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/printed.h"
#include "ro/net/npc_dialog.h"
#include "ro/net/npc_shop.h"
#include "ro/sim/npc_dialog.h"
#include "ro/sim/npc_shop_cart.h"
#include "ro/sim/player_status.h"
#include "ro/sim/world.h"
#include "ro/vfs/trade_words.h"

namespace moonclient {
namespace {

using ro::core::Printed;
using ro::sim::NpcShopIssue;
using ro::sim::NpcShopKind;
using ro::sim::NpcShopSide;
using ro::vfs::TradeWord;

// Les couleurs des lignes de conversation, celles du client : cyan pour ce qui
// a réussi, rouge pour ce qui a échoué.
constexpr std::uint32_t kSucceededColor = 0x00FFFF;
constexpr std::uint32_t kFailedColor = 0xFF0000;

// Le résultat du market qui écrase tout échec.
constexpr std::uint16_t kMarketFailed = 0xFFFF;

// L'unité d'un prix en points, celle du client pour une boutique qui ne vend
// pas contre des zeny.
constexpr std::string_view kPointUnit = " P";
constexpr std::string_view kZenyUnit = "z";

// Les poids se comptent en dixièmes, comme le serveur les tient.
constexpr std::int64_t kWeightTenths = 10;

// UN CODE DE RÉSULTAT, ET CE QUE LE CLIENT EN DIT. Voir
// `docs/commerces-messages.md` : la table vient du client d'origine.
struct Said {
  std::optional<TradeWord> word;
  bool succeeded = false;
};

Said SaidOfPurchase(std::uint16_t result) {
  switch (result) {
    case 0:
      return {TradeWord::kDealSucceeded, true};
    case 1:
      return {TradeWord::kLacksZeny, false};
    case 2:
      return {TradeWord::kOverWeight, false};
    case 3:
      return {TradeWord::kOverCapacity, false};
    case 9:
      return {TradeWord::kEachItemOverCount, false};
    case 10:
      return {TradeWord::kSentByRodex, true};
    default:
      return {};
  }
}

Said SaidOf(const ro::sim::NpcShopOutcome& outcome) {
  switch (outcome.kind) {
    case NpcShopKind::kZeny:
      if (outcome.side == NpcShopSide::kSell) {
        return outcome.result == ro::sim::kNpcShopDealSucceeded
                   ? Said{TradeWord::kDealSucceeded, true}
                   : Said{TradeWord::kDealFailed, false};
      }
      return SaidOfPurchase(outcome.result);
    case NpcShopKind::kMarket:
      if (outcome.result == kMarketFailed) {
        return {TradeWord::kPurchaseFailed, false};
      }
      return SaidOfPurchase(outcome.result);
    case NpcShopKind::kPoints:
      switch (outcome.result) {
        case 0:
          return {TradeWord::kDealSucceeded, true};
        case 1:
          return {TradeWord::kPointFailNpc, false};
        case 2:
          return {TradeWord::kPointFailSystem, false};
        case 3:
          return {TradeWord::kOverWeight, false};
        case 4:
          return {TradeWord::kPointFailTrading, false};
        case 5:
          return {TradeWord::kPointFailItem, false};
        case 6:
          return {TradeWord::kPointFailMoney, false};
        case 7:
          return {TradeWord::kPointFailTotal, false};
        case 8:
          return {TradeWord::kPointFailSome, false};
        case 9:
          return {TradeWord::kInventoryKindsFull, false};
        default:
          return {};
      }
    case NpcShopKind::kNone:
      break;
  }
  return {};
}

// Ce que le joueur lit quand le panier est retenu. Nos mots : le jeu n'en a pas
// pour dire « avant ».
const char* IssueText(NpcShopIssue issue, bool points) {
  switch (issue) {
    case NpcShopIssue::kEmptyCart:
      return ro::core::Tr("Le panier est vide.");
    case NpcShopIssue::kBadAmount:
      return ro::core::Tr("Une quantité est nulle.");
    case NpcShopIssue::kNotOffered:
      return ro::core::Tr("Un item n'est plus proposé par la boutique.");
    case NpcShopIssue::kLacksMoney:
      return points ? ro::core::Tr("Points insuffisants.") : ro::core::Tr("Zeny insuffisant.");
    case NpcShopIssue::kOverStock:
      return ro::core::Tr("Plus que le stock restant.");
    case NpcShopIssue::kOnePerPurchase:
      return ro::core::Tr("Un équipement s'achète un à la fois.");
    case NpcShopIssue::kOverStack:
      return ro::core::Tr("Dépasserait 30 000 exemplaires d'un même item.");
    case NpcShopIssue::kOverWeight:
      return ro::core::Tr("Trop lourd pour le personnage.");
    case NpcShopIssue::kOverOwned:
      return ro::core::Tr("Plus que ce que l'inventory contient.");
    case NpcShopIssue::kZenyOverflow:
      return ro::core::Tr("La recette dépasserait le plafond de zeny : l'excédent serait perdu.");
  }
  return "";
}

// Le nom montré d'un PNJ : ce qui précède le dièse. Le reste distingue deux
// PNJ homonymes dans les scripts, et le client ne l'écrit jamais.
std::string_view ShownNpcName(std::string_view name) { return name.substr(0, name.find('#')); }

const ro::sim::InventoryItem* BagItemAt(const ro::sim::World& world, std::uint16_t slot) {
  for (const ro::sim::InventoryItem& item : world.inventory().items()) {
    if (item.client_slot == slot) {
      return &item;
    }
  }
  return nullptr;
}

bool Precious(const ro::sim::InventoryItem& item) {
  return item.refine_level > 0 ||
         std::ranges::any_of(item.cards, [](std::uint32_t card) { return card != 0; });
}

std::int64_t Carried(const ro::sim::World& world, std::uint32_t item_id) {
  std::int64_t carried = 0;
  for (const ro::sim::InventoryItem& item : world.inventory().items()) {
    if (item.item_id == item_id && item.worn_at == 0) {
      carried += item.amount;
    }
  }
  return carried;
}

std::vector<ro::sim::NpcShopBuyLine> BuyLinesOf(std::span<const ro::ui::NpcShopCartLine> cart) {
  std::vector<ro::sim::NpcShopBuyLine> lines;
  lines.reserve(cart.size());
  for (const ro::ui::NpcShopCartLine& line : cart) {
    lines.push_back({.item_id = line.item_id, .amount = line.amount});
  }
  return lines;
}

std::vector<ro::sim::NpcShopSellLine> SellLinesOf(std::span<const ro::ui::NpcShopCartLine> cart) {
  std::vector<ro::sim::NpcShopSellLine> lines;
  lines.reserve(cart.size());
  for (const ro::ui::NpcShopCartLine& line : cart) {
    lines.push_back({.client_slot = line.client_slot, .amount = line.amount});
  }
  return lines;
}

}  // namespace

void NpcShopStage::StartSession() {
  shop_.CloseSilently();
  outcome_said_ = shop_.outcome().seq;
  asked_opening_ = 0;
  sent_from_cart_ = false;
  close_after_sale_ = false;
  hovered_ = {};
  described_.reset();
}

std::optional<TradeItemFocus> NpcShopStage::TakeDescribed() noexcept {
  return std::exchange(described_, std::nullopt);
}

void NpcShopStage::AskList(NpcShopSide side, const TradeContext& context) {
  // Une boutique qu'on ne peut pas nommer ne se redemande pas : c'est un
  // script qui l'a ouverte, et elle n'a qu'une liste.
  if (shop_.kind() != NpcShopKind::kZeny || shop_.npc_id() == 0) {
    return;
  }
  if (auto asked = ro::net::ChooseNpcShopSide(context.link, shop_, side); !asked) {
    ro::core::LogError("boutique de PNJ : {}", ro::core::Describe(asked.error()));
  }
  asked_opening_ = shop_.opening_seq();
  asked_side_ = side;
}

void NpcShopStage::Follow(ro::ui::NpcShopWindowState& state,
                          const ro::ui::NpcShopWindowAction& answer, const TradeContext& context) {
  hovered_ = TradeItemFocus{.item_id = answer.hovered_id, .bag_slot = answer.hovered_slot};
  if (answer.describe_id != 0) {
    described_ = TradeItemFocus{.item_id = answer.describe_id,
                                .bag_slot = answer.describe_slot,
                                .gesture = answer.item_gesture};
  }
  if (!shop_.open()) {
    return;
  }
  // 🔴 UNE FENÊTRE BAISSÉE EST UNE FERMETURE, quel que soit le geste — la
  // croix, « Fermer », Échap que la pile a pris. Elle se dit au serveur au
  // tour suivant, par le congé.
  //
  // ⚠ MAIS SEULEMENT UNE FENÊTRE QUI MONTRAIT DÉJÀ CETTE OUVERTURE : à l'image
  // où la boutique s'ouvre, la fenêtre est encore baissée, et la prendre pour
  // une fermeture refermerait toute boutique à peine ouverte.
  const bool shown = state.opening_seen == shop_.opening_seq();
  if (answer.close || (!state.open && shown)) {
    shop_.Close();
    return;
  }
  if (!shown) {
    return;
  }
  if (answer.drop_refused) {
    context.world.RecordChat(ro::sim::ChatKind::kServer, 0,
                             ro::core::Tr("Ce PNJ n'achète pas cet item."), context.now_ms,
                             kFailedColor);
  }
  if (answer.tab_changed) {
    AskList(state.tab == ro::ui::NpcShopTab::kSell ? NpcShopSide::kSell : NpcShopSide::kBuy,
            context);
  }

  const ro::sim::PlayerStatus& status = context.world.player_status();
  const bool points = shop_.kind() == NpcShopKind::kPoints;
  if (!answer.buy.empty()) {
    const std::vector<ro::sim::NpcShopBuyLine> lines = BuyLinesOf(answer.buy);
    const std::int64_t money = points ? shop_.points() : status.zeny;
    const ro::sim::NpcShopVerdict verdict = ro::sim::CheckNpcShopBuy(
        shop_, lines, money, context.world.inventory().items(), status.weight, status.max_weight);
    // ⚠ REJUGÉ ICI : un Ctrl + clic n'a montré aucun verdict, et la fenêtre a
    // pu juger le panier d'une image de retard. Ce qui échouerait ne part pas,
    // et la raison se dit là où le joueur la lira.
    if (!verdict.clear()) {
      context.world.RecordChat(ro::sim::ChatKind::kServer, 0,
                               IssueText(verdict.issues.front(), points), context.now_ms,
                               kFailedColor);
      return;
    }
    if (auto sent = ro::net::BuyFromNpcShop(context.link, shop_, lines); !sent) {
      ro::core::LogError("boutique de PNJ : {}", ro::core::Describe(sent.error()));
      return;
    }
    sent_from_cart_ = !answer.immediate;
  } else if (!answer.sell.empty()) {
    const std::vector<ro::sim::NpcShopSellLine> lines = SellLinesOf(answer.sell);
    const ro::sim::NpcShopVerdict verdict =
        ro::sim::CheckNpcShopSell(shop_, lines, context.world.inventory().items(), status.zeny);
    if (!verdict.clear()) {
      context.world.RecordChat(ro::sim::ChatKind::kServer, 0,
                               IssueText(verdict.issues.front(), false), context.now_ms,
                               kFailedColor);
      return;
    }
    if (auto sent = ro::net::SellToNpcShop(context.link, shop_, lines); !sent) {
      ro::core::LogError("boutique de PNJ : {}", ro::core::Describe(sent.error()));
      return;
    }
    sent_from_cart_ = !answer.immediate;
    close_after_sale_ = sent_from_cart_ && state.close_after_sale;
  }
}

void NpcShopStage::SayOutcome(ro::ui::NpcShopWindowState& state, const TradeContext& context) {
  const ro::sim::NpcShopOutcome& outcome = shop_.outcome();
  if (outcome.seq == 0 || outcome.seq == outcome_said_) {
    return;
  }
  outcome_said_ = outcome.seq;
  const Said said = SaidOf(outcome);
  outcome_text_.clear();
  if (said.word.has_value()) {
    outcome_text_ = std::string(ro::vfs::TradeText(context.words, *said.word));
    if (!outcome_text_.empty()) {
      context.world.RecordChat(ro::sim::ChatKind::kServer, 0, outcome_text_, context.now_ms,
                               said.succeeded ? kSucceededColor : kFailedColor);
    }
  }
  view_.outcome_done = said.succeeded;
  if (!said.succeeded) {
    return;
  }
  // LE PANIER A ÉTÉ SERVI : il se vide. Un achat immédiat n'y avait pas touché.
  if (sent_from_cart_) {
    state.cart.clear();
  }
  if (outcome.side == NpcShopSide::kSell && close_after_sale_) {
    close_after_sale_ = false;
    shop_.Close();
    state.open = false;
    return;
  }
  // LA LISTE A CHANGÉ — un objet vendu n'est plus au sac, une pile achetée a
  // grossi —, et le serveur a oublié la boutique : la redemander la rafraîchit
  // ET la réarme.
  if (shop_.open()) {
    AskList(outcome.side, context);
  }
}

void NpcShopStage::Advance(ro::ui::NpcShopWindowState& state,
                           const ro::ui::NpcShopWindowAction& answer, const TradeContext& context) {
  Follow(state, answer, context);

  if (const ro::sim::NpcShopFarewell due = shop_.TakeFarewell();
      due != ro::sim::NpcShopFarewell::kNone) {
    if (auto said = ro::net::SayNpcShopFarewell(context.link, due); !said) {
      ro::core::LogError("boutique de PNJ : {}", ro::core::Describe(said.error()));
    }
    // 🔴 LE DIALOGUE QUI A OUVERT LA BOUTIQUE PEUT RESTER À L'ÉCRAN : un script
    // qui écrit puis appelle `callshop` n'envoie jamais de « fermer ». Sa page
    // muette se referme avec la boutique ; une page qui attend encore une
    // réponse — un menu, une saisie — reste au joueur.
    if (const std::optional<ro::sim::NpcDialog>& dialog = context.world.npc_dialog();
        dialog.has_value() && (dialog->prompt == ro::sim::NpcPrompt::kNone ||
                               dialog->prompt == ro::sim::NpcPrompt::kClose)) {
      if (auto left = ro::net::LeaveNpcDialog(context.link, context.world); !left) {
        ro::core::LogError("boutique de PNJ : {}", ro::core::Describe(left.error()));
      }
    }
  }

  if (!shop_.open()) {
    state.open = false;
  } else if (shop_.opening_seq() != state.opening_seen) {
    ro::ui::OpenNpcShopOn(state, shop_.opening_seq(),
                          shop_.deals() != ro::sim::NpcShopDeals::kSellOnly);
    close_after_sale_ = false;
    outcome_text_.clear();
    // LA BOUTIQUE NOMMÉE S'OUVRE SUR L'ACHAT : sa liste se demande tout de
    // suite, sans passer par la fenêtre « Acheter / Vendre » du client
    // d'origine.
    if (shop_.buy_seq() == 0 && asked_opening_ != shop_.opening_seq()) {
      AskList(NpcShopSide::kBuy, context);
    }
  }

  SayOutcome(state, context);
  Compose(state, context);
}

void NpcShopStage::Compose(const ro::ui::NpcShopWindowState& state, const TradeContext& context) {
  if (!shop_.open()) {
    return;
  }
  const ro::sim::World& world = context.world;
  const ro::sim::PlayerStatus& status = world.player_status();
  const bool points = shop_.kind() == NpcShopKind::kPoints;

  view_.kind = shop_.kind() == NpcShopKind::kMarket   ? ro::ui::NpcShopWindowKind::kMarket
               : shop_.kind() == NpcShopKind::kPoints ? ro::ui::NpcShopWindowKind::kPoints
                                                      : ro::ui::NpcShopWindowKind::kZeny;
  view_.can_buy = shop_.deals() != ro::sim::NpcShopDeals::kSellOnly;
  view_.can_sell = shop_.deals() != ro::sim::NpcShopDeals::kBuyOnly;

  // LE TITRE : le PNJ boutique, ou celui qui parle — un script ne nomme pas sa
  // boutique.
  std::uint32_t named = shop_.npc_id();
  if (named == 0 && world.npc_dialog().has_value()) {
    named = world.npc_dialog()->npc_id;
  }
  title_.clear();
  if (const ro::sim::Actor* npc = world.Find(named); npc != nullptr) {
    title_ = std::string(ShownNpcName(npc->name));
  }
  view_.title = title_;

  unit_ = std::string(points ? kPointUnit : kZenyUnit);
  view_.unit = unit_;
  if (points) {
    view_.purse = shop_.points();
    purse_line_ = ro::vfs::TradeTextWith(context.words, TradeWord::kPointShopPoints,
                                         world.player_name(), shop_.points());
    if (purse_line_.empty()) {
      purse_line_ =
          Printed(ro::core::Tr("Points : %s"), ro::ui::TradeAmountText(shop_.points(), {}).c_str());
    }
  } else {
    view_.purse = status.zeny == ro::sim::kUnknownStat ? ro::ui::kTradeNoPurse : status.zeny;
    purse_line_ = status.zeny == ro::sim::kUnknownStat
                      ? std::string()
                      : Printed(ro::core::Tr("Zeny : %s"),
                                ro::ui::TradeAmountText(status.zeny, kZenyUnit).c_str());
  }
  view_.purse_line = purse_line_;

  // LES OFFRES.
  const std::span<const ro::sim::NpcShopOffer> offers = shop_.buy_offers();
  offer_names_.clear();
  offer_names_.reserve(offers.size());
  offers_.clear();
  offers_.reserve(offers.size());
  for (const ro::sim::NpcShopOffer& offer : offers) {
    offer_names_.push_back(context.items.NameOf(offer.item_id));
    ro::ui::NpcShopOfferView shown;
    shown.item.item_id = offer.item_id;
    shown.item.icon = context.items.IconOf(offer.item_id);
    shown.base_price = offer.base_price;
    shown.price = offer.price;
    shown.stock =
        offer.stock == ro::sim::kNpcShopUnlimitedStock ? ro::ui::kNpcShopNoStock : offer.stock;
    // CE QU'ON PEUT EN PRENDRE D'UN COUP : un équipement à l'unité, sinon ce qui
    // reste de place dans la pile, et jamais plus que le rayon.
    std::int64_t most = ro::sim::ItemTypeStacks(offer.item_type)
                            ? ro::sim::kNpcShopMostPerStack - Carried(world, offer.item_id)
                            : 1;
    if (offer.stock != ro::sim::kNpcShopUnlimitedStock) {
      most = std::min(most, offer.stock);
    }
    shown.most = static_cast<int>(std::max<std::int64_t>(most, 0));
    offers_.push_back(shown);
  }
  for (std::size_t index = 0; index < offers_.size(); ++index) {
    offers_[index].item.name = offer_names_[index];
  }
  view_.offers = offers_;
  view_.offers_listed = shop_.buy_seq() != 0;

  // LES VENTES : la case que le serveur accepte, l'objet que le sac y tient.
  //
  // 🔴 LE VERROU DE VENTE DES FAVORIS EST À NOUS, pas au serveur : il n'en sait
  // rien. Posé, il retire les favoris de la liste — et le compte le dit, pour
  // qu'une ligne qui manque ne se lise pas comme un refus du PNJ.
  sell_names_.clear();
  sells_.clear();
  view_.hidden_favorites = 0;
  for (const ro::sim::NpcShopSellOffer& offer : shop_.sell_offers()) {
    const ro::sim::InventoryItem* item = BagItemAt(world, offer.client_slot);
    if (item == nullptr || item->amount <= 0) {
      continue;
    }
    if (context.sell_locked && item->favorite) {
      ++view_.hidden_favorites;
      continue;
    }
    sell_names_.push_back(context.items.NameOf(*item, context.creators));
    ro::ui::NpcShopSellView shown;
    shown.item.item_id = item->item_id;
    shown.item.icon = context.items.IconOf(item->item_id);
    shown.item.damaged = item->damaged;
    shown.client_slot = offer.client_slot;
    shown.amount = item->amount;
    shown.base_price = offer.base_price;
    shown.price = offer.price;
    shown.precious = Precious(*item);
    sells_.push_back(shown);
  }
  for (std::size_t index = 0; index < sells_.size(); ++index) {
    sells_[index].item.name = sell_names_[index];
  }
  view_.sells = sells_;
  view_.sells_listed = shop_.sell_seq() != 0;
  // LES FILTRES DU SERVEUR : tant que `@sellstuff` est éteint, il retire de la
  // liste les cartes, les items raffinés ou sertis et quelques munitions ; tant
  // que `@sellitem` l'est, un groupe d'items de plus. Rien ne le dit ailleurs.
  view_.server_filter_note = {};
  if (context.commands != nullptr && context.commands->told &&
      (!context.commands->sell_stuff || !context.commands->sell_item)) {
    view_.server_filter_note = ro::core::Tr(
        "Le serveur masque les cartes, les items raffinés ou sertis : voir Sell Stuff et "
        "Sell Item dans Commands Settings.");
  }

  view_.outcome = outcome_text_;
  view_.awaiting = shop_.awaiting_result();
  ComposeVerdict(state, context);
}

void NpcShopStage::ComposeVerdict(const ro::ui::NpcShopWindowState& state,
                                  const TradeContext& context) {
  const ro::sim::World& world = context.world;
  const ro::sim::PlayerStatus& status = world.player_status();
  const bool points = shop_.kind() == NpcShopKind::kPoints;
  blocker_texts_.clear();
  blockers_.clear();
  cart_weight_.clear();
  view_.cart_total = 0;
  if (state.cart.empty()) {
    view_.blockers = {};
    view_.cart_weight = {};
    return;
  }
  ro::sim::NpcShopVerdict verdict;
  if (state.tab == ro::ui::NpcShopTab::kSell) {
    verdict = ro::sim::CheckNpcShopSell(shop_, SellLinesOf(state.cart), world.inventory().items(),
                                        status.zeny);
  } else {
    verdict = ro::sim::CheckNpcShopBuy(shop_, BuyLinesOf(state.cart),
                                       points ? shop_.points() : status.zeny,
                                       world.inventory().items(), status.weight, status.max_weight);
    if (verdict.weight_known) {
      std::array<char, 64> weight{};
      std::snprintf(weight.data(), weight.size(), ro::core::Tr("+%lld poids"),
                    static_cast<long long>(verdict.added_weight / kWeightTenths));
      cart_weight_ = weight.data();
    }
  }
  for (const NpcShopIssue issue : verdict.issues) {
    blocker_texts_.emplace_back(IssueText(issue, points));
  }
  for (const std::string& text : blocker_texts_) {
    blockers_.push_back(text);
  }
  view_.blockers = blockers_;
  view_.cart_total = verdict.total;
  view_.cart_weight = cart_weight_;
}

}  // namespace moonclient
