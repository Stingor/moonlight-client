// LA FICHE D'UN OBJET, VUE DE LA BOUCLE, dans ses deux hôtes : la fenêtre de
// description et le panneau de droite de la base de données (ADR 0116).
//
// 🔴 UN SEUL ROUTAGE : ce que la fiche demande — un lien suivi, un geste sur un
// monstre ou un objet de ses onglets, « Marché », « alootid », un livre, le
// vote shop, l'Atlas, « @item », « Vendu chez » — ne dépend pas de l'hôte qui
// l'a dessinée. Une copie par hôte en laisserait un jour une sans un geste.

#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/core/printed.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"

#include "game/game.h"

namespace moonclient {

DatabaseContext Game::DatabaseContextNow(std::uint32_t now_ms) {
  return DatabaseContext{.link = link_,
                         .world = world_,
                         .market = pieces_.trade.market().market(),
                         .online = link_.is_open(),
                         .link_facts = ItemLinkFactsNow(),
                         .now_ms = now_ms};
}

ro::ui::ItemLinkFacts Game::ItemLinkFactsNow() {
  // L'ALBUM S'OUVRE PARTOUT EN PARTIE : c'est au serveur qu'il demande son état,
  // et le lien seul le permet.
  return ro::ui::ItemLinkFacts{.vote_shop = pieces_.cash_shop.offers(),
                               .album = card_album_.LinkFacts(link_.is_open())};
}

void Game::ShowCardInAlbum(std::uint32_t card_id, std::uint32_t now_ms) {
  card_album_.Show(card_id, windows_.card_album, link_, now_ms);
}

void Game::DecorateItemSheet(DescriptionView& sheet, std::uint32_t item_id) {
  sheet.NoteLinkFacts(ItemLinkFactsNow());
  sheet.NoteHatEffect(pieces_.cash_shop.hat_effects().OrdinalOf(item_id) != 0);
  // L'ÉTIQUETTE « CRAFT », que seul l'Atlas sait composer.
  std::string craft_label;
  std::string craft_tip;
  if (craft_atlas_.has_value() && craft_atlas_->CraftTagOf(item_id, &craft_label, &craft_tip)) {
    sheet.NoteCraftTag(std::move(craft_label), std::move(craft_tip));
  }
  // LE BOUTON DU VOTE SHOP, que seule la boutique sait composer — et seulement
  // quand l'item est à son catalogue.
  if (const std::optional<CashShopStage::Offer> offer = pieces_.cash_shop.Find(item_id);
      offer.has_value()) {
    sheet.NoteVoteShopTag(ro::core::Printed(ro::core::Tr("Vote Shop : %d pts"), offer->price));
  }
  // LA LIGNE « @item », pour le staff seulement.
  sheet.NoteStaff(Rights().staff_tools, pieces_.item_properties.IsStackable(item_id),
                  StaffFreeBagSlots(world_.inventory()));
}

void Game::AdvanceHoveredDollEffect(std::uint32_t now_ms) {
  // L'EFFET DE CHAPEAU DU PANTIN SURVOLÉ. ⚠ APRÈS LES PANTINS, composés plus
  // haut, qui effacent chacun le rappel de l'image d'avant ; un seul se survole
  // à la fois, et la boutique passe d'abord.
  if (!pieces_.doll_effects.has_value()) {
    return;
  }
  const ro::sim::HatEffectItems& hats = pieces_.cash_shop.hat_effects();
  ro::ui::DescriptionWindowState& database_sheet = windows_.monster_sheet.database.state.item_sheet;
  if (windows_.cash_shop.state.doll.wanted) {
    pieces_.doll_effects->Advance(
        windows_.cash_shop.state.doll, hats.OrdinalOf(windows_.cash_shop.state.doll_item),
        pieces_.cash_shop.doll_framing(), pieces_.cash_shop.doll_body(), world_, *stage_, now_ms);
  } else if (windows_.description.open && windows_.description.doll.wanted) {
    pieces_.doll_effects->Advance(
        windows_.description.doll, hats.OrdinalOf(windows_.description.item_id),
        item_description_->doll_framing(), item_description_->doll_body(), world_, *stage_, now_ms);
  } else if (const DescriptionView* const shown = database_.item_sheet();
             database_sheet.doll.wanted && shown != nullptr) {
    // LA FICHE D'UN OBJET DANS LA BASE, qui a son propre pantin.
    pieces_.doll_effects->Advance(
        database_sheet.doll, hats.OrdinalOf(database_.ShownItem(windows_.monster_sheet.database)),
        shown->doll_framing(), shown->doll_body(), world_, *stage_, now_ms);
  } else {
    pieces_.doll_effects->Rest();
  }
}

void Game::RouteItemSheet(const ro::ui::DescriptionWindowAction& asked, std::uint32_t item_id,
                          const DescriptionView& sheet, ro::ui::ItemSheetHost host,
                          std::uint32_t now_ms) {
  FollowDescriptionLink(asked, now_ms);
  // ⚠ LES FAITS DU SERVEUR SONT PARTAGÉS : la fenêtre les tient pour les deux.
  item_description_->RefreshSources(item_id, asked);
  // « VENDU CHEZ » : ce que la fiche a montré ou redemandé, joué à sa
  // composition suivante.
  database_.NoteSheetAsked(host, asked);

  // UN GESTE SUR UN LIEN DE SES ONGLETS — table de tirage, sources, combos,
  // en-tête. ⚠ DÉCRIRE UN AUTRE OBJET N'ARRIVE PAS ICI : la fiche navigue
  // elle-même. Le reste est ce qu'on fait de tout lien.
  const ro::ui::ChatLinkAsk& link = asked.link;
  if (link.hovered_id != 0 && link.hovered_kind == ro::ui::ChatLinkKind::kMonster &&
      monster_sheet_.has_value()) {
    monster_sheet_->NoteHovered(link.hovered_id, /*by_view=*/false);
  }
  if (link.item_id != 0 && link.kind == ro::ui::ChatLinkKind::kMonster) {
    // Le nom de la table de tirage, celui des sources, ou celui de la base ; le
    // rang de la base quand elle le sait, et l'aperçu dira le vrai.
    MonsterLink monster = DatabaseMonsterLink(link.item_id);
    if (std::string_view named = sheet.OddsNameOf(link.item_id); !named.empty()) {
      monster.name = std::string(named);
    } else if (named = sheet.SourceNameOf(link.item_id); !named.empty()) {
      monster.name = std::string(named);
    }
    FollowMonsterGesture(link.gesture, monster, /*by_view=*/false, now_ms);
  }
  // Un objet : ses gestes, et son survol qui demande l'aperçu.
  ro::ui::ChatLinkAsk item = link;
  if (item.hovered_kind != ro::ui::ChatLinkKind::kItem) {
    item.hovered_id = 0;
  }
  if (item.kind != ro::ui::ChatLinkKind::kItem) {
    item.item_id = 0;
  }
  FollowCatalogItem(item, now_ms);

  // LES BOUTONS DE L'OBJET MONTRÉ.
  if (asked.open_vote_shop) {
    static_cast<void>(pieces_.cash_shop.OpenWithItem(item_id, windows_.cash_shop.state, link_));
  }
  // Le « @item » du staff : le droit se relit ici, au geste.
  if (asked.staff_item.has_value()) {
    SendStaffItems(std::span(&*asked.staff_item, 1), Rights(), link_, world_);
  }
  if (asked.open_craft_atlas) {
    ro::ui::OpenCraftAtlasOn(windows_.craft_atlas.state, item_id);
  }
  if (asked.read_book || asked.recite_book) {
    const bool first_page = settings_->options().descriptions().book_first_page();
    if (asked.read_book) {
      item_description_->books().Open(item_id, windows_.book, first_page);
    } else {
      item_description_->books().StartRecital(item_id, windows_.book, first_page, now_ms);
    }
  }
  if (asked.toggle_autoloot) {
    item_description_->ToggleAutoloot(link_, item_id);
  }
  if (asked.search_market && item_id != 0) {
    pieces_.trade.market().AskOpen(item_id);
  }
  if (asked.open_database && item_id != 0) {
    database_.AskOpen(ro::ui::DatabaseTarget{.kind = ro::ui::DatabaseKind::kItem, .id = item_id});
  }
}

}  // namespace moonclient
