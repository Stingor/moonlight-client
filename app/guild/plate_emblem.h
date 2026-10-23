#pragma once

#include <cstdint>

#include "ro/formats/image.h"
#include "ro/render/clan_emblem.h"
#include "ro/render/entity_names.h"
#include "ro/sim/actor.h"
#include "ro/sim/guild.h"
#include "ro/sim/world_fwd.h"

#include "guild/emblem_plates.h"

namespace moonclient {

// L'IMAGE DE TÊTE D'UNE ÉTIQUETTE : l'emblème de guilde, ou celui du clan.
//
// 🔴 LA GUILDE PASSE D'ABORD, ET ELLE SEULE DÉCIDE QUAND ELLE EST LÀ. Qui a une
// guilde ne montre jamais son clan — ni quand sa guilde n'a pas d'emblème, ni
// quand la case de l'emblème de guilde est décochée. Le clan ne paraît que chez
// qui n'a pas de guilde, et sa case le commande seule.

enum class PlateEmblemKind : std::uint8_t { kNone, kGuild, kClan };

struct PlateEmblemChoice {
  PlateEmblemKind kind = PlateEmblemKind::kNone;
  // La guilde ou le clan désigné ; zéro pour `kNone`.
  std::int32_t owner_id = 0;
};

// La règle, sur des données pures. `guild_id` et `clan_id` valent zéro quand
// l'acteur n'en a pas.
[[nodiscard]] PlateEmblemChoice ChoosePlateEmblem(std::int32_t guild_id, std::int32_t clan_id,
                                                  const ro::render::EntityNamesLook& look);

// Le clan que l'état `kEfstClanInfo` de cet acteur désigne, ou zéro.
//
// ⚠ VAUT AUSSI POUR LE JOUEUR : le serveur lui pose cet état à chaque entrée
// sur une carte, comme à n'importe quel membre.
[[nodiscard]] std::int32_t ClanIdOf(const ro::sim::Actor& actor);

// L'image que l'étiquette de cet acteur porte à gauche, ou nul. `plates` peut
// être nul : les emblèmes de guilde ne se cherchent pas encore.
[[nodiscard]] const ro::formats::Image* NamePlateEmblem(const ro::sim::Actor& actor,
                                                        const ro::sim::World& world,
                                                        const ro::sim::GuildBook& guild,
                                                        EmblemPlates* plates,
                                                        const ro::render::ClanEmblems& clans,
                                                        const ro::render::EntityNamesLook& look);

}  // namespace moonclient
