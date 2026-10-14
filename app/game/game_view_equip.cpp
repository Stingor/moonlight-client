// L'ÉQUIPEMENT VU PAR LES AUTRES, vu de la boucle, des deux côtés : « Voir
// l'équipement » d'un autre joueur — ce que la fenêtre a rendu à l'image
// précédente, puis la vue qu'elle dessinera — et « Montrer mon équipement »,
// ce que le serveur dit du nôtre.
//
// ⚠ À CÔTÉ DE LA BOUCLE : la fenêtre forme un sujet entier — une fiche reçue,
// ses gestes, ses outils du staff.

#include <array>
#include <cstdio>
#include <optional>
#include <string>
#include <utility>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/chat_target.h"
#include "ro/net/inventory.h"
#include "ro/sim/chat_line.h"
#include "ro/sim/world.h"
#include "ro/vfs/item_table.h"
#include "ro/vfs/item_words.h"

#include "chat/chat_view.h"
#include "game/game.h"
#include "items/inventory_view.h"
#include "world/click_action.h"

namespace moonclient {
namespace {

// LA COULEUR DE L'ANNONCE de notre équipement montré ou caché : un jaune pâle,
// en `0xRRGGBB`, celle de tous les échos de réglage.
constexpr std::uint32_t kConfigMessageColor = 0xFFFF64;

// Assez pour « @cloneequip » et un nom de personnage.
constexpr std::size_t kCommandBytes = 64;

}  // namespace

void Game::AdvanceViewEquip(std::uint32_t now_ms) {
  if (!viewed_equipment_.has_value()) {
    windows_.view_equip.view = nullptr;
    return;
  }
  ro::ui::ViewEquipWindowState& state = windows_.view_equip.state;
  const ro::ui::ViewEquipAnswer answer = std::exchange(windows_.view_equip.answer, {});

  // LES DEUX CASES DU PIED, retenues d'une séance à l'autre. ⚠ LE FICHIER NE
  // S'ÉCRIT QU'AU CHANGEMENT : `Touch` arme son anti-rebond.
  ViewEquipMemory& memory = settings_->options().view_equip();
  if (memory.show_empty != state.show_empty || memory.compare != state.compare) {
    memory.show_empty = state.show_empty;
    memory.compare = state.compare;
    settings_->file().Touch();
  }

  // ── CE QUE LA FENÊTRE A DEMANDÉ ───────────────────────────────────────────
  //
  // 🔴 LES TROIS OUTILS DU STAFF SONT DES COMMANDES DU SERVEUR, envoyées comme
  // une ligne tapée : il reste seul juge du droit, et son verdict revient dans
  // la conversation. Les deux premières prennent un NOM — la seule identité que
  // la fiche porte —, la troisième la classe AFFICHÉE : sur un joueur monté ou
  // déguisé, c'est une classe factice que le serveur refuse, et son refus le dit
  // mieux qu'une table de classes de base à tenir ici.
  const std::optional<ro::sim::ViewedEquipment>& sheet = inspected_.shown();
  std::array<char, kCommandBytes> command{};
  switch (answer.verb) {
    case ro::ui::ViewEquipVerb::kNothing:
      break;
    case ro::ui::ViewEquipVerb::kRefresh:
      if (const std::uint32_t shown = viewed_equipment_->shown_id(); shown != 0) {
        viewed_equipment_->NotePending(shown);
        ReportRefusal(ro::net::RequestViewEquip(link_, shown));
      }
      break;
    case ro::ui::ViewEquipVerb::kCloneEquip:
      if (sheet.has_value()) {
        std::snprintf(command.data(), command.size(), "@cloneequip %s", sheet->name.c_str());
      }
      break;
    case ro::ui::ViewEquipVerb::kCloneStat:
      if (sheet.has_value()) {
        std::snprintf(command.data(), command.size(), "@clonestat %s", sheet->name.c_str());
      }
      break;
    case ro::ui::ViewEquipVerb::kCloneJob:
      if (sheet.has_value()) {
        std::snprintf(command.data(), command.size(), "@job %u",
                      static_cast<unsigned>(sheet->look.job));
      }
      break;
  }
  if (command[0] != '\0' && Rights().staff_tools) {
    if (auto sent = ro::net::SendServerCommand(link_, world_.player_name(), command.data());
        !sent) {
      ro::core::LogWarning("commande de staff « {} » non émise : {}", command.data(),
                           ro::core::Describe(sent.error()));
    }
  }

  // ── LES GESTES SUR UNE PIÈCE, les mêmes que partout où un objet se montre ──
  //
  // ⚠ LE SURVOL D'ABORD : l'aperçu se décore de la pièce elle-même — cartes,
  // raffinement —, que son seul identifiant ne dit pas. Comme sur un lien de la
  // conversation, il ne se filtre par aucun réglage : il se montre toujours.
  if (answer.link.hovered_id != 0) {
    const ViewedEquipmentView::Designated hovered =
        ViewedEquipmentView::PieceOf(inspected_, world_, answer.hovered_token);
    if (hovered.item != nullptr) {
      hovered_item_ = hovered.item->item_id;
      hovered_slot_ = hovered.item->client_slot;
      hovered_holder_ =
          hovered.theirs ? ro::ui::ItemHolder::kViewedEquipment : ro::ui::ItemHolder::kEquipment;
    }
  }
  const ViewedEquipmentView::Designated aimed =
      answer.link.gesture == ro::ui::ChatLinkGesture::kNothing
          ? ViewedEquipmentView::Designated{}
          : ViewedEquipmentView::PieceOf(inspected_, world_, answer.link.token);
  if (aimed.item != nullptr) {
    switch (answer.link.gesture) {
      case ro::ui::ChatLinkGesture::kDescribe:
        // 🔴 SA PIÈCE SE DÉCRIT DEPUIS LA FICHE, la mienne depuis mon sac : c'est
        // le contenant qui décore le nom, et chacune a le sien.
        windows_.description.item_id = aimed.item->item_id;
        windows_.description.from_slot = aimed.item->client_slot;
        windows_.description.from_holder =
            aimed.theirs ? ro::ui::ItemHolder::kViewedEquipment : ro::ui::ItemHolder::kEquipment;
        windows_.description.open = true;
        windows_.description.place_at_mouse = true;
        windows_.description.raise = true;
        break;
      case ro::ui::ChatLinkGesture::kRepost:
        // La pièce ENTIÈRE part dans le lien : raffinement, cartes, grade.
        PostChatLink(ItemLinkOf(*aimed.item));
        break;
      case ro::ui::ChatLinkGesture::kOpenDatabase:
        FollowDescriptionLink(ro::ui::DescriptionWindowAction{.followed = ro::vfs::ItemDatabaseUrl(
                                                                  aimed.item->item_id)},
                              now_ms);
        break;
      case ro::ui::ChatLinkGesture::kMarketSearch:
        pieces_.trade.market().AskOpen(aimed.item->item_id);
        break;
      case ro::ui::ChatLinkGesture::kItemInfo:
      case ro::ui::ChatLinkGesture::kWhoDrops: {
        const char* const verb =
            answer.link.gesture == ro::ui::ChatLinkGesture::kItemInfo ? "@iteminfo " : "@whodrops ";
        SubmitChat(*chat_dispatch_, verb + std::to_string(aimed.item->item_id), now_ms);
        break;
      }
      case ro::ui::ChatLinkGesture::kVoteShop:
        // Le vote shop : la boutique s'ouvre, l'item au panier.
        static_cast<void>(
            pieces_.cash_shop.OpenWithItem(aimed.item->item_id, windows_.cash_shop.state, link_));
        break;
      case ro::ui::ChatLinkGesture::kNothing:
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

  // ── LA FICHE REÇUE, et la vue qui la montre ───────────────────────────────
  viewed_equipment_->Refresh(inspected_, world_, styles_, Rights().staff_tools, state, now_ms);
  viewed_equipment_->NoteVoteShop(pieces_.cash_shop.offers());
  windows_.view_equip.view = sheet.has_value() ? &viewed_equipment_->view() : nullptr;
}

// ⚠ UNE LIGNE PAR ANNONCE, à l'entrée comme à chaque écho : le joueur qui
// clique voit ainsi ce que le serveur a retenu, même quand rien n'a changé.
void Game::SayEquipmentShown(std::uint32_t now_ms) {
  const std::optional<bool>& shown = own_config_.equipment_shown();
  if (!shown.has_value()) {
    return;
  }
  const std::string_view shown_text = ro::vfs::EquipShownText(pieces_.item_words, *shown);
  if (shown_text.empty()) {
    return;
  }
  world_.RecordChat(ro::sim::ChatKind::kServer, 0, std::string(shown_text), now_ms,
                    kConfigMessageColor);
}

// ⚠ UNE LIGNE PAR ANNONCE, comme l'équipement, de la même couleur — mais sous
// son propre genre : le serveur la redit à chaque connexion, et le joueur doit
// pouvoir la taire sans taire le reste.
void Game::SayCallable(std::uint32_t now_ms) {
  const std::optional<bool>& callable = own_config_.callable();
  if (!callable.has_value()) {
    return;
  }
  const char* said =
      *callable ? ro::core::Tr("Vous pouvez être joint par l'appel d'urgence. (/call)")
                : ro::core::Tr("Vous ne pouvez pas être joint par l'appel d'urgence. (/call)");
  world_.RecordChat(ro::sim::ChatKind::kCall, 0, std::string(said), now_ms, kConfigMessageColor);
}

}  // namespace moonclient
