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

// UN TAUX EN CENTIÈMES DE POUR CENT, écrit « 12,50 % ».
[[nodiscard]] std::string DropRateText(std::uint32_t rate_hundredths);

// La recherche que la saisie et les filtres de la fenêtre demandent.
[[nodiscard]] ro::vfs::ItemSearch ItemSearchOf(const ro::ui::DatabaseFilters& filters,
                                               std::string_view text);
[[nodiscard]] ro::vfs::MonsterSearch MonsterSearchOf(const ro::ui::DatabaseFilters& filters,
                                                     std::string_view text);

// Le libellé d'un `Type` de la base, ou le texte tel quel quand la fenêtre ne
// le connaît pas. Traduit.
[[nodiscard]] std::string ItemTypeLabel(std::string_view server_type);

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
