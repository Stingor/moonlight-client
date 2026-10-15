#pragma once

// LA BASE DE DONNÉES, DANS LA BOUCLE DE LA PARTIE : l'intention d'ouvrir — sur
// une fiche ou sans —, la lecture paresseuse des tables, la liste cherchée et
// filtrée, la fiche d'un objet composée, « Vendu chez » demandé au Marché. La
// fiche d'un monstre est celle de `MonsterSheet`, que `Game` lui prête. Voir
// l'ADR 0116.

#include <cstdint>
#include <future>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/sim/market.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/database_window.h"
#include "ro/vfs/server_db.h"
#include "ro/vfs/vfs.h"

#include "game/database_compose.h"
#include "game/database_windows.h"
#include "game/trade_items.h"
#include "items/description_view.h"

namespace moonclient {

// CE QUE L'ÉTAGE REÇOIT À CHAQUE IMAGE, le temps de l'appel.
struct DatabaseContext {
  ro::net::Connection& link;
  const ro::sim::World& world;
  ro::sim::Market& market;
  // Connecté à un serveur qui comprend le Marché : « Vendu chez » se demande.
  bool online = false;
  std::span<const ro::ui::VoteShopOffer> vote_shop;
};

class DatabaseStage {
 public:
  // Les tables de l'installation et les mots du jeu. Facultatifs ; leur
  // adresse ne bouge pas de la partie.
  void See(ro::vfs::Vfs* vfs, const ro::vfs::ItemTable* items, ro::render::ItemIcons* icons,
           const ro::vfs::ItemWords* words);

  // 🔴 L'INTENTION UNIQUE : « ouvre la base de données », sur cette fiche ou
  // sans. Le menu principal, la touche, `/db`, le geste d'item, le menu du sac,
  // la description et la fiche de monstre passent tous par elle.
  void AskOpen(std::optional<ro::ui::DatabaseTarget> target = std::nullopt) {
    asked_open_ = true;
    asked_target_ = target;
  }
  // `/db <texte>` : la fenêtre s'ouvre, la saisie posée.
  void AskOpenOnQuery(std::string query) {
    asked_open_ = true;
    asked_query_ = std::move(query);
  }

  // L'IMAGE : les ouvertures, la lecture, ce que la fenêtre a rendu, puis la
  // vue. `monster` est la vue de LA fiche de monstre, quand la fenêtre montre
  // un monstre ; nulle sinon.
  void Advance(DatabaseWindows& windows, const DatabaseContext& context);
  void NoteMonsterView(DatabaseWindows& windows, const ro::ui::MonsterSheetView* monster);

  // La base, lue ; nulle tant qu'elle ne l'est pas.
  [[nodiscard]] const ro::vfs::ServerDb* db() const noexcept {
    return db_.known() ? &db_ : nullptr;
  }
  // Le monstre que la fenêtre montre ; zéro quand elle n'en montre pas.
  [[nodiscard]] std::uint32_t ShownMonster(const DatabaseWindows& windows) const;
  // La fiche d'un monstre tirée de la base, pour la fiche de monstre.
  [[nodiscard]] std::optional<ro::sim::MonsterSheet> BaseSheetOf(std::uint32_t monster_id) const;
  // « Y aller » vers un PNJ vendeur, une fois.
  [[nodiscard]] std::optional<ro::sim::MarketOffer> TakeGoTo() noexcept;

 private:
  void OpenAsked(DatabaseWindows& windows);
  void Load();
  void Index();
  void ComposeRows(const ro::ui::DatabaseWindowState& state);
  // `sellers_shown` : l'onglet « Vendu chez » était montré à l'image d'avant.
  void ComposeItem(std::uint32_t item_id, const DatabaseContext& context, bool sellers_shown);
  // CE QUE « VENDU CHEZ » DEMANDE AU MARCHÉ : rien, l'objet une fois, ou de
  // nouveau après un refus.
  enum class SellersAsk : std::uint8_t { kNever, kOnce, kRetry };
  void FollowSellers(std::uint32_t item_id, const DatabaseContext& context, SellersAsk ask);
  [[nodiscard]] std::string ItemName(std::uint32_t item_id) const;

  ro::vfs::Vfs* vfs_ = nullptr;
  TradeItems items_;
  std::optional<DescriptionView> description_;

  bool asked_open_ = false;
  std::optional<ro::ui::DatabaseTarget> asked_target_;
  std::optional<std::string> asked_query_;
  std::optional<ro::sim::MarketOffer> go_to_;

  // ── LA LECTURE, à la première ouverture ───────────────────────────────────
  bool load_started_ = false;
  std::future<ro::core::Result<ro::vfs::ServerDb>> loading_;
  ro::vfs::ServerDb db_;
  std::string status_;
  std::vector<std::string> item_keys_;
  std::vector<std::string> monster_keys_;
  std::vector<std::string> item_names_;

  // ── LA LISTE, refaite quand la recherche change ─────────────────────────
  struct Asked {
    int tab = -1;
    std::string text;
    ro::ui::DatabaseFilters filters;
    bool operator==(const Asked&) const = default;
  };
  std::optional<Asked> listed_;
  std::vector<std::string> row_notes_;
  std::vector<ro::ui::DatabaseRowView> rows_;
  std::string summary_;

  // ── LA FICHE D'UN OBJET ───────────────────────────────────────────────────
  std::uint32_t item_shown_ = 0;
  std::vector<ItemFactLine> facts_;
  std::vector<ro::ui::DatabaseFact> fact_views_;
  std::vector<std::string> dropper_names_;
  std::vector<std::string> dropper_rates_;
  std::vector<ro::ui::DatabaseDropperRow> droppers_;
  ro::ui::DatabaseItemView item_view_;

  // « VENDU CHEZ » : la réponse par objet, et ce qu'on attend.
  std::map<std::uint32_t, std::vector<ro::sim::MarketOffer>> sellers_;
  std::uint32_t sellers_asked_ = 0;
  std::uint16_t sellers_query_ = 0;
  std::uint32_t seller_rows_for_ = 0;
  std::uint32_t refusal_seen_ = 0;
  std::uint32_t answer_seen_ = 0;
  bool sellers_refused_ = false;
  std::string sellers_status_;
  std::vector<SellerLine> seller_lines_;
  std::vector<ro::ui::DatabaseSellerRow> seller_rows_;

  ro::ui::DatabaseView view_;
};

}  // namespace moonclient
