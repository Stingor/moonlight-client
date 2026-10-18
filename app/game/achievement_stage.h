#pragma once

// LES SUCCÈS, DANS LA BOUCLE DE LA PARTIE : la progression que le serveur
// pousse, la fenêtre qui la montre, le panneau des succès suivis, la bannière
// et l'effet d'un succès achevé, et la réclamation d'une récompense.
//
// Les succès vivent dans `sim::AchievementBook` ; la fenêtre ne fait que
// demander, et la réclamation passe par un verbe de `net` qui rejoue les gardes
// du serveur avant d'émettre.

#include <array>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/render/game_images.h"
#include "ro/render/title_table.h"
#include "ro/sim/achievement_book.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/achievement_window.h"
#include "ro/ui/tracker_panel.h"
#include "ro/vfs/achievement_catalog.h"
#include "ro/vfs/item_words.h"
#include "ro/vfs/status_table.h"

#include "game/achievement_memory.h"
#include "game/achievement_rows.h"
#include "game/bug_report_context.h"
#include "game/npc_shop_stage.h"
#include "game/trade_items.h"

namespace moonclient {

// LES PIÈCES DES SUCCÈS, telles que `UiStage::Windows` les porte : ce que la
// fenêtre et le panneau gardent d'une image à l'autre, les vues que la boucle
// compose, et ce que le joueur a demandé.
struct AchievementWindows {
  ro::ui::AchievementWindowState window;
  const ro::ui::AchievementView* view = nullptr;
  ro::ui::AchievementAction answer;

  // LE SUIVI, que le panneau commun dessine (`TrackerStage`) : les entrées que
  // l'étage compose, et ce que le joueur y a demandé sur un succès.
  std::span<const ro::ui::TrackerEntry> tracker_entries;
  std::optional<std::uint32_t> tracker_open;
  std::optional<std::uint32_t> tracker_untrack;
  // Un réglage du suivi pris sous la puce de la fenêtre des succès, et les
  // réglages en vigueur, que la puce montre.
  std::optional<ro::ui::TrackerSettingsAnswer> tracker_settings;
  ro::ui::TrackerLook tracker_look;

  // La phrase de la bannière tant qu'elle tient ; vide sinon. Son style, et
  // son âge, qui règle l'effacement de la discrète.
  std::string_view banner;
  ro::ui::AchievementBannerStyle banner_style = ro::ui::AchievementBannerStyle::kNative;
  std::uint32_t banner_elapsed_ms = 0;

  // L'icône du menu s'allume : un succès vient d'être achevé, et la fenêtre ne
  // s'est pas rouverte depuis.
  bool news_lit = false;
};

class AchievementStage {
 public:
  // Ce que les succès lisent ailleurs : le catalogue, les mots de la table de
  // messages, les libellés des titres et des états, les items et leurs icônes,
  // les images de l'installation. Tous facultatifs.
  void See(const ro::vfs::AchievementCatalog* catalog, const ro::vfs::ItemWords* words,
           const ro::render::TitleTable* titles, const ro::vfs::StatusTable* statuses,
           const ro::vfs::ItemTable* items, ro::render::ItemIcons* icons,
           ro::render::GameImages* images) noexcept;

  // Où se trouve l'installation : la mémoire des succès vit sous son
  // `savedata\`.
  void SeeSaveRoot(std::filesystem::path client_root) { client_root_ = std::move(client_root); }

  // Une partie commence : rien de la précédente ne survit, hors ce que la
  // mémoire du disque rend au personnage.
  void StartSession();

  // Une trame reçue. Un succès qu'elle achève lève la bannière, joue l'effet sur
  // le joueur et allume l'icône du menu.
  [[nodiscard]] ro::net::FrameOutcome ApplyFrame(const ro::net::Frame& frame,
                                                 AchievementWindows& windows, ro::sim::World& world,
                                                 std::uint32_t now_ms);

  // L'ICÔNE DU MENU ET SA TOUCHE : ouvrir la fenêtre — sur le résumé, comme le
  // client qui la reconstruit —, ou la refermer. Joué à l'image suivante.
  void Toggle() noexcept { toggle_asked_ = true; }

  // L'IMAGE : ce que la fenêtre et le panneau ont rendu, puis les vues de la
  // prochaine.
  void Advance(AchievementWindows& windows, ro::net::Connection& link, ro::sim::World& world,
               std::uint32_t now_ms);

  // L'objet en récompense qu'on veut décrire — une fois.
  [[nodiscard]] std::optional<TradeItemFocus> TakeDescribed() noexcept;

  // Le rapport de bug que le joueur a demandé, avec ce que la fenêtre montrait
  // — une fois.
  [[nodiscard]] std::optional<BugReportContext> TakeBugReport() noexcept;

  [[nodiscard]] const ro::sim::AchievementBook& book() const noexcept { return book_; }

 private:
  void Open(AchievementWindows& windows);
  // Le personnage est connu : sa mémoire se pose, une fois par partie.
  void Restore(AchievementWindows& windows, const ro::sim::World& world);
  // Retient ce que le personnage a laissé, et l'écrit.
  void Remember(const AchievementWindows& windows);
  void Claim(ro::net::Connection& link, std::uint32_t achievement_id, std::uint32_t now_ms);
  void PumpClaims(ro::net::Connection& link, std::uint32_t now_ms);
  [[nodiscard]] static AchievementQuery QueryOf(const ro::ui::AchievementWindowState& state);
  void Follow(AchievementWindows& windows, ro::net::Connection& link, ro::sim::World& world,
              std::uint32_t now_ms);
  void JumpTo(AchievementWindows& windows, std::uint32_t achievement_id);
  void SelectFirstRow(AchievementWindows& windows);
  void ToggleTracking(AchievementWindows& windows, std::uint32_t achievement_id,
                      ro::sim::World& world, std::uint32_t now_ms);
  void LoadArt();
  [[nodiscard]] ro::ui::SkinImage ImageAt(std::string_view leaf) const;
  [[nodiscard]] ro::ui::SkinImage IconOf(const ro::vfs::AchievementInfo& info) const;
  void ComposeWords();
  void ComposeWindow(const AchievementWindows& windows);
  void ComposeDetail(const ro::vfs::AchievementInfo& info);
  void ComposeSummary();
  void ComposeTracker();
  [[nodiscard]] std::string_view Word(ro::vfs::AchievementWord word) const;

  const ro::vfs::AchievementCatalog* catalog_ = nullptr;
  const ro::vfs::ItemWords* words_ = nullptr;
  const ro::render::TitleTable* titles_ = nullptr;
  const ro::vfs::StatusTable* statuses_ = nullptr;
  ro::render::GameImages* images_ = nullptr;
  TradeItems items_;
  bool art_loaded_ = false;
  // LA GÉNÉRATION DES IMAGES dont l'art est tiré : un skin changé la fait
  // grandir, et l'art se relit.
  std::uint64_t art_generation_ = 0;
  ro::ui::AchievementArt art_;

  ro::sim::AchievementBook book_;
  std::filesystem::path client_root_;
  AchievementMemory memory_;
  // Le personnage dont la mémoire s'est posée ; vide tant qu'on ne le sait pas.
  std::string character_;
  bool was_open_ = false;
  // « Tout réclamer » : les succès qui restent, réclamés un à un.
  std::vector<std::uint32_t> claim_queue_;
  // La dernière réclamation partie, et quand : sans réponse, elle se clôt seule.
  std::optional<std::uint32_t> last_claim_;
  std::uint32_t last_claim_ms_ = 0;
  // La fenêtre a changé à cette image : elle se retient à la fin de l'image.
  bool remember_asked_ = false;
  bool toggle_asked_ = false;
  std::optional<TradeItemFocus> described_;
  std::optional<BugReportContext> bug_report_;
  // La dernière bannière, et l'instant de son DERNIER succès.
  std::string banner_text_;
  std::uint32_t banner_since_ms_ = 0;

  // ── CE QUI SE COMPOSE À CHAQUE IMAGE ─────────────────────────────────────
  // ⚠ LES VUES POINTENT DEDANS : elles se prennent une fois les vecteurs remplis.
  std::array<std::vector<std::string_view>, ro::ui::kAchievementTabCount> subtab_words_;
  std::vector<std::string> row_dates_;
  std::vector<ro::ui::AchievementRowView> rows_;
  std::vector<std::string> objective_texts_;
  std::vector<ro::ui::AchievementObjectiveView> objectives_;
  std::string buff_tooltip_;
  std::string item_tooltip_;
  std::vector<ro::ui::AchievementRewardView> rewards_;
  std::vector<std::string> recent_dates_;
  std::vector<ro::ui::AchievementRecentView> recent_;
  std::vector<std::string> tracker_texts_;
  std::vector<ro::ui::TrackerLine> tracker_lines_;
  std::vector<ro::ui::TrackerEntry> tracker_entries_;
  ro::ui::AchievementView view_;
};

}  // namespace moonclient
