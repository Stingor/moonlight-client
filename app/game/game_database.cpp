// LA BASE DE DONNÉES, vue de la boucle : ce que sa fenêtre a rendu, la vue
// qu'elle dessinera, et la fiche de monstre qu'elle héberge (ADR 0116).
//
// ⚠ ELLE PASSE JUSTE AVANT LA FICHE DE MONSTRE, que `AdvanceMonsterSheet`
// appelle d'abord : les gestes faits sur la fiche hébergée sont ceux que la
// fiche joue à cette même image.

#include <optional>
#include <utility>

#include "ro/ui/database_window.h"

#include "game/bug_report_context.h"
#include "game/game.h"

namespace moonclient {

void Game::AdvanceDatabase(std::uint32_t now_ms) {
  DatabaseWindows& windows = windows_.monster_sheet.database;
  // La fiche de monstre était-elle dans le panneau à l'image dessinée ?
  const bool hosted_before = windows.view != nullptr && windows.view->monster != nullptr;
  ro::ui::DatabaseAnswer& answer = windows.answer;

  // ── LA FICHE D'UN OBJET : les demandes de la description, routées de même ─
  const std::uint32_t sheet_item = database_.ShownItem(windows);
  const ro::ui::DescriptionWindowAction sheet = std::exchange(answer.item, {});
  if (const DescriptionView* const shown = database_.item_sheet();
      shown != nullptr && sheet_item != 0) {
    RouteItemSheet(sheet, sheet_item, *shown, ro::ui::ItemSheetHost::kDatabase, now_ms);
    // « SIGNALER UN BUG » : le formulaire de la description, pré-rempli de
    // l'objet de la fiche.
    if (sheet.report_bug) {
      AskBugReport(windows_.bug_report, shown->BugReport(sheet_item));
    }
  }

  // ── UNE LIGNE DE LA LISTE : les gestes de tout objet ou de tout monstre ───
  const ro::ui::ChatLinkAsk row = std::exchange(answer.list, {});
  if (row.hovered_id != 0 && row.hovered_kind == ro::ui::ChatLinkKind::kMonster &&
      monster_sheet_.has_value()) {
    monster_sheet_->NoteHovered(row.hovered_id, /*by_view=*/false);
  }
  if (row.kind == ro::ui::ChatLinkKind::kMonster && row.item_id != 0) {
    FollowMonsterGesture(row.gesture, DatabaseMonsterLink(row.item_id), /*by_view=*/false, now_ms);
  } else {
    ro::ui::ChatLinkAsk item_row = row;
    if (item_row.hovered_kind != ro::ui::ChatLinkKind::kItem) {
      item_row.hovered_id = 0;
    }
    FollowCatalogItem(item_row, now_ms);
  }

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

  database_.Advance(windows, DatabaseContextNow(now_ms));
  // LA FICHE D'UN OBJET SE DÉCORE COMME LA DESCRIPTION : boutique, Atlas,
  // staff, et le pantin de sa pièce.
  if (DescriptionView* const shown = database_.item_sheet(); shown != nullptr) {
    const std::uint32_t item_id = database_.ShownItem(windows);
    DecorateItemSheet(*shown, item_id);
    shown->RefreshDoll(world_, windows.state.item_sheet.doll,
                       pieces_.cash_shop.hat_effects().OrdinalOf(item_id) != 0, styles_, now_ms);
  }
  // « Y ALLER » vers un PNJ qui vend l'objet : la navigation y guide.
  if (const std::optional<ro::sim::MarketOffer> seller = database_.TakeGoTo(); seller.has_value()) {
    navi_.GuideToPlace(seller->map_name, seller->cell_x, seller->cell_y);
  }
}

MonsterLink Game::DatabaseMonsterLink(std::uint32_t monster_id) const {
  MonsterLink link{.monster_id = monster_id};
  if (const ro::vfs::ServerDb* const db = database_.db(); db != nullptr) {
    if (const ro::vfs::ServerMonster* const monster = db->FindMonster(monster_id);
        monster != nullptr) {
      link.rank = monster->rank;
      link.name = monster->name;
    }
  }
  return link;
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
