#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "ro/net/world_update.h"
#include "ro/sim/member_statuses.h"
#include "ro/sim/party_roster.h"

namespace ro::net {

// ── LES ÉTATS DES MEMBRES DU GROUPE, TIRÉS DU FIL ───────────────────────────
//
// Deux familles de trames nourrissent `sim::MemberStatuses` :
//
//   · `ZC 0x0F2D`, la liste complète que le serveur rend à une demande
//     `CZ 0x0F2C` — la même trame que la cible lit de son côté : les deux
//     traducteurs la reçoivent, chacun y prend le compte qui le regarde ;
//   · les transitions d'état diffusées à la zone, que le monde lit aussi.
//
// Seuls les MEMBRES DU GROUPE, moi excepté, y entrent : la diffusion à la zone
// arrive pour tout joueur à l'écran, adversaire compris, et le serveur ne
// décrit en entier que les membres de mon groupe. Mes propres états ont leur
// liste à eux, dans le monde.
//
// Ce qui ne concerne pas un membre rend `kUnhandled`.
[[nodiscard]] FrameOutcome ApplyMemberStatusFrame(const Frame& frame, sim::MemberStatuses& statuses,
                                                  const sim::PartyRoster& party,
                                                  std::uint32_t own_account_id,
                                                  std::uint32_t now_ms);

// Les entrées d'une réponse `ZC 0x0F2D`, dont le bloc fixe annonce `count`. Le
// compte est borné par ce que la trame porte et par le plafond du serveur ; rien
// quand la trame est illisible.
[[nodiscard]] std::optional<std::vector<sim::StatusReport>> ReadStatusReports(const Frame& frame,
                                                                              std::uint8_t count);

// `status` d'une réponse `ZC 0x0F2D` : le compte n'est pas de mon groupe.
inline constexpr std::uint8_t kStatusListRefused = 2;

// UNE RÉPONSE `ZC 0x0F2D` ENTIÈRE : l'entité, le verdict du serveur, et ses
// états. ⚠ SEUL `status` À ZÉRO FAIT FOI, liste vide comprise : un ou deux
// disent que le serveur ne sait pas, et les états sont alors vides sans rien
// dire.
struct StatusListAnswer {
  std::uint32_t gid = 0;
  std::uint8_t status = 0;
  std::vector<sim::StatusReport> reports;
};
// Rien pour une autre trame, ou une trame illisible.
[[nodiscard]] std::optional<StatusListAnswer> ReadStatusList(const Frame& frame);

}  // namespace ro::net
