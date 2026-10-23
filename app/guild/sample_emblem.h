#pragma once

#include "ro/render/entity_names.h"
#include "ro/render/game_images.h"
#include "ro/sim/guild.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/entity_sample.h"
#include "ro/ui/skin_art.h"

#include "guild/guild_emblems.h"

namespace moonclient {

// L'EMBLÈME QUE L'APERÇU D'ÉTIQUETTE MONTRE, ou une image vide.
//
// Le cadre montre MON personnage : il porte donc ce que mon étiquette porterait
// en jeu, par la même règle — `ChoosePlateEmblem`. Ma guilde vient du carnet,
// mon clan de l'état que le serveur pose sur mon acteur. Rien quand l'aperçu
// montre une autre espèce que la mienne.
[[nodiscard]] ro::ui::SkinImage SampleEmblem(const ro::ui::EntitySample& sample,
                                             const ro::sim::World& world,
                                             const ro::sim::GuildBook& guild,
                                             const GuildEmblems* emblems,
                                             ro::render::GameImages* images,
                                             const ro::render::EntityNamesLook& look);

}  // namespace moonclient
