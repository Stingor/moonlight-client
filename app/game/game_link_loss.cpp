// LA LIAISON AU SERVEUR PERDUE, et ce que le joueur en voit.
//
// 🔴 AUCUNE PERTE N'EST MUETTE. Le serveur qui ferme, la socket qui échoue, le
// silence qui dure, l'éviction, le passage raté vers un autre serveur de zone :
// tous passent par `DeclareLinkLoss`, qui ouvre la même boîte et écrit la même
// ligne. Le monde reste dessiné derrière, mais plus rien ne part : la liaison
// est fermée, et une liaison fermée n'émet plus.
//
// ⚠ « OK » RAMÈNE À L'ÉCRAN DE CONNEXION, pas à la sélection : sans serveur il
// n'y a plus de session, donc plus de jetons pour rouvrir la tablée. Le motif
// voyage jusqu'au formulaire, qui l'affiche sous ses champs.

#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/core/log.h"
#include "ro/net/link_loss.h"
#include "ro/net/link_watch.h"
#include "ro/sim/world.h"

#include "game/game.h"

namespace moonclient {

void Game::DeclareLinkLoss(std::string_view cause, bool say_in_chat, std::uint32_t now_ms) {
  link_.Close();
  windows_.link_notice.unsteady = false;
  // ⚠ LA PREMIÈRE CAUSE GAGNE : voir `ro::net::LinkLoss`.
  if (!link_loss_.Declare(cause)) {
    return;
  }
  windows_.link_notice.text = link_loss_.text();
  windows_.link_notice.acknowledged = false;
  // LA CONVERSATION GARDE LA TRACE, sur une seule ligne. ⚠ PAS POUR UNE
  // ÉVICTION : le monde vient d'écrire la sienne en lisant le paquet.
  if (say_in_chat) {
    world_.RecordChat(
        ro::sim::ChatKind::kServer, 0,
        std::string(ro::core::Tr("Déconnecté du serveur.")) + " " + std::string(cause), now_ms);
  }
  // 🔴 LA VEILLE S'ARRÊTE ICI, ET NE REPART PLUS tant que la perte est
  // déclarée — voir `Game::AdvanceAfk` : elle cache l'interface, donc la boîte.
  afk_screen_.EndNow(afk_idle_, camera_);
}

void Game::FollowLinkLoss() {
  // ⚠ LA BOÎTE APPARTIENT À LA COQUILLE, qui survit à la partie : une partie
  // neuve ne doit pas hériter de la boîte de la précédente.
  if (!link_loss_.declared()) {
    if (!windows_.link_notice.text.empty() || windows_.link_notice.acknowledged) {
      windows_.link_notice.text.clear();
      windows_.link_notice.acknowledged = false;
    }
    return;
  }
  if (!windows_.link_notice.acknowledged || link_loss_.acknowledged()) {
    return;
  }
  // LE JOUEUR A PRIS ACTE : la partie s'arrête par la même porte que le retour
  // à la sélection — la coquille écrit les réglages, `Close` la conversation —,
  // et `main` rouvre l'écran de connexion avec le motif.
  link_loss_.Acknowledge();
  windows_.link_notice.text.clear();
  windows_.link_notice.acknowledged = false;
  ro::core::LogInfo("liaison perdue : retour à l'écran de connexion");
  running_ = false;
}

void Game::WatchLinkSilence(bool received, std::uint32_t now_ms) {
  if (received) {
    link_watch_.NoteReceived(now_ms);
  }
  switch (link_watch_.Health(now_ms)) {
    case ro::net::LinkHealth::kSteady:
      windows_.link_notice.unsteady = false;
      return;
    case ro::net::LinkHealth::kUnsteady:
      windows_.link_notice.unsteady = true;
      return;
    case ro::net::LinkHealth::kLost:
      ro::core::LogError("liaison perdue : rien reçu du serveur depuis {} ms",
                         ro::net::kLinkSilentLossMs);
      DeclareLinkLoss(ro::net::LinkLossText(ro::net::LinkLossCause::kSilent), true, now_ms);
      return;
  }
}

}  // namespace moonclient
