#pragma once

// LA BASE DE DONNÉES, DANS LA BOUCLE DE LA PARTIE : l'intention d'ouvrir — sur
// une fiche ou sans —, la lecture paresseuse des tables, la liste cherchée et
// filtrée, la part de la base dans la fiche d'un objet — « Général », les
// sources et scripts de repli, « Vendu chez » demandé au Marché —, dans ses
// deux hôtes. La fiche d'un objet est celle de la description, celle d'un
// monstre celle de `MonsterSheet`, que `Game` lui prête. Voir l'ADR 0116.

#include <array>
#include <cstddef>
#include <cstdint>
#include <future>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <utility>
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
  std::uint32_t now_ms = 0;
};

class DatabaseStage {
 public:
  // Les tables de l'installation et les mots du jeu. Facultatifs ; leur
  // adresse ne bouge pas de la partie.
  void See(ro::vfs::Vfs* vfs, const ro::vfs::ItemTable* items, ro::render::ItemIcons* icons,
           const ro::vfs::ItemWords* words);
  // LA FICHE D'UN OBJET DE LA BASE EMPRUNTE CELLE DE LA FENÊTRE DE
  // DESCRIPTION : voir `DescriptionView::BorrowFrom`. Après `See`.
  void BorrowSheetFrom(DescriptionView& owner, ro::render::ActorWardrobe* wardrobe);

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

  // L'IMAGE : les ouvertures, la lecture, puis la vue. La fiche d'un objet
  // montré se compose ici ; `Game` la décore ensuite (`item_sheet`).
  void Advance(DatabaseWindows& windows, const DatabaseContext& context);
  void NoteMonsterView(DatabaseWindows& windows, const ro::ui::MonsterSheetView* monster);

  // CE QU'UNE FICHE D'OBJET A DEMANDÉ à son image — « Vendu chez » montré,
  // « Réessayer », « Y aller » —, joué à la composition suivante de cet hôte.
  void NoteSheetAsked(ro::ui::ItemSheetHost host, const ro::ui::DescriptionWindowAction& asked);
  // LA PART DE LA BASE DANS LA FICHE D'UN OBJET, posée dans `sheet` APRÈS ses
  // faits du serveur. ⚠ LIT LA BASE À LA PREMIÈRE DEMANDE, à côté de la
  // boucle : la description en profite dès qu'elle s'ouvre.
  void FillItemSheet(ro::ui::ItemSheetHost host, std::uint32_t item_id,
                     const DatabaseContext& context, DescriptionView& sheet);

  // La base, lue ; nulle tant qu'elle ne l'est pas.
  [[nodiscard]] const ro::vfs::ServerDb* db() const noexcept {
    return db_.known() ? &db_ : nullptr;
  }
  // LA BASE, LUE OU NON : son adresse ne bouge pas de la partie, et `known()`
  // dit le jour où elle est lue. Pour qui la consulte à chaque image sans la
  // demander — le bloc de faits des descriptions.
  [[nodiscard]] const ro::vfs::ServerDb& base() const noexcept { return db_; }
  // Le monstre ou l'objet que la fenêtre montre ; zéro quand elle n'en montre
  // pas.
  [[nodiscard]] std::uint32_t ShownMonster(const DatabaseWindows& windows) const;
  [[nodiscard]] std::uint32_t ShownItem(const DatabaseWindows& windows) const;
  // LA FICHE DE L'OBJET MONTRÉ, à décorer comme la description ; nulle quand la
  // fenêtre n'en montre pas.
  [[nodiscard]] DescriptionView* item_sheet() noexcept {
    return view_.item != nullptr && description_.has_value() ? &*description_ : nullptr;
  }
  // La fiche d'un monstre tirée de la base, pour la fiche de monstre.
  [[nodiscard]] std::optional<ro::sim::MonsterSheet> BaseSheetOf(std::uint32_t monster_id) const;
  // « Y aller » vers un PNJ vendeur, une fois.
  [[nodiscard]] std::optional<ro::sim::MarketOffer> TakeGoTo() noexcept;

 private:
  static constexpr std::size_t kSheetHosts = 2;
  [[nodiscard]] static std::size_t HostRank(ro::ui::ItemSheetHost host) noexcept {
    return host == ro::ui::ItemSheetHost::kDatabase ? 1 : 0;
  }
  // Ce qu'une fiche a demandé à l'image d'avant.
  struct SheetAsked {
    bool sellers_shown = false;
    bool ask_sellers = false;
    int go_to_seller = -1;
  };
  // LA PART D'UN HÔTE : les textes que sa fiche montre, refaits au changement
  // d'objet seulement.
  struct SheetPart {
    std::uint32_t item_id = 0;
    std::vector<ItemFactLine> facts;
    std::vector<ro::ui::ItemSheetFact> fact_views;
    DropperRows droppers;
    std::uint32_t seller_rows_for = 0;
    std::vector<SellerLine> seller_lines;
    std::vector<ro::ui::ItemSheetSellerRow> seller_rows;
    SheetAsked asked;
  };

  void OpenAsked(DatabaseWindows& windows);
  void Load();
  void Index();
  void ComposeRows(const ro::ui::DatabaseWindowState& state);
  void ComposeItem(std::uint32_t item_id, const DatabaseContext& context);
  // CE QUE « VENDU CHEZ » DEMANDE AU MARCHÉ : rien, l'objet une fois, ou de
  // nouveau après un refus.
  enum class SellersAsk : std::uint8_t { kNever, kOnce, kRetry };
  void FollowSellers(SheetPart& part, std::uint32_t item_id, const DatabaseContext& context,
                     SellersAsk ask, ItemSheetBase& base);
  [[nodiscard]] std::string ItemName(std::uint32_t item_id) const;

  ro::vfs::Vfs* vfs_ = nullptr;
  TradeItems items_;
  std::optional<DescriptionView> description_;

  bool asked_open_ = false;
  std::optional<ro::ui::DatabaseTarget> asked_target_;
  std::optional<std::string> asked_query_;
  std::optional<ro::sim::MarketOffer> go_to_;

  // ── LA LECTURE, à la première demande ─────────────────────────────────────
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

  // ── LA FICHE D'UN OBJET, dans chacun de ses deux hôtes ───────────────────
  std::array<SheetPart, kSheetHosts> sheets_;

  // « VENDU CHEZ » : la réponse par objet, et ce qu'on attend. ⚠ UNE SEULE
  // DEMANDE EN VOL, pour les deux hôtes.
  std::map<std::uint32_t, std::vector<ro::sim::MarketOffer>> sellers_;
  std::uint32_t sellers_asked_ = 0;
  std::uint16_t sellers_query_ = 0;
  std::uint32_t refusal_seen_ = 0;
  std::uint32_t answer_seen_ = 0;
  bool sellers_refused_ = false;
  std::string sellers_status_;

  ro::ui::DatabaseView view_;
};

}  // namespace moonclient
