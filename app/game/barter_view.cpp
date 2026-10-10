// Le troc vu par la fenêtre. Voir l'en-tête.

#include "game/barter_view.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/sim/npc_shop_cart.h"
#include "ro/ui/trade_widgets.h"

namespace moonclient {
namespace {

using ro::sim::BarterCartRefusal;
using ro::vfs::TradeWord;

// Les poids se tiennent en dixièmes, comme le serveur les compte.
constexpr std::int64_t kWeightTenths = 10;

constexpr std::string_view kZenyUnit = "z";

using ro::core::Printed;

// Le raffinement d'un article, que le fil porte sur quatre octets et un nom sur
// deux : au-delà, rien de réel.
std::uint16_t RefineShown(std::uint32_t refine_level) {
  return static_cast<std::uint16_t>(
      std::min<std::uint32_t>(refine_level, std::numeric_limits<std::uint16_t>::max()));
}

// Le mot du jeu, ou le nôtre quand l'installation ne le porte pas.
std::string GameOr(const ro::vfs::ItemWords& words, TradeWord word, std::string fallback) {
  std::string said(ro::vfs::TradeText(words, word));
  return said.empty() ? fallback : said;
}

}  // namespace

// ── LES MOTS ────────────────────────────────────────────────────────────────

BarterResultSaying SayingOfBarterResult(std::uint8_t code) {
  switch (code) {
    case ro::sim::kBarterResultSucceeded:
      return {TradeWord::kDealSucceeded, true};
    case ro::sim::kBarterResultNotEnoughZeny:
      return {TradeWord::kLacksZeny, false};
    case ro::sim::kBarterResultOverWeight:
      return {TradeWord::kOverWeight, false};
    case ro::sim::kBarterResultTooManyItems:
      return {TradeWord::kOverCapacity, false};
    case ro::sim::kBarterResultFailed:
      return {TradeWord::kExchangeFailed, false};
    case ro::sim::kBarterResultExchanged:
      return {TradeWord::kExchangeSucceeded, true};
    case ro::sim::kBarterResultSoldOut:
      return {TradeWord::kExchangeOutOfStock, false};
    case ro::sim::kBarterResultNotEnoughGoods:
      return {TradeWord::kExchangeLacksGoods, false};
    default:
      // Un code que la table ne connaît pas : le client d'origine se tait, mais
      // un succès reste un succès.
      return {std::nullopt, ro::sim::BarterResultSucceeded(code)};
  }
}

std::string BarterResultText(std::uint8_t code, const ro::vfs::ItemWords& words) {
  const BarterResultSaying saying = SayingOfBarterResult(code);
  if (saying.word.has_value()) {
    std::string said(ro::vfs::TradeText(words, *saying.word));
    if (!said.empty()) {
      return said;
    }
  }
  return saying.succeeded ? std::string(ro::core::Tr("Échange effectué."))
                          : Printed(ro::core::Tr("Échange refusé par le serveur (code %lld)."),
                                    static_cast<long long>(code));
}

std::string BarterRefusalText(BarterCartRefusal refusal, std::string_view item_name,
                              const ro::vfs::ItemWords& words) {
  const std::string name(item_name);
  switch (refusal) {
    case BarterCartRefusal::kNone:
      return {};
    case BarterCartRefusal::kNotOpen:
      return ro::core::Tr("Le troc est fermé.");
    case BarterCartRefusal::kEmptyCart:
      return ro::core::Tr("Le panier est vide.");
    case BarterCartRefusal::kTooManyLines:
      return ro::core::Tr("Trop de lignes pour un seul échange.");
    case BarterCartRefusal::kUnknownItem:
    case BarterCartRefusal::kZeroAmount:
      return GameOr(words, TradeWord::kExchangeFailed,
                    ro::core::Tr("Un item n'est plus proposé par la boutique."));
    case BarterCartRefusal::kSameItemTwice:
      return Printed(ro::core::Tr("%s figure deux fois dans le panier."), name.c_str());
    case BarterCartRefusal::kGearAlreadyInCart:
      return GameOr(words, TradeWord::kBuyOneAtATime,
                    ro::core::Tr("Un équipement s'échange un à la fois."));
    case BarterCartRefusal::kGearOneAtATime:
      return GameOr(words, TradeWord::kEquipOnlyOne,
                    ro::core::Tr("Un équipement s'échange un à la fois."));
    case BarterCartRefusal::kOverLineLimit: {
      std::string said =
          ro::vfs::TradeTextWith(words, TradeWord::kLimitBuyItem, ro::sim::kBarterMostPerLine);
      return said.empty() ? Printed(ro::core::Tr("Pas plus de %lld exemplaires d'un même item à "
                                                 "la fois."),
                                    static_cast<long long>(ro::sim::kBarterMostPerLine))
                          : said;
    }
    case BarterCartRefusal::kOutOfStock:
      return ro::core::Tr("Plus que le stock restant.");
    case BarterCartRefusal::kOverWeight:
      return GameOr(words, TradeWord::kOverWeight, ro::core::Tr("Trop lourd pour le personnage."));
    case BarterCartRefusal::kCurrencyWorn:
      return GameOr(words, TradeWord::kBarterEquipped,
                    Printed(ro::core::Tr("%s est porté : ôtez-le d'abord."), name.c_str()));
    case BarterCartRefusal::kCurrencyFavorite:
      return Printed(ro::core::Tr("%s est dans les favoris, que l'échange ne prend pas."),
                     name.c_str());
    case BarterCartRefusal::kNotEnoughCurrency:
      return Printed(ro::core::Tr("Pas assez de %s."), name.c_str());
    case BarterCartRefusal::kNotEnoughZeny:
      return ro::core::Tr("Zeny insuffisant.");
  }
  return {};
}

// ── LA PLUS GRANDE QUANTITÉ ─────────────────────────────────────────────────

std::uint32_t BarterMostAddable(const ro::sim::Barter& barter, const ro::sim::BarterItem& item,
                                const ro::sim::Inventory& inventory,
                                const ro::sim::PlayerStatus& status) {
  const auto passes = [&](std::uint32_t amount) {
    return ro::sim::CheckBarterAddition(barter, item.shop_index, amount, inventory, status)
        .passed();
  };
  if (!passes(1)) {
    return 0;
  }
  const std::uint32_t carted = barter.InCart(item.shop_index);
  if (!ro::sim::ItemTypeStacks(static_cast<std::uint8_t>(item.type))) {
    return 1;
  }
  std::uint64_t ceiling =
      carted >= ro::sim::kBarterMostPerLine ? 0 : ro::sim::kBarterMostPerLine - carted;
  if (!item.unlimited()) {
    ceiling = std::min<std::uint64_t>(ceiling, barter.RemainingStock(item));
  }
  for (const ro::sim::BarterCurrency& currency : item.currencies) {
    if (currency.amount > 0) {
      ceiling = std::min<std::uint64_t>(
          ceiling,
          ro::sim::HoldBarterCurrency(inventory, currency.item_id, currency.refine_level).usable /
              currency.amount);
    }
  }
  auto high = static_cast<std::uint32_t>(ceiling);
  if (high <= 1 || passes(high)) {
    return std::max<std::uint32_t>(high, 1);
  }
  // `low` passe, `high` non.
  std::uint32_t low = 1;
  while (high - low > 1) {
    const std::uint32_t middle = low + (high - low) / 2;
    (passes(middle) ? low : high) = middle;
  }
  return low;
}

// ── LA VUE ──────────────────────────────────────────────────────────────────

const ro::ui::BarterView& BarterViewComposer::Compose(const BarterScene& scene) {
  const ro::sim::Barter& barter = scene.barter;
  view_ = ro::ui::BarterView{};
  view_.expanded = barter.generation() == ro::sim::BarterGeneration::kExpanded;
  title_ = std::string(scene.title);
  view_.title = title_;
  list_title_ = std::string(ro::vfs::TradeText(scene.words, TradeWord::kBarterList));
  view_.list_title = list_title_;

  // LE ZENY NE SE MONTRE QUE S'IL SE DÉPENSE : un troc simple n'en prend jamais.
  purse_line_.clear();
  const bool zeny_spent = std::ranges::any_of(
      barter.items(), [](const ro::sim::BarterItem& item) { return item.zeny > 0; });
  if (zeny_spent && scene.status.zeny != ro::sim::kUnknownStat) {
    purse_line_ = Printed(ro::core::Tr("Zeny : %s"),
                          ro::ui::TradeAmountText(scene.status.zeny, kZenyUnit).c_str());
  }
  view_.purse_line = purse_line_;

  ComposeOffers(scene);
  ComposeCart(scene);
  return view_;
}

void BarterViewComposer::ComposeOffers(const BarterScene& scene) {
  const ro::sim::Barter& barter = scene.barter;
  const std::span<const ro::sim::BarterItem> items = barter.items();

  // ⚠ D'ABORD TOUS LES TEXTES ET TOUS LES PRIX, ENSUITE LES VUES qui pointent
  // dedans : un vecteur qui grandit déplace ce qu'il tient.
  names_.clear();
  prices_.clear();
  refusals_.clear();
  offers_.clear();
  std::vector<std::size_t> first_price;
  first_price.reserve(items.size());
  for (const ro::sim::BarterItem& item : items) {
    names_.push_back(scene.looks.name_of(item.item_id, RefineShown(item.refine_level)));
    first_price.push_back(prices_.size());
    for (const ro::sim::BarterCurrency& currency : item.currencies) {
      const ro::sim::BarterCurrencyHolding holding =
          ro::sim::HoldBarterCurrency(scene.inventory, currency.item_id, currency.refine_level);
      ro::ui::BarterPriceView price;
      price.item_id = currency.item_id;
      price.refine_level = currency.refine_level;
      price.amount = currency.amount;
      price.held = static_cast<std::int64_t>(holding.usable);
      price.may_take_higher_refine = holding.may_take_higher_refine;
      price.item.item_id = currency.item_id;
      price.item.icon = scene.looks.icon_of(currency.item_id);
      prices_.push_back(price);
      names_.push_back(scene.looks.name_of(currency.item_id, currency.refine_level));
    }

    ro::ui::BarterOfferView offer;
    offer.item.item_id = item.item_id;
    offer.item.icon = scene.looks.icon_of(item.item_id);
    offer.shop_index = item.shop_index;
    offer.stock = item.unlimited() ? ro::ui::kBarterNoStockLimit
                                   : static_cast<std::int64_t>(barter.RemainingStock(item));
    offer.weight_tenths = item.weight;
    offer.zeny = item.zeny;
    offer.most = static_cast<int>(BarterMostAddable(barter, item, scene.inventory, scene.status));
    if (offer.most == 0) {
      const ro::sim::BarterCartCheck check =
          ro::sim::CheckBarterAddition(barter, item.shop_index, 1, scene.inventory, scene.status);
      refusals_.push_back(
          BarterRefusalText(check.refusal, scene.looks.name_of(check.item_id, 0), scene.words));
    } else {
      refusals_.emplace_back();
    }
    offers_.push_back(offer);
  }

  // Les noms se rangent article, puis ses devises : on les relit dans cet ordre.
  std::size_t name = 0;
  for (std::size_t index = 0; index < offers_.size(); ++index) {
    const std::size_t count = items[index].currencies.size();
    offers_[index].item.name = names_[name++];
    for (std::size_t rank = 0; rank < count; ++rank) {
      prices_[first_price[index] + rank].item.name = names_[name++];
    }
    offers_[index].prices =
        std::span<const ro::ui::BarterPriceView>(prices_).subspan(first_price[index], count);
    offers_[index].refusal = refusals_[index];
  }
  view_.offers = offers_;
}

void BarterViewComposer::ComposeCart(const BarterScene& scene) {
  const ro::sim::Barter& barter = scene.barter;
  cart_.clear();
  std::int64_t weight_tenths = 0;
  for (const ro::sim::BarterCartLine& line : barter.cart()) {
    const ro::sim::BarterItem* item = barter.FindItem(line.shop_index);
    std::uint32_t more = 0;
    if (item != nullptr) {
      more = BarterMostAddable(barter, *item, scene.inventory, scene.status);
      weight_tenths += static_cast<std::int64_t>(item->weight) * line.amount;
    }
    const std::uint64_t most = std::uint64_t{line.amount} + more;
    cart_.push_back(ro::ui::BarterCartLineView{
        .shop_index = line.shop_index,
        .amount =
            static_cast<int>(std::min<std::uint64_t>(line.amount, ro::sim::kBarterMostPerLine)),
        .most = static_cast<int>(std::min<std::uint64_t>(most, ro::sim::kBarterMostPerLine))});
  }
  view_.cart = cart_;
  view_.cart_seq = barter.cart_seq();

  cart_weight_.clear();
  blocker_text_.clear();
  blockers_.clear();
  if (!cart_.empty()) {
    cart_weight_ =
        Printed(ro::core::Tr("+%lld poids"), static_cast<long long>(weight_tenths / kWeightTenths));
    const ro::sim::BarterCartCheck check =
        ro::sim::CheckBarterCart(barter, scene.inventory, scene.status);
    if (!check.passed()) {
      blocker_text_ =
          BarterRefusalText(check.refusal, scene.looks.name_of(check.item_id, 0), scene.words);
      blockers_.push_back(blocker_text_);
    }
  }
  view_.cart_weight = cart_weight_;
  view_.blockers = blockers_;
}

}  // namespace moonclient
