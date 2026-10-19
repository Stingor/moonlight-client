#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

#include "ro/audio/audio_engine.h"
#include "ro/net/connection.h"
#include "ro/net/link_loss.h"
#include "ro/net/link_watch.h"
#include "ro/net/session.h"
#include "ro/net/zone_move.h"
#include "ro/render/gl_context.h"
#include "ro/sim/friend_list.h"
#include "ro/sim/guild.h"
#include "ro/sim/party_roster.h"
#include "ro/sim/sight_purge.h"
#include "ro/sim/title_book.h"
#include "ro/sim/world.h"
#include "ro/ui/entity_menu.h"
#include "ro/vfs/vfs.h"

#include "character/staff_view.h"
#include "game/client_shell.h"
#include "game/game_pieces.h"
#include "game/game_settings.h"
#include "game/ui_stage.h"
#include "world/entity_menu.h"
#include "world/native_marker.h"
#include "world/navi_session.h"
#include "world/quick_cast.h"
#include "world/selection_marker.h"
#include "world/status_portrait_view.h"
#include "world/target_hud_view.h"
#include "world/targeting.h"

namespace moonclient {

// LE SOCLE DE LA PARTIE : ce qu'elle tient de la coquille, et ce qui ne change
// pas d'elle — la liaison, le monde, les livres hors du monde, le mode Ciblage,
// le menu du clic droit.
//
// 🔴 C'EST LA PREMIÈRE BASE DE `Game`, ET C'EST TOUT CE QU'ELLE PROMET : elle se
// construit avant tout le reste de la partie et se détruit après. Ce qui suit
// peut donc en garder l'adresse sans jamais la voir pendre.
//
// ⚠ AUCUNE MÉTHODE : la partie lit ces champs comme les siens, sous le même nom.
class GameFoundation {
 protected:
  GameFoundation(ClientShell& shell, ro::net::Connection link, ro::sim::World world,
                 const ro::net::ZoneTicket& ticket);

  // ── CE QUE LA COQUILLE POSSÈDE, ET QUE LA PARTIE NE FAIT QUE TENIR ─────────
  //
  // 🔴 DES POIGNÉES, PAS DES MEMBRES. La fenêtre, les archives, les réglages,
  // l'interface et les accessoires appartiennent désormais à `ClientShell` :
  // ils existent avant la partie et lui survivent, ce qui est la condition pour
  // qu'un écran de connexion puisse se dessiner sans carte.
  //
  // ⚠ ELLES GARDENT LEUR NOM ET LEUR FORME D'ACCÈS — `gl_->`, `*vfs_`,
  // `windows_.`, `settings_->` — et ce n'est pas une coquetterie : c'est ce qui
  // rend le changement de PROPRIÉTAIRE lisible dans le diff, au lieu de le
  // noyer sous cinq cents réécritures d'appel.
  //
  // ⚠ ELLES SONT VALIDES PARCE QUE LA COQUILLE EST OUVERTE AVANT : une partie
  // se construit après `ClientShell::OpenArchives`, jamais avant.
  ClientShell& shell_;
  // Une COPIE, parce que c'est un chemin : le partager n'apporterait rien.
  std::filesystem::path client_root_;
  ro::vfs::Vfs* vfs_;
  ro::render::GlContext* gl_;
  GameSettings* settings_;
  UiStage::Windows& windows_;
  UiStage* interface_;
  GamePieces& pieces_;
  ro::audio::AudioEngine& audio_;

  // ── CE QUI NE CHANGE PAS DE LA PARTIE ──────────────────────────────────────
  ro::net::Connection link_;
  ro::sim::World world_;
  ro::sim::SightPurge sight_purge_;  // les restes hors de la vue du serveur
  // ⚠ HORS DU MONDE, et aussi durables que la partie : les titres du personnage
  // (`ro/sim/title_book.h`), la guilde (`ro/sim/guild.h`), le groupe et les amis
  // (`ro/sim/party_roster.h`), la navigation et la minimap (`world/navi_session.h`).
  ro::sim::TitleBook titles_;
  ro::sim::GuildBook guild_book_;
  ro::sim::PartyRoster party_;
  ro::sim::FriendList friends_;
  NaviSession navi_;
  // DE QUOI RENTRER SUR UN AUTRE SERVEUR DE ZONE, et l'entrée en cours quand
  // le serveur en a désigné un. Voir `ro::net::ServerMoveOf`.
  ro::net::ZoneTicket ticket_;
  // ── LE MODE CIBLAGE ────────────────────────────────────────────────────────
  // Voir `world/targeting.h`. Ses réglages vivent dans les options ; la vue du
  // HUD et la flèche se recomposent à chaque image.
  Targeting targeting_;
  QuickCast quick_cast_;  // ses réglages vivent dans les options
  std::optional<TargetHudView> target_hud_;
  // LE PORTRAIT DE STATUT DU JOUEUR : sa vue se recompose à chaque image.
  std::optional<PortraitHudView> status_portrait_;
  SelectionMarker selection_marker_;
  MarkerPose marker_pose_;
  // L'entrée sur la carte : l'animation de la flèche compte depuis elle. Vide
  // jusqu'au premier dessin qui la suit.
  std::optional<std::uint32_t> marker_epoch_ms_;
  // L'instant de la décision précédente : voir `MarkerSampleMs`.
  std::optional<std::uint32_t> marker_previous_frame_ms_;
  // La mort cache la flèche une fois, au moment où elle survient.
  bool player_was_dead_ = false;
  // La souris était sur un cadre qui agit, à l'image précédente : le curseur
  // prend l'épée.
  bool target_hud_hovered_ = false;
  // ── LE MENU DU CLIC DROIT ──────────────────────────────────────────────────
  // Ce que les gestes du monde lui laissent, la cible relevée à l'ouverture, et
  // ce qu'il montre. ⚠ LA VUE VIT ICI parce que l'étage d'interface la désigne.
  EntityMenuGestures menu_gestures_;
  EntityMenuFacts menu_target_;
  ro::ui::EntityMenuView menu_view_;
  // LE MODÈLE DE LA CARTE que vise le menu, quand c'est lui qui est ouvert :
  // ses lignes vont alors à l'éditeur, voir `Game::PlayMapModelVerb`.
  std::optional<std::size_t> menu_model_;
  // LE POINT DU SOL que vise le menu quand il propose de coller le modèle copié,
  // dans le repère du RSW : voir `Game::OpenMapGroundMenu`.
  std::optional<std::array<float, 3>> menu_ground_;
  // L'instant de la demande d'ouverture. ⚠ ELLE PÉRIT : l'interface peut ne pas
  // être dessinée — la veille la cache —, et un menu qui surgirait au réveil
  // viserait une entité cliquée il y a longtemps.
  std::uint32_t menu_asked_ms_ = 0;
  std::optional<ro::net::MapPhase> moving_;
  // Quand le passage en cours a commencé : voir `ro::net::ZoneMoveOverdue`.
  std::uint32_t moving_since_ms_ = 0;
  // LA GARDE DU SILENCE et LA PERTE DÉCLARÉE : voir `game_link_loss.cpp`.
  ro::net::LinkWatch link_watch_;
  ro::net::LinkLoss link_loss_;
  // CE QUE LE VOLET DU STAFF TIENT LE TEMPS DE LA PARTIE : la liste des classes,
  // composée une fois, et ce qu'on sait avoir basculé chez le serveur. ⚠ À LA
  // PARTIE ET NON À LA FENÊTRE : le serveur oublie ses bascules à la déconnexion.
  StaffSession staff_;
};

}  // namespace moonclient
