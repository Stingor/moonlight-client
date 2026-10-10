#pragma once

#include <cstdint>
#include <vector>

#include "ro/render/camera.h"
#include "ro/ui/sound_zones.h"

#include "world/actor_sounds.h"
#include "world/map_stage.h"

namespace moonclient {

// LES ZONES DE SON DE LA CARTE, prêtes à dessiner : l'outil de l'établi du staff.
//
// Chaque source passe du repère du client au nôtre, se pose sur le TERRAIN
// point par point — une carte en pente ferait flotter ou s'enterrer un
// rectangle posé à une seule hauteur — et se projette. Seules les sources
// proches de l'auditeur se montrent : une carte en porte des centaines, et les
// projeter toutes à chaque image ne montrerait qu'un fouillis lointain.
[[nodiscard]] std::vector<ro::ui::SoundZoneView> SoundZoneViews(
    const ActorSounds& sounds, const MapStage& stage, const ro::render::Camera& camera,
    float aspect_ratio, float viewport_width, float viewport_height, std::uint32_t now_ms);

}  // namespace moonclient
