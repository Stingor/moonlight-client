// LE PASSAGE D'UN SERVEUR DE ZONE À UN AUTRE, quand le serveur courant passe la
// main : une liaison neuve s'ouvre, et la partie reste hors ligne tant qu'elle
// n'a pas été acceptée.
//
// 🔴 UNE ENTRÉE IMPOSSIBLE, REFUSÉE, ÉCHOUÉE OU SANS RÉPONSE LAISSE LA PARTIE HORS LIGNE POUR
// DE BON : c'est une perte de liaison, et elle se dit comme les autres — voir
// `game_link_loss.cpp`.

#include <utility>

#include "ro/core/log.h"
#include "ro/net/link_loss.h"
#include "ro/net/link_watch.h"
#include "ro/net/moonclient_handshake.h"
#include "ro/net/session.h"

#include "game/game.h"
#include "patcher/patcher_state.h"

namespace moonclient {

// La poignée de main d'un serveur de zone qui ne nous a pas encore entendus,
// avec le dernier patch que le patcheur a posé dans cette installation.
ro::net::MoonclientHandshake Game::FreshHandshake() const {
  return ro::net::MoonclientHandshake(
      ro::net::ClientIdentityOf(patcher::ReadLastAppliedPatch(client_root_)));
}

void Game::StartZoneMove(const ro::net::CharacterPhase::Handoff& handoff, std::uint32_t now_ms) {
  link_.Close();
  if (!ticket_.usable()) {
    ro::core::LogError(
        "changement de serveur de zone vers {} : aucune entrée connue, la partie "
        "reste hors ligne",
        handoff.map_name);
    DeclareLinkLoss(ro::net::LinkLossText(ro::net::LinkLossCause::kZoneMoveFailed), true, now_ms);
    return;
  }
  ro::core::LogInfo("changement de serveur de zone vers {}", handoff.map_name);
  auto opened = ro::net::MapPhase::Start(handoff, ticket_.account, now_ms);
  if (!opened.has_value()) {
    ro::core::LogError("changement de serveur de zone : {}", ro::core::Describe(opened.error()));
    DeclareLinkLoss(ro::net::LinkLossText(ro::net::LinkLossCause::kZoneMoveFailed), true, now_ms);
    return;
  }
  // UNE LIAISON NEUVE, DONC UN FICHIER DE CAPTURE NEUF quand on enregistre — voir
  // `ZoneStreamCapture`. Posé avant la première lecture : l'accord d'entrée
  // est la toute première trame.
  if (ro::net::ReceiveObserver capture = shell_.ObserveZoneLink(); capture) {
    opened->SetReceiveObserver(std::move(capture));
  }
  moving_.emplace(std::move(*opened));
  moving_since_ms_ = now_ms;
}

void Game::AdvanceZoneMove(std::uint32_t now_ms) {
  switch (moving_->Advance()) {
    case ro::net::PhaseState::kWaiting:
      // ⚠ L'ATTENTE A UNE FIN : voir `ro::net::ZoneMoveOverdue`.
      if (!ro::net::ZoneMoveOverdue(moving_since_ms_, now_ms)) {
        return;
      }
      ro::core::LogError("serveur de zone : aucune réponse depuis {} ms",
                         now_ms - moving_since_ms_);
      break;
    case ro::net::PhaseState::kDone:
      link_ = moving_->TakeLink();
      // LA LIAISON EST NEUVE : les ateliers doivent la regarder à nouveau.
      pieces_.craft.Watch(link_, world_);
      pieces_.trade.deal().Watch(link_);
      // 🔴 LA POIGNÉE DE MAIN SE REFAIT : c'est un AUTRE serveur, qui arme son
      // propre minuteur et n'a jamais entendu la première.
      handshake_ = FreshHandshake();
      reported_handshake_ = ro::net::MoonclientHandshakeState::kUnsent;
      // Ce serveur n'a jamais entendu nos surfaces non plus.
      pieces_.surfaces.Rearm();
      // Ni ceux qu'on bloquait : il repart d'une liste vide.
      pieces_.whispers.blocks().Clear();
      ro::core::LogInfo("serveur de zone : entrée acceptée");
      moving_.reset();
      return;
    case ro::net::PhaseState::kRefused:
      ro::core::LogError("serveur de zone : entrée refusée (code {})", moving_->refusal_code());
      break;
    case ro::net::PhaseState::kFailed:
      ro::core::LogError("serveur de zone : {}", ro::core::Describe(moving_->error()));
      break;
  }
  moving_.reset();
  DeclareLinkLoss(ro::net::LinkLossText(ro::net::LinkLossCause::kZoneMoveFailed), true, now_ms);
}

}  // namespace moonclient
