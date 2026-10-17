// Les succès dans la boucle de la partie. Voir l'en-tête.

#include "game/achievement_stage.h"

#include <algorithm>
#include <utility>

#include "ro/core/ascii.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/achievement.h"
#include "ro/proto/map_packets.h"
#include "ro/render/skin_pieces.h"
#include "ro/sim/world.h"
#include "ro/vfs/achievement_words.h"

#include "game/achievement_bug_report.h"
#include "game/achievement_rows.h"

namespace moonclient {
namespace {

using ro::vfs::AchievementInfo;
using ro::vfs::AchievementWord;

// L'effet que le client joue sur le joueur quand un succès s'achève.
constexpr std::uint32_t kCompletedEffectId = 1094;

// La couleur des lignes de conversation des succès : « on n'en suit pas
// davantage », et le succès achevé quand le joueur a éteint la bannière.
constexpr std::uint32_t kTrackingFullColor = 0xFFFF64;

// Le dossier de l'art de la fenêtre, sous celui de l'interface.
constexpr std::string_view kArtFolder = "achievement_re\\";
constexpr std::string_view kIconPrefix = "icon_";
constexpr std::string_view kBitmapExtension = ".bmp";
// Les deux faces des cases à cocher rondes, prises à la racine de l'interface.
constexpr std::string_view kRadioOn = "radiobtn_on.bmp";
constexpr std::string_view kRadioOff = "radiobtn_off.bmp";

// Le premier mot d'onglet ; chaque onglet consomme ensuite un mot par
// sous-onglet, dans l'ordre de l'arbre (`kAchievementSubtabCounts`) et de
// `AchievementWord`. ⚠ « TOUS » N'A PAS DE MOT dans la table du jeu : son
// libellé est à nous, et il ne consomme aucun mot.
constexpr AchievementWord kFirstTabWord = AchievementWord::kTabSummary;
constexpr const auto& kSubtabCounts = ro::ui::kAchievementSubtabCounts;

// Une réclamation sans réponse au bout de ce temps se tient pour close.
constexpr std::uint32_t kClaimSilenceMs = 10000;

// Les cinq rangées qu'on voit à la fois dans la liste.
constexpr int kVisibleRows = 5;

[[nodiscard]] ro::ui::AchievementChest ChestOf(const AchievementInfo& info,
                                               const ro::sim::AchievementProgress* progress) {
  if (!info.rewards() || (progress != nullptr && progress->rewarded)) {
    return ro::ui::AchievementChest::kNone;
  }
  return progress != nullptr && progress->completed ? ro::ui::AchievementChest::kUnclaimed
                                                    : ro::ui::AchievementChest::kWaiting;
}

// Combien de sous-onglets l'arbre range sous cet onglet.
[[nodiscard]] int SubtabCountOf(int tab) {
  return tab >= 0 && static_cast<std::size_t>(tab) < kSubtabCounts.size()
             ? kSubtabCounts[static_cast<std::size_t>(tab)]
             : 0;
}

// LE SOUS-ONGLET CHOISI SE MONTRE : le défilement d'un autre onglet laisserait
// une bande vide, où un clic choisirait un sous-onglet absent.
void ShowSubtab(ro::ui::AchievementWindowState& state) {
  state.subtab_scroll =
      ro::ui::AchievementSubtabScrollShowing(state.subtab, SubtabCountOf(state.tab));
}

}  // namespace

void AchievementStage::See(const ro::vfs::AchievementCatalog* catalog,
                           const ro::vfs::ItemWords* words, const ro::render::TitleTable* titles,
                           const ro::vfs::StatusTable* statuses, const ro::vfs::ItemTable* items,
                           ro::render::ItemIcons* icons, ro::render::GameImages* images) noexcept {
  catalog_ = catalog;
  words_ = words;
  titles_ = titles;
  statuses_ = statuses;
  images_ = images;
  items_.See(items, icons, words);
  art_loaded_ = false;
}

void AchievementStage::StartSession() {
  book_ = ro::sim::AchievementBook{};
  toggle_asked_ = false;
  described_.reset();
  bug_report_.reset();
  banner_text_.clear();
  banner_since_ms_ = 0;
  memory_ = AchievementMemory{};
  LoadAchievementMemory(client_root_, memory_);
  character_.clear();
  was_open_ = false;
  claim_queue_.clear();
  last_claim_.reset();
  remember_asked_ = false;
}

ro::net::FrameOutcome AchievementStage::ApplyFrame(const ro::net::Frame& frame,
                                                   AchievementWindows& windows,
                                                   ro::sim::World& world, std::uint32_t now_ms) {
  const ro::net::AchievementFrameResult result = ro::net::ApplyAchievementFrame(frame, book_);
  if (result.completed.has_value()) {
    const AchievementInfo* info = catalog_ != nullptr ? catalog_->Find(*result.completed) : nullptr;
    if (words_ != nullptr) {
      banner_text_ = ro::vfs::AchievementCompletedText(
          *words_, info != nullptr ? std::string_view(info->name) : std::string_view());
    }
    banner_since_ms_ = now_ms;
    // ⚠ LA VUE SE REPREND TOUT DE SUITE : le texte qu'elle désignait vient
    // d'être remplacé, et l'interface peut se dessiner avant la prochaine image
    // de l'étage.
    windows.banner = banner_text_;
    windows.banner_style = memory_.banner;
    windows.banner_elapsed_ms = 0;
    // SANS BANNIÈRE, la nouvelle passe par la conversation : elle ne se perd pas.
    if (memory_.banner == ro::ui::AchievementBannerStyle::kNone && !banner_text_.empty()) {
      world.RecordChat(ro::sim::ChatKind::kServer, 0, banner_text_, now_ms, kTrackingFullColor);
    }
    static_cast<void>(world.RecordEffect(world.player_id(), kCompletedEffectId, now_ms));
    windows.news_lit = true;
  }
  return result.outcome;
}

std::optional<TradeItemFocus> AchievementStage::TakeDescribed() noexcept {
  return std::exchange(described_, std::nullopt);
}

std::optional<BugReportContext> AchievementStage::TakeBugReport() noexcept {
  return std::exchange(bug_report_, std::nullopt);
}

// ── LES DEMANDES ───────────────────────────────────────────────────────────

void AchievementStage::Open(AchievementWindows& windows) {
  // LA FENÊTRE SE ROUVRE TELLE QU'ON L'A LAISSÉE — le client la rouvrait sur
  // le résumé. Seule la recherche repart à vide.
  windows.window.search.fill('\0');
  windows.window.open = true;
  windows.news_lit = false;
}

AchievementQuery AchievementStage::QueryOf(const ro::ui::AchievementWindowState& state) {
  return AchievementQuery{.tab = state.tab,
                          .subtab = state.subtab,
                          .filter = state.filter,
                          .sort = state.sort,
                          .search = std::string_view(state.search.data())};
}

void AchievementStage::Restore(AchievementWindows& windows, const ro::sim::World& world) {
  if (!character_.empty() || world.player_name().empty()) {
    return;
  }
  character_ = world.player_name();
  const RememberedAchievements* kept = memory_.Of(character_);
  if (kept == nullptr) {
    return;
  }
  ro::ui::AchievementWindowState& state = windows.window;
  state.tab = kept->tab;
  // ⚠ UN SOUS-ONGLET QUE L'ONGLET N'A PAS rend l'onglet entier.
  state.subtab =
      kept->subtab < SubtabCountOf(kept->tab) ? kept->subtab : ro::ui::kAchievementWholeTab;
  ShowSubtab(state);
  state.filter = kept->filter;
  state.sort = kept->sort;
  state.selected = kept->selected;
  for (const std::uint32_t achievement_id : kept->tracked) {
    // Un succès sorti du catalogue ne prend pas une place qu'on ne pourrait
    // plus libérer.
    if (catalog_ != nullptr && catalog_->Find(achievement_id) != nullptr) {
      static_cast<void>(book_.ToggleTracking(achievement_id));
    }
  }
}

void AchievementStage::Remember(const AchievementWindows& windows) {
  if (character_.empty()) {
    return;
  }
  const ro::ui::AchievementWindowState& state = windows.window;
  const std::span<const std::uint32_t> tracked = book_.tracked();
  // ⚠ RELU AVANT D'ÉCRIRE : un autre client, sur la même installation, a pu
  // retenir ses propres succès depuis ; on n'écrit que sa propre part.
  memory_ = AchievementMemory{};
  LoadAchievementMemory(client_root_, memory_);
  memory_.Remember(RememberedAchievements{.character = character_,
                                          .tracked = {tracked.begin(), tracked.end()},
                                          .tab = state.tab,
                                          .subtab = state.subtab,
                                          .filter = state.filter,
                                          .sort = state.sort,
                                          .selected = state.selected});
  static_cast<void>(SaveAchievementMemory(client_root_, memory_));
}

void AchievementStage::Claim(ro::net::Connection& link, std::uint32_t achievement_id,
                             std::uint32_t now_ms) {
  last_claim_ = achievement_id;
  last_claim_ms_ = now_ms;
  const AchievementInfo* info = catalog_ != nullptr ? catalog_->Find(achievement_id) : nullptr;
  if (const auto claimed = ro::net::ClaimAchievementReward(link, book_, achievement_id,
                                                           info != nullptr && info->rewards());
      !claimed) {
    ro::core::LogInfo("succès : réclamation de {} retenue — {}", achievement_id,
                      ro::core::Describe(claimed.error()));
  }
}

void AchievementStage::PumpClaims(ro::net::Connection& link, std::uint32_t now_ms) {
  // ⚠ LE SERVEUR PEUT SE TAIRE — succès qu'il ne connaît pas, rangée
  // introuvable. Passé ce délai, la réclamation se tient pour close, et la
  // file reprend.
  if (last_claim_.has_value() && book_.ClaimPending(*last_claim_) &&
      now_ms - last_claim_ms_ >= kClaimSilenceMs) {
    book_.SettleClaim(*last_claim_);
  }
  // UNE SEULE À LA FOIS : la suivante part quand la précédente a sa réponse.
  // Le serveur les traite une à une, et un refus se lit alors sans ambiguïté.
  while (!claim_queue_.empty() && !book_.ClaimInFlight()) {
    const std::uint32_t next = claim_queue_.front();
    claim_queue_.erase(claim_queue_.begin());
    Claim(link, next, now_ms);
  }
}

void AchievementStage::SelectFirstRow(AchievementWindows& windows) {
  windows.window.list_scroll = 0;
  windows.window.selected.reset();
  if (catalog_ == nullptr) {
    return;
  }
  const auto rows = RowsOfTab(*catalog_, book_, QueryOf(windows.window));
  if (!rows.empty()) {
    windows.window.selected = rows.front()->achievement_id;
  }
}

void AchievementStage::JumpTo(AchievementWindows& windows, std::uint32_t achievement_id) {
  const AchievementInfo* info = catalog_ != nullptr ? catalog_->Find(achievement_id) : nullptr;
  if (info == nullptr) {
    return;
  }
  ro::ui::AchievementWindowState& state = windows.window;
  // ⚠ UN SAUT QUITTE LA RECHERCHE : il mène à l'onglet du succès.
  state.search.fill('\0');
  state.tab = ro::ui::AchievementTabOfMajor(info->major);
  state.subtab = info->minor;
  ShowSubtab(state);
  const auto rows = RowsOfTab(*catalog_, book_, QueryOf(state));
  const auto found = std::ranges::find(rows, info);
  if (found == rows.end()) {
    // Le filtre le cache : la sélection retombe sur la première rangée.
    SelectFirstRow(windows);
    return;
  }
  const auto position = static_cast<int>(found - rows.begin());
  const int last_top = std::max(0, static_cast<int>(rows.size()) - kVisibleRows);
  state.selected = achievement_id;
  state.list_scroll = position < kVisibleRows ? 0 : std::min(position, last_top);
}

void AchievementStage::ToggleTracking(AchievementWindows& windows, std::uint32_t achievement_id,
                                      ro::sim::World& world, std::uint32_t now_ms) {
  switch (book_.ToggleTracking(achievement_id)) {
    case ro::sim::TrackOutcome::kTracked:
    case ro::sim::TrackOutcome::kUntracked:
      break;
    case ro::sim::TrackOutcome::kFull:
      if (words_ != nullptr) {
        if (const std::string_view full = Word(AchievementWord::kTrackingFull); !full.empty()) {
          world.RecordChat(ro::sim::ChatKind::kServer, 0, std::string(full), now_ms,
                           kTrackingFullColor);
        }
      }
      break;
  }
  Remember(windows);
}

void AchievementStage::Follow(AchievementWindows& windows, ro::net::Connection& link,
                              ro::sim::World& world, std::uint32_t now_ms) {
  if (std::exchange(toggle_asked_, false)) {
    if (windows.window.open) {
      windows.window.open = false;
    } else {
      Open(windows);
    }
  }

  const ro::ui::AchievementAction answer = std::exchange(windows.answer, {});
  if (windows.window.open) {
    // ⚠ AVANT TOUT CHANGEMENT : le rapport décrit l'image que le joueur a vue.
    if (answer.report_bug) {
      bug_report_ = AchievementBugReport(AchievementBugScene{
          .catalog = catalog_,
          .book = &book_,
          .window = &windows.window,
          .list_mode = ro::ui::AchievementListMode(view_),
          .rows = rows_.size(),
      });
    }
    if (answer.tab_changed || answer.order_changed) {
      SelectFirstRow(windows);
    }
    // CE QU'ON VIENT DE CHOISIR SE RETIENT TOUT DE SUITE : quitter le jeu
    // fenêtre ouverte ne doit pas le perdre.
    if (answer.tab_changed || answer.order_changed || answer.select.has_value() ||
        answer.jump_to.has_value()) {
      remember_asked_ = true;
    }
    if (answer.select.has_value()) {
      windows.window.selected = *answer.select;
    }
    if (answer.jump_to.has_value()) {
      JumpTo(windows, *answer.jump_to);
    }
    if (answer.toggle_tracking.has_value()) {
      ToggleTracking(windows, *answer.toggle_tracking, world, now_ms);
    }
    if (answer.claim.has_value()) {
      Claim(link, *answer.claim, now_ms);
    }
    if (answer.claim_all && catalog_ != nullptr) {
      claim_queue_ = ClaimableAchievements(*catalog_, book_);
    }
    if (answer.banner_style.has_value()) {
      memory_ = AchievementMemory{};
      LoadAchievementMemory(client_root_, memory_);
      memory_.banner = *answer.banner_style;
      static_cast<void>(SaveAchievementMemory(client_root_, memory_));
    }
    if (answer.describe_item.has_value() && *answer.describe_item > 0) {
      described_ = TradeItemFocus{.item_id = static_cast<std::uint32_t>(*answer.describe_item)};
    }
  }

  if (answer.tracker_settings.look.has_value() || answer.tracker_settings.reset_place) {
    windows.tracker_settings = answer.tracker_settings;
  }
  // CE QUE LE PANNEAU COMMUN DEMANDE SUR UN SUCCÈS.
  if (const std::optional<std::uint32_t> opened = std::exchange(windows.tracker_open, std::nullopt);
      opened.has_value()) {
    if (!windows.window.open) {
      Open(windows);
    }
    JumpTo(windows, *opened);
  }
  if (const std::optional<std::uint32_t> dropped =
          std::exchange(windows.tracker_untrack, std::nullopt);
      dropped.has_value() && book_.IsTracked(*dropped)) {
    ToggleTracking(windows, *dropped, world, now_ms);
  }
  // LA FENÊTRE SE FERME : ce qu'on y a laissé se retient.
  if (std::exchange(remember_asked_, false) || (was_open_ && !windows.window.open)) {
    Remember(windows);
  }
  was_open_ = windows.window.open;
  PumpClaims(link, now_ms);
}

void AchievementStage::Advance(AchievementWindows& windows, ro::net::Connection& link,
                               ro::sim::World& world, std::uint32_t now_ms) {
  Restore(windows, world);
  Follow(windows, link, world, now_ms);

  windows.banner = {};
  windows.banner_style = memory_.banner;
  windows.banner_elapsed_ms = now_ms - banner_since_ms_;
  if (!banner_text_.empty() &&
      windows.banner_elapsed_ms < ro::ui::AchievementBannerLifeMs(memory_.banner)) {
    windows.banner = banner_text_;
  }

  windows.view = nullptr;
  windows.tracker_entries = {};
  if (catalog_ == nullptr) {
    return;
  }
  if (!art_loaded_ || (images_ != nullptr && images_->generation() != art_generation_)) {
    LoadArt();
  }
  ComposeTracker();
  windows.tracker_entries = tracker_entries_;
  if (windows.window.open) {
    ComposeWindow(windows);
    windows.view = &view_;
  }
}

// ── L'ART ──────────────────────────────────────────────────────────────────

ro::ui::SkinImage AchievementStage::ImageAt(std::string_view leaf) const {
  if (images_ == nullptr) {
    return {};
  }
  std::string path(ro::render::kSkinRoot);
  path += leaf;
  // ⚠ `GetOurs` ET NON `Get` : le chemin est écrit dans NOS sources, en UTF-8,
  // et le dossier d'interface est en coréen.
  const ro::render::IconTexture& icon = images_->GetOurs(path);
  if (!icon.usable()) {
    return {};
  }
  return ro::ui::SkinImage{.id = static_cast<ImTextureID>(icon.texture.name()),
                           .width = static_cast<float>(icon.width_px),
                           .height = static_cast<float>(icon.height_px)};
}

ro::ui::SkinImage AchievementStage::PaperArtAt(std::string_view leaf,
                                               DarkPaperVariant variant) const {
  ro::ui::SkinImage image = ImageAt(leaf);
  if (!image.usable()) {
    return image;
  }
  std::string path(ro::render::kSkinRoot);
  path += leaf;
  if (const ro::render::IconTexture& turned = (images_->*variant)(path); turned.usable()) {
    image.dark_paper_id = static_cast<ImTextureID>(turned.texture.name());
  }
  return image;
}

ro::ui::SkinImage AchievementStage::IconOf(const AchievementInfo& info) const {
  std::string leaf(kArtFolder);
  leaf += kIconPrefix;
  leaf += ro::core::AsciiLower(info.group);
  leaf += kBitmapExtension;
  // Un pictogramme dans sa bulle, peint sur du blanc comme le reste de l'art.
  return PaperArtAt(leaf, &ro::render::GameImages::GetOursForDarkPaper);
}

void AchievementStage::LoadArt() {
  art_loaded_ = true;
  art_generation_ = images_ != nullptr ? images_->generation() : 0;
  const auto leaf_of = [](std::string_view name) {
    std::string leaf(kArtFolder);
    leaf += name;
    leaf += kBitmapExtension;
    return leaf;
  };
  // TROIS SORTES D'ART. L'interface — fonds, onglets, rangées, boutons, cadres,
  // ascenseur — est peinte sur du blanc et se retourne pour un papier sombre.
  // Deux objets peints sur du blanc, le trophée et le tampon, s'en détourent.
  // Les autres objets ont déjà un fond transparent et se posent tels.
  const auto art = [&](std::string_view name) {
    return PaperArtAt(leaf_of(name), &ro::render::GameImages::GetOursForDarkPaper);
  };
  const auto object = [&](std::string_view name) {
    return PaperArtAt(leaf_of(name), &ro::render::GameImages::GetOursMatted);
  };
  const auto as_is = [&](std::string_view name) { return ImageAt(leaf_of(name)); };
  art_.bg_tab = art("bg_tab");
  art_.tab_out = art("tab_out");
  art_.tab_over = art("tab_over");
  art_.tab_press = art("tab_press");
  art_.tab_sub_out = art("tab_sub_out");
  art_.tab_sub_over = art("tab_sub_over");
  art_.tab_sub_press = art("tab_sub_press");
  art_.tab_subarrow = art("tab_subarrow");
  art_.bg_upper = art("bg_upper");
  art_.upper_trophy = object("upper_trophy");
  art_.radio_on = PaperArtAt(kRadioOn, &ro::render::GameImages::GetOursForDarkPaper);
  art_.radio_off = PaperArtAt(kRadioOff, &ro::render::GameImages::GetOursForDarkPaper);
  art_.bg_list = art("bg_list");
  art_.list_out = art("list_out");
  art_.list_press = art("list_press");
  art_.list_complete_out = art("list_complete_out");
  art_.list_complete_press = art("list_complete_press");
  art_.list_pin = as_is("list_pin");
  art_.badge_complete = as_is("badge_complete");
  art_.rewardbox_default = as_is("list_rewardbox_default");
  art_.rewardbox_unclaimed = as_is("list_rewardbox_not_receive");
  art_.bg_detail = art("bg_detail");
  art_.detail_stamp = object("detail_stamp");
  art_.detail_bullet = art("detail_bullet");
  art_.receive_out = art("btn_receive_out");
  art_.receive_over = art("btn_receive_over");
  art_.receive_press = art("btn_receive_press");
  art_.receive_disabled = art("btn_receive_disable");
  art_.reward_title = art("reward_title");
  art_.reward_buff = art("reward_buff");
  art_.reward_item = art("reward_item");
  art_.bg_summary = art("bg_summary");
  art_.scroll_arrow_up = art("scroll_v_arrow_up");
  art_.scroll_arrow_down = art("scroll_v_arrow_down");
  art_.scroll_bg_mid = art("scroll_v_bg_mid");
  art_.scroll_bg_down = art("scroll_v_bg_down");
  art_.scroll_thumb_up = art("scroll_v_thumb_up");
  art_.scroll_thumb_mid = art("scroll_v_thumb_mid");
  art_.scroll_thumb_down = art("scroll_v_thumb_down");
}

// ── LES VUES ───────────────────────────────────────────────────────────────

std::string_view AchievementStage::Word(AchievementWord word) const {
  return words_ != nullptr ? ro::vfs::AchievementText(*words_, word) : std::string_view();
}

void AchievementStage::ComposeWords() {
  ro::ui::AchievementWords& words = view_.words;
  words.window_title = Word(AchievementWord::kWindowTitle);
  words.summary_title = Word(AchievementWord::kSummaryTitle);
  words.recent = Word(AchievementWord::kRecent);
  words.filter_all = Word(AchievementWord::kFilterAll);
  words.filter_unclaimed = Word(AchievementWord::kFilterUnclaimed);
  auto rank = static_cast<std::size_t>(kFirstTabWord);
  for (std::size_t tab = 0; tab < ro::ui::kAchievementTabCount; ++tab) {
    words.tabs[tab] = tab == ro::ui::kAchievementAllTab
                          ? std::string_view(ro::core::Tr("Tous"))
                          : Word(static_cast<AchievementWord>(rank++));
    subtab_words_[tab].clear();
    for (int sub = 0; sub < kSubtabCounts[tab]; ++sub) {
      subtab_words_[tab].push_back(Word(static_cast<AchievementWord>(rank++)));
    }
    words.subtabs[tab] = subtab_words_[tab];
  }
}

void AchievementStage::ComposeWindow(const AchievementWindows& windows) {
  const ro::ui::AchievementWindowState& state = windows.window;
  view_ = ro::ui::AchievementView{};
  view_.art = &art_;
  view_.tab = state.tab;
  view_.searching = state.search.front() != '\0';
  view_.claimable = ClaimableAchievements(*catalog_, book_).size();
  view_.banner_style = memory_.banner;
  view_.tracker_look = windows.tracker_look;
  ComposeWords();

  const ro::sim::AchievementStanding& standing = book_.standing();
  ro::ui::AchievementUpperView& upper = view_.upper;
  upper.total_points = standing.total_points;
  upper.rank = static_cast<std::int16_t>(standing.rank);
  upper.rank_points = standing.rank_points;
  upper.rank_span = standing.rank_span;
  upper.tab = TallyOfTab(*catalog_, book_, state.tab, state.subtab);
  if (state.tab == ro::ui::kAchievementSummaryTab) {
    upper.title = view_.words.summary_title;
  } else if (state.tab > 0 && state.tab < ro::ui::kAchievementTabCount) {
    const auto tab = static_cast<std::size_t>(state.tab);
    const std::span<const std::string_view> subs = view_.words.subtabs[tab];
    upper.title = state.subtab >= 0 && static_cast<std::size_t>(state.subtab) < subs.size()
                      ? subs[static_cast<std::size_t>(state.subtab)]
                      : view_.words.tabs[tab];
  }

  if (view_.searching) {
    upper.title = ro::core::Tr("Recherche");
  }
  if (state.tab == ro::ui::kAchievementSummaryTab && !view_.searching) {
    ComposeSummary();
    return;
  }

  const auto shown = RowsOfTab(*catalog_, book_, QueryOf(state));
  if (view_.searching) {
    // LE COMPTE DU BANDEAU EST CELUI DES RÉSULTATS : faits parmi trouvés.
    upper.tab = ro::ui::AchievementTally{.done = static_cast<std::uint32_t>(std::ranges::count_if(
                                             shown,
                                             [this](const AchievementInfo* info) {
                                               const ro::sim::AchievementProgress* progress =
                                                   book_.Find(info->achievement_id);
                                               return progress != nullptr && progress->completed;
                                             })),
                                         .total = static_cast<std::uint32_t>(shown.size())};
  }
  row_dates_.clear();
  row_dates_.reserve(shown.size());
  for (const AchievementInfo* info : shown) {
    const ro::sim::AchievementProgress* progress = book_.Find(info->achievement_id);
    row_dates_.push_back(progress != nullptr && progress->completed
                             ? AchievementDateText(progress->completed_at)
                             : std::string());
  }
  rows_.clear();
  rows_.reserve(shown.size());
  const AchievementInfo* selected = nullptr;
  for (std::size_t index = 0; index < shown.size(); ++index) {
    const AchievementInfo& info = *shown[index];
    const ro::sim::AchievementProgress* progress = book_.Find(info.achievement_id);
    rows_.push_back(
        ro::ui::AchievementRowView{.achievement_id = info.achievement_id,
                                   .name = info.name,
                                   .summary = info.summary,
                                   .score = info.score,
                                   .icon = IconOf(info),
                                   .completed = progress != nullptr && progress->completed,
                                   .tracked = book_.IsTracked(info.achievement_id),
                                   .chest = ChestOf(info, progress),
                                   .date = row_dates_[index]});
    if (state.selected == info.achievement_id) {
      selected = &info;
    }
  }
  view_.rows = rows_;
  if (selected != nullptr) {
    ComposeDetail(*selected);
  }
}

void AchievementStage::ComposeDetail(const AchievementInfo& info) {
  const ro::sim::AchievementProgress* progress = book_.Find(info.achievement_id);
  const bool completed = progress != nullptr && progress->completed;

  objective_texts_.clear();
  objective_texts_.reserve(info.objectives.size());
  for (std::size_t index = 0; index < info.objectives.size(); ++index) {
    objective_texts_.push_back(ObjectiveText(info, index, progress));
  }
  objectives_.clear();
  const bool counted = info.gauge == ro::vfs::AchievementGauge::kCounts;
  for (std::size_t index = 0; index < info.objectives.size(); ++index) {
    ro::ui::AchievementObjectiveView line{.text = objective_texts_[index],
                                          .state = ObjectiveStateOf(info, index, progress),
                                          .target = info.objectives[index].target,
                                          .shortcut = info.objectives[index].shortcut};
    if (counted) {
      line.count = ObjectiveCount(progress, index);
    }
    objectives_.push_back(line);
  }

  ro::ui::AchievementReceive receive = ro::ui::AchievementReceive::kHidden;
  if (info.rewards() && (progress == nullptr || !progress->rewarded)) {
    // ⚠ UNE RÉCLAMATION EN VOL ÉTEINT LE BOUTON : un second clic ne vaudrait
    // qu'un refus.
    receive = completed && !book_.ClaimPending(info.achievement_id)
                  ? ro::ui::AchievementReceive::kEnabled
                  : ro::ui::AchievementReceive::kDisabled;
  }

  rewards_.clear();
  if (info.rewards()) {
    if (AchievementInfo::Grants(info.reward_title)) {
      rewards_.push_back(ro::ui::AchievementRewardView{
          .kind = ro::ui::AchievementRewardKind::kTitle,
          .tooltip = titles_ != nullptr ? titles_->Find(*info.reward_title) : std::string_view()});
    }
    if (info.reward_script) {
      buff_tooltip_.clear();
      if (statuses_ != nullptr && AchievementInfo::Grants(info.reward_buff)) {
        if (const ro::vfs::StatusInfo* status =
                statuses_->Find(static_cast<std::uint32_t>(*info.reward_buff));
            status != nullptr) {
          buff_tooltip_ = status->name;
          for (const std::string& line : status->description) {
            buff_tooltip_ += '\n';
            buff_tooltip_ += line;
          }
        }
      }
      rewards_.push_back(ro::ui::AchievementRewardView{.kind = ro::ui::AchievementRewardKind::kBuff,
                                                       .tooltip = buff_tooltip_});
    }
    if (AchievementInfo::Grants(info.reward_item)) {
      // LE NOM DE L'ITEM AU SURVOL : le client n'en disait rien sans le clic droit.
      item_tooltip_ = items_.NameOf(static_cast<std::uint32_t>(*info.reward_item));
      rewards_.push_back(ro::ui::AchievementRewardView{
          .kind = ro::ui::AchievementRewardKind::kItem,
          .tooltip = item_tooltip_,
          .item_icon = items_.IconOf(static_cast<std::uint32_t>(*info.reward_item)),
          .item_id = *info.reward_item});
    }
  }

  view_.detail = ro::ui::AchievementDetailView{.achievement_id = info.achievement_id,
                                               .name = info.name,
                                               .details = info.details,
                                               .completed = completed,
                                               .counted = counted,
                                               .objectives = objectives_,
                                               .receive = receive,
                                               .rewards = rewards_};
}

void AchievementStage::ComposeSummary() {
  ro::ui::AchievementSummaryView& summary = view_.summary;
  summary.overall =
      TallyOfTab(*catalog_, book_, ro::ui::kAchievementSummaryTab, ro::ui::kAchievementWholeTab);
  for (std::size_t category = 0; category < ro::ui::kAchievementCategoryCount; ++category) {
    summary.categories[category] = TallyOfTab(
        *catalog_, book_, ro::ui::kAchievementFirstCategoryTab + static_cast<int>(category),
        ro::ui::kAchievementWholeTab);
  }
  const auto recent = MostRecentlyCompleted(*catalog_, book_);
  recent_dates_.clear();
  recent_dates_.reserve(recent.size());
  for (const AchievementInfo* info : recent) {
    recent_dates_.push_back(AchievementDateText(book_.Find(info->achievement_id)->completed_at));
  }
  recent_.clear();
  for (std::size_t index = 0; index < recent.size(); ++index) {
    const AchievementInfo& info = *recent[index];
    const ro::sim::AchievementProgress* progress = book_.Find(info.achievement_id);
    recent_.push_back(ro::ui::AchievementRecentView{
        .achievement_id = info.achievement_id,
        .name = info.name,
        .summary = info.summary,
        .score = info.score,
        .icon = IconOf(info),
        .unclaimed = ChestOf(info, progress) == ro::ui::AchievementChest::kUnclaimed,
        .date = recent_dates_[index]});
  }
  summary.recent = recent_;
}

void AchievementStage::ComposeTracker() {
  tracker_texts_.clear();
  tracker_lines_.clear();
  tracker_entries_.clear();
  // Les textes d'abord, puis les lignes qui pointent dedans, puis les entrées
  // qui pointent dans les lignes : aucun vecteur ne bouge après qu'on y a pointé.
  std::vector<std::size_t> first_line;
  for (const std::uint32_t achievement_id : book_.tracked()) {
    const AchievementInfo* info = catalog_->Find(achievement_id);
    first_line.push_back(tracker_texts_.size());
    if (info == nullptr) {
      continue;
    }
    const ro::sim::AchievementProgress* progress = book_.Find(achievement_id);
    for (std::size_t index = 0; index < info->objectives.size(); ++index) {
      tracker_texts_.push_back(TrackerLineText(*info, index, progress));
    }
  }
  tracker_lines_.reserve(tracker_texts_.size());
  std::size_t rank = 0;
  for (const std::uint32_t achievement_id : book_.tracked()) {
    const AchievementInfo* info = catalog_->Find(achievement_id);
    if (info == nullptr) {
      ++rank;
      continue;
    }
    const ro::sim::AchievementProgress* progress = book_.Find(achievement_id);
    for (std::size_t index = 0; index < info->objectives.size(); ++index) {
      tracker_lines_.push_back(
          ro::ui::TrackerLine{.text = tracker_texts_[first_line[rank] + index],
                              .reached = ObjectiveReached(*info, index, progress)});
    }
    ++rank;
  }
  rank = 0;
  for (const std::uint32_t achievement_id : book_.tracked()) {
    const AchievementInfo* info = catalog_->Find(achievement_id);
    const std::size_t first = first_line[rank++];
    if (info == nullptr) {
      continue;
    }
    const ro::sim::AchievementProgress* progress = book_.Find(achievement_id);
    tracker_entries_.push_back(
        ro::ui::TrackerEntry{.key = achievement_id,
                             .name = info->name,
                             .completed = progress != nullptr && progress->completed,
                             .lines = std::span<const ro::ui::TrackerLine>(tracker_lines_)
                                          .subspan(first, info->objectives.size())});
  }
}

}  // namespace moonclient
