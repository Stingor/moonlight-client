#include "game/handshake_pump.h"

#include "ro/core/log.h"

#include "game/game_pieces.h"
#include "game/surface_announcer.h"

namespace moonclient {

void PumpHandshake(ro::net::Connection& link, ro::net::MoonclientHandshake& handshake,
                   ro::net::MoonclientHandshakeState& reported, GamePieces& pieces,
                   bool mvp_tracker_enabled, std::uint32_t reading_caps, std::uint32_t now_ms) {
  handshake.Pump(link, now_ms);
  if (handshake.state() != reported) {
    reported = handshake.state();
    // 🔴 DEUX DES CINQ ÉTATS ANNONCENT LA FIN DE LA SESSION — le refus, qui
    // donne cinq secondes, et l'abandon après cinq envois. Les écrire au même
    // rang que « émise » et « acceptée » les faisait disparaître du filtre par
    // gravité à l'instant précis où le joueur y cherche pourquoi il a été
    // éjecté.
    const bool cut_off = reported == ro::net::MoonclientHandshakeState::kRefused ||
                         reported == ro::net::MoonclientHandshakeState::kGivenUp;
    if (cut_off) {
      ro::core::LogWarning("{}", ro::net::DescribeHandshake(reported));
    } else {
      ro::core::LogInfo("{}", ro::net::DescribeHandshake(reported));
    }
    if (reported == ro::net::MoonclientHandshakeState::kVerified) {
      // LE CATALOGUE DE LA BOUTIQUE, une fois par partie : le personnage est
      // sur sa carte, et le serveur écoute.
      pieces.cash_shop.RequestCatalogueOnce(link);
    }
  }
  // 🔴 LE SERVEUR NOUS RECONNAÎT : c'est alors qu'il accepte nos annonces, et il
  // les oublie à chaque entrée en zone. Sans elles, il refuse toute commande
  // d'album et dépouille les dialogues de leurs images. Le carnet y entre et en
  // sort avec son interrupteur.
  pieces.surfaces.Pump(link, handshake.state() == ro::net::MoonclientHandshakeState::kVerified,
                       ShownSurfaces(mvp_tracker_enabled) | reading_caps);
}

}  // namespace moonclient
