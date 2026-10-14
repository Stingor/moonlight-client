// LES GESTES QUI CHERCHENT, vus de la boucle : le nom d'un monstre d'où qu'il
// vienne, puis une recherche de navigation `<NAVS>` et son ouverture.
//
// 🔴 UN SEUL ROUTAGE POUR TOUT NOM DE MONSTRE : le fil, la saisie, la table de
// tirage d'une description, le carnet de chasse et la fenêtre de navigation
// offrent le même menu. Une copie par appelant en laisserait un jour une sans
// la recherche, ou jouerait le geste sur un autre lien que celui du menu.

#include <cstdint>
#include <string>

#include <imgui.h>

#include "ro/ui/navigation_window.h"

#include "chat/chat_place_link.h"
#include "chat/chat_search_gestures.h"
#include "game/game.h"

namespace moonclient {

void Game::FollowMonsterGesture(ro::ui::ChatLinkGesture gesture, const MonsterLink& monster,
                                bool by_view, std::uint32_t now_ms) {
  // 🔴 UNE CLASSE DE SPRITE N'OUVRE QUE LA FICHE, lue comme telle : voir
  // `MonsterDatabaseId`. Zéro ici retire le bestiaire et les commandes.
  const std::uint32_t database_id = MonsterDatabaseId(monster, by_view);
  switch (gesture) {
    case ro::ui::ChatLinkGesture::kDescribe:
      // La fiche ; sans elle, la page du bestiaire.
      if (monster_sheet_.has_value()) {
        monster_sheet_->Open(windows_.monster_sheet.state, monster.monster_id, by_view);
      } else if (database_id != 0) {
        FollowDescriptionLink(
            ro::ui::DescriptionWindowAction{.followed = MonsterBestiaryUrl(database_id)}, now_ms);
      }
      break;
    case ro::ui::ChatLinkGesture::kOpenBestiary:
      if (database_id != 0) {
        FollowDescriptionLink(
            ro::ui::DescriptionWindowAction{.followed = MonsterBestiaryUrl(database_id)}, now_ms);
      }
      break;
    case ro::ui::ChatLinkGesture::kRepost:
      if (database_id != 0) {
        PostChatLink(monster);
      }
      break;
    case ro::ui::ChatLinkGesture::kWhereToFind:
      // LA NAVIGATION SAIT OÙ : la recherche part sur le nom, monstres seulement.
      OpenNaviSearch(WhereToFindSearch(monster));
      break;
    case ro::ui::ChatLinkGesture::kPostWhereToFind:
      // La MÊME question, posée à quelqu'un d'autre.
      PostChatLink(WhereToFindSearch(monster));
      break;
    case ro::ui::ChatLinkGesture::kMobInfo:
    case ro::ui::ChatLinkGesture::kWhereIs:
      // ⚠ ENVOYÉES COMME UNE LIGNE TAPÉE : leur réponse revient dans le fil, et
      // le serveur applique les mêmes droits qu'à une commande écrite à la main.
      if (const std::string line = MonsterCommandLine(gesture, database_id);
          !line.empty() && chat_dispatch_.has_value()) {
        SubmitChat(*chat_dispatch_, line, now_ms);
      }
      break;
    case ro::ui::ChatLinkGesture::kNothing:
    case ro::ui::ChatLinkGesture::kMenu:
    case ro::ui::ChatLinkGesture::kOpenDatabase:
    case ro::ui::ChatLinkGesture::kItemInfo:
    case ro::ui::ChatLinkGesture::kWhoDrops:
    case ro::ui::ChatLinkGesture::kMarketSearch:
    case ro::ui::ChatLinkGesture::kTryStyle:
    case ro::ui::ChatLinkGesture::kCopyCode:
    case ro::ui::ChatLinkGesture::kOpenInNavigation:
    case ro::ui::ChatLinkGesture::kPlaceContents:
    case ro::ui::ChatLinkGesture::kStaffWarp:
    case ro::ui::ChatLinkGesture::kVoteShop:
    case ro::ui::ChatLinkGesture::kWhisperWindow:
    case ro::ui::ChatLinkGesture::kInviteParty:
    case ro::ui::ChatLinkGesture::kInviteGuild:
    case ro::ui::ChatLinkGesture::kAddFriend:
    case ro::ui::ChatLinkGesture::kMonsterSheet:
    case ro::ui::ChatLinkGesture::kShareRespawn:
    case ro::ui::ChatLinkGesture::kGuideToTomb:
    case ro::ui::ChatLinkGesture::kAddToMvpLog:
    case ro::ui::ChatLinkGesture::kInviteMvp:
    case ro::ui::ChatLinkGesture::kShowImageOnce:
    case ro::ui::ChatLinkGesture::kAllowImageHost:
    case ro::ui::ChatLinkGesture::kViewImage:
    case ro::ui::ChatLinkGesture::kGoThere:
      break;
  }
}

void Game::FollowNaviSearchLink(const ro::ui::ChatLinkAsk& asked, const NaviSearchLink& link,
                                std::uint32_t now_ms) {
  switch (asked.gesture) {
    case ro::ui::ChatLinkGesture::kDescribe:
      OpenNaviSearch(link);
      break;
    case ro::ui::ChatLinkGesture::kGoThere:
      // Le trajet se lit sur la minimap et au sol, la destination reste écrite,
      // comme pour un lieu partagé.
      if (const std::string map = NaviSearchDestination(link); !map.empty()) {
        navi_.GuideToPlace(map, 0, 0);
        world_.RecordChat(
            ro::sim::ChatKind::kServer, 0,
            PlaceDestinationLine(PlaceLink{.map = map}, pieces_.map_display_names.has_value()
                                                            ? &*pieces_.map_display_names
                                                            : nullptr),
            now_ms);
      }
      break;
    case ro::ui::ChatLinkGesture::kPlaceContents:
      windows_.navigation.state.open = true;
      windows_.navigation.state.raise = true;
      if (const ro::formats::NaviData* data = navi_.data(); data != nullptr && !link.map.empty())
        navi_.navigation_view.ShowContents(*data, data->FindMap(link.map));
      break;
    case ro::ui::ChatLinkGesture::kCopyCode:
      // Le terme tel qu'il a voyagé : le nom interne pour une carte.
      ImGui::SetClipboardText(link.term.c_str());
      break;
    case ro::ui::ChatLinkGesture::kRepost:
      PostChatLink(link);
      break;
    case ro::ui::ChatLinkGesture::kNothing:
    case ro::ui::ChatLinkGesture::kMenu:
    case ro::ui::ChatLinkGesture::kOpenDatabase:
    case ro::ui::ChatLinkGesture::kItemInfo:
    case ro::ui::ChatLinkGesture::kWhoDrops:
    case ro::ui::ChatLinkGesture::kMarketSearch:
    case ro::ui::ChatLinkGesture::kTryStyle:
    case ro::ui::ChatLinkGesture::kOpenBestiary:
    case ro::ui::ChatLinkGesture::kOpenInNavigation:
    case ro::ui::ChatLinkGesture::kStaffWarp:
    case ro::ui::ChatLinkGesture::kVoteShop:
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
      break;
  }
}

void Game::OpenNaviSearch(const NaviSearchLink& link) {
  // 🔴 LE TERME TRANSPORTÉ, jamais sa forme lisible : le nom interne d'une carte
  // est le seul que la recherche retrouve. Et le CONTEXTE part avec lui : sans
  // lui, le lien d'un PNJ ouvrirait tous ses homonymes du serveur.
  if (link.term.empty()) {
    return;
  }
  ro::ui::OpenNavigationSearch(windows_.navigation.state, link.term,
                               link.family == NaviSearchFamily::kMonster, link.map);
  navi_.Search(link.term, link.map, world_.map_name());
}

}  // namespace moonclient
