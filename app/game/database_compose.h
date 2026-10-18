#pragma once

// CE QUE LA BASE DE DONNÉES COMPOSE À PARTIR DE LA BASE DU SERVEUR, à part de
// son étage : des décisions pures, qui se jugent sur un banc sans fenêtre ni
// fil. L'étage (`DatabaseStage`) les exécute. Voir l'ADR 0116.

#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "ro/sim/inventory.h"
#include "ro/sim/market.h"
#include "ro/sim/monster_sheet.h"
#include "ro/ui/database_window.h"
#include "ro/vfs/server_db.h"
#include "ro/vfs/server_db_search.h"

namespace moonclient {

// UNE LIGNE « libellé : valeur » de la fiche d'un objet, ses textes possédés.
struct ItemFactLine {
  std::string label;
  std::string value;
};

// Ce que la fiche d'un objet dit de lui, dans l'ordre où elle le montre : type,
// poids, prix, attaque, défense, emplacements, niveaux, métiers. Une ligne vide
// de sens (une attaque nulle) n'est pas écrite.
[[nodiscard]] std::vector<ItemFactLine> ItemFactsOf(const ro::vfs::ServerItem& item);

// LES MONSTRES QUI LÂCHENT UN OBJET, aux taux réels, rangés comme les deux
// onglets de la fiche : les ordinaires d'un côté, mini-boss et MVP de l'autre.
// ⚠ LES NOMS POINTENT DANS `db`, qui doit leur survivre.
struct DropperRows {
  std::vector<ro::ui::DescriptionDropRow> ordinary;
  std::vector<ro::ui::DescriptionDropRow> boss;
};
[[nodiscard]] DropperRows DropperRowsOf(const ro::vfs::ServerDb& db, std::uint32_t item_id);

// La recherche que la saisie et les filtres de la fenêtre demandent.
[[nodiscard]] ro::vfs::ItemSearch ItemSearchOf(const ro::ui::DatabaseFilters& filters,
                                               std::string_view text);
[[nodiscard]] ro::vfs::MonsterSearch MonsterSearchOf(const ro::ui::DatabaseFilters& filters,
                                                     std::string_view text);

// Le libellé d'un `Type` de la base, ou le texte tel quel quand la fenêtre ne
// le connaît pas. Traduit.
[[nodiscard]] std::string ItemTypeLabel(std::string_view server_type);

// LE `Type` DE LA BASE QUI RÉPOND À UNE FAMILLE DU CLIENT, pour l'objet que la
// base ne connaît pas : les consommables y sont `Usable`. Vide pour `kOther`,
// que la base ne nomme pas.
[[nodiscard]] std::string_view ItemDbTypeOfCategory(ro::sim::ItemCategory category);

// LE TYPE D'UN OBJET DU SAC, tel que `ItemTypeLabel` le nomme : celui que la
// base donne à l'objet, lue et le connaissant ; à défaut, celui de la famille
// que le fil a dite au client ; « Autre » pour une famille que la base ne
// nomme pas. Traduit.
[[nodiscard]] std::string ItemTypeLabelOf(const ro::sim::InventoryItem& item,
                                          const ro::vfs::ServerDb* db);

// Le nom d'un objet tel que le joueur le lit.
using ItemNamer = std::function<std::string(std::uint32_t item_id)>;

// LA FICHE D'UN MONSTRE TIRÉE DE LA BASE SEULE : ce que la fenêtre de monstre
// montre tant que le serveur n'a pas répondu, ou hors ligne. Les taux sont les
// taux RÉELS, les apparitions celles des scripts chargés. ⚠ NI RÉSISTANCES, NI
// COMPÉTENCES, NI GAIN D'EXP : seul le serveur les sait.
[[nodiscard]] ro::sim::MonsterSheet MonsterSheetOf(const ro::vfs::ServerMonster& monster,
                                                   const ro::vfs::ServerDb& db,
                                                   const ItemNamer& item_name);

// `/db` ouvre la base de données ; `/db <texte>` l'ouvre sur cette recherche.
// ⚠ UNE COMMANDE DE CE CLIENT : elle ne part jamais au serveur. Rien : la ligne
// n'est pas cette commande.
inline constexpr std::string_view kDatabaseCommand = "/db";
[[nodiscard]] std::optional<std::string> ReadDatabaseCommand(std::string_view line);

// « VENDU CHEZ » : une ligne par boutique de PNJ, ses textes possédés.
struct SellerLine {
  std::string npc;
  std::string place;
  std::string price;
};
[[nodiscard]] std::vector<SellerLine> SellerLinesOf(std::span<const ro::sim::MarketOffer> offers,
                                                    const ItemNamer& item_name);

}  // namespace moonclient
