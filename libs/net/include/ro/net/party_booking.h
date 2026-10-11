#pragma once

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/party_booking.h"

namespace ro::net {

// LA RECHERCHE DE GROUPE : six paquets reçus, quatre émis. Voir
// `ro/sim/party_booking.h` et `ro/proto/party_booking_packets.h`.
//
// `list_shown` dit si la liste des annonces est ouverte : une annonce poussée y
// entre alors, au lieu de se signaler dans la conversation.
[[nodiscard]] FrameOutcome ApplyPartyBookingFrame(const Frame& frame, sim::PartyBooking& booking,
                                                  bool list_shown);

// « Cherche. » Refusé, sans rien envoyer, sur des critères fautifs — voir
// `sim::CheckPartyBookingSearch`.
[[nodiscard]] core::Result<void> RequestPartyBookingSearch(
    Connection& link, sim::PartyBooking& booking, const sim::PartyBookingCriteria& criteria);

// « La page suivante », avec les critères de la dernière recherche. Refusé si
// le serveur n'en annonce pas.
[[nodiscard]] core::Result<void> RequestPartyBookingNextPage(Connection& link,
                                                             sim::PartyBooking& booking);

// « Publie mon annonce. » `draft` porte le niveau, le lieu, les métiers et le
// nom du joueur. Refusé si le joueur en a déjà une, si une publication attend
// son accusé, ou sur une annonce fautive.
[[nodiscard]] core::Result<void> RequestPartyBookingRegister(Connection& link,
                                                             sim::PartyBooking& booking,
                                                             const sim::PartyBookingAd& draft);

// « Change les métiers de mon annonce. » Refusé sans annonce, ou sans métier.
[[nodiscard]] core::Result<void> RequestPartyBookingUpdate(Connection& link,
                                                           const sim::PartyBooking& booking,
                                                           const sim::PartyBookingJobs& jobs);

// « Retire mon annonce. » Refusé sans annonce.
[[nodiscard]] core::Result<void> RequestPartyBookingDelete(Connection& link,
                                                           const sim::PartyBooking& booking);

}  // namespace ro::net
