#pragma once

#include <cstdint>
#include <span>
#include <string_view>

namespace moonclient {

// LES MÉTIERS QUE « @jobchange » ACCEPTE, tels que le serveur les nomme.
//
// 🔴 RELEVÉS SUR LE SERVEUR, PAS ÉCRITS À LA MAIN : la table est engendrée par
// `scripts/gen_server_jobchange.py` depuis son relevé versionné
// (`scripts/server_jobchange.json`). Un métier y figure quand la commande le
// mène à son terme : ni refusé nommément (montures, tenues), ni hors de
// `pcdb_checkid`, ni sans conversion, ni absent de la job_db.
struct ServerJobChangeTarget {
  std::uint16_t job_id = 0;
  // Le nom que le serveur lui donne, en anglais : celui que la commande
  // reconnaît quand on l'écrit en toutes lettres.
  std::string_view server_name;
};

// Tous, triés par numéro.
[[nodiscard]] std::span<const ServerJobChangeTarget> ServerJobChangeTargets();

}  // namespace moonclient
