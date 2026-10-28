#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "ro/core/error.h"
#include "ro/sim/survivors_rules.h"
#include "ro/sim/walk_map.h"
#include "ro/vfs/map_display_names.h"
#include "ro/vfs/vfs.h"

#include "survivors/arena_ground_look.h"

namespace moonclient {

// ── L'ARÈNE D'UNE PARTIE ────────────────────────────────────────────────────
//
// L'arène vient du départ (`SurvivorsStart::arena_id`) : la partie libre joue
// celle que l'accueil a retenue (`FreeArenaOf`), le défi et la partie classée
// l'arène classée (Q6, Q7). Ce fichier dit ce que la salle en tire : le sol que la simulation
// lit, le nom affiché, le bandeau d'entrée et la musique des champs.
//
// Une arène générée (`ArenaRule::terrain_id`) n'a aucun fichier : son sol est
// celui de son terrain sous la graine de la partie (`SurvivorsRun::Start`), et
// son nom, son bandeau et sa musique sont ceux de son biome
// (`arena_ground_look.h`). Une carte se lit dans l'installation, et se nomme
// par la table des cartes.

// LE SOL DE L'ARÈNE POUR LA SIMULATION : la carte lue dans l'installation, ou
// rien pour une arène générée — la partie naît alors sans installation, et
// `vfs` peut être nul. kNotFound pour une carte sans installation ; les refus
// du GAT tels quels.
[[nodiscard]] ro::core::Result<std::optional<ro::sim::WalkMap>> ArenaWalkOf(
    ro::vfs::Vfs* vfs, const ro::sim::ArenaRule& arena);

// L'aspect du biome d'une arène générée ; nul pour une carte, ou pour un
// terrain que le catalogue d'app ne dessine pas.
[[nodiscard]] const BiomeLook* ArenaLookOf(const ro::sim::SurvivorsRules& rules,
                                           const ro::sim::ArenaRule& arena);

// LE NOM AFFICHÉ, en UTF-8 et dans la langue du joueur : celui du biome, sinon
// celui que la table des cartes donne (`names` peut être nul), sinon la clé.
[[nodiscard]] std::string ArenaNameOf(const ro::sim::SurvivorsRules& rules,
                                      const ro::sim::ArenaRule& arena,
                                      const ro::vfs::MapDisplayNames* names);

// LE BANDEAU D'ENTRÉE : celui de la table des cartes pour une carte ; pour une
// arène générée, le fond et le nom de son biome, annoncés comme une carte
// récente — le réglage du joueur décide ensuite (`MapSignWanted`). Rien quand
// il n'y a de quoi en montrer aucun.
[[nodiscard]] std::optional<ro::vfs::MapSign> ArenaSignOf(const ro::sim::SurvivorsRules& rules,
                                                          const ro::sim::ArenaRule& arena,
                                                          const ro::vfs::MapDisplayNames* names);

// LA MUSIQUE DES CHAMPS (`ArenaTrackAt`) : celle du biome, sinon celle des
// champs de Prontera.
[[nodiscard]] std::string_view ArenaFieldTrackOf(const ro::sim::SurvivorsRules& rules,
                                                 const ro::sim::ArenaRule& arena);

}  // namespace moonclient
