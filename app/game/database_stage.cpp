// La base de données dans la boucle. Voir l'en-tête.

#include "game/database_stage.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/core/search_key.h"
#include "ro/net/market.h"
#include "ro/vfs/server_db_search.h"

namespace moonclient {
namespace {

constexpr std::size_t kLineBytes = 96;
// La clé de recherche d'une ligne joint ses noms : un séparateur que personne
// ne tape empêche une saisie d'enjamber deux noms.
constexpr std::string_view kKeySeparator = "\n";

[[nodiscard]] std::string Joined(std::initializer_list<std::string_view> names) {
  std::string key;
  for (const std::string_view name : names) {
    if (!key.empty()) {
      key += kKeySeparator;
    }
    key += ro::core::SearchKey(name);
  }
  return key;
}

[[nodiscard]] std::string RefusalText(ro::sim::MarketStatus status) {
  if (status == ro::sim::MarketStatus::kTooSoon) {
    return ro::core::Tr("Trop de demandes : patientez un instant avant de recommencer.");
  }
  return ro::core::Tr("Le serveur a refusé la demande.");
}

[[nodiscard]] std::string MonsterNote(const ro::vfs::ServerMonster& monster) {
  std::array<char, kLineBytes> text{};
  const auto level = static_cast<unsigned>(monster.level);
  if (monster.rank == ro::vfs::kServerMonsterMvp) {
    std::snprintf(text.data(), text.size(), ro::core::Tr("Niv. %u MVP"), level);
  } else if (monster.rank == ro::vfs::kServerMonsterBoss) {
    std::snprintf(text.data(), text.size(), ro::core::Tr("Niv. %u Boss"), level);
  } else {
    std::snprintf(text.data(), text.size(), ro::core::Tr("Niv. %u"), level);
  }
  return text.data();
}

}  // namespace

void DatabaseStage::See(ro::vfs::Vfs* vfs, const ro::vfs::ItemTable* items,
                        ro::render::ItemIcons* icons, const ro::vfs::ItemWords* words) {
  vfs_ = vfs;
  items_.See(items, icons, words);
  description_.emplace(items, words, icons);
}

std::uint32_t DatabaseStage::ShownMonster(const DatabaseWindows& windows) const {
  const std::optional<ro::ui::DatabaseTarget> shown = windows.state.history.current();
  if (!windows.state.open || db() == nullptr || !shown.has_value() ||
      shown->kind != ro::ui::DatabaseKind::kMonster) {
    return 0;
  }
  return shown->id;
}

std::optional<ro::sim::MonsterSheet> DatabaseStage::BaseSheetOf(std::uint32_t monster_id) const {
  const ro::vfs::ServerMonster* const monster = db_.known() ? db_.FindMonster(monster_id) : nullptr;
  if (monster == nullptr) {
    return std::nullopt;
  }
  return MonsterSheetOf(*monster, db_, [this](std::uint32_t item_id) { return ItemName(item_id); });
}

std::optional<ro::sim::MarketOffer> DatabaseStage::TakeGoTo() noexcept {
  return std::exchange(go_to_, std::nullopt);
}

std::string DatabaseStage::ItemName(std::uint32_t item_id) const {
  // LE NOM QUE LE JOUEUR LIT : celui de la table du client, emplacements de
  // carte compris ; celui du serveur pour un objet que le client ignore.
  if (const ro::vfs::ItemTable* table = items_.table();
      table != nullptr && table->Find(item_id) != nullptr) {
    return items_.NameOf(item_id);
  }
  if (const ro::vfs::ServerItem* item = db_.known() ? db_.FindItem(item_id) : nullptr;
      item != nullptr) {
    return item->name;
  }
  return "#" + std::to_string(item_id);
}

void DatabaseStage::OpenAsked(DatabaseWindows& windows) {
  if (std::exchange(windows.toggle_asked, false)) {
    if (windows.state.open) {
      windows.state.open = false;
    } else {
      AskOpen();
    }
  }
  if (!std::exchange(asked_open_, false)) {
    return;
  }
  ro::ui::OpenDatabaseOn(windows.state, std::exchange(asked_target_, std::nullopt));
  if (std::optional<std::string> query = std::exchange(asked_query_, std::nullopt);
      query.has_value()) {
    // `/db <texte>` : la recherche se pose dans l'onglet montré.
    std::array<char, ro::ui::kDatabaseQueryBytes>& field =
        windows.state.tab == ro::ui::kDatabaseItemTab ? windows.state.item_query
                                                      : windows.state.monster_query;
    field = {};
    std::copy_n(query->data(), std::min(query->size(), field.size() - 1), field.data());
  }
}

void DatabaseStage::Load() {
  if (!load_started_) {
    // 🔴 À LA PREMIÈRE OUVERTURE SEULEMENT : les octets se lisent ici, le YAML
    // se décode à côté de la boucle, qui ne s'arrête pas pour lui.
    load_started_ = true;
    if (vfs_ == nullptr) {
      status_ = ro::core::Tr("La base de données n'est pas disponible sans installation.");
      return;
    }
    auto texts = ro::vfs::ReadServerDbTexts(*vfs_);
    if (!texts) {
      status_ = ro::core::Tr("Les tables de la base de données sont illisibles.");
      ro::core::LogError("base de données : {}", ro::core::Describe(texts.error()));
      return;
    }
    loading_ = std::async(std::launch::async, [texts = std::move(*texts)] {
      return ro::vfs::ParseServerDb(texts.item_db, texts.mob_db, texts.context);
    });
  }
  if (!loading_.valid() ||
      loading_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
    return;
  }
  ro::core::Result<ro::vfs::ServerDb> read = loading_.get();
  if (!read) {
    status_ = ro::core::Tr("Les tables de la base de données sont illisibles.");
    ro::core::LogError("base de données : {}", ro::core::Describe(read.error()));
    return;
  }
  if (!read->known()) {
    status_ = ro::core::Tr("Les tables de la base de données sont absentes de l'installation.");
    return;
  }
  db_ = std::move(*read);
  ro::core::LogInfo("base de données : {} objets, {} monstres", db_.items().size(),
                    db_.monsters().size());
  Index();
}

void DatabaseStage::Index() {
  item_names_.clear();
  item_keys_.clear();
  for (const ro::vfs::ServerItem& item : db_.items()) {
    item_names_.push_back(ItemName(item.item_id));
    item_keys_.push_back(Joined({item_names_.back(), item.name, item.aegis_name}));
  }
  monster_keys_.clear();
  for (const ro::vfs::ServerMonster& monster : db_.monsters()) {
    monster_keys_.push_back(Joined({monster.name, monster.aegis_name}));
  }
}

void DatabaseStage::ComposeRows(const ro::ui::DatabaseWindowState& state) {
  const bool items = state.tab == ro::ui::kDatabaseItemTab;
  const std::string_view text(items ? state.item_query.data() : state.monster_query.data());
  Asked asked{.tab = state.tab, .text = std::string(text), .filters = state.filters};
  if (listed_.has_value() && *listed_ == asked) {
    return;
  }
  listed_ = std::move(asked);
  rows_.clear();
  row_notes_.clear();
  if (items) {
    const std::vector<std::size_t> found =
        ro::vfs::SearchItems(db_, item_keys_, ItemSearchOf(state.filters, text));
    row_notes_.reserve(found.size());
    for (const std::size_t rank : found) {
      row_notes_.push_back(ItemTypeLabel(db_.items()[rank].type));
    }
    for (std::size_t row = 0; row < found.size(); ++row) {
      rows_.push_back(ro::ui::DatabaseRowView{.id = db_.items()[found[row]].item_id,
                                              .name = item_names_[found[row]],
                                              .note = row_notes_[row]});
    }
  } else {
    const std::vector<std::size_t> found =
        ro::vfs::SearchMonsters(db_, monster_keys_, MonsterSearchOf(state.filters, text));
    row_notes_.reserve(found.size());
    for (const std::size_t rank : found) {
      row_notes_.push_back(MonsterNote(db_.monsters()[rank]));
    }
    for (std::size_t row = 0; row < found.size(); ++row) {
      const ro::vfs::ServerMonster& monster = db_.monsters()[found[row]];
      rows_.push_back(ro::ui::DatabaseRowView{
          .id = monster.monster_id, .name = monster.name, .note = row_notes_[row]});
    }
  }
  std::array<char, kLineBytes> text_line{};
  std::snprintf(text_line.data(), text_line.size(),
                items ? ro::core::Tr("%d items") : ro::core::Tr("%d monstres"),
                static_cast<int>(rows_.size()));
  summary_ = text_line.data();
}

void DatabaseStage::ComposeItem(std::uint32_t item_id, const DatabaseContext& context,
                                bool sellers_shown) {
  const ro::vfs::ServerItem* const item = db_.FindItem(item_id);
  if (item == nullptr) {
    view_.item = nullptr;
    return;
  }
  if (item_shown_ != item_id) {
    item_shown_ = item_id;
    facts_ = ItemFactsOf(*item);
    fact_views_.clear();
    for (const ItemFactLine& line : facts_) {
      fact_views_.push_back(ro::ui::DatabaseFact{.label = line.label, .value = line.value});
    }
    const std::vector<ro::vfs::ItemDropper> droppers = db_.DroppersOf(item_id);
    dropper_names_.clear();
    dropper_rates_.clear();
    droppers_.clear();
    for (const ro::vfs::ItemDropper& dropper : droppers) {
      const ro::vfs::ServerMonster* const monster = db_.FindMonster(dropper.monster_id);
      dropper_names_.push_back(monster != nullptr ? monster->name
                                                  : "#" + std::to_string(dropper.monster_id));
      dropper_rates_.push_back(DropRateText(dropper.rate_hundredths));
    }
    for (std::size_t rank = 0; rank < droppers.size(); ++rank) {
      const ro::vfs::ServerMonster* const monster = db_.FindMonster(droppers[rank].monster_id);
      droppers_.push_back(ro::ui::DatabaseDropperRow{
          .monster_id = droppers[rank].monster_id,
          .name = dropper_names_[rank],
          .rank = monster != nullptr ? monster->rank : ro::vfs::kServerMonsterOrdinary,
          .rate = dropper_rates_[rank],
          .mvp_reward = droppers[rank].mvp_reward});
    }
  }
  item_view_ = ro::ui::DatabaseItemView{};
  item_view_.item_id = item_id;
  const auto named =
      std::ranges::lower_bound(db_.items(), item_id, {}, &ro::vfs::ServerItem::item_id);
  item_view_.name = item_names_[static_cast<std::size_t>(named - db_.items().begin())];
  item_view_.icon = items_.IconOf(item_id);
  item_view_.facts = fact_views_;
  if (description_.has_value()) {
    description_->Refresh(context.world, item_id, ro::ui::ItemHolder::kInventory, 0);
    item_view_.description = &description_->view();
  }
  item_view_.script = item->script;
  item_view_.equip_script = item->equip_script;
  item_view_.unequip_script = item->unequip_script;
  item_view_.droppers = droppers_;
  item_view_.vote_shop = context.vote_shop;
  FollowSellers(item_id, context, sellers_shown ? SellersAsk::kOnce : SellersAsk::kNever);
  view_.item = &item_view_;
}

void DatabaseStage::FollowSellers(std::uint32_t item_id, const DatabaseContext& context,
                                  SellersAsk ask) {
  // LA RÉPONSE ARRIVÉE, rangée sous l'objet demandé — si c'est bien la sienne.
  const ro::sim::MarketAnswer& aside = context.market.aside();
  if (aside.complete() && aside.complete_seq() != answer_seen_) {
    answer_seen_ = aside.complete_seq();
    if (sellers_asked_ != 0 && aside.query_id() == sellers_query_) {
      const std::span<const ro::sim::MarketOffer> offers = aside.offers();
      sellers_[sellers_asked_].assign(offers.begin(), offers.end());
    }
  }
  if (context.market.aside_refusal_seq() != refusal_seen_) {
    refusal_seen_ = context.market.aside_refusal_seq();
    sellers_refused_ = true;
    sellers_status_ = RefusalText(context.market.aside_refusal());
  }
  ro::ui::DatabaseItemView& view = item_view_;
  if (!context.online) {
    view.sellers_phase = ro::ui::DatabaseSellersPhase::kOffline;
    return;
  }
  if (const auto known = sellers_.find(item_id); known != sellers_.end()) {
    if (seller_rows_for_ != item_id) {
      seller_rows_for_ = item_id;
      seller_lines_ =
          SellerLinesOf(known->second, [this](std::uint32_t id) { return ItemName(id); });
      seller_rows_.clear();
      for (const SellerLine& line : seller_lines_) {
        seller_rows_.push_back(
            ro::ui::DatabaseSellerRow{.npc = line.npc, .place = line.place, .price = line.price});
      }
    }
    view.sellers_phase = ro::ui::DatabaseSellersPhase::kReady;
    view.sellers = seller_rows_;
    return;
  }
  // UNE DEMANDE PAR OBJET, et une seule en vol : un refus attend « Réessayer ».
  // Elle ne part qu'onglet ouvert.
  if ((ask == SellersAsk::kOnce && sellers_asked_ != item_id) || ask == SellersAsk::kRetry) {
    sellers_asked_ = item_id;
    sellers_refused_ = false;
    if (auto sent = ro::net::RequestMarket(context.link, context.market,
                                           ro::sim::MarketNpcSellersQuery(item_id));
        !sent) {
      ro::core::LogError("base de données : {}", ro::core::Describe(sent.error()));
    } else {
      sellers_query_ = context.market.aside().query_id();
    }
  }
  view.sellers_phase = sellers_asked_ == item_id && sellers_refused_
                           ? ro::ui::DatabaseSellersPhase::kRefused
                           : ro::ui::DatabaseSellersPhase::kAsking;
  view.sellers_status = sellers_status_;
}

void DatabaseStage::Advance(DatabaseWindows& windows, const DatabaseContext& context) {
  OpenAsked(windows);
  const ro::ui::DatabaseAnswer answer = std::exchange(windows.answer, {});
  if (!windows.state.open) {
    windows.view = nullptr;
    return;
  }
  view_ = ro::ui::DatabaseView{};
  if (db_.known()) {
    if (const std::optional<ro::ui::DatabaseTarget> shown = windows.state.history.current();
        shown.has_value() && shown->kind == ro::ui::DatabaseKind::kItem) {
      if (answer.ask_sellers) {
        FollowSellers(shown->id, context, SellersAsk::kRetry);
      }
      if (answer.go_to_seller >= 0) {
        const auto known = sellers_.find(shown->id);
        if (known != sellers_.end() &&
            static_cast<std::size_t>(answer.go_to_seller) < known->second.size()) {
          go_to_ = known->second[static_cast<std::size_t>(answer.go_to_seller)];
        }
      }
    }
  }
  Load();
  view_.loading = loading_.valid();
  view_.status = status_;
  if (db_.known()) {
    ComposeRows(windows.state);
    view_.rows = rows_;
    view_.summary = summary_;
    const std::optional<ro::ui::DatabaseTarget> shown = windows.state.history.current();
    // LA FICHE SUIT LE FIL, pas l'onglet : l'onglet ne choisit que la liste.
    if (shown.has_value() && shown->kind == ro::ui::DatabaseKind::kItem) {
      ComposeItem(shown->id, context, answer.sellers_shown);
    }
  }
  windows.view = &view_;
}

void DatabaseStage::NoteMonsterView(DatabaseWindows& windows,
                                    const ro::ui::MonsterSheetView* monster) {
  if (windows.view != nullptr) {
    view_.monster = monster;
  }
}

}  // namespace moonclient
