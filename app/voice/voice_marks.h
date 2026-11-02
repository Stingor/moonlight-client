#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "ro/render/actor_pick.h"
#include "ro/render/camera.h"
#include "ro/render/game_images.h"
#include "ro/sim/world_fwd.h"
#include "ro/ui/voice_overlay.h"

#include "options/voice_options.h"

namespace moonclient {

// ── L'ICÔNE AU-DESSUS DE QUI PARLE ──────────────────────────────────────────
//
// 🔴 LE CLIENT SAIT LUI-MÊME QUI PARLE : chaque paquet de voix porte le nom de
// qui l'a dit. Il ne montre donc que les voix qu'il entend — un joueur bloqué
// ou sur un autre canal ne s'allume pas —, et la sienne sans attendre le
// serveur. Voir `ro::net::kVoiceSpeakingHatEffect`, qu'il ignore.

// Les têtes, à l'écran, des personnages dont la voix porte le nom, et la sienne
// quand sa voix part. ⚠ PAR LE NOM : le serveur voix ne connaît pas
// l'identifiant d'un acteur, et un nom de personnage est unique.
[[nodiscard]] std::vector<ro::ui::VoiceMarkView> VoiceMarksOnScreen(
    const ro::render::Camera& camera, float aspect_ratio,
    std::span<const ro::render::DrawnActor> drawn, std::span<const std::uint32_t> ids,
    const ro::sim::World& world, const ro::ui::VoiceOverlayState& voice, float viewport_width,
    float viewport_height);

// L'image de l'icône choisie, à sa taille d'interface ; vide pour « aucune »,
// ou quand l'installation ne la porte pas.
[[nodiscard]] ro::ui::SkinImage SpeakingIconImage(ro::render::GameImages* images,
                                                  SpeakingIcon icon);
// Toutes, au rang de `SpeakingIcon` : ce que la page Son montre au choix.
[[nodiscard]] std::array<ro::ui::SkinImage, static_cast<std::size_t>(SpeakingIcon::kCount)>
SpeakingIconImages(ro::render::GameImages* images);

}  // namespace moonclient
