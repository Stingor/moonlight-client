#pragma once

#include <cstdint>

#include "ro/net/connection.h"
#include "ro/net/world_update.h"
#include "ro/sim/map_guidance.h"
#include "ro/sim/world_fwd.h"

namespace ro::net {

// ── CE QU'UN SCRIPT MONTRE SUR LA CARTE ─────────────────────────────────────
//
// Deux commandes de script qui n'ont rien à changer dans le monde : poser ou
// retirer un repère (`viewpoint`), et demander une navigation (`navigateto`).
// Elles vivent hors de `ApplyFrame` : l'appelant soumet CHAQUE trame, et ce
// qu'il ne reconnaît pas rend `kUnhandled`.
//
// ⚠ `now_ms` EST L'HORLOGE DE L'APPELANT : elle date la pose d'un repère, dont
// l'interface se sert pour le faire clignoter et expirer.
//
// ⚠ LES REPÈRES SE VIDENT AU CHANGEMENT DE CARTE, et c'est l'appelant qui le
// sait : voir `sim::MapViewpoints::Clear`.
//
// LES DEMANDES DE NAVIGATION S'EMPILENT, et l'appelant les prend une à une avec
// `sim::NavigationRequests::Take`. Un type de cible inconnu rend `kIgnored` : la
// demande n'aurait aucun sens.
[[nodiscard]] FrameOutcome ApplyMapGuidanceFrame(const Frame& frame, sim::MapGuidance& guidance,
                                                 std::uint32_t now_ms);

// ── LE BOSS RÉVÉLÉ PAR UN CONVEX MIRROR ─────────────────────────────────────
//
// `ZC 0x0293` dit où se tient le boss de la carte, ou qu'il n'y en a pas, ou
// quand il reviendra. Chaque annonce efface d'abord la case connue ; seules
// « vivant » et « vivant, à annoncer » en posent une nouvelle. La première
// révélation, l'absence et la tombe s'écrivent aussi dans la conversation, en
// blanc : c'est pourquoi le monde est passé ici.
[[nodiscard]] FrameOutcome ApplyBossInfoFrame(const Frame& frame, sim::MapGuidance& guidance,
                                              sim::World& world, std::uint32_t now_ms);

}  // namespace ro::net
