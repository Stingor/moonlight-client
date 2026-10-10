#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "ro/net/connection.h"
#include "ro/net/craft_session.h"
#include "ro/render/item_icons.h"
#include "ro/sim/craft_session.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/make_item_window.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "items/craft_book.h"

namespace moonclient {

// LES RÉGLAGES DE LA FABRICATION, et leurs défauts.
struct MakeItemSettings {
  bool show_owned = true;
  bool show_filter = true;
  bool hover_preview = true;
  bool show_history = false;
  bool log_time = true;
  // Entrée fabrique — et la fenêtre garde alors la touche.
  bool enter_key = false;
  // Relancer le SKILL après chaque fabrication. Jamais la fabrication elle-même.
  bool auto_recast = false;
  // 🔴 Relancer l'OBJET qui a ouvert la liste : il est DÉTRUIT à chaque usage.
  bool auto_reuse_item = false;
  // Le plafond d'exemplaires consommés par chaîne. Zéro : sans limite.
  int auto_reuse_max = 0;
};

// LA FABRICATION, VU DE LA PARTIE : ce que le client fait des listes du serveur.
//
// 🔴 UNE LISTE = UNE FABRICATION. Le serveur efface son skill en attente dès
// qu'il reçoit une réponse. La liste reste affichée, grisée ; la suivante
// demande de relancer — le skill, ou l'objet qui l'a ouverte.
//
// ⚠ LA LISTE NE DIT PAS CE QUI L'A OUVERTE. Un même paquet sert quatre skills,
// et les objets à script de fabrication ouvrent les mêmes listes : c'est ce que
// le joueur a ENVOYÉ juste avant qui le dit — voir `CraftSession::last_cast`.
//
// ⚠ ET LE SERVEUR NE RÉPOND PAS TOUJOURS : flèches, convertisseurs, forge et
// pharmacie créent l'objet sans un mot. Le résultat se CONSTATE alors dans le
// sac — le produit monte, ou le matériau baisse seul.
class MakeItemFlow {
 public:
  MakeItemFlow(const ro::vfs::ItemTable* items, const ro::vfs::ItemWords* words,
               ro::render::ItemIcons* icons);

  void SeeBook(const CraftBook* book) noexcept { book_ = book; }

  // Lit ce que le serveur a dit, et compose la vue. Rend vrai quand une liste
  // vient d'arriver — le serveur n'ayant qu'un skill en attente, l'affinage
  // doit alors se refermer.
  bool Refresh(ro::sim::World& world, ro::ui::MakeItemWindowState& state,
               const MakeItemSettings& settings, std::uint32_t now_ms);

  [[nodiscard]] const ro::ui::MakeItemView& view() const noexcept { return view_; }

  // Route ce que la fenêtre a demandé, fait avancer la relance, et émet.
  void Apply(const ro::ui::MakeItemWindowAction& asked, ro::ui::MakeItemWindowState& state,
             const MakeItemSettings& settings, ro::net::Connection& link, ro::sim::World& world,
             std::uint32_t now_ms);

 private:
  enum class Pending : std::uint8_t { kNone, kConfirm, kCancel, kRecast, kReuseItem };

  struct Material {
    std::uint32_t item_id = 0;
    int quantity = 0;
    // À posséder, jamais consommé : un guide.
    bool kept = false;
    // Ce que la fabrication REND : les flèches. N'entre jamais dans la faisabilité.
    bool output = false;
  };

  struct Entry {
    std::uint32_t item_id = 0;
    std::string name;
    int owned = 0;
    int craftable = -1;
    std::vector<Material> materials;
  };

  struct LogLine {
    std::string text;
    ro::ui::SkinTone tone = ro::ui::SkinTone::kText;
    std::string time;
  };

  // ── LA LECTURE DU SERVEUR (make_item_flow.cpp) ─────────────────────────────
  void TakeList(ro::sim::World& world, ro::ui::MakeItemWindowState& state,
                const MakeItemSettings& settings, std::uint32_t now_ms);
  void TakeResult(ro::sim::World& world, const MakeItemSettings& settings, std::uint32_t now_ms);
  void TakeMastery(const ro::sim::World& world);
  void TakeRefusal(std::uint32_t now_ms);
  void Probe(const ro::sim::World& world, const MakeItemSettings& settings, std::uint32_t now_ms);
  void Tick(const MakeItemSettings& settings, const ro::sim::World& world,
            ro::ui::MakeItemWindowState& state, std::uint32_t now_ms);
  void Flush(ro::ui::MakeItemWindowState& state, const MakeItemSettings& settings,
             ro::net::Connection& link, ro::sim::World& world, std::uint32_t now_ms);

  void RequestMake(ro::ui::MakeItemWindowState& state, bool selection_visible,
                   const ro::sim::World& world, std::uint32_t now_ms);
  void CloseAndCancel(ro::ui::MakeItemWindowState& state);
  void ScheduleAutoRecast(const MakeItemSettings& settings, const ro::sim::World& world,
                          std::uint32_t now_ms);
  void RetryRelaunch(const MakeItemSettings& settings, const ro::sim::World& world,
                     bool window_open, std::uint32_t now_ms);
  void SendReuseItem(const MakeItemSettings& settings, ro::net::Connection& link,
                     ro::sim::World& world);
  void Log(std::string text, ro::ui::SkinTone tone);

  void ResolveMaterials(Entry& entry) const;
  void RecountOwned(const ro::sim::World& world);
  [[nodiscard]] int Craftable(const Entry& entry, const ro::sim::World& world) const;
  [[nodiscard]] const Entry* EntryOf(std::uint32_t item_id) const;
  [[nodiscard]] std::string ItemName(std::uint32_t item_id) const;
  [[nodiscard]] ro::ui::SkinImage IconOf(std::uint32_t item_id) const;
  [[nodiscard]] bool AcceptsForgeSlots(std::uint32_t product) const;
  // La série ne tourne que si la relance de ce qui a ouvert la liste — skill ou
  // objet — est cochée.
  [[nodiscard]] bool BatchPossible(const MakeItemSettings& settings) const noexcept {
    return from_item_ ? settings.auto_reuse_item : settings.auto_recast;
  }

  // ── LA VUE (make_item_compose.cpp) ─────────────────────────────────────────
  void Compose(const ro::sim::World& world, const ro::ui::MakeItemWindowState& state,
               const MakeItemSettings& settings);
  void ComposeStale(const ro::sim::World& world);
  void ComposeRecipe(const ro::sim::World& world, const Entry& chosen);
  void ComposeChance(const ro::sim::World& world, const Entry& chosen);
  void ComposeForge(const ro::sim::World& world, std::uint32_t product);
  void ComposeFooter(const ro::sim::World& world, const ro::ui::MakeItemWindowState& state,
                     const MakeItemSettings& settings);
  void AddLine(std::vector<ro::ui::CraftTextLine>& into, std::string text, ro::ui::SkinTone tone,
               bool wrapped, const char* help_title = nullptr,
               std::vector<const char*> help_lines = {});

  const ro::vfs::ItemTable* items_;
  const ro::vfs::ItemWords* words_;
  ro::render::ItemIcons* icons_;
  const CraftBook* book_ = nullptr;

  // ── CE QUI EST DÉJÀ LU ─────────────────────────────────────────────────────
  std::uint64_t seen_list_ = 0;
  std::uint64_t seen_result_ = 0;
  std::uint64_t seen_mastery_ = 0;
  std::uint64_t seen_refusal_ = 0;

  // ── LA SESSION ─────────────────────────────────────────────────────────────
  std::vector<Entry> entries_;
  // La dernière liste NON VIDE, gardée quand la suivante arrive vide : c'est
  // elle qui dit ce qui manque.
  std::vector<Entry> stale_entries_;
  ro::sim::MakeListKind kind_ = ro::sim::MakeListKind::kArrow;
  std::uint16_t make_type_ = 0;
  bool empty_list_ = false;
  // « Le serveur honore-t-il encore cette liste ? » — pas « est-elle affichée ».
  bool list_armed_ = false;
  bool arrow_list_ = false;
  int cook_mastery_ = -1;

  // Ce qui a ouvert la liste : un skill, ou un objet.
  int skill_id_ = 0;
  int skill_level_ = 0;
  bool from_item_ = false;
  std::uint32_t source_item_id_ = 0;
  std::string source_item_name_;

  // Les matériaux optionnels de la PROCHAINE forge, et le produit qu'ils visent.
  std::array<std::uint32_t, ro::net::kProduceExtraSlots> forge_slots_ = {};
  std::uint32_t forge_for_id_ = 0;

  // ── LA DEMANDE EN VOL, et sa sonde ─────────────────────────────────────────
  Pending pending_ = Pending::kNone;
  std::uint32_t pending_id_ = 0;
  bool awaiting_ = false;
  std::uint32_t sent_at_ = 0;
  std::uint32_t last_sent_id_ = 0;
  std::string last_sent_name_;
  int sent_owned_ = -1;
  std::uint32_t sent_material_ = 0;
  int sent_material_owned_ = -1;

  // ── LA SÉRIE ET LA RELANCE ─────────────────────────────────────────────────
  int batch_left_ = 0;
  bool batch_fire_ = false;
  std::uint32_t auto_recast_at_ = 0;
  int auto_chain_ = 0;
  int auto_items_used_ = 0;
  bool auto_ours_ = false;
  std::string stop_reason_;
  std::uint32_t relaunch_sent_at_ = 0;
  int relaunch_retries_ = 0;
  int reuse_owned_before_ = -1;

  std::string last_result_;
  ro::ui::SkinTone last_result_tone_ = ro::ui::SkinTone::kText;
  std::deque<LogLine> history_;

  // ── LA VUE ─────────────────────────────────────────────────────────────────
  // ⚠ LES VUES POINTENT DANS CES CONTENEURS : elles se prennent une fois tout
  // rempli.
  std::string title_;
  std::vector<ro::ui::MakeItemRowView> rows_;
  std::vector<std::string> stale_texts_;
  std::vector<ro::ui::MakeStaleLineView> stale_lines_;
  std::string recipe_title_;
  std::vector<std::string> material_labels_;
  std::vector<ro::ui::MakeMaterialView> materials_;
  // Les lignes composées, et ce qu'elles portent : textes et listes d'aide.
  std::deque<std::string> line_texts_;
  std::deque<std::vector<const char*>> line_helps_;
  std::vector<ro::ui::CraftTextLine> chance_lines_;
  std::vector<ro::ui::CraftTextLine> forge_notes_;
  std::vector<ro::ui::CraftTextLine> footer_notes_;
  std::array<std::vector<std::string>, ro::net::kProduceExtraSlots> choice_labels_;
  std::array<std::vector<ro::ui::ForgeChoiceView>, ro::net::kProduceExtraSlots> choices_;
  std::array<std::string, ro::net::kProduceExtraSlots> chosen_labels_;
  std::string relaunch_tip_;
  std::string batch_progress_;
  std::vector<ro::ui::CraftLogLineView> history_view_;
  ro::ui::MakeItemView view_;
};

}  // namespace moonclient
