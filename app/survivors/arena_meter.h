#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "ro/sim/damage_meter.h"
#include "ro/sim/survivors_news.h"
#include "ro/sim/survivors_run.h"

// LE COMPTEUR DE DÉGÂTS DANS L'ARÈNE : les coups du héros comptent comme ceux
// du personnage — infligés, au nom du skill et de son niveau, contre le
// monstre touché, et encaissés. Le compteur ne voit sinon que ce que le
// serveur annonce, et l'arène ne lui annonce rien.
namespace moonclient {

// Le nom d'un monstre de l'arène, par son Id.
using ArenaFoeNamer = std::function<std::string(std::uint16_t foe_id)>;

// Porte au compteur les coups d'une image de l'arène, à l'heure `now_ms` de
// l'horloge du compteur.
void RecordArenaBlows(const ro::sim::SurvivorsNews& news, const ro::sim::SurvivorsField& field,
                      const ArenaFoeNamer& foe_name_of, ro::sim::DamageMeter& meter,
                      std::uint32_t now_ms);

}  // namespace moonclient
