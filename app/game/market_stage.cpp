// Le Marché dans la boucle. Voir l'en-tête.

#include "game/market_stage.h"

#include <array>
#include <cstdio>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/search_key.h"
#include "ro/net/market.h"
#include "ro/render/game_images.h"
#include "ro/render/skin_pieces.h"

#include "game/store_search_catalogue.h"
#include "game/store_search_grade.h"
#include "items/inventory_view.h"

namespace moonclient {
namespace {

constexpr char kCarriageReturn = '\r';
// La place d'une ligne composée par `snprintf`.
constexpr std::size_t kLineBytes = 160;
// Les lettres des grades, du premier au quatrième.
constexpr std::string_view kGradeLetters = "DCBA";

std::string_view ShownShopName(std::string_view name) {
  return name.substr(0, name.find(kCarriageReturn));
}

ro::ui::SkinImage GradeBadgeOf(std::uint8_t grade, ro::render::GameImages* images) {
  const std::string path = StoreSearchGradeBadgePath(grade);
  if (path.empty() || images == nullptr) {
    return {};
  }
  const ro::render::IconTexture& badge =
      images->GetOurs(std::string(ro::render::kSkinRoot).append(path));
  if (!badge.usable()) {
    return {};
  }
  return ro::ui::SkinImage{.id = static_cast<ImTextureID>(badge.texture.name()),
                           .width = static_cast<float>(badge.width_px),
                           .height = static_cast<float>(badge.height_px)};
}

ro::sim::MarketColumn ColumnOf(int column) {
  switch (column) {
    case ro::ui::kMarketItemColumn:
      return ro::sim::MarketColumn::kItem;
    case ro::ui::kMarketAmountColumn:
      return ro::sim::MarketColumn::kAmount;
    case ro::ui::kMarketShopColumn:
      return ro::sim::MarketColumn::kShop;
    case ro::ui::kMarketMapColumn:
      return ro::sim::MarketColumn::kMap;
    default:
      break;
  }
  return ro::sim::MarketColumn::kPrice;
}

ro::sim::MarketKind KindOfTab(int tab) {
  switch (tab) {
    case ro::ui::kMarketBuyTab:
      return ro::sim::MarketKind::kBuying;
    case ro::ui::kMarketNpcTab:
      return ro::sim::MarketKind::kNpc;
    default:
      break;
  }
  return ro::sim::MarketKind::kVending;
}

ro::sim::MarketCategory CategoryOf(int category) {
  if (category < 0 || static_cast<std::size_t>(category) >= ro::sim::kMarketCategoryCount) {
    return ro::sim::MarketCategory::kAny;
  }
  return static_cast<ro::sim::MarketCategory>(category);
}

std::string FaultText(ro::sim::MarketFault fault) {
  switch (fault) {
    case ro::sim::MarketFault::kUnknownItem:
      return ro::core::Tr("Aucun item ne porte ce nom.");
    case ro::sim::MarketFault::kTooManyItems:
      return ro::core::Tr("Trop d'items portent ce nom : précisez la recherche.");
    case ro::sim::MarketFault::kBadPrice:
      return ro::core::Tr("Fourchette de prix illisible.");
    case ro::sim::MarketFault::kBadRefine:
      return ro::core::Tr("Le raffinement minimum dépasse le maximum.");
    case ro::sim::MarketFault::kNone:
      break;
  }
  return {};
}

std::string RefusalText(ro::sim::MarketStatus status) {
  switch (status) {
    case ro::sim::MarketStatus::kTooSoon:
      return ro::core::Tr("Trop de demandes : patientez un instant avant de recommencer.");
    case ro::sim::MarketStatus::kStoreGone:
      return ro::core::Tr("Cette échoppe n'est plus ouverte.");
    case ro::sim::MarketStatus::kBadRequest:
      return ro::core::Tr("Le serveur a refusé la demande.");
    case ro::sim::MarketStatus::kOk:
    case ro::sim::MarketStatus::kOpen:
      break;
  }
  return {};
}

std::string PlaceOf(const ro::sim::MarketOffer& offer) {
  std::array<char, kLineBytes> text{};
  std::snprintf(text.data(), text.size(), "%s %d,%d", offer.map_name.c_str(), offer.cell_x,
                offer.cell_y);
  return text.data();
}

}  // namespace

std::vector<std::string> MarketDetailsOf(const ro::sim::MarketOffer& offer, const TradeItems& items,
                                         const ro::vfs::RandomOptionNames* options) {
  std::vector<std::string> lines;
  std::array<char, kLineBytes> text{};
  if (!ro::sim::ItemIsForged(offer.item)) {
    for (const std::uint32_t card : offer.item.cards) {
      if (card == 0) {
        continue;
      }
      std::snprintf(text.data(), text.size(), ro::core::Tr("Carte : %s"),
                    items.NameOf(card).c_str());
      lines.emplace_back(text.data());
    }
  }
  if (offer.item.grade > 0 && offer.item.grade <= kGradeLetters.size()) {
    std::snprintf(text.data(), text.size(), ro::core::Tr("Grade : %c"),
                  kGradeLetters[offer.item.grade - 1U]);
    lines.emplace_back(text.data());
  }
  for (const ro::sim::ItemOption& option : offer.item.options) {
    if (!option.filled()) {
      continue;
    }
    std::string shown =
        options != nullptr ? options->Format(option.id, option.value) : std::string();
    if (shown.empty()) {
      std::snprintf(text.data(), text.size(), ro::core::Tr("Option %d : %d"),
                    static_cast<int>(option.id), static_cast<int>(option.value));
      shown = text.data();
    }
    lines.push_back(std::move(shown));
  }
  return lines;
}

void MarketStage::StartSession() {
  market_ = ro::sim::Market();
  asked_open_ = false;
  toggle_asked_ = false;
  asked_opening_ = {};
  comparing_.reset();
  go_to_.reset();
  hovered_ = {};
  described_.reset();
  open_seen_ = 0;
  refusal_seen_ = 0;
  search_seen_ = 0;
  status_.clear();
  store_status_.clear();
  order_.clear();
}

std::optional<MarketDestination> MarketStage::TakeGoTo() noexcept {
  return std::exchange(go_to_, std::nullopt);
}

std::optional<TradeItemFocus> MarketStage::TakeDescribed() noexcept {
  return std::exchange(described_, std::nullopt);
}

const ro::sim::MarketOffer* MarketStage::OfferAt(int row, bool in_store) const {
  const std::vector<std::size_t>& ranks = in_store ? store_ranks_ : page_ranks_;
  const std::span<const ro::sim::MarketOffer> offers =
      in_store ? market_.store().offers() : market_.search().offers();
  if (row < 0 || static_cast<std::size_t>(row) >= ranks.size()) {
    return nullptr;
  }
  const std::size_t rank = ranks[static_cast<std::size_t>(row)];
  return rank < offers.size() ? &offers[rank] : nullptr;
}

void MarketStage::Note(int row, bool in_store, bool hovered, bool described,
                       ro::ui::ChatLinkGesture gesture) {
  const ro::sim::MarketOffer* offer = OfferAt(row, in_store);
  if (offer == nullptr || offer->item.item_id == 0) {
    return;
  }
  // L'OFFRE SE DÉCRIT PAR CE QU'ELLE PORTE : raffinement, cartes et options
  // voyagent avec elle, jusque dans l'infobulle.
  const TradeItemFocus focus{
      .item_id = offer->item.item_id, .link = ItemLinkOf(offer->item), .gesture = gesture};
  if (hovered) {
    hovered_ = focus;
  }
  if (described) {
    described_ = focus;
  }
}

void MarketStage::Advance(MarketWindows& windows, const TradeContext& context) {
  hovered_ = {};
  // UNE OUVERTURE DU SERVEUR — un catalogue, `@shopsearch` — ouvre la fenêtre
  // comme le bouton du menu, la saisie posée sur ce qu'elle porte.
  if (open_seen_ != market_.open_seq()) {
    open_seen_ = market_.open_seq();
    AskOpenOn(MarketOpening{.query = market_.open_query()});
  }
  // LA TOUCHE OU L'ICÔNE : une fenêtre ouverte se ferme, sinon elle s'ouvre.
  const bool toggled = std::exchange(toggle_asked_, false);
  if (std::exchange(windows.toggle_asked, false) || toggled) {
    if (windows.state.open) {
      windows.state.open = false;
      windows.state.store_open = false;
    } else {
      AskOpen();
    }
  }
  OpenAsked(windows, context);
  Follow(windows, context);
  SayNews();
  if (!windows.state.open) {
    windows.view = nullptr;
    return;
  }
  Compose(windows.state, context);
  windows.view = &view_;
}

void MarketStage::OpenAsked(MarketWindows& windows, const TradeContext& context) {
  if (!std::exchange(asked_open_, false)) {
    return;
  }
  MarketOpening opening = std::exchange(asked_opening_, {});
  ro::ui::OpenMarketOn(windows.state, opening.query);
  // UNE COMPARAISON s'ouvre sur l'onglet de sa sorte.
  comparing_ = std::move(opening.compare);
  if (comparing_.has_value()) {
    windows.state.tab = MarketTabOf(comparing_->kind);
  }
  status_.clear();
  Search(windows, context);
}

void MarketStage::Follow(MarketWindows& windows, const TradeContext& context) {
  const ro::ui::MarketAction answer = std::exchange(windows.answer, {});
  if (answer.close) {
    windows.state.store_open = false;
    return;
  }
  Note(answer.hovered_row, answer.hovered_in_store, true, false);
  Note(answer.describe_row, answer.describe_in_store, false, true, answer.item_gesture);
  MarketRoute route =
      RouteMarketAnswer(answer, market_.search().offers(), page_ranks_, market_.search().kind());
  // UNE RECHERCHE NEUVE oublie la comparaison ; « Actualiser » la redemande.
  if (route.search) {
    windows.state.page = 1;
    windows.state.expanded_row = ro::ui::kMarketNoRow;
    comparing_.reset();
  }
  if (route.search || route.refresh) {
    Search(windows, context);
  }
  if (route.go_to.has_value()) {
    go_to_ = std::move(route.go_to);
  }
  // « VOIR » DEMANDE LE CONTENU DE L'ÉCHOPPE : sa fenêtre s'ouvre, et se
  // remplit à la réponse.
  if (route.store_query.has_value()) {
    store_title_ = std::string(ShownShopName(route.store_offer->shop_name));
    store_status_.clear();
    windows.state.store_open = true;
    if (auto sent = ro::net::RequestMarket(context.link, market_, std::move(*route.store_query));
        !sent) {
      ro::core::LogError("marché : {}", ro::core::Describe(sent.error()));
    }
  }
}

void MarketStage::Search(MarketWindows& windows, const TradeContext& context) {
  const ro::ui::MarketWindowState& state = windows.state;
  if (comparing_.has_value()) {
    status_.clear();
    if (auto sent = ro::net::RequestMarket(
            context.link, market_,
            ro::sim::MarketCompareQuery(comparing_->kind, comparing_->owner_aid));
        !sent) {
      ro::core::LogError("marché : {}", ro::core::Describe(sent.error()));
    }
    return;
  }
  std::vector<ro::sim::StoreSearchCatalogueEntry> catalogue;
  if (const ro::vfs::ItemTable* table = context.items.table(); table != nullptr) {
    catalogue = StoreSearchCatalogueOf(*table, context.words, {});
  }
  const ro::sim::MarketForm form{.kind = KindOfTab(state.tab),
                                 .text = state.query.data(),
                                 .category = CategoryOf(state.category),
                                 .refine_min = state.refine_min,
                                 .refine_max = state.refine_max,
                                 .min_price = state.min_price.data(),
                                 .max_price = state.max_price.data(),
                                 .with_cards = state.with_cards,
                                 .with_options = state.with_options};
  ro::sim::MarketPlan plan = ro::sim::PlanMarketSearch(form, catalogue);
  if (plan.fault != ro::sim::MarketFault::kNone) {
    status_ = FaultText(plan.fault);
    return;
  }
  status_.clear();
  if (auto sent = ro::net::RequestMarket(context.link, market_, std::move(plan.query)); !sent) {
    ro::core::LogError("marché : {}", ro::core::Describe(sent.error()));
  }
}

void MarketStage::SayNews() {
  if (refusal_seen_ == market_.refusal_seq()) {
    return;
  }
  refusal_seen_ = market_.refusal_seq();
  // Le refus s'écrit au pied de la fenêtre qu'il concerne.
  std::string& said = market_.refusal_for_store() ? store_status_ : status_;
  said = RefusalText(market_.refusal());
}

void MarketStage::ComposeRows(std::span<const ro::sim::MarketOffer> offers,
                              std::span<const std::size_t> ranks, std::size_t first_name,
                              const TradeContext& context,
                              std::vector<ro::ui::MarketRowView>& rows) {
  rows.clear();
  rows.reserve(ranks.size());
  // L'ÉCHOPPE COMPARÉE reste dans la liste, marquée : ses prix sont la référence.
  const std::uint32_t compared_aid = comparing_.has_value() ? comparing_->owner_aid : 0;
  for (std::size_t row = 0; row < ranks.size(); ++row) {
    const ro::sim::MarketOffer& offer = offers[ranks[row]];
    rows.push_back(ro::ui::MarketRowView{
        .item = ro::ui::TradeItemView{.item_id = offer.item.item_id,
                                      .name = names_[first_name + row],
                                      .icon = context.items.IconOf(offer.item.item_id),
                                      .damaged = offer.item.damaged},
        .grade_badge = GradeBadgeOf(offer.item.grade, context.images),
        .price = prices_[first_name + row],
        .amount = amounts_[first_name + row],
        .shop_name = ShownShopName(offer.shop_name),
        .place = places_[first_name + row],
        .autotrade = offer.autotrade,
        .compared = compared_aid != 0 && offer.owner_aid == compared_aid,
        .details = details_[first_name + row]});
  }
}

void MarketStage::Compose(const ro::ui::MarketWindowState& state, const TradeContext& context) {
  const std::span<const ro::sim::MarketOffer> offers = market_.search().offers();
  const ro::sim::MarketSort sort{.column = ColumnOf(state.sort_column),
                                 .descending = state.sort_descending};
  // L'ORDRE SE REFAIT quand une réponse arrive ou que le tri change : les clés
  // de nom se calculent alors pour toutes les offres.
  if (search_seen_ != market_.search().complete_seq() || sorted_by_.column != sort.column ||
      sorted_by_.descending != sort.descending || order_.size() != offers.size()) {
    search_seen_ = market_.search().complete_seq();
    sorted_by_ = sort;
    std::vector<std::string> keys;
    keys.reserve(offers.size());
    for (const ro::sim::MarketOffer& offer : offers) {
      keys.push_back(ro::core::SearchKey(context.items.NameOf(offer.item, context.creators)));
    }
    order_ = ro::sim::MarketOrder(offers, keys, sort);
  }
  const std::span<const std::size_t> page = ro::sim::MarketPage(order_, state.page);
  page_ranks_.assign(page.begin(), page.end());
  const std::span<const ro::sim::MarketOffer> store = market_.store().offers();
  store_ranks_.clear();
  for (std::size_t rank = 0; rank < store.size(); ++rank) {
    store_ranks_.push_back(rank);
  }
  // LES TEXTES D'ABORD, tous, la page puis l'échoppe : les lignes gardent des
  // vues dessus, et un vecteur qui grandit déplace ses textes.
  const std::size_t text_count = page_ranks_.size() + store_ranks_.size();
  for (std::vector<std::string>* texts : {&names_, &places_, &prices_, &amounts_}) {
    texts->clear();
    texts->reserve(text_count);
  }
  details_.clear();
  details_.reserve(text_count);
  const auto add_texts = [&](std::span<const ro::sim::MarketOffer> from,
                             std::span<const std::size_t> ranks, ro::sim::MarketKind kind) {
    for (const std::size_t rank : ranks) {
      const ro::sim::MarketOffer& offer = from[rank];
      names_.push_back(context.items.NameOf(offer.item, context.creators));
      places_.push_back(PlaceOf(offer));
      prices_.push_back(MarketPriceText(offer, offer.currency_item != 0
                                                   ? context.items.NameOf(offer.currency_item)
                                                   : std::string()));
      amounts_.push_back(MarketAmountText(offer, kind));
      details_.push_back(MarketDetailsOf(offer, context.items, options_));
    }
  };
  add_texts(offers, page_ranks_, market_.search().kind());
  add_texts(store, store_ranks_, market_.store().kind());
  ComposeRows(offers, page_ranks_, 0, context, rows_);
  ComposeRows(store, store_ranks_, page_ranks_.size(), context, store_rows_);
  std::array<char, kLineBytes> text{};
  if (market_.search().waiting()) {
    summary_ = ro::core::Tr("Recherche en cours…");
  } else if (comparing_.has_value() && !comparing_->owner_name.empty()) {
    std::snprintf(text.data(), text.size(), ro::core::Tr("%d offres, comparées à l'échoppe de %s"),
                  static_cast<int>(offers.size()), comparing_->owner_name.c_str());
    summary_ = text.data();
  } else if (market_.search().truncated()) {
    std::snprintf(text.data(), text.size(),
                  ro::core::Tr("%d offres, les meilleures seulement : précisez la recherche."),
                  static_cast<int>(offers.size()));
    summary_ = text.data();
  } else {
    std::snprintf(text.data(), text.size(), ro::core::Tr("%d offres"),
                  static_cast<int>(offers.size()));
    summary_ = text.data();
  }
  view_ = ro::ui::MarketView{
      .rows = rows_,
      .page_count = ro::sim::MarketPageCount(offers.size()),
      .summary = summary_,
      .status = status_,
      .busy = market_.search().waiting(),
      .amount_shown = MarketAmountColumnShown(offers, market_.search().kind()),
      .store_title = store_title_,
      .store_status = market_.store().waiting() ? std::string_view(ro::core::Tr("Chargement…"))
                                                : std::string_view(store_status_),
      .store_rows = store_rows_,
      .store_amount_shown = MarketAmountColumnShown(store, market_.store().kind())};
}

}  // namespace moonclient
