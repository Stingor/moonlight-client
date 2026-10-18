// LES VUES DES QUÊTES : l'art, la fenêtre et le panneau de suivi, composés à
// chaque image. Voir `quest_stage.h`.

#include <algorithm>
#include <optional>
#include <string>

#include "ro/core/i18n.h"
#include "ro/sim/world.h"

#include "game/braces_filled.h"
#include "game/quest_stage.h"
#include "game/skin_image_of.h"
#include "quest/quest_rules.h"

namespace moonclient {
namespace {

// Le dossier de l'art du journal, sous celui de l'interface.
constexpr std::string_view kArtFolder = "renew_questui\\";
constexpr std::string_view kIconPrefix = "ico_";
constexpr std::string_view kBitmapExtension = ".bmp";

// Les lignes d'une description, coupées sur ses fins de ligne.
[[nodiscard]] std::vector<std::string> LinesOf(std::string_view text) {
  std::vector<std::string> lines;
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t end = text.find('\n', start);
    if (end == std::string_view::npos) {
      lines.emplace_back(text.substr(start));
      break;
    }
    lines.emplace_back(text.substr(start, end - start));
    start = end + 1;
  }
  if (lines.size() == 1 && lines.front().empty()) {
    lines.clear();
  }
  return lines;
}

// « 1 500 000 » : les milliers séparés d'une espace, comme ailleurs dans le jeu.
[[nodiscard]] std::string Grouped(std::uint64_t value) {
  std::string digits = std::to_string(value);
  constexpr std::size_t kGroup = 3;
  for (std::size_t at = digits.size(); at > kGroup; at -= kGroup) {
    digits.insert(at - kGroup, 1, ' ');
  }
  return digits;
}

// Comment une quête est sortie du journal, traduit ; vide quand on ne le sait pas.
[[nodiscard]] const char* EndingLabel(const std::optional<ro::sim::QuestEnding>& ending) {
  if (!ending.has_value()) {
    return "";
  }
  switch (ending->reason) {
    case ro::sim::QuestEndReason::kCompleted:
      return ro::core::Tr("Achevée");
    case ro::sim::QuestEndReason::kErased:
      return ro::core::Tr("Abandonnée");
    case ro::sim::QuestEndReason::kReplaced:
      return ro::core::Tr("Étape franchie");
    case ro::sim::QuestEndReason::kRetired:
      return ro::core::Tr("Retirée");
  }
  return "";
}

}  // namespace

// ── L'ART ──────────────────────────────────────────────────────────────────

ro::ui::SkinImage QuestStage::ImageAt(std::string_view leaf) const {
  return SkinImageAt(images_, leaf);
}

ro::ui::SkinImage QuestStage::IconOf(std::string_view icon_key) const {
  if (icon_key.empty()) {
    return {};
  }
  // L'IMAGE QUE LE CATALOGUE DÉCLARE, sinon celle que la clé nomme : une
  // icône portée par des quêtes n'est pas toujours déclarée.
  std::string leaf(kArtFolder);
  const QuestIcon* declared = catalog_ != nullptr ? catalog_->FindIcon(icon_key) : nullptr;
  if (declared != nullptr && !declared->image.empty()) {
    leaf += declared->image;
  } else {
    leaf += kIconPrefix;
    leaf += icon_key;
    leaf += kBitmapExtension;
  }
  return ImageAt(leaf);
}

void QuestStage::LoadArt() {
  art_loaded_ = true;
  art_generation_ = images_ != nullptr ? images_->generation() : 0;
  // TOUT L'ART DE LA FENÊTRE EST PEINT SUR DU BLANC : lignes, cadenas, cases et
  // filigranes se posent aussi sur un papier sombre.
  const auto art = [this](std::string_view name) {
    std::string leaf(kArtFolder);
    leaf += name;
    leaf += kBitmapExtension;
    return PaperArtAt(images_, leaf, &ro::render::GameImages::GetOursForDarkPaper);
  };
  art_.row = art("bg_questlist");
  art_.row_selected = art("bg_questlist_check");
  art_.lock_active = art("bt_lock_open");
  art_.lock_suspended = art("bt_lock");
  art_.track_on = art("bt_check_on");
  art_.track_off = art("bt_check_off");
  art_.detail_empty = art("bg_detail_quest");
  art_.detail_recommended = art("bg_detail_recommended");
}

// ── LA FENÊTRE ─────────────────────────────────────────────────────────────

void QuestStage::ComposeRow(const ro::sim::QuestRecord* quest, const QuestHistoryEntry* left,
                            std::int64_t now_unix_s, ro::ui::QuestRowView& into) {
  const std::uint32_t quest_id = quest != nullptr ? quest->quest_id : left->quest_id;
  const QuestInfo* info = catalog_ != nullptr ? catalog_->Find(quest_id) : nullptr;
  into = ro::ui::QuestRowView{};
  into.quest_id = quest_id;
  if (info != nullptr && !info->title.empty()) {
    into.title = info->title;
  } else if (left != nullptr && !left->title.empty()) {
    into.title = left->title;
  } else {
    into.title = texts_.emplace_back(TitleOf(quest_id));
  }
  if (info != nullptr) {
    into.summary = info->summary;
    into.icon = IconOf(info->icon);
    if (const QuestIcon* icon = catalog_->FindIcon(info->icon); icon != nullptr) {
      into.icon_label = icon->label;
    }
    into.description = description_lists_.emplace_back(LinesOf(info->description));
    if (info->npc.has_value()) {
      into.npc = ro::ui::QuestNpcView{
          .label = texts_.emplace_back(
              BracesFilled("{} ({}, {})", {info->npc->map_name, std::to_string(info->npc->cell_x),
                                           std::to_string(info->npc->cell_y)})),
          .map_name = info->npc->map_name,
          .cell_x = info->npc->cell_x,
          .cell_y = info->npc->cell_y};
    }
    if (info->reward_base_exp > 0) {
      into.reward_base_exp = texts_.emplace_back(Grouped(info->reward_base_exp));
    }
    if (info->reward_job_exp > 0) {
      into.reward_job_exp = texts_.emplace_back(Grouped(info->reward_job_exp));
    }
    std::vector<ro::ui::QuestRewardView>& rewards = reward_lists_.emplace_back();
    for (const QuestRewardItem& reward : info->reward_items) {
      rewards.push_back(
          ro::ui::QuestRewardView{.item_id = reward.item_id,
                                  .name = texts_.emplace_back(items_.NameOf(reward.item_id)),
                                  .icon = items_.IconOf(reward.item_id),
                                  .count = reward.count});
    }
    into.reward_items = rewards;
  }
  if (quest == nullptr) {
    // UNE QUÊTE SORTIE DU JOURNAL n'a plus d'état de jeu : titre et texte seuls.
    return;
  }
  into.unknown = info == nullptr;
  into.active = quest->active;
  into.tracked = Tracked(quest_id);
  const RememberedQuests* kept = memory_.Of(character_);
  into.fresh = kept != nullptr && std::ranges::find(kept->seen, quest_id) == kept->seen.end();
  into.objectives_met = ObjectivesMet(*quest);
  into.waiting = IsWaitingQuest(info);
  const QuestTimeLeft time_left = into.waiting ? WaitLeftOf(quest->expires_unix_s, now_unix_s)
                                               : TimeLeftOf(quest->expires_unix_s, now_unix_s);
  if (!time_left.text.empty()) {
    into.time_left = texts_.emplace_back(time_left.text);
    into.urgent = time_left.urgent;
    into.expired = time_left.expired;
    into.deadline = texts_.emplace_back(QuestDateText(quest->expires_unix_s));
  }
  into.progress = QuestProgress(*quest);
  std::vector<ro::ui::QuestHuntView>& hunts = hunt_lists_.emplace_back();
  for (const ro::sim::QuestHunt& hunt : quest->hunts) {
    hunts.push_back(ro::ui::QuestHuntView{.label = texts_.emplace_back(HuntLabel(hunt)),
                                          .killed = hunt.killed,
                                          .total = hunt.total,
                                          .done = hunt.done(),
                                          .mob_id = hunt.mob_id});
  }
  into.hunts = hunts;
}

void QuestStage::ComposeHistory(const ro::ui::QuestWindowState& state, std::int64_t now_unix_s) {
  const RememberedQuests* kept = memory_.Of(character_);
  if (kept == nullptr) {
    return;
  }
  const std::string_view search(state.search.data());
  // Les plus récentes d'abord.
  for (auto left = kept->history.rbegin(); left != kept->history.rend(); ++left) {
    const QuestInfo* info = catalog_ != nullptr ? catalog_->Find(left->quest_id) : nullptr;
    const std::string_view title =
        info != nullptr && !info->title.empty() ? std::string_view(info->title) : left->title;
    if (!search.empty() &&
        !QuestMatches(ro::sim::QuestRecord{.quest_id = left->quest_id}, info, search) &&
        !QuestTextContains(title, search)) {
      continue;
    }
    const std::uint32_t next_quest_id = left->ending.has_value() ? left->ending->next_quest_id : 0;
    history_.push_back(ro::ui::QuestHistoryView{
        .quest_id = left->quest_id,
        .title = title,
        .icon = info != nullptr ? IconOf(info->icon) : ro::ui::SkinImage{},
        .left_at = texts_.emplace_back(QuestDateText(left->left_unix_s)),
        .ending = EndingLabel(left->ending),
        .next_quest_id = next_quest_id,
        .next_title = next_quest_id != 0
                          ? std::string_view(texts_.emplace_back(TitleOf(next_quest_id)))
                          : std::string_view()});
  }
  view_.history = history_;
  if (state.selected != 0) {
    const auto chosen =
        std::ranges::find(kept->history, state.selected, &QuestHistoryEntry::quest_id);
    if (chosen != kept->history.end()) {
      ComposeRow(nullptr, &*chosen, now_unix_s, selected_row_);
      view_.selected = &selected_row_;
    }
  }
}

ro::ui::QuestRecommendedView QuestStage::RecommendedRow(const RecommendedQuest& entry) {
  const RememberedQuests* kept = memory_.Of(character_);
  ro::ui::QuestRecommendedView row{
      .entry_id = entry.entry_id,
      .title = entry.title,
      .summary = entry.summary,
      .icon = IconOf(entry.icon),
      .image = entry.image.empty() ? ro::ui::SkinImage{}
                                   : ImageAt(std::string(kArtFolder) + entry.image),
      .fresh = kept != nullptr && std::ranges::find(kept->seen_recommended, entry.entry_id) ==
                                      kept->seen_recommended.end(),
      .blocks = entry.blocks};
  if (const QuestIcon* icon = catalog_ != nullptr ? catalog_->FindIcon(entry.icon) : nullptr;
      icon != nullptr) {
    row.icon_label = icon->label;
  }
  if (entry.npc.has_value()) {
    row.npc = ro::ui::QuestNpcView{
        .label = texts_.emplace_back(
            BracesFilled("{} ({}, {})", {entry.npc->map_name, std::to_string(entry.npc->cell_x),
                                         std::to_string(entry.npc->cell_y)})),
        .map_name = entry.npc->map_name,
        .cell_x = entry.npc->cell_x,
        .cell_y = entry.npc->cell_y};
  }
  return row;
}

void QuestStage::ComposeRecommended(const ro::ui::QuestWindowState& state) {
  if (recommended_ == nullptr) {
    return;
  }
  const std::string_view search(state.search.data());
  for (const RecommendedQuest& entry : recommended_->entries()) {
    // LE FILTRE ET LA RECHERCHE DES QUÊTES valent ici : l'icône range l'affiche
    // dans sa catégorie, et la recherche lit tout son texte.
    QuestInfo seen_as{.title = entry.title, .summary = entry.summary, .icon = entry.icon};
    for (const std::string& block : entry.blocks) {
      seen_as.description += block;
      seen_as.description += '\n';
    }
    if (entry.npc.has_value()) {
      seen_as.npc = entry.npc;
    }
    if (!QuestInCategory(&seen_as, catalog_, state.category) ||
        !QuestMatches(ro::sim::QuestRecord{}, &seen_as, search)) {
      continue;
    }
    recommended_rows_.push_back(RecommendedRow(entry));
  }
  view_.recommended = recommended_rows_;
  if (state.selected_recommended != 0) {
    const auto chosen = std::ranges::find(recommended_rows_, state.selected_recommended,
                                          &ro::ui::QuestRecommendedView::entry_id);
    if (chosen != recommended_rows_.end()) {
      view_.selected_recommended = &*chosen;
    } else if (const RecommendedQuest* entry = recommended_->Find(state.selected_recommended);
               entry != nullptr) {
      // CHOISIE MAIS CACHÉE par le filtre ou la recherche : son détail reste.
      selected_recommended_ = RecommendedRow(*entry);
      view_.selected_recommended = &selected_recommended_;
    }
  }
}

void QuestStage::ComposeWindow(const QuestWindows& windows, std::int64_t now_unix_s) {
  const ro::ui::QuestWindowState& state = windows.window;
  texts_.clear();
  recommended_rows_.clear();
  hunt_lists_.clear();
  reward_lists_.clear();
  description_lists_.clear();
  rows_.clear();
  groups_.clear();
  history_.clear();
  categories_.clear();
  view_ = ro::ui::QuestWindowView{};
  view_.tab = state.tab;
  view_.catalog_problem = catalog_problem_;

  if (catalog_ != nullptr) {
    for (const QuestCategory& category : catalog_->categories()) {
      categories_.push_back(ro::ui::QuestCategoryView{
          .key = category.key,
          .label = category.label,
          .image = category.icons.empty() ? ro::ui::SkinImage{} : IconOf(category.icons.front())});
    }
  }
  view_.categories = categories_;

  const std::span<const ro::sim::QuestRecord> quests = log_.quests();
  const RememberedQuests* kept = memory_.Of(character_);
  for (const ro::sim::QuestRecord& quest : quests) {
    ++view_.tab_counts[static_cast<std::size_t>(TabOf(quest))];
  }
  view_.tab_counts[static_cast<std::size_t>(ro::ui::QuestTab::kHistory)] =
      kept != nullptr ? kept->history.size() : 0;
  view_.tab_counts[static_cast<std::size_t>(ro::ui::QuestTab::kRecommended)] =
      recommended_ != nullptr ? recommended_->entries().size() : 0;

  if (state.tab == ro::ui::QuestTab::kHistory) {
    ComposeHistory(state, now_unix_s);
    return;
  }
  if (state.tab == ro::ui::QuestTab::kRecommended) {
    ComposeRecommended(state);
    return;
  }
  const std::string_view search(state.search.data());

  QuestSortFacts facts;
  if (kept != nullptr) {
    facts.received_unix_s = kept->received_unix_s;
  }
  const std::vector<const ro::sim::QuestRecord*> shown = QuestsOfTab(
      quests, catalog_,
      QuestQuery{
          .tab = state.tab, .category = state.category, .search = search, .sort = state.sort},
      facts, now_unix_s);

  // LES GROUPES SUIVENT L'ORDRE DES CATÉGORIES DU CATALOGUE, « le reste » en
  // dernier ; dans un groupe, l'ordre du tri. Les lignes se rangent groupe par
  // groupe pour qu'un groupe soit une tranche contiguë de `rows_`.
  std::vector<std::string_view> group_keys;
  if (state.group_by_category) {
    if (catalog_ != nullptr) {
      for (const QuestCategory& category : catalog_->categories()) {
        if (category.key != kQuestAllCategory && category.key != kQuestOtherCategory) {
          group_keys.push_back(category.key);
        }
      }
    }
    group_keys.push_back(kQuestOtherCategory);
  } else {
    group_keys.push_back({});
  }
  std::vector<std::pair<std::size_t, std::size_t>> slices;
  rows_.reserve(shown.size());
  for (const std::string_view key : group_keys) {
    const std::size_t first = rows_.size();
    for (const ro::sim::QuestRecord* quest : shown) {
      const QuestInfo* info = catalog_ != nullptr ? catalog_->Find(quest->quest_id) : nullptr;
      if (!key.empty() && QuestGroupKey(info, catalog_) != key) {
        continue;
      }
      ComposeRow(quest, nullptr, now_unix_s, rows_.emplace_back());
    }
    slices.emplace_back(first, rows_.size() - first);
  }
  // ⚠ LES SPANS SE PRENNENT ICI, `rows_` NE BOUGERA PLUS.
  for (std::size_t rank = 0; rank < group_keys.size(); ++rank) {
    const auto [first, count] = slices[rank];
    if (count == 0) {
      continue;
    }
    ro::ui::QuestGroupView group{
        .key = group_keys[rank],
        .rows = std::span<const ro::ui::QuestRowView>(rows_).subspan(first, count)};
    if (!group_keys[rank].empty() && catalog_ != nullptr) {
      const auto category =
          std::ranges::find(catalog_->categories(), group_keys[rank], &QuestCategory::key);
      if (category != catalog_->categories().end()) {
        group.label = category->label;
        group.image =
            category->icons.empty() ? ro::ui::SkinImage{} : IconOf(category->icons.front());
      }
    } else if (!group_keys[rank].empty()) {
      group.label = ro::core::Tr("Autres");
    }
    groups_.push_back(group);
  }
  view_.groups = groups_;

  if (state.selected != 0) {
    const auto chosen = std::ranges::find(rows_, state.selected, &ro::ui::QuestRowView::quest_id);
    if (chosen != rows_.end()) {
      view_.selected = &*chosen;
    } else if (const ro::sim::QuestRecord* quest = log_.Find(state.selected); quest != nullptr) {
      // CHOISIE MAIS CACHÉE par le filtre ou la recherche : son détail reste.
      ComposeRow(quest, nullptr, now_unix_s, selected_row_);
      view_.selected = &selected_row_;
    }
  }
}

// ── LE PANNEAU DE SUIVI ────────────────────────────────────────────────────

void QuestStage::ComposeTracker(const ro::sim::World& world, const ro::ui::TrackerLook& look,
                                std::int64_t now_unix_s, std::uint32_t now_ms) {
  tracker_texts_.clear();
  tracker_line_lists_.clear();
  tracker_entries_.clear();
  tracker_more_.clear();
  tracker_total_ = 0;
  const RememberedQuests* kept = memory_.Of(character_);
  if (kept == nullptr) {
    return;
  }
  const std::vector<const ro::sim::QuestRecord*> tracked = TrackedQuests(
      log_.quests(), kept->tracked, catalog_, world.map_name(), look.current_map_first);
  const auto most = static_cast<std::size_t>(std::max(look.most_quests, 0));
  for (const ro::sim::QuestRecord* quest : tracked) {
    if (tracker_entries_.size() >= most) {
      break;
    }
    const QuestInfo* info = catalog_ != nullptr ? catalog_->Find(quest->quest_id) : nullptr;
    std::vector<ro::ui::TrackerLine>& lines = tracker_line_lists_.emplace_back();
    if (quest->hunts.empty() && info != nullptr && !info->summary.empty()) {
      lines.push_back(ro::ui::TrackerLine{.text = tracker_texts_.emplace_back(BracesFilled(
                                              "- {}", {PlainQuestText(info->summary)}))});
    }
    for (const ro::sim::QuestHunt& hunt : quest->hunts) {
      std::string text = BracesFilled("- {}", {HuntLabel(hunt)});
      if (hunt.total > 0) {
        text += BracesFilled(" ({}/{})", {std::to_string(hunt.killed), std::to_string(hunt.total)});
      }
      lines.push_back(ro::ui::TrackerLine{
          .text = tracker_texts_.emplace_back(std::move(text)),
          .reached = hunt.done(),
          .progress = hunt.total > 0 ? std::clamp(static_cast<float>(hunt.killed) /
                                                      static_cast<float>(hunt.total),
                                                  0.0F, 1.0F)
                                     : -1.0F});
    }
    const QuestTimeLeft time_left = TimeLeftOf(quest->expires_unix_s, now_unix_s);
    float flash = 0.0F;
    const auto flashing = std::ranges::find(flash_until_ms_, quest->quest_id,
                                            &std::pair<std::uint32_t, std::uint32_t>::first);
    if (flashing != flash_until_ms_.end()) {
      flash = std::clamp(static_cast<float>(static_cast<std::int32_t>(flashing->second - now_ms)) /
                             static_cast<float>(kQuestFlashMs),
                         0.0F, 1.0F);
    }
    tracker_entries_.push_back(ro::ui::TrackerEntry{
        .key = quest->quest_id,
        .name = info != nullptr && !info->title.empty()
                    ? std::string_view(info->title)
                    : std::string_view(tracker_texts_.emplace_back(TitleOf(quest->quest_id))),
        .badge = time_left.text.empty()
                     ? std::string_view()
                     : std::string_view(tracker_texts_.emplace_back(time_left.text)),
        .badge_urgent = time_left.urgent,
        .badge_expired = time_left.expired,
        .completed = ObjectivesMet(*quest),
        .flash = flash,
        .lines = lines,
        .can_guide = info != nullptr && info->npc.has_value()});
  }
  tracker_total_ = tracked.size();
  if (tracked.size() > tracker_entries_.size()) {
    tracker_more_ = BracesFilled(ro::core::Tr("+{} autres"),
                                 {std::to_string(tracked.size() - tracker_entries_.size())});
  }
}

}  // namespace moonclient
