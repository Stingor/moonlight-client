// LA BASE DE DONNÉES, vue de la boucle : ce que sa fenêtre a rendu, la vue
// qu'elle dessinera, et la fiche de monstre qu'elle héberge (ADR 0116).
//
// ⚠ ELLE PASSE JUSTE AVANT LA FICHE DE MONSTRE, que `AdvanceMonsterSheet`
// appelle d'abord : les gestes faits sur la fiche hébergée sont ceux que la
// fiche joue à cette même image.

#include <optional>
#include <utility>

#include "ro/ui/database_window.h"

#include "game/game.h"

namespace moonclient {

void Game::AdvanceDatabase(std::uint32_t now_ms) {
  DatabaseWindows& windows = windows_.monster_sheet.database;
  // La fiche de monstre était-elle dans le panneau à l'image dessinée ?
  const bool hosted_before = windows.view != nullptr && windows.view->monster != nullptr;
  ro::ui::DatabaseAnswer& answer = windows.answer;

  // ── L'OBJET DE LA FICHE : le Marché, et les gestes de tout objet ──────────
  if (answer.market_item != 0) {
    pieces_.trade.market().AskOpen(std::exchange(answer.market_item, 0));
  }
  FollowCatalogItem(std::exchange(answer.item, {}), now_ms);

  // ── LA FICHE DE MONSTRE HÉBERGÉE ──────────────────────────────────────────
  //
  // 🔴 SES LIENS RESTENT DANS LA BASE : un butin cliqué ouvre sa fiche d'objet,
  // l'homonyme la sienne, au lieu d'une description ou d'une autre fenêtre. Le
  // reste — survols, menus, cartes d'apparition, sprite — passe par la fiche
  // de monstre, comme dans sa propre fenêtre.
  ro::ui::MonsterSheetAnswer monster = std::exchange(answer.monster, {});
  if (monster.item.gesture == ro::ui::ChatLinkGesture::kDescribe && monster.item.item_id != 0) {
    database_.AskOpen(
        ro::ui::DatabaseTarget{.kind = ro::ui::DatabaseKind::kItem, .id = monster.item.item_id});
    monster.item.gesture = ro::ui::ChatLinkGesture::kNothing;
  }
  if (monster.open_monster != 0) {
    database_.AskOpen(ro::ui::DatabaseTarget{.kind = ro::ui::DatabaseKind::kMonster,
                                             .id = std::exchange(monster.open_monster, 0)});
  }
  if (hosted_before) {
    windows_.monster_sheet.answer = monster;
  }

  database_.Advance(windows, DatabaseContext{.link = link_,
                                             .world = world_,
                                             .market = pieces_.trade.market().market(),
                                             .online = link_.is_open(),
                                             .vote_shop = pieces_.cash_shop.offers()});
  // « Y ALLER » vers un PNJ qui vend l'objet : la navigation y guide.
  if (const std::optional<ro::sim::MarketOffer> seller = database_.TakeGoTo(); seller.has_value()) {
    navi_.GuideToPlace(seller->map_name, seller->cell_x, seller->cell_y);
  }
}

bool Game::HostMonsterSheet(MonsterSheet& sheet) {
  DatabaseWindows& database = windows_.monster_sheet.database;
  ro::ui::MonsterSheetWindowState& state = windows_.monster_sheet.state;
  const ro::vfs::ServerDb* const db = database_.db();
  // UNE FICHE OUVERTE EN JEU, BASE OUVERTE : elle passe dans la base, qui la
  // connaît. Une classe de sprite que la base ne nomme pas reste à part.
  if (state.open && database.state.open && db != nullptr && sheet.shown_id() != 0 &&
      db->FindMonster(sheet.shown_id()) != nullptr) {
    ro::ui::OpenDatabaseOn(
        database.state,
        ro::ui::DatabaseTarget{.kind = ro::ui::DatabaseKind::kMonster, .id = sheet.shown_id()});
    state.open = false;
  }
  const std::uint32_t shown = database_.ShownMonster(database);
  // ⚠ UN SEUL COMPOSANT DE FICHE : tant que la fenêtre à part en montre une,
  // le panneau de la base n'en montre pas.
  if (shown == 0 || state.open) {
    return false;
  }
  if (!sheet.HasBase(shown)) {
    if (std::optional<ro::sim::MonsterSheet> base = database_.BaseSheetOf(shown);
        base.has_value()) {
      sheet.NoteBase(shown, std::move(*base));
    }
  }
  sheet.Show(shown);
  return true;
}

}  // namespace moonclient
