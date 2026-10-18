// La recherche d'échoppes dans la boucle. Voir l'en-tête.

#include "game/store_search_stage.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include "ro/core/log.h"
#include "ro/net/store_search.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/trade_words.h"

#include "game/store_search_catalogue.h"
#include "items/inventory_view.h"

namespace moonclient {
namespace {

using ro::vfs::TradeWord;

constexpr char kCarriageReturn = '\r';

// Le mot d'un refus de la saisie, avec les mots du jeu.
TradeWord WordOfFault(ro::sim::StoreSearchFault fault) {
  switch (fault) {
    case ro::sim::StoreSearchFault::kNoItemName:
      return TradeWord::kSearchNoItemName;
    case ro::sim::StoreSearchFault::kUnknownItem:
      return TradeWord::kSearchUnknownItem;
    case ro::sim::StoreSearchFault::kUnknownCard:
      return TradeWord::kSearchUnknownCard;
    case ro::sim::StoreSearchFault::kTooManyIds:
      return TradeWord::kSearchTooMany;
    case ro::sim::StoreSearchFault::kBadPrice:
    case ro::sim::StoreSearchFault::kNone:
      break;
  }
  return TradeWord::kSearchBadPrice;
}

// LE REFUS DU SERVEUR QUI S'ÉCRIT AU PIED, ou rien : les deux autres se disent
// dans une boîte.
std::optional<TradeWord> FooterWordOf(std::uint8_t reason) {
  switch (reason) {
    case ro::sim::kStoreSearchNothingFound:
      return TradeWord::kSearchNothingFound;
    case ro::sim::kStoreSearchTooManyResults:
      return TradeWord::kSearchTooMany;
    case ro::sim::kStoreSearchNoUsesLeft:
      return TradeWord::kSearchNoUsesLeft;
    default:
      return std::nullopt;
  }
}

std::optional<TradeWord> NoticeWordOf(std::uint8_t reason) {
  switch (reason) {
    case ro::sim::kStoreSearchTooSoon:
      return TradeWord::kSearchTooSoon;
    case ro::sim::kStoreSearchStoreUnavailable:
      return TradeWord::kNoSaleInformation;
    default:
      return std::nullopt;
  }
}

std::string_view ShownShopName(std::string_view name) {
  return name.substr(0, name.find(kCarriageReturn));
}

}  // namespace

void StoreSearchStage::StartSession() {
  search_ = ro::sim::StoreSearch();
  hovered_ = {};
  described_.reset();
  answer_seen_ = 0;
  failure_said_ = 0;
  status_.clear();
}

std::optional<TradeItemFocus> StoreSearchStage::TakeDescribed() noexcept {
  return std::exchange(described_, std::nullopt);
}

const ro::sim::StoreSearchResult* StoreSearchStage::ResultAt(
    const ro::ui::StoreSearchWindowState& state, int row) const {
  const std::span<const ro::sim::StoreSearchResult> page =
      ro::sim::StoreSearchPage(search_.results(), state.page);
  if (row < 0 || static_cast<std::size_t>(row) >= page.size()) {
    return nullptr;
  }
  return &page[static_cast<std::size_t>(row)];
}

void StoreSearchStage::Note(const ro::ui::StoreSearchWindowState& state, int row, bool hovered,
                            bool described, ro::ui::ChatLinkGesture gesture) {
  const ro::sim::StoreSearchResult* result = ResultAt(state, row);
  if (result == nullptr || result->item.item_id == 0) {
    return;
  }
  // L'OFFRE SE DÉCRIT ET SE LIE PAR CE QU'ELLE PORTE : raffinement et cartes
  // voyagent avec elle, jusque dans le lien de la saisie.
  const TradeItemFocus focus{
      .item_id = result->item.item_id, .link = ItemLinkOf(result->item), .gesture = gesture};
  if (hovered) {
    hovered_ = focus;
  }
  if (described) {
    described_ = focus;
  }
}

void StoreSearchStage::Advance(StoreSearchWindows& windows, const TradeContext& context) {
  hovered_ = {};
  Follow(windows, context);
  SayNews(windows, context);
  if (!search_.open() || !windows.state.open) {
    windows.view = nullptr;
    return;
  }
  Compose(windows.state, context);
  windows.view = &view_;
}

void StoreSearchStage::Follow(StoreSearchWindows& windows, const TradeContext& context) {
  ro::ui::StoreSearchWindowState& state = windows.state;
  const ro::ui::StoreSearchAction answer = std::exchange(windows.answer, {});
  switch (ro::sim::NextStoreSearchWindowStep(
      {.search_open = search_.open(),
       .shows_current_opening = state.opening_seen == search_.opening_seq(),
       .window_open = state.open,
       .close_asked = answer.close})) {
    case ro::sim::StoreSearchWindowStep::kHide:
      state.open = false;
      return;
    case ro::sim::StoreSearchWindowStep::kReopen:
      ro::ui::OpenStoreSearchOn(state, search_.opening_seq());
      state.first_page_seen = search_.first_page_seq();
      answer_seen_ = search_.answer_seq();
      failure_said_ = search_.failure_seq();
      status_.clear();
      return;
    case ro::sim::StoreSearchWindowStep::kSendClose:
      // La fermeture se dit, et le serveur oublie résultats et essais. ⚠ LE
      // MODÈLE SE FERME MÊME SI L'ENVOI ÉCHOUE : rien ne se répète.
      state.open = false;
      status_.clear();
      if (auto sent = ro::net::RequestStoreSearchClose(context.link, search_); !sent) {
        ro::core::LogError("recherche d'échoppes : {}", ro::core::Describe(sent.error()));
      }
      return;
    case ro::sim::StoreSearchWindowStep::kFollow:
      break;
  }
  Note(state, answer.hovered_row, true, false);
  Note(state, answer.describe_row, false, true, answer.item_gesture);
  if (answer.search) {
    Search(state, context);
  }
  // LA PAGE SUIVANTE NE PART QUE DEPUIS LA DERNIÈRE PAGE PLEINE : la fenêtre a
  // déjà avancé d'une page, d'où le rang précédent.
  if (answer.next_page && ro::sim::StoreSearchMayAskNextPage(
                              search_.results().size(), state.page - 1, search_.more_on_server())) {
    if (auto sent = ro::net::RequestStoreSearchNextPage(context.link, search_); !sent) {
      ro::core::LogError("recherche d'échoppes : {}", ro::core::Describe(sent.error()));
    }
  }
  // ⚠ LA LIGNE DÉSIGNÉE DOIT ÊTRE TOUJOURS LA MÊME : une page arrivée entre le
  // double-clic et sa confirmation peut avoir mis une autre échoppe à ce rang.
  if (const ro::sim::StoreSearchResult* clicked = ResultAt(state, answer.clicked.row);
      clicked != nullptr && clicked->store_id == answer.clicked.store_id &&
      clicked->owner_aid == answer.clicked.owner_aid) {
    // ⚠ COPIÉ AVANT L'ENVOI : le résultat vit dans le modèle que l'envoi touche.
    const ro::sim::StoreSearchResult result = *clicked;
    if (auto sent = ro::net::RequestStoreSearchClick(context.link, search_, result); !sent) {
      ro::core::LogError("recherche d'échoppes : {}", ro::core::Describe(sent.error()));
    }
  }
}
void StoreSearchStage::Search(const ro::ui::StoreSearchWindowState& state,
                              const TradeContext& context) {
  std::vector<ro::sim::StoreSearchCatalogueEntry> catalogue;
  if (const ro::vfs::ItemTable* table = context.items.table(); table != nullptr) {
    catalogue = StoreSearchCatalogueOf(*table, context.words, cards_);
  }
  const ro::sim::StoreSearchForm form{.kind = state.kind == ro::ui::kStoreSearchSellKind
                                                  ? ro::sim::StoreSearchKind::kVending
                                                  : ro::sim::StoreSearchKind::kBuying,
                                      .item_name = state.item_name.data(),
                                      .card_name = state.card_name.data(),
                                      .min_price = state.min_price.data(),
                                      .max_price = state.max_price.data(),
                                      .similar = state.similar};
  const ro::sim::StoreSearchPlan plan = ro::sim::PlanStoreSearch(form, catalogue);
  if (plan.fault != ro::sim::StoreSearchFault::kNone) {
    status_ = std::string(ro::vfs::TradeText(context.words, WordOfFault(plan.fault)));
    return;
  }
  if (auto sent = ro::net::RequestStoreSearch(context.link, search_, plan); !sent) {
    ro::core::LogError("recherche d'échoppes : {}", ro::core::Describe(sent.error()));
  }
}

void StoreSearchStage::SayNews(StoreSearchWindows& windows, const TradeContext& context) {
  // UNE RECHERCHE NEUVE A RÉPONDU, ou n'a rien trouvé : retour à la première
  // page.
  if (windows.state.first_page_seen != search_.first_page_seq()) {
    windows.state.first_page_seen = search_.first_page_seq();
    windows.state.page = 1;
    windows.state.hovered_row = ro::ui::kStoreSearchNoRow;
  }
  // UNE RÉPONSE DU SERVEUR efface le pied ; le refus qui tient encore s'y écrit.
  if (answer_seen_ != search_.answer_seq()) {
    answer_seen_ = search_.answer_seq();
    status_.clear();
    if (const std::optional<std::uint8_t> standing = search_.standing_failure();
        standing.has_value()) {
      if (const std::optional<TradeWord> word = FooterWordOf(*standing); word.has_value()) {
        status_ = std::string(ro::vfs::TradeText(context.words, *word));
      }
    }
  }
  // LES DEUX REFUS QUI SE DISENT DANS UNE BOÎTE, même si une page suit.
  if (failure_said_ != search_.failure_seq()) {
    failure_said_ = search_.failure_seq();
    if (const std::optional<TradeWord> word = NoticeWordOf(search_.failure()); word.has_value()) {
      windows.notice.text = std::string(ro::vfs::TradeText(context.words, *word));
      windows.notice.asked = false;
    }
  }
}

void StoreSearchStage::Compose(const ro::ui::StoreSearchWindowState& state,
                               const TradeContext& context) {
  const ro::vfs::ItemWords& words = context.words;
  const std::span<const ro::sim::StoreSearchResult> page =
      ro::sim::StoreSearchPage(search_.results(), state.page);
  // LES NOMS D'ABORD, tous : les lignes gardent des vues dessus.
  names_.clear();
  names_.reserve(page.size());
  for (const ro::sim::StoreSearchResult& result : page) {
    names_.push_back(context.items.NameOf(result.item, context.creators));
  }
  rows_.clear();
  rows_.reserve(page.size());
  for (std::size_t rank = 0; rank < page.size(); ++rank) {
    const ro::sim::StoreSearchResult& result = page[rank];
    rows_.push_back(ro::ui::StoreSearchRowView{
        .item = ro::ui::TradeItemView{.item_id = result.item.item_id,
                                      .name = names_[rank],
                                      .icon = context.items.IconOf(result.item.item_id)},
        .grade_badge = GradeBadgeOf(result.item.grade, context.images),
        .shop_name = ShownShopName(result.shop_name),
        .amount = result.item.amount,
        .price_zeny = result.price_zeny,
        .store_id = result.store_id,
        .owner_aid = result.owner_aid});
  }
  uses_left_ = ro::vfs::TradeTextWith(words, TradeWord::kSearchUsesLeft, search_.remaining_uses());
  view_ = ro::ui::StoreSearchView{
      .words = {.title = ro::vfs::TradeText(words, TradeWord::kSearchTitle),
                .sell = ro::vfs::TradeText(words, TradeWord::kSearchSell),
                .buy = ro::vfs::TradeText(words, TradeWord::kSearchBuy),
                .shop_name = ro::vfs::TradeText(words, TradeWord::kSearchShopName),
                .item = ro::vfs::TradeText(words, TradeWord::kSearchItem),
                .count = ro::vfs::TradeText(words, TradeWord::kSearchCount),
                .price = ro::vfs::TradeText(words, TradeWord::kSearchPrice),
                .similar = ro::vfs::TradeText(words, TradeWord::kSearchSimilar),
                .card_hint = ro::vfs::TradeText(words, TradeWord::kSearchCardHint),
                .previous_page = ro::vfs::TradeText(words, TradeWord::kSearchPreviousPage),
                .next_page = ro::vfs::TradeText(words, TradeWord::kSearchNextPage),
                .uses_left = uses_left_,
                .open_store = ro::vfs::TradeText(words, TradeWord::kSearchOpenStore)},
      .rows = rows_,
      .next_held = ro::sim::StoreSearchPageHeld(search_.results().size(), state.page),
      .next_askable = ro::sim::StoreSearchMayAskNextPage(search_.results().size(), state.page,
                                                         search_.more_on_server()),
      .click_opens_store = search_.effect() == ro::sim::StoreSearchEffect::kOpenRemotely,
      .status = status_};
}

}  // namespace moonclient
