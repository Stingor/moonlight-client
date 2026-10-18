#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "ro/net/connection.h"
#include "ro/render/item_icons.h"
#include "ro/sim/craft_session.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/weapon_refine_window.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "items/craft_book.h"
#include "items/craft_log.h"
#include "items/creator_names.h"

namespace moonclient {

// LES RÉGLAGES DE L'AFFINAGE, et leurs défauts.
struct WeaponRefineSettings {
  // Un échec DÉTRUIT l'arme : on demande avant.
  bool confirm = true;
  bool show_cards = true;
  bool show_filter = true;
  bool hover_preview = true;
  bool enter_key = false;
  // Relancer le skill après chaque tentative — jamais affiner tout seul.
  bool auto_recast = false;
  // Tout enchaîner, SANS confirmation. Implique la relance.
  bool auto_refine = false;
  bool show_history = false;
  bool log_time = true;

  [[nodiscard]] bool chains() const noexcept { return auto_recast || auto_refine; }
};

// L'AFFINAGE, VU DE LA PARTIE : ce que le client fait de la liste du serveur —
// la sélection, la confirmation, la chaîne automatique, le journal — et la
// fenêtre qu'il en dessine.
//
// 🔴 UNE LISTE = UNE TENTATIVE. Le serveur efface son skill en attente dès qu'il
// reçoit une réponse : la liste reste affichée, grisée, et l'affinage suivant
// demande de relancer le skill. Fermer RÉPOND aussi — par l'annulation, sans
// laquelle le personnage ne lancerait plus rien.
class WeaponRefineFlow {
 public:
  WeaponRefineFlow(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
                   ro::render::ItemIcons* icons);

  void SeeCreators(CreatorNames creators) noexcept { creators_ = creators; }
  void SeeBook(const CraftBook* book) noexcept { book_ = book; }

  // Lit ce que le serveur a dit depuis l'image précédente, et compose la vue.
  // À appeler une fois par image, avant le dessin.
  void Refresh(ro::sim::World& world, ro::ui::WeaponRefineWindowState& state,
               const WeaponRefineSettings& settings, std::uint32_t now_ms);

  [[nodiscard]] const ro::ui::WeaponRefineView& view() const noexcept { return view_; }

  // Route ce que la fenêtre a demandé, fait avancer la chaîne, et émet.
  void Apply(const ro::ui::WeaponRefineWindowAction& asked, ro::ui::WeaponRefineWindowState& state,
             const WeaponRefineSettings& settings, ro::net::Connection& link, ro::sim::World& world,
             std::uint32_t now_ms);

  // 🔴 UNE FABRICATION VIENT D'OUVRIR SA LISTE : le serveur ne garde qu'un skill
  // en attente, et un affinage armé viderait la liste de fabrication. La
  // session d'affinage se referme donc, en répondant.
  void CloseForOtherCraft(ro::ui::WeaponRefineWindowState& state, ro::net::Connection& link);

 private:
  enum class Pending : std::uint8_t { kNone, kRefine, kCancel, kRecast };

  void TakeList(ro::sim::World& world, ro::ui::WeaponRefineWindowState& state,
                const WeaponRefineSettings& settings, std::uint32_t now_ms);
  void TakeResult(ro::sim::World& world, const WeaponRefineSettings& settings, bool window_open,
                  std::uint32_t now_ms);
  void TakeRefusal(const WeaponRefineSettings& settings, bool window_open, std::uint32_t now_ms);
  void Tick(const ro::ui::WeaponRefineWindowAction& asked, ro::ui::WeaponRefineWindowState& state,
            const WeaponRefineSettings& settings, const ro::sim::World& world,
            std::uint32_t now_ms);
  void Flush(ro::ui::WeaponRefineWindowState& state, ro::net::Connection& link,
             ro::sim::World& world, std::uint32_t now_ms);

  void RequestRefine(std::int32_t index, ro::ui::WeaponRefineWindowState& state,
                     const WeaponRefineSettings& settings, std::uint32_t now_ms);
  void RequestClose(ro::ui::WeaponRefineWindowState& state);
  void ScheduleAutoRecast(ro::sim::RefineOutcome outcome, const WeaponRefineSettings& settings,
                          const ro::sim::World& world, bool window_open, std::uint32_t now_ms);
  void ScheduleAutoRefine(const WeaponRefineSettings& settings, const ro::sim::World& world,
                          bool window_open, std::uint32_t now_ms);
  void RetryRecast(const WeaponRefineSettings& settings, bool window_open, std::uint32_t now_ms);
  // Le motif qui interdit de continuer la chaîne, ou nul.
  [[nodiscard]] const char* AutoStopCause(const ro::sim::World& world) const;
  void ResetSession(ro::ui::WeaponRefineWindowState& state);

  [[nodiscard]] const ro::sim::RefinableWeapon* EntryAt(std::int32_t index) const;
  [[nodiscard]] std::string WeaponName(const ro::sim::World& world,
                                       const ro::sim::RefinableWeapon& weapon, bool bare) const;
  [[nodiscard]] std::string ItemName(std::uint32_t item_id) const;
  [[nodiscard]] ro::ui::SkinImage IconOf(std::uint32_t item_id) const;
  [[nodiscard]] int SlotsOf(std::uint32_t item_id) const;
  [[nodiscard]] int OreStock(const ro::sim::World& world) const;
  void Compose(const ro::sim::World& world, const ro::ui::WeaponRefineWindowState& state,
               const WeaponRefineSettings& settings);

  const ro::vfs::ItemTable* items_;
  const ro::vfs::ItemWords* words_;
  ro::render::ItemIcons* icons_;
  CreatorNames creators_;
  const CraftBook* book_ = nullptr;

  // ── CE QUI EST DÉJÀ LU ─────────────────────────────────────────────────────
  std::uint64_t seen_list_ = 0;
  std::uint64_t seen_result_ = 0;
  std::uint64_t seen_refusal_ = 0;

  // ── LA SESSION ─────────────────────────────────────────────────────────────
  std::vector<ro::sim::RefinableWeapon> entries_;
  bool empty_list_ = false;
  // Une tentative est partie : la liste est morte côté serveur.
  bool consumed_ = false;
  // « Le serveur attend encore une réponse. » Seule raison d'annuler.
  bool session_armed_ = false;
  bool awaiting_ = false;
  std::uint32_t awaiting_since_ = 0;
  Pending pending_ = Pending::kNone;
  std::int32_t pending_index_ = 0;
  std::uint32_t last_send_ms_ = 0;
  std::int32_t sent_index_ = -1;
  std::string sent_name_;
  int sent_refine_ = -1;

  // ── LA CHAÎNE ──────────────────────────────────────────────────────────────
  std::uint32_t auto_recast_at_ = 0;
  int auto_chain_ = 0;
  const char* stop_reason_ = nullptr;
  std::uint32_t auto_refine_at_ = 0;
  int auto_refine_count_ = 0;
  bool paused_ = false;
  // 🔴 LA CHAÎNE COMPLÈTE NE PART QU'APRÈS UN OUI DANS CETTE FENÊTRE. Le réglage
  // coché survit d'une partie à l'autre ; ce consentement-ci meurt avec la
  // session, pour qu'une fenêtre rouverte ne détruise rien d'elle-même.
  bool chain_consent_ = false;
  std::uint32_t recast_sent_at_ = 0;
  int recast_retries_ = 0;

  std::string last_result_;
  ro::ui::SkinTone last_result_tone_ = ro::ui::SkinTone::kText;
  CraftLog log_;

  // ── LA VUE ─────────────────────────────────────────────────────────────────
  // ⚠ LES VUES POINTENT DANS CES CHAÎNES : elles se prennent une fois tout
  // rempli.
  std::vector<std::string> bare_names_;
  std::vector<std::string> full_names_;
  std::vector<ro::ui::WeaponRefineRowView> rows_;
  std::array<std::string, ro::ui::kRefineOreCount> ore_names_;
  std::string confirm_name_;
  ro::ui::WeaponRefineView view_;
};

}  // namespace moonclient
