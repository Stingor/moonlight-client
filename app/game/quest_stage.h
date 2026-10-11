#pragma once

// LES QUÊTES, DANS LA BOUCLE DE LA PARTIE : le journal que le serveur pousse,
// la fenêtre qui le montre, ce que le joueur suit à l'écran, les lignes de
// conversation des nouvelles, et ce que le client retient que le serveur ne
// sait pas (suivi, nouveautés, historique).
//
// Le journal vit dans `sim::QuestLog` ; les textes viennent de
// `data\yaml_files\quests.yml` ; suspendre ou reprendre passe par un verbe de
// `net`, et l'état ne change qu'à la réponse du serveur.

#include <array>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/render/game_images.h"
#include "ro/sim/quest_log.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/chat_link.h"
#include "ro/ui/quest_window.h"
#include "ro/ui/rich_text.h"
#include "ro/ui/tracker_panel.h"

#include "game/bug_report_context.h"
#include "game/trade_items.h"
#include "quest/quest_catalog.h"
#include "quest/quest_memory.h"
#include "quest/recommended_catalog.h"
#include "quest/tracker_memory.h"

namespace moonclient {

// L'ÉCLAT D'UNE QUÊTE QUI AVANCE dans le panneau de suivi, en millisecondes.
inline constexpr std::uint32_t kQuestFlashMs = 1500;

// LES PIÈCES DES QUÊTES, telles que `UiStage::Windows` les porte.
struct QuestWindows {
  ro::ui::QuestWindowState window;
  const ro::ui::QuestWindowView* view = nullptr;
  const ro::ui::QuestArt* art = nullptr;
  ro::ui::QuestWindowAction answer;
  // L'icône du menu s'allume : une quête vient d'arriver, et la fenêtre ne
  // s'est pas rouverte depuis.
  bool news_lit = false;
};

// LE PANNEAU DE SUIVI, commun aux quêtes et aux succès : deux panneaux au
// plus, leurs sections composées à chaque image, et ce qu'ils ont rendu.
struct TrackerWindows {
  std::array<ro::ui::TrackerPanelState, kTrackerPanelCount> panels;
  std::array<std::span<const ro::ui::TrackerSection>, kTrackerPanelCount> sections;
  std::array<ro::ui::TrackerPanelAction, kTrackerPanelCount> answers;
  ro::ui::TrackerLook look;
  // Le joueur veut le suivi à l'écran (la case « Montrer le suivi »).
  bool shown = true;
  // CE QUE LES FENÊTRES DEMANDENT AU SUIVI, consommé par son étage : la case,
  // et les réglages pris sous la puce des quêtes ou des succès.
  std::optional<bool> shown_asked;
  std::optional<ro::ui::TrackerSettingsAnswer> settings_asked;
};

class QuestStage {
 public:
  // Le catalogue — nul quand `data\yaml_files\quests.yml` manque ou ne se lit
  // pas, `catalog_problem` dit alors pourquoi —, les items et leurs icônes, les
  // images de l'installation.
  // Les recommandations — nulles quand leur fichier manque : l'onglet est vide.
  void See(const QuestCatalog* catalog, std::string catalog_problem,
           const RecommendedCatalog* recommended, const ro::vfs::ItemTable* items,
           ro::render::ItemIcons* icons, const ro::vfs::ItemWords* words,
           ro::render::GameImages* images) noexcept;

  // Où se trouve l'installation : la mémoire vit sous son `savedata\`.
  void SeeSaveRoot(std::filesystem::path client_root) { client_root_ = std::move(client_root); }

  // Une partie commence : le journal repart vide. ⚠ LE SERVEUR NE REDIT LA
  // LISTE QU'À L'ENTRÉE SUR LE SERVEUR DE CARTE, et jamais quand elle est vide.
  void StartSession();

  // Une trame reçue : le journal suit, et la conversation dit les nouvelles.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame, QuestWindows& windows,
                                                 ro::sim::World& world, std::uint32_t now_ms);

  // L'ICÔNE DU MENU ET SA TOUCHE : ouvrir ou refermer. Joué à l'image suivante.
  void Toggle() noexcept { toggle_asked_ = true; }

  // L'IMAGE : ce que la fenêtre et le panneau ont rendu, puis les vues de la
  // prochaine. `now_unix_s` est l'heure du mur, celle des échéances.
  void Advance(QuestWindows& windows, TrackerWindows& tracker, ro::net::Connection& link,
               ro::sim::World& world, std::int64_t now_unix_s, std::uint32_t now_ms);

  // CE QUE LA FENÊTRE OU LE PANNEAU DEMANDE AILLEURS — une fois chacun.
  [[nodiscard]] std::optional<ro::ui::RichPlace> TakeGuide() noexcept;
  [[nodiscard]] std::uint32_t TakeMonster() noexcept;
  [[nodiscard]] ro::ui::ChatLinkAsk TakeItemGesture() noexcept;
  [[nodiscard]] std::string TakeAddress() noexcept;
  [[nodiscard]] std::optional<BugReportContext> TakeBugReport() noexcept;

  // LE SUIVI : les entrées des quêtes suivies, composées par `Advance`.
  [[nodiscard]] std::span<const ro::ui::TrackerEntry> tracker_entries() const noexcept {
    return tracker_entries_;
  }
  [[nodiscard]] std::string_view tracker_more() const noexcept { return tracker_more_; }
  // Toutes les quêtes suivies et actives, montrées ou non.
  [[nodiscard]] std::size_t tracker_total() const noexcept { return tracker_total_; }
  // Une action du panneau sur une quête.
  void FollowTracker(QuestWindows& windows, const ro::ui::TrackerPanelAction& asked);

  [[nodiscard]] const ro::sim::QuestLog& log() const noexcept { return log_; }

 private:
  void Restore(QuestWindows& windows, const ro::sim::World& world, std::int64_t now_unix_s);
  void Remember(const QuestWindows& windows);
  [[nodiscard]] RememberedQuests& Kept();
  void Follow(QuestWindows& windows, TrackerWindows& tracker, ro::net::Connection& link,
              std::uint32_t now_ms);
  void OpenOn(QuestWindows& windows, std::uint32_t quest_id);
  // La même, ou sa ligne d'historique si elle a quitté le journal ; rien si elle
  // n'est ni l'un ni l'autre.
  void OpenOnAnywhere(QuestWindows& windows, std::uint32_t quest_id);
  void Track(std::uint32_t quest_id, bool tracked);
  [[nodiscard]] bool Tracked(std::uint32_t quest_id) const;
  void NoteArrival(std::uint32_t quest_id, std::int64_t now_unix_s, bool auto_track);
  void NoteDeparture(std::uint32_t quest_id, std::int64_t now_unix_s,
                     std::optional<ro::sim::QuestEnding> ending);
  // La ligne de conversation d'une quête qui sort du journal, selon la raison
  // que le serveur a dite ; sans raison, celle du client d'origine.
  [[nodiscard]] std::string DepartureLine(std::uint32_t quest_id,
                                          const std::optional<ro::sim::QuestEnding>& ending) const;
  void PruneToJournal();
  void SayProgress(const ro::sim::QuestRecord& quest, std::size_t hunt_rank, ro::sim::World& world,
                   std::uint32_t now_ms) const;
  void SayReadyWaits(QuestWindows& windows, ro::sim::World& world, std::int64_t now_unix_s,
                     std::uint32_t now_ms);
  [[nodiscard]] ro::ui::QuestTab TabOf(const ro::sim::QuestRecord& quest) const;
  [[nodiscard]] std::string TitleOf(std::uint32_t quest_id) const;
  // Le titre que le catalogue donne, VIDE s'il n'en donne pas : les lignes du
  // client d'origine n'ont pas de nom de repli.
  [[nodiscard]] std::string CatalogTitle(std::uint32_t quest_id) const;
  void LoadArt();
  [[nodiscard]] ro::ui::SkinImage ImageAt(std::string_view leaf) const;
  // Une pièce peinte sur du blanc, avec sa variante pour un papier sombre.
  [[nodiscard]] ro::ui::SkinImage PaperArtAt(std::string_view leaf) const;
  [[nodiscard]] ro::ui::SkinImage IconOf(std::string_view icon_key) const;
  void ComposeWindow(const QuestWindows& windows, std::int64_t now_unix_s);
  void ComposeRow(const ro::sim::QuestRecord* quest, const QuestHistoryEntry* left,
                  std::int64_t now_unix_s, ro::ui::QuestRowView& into);
  void ComposeRecommended(const ro::ui::QuestWindowState& state);
  void ComposeHistory(const ro::ui::QuestWindowState& state, std::int64_t now_unix_s);
  [[nodiscard]] ro::ui::QuestRecommendedView RecommendedRow(const RecommendedQuest& entry);
  void ComposeTracker(const ro::sim::World& world, const ro::ui::TrackerLook& look,
                      std::int64_t now_unix_s, std::uint32_t now_ms);

  const QuestCatalog* catalog_ = nullptr;
  std::string catalog_problem_;
  const RecommendedCatalog* recommended_ = nullptr;
  ro::render::GameImages* images_ = nullptr;
  TradeItems items_;
  bool art_loaded_ = false;
  // Voir `AchievementStage::art_generation_`.
  std::uint64_t art_generation_ = 0;
  ro::ui::QuestArt art_;

  ro::sim::QuestLog log_;
  std::filesystem::path client_root_;
  QuestMemory memory_;
  // Le personnage dont la mémoire s'est posée ; vide tant qu'on ne le sait pas.
  std::string character_;
  // Les quêtes reçues avant que le personnage soit connu : leur arrivée se
  // note à la restauration.
  std::vector<std::uint32_t> arrived_early_;
  bool toggle_asked_ = false;
  // Le réglage du suivi, relu à chaque image : une quête reçue se suit-elle ?
  bool track_new_quests_ = true;
  // La liste entière est arrivée dans cette session.
  bool list_received_ = false;
  // Un personnage vu pour la première fois attend sa liste : ce qui arrive se
  // tient pour déjà vu.
  bool first_journal_pending_ = false;
  bool was_open_ = false;
  bool remember_asked_ = false;
  // L'ÉCLAT D'UNE QUÊTE QUI AVANCE : jusqu'à quand, en millisecondes du jeu.
  std::vector<std::pair<std::uint32_t, std::uint32_t>> flash_until_ms_;
  // LES ATTENTES VUES EN COURS : la conversation dit le moment où chacune
  // s'achève. Une attente déjà échue à l'arrivée ne s'annonce pas — l'onglet
  // la montre « prête ».
  std::vector<std::uint32_t> pending_waits_;

  std::optional<ro::ui::RichPlace> guide_;
  std::uint32_t monster_ = 0;
  ro::ui::ChatLinkAsk item_gesture_;
  std::string address_;
  std::optional<BugReportContext> bug_report_;

  // ── CE QUI SE COMPOSE À CHAQUE IMAGE ─────────────────────────────────────
  // ⚠ LES VUES POINTENT DEDANS. Les textes et les listes d'une ligne vivent
  // dans des `deque`, dont un ajout ne déplace pas les éléments déjà là ; les
  // vecteurs que des spans désignent se remplissent EN ENTIER avant qu'on
  // prenne la première.
  std::vector<ro::ui::QuestCategoryView> categories_;
  std::deque<std::string> texts_;
  std::deque<std::vector<ro::ui::QuestHuntView>> hunt_lists_;
  std::deque<std::vector<ro::ui::QuestRewardView>> reward_lists_;
  std::deque<std::vector<std::string>> description_lists_;
  std::vector<ro::ui::QuestRowView> rows_;
  std::vector<ro::ui::QuestGroupView> groups_;
  std::vector<ro::ui::QuestHistoryView> history_;
  std::vector<ro::ui::QuestRecommendedView> recommended_rows_;
  ro::ui::QuestRecommendedView selected_recommended_;
  ro::ui::QuestRowView selected_row_;
  ro::ui::QuestWindowView view_;
  std::deque<std::string> tracker_texts_;
  std::deque<std::vector<ro::ui::TrackerLine>> tracker_line_lists_;
  std::vector<ro::ui::TrackerEntry> tracker_entries_;
  std::string tracker_more_;
  std::size_t tracker_total_ = 0;
};

}  // namespace moonclient
