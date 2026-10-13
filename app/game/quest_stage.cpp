// Les quêtes dans la boucle de la partie : trames, demandes, mémoire. La
// composition des vues est dans `quest_compose.cpp`. Voir l'en-tête.

#include "game/quest_stage.h"

#include <algorithm>
#include <ctime>
#include <format>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/bug_report.h"
#include "ro/net/quest_log.h"
#include "ro/sim/world.h"

#include "quest/quest_rules.h"

namespace moonclient {
namespace {

// La couleur des nouvelles des quêtes dans la conversation : le jaune du
// client, dans le canal d'information des quêtes.
constexpr std::uint32_t kQuestChatColor = 0xFFFF00;
// Et celle des lignes de la bulle de suivi, en haut de l'écran : le même jaune.
constexpr std::uint32_t kHuntBalloonColor = 0xFFFF00;

// L'heure du mur, celle des échéances que le serveur envoie.
[[nodiscard]] std::int64_t UnixNow() { return static_cast<std::int64_t>(std::time(nullptr)); }

void Erase(std::vector<std::uint32_t>& ids, std::uint32_t quest_id) { std::erase(ids, quest_id); }

void Add(std::vector<std::uint32_t>& ids, std::uint32_t quest_id) {
  if (std::ranges::find(ids, quest_id) == ids.end()) {
    ids.push_back(quest_id);
  }
}

[[nodiscard]] bool Has(const std::vector<std::uint32_t>& ids, std::uint32_t quest_id) {
  return std::ranges::find(ids, quest_id) != ids.end();
}

}  // namespace

void QuestStage::See(const QuestCatalog* catalog, std::string catalog_problem,
                     const RecommendedCatalog* recommended, const ro::vfs::ItemTable* items,
                     ro::render::ItemIcons* icons, const ro::vfs::ItemWords* words,
                     ro::render::GameImages* images) noexcept {
  catalog_ = catalog;
  catalog_problem_ = std::move(catalog_problem);
  recommended_ = recommended;
  images_ = images;
  items_.See(items, icons, words);
  art_loaded_ = false;
}

void QuestStage::StartSession() {
  log_.Clear();
  memory_ = QuestMemory{};
  LoadQuestMemory(client_root_, memory_);
  character_.clear();
  arrived_early_.clear();
  toggle_asked_ = false;
  was_open_ = false;
  remember_asked_ = false;
  list_received_ = false;
  first_journal_pending_ = false;
  flash_until_ms_.clear();
  pending_waits_.clear();
  guide_.reset();
  monster_ = 0;
  settings_asked_ = false;
  item_gesture_ = {};
  address_.clear();
  bug_report_.reset();
}

// ── LA MÉMOIRE ─────────────────────────────────────────────────────────────

RememberedQuests& QuestStage::Kept() {
  if (RememberedQuests* kept = memory_.Of(character_); kept != nullptr) {
    return *kept;
  }
  memory_.Remember(RememberedQuests{.character = character_});
  return *memory_.Of(character_);
}

void QuestStage::Restore(QuestWindows& windows, const ro::sim::World& world,
                         std::int64_t now_unix_s) {
  if (!character_.empty() || world.player_name().empty()) {
    return;
  }
  character_ = world.player_name();
  // 🔴 UN PERSONNAGE QUE LE CLIENT N'A JAMAIS VU n'a rien de « nouveau » :
  // tout son journal arrive d'un coup, et le marquer entier serait du bruit.
  const bool first_time = memory_.Of(character_) == nullptr;
  RememberedQuests& kept = Kept();
  ro::ui::QuestWindowState& state = windows.window;
  state.tab = kept.tab;
  state.sort = kept.sort;
  state.category = kept.category;
  state.group_by_category = kept.group_by_category;
  // ⚠ LA LISTE PEUT ARRIVER APRÈS : le serveur la demande d'abord au serveur de
  // personnages. Tant qu'elle n'est pas là, ce qui arrive se tient pour déjà vu.
  first_journal_pending_ = first_time && !list_received_;
  for (const std::uint32_t quest_id : std::exchange(arrived_early_, {})) {
    NoteArrival(quest_id, now_unix_s, track_new_quests_);
  }
  if (first_time) {
    for (const ro::sim::QuestRecord& quest : log_.quests()) {
      Add(kept.seen, quest.quest_id);
    }
  }
  if (list_received_) {
    PruneToJournal();
  }
  Remember(windows);
}

void QuestStage::PruneToJournal() {
  // CE QUE LA LISTE ENTIÈRE NE PORTE PLUS est sorti du journal ailleurs — un
  // autre client, ou un retrait reçu juste avant une coupure. Sans cet oubli,
  // la même quête reprise plus tard ne serait annoncée ni « nouvelle ».
  RememberedQuests& kept = Kept();
  const auto absent = [this](std::uint32_t quest_id) { return log_.Find(quest_id) == nullptr; };
  std::erase_if(kept.tracked, absent);
  std::erase_if(kept.seen, absent);
  std::erase_if(kept.received_unix_s,
                [&absent](const auto& arrival) { return absent(arrival.first); });
  remember_asked_ = true;
}

void QuestStage::Remember(const QuestWindows& windows) {
  if (character_.empty()) {
    return;
  }
  RememberedQuests mine = Kept();
  const ro::ui::QuestWindowState& state = windows.window;
  mine.tab = state.tab;
  mine.sort = state.sort;
  mine.category = state.category;
  mine.group_by_category = state.group_by_category;
  // ⚠ RELU AVANT D'ÉCRIRE : un autre client, sur la même installation, a pu
  // retenir ses propres quêtes depuis ; on n'écrit que sa propre part.
  //
  // ⚠ ET RIEN SI LA RELECTURE ÉCHOUE : le fichier abîmé — ou tenu un instant
  // par un antivirus — porte la part des AUTRES personnages, qu'une écriture
  // depuis une mémoire vide effacerait toutes.
  QuestMemory reread;
  if (!LoadQuestMemory(client_root_, reread)) {
    memory_.Remember(std::move(mine));
    return;
  }
  memory_ = std::move(reread);
  memory_.Remember(std::move(mine));
  static_cast<void>(SaveQuestMemory(client_root_, memory_));
}

void QuestStage::Track(std::uint32_t quest_id, bool tracked) {
  RememberedQuests& kept = Kept();
  if (tracked) {
    Add(kept.tracked, quest_id);
    Erase(kept.untracked, quest_id);
  } else {
    Erase(kept.tracked, quest_id);
    Add(kept.untracked, quest_id);
  }
  remember_asked_ = true;
}

bool QuestStage::Tracked(std::uint32_t quest_id) const {
  const RememberedQuests* kept = memory_.Of(character_);
  return kept != nullptr && Has(kept->tracked, quest_id);
}

void QuestStage::NoteArrival(std::uint32_t quest_id, std::int64_t now_unix_s, bool auto_track) {
  RememberedQuests& kept = Kept();
  if (kept.received_unix_s.contains(quest_id)) {
    return;
  }
  kept.received_unix_s[quest_id] = now_unix_s;
  if (first_journal_pending_) {
    Add(kept.seen, quest_id);
  }
  if (auto_track && !Has(kept.untracked, quest_id)) {
    Add(kept.tracked, quest_id);
  }
  remember_asked_ = true;
}

void QuestStage::NoteDeparture(std::uint32_t quest_id, std::int64_t now_unix_s,
                               std::optional<ro::sim::QuestEnding> ending) {
  if (character_.empty()) {
    return;
  }
  RememberedQuests& kept = Kept();
  // UNE QUÊTE N'A QU'UNE LIGNE D'HISTORIQUE, sa dernière sortie : une
  // quotidienne rendue chaque jour ne remplit pas l'onglet.
  std::erase_if(kept.history,
                [quest_id](const QuestHistoryEntry& left) { return left.quest_id == quest_id; });
  kept.history.push_back(QuestHistoryEntry{.quest_id = quest_id,
                                           .title = TitleOf(quest_id),
                                           .left_unix_s = now_unix_s,
                                           .ending = ending});
  if (kept.history.size() > kMostQuestHistory) {
    kept.history.erase(kept.history.begin());
  }
  Erase(kept.tracked, quest_id);
  Erase(kept.untracked, quest_id);
  Erase(kept.seen, quest_id);
  kept.received_unix_s.erase(quest_id);
  remember_asked_ = true;
}

std::string QuestStage::DepartureLine(std::uint32_t quest_id,
                                      const std::optional<ro::sim::QuestEnding>& ending) const {
  if (!ending.has_value()) {
    return QuestDeletedLine(CatalogTitle(quest_id));
  }
  const std::string title = TitleOf(quest_id);
  switch (ending->reason) {
    case ro::sim::QuestEndReason::kCompleted:
      return QuestFill(ro::core::Tr("Quête « {} » achevée."), {title});
    case ro::sim::QuestEndReason::kErased:
      return QuestFill(ro::core::Tr("Quête « {} » abandonnée."), {title});
    case ro::sim::QuestEndReason::kReplaced:
      if (ending->next_quest_id != 0) {
        return QuestFill(ro::core::Tr("Quête « {} » : étape franchie, la suite est « {} »."),
                         {title, TitleOf(ending->next_quest_id)});
      }
      return QuestFill(ro::core::Tr("Quête « {} » : étape franchie."), {title});
    case ro::sim::QuestEndReason::kRetired:
      break;
  }
  return QuestDeletedLine(CatalogTitle(quest_id));
}

std::string QuestStage::CatalogTitle(std::uint32_t quest_id) const {
  const QuestInfo* info = catalog_ != nullptr ? catalog_->Find(quest_id) : nullptr;
  return info != nullptr ? info->title : std::string();
}

std::string QuestStage::TitleOf(std::uint32_t quest_id) const {
  if (std::string title = CatalogTitle(quest_id); !title.empty()) {
    return title;
  }
  return QuestFill(ro::core::Tr("Quête {}"), {std::to_string(quest_id)});
}

// ── LES TRAMES ─────────────────────────────────────────────────────────────

void QuestStage::SayProgress(const ro::sim::QuestRecord& quest, std::size_t hunt_rank,
                             ro::sim::World& world, std::uint32_t now_ms) const {
  if (hunt_rank >= quest.hunts.size()) {
    return;
  }
  std::string line = QuestHuntLine(CatalogTitle(quest.quest_id), quest.hunts[hunt_rank]);
  if (line.empty()) {
    return;
  }
  world.RecordChat(ro::sim::ChatKind::kServer, ro::sim::ChatType::kQuestInfo, 0, std::move(line),
                   now_ms, kQuestChatColor);
}

void QuestStage::SayReadyWaits(QuestWindows& windows, ro::sim::World& world,
                               std::int64_t now_unix_s, std::uint32_t now_ms) {
  std::erase_if(pending_waits_, [this](std::uint32_t quest_id) {
    const ro::sim::QuestRecord* quest = log_.Find(quest_id);
    return quest == nullptr || !quest->active;
  });
  for (const ro::sim::QuestRecord& quest : log_.quests()) {
    const QuestInfo* info = catalog_ != nullptr ? catalog_->Find(quest.quest_id) : nullptr;
    if (!quest.active || !IsWaitingQuest(info) || quest.expires_unix_s == 0) {
      continue;
    }
    const bool pending = Has(pending_waits_, quest.quest_id);
    if (static_cast<std::int64_t>(quest.expires_unix_s) > now_unix_s) {
      if (!pending) {
        pending_waits_.push_back(quest.quest_id);
      }
    } else if (pending) {
      Erase(pending_waits_, quest.quest_id);
      world.RecordChat(ro::sim::ChatKind::kServer, ro::sim::ChatType::kQuestInfo, 0,
                       QuestFill(ro::core::Tr("Quête « {} » prête : retournez voir le PNJ."),
                                 {TitleOf(quest.quest_id)}),
                       now_ms, kQuestChatColor);
      windows.news_lit = true;
    }
  }
}

ro::ui::QuestTab QuestStage::TabOf(const ro::sim::QuestRecord& quest) const {
  const QuestInfo* info = catalog_ != nullptr ? catalog_->Find(quest.quest_id) : nullptr;
  if (!quest.active) {
    return ro::ui::QuestTab::kSuspended;
  }
  return IsWaitingQuest(info) ? ro::ui::QuestTab::kWaiting : ro::ui::QuestTab::kActive;
}

ro::net::FrameOutcome QuestStage::ApplyFrame(const ro::net::Frame& frame, QuestWindows& windows,
                                             ro::sim::World& world, std::uint32_t now_ms) {
  const ro::net::QuestFrameResult result = ro::net::ApplyQuestFrame(frame, log_);
  // UN RETRAIT D'UNE QUÊTE QUE LE JOURNAL NE TENAIT PAS s'annonce quand même,
  // sous un titre vide, comme au client d'origine.
  if (result.deleted.has_value() && !result.removed.has_value()) {
    world.RecordChat(ro::sim::ChatKind::kServer, ro::sim::ChatType::kQuestInfo, 0,
                     QuestDeletedLine({}), now_ms, kQuestChatColor);
  }
  if (result.outcome != ro::net::FrameOutcome::kApplied) {
    return result.outcome;
  }
  const std::int64_t now_unix_s = UnixNow();
  const auto arrive = [this, now_unix_s](std::uint32_t quest_id) {
    if (character_.empty()) {
      arrived_early_.push_back(quest_id);
    } else {
      NoteArrival(quest_id, now_unix_s, track_new_quests_);
    }
  };
  if (result.replaced) {
    list_received_ = true;
    for (const ro::sim::QuestRecord& quest : log_.quests()) {
      arrive(quest.quest_id);
    }
    first_journal_pending_ = false;
    if (!character_.empty()) {
      PruneToJournal();
    }
  }
  if (result.added.has_value()) {
    // UNE QUÊTE REÇUE EN JEU est une vraie nouvelle, même au premier passage :
    // un journal vide n'envoie jamais sa liste.
    first_journal_pending_ = false;
    arrive(*result.added);
    // UNE QUÊTE DÉJÀ AU JOURNAL n'est pas une nouvelle : le serveur renvoie
    // l'ajout quand il change son état.
    if (!result.added_was_listed) {
      world.RecordChat(ro::sim::ChatKind::kServer, ro::sim::ChatType::kQuestInfo, 0,
                       QuestReceivedLine(), now_ms, kQuestChatColor);
      windows.news_lit = true;
    }
  }
  if (result.removed.has_value()) {
    world.RecordChat(ro::sim::ChatKind::kServer, ro::sim::ChatType::kQuestInfo, 0,
                     DepartureLine(*result.removed, result.ending), now_ms, kQuestChatColor);
    NoteDeparture(*result.removed, now_unix_s, result.ending);
  }
  // ⚠ UNE SEULE BULLE PAR TRAME, une ligne par chasse redite : elle remplace
  // la précédente en haut de l'écran.
  ro::sim::BalloonNotice hunts{.family = ro::sim::BalloonFamily::kHunt, .heard_ms = now_ms};
  for (const ro::net::QuestProgress& said : result.reported) {
    if (const ro::sim::QuestRecord* quest = log_.Find(said.quest_id); quest != nullptr) {
      SayProgress(*quest, said.hunt_rank, world, now_ms);
      if (said.hunt_rank < quest->hunts.size()) {
        hunts.lines.push_back(ro::sim::BalloonLine{
            .text = HuntBalloonLine(quest->hunts[said.hunt_rank]), .color = kHuntBalloonColor});
      }
    }
  }
  world.balloon_news().Post(std::move(hunts));
  for (const ro::net::QuestProgress& progress : result.progressed) {
    std::erase_if(flash_until_ms_,
                  [&progress](const auto& flash) { return flash.first == progress.quest_id; });
    flash_until_ms_.emplace_back(progress.quest_id, now_ms + kQuestFlashMs);
  }
  // CE QUE LA TRAME A CHANGÉ S'ÉCRIT TOUT DE SUITE : une coupure dans la même
  // image ne laisserait plus d'image pour le faire.
  if (std::exchange(remember_asked_, false)) {
    Remember(windows);
  }
  return result.outcome;
}

// ── LES DEMANDES ───────────────────────────────────────────────────────────

std::optional<ro::ui::RichPlace> QuestStage::TakeGuide() noexcept {
  return std::exchange(guide_, std::nullopt);
}

std::uint32_t QuestStage::TakeMonster() noexcept { return std::exchange(monster_, 0); }

bool QuestStage::TakeSettings() noexcept { return std::exchange(settings_asked_, false); }

ro::ui::ChatLinkAsk QuestStage::TakeItemGesture() noexcept {
  return std::exchange(item_gesture_, {});
}

std::string QuestStage::TakeAddress() noexcept { return std::exchange(address_, {}); }

std::optional<BugReportContext> QuestStage::TakeBugReport() noexcept {
  return std::exchange(bug_report_, std::nullopt);
}

void QuestStage::OpenOn(QuestWindows& windows, std::uint32_t quest_id) {
  ro::ui::QuestWindowState& state = windows.window;
  const ro::sim::QuestRecord* quest = log_.Find(quest_id);
  if (quest == nullptr) {
    return;
  }
  // ⚠ UN SAUT QUITTE LA RECHERCHE ET LE FILTRE : il mène à la quête.
  state.search.fill('\0');
  state.category = "all";
  state.tab = TabOf(*quest);
  state.folded_groups.clear();
  state.selected = quest_id;
  state.scroll_to_selected = true;
  state.raise = true;
  state.open = true;
  windows.news_lit = false;
  Add(Kept().seen, quest_id);
  remember_asked_ = true;
}

void QuestStage::OpenOnAnywhere(QuestWindows& windows, std::uint32_t quest_id) {
  if (log_.Find(quest_id) != nullptr) {
    OpenOn(windows, quest_id);
    return;
  }
  // SORTIE À SON TOUR, la quête se montre dans l'historique.
  const RememberedQuests* kept = memory_.Of(character_);
  if (kept == nullptr || std::ranges::find(kept->history, quest_id, &QuestHistoryEntry::quest_id) ==
                             kept->history.end()) {
    return;
  }
  ro::ui::QuestWindowState& state = windows.window;
  state.search.fill('\0');
  state.tab = ro::ui::QuestTab::kHistory;
  state.selected = quest_id;
  state.scroll_to_selected = true;
}

void QuestStage::FollowTracker(QuestWindows& windows, const ro::ui::TrackerPanelAction& asked) {
  if (asked.open.has_value() && asked.open->kind == ro::ui::TrackerKind::kQuest) {
    OpenOn(windows, asked.open->key);
  }
  if (asked.guide.has_value() && asked.guide->kind == ro::ui::TrackerKind::kQuest) {
    const QuestInfo* info = catalog_ != nullptr ? catalog_->Find(asked.guide->key) : nullptr;
    if (info != nullptr && info->npc.has_value()) {
      guide_ = ro::ui::RichPlace{
          .map = info->npc->map_name, .x = info->npc->cell_x, .y = info->npc->cell_y};
    }
  }
  if (asked.untrack.has_value() && asked.untrack->kind == ro::ui::TrackerKind::kQuest) {
    Track(asked.untrack->key, false);
  }
}

void QuestStage::Follow(QuestWindows& windows, TrackerWindows& tracker, ro::net::Connection& link,
                        std::uint32_t now_ms) {
  static_cast<void>(now_ms);
  ro::ui::QuestWindowState& state = windows.window;
  if (std::exchange(toggle_asked_, false)) {
    state.open = !state.open;
    if (state.open) {
      state.raise = true;
      windows.news_lit = false;
    }
  }
  const ro::ui::QuestWindowAction answer = std::exchange(windows.answer, {});
  if (answer.report_bug) {
    std::string fields =
        std::format(R"("quest_window":{{"tab":{},"selected":{},"quests":{}}})",
                    static_cast<int>(state.tab), state.selected, log_.quests().size());
    bug_report_ = FieldsBugReport(std::string(ro::core::Tr("Journal des quêtes")), fields);
    bug_report_->category = static_cast<std::uint8_t>(ro::net::BugReportCategory::kQuest);
    if (state.selected != 0) {
      bug_report_->label += " — " + TitleOf(state.selected);
    }
  }
  // ⚠ « VUE » NE VAUT QUE POUR UNE QUÊTE DU JOURNAL : une ligne d'historique
  // regardée ne doit pas faire taire la même quête reprise plus tard.
  if (answer.selected.has_value() && !character_.empty() &&
      log_.Find(*answer.selected) != nullptr) {
    Add(Kept().seen, *answer.selected);
    remember_asked_ = true;
  }
  if (answer.open_quest.has_value()) {
    OpenOnAnywhere(windows, *answer.open_quest);
  }
  if (answer.read_recommended.has_value() && !character_.empty()) {
    Add(Kept().seen_recommended, *answer.read_recommended);
    remember_asked_ = true;
  }
  if (answer.toggle_active.has_value()) {
    if (const ro::sim::QuestRecord* quest = log_.Find(*answer.toggle_active); quest != nullptr) {
      // ⚠ L'ÉTAT NE CHANGE QU'À LA RÉPONSE DU SERVEUR (0x02B7), comme au client.
      if (const ro::core::Result<void> sent =
              ro::net::RequestQuestActive(link, quest->quest_id, !quest->active);
          !sent) {
        ro::core::LogWarning("quêtes : bascule de {} non envoyée — {}", quest->quest_id,
                             ro::core::Describe(sent.error()));
      }
    }
  }
  if (answer.toggle_tracked.has_value() && !character_.empty()) {
    Track(*answer.toggle_tracked, !Tracked(*answer.toggle_tracked));
  }
  if (answer.guide_to.has_value()) {
    guide_ = answer.guide_to;
  }
  if (answer.open_monster != 0) {
    monster_ = answer.open_monster;
  }
  if (answer.item.gesture != ro::ui::ChatLinkGesture::kNothing || answer.item.hovered_id != 0) {
    item_gesture_ = answer.item;
  }
  if (!answer.followed_address.empty()) {
    address_ = answer.followed_address;
  }
  if (!character_.empty()) {
    RememberedQuests& kept = Kept();
    if (answer.forget.has_value()) {
      std::erase_if(kept.history, [&answer](const QuestHistoryEntry& left) {
        return left.quest_id == *answer.forget;
      });
      remember_asked_ = true;
    }
    if (answer.forget_all) {
      kept.history.clear();
      remember_asked_ = true;
    }
    // L'ONGLET, L'ORDRE, LE FILTRE ET LE REGROUPEMENT se retiennent dès qu'ils
    // changent : quitter le jeu fenêtre ouverte ne doit pas les perdre.
    if (kept.tab != state.tab || kept.sort != state.sort || kept.category != state.category ||
        kept.group_by_category != state.group_by_category) {
      remember_asked_ = true;
    }
  }
  if (answer.show_tracker.has_value()) {
    tracker.shown_asked = answer.show_tracker;
  }
  settings_asked_ = settings_asked_ || answer.open_settings;
  if (std::exchange(remember_asked_, false) || (was_open_ && !state.open)) {
    Remember(windows);
  }
  was_open_ = state.open;
}

void QuestStage::Advance(QuestWindows& windows, TrackerWindows& tracker, ro::net::Connection& link,
                         ro::sim::World& world, std::int64_t now_unix_s, std::uint32_t now_ms) {
  track_new_quests_ = tracker.look.track_new_quests;
  Restore(windows, world, now_unix_s);
  Follow(windows, tracker, link, now_ms);
  SayReadyWaits(windows, world, now_unix_s, now_ms);
  std::erase_if(flash_until_ms_, [now_ms](const auto& flash) {
    return static_cast<std::int32_t>(flash.second - now_ms) <= 0;
  });
  if (!art_loaded_ || (images_ != nullptr && images_->generation() != art_generation_)) {
    LoadArt();
  }
  windows.art = &art_;
  // UN FILTRE QUE LE CATALOGUE NE CONNAÎT PAS retombe sur « tout » : sans son
  // bouton, rien ne permettrait d'en sortir.
  std::string& category = windows.window.category;
  if (catalog_ == nullptr ||
      std::ranges::find(catalog_->categories(), category, &QuestCategory::key) ==
          catalog_->categories().end()) {
    category = kQuestAllCategory;
  }
  ComposeTracker(world, tracker.look, now_unix_s, now_ms);
  windows.view = nullptr;
  if (windows.window.open) {
    ComposeWindow(windows, now_unix_s);
    view_.tracker_shown = tracker.shown;
    windows.view = &view_;
  }
}

}  // namespace moonclient
