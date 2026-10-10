#pragma once

#include <cstdint>

#include "ro/core/error.h"
#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/store_search.h"

namespace ro::net {

// LA RECHERCHE D'ÉCHOPPES : quatre paquets reçus, quatre émis. Voir
// `ro/sim/store_search.h`.
//
// Ce traducteur lit aussi, SANS LES CONSOMMER, les deux changements de carte :
// le serveur ferme la recherche quand on quitte la sienne.
[[nodiscard]] FrameOutcome ApplyStoreSearchFrame(const Frame& frame, sim::StoreSearch& search);

// « Cherche. » Le plan vient de `sim::PlanStoreSearch` ; refusé, sans rien
// envoyer, hors d'une recherche ouverte ou sur un plan fautif.
[[nodiscard]] core::Result<void> RequestStoreSearch(Connection& link,
                                                    const sim::StoreSearch& search,
                                                    const sim::StoreSearchPlan& plan);

// « La page suivante. » Refusé si le serveur n'en annonce pas.
[[nodiscard]] core::Result<void> RequestStoreSearchNextPage(Connection& link,
                                                            const sim::StoreSearch& search);

// « Ce résultat-là. » Le marqueur s'efface et l'enseigne du marchand s'allume
// dès l'envoi. Refusé hors d'une recherche ouverte.
[[nodiscard]] core::Result<void> RequestStoreSearchClick(Connection& link, sim::StoreSearch& search,
                                                         const sim::StoreSearchResult& result);

// « Je ferme la recherche. » Le modèle la ferme à l'envoi — même si l'envoi
// échoue — : le serveur ne répond pas. Refusé si aucune n'est ouverte.
[[nodiscard]] core::Result<void> RequestStoreSearchClose(Connection& link,
                                                         sim::StoreSearch& search);

}  // namespace ro::net
