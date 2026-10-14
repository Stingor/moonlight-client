// LES DESCRIPTIONS, VUES DE LA BOUCLE : celles d'un objet, d'une compétence et
// de l'objet survolé, composées avant le dessin ; puis ce que les autres
// fenêtres en demandent — quel objet ou quelle compétence décrire, ce que la
// souris survole — et ce que les fenêtres de description demandent elles-mêmes :
// suivre un lien, interroger le serveur sur une compétence.
//
// 🔴 PLUSIEURS FENÊTRES DEMANDENT, UNE SEULE L'A FAIT : c'est ici qu'on tranche,
// dans un ordre fixe, parce que le joueur n'a qu'une souris.
//
// ⚠ ET C'EST ICI QUE SE DÉCIDE CE QU'ON N'ENVOIE PAS. `ui` ne possède rien : la
// fenêtre redemande à chaque image, sans savoir ce que le monde porte déjà.
// Aucune cadence ne peut vivre là-bas.

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <span>
#include <string>
#include <utility>

#include <SDL3/SDL.h>

#include "ro/core/error.h"
#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/sim/bank.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/inventory.h"
#include "ro/sim/world.h"
#include "ro/ui/rich_text.h"

#include "game/bug_report_route.h"
#include "game/game.h"

namespace moonclient {
namespace {

// Le libellé du bouton du vote shop : son nom et un prix.
constexpr std::size_t kVoteShopLabelBytes = 64;

}  // namespace

// LE LIEN SUIVI DEPUIS LA DESCRIPTION D'UNE COMPÉTENCE.
//
// ⚠ LE MÊME TRAITEMENT QUE CELUI D'UN OBJET : ce qu'on fait d'une adresse ou
// d'un lieu ne dépend pas de la fenêtre d'où il vient. Seule l'action diffère,
// parce qu'elle porte deux demandes de plus.
void Game::FollowSkillDescriptionLink(const ro::ui::SkillDescriptionAction& asked,
                                      std::uint32_t now_ms) {
  ro::ui::DescriptionWindowAction relayed;
  relayed.followed = asked.followed;
  relayed.followed_kind = asked.followed_kind;
  FollowDescriptionLink(relayed, now_ms);
}

void Game::FollowDescriptionLink(const ro::ui::DescriptionWindowAction& asked,
                                 std::uint32_t now_ms) {
  if (asked.followed.empty()) {
    return;
  }
  if (asked.followed_kind == ro::ui::RichLinkKind::kItem) {
    // UN OBJET SE DÉCRIT, comme au clic de tout lien d'objet.
    if (const std::optional<std::uint32_t> item_id = ro::ui::ReadRichItem(asked.followed)) {
      FollowCatalogItem(
          ro::ui::ChatLinkAsk{.gesture = ro::ui::ChatLinkGesture::kDescribe, .item_id = *item_id},
          now_ms);
    }
    return;
  }
  if (asked.followed_kind == ro::ui::RichLinkKind::kPlace) {
    // LE LIEU LANCE LE GUIDAGE, et la conversation dit vers où : le trajet se
    // lit sur la minimap et au sol, la destination reste écrite.
    // ⚠ L'ESPACE EST POSÉE ICI, PAS DANS LE TEXTE : un catalogue rogne les
    // espaces de bord, et « Destination : » les perdrait à la relecture.
    if (const std::optional<ro::ui::RichPlace> place = ro::ui::ReadRichPlace(asked.followed))
      navi_.GuideToPlace(place->map, place->x, place->y);
    std::string where(ro::core::Tr("Destination :"));
    where += ' ';
    where += asked.followed;
    world_.RecordChat(ro::sim::ChatKind::kServer, 0, where, now_ms);
    return;
  }
  // 🔴 UNE ADRESSE OUVRE UN PROGRAMME DU SYSTÈME, et elle vient d'une DONNÉE du
  // jeu. On n'ouvre donc que ce qu'on reconnaît : `http://` ou `https://`, en
  // toutes lettres. Un simple « commence par http » laisserait passer un
  // « httpfoo:// » — et tout autre schéma ouvrirait ce que le système lui
  // associe, ce qui n'est pas ce que le joueur a cliqué.
  const bool safe = asked.followed.starts_with("http://") || asked.followed.starts_with("https://");
  if (!safe) {
    ro::core::LogWarning("description : adresse refusée — {}", asked.followed);
    return;
  }
  if (!SDL_OpenURL(asked.followed.c_str())) {
    ro::core::LogWarning("description : {} n'a pas pu s'ouvrir", asked.followed);
  }
}

void Game::StageDescriptions(std::uint32_t now_ms) {
  // OÙ LES DEUX FENÊTRES S'OUVRENT : le réglage, relu à chaque image — il se
  // change fenêtre ouverte, dans le panneau.
  const ro::ui::DescriptionPlacement placement = settings_->options().descriptions().placement();
  windows_.description.placement = placement;
  windows_.skill_description.placement = placement;
  windows_.description.compare = settings_->options().descriptions().compare();
  const ro::ui::RichTextSelect text_select = settings_->options().descriptions().text_select();
  windows_.description.text_select = text_select;
  windows_.skill_description.text_select = text_select;
  // LE LIVRE : sa fenêtre s'ouvre où les descriptions s'ouvrent, et la
  // récitation avance à chaque image. Une ligne dite rouvre la conversation
  // si le joueur l'avait fermée : c'est là qu'elle s'écrit.
  BookReader& books = item_description_->books();
  if (books.Tick(world_, windows_.book, now_ms)) {
    windows_.chat.main.open = true;
  }
  windows_.book.placement = placement;
  windows_.book_view = &books.view();
  windows_.book_recital = books.recital_line();
  // LA DESCRIPTION SE REFAIT À CHAQUE IMAGE : le nom décoré suit l'objet, qui
  // peut se faire sertir ou raffiner pendant qu'on le lit.
  //
  // ⚠ FENÊTRE OUVERTE SEULEMENT : l'objet décrit reste désigné après la
  // fermeture, et sans cette garde son nom se recomposerait à chaque image pour
  // une fenêtre que personne ne voit. Rouverte, elle se refait avant son dessin.
  if (windows_.description.open) {
    item_description_->Refresh(world_, windows_.description.item_id,
                               windows_.description.from_holder, windows_.description.from_slot);
    item_description_->NoteBack(
        windows_.description.back.empty() ? 0 : windows_.description.back.back());
    item_description_->NoteVoteShop(pieces_.cash_shop.offers());
    item_description_->NoteHatEffect(
        pieces_.cash_shop.hat_effects().OrdinalOf(windows_.description.item_id) != 0);
  }
  // CE QUE LE SERVEUR SEUL SAIT : les sources et le script, demandés dès que la
  // fenêtre est ouverte. ⚠ À CHAQUE IMAGE, fermée comprise : c'est ainsi que la
  // vue voit la RÉOUVERTURE, qui fait redemander les sources.
  item_description_->AskFacts(link_, windows_.description.open, windows_.description.item_id,
                              now_ms);
  if (windows_.description.open) {
    // LE PANTIN DU « VIEWID », quand sa ligne est survolée.
    item_description_->RefreshDoll(
        world_, windows_.description.doll,
        pieces_.cash_shop.hat_effects().OrdinalOf(windows_.description.item_id) != 0, styles_,
        now_ms);
    // L'ÉTIQUETTE « CRAFT », que seul l'Atlas sait composer.
    std::string craft_label;
    std::string craft_tip;
    if (craft_atlas_.has_value() &&
        craft_atlas_->CraftTagOf(windows_.description.item_id, &craft_label, &craft_tip)) {
      item_description_->NoteCraftTag(std::move(craft_label), std::move(craft_tip));
    }
    // LE BOUTON DU VOTE SHOP, que seule la boutique sait composer — et
    // seulement quand l'item est à son catalogue.
    if (const std::optional<CashShopStage::Offer> offer =
            pieces_.cash_shop.Find(windows_.description.item_id);
        offer.has_value()) {
      std::array<char, kVoteShopLabelBytes> label{};
      std::snprintf(label.data(), label.size(), ro::core::Tr("Vote Shop : %d pts"), offer->price);
      item_description_->NoteVoteShopTag(label.data());
    }
    // LA LIGNE « @item », pour le staff seulement.
    item_description_->NoteStaff(Rights().staff_tools,
                                 pieces_.item_properties.IsStackable(windows_.description.item_id),
                                 StaffFreeBagSlots(world_.inventory()));
  }
  // ⚠ L'APERÇU NE SE BÂTIT QUE S'IL SE MONTRE : sans le réglage, il n'a rien à
  // composer, et une description entière par image de survol serait payée pour
  // rien.
  description_preview_->Refresh(world_, hovered_item_, hovered_holder_, hovered_slot_);
  skill_description_->Refresh(windows_.skill_description.skill_id, world_);
}

void Game::ApplyDescriptionAnswers(const UiStage::Answers& answers, std::uint32_t now_ms) {
  // ── LA DESCRIPTION D'UN OBJET ───────────────────────────────────────────────
  //
  // ⚠ PLUSIEURS FENÊTRES PEUVENT LA DEMANDER, une seule l'a demandée : le joueur
  // n'a qu'une souris. On prend la première qui parle.
  //
  // ⚠ ELLE SE REPLACE SOUS LE CURSEUR À CHAQUE DEMANDE, même déjà ouverte : on
  // décrit un objet après l'autre, et une fenêtre restée au loin obligerait à
  // aller la chercher des yeux.
  std::uint32_t described = answers.inventory.describe_id;
  std::uint16_t described_slot = answers.inventory.describe_slot;
  ro::ui::ItemHolder described_holder = ro::ui::ItemHolder::kInventory;
  if (described == 0) {
    described = answers.cart.describe_id;
    described_slot = answers.cart.describe_slot;
    described_holder = ro::ui::ItemHolder::kCart;
  }
  if (described == 0) {
    described = answers.storage.describe_id;
    described_slot = answers.storage.describe_slot;
    described_holder = ro::ui::ItemHolder::kStorage;
  }
  if (described == 0) {
    // ⚠ LA FICHE REND UNE PLACE DE SAC, pas une place de mannequin : c'est donc
    // bien le sac qu'il faut interroger pour décorer le nom.
    described = answers.character.describe_id;
    described_slot = answers.character.describe_slot;
    described_holder = ro::ui::ItemHolder::kInventory;
  }
  if (described == 0 && answers.shortcuts.describe >= 0) {
    // ⚠ UNE CASE NE CONNAÎT PAS DE PLACE, seulement un IDENTIFIANT — c'est ce
    // qui lui permet de survivre à la pile qu'on vide. Le nom décoré, lui, a
    // besoin d'une place : on la cherche donc dans le sac, au moment de décrire.
    // Sans exemplaire sous la main, le nom de base fera l'affaire.
    described = shortcuts_->DescribedItem(static_cast<std::size_t>(answers.shortcuts.describe),
                                          world_, &described_slot);
    described_holder = ro::ui::ItemHolder::kInventory;
  }
  // ⚠ REDEMANDER L'OBJET QU'ELLE MONTRE LA FERME : le même clic droit ouvre et
  // referme, sans aller chercher la croix. Le même OBJET, c'est la même place
  // dans le même contenant — deux exemplaires d'un identifiant diffèrent par
  // leur raffinement et leurs cartes.
  const bool same_item = windows_.description.open && described != 0 &&
                         windows_.description.item_id == described &&
                         windows_.description.from_slot == described_slot &&
                         windows_.description.from_holder == described_holder;
  if (same_item) {
    windows_.description.open = false;
  } else if (described != 0) {
    windows_.description.item_id = described;
    windows_.description.from_slot = described_slot;
    windows_.description.from_holder = described_holder;
    windows_.description.open = true;
    windows_.description.place_at_mouse = true;
    windows_.description.raise = true;
  }
  // ── LA DESCRIPTION D'UNE COMPÉTENCE ───────────────────────────────────────
  //
  // Le grimoire la demande au Ctrl + clic droit, la barre au clic droit ; la
  // redemander pour la compétence qu'elle montre la referme.
  std::uint16_t described_skill = answers.character.grimoire.describe;
  if (described_skill == 0 && answers.shortcuts.describe >= 0) {
    described_skill =
        shortcuts_->DescribedSkill(static_cast<std::size_t>(answers.shortcuts.describe));
  }
  ro::ui::AskSkillDescription(windows_.skill_description, described_skill);
  FollowDescriptionLink(answers.description, now_ms);
  item_description_->RefreshSources(windows_.description.item_id, answers.description);
  // UN GESTE SUR UNE LIGNE DE LA TABLE DE TIRAGE. ⚠ DÉCRIRE UN OBJET N'ARRIVE PAS
  // ICI : la fenêtre navigue elle-même. Le reste est ce qu'on fait de tout lien —
  // la fiche d'un monstre, le lien posé dans la conversation, la base du site.
  const ro::ui::ChatLinkAsk& odds = answers.description.link;
  if (odds.hovered_id != 0 && odds.hovered_kind == ro::ui::ChatLinkKind::kMonster &&
      monster_sheet_.has_value()) {
    monster_sheet_->NoteHovered(odds.hovered_id, /*by_view=*/false);
  }
  if (odds.item_id != 0 && odds.kind == ro::ui::ChatLinkKind::kMonster) {
    // Le rang n'est pas dans la table : le lien part ordinaire, et son aperçu
    // dira le vrai.
    std::string_view named = item_description_->OddsNameOf(odds.item_id);
    if (named.empty()) {
      named = item_description_->SourceNameOf(odds.item_id);
    }
    FollowMonsterGesture(odds.gesture,
                         MonsterLink{.monster_id = odds.item_id, .name = std::string(named)},
                         /*by_view=*/false, now_ms);
  } else if (odds.item_id != 0) {
    switch (odds.gesture) {
      case ro::ui::ChatLinkGesture::kRepost:
        PostChatLink(ro::vfs::ItemLink{.item_id = odds.item_id});
        break;
      case ro::ui::ChatLinkGesture::kOpenDatabase:
        FollowDescriptionLink(
            ro::ui::DescriptionWindowAction{.followed = ro::vfs::ItemDatabaseUrl(odds.item_id)},
            now_ms);
        break;
      case ro::ui::ChatLinkGesture::kMarketSearch:
        pieces_.trade.market().AskOpen(odds.item_id);
        break;
      case ro::ui::ChatLinkGesture::kItemInfo:
      case ro::ui::ChatLinkGesture::kWhoDrops: {
        const char* const verb =
            odds.gesture == ro::ui::ChatLinkGesture::kItemInfo ? "@iteminfo " : "@whodrops ";
        if (chat_dispatch_.has_value()) {
          SubmitChat(*chat_dispatch_, verb + std::to_string(odds.item_id), now_ms);
        }
        break;
      }
      case ro::ui::ChatLinkGesture::kVoteShop:
        // Le vote shop : la boutique s'ouvre, l'item au panier.
        static_cast<void>(
            pieces_.cash_shop.OpenWithItem(odds.item_id, windows_.cash_shop.state, link_));
        break;
      case ro::ui::ChatLinkGesture::kNothing:
      case ro::ui::ChatLinkGesture::kDescribe:
      case ro::ui::ChatLinkGesture::kMenu:
      case ro::ui::ChatLinkGesture::kTryStyle:
      case ro::ui::ChatLinkGesture::kCopyCode:
      case ro::ui::ChatLinkGesture::kOpenBestiary:
      case ro::ui::ChatLinkGesture::kOpenInNavigation:
      case ro::ui::ChatLinkGesture::kPlaceContents:
      case ro::ui::ChatLinkGesture::kStaffWarp:
      case ro::ui::ChatLinkGesture::kWhisperWindow:
      case ro::ui::ChatLinkGesture::kInviteParty:
      case ro::ui::ChatLinkGesture::kInviteGuild:
      case ro::ui::ChatLinkGesture::kAddFriend:
      case ro::ui::ChatLinkGesture::kMonsterSheet:
      case ro::ui::ChatLinkGesture::kShareRespawn:
      case ro::ui::ChatLinkGesture::kGuideToTomb:
      case ro::ui::ChatLinkGesture::kWhereToFind:
      case ro::ui::ChatLinkGesture::kMobInfo:
      case ro::ui::ChatLinkGesture::kWhereIs:
      case ro::ui::ChatLinkGesture::kAddToMvpLog:
      case ro::ui::ChatLinkGesture::kInviteMvp:
      case ro::ui::ChatLinkGesture::kShowImageOnce:
      case ro::ui::ChatLinkGesture::kAllowImageHost:
      case ro::ui::ChatLinkGesture::kViewImage:
      case ro::ui::ChatLinkGesture::kPostWhereToFind:
      case ro::ui::ChatLinkGesture::kGoThere:
        break;
    }
  }
  // Le bouton du vote shop : la boutique s'ouvre, l'item décrit au panier.
  if (answers.description.open_vote_shop) {
    static_cast<void>(pieces_.cash_shop.OpenWithItem(windows_.description.item_id,
                                                     windows_.cash_shop.state, link_));
  }
  // Le « @item » du staff : le droit se relit ici, au geste.
  if (answers.description.staff_item.has_value()) {
    SendStaffItems(std::span(&*answers.description.staff_item, 1), Rights(), link_, world_);
  }
  // L'étiquette « Craft » : l'Atlas s'ouvre sur l'item décrit.
  if (answers.description.open_craft_atlas) {
    ro::ui::OpenCraftAtlasOn(windows_.craft_atlas.state, windows_.description.item_id);
  }
  // La case « Comparer » de la fenêtre : c'est le réglage lui-même.
  if (answers.description.compare_changed) {
    settings_->options().descriptions().SetCompare(answers.description.compare, &settings_->file());
  }
  RouteBugReports(answers, windows_, world_, *item_description_, skill_description_->view(),
                  settings_->options().bug_report().buttons(), link_);
  // « SIGNALER CE JOUEUR » : l'envoi que sa fenêtre demande.
  pieces_.player_report.Advance(windows_.player_report, answers.player_report, world_.player_id(),
                                link_);
  if (answers.description.read_book || answers.description.recite_book) {
    const bool first_page = settings_->options().descriptions().book_first_page();
    if (answers.description.read_book) {
      item_description_->books().Open(windows_.description.item_id, windows_.book, first_page);
    } else {
      item_description_->books().StartRecital(windows_.description.item_id, windows_.book,
                                              first_page, now_ms);
    }
  }
  if (answers.book.closed) {
    item_description_->books().NoteClosed(windows_.book.item_id, answers.book.page);
  }
  if (answers.description.toggle_autoloot) {
    item_description_->ToggleAutoloot(link_, windows_.description.item_id);
  }
  if (answers.description.search_market && windows_.description.item_id != 0) {
    pieces_.trade.market().AskOpen(windows_.description.item_id);
  }
  FollowSkillDescriptionLink(answers.skill_description, now_ms);
  skill_facts_.Ask(answers.skill_description, link_);
  // CE QUE LA SOURIS SURVOLE, retenu pour l'image suivante. ⚠ PLUSIEURS
  // FENÊTRES RÉPONDENT, une seule est survolée : le joueur n'a qu'une souris.
  // Le sertissage et la conversation complètent ce relevé après lui.
  // ⚠ CHAQUE FENÊTRE A SON RÉGLAGE, et c'est ici qu'il décide : une fenêtre qui
  // n'en veut pas ne fournit aucun objet, donc l'aperçu ne se compose ni ne se
  // dessine. Le filtrer plus loin ferait bâtir une description pour rien.
  //
  // 🔴 ET LA PLACE VIENT AVEC SON CONTENANT. C'est elle qui donne au nom son
  // raffinement, ses cartes et sa forge : sans elle l'aperçu n'écrivait que le
  // nom de base, et « +7 Knife [3] » s'y lisait « Knife ».
  hovered_item_ = 0;
  hovered_slot_ = 0;
  hovered_holder_ = ro::ui::ItemHolder::kInventory;
  if (windows_.inventory.hover_preview) {
    hovered_item_ = answers.inventory.hovered_id;
    hovered_slot_ = answers.inventory.hovered_slot;
  }
  if (hovered_item_ == 0 && windows_.cart.hover_preview) {
    hovered_item_ = answers.cart.hovered_id;
    hovered_slot_ = answers.cart.hovered_slot;
    hovered_holder_ = ro::ui::ItemHolder::kCart;
  }
  if (hovered_item_ == 0 && windows_.storage.hover_preview) {
    hovered_item_ = answers.storage.hovered_id;
    hovered_slot_ = answers.storage.hovered_slot;
    hovered_holder_ = ro::ui::ItemHolder::kStorage;
  }
  if (hovered_item_ == 0 && windows_.character.hover_preview) {
    hovered_item_ = answers.character.hovered_id;
    hovered_slot_ = answers.character.hovered_slot;
  }
  // LES COMMERCES, qui montrent leur aperçu comme le sac : un objet du sac
  // par sa case, un objet de la boutique par son identifiant.
  if (hovered_item_ == 0) {
    // Une pièce jointe de courrier survolée vaut une offre de commerce, et une
    // carte de la boutique de vote aussi.
    TradeItemFocus traded =
        pieces_.trade.hovered().item_id != 0 ? pieces_.trade.hovered() : pieces_.mail.hovered();
    if (const std::optional<ro::vfs::ItemLink>& voted = pieces_.cash_shop.hovered();
        traded.item_id == 0 && voted.has_value()) {
      traded = TradeItemFocus{.item_id = voted->item_id, .link = voted};
    }
    hovered_item_ = traded.item_id;
    hovered_slot_ = traded.bag_slot;
    hovered_holder_ = ro::ui::ItemHolder::kInventory;
    // L'OBJET D'UN AUTRE — une offre d'échoppe — s'aperçoit comme un lien :
    // ce qu'il porte voyage avec lui.
    if (traded.item_id != 0 && traded.link.has_value() && description_preview_.has_value()) {
      description_preview_->NoteLinked(*traded.link);
      hovered_slot_ = 0;
      hovered_holder_ = ro::ui::ItemHolder::kChatLink;
    }
  }
  // ⚠ L'ALBUM DE CARTES MONTRE SON APERÇU SANS RÉGLAGE, et la carte EN ENTIER :
  // on y regarde des cartes, c'est justement ce qu'on vient voir. Sans place —
  // une pochette n'est pas un objet qu'on tient.
  // Un objet de la table de tirage, sans place : c'est une entrée de catalogue.
  if (hovered_item_ == 0 && odds.hovered_id != 0 &&
      odds.hovered_kind == ro::ui::ChatLinkKind::kItem) {
    hovered_item_ = odds.hovered_id;
    hovered_slot_ = 0;
    hovered_holder_ = ro::ui::ItemHolder::kInventory;
  }
  if (hovered_item_ == 0 && answers.card_album.hovered_id != 0) {
    hovered_item_ = answers.card_album.hovered_id;
    hovered_slot_ = 0;
    hovered_holder_ = ro::ui::ItemHolder::kCardAlbum;
  }
}

void Game::FollowTradeItem(const TradeItemFocus& focus, std::uint32_t now_ms) {
  // LE LIEN DANS LA SAISIE : celui que porte l'objet d'un autre, sinon celui de
  // sa case du sac, sinon l'entrée du catalogue.
  if (focus.gesture == ro::ui::ChatLinkGesture::kRepost) {
    if (focus.link.has_value()) {
      PostChatLink(*focus.link);
    } else if (focus.bag_slot != 0) {
      PostItemLink(world_.inventory(), focus.bag_slot);
    } else {
      PostChatLink(ro::vfs::ItemLink{.item_id = focus.item_id});
    }
    return;
  }
  // LES AUTRES ENTRÉES DU MENU DES OBJETS ne lisent que l'identifiant.
  if (focus.gesture != ro::ui::ChatLinkGesture::kDescribe) {
    FollowCatalogItem(ro::ui::ChatLinkAsk{.gesture = focus.gesture, .item_id = focus.item_id},
                      now_ms);
    return;
  }
  windows_.description.item_id = focus.item_id;
  windows_.description.from_slot = focus.bag_slot;
  windows_.description.from_holder = ro::ui::ItemHolder::kInventory;
  // L'OBJET D'UN AUTRE se décrit comme un lien : raffinement, cartes et options
  // voyagent avec lui.
  if (focus.link.has_value() && item_description_.has_value()) {
    item_description_->NoteLinked(*focus.link);
    windows_.description.from_slot = 0;
    windows_.description.from_holder = ro::ui::ItemHolder::kChatLink;
  }
  windows_.description.open = true;
  windows_.description.place_at_mouse = true;
  windows_.description.raise = true;
}

}  // namespace moonclient
