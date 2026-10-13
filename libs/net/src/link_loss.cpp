#include "ro/net/link_loss.h"

#include <cstddef>
#include <string>
#include <string_view>

#include "ro/core/i18n.h"
#include "ro/net/refusal_text.h"

namespace ro::net {

LinkLossCause LinkLossCauseOf(LinkStatus status) noexcept {
  switch (status) {
    case LinkStatus::kClosedByPeer:
      return LinkLossCause::kClosedByPeer;
    case LinkStatus::kProtocolError:
      return LinkLossCause::kProtocolError;
    case LinkStatus::kIoError:
    case LinkStatus::kOpen:
      break;
  }
  return LinkLossCause::kIoError;
}

std::string LinkLossText(LinkLossCause cause) {
  switch (cause) {
    case LinkLossCause::kClosedByPeer:
      return core::Tr("Le serveur a fermé la connexion.");
    case LinkLossCause::kSilent:
      return core::Tr("Le serveur ne répond plus.");
    case LinkLossCause::kProtocolError:
      return core::Tr("Le serveur a envoyé une réponse que ce client ne sait pas lire.");
    case LinkLossCause::kIoError:
      return core::Tr("La connexion réseau a été interrompue.");
    case LinkLossCause::kZoneMoveFailed:
      return core::Tr("Le passage vers un autre serveur de carte a échoué.");
  }
  return core::Tr("La connexion réseau a été interrompue.");
}

std::string EvictionText(std::uint8_t code) {
  // 🔴 UNE SUBSTITUTION QUI NE PEUT PAS ÉCHOUER. Le gabarit vient du catalogue,
  // que le joueur peut éditer : une accolade de travers ne doit pas faire
  // tomber le client au moment même où il doit dire pourquoi la session finit.
  // Le premier « {} » reçoit le motif ; sans « {} », le motif suit le gabarit.
  const std::string why = BanText(code);
  const std::string_view pattern = core::Tr("Motif : {}.");
  constexpr std::string_view kHole = "{}";
  const std::size_t hole = pattern.find(kHole);
  if (hole == std::string_view::npos) {
    return std::string(pattern) + " " + why;
  }
  return std::string(pattern.substr(0, hole)) + why +
         std::string(pattern.substr(hole + kHole.size()));
}

bool LinkLoss::Declare(std::string_view cause) {
  if (declared_) {
    return false;
  }
  declared_ = true;
  text_ = std::string(core::Tr("Déconnecté du serveur.")) + "\n" + std::string(cause);
  return true;
}

void LinkLoss::Acknowledge() noexcept {
  if (declared_) {
    acknowledged_ = true;
  }
}

}  // namespace ro::net
