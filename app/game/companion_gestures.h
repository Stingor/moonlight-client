#pragma once

// LES GESTES DE LA SOURIS QUI DONNENT DES ORDRES AUX COMPAGNONS.
//
// Le geste de chacun vient de la table des raccourcis (« Homoncule : ordre »,
// « Mercenaire : ordre ») : Alt + clic DROIT et Alt + clic GAUCHE par défaut.
// Sur un acteur attaquable, deux appuis séparés l'attaquent ; au sol, « aller
// ici » ; Maj range l'ordre dans la file réservée. Un skill de compagnon qui
// attend sa cible prend le clic gauche suivant, sans modificateur.

#include <cstdint>
#include <optional>
#include <span>

#include <glm/glm.hpp>

#include "ro/render/camera.h"
#include "ro/render/sprite_renderer.h"
#include "ro/sim/companion_ai.h"
#include "ro/sim/world_fwd.h"

#include "world/map_stage.h"

namespace moonclient {

class CompanionAiStage;

// CE QUE LE GESTE VOIT DU MONDE SOUS LA SOURIS.
struct CompanionPointer {
  const ro::sim::World* world = nullptr;
  const MapStage* stage = nullptr;
  const ro::render::Camera* camera = nullptr;
  std::span<const ro::render::DrawnActor> drawn;
  std::span<const std::uint32_t> drawn_ids;
  float aspect_ratio = 1.0F;
  float viewport_width = 0.0F;
  float viewport_height = 0.0F;
  glm::vec2 pointer{};
};

// UN BOUTON DROIT QUI A BOUGÉ de plus que ces pixels entre l'appui et le
// relâchement est un glissé de caméra, pas un ordre.
inline constexpr float kCompanionClickSlopPixels = 4.0F;

// LE CLIC GAUCHE : le skill de compagnon armé d'abord, puis l'ordre au
// compagnon que ce clic commande (`ordered`, lu dans la table), s'il est là.
// Rend vrai quand le clic est pris. ⚠ SANS LE COMPAGNON, LE GESTE GARDE SON
// SENS ORDINAIRE. ⚠ ET LE GRIMOIRE ARMÉ (`grimoire_aiming`) GARDE LE CLIC.
[[nodiscard]] bool CompanionLeftPress(CompanionAiStage& ai, const CompanionPointer& at,
                                      bool grimoire_aiming,
                                      std::optional<ro::sim::AiCompanion> ordered, bool shift_held,
                                      bool second_of_double);

// LE RELÂCHEMENT DU BOUTON DROIT qui commande un compagnon (`ordered`), s'il
// est là. Rend vrai quand le geste est pris ; un acteur qui n'est pas
// attaquable — un joueur, un PNJ — le laisse au menu d'entité.
[[nodiscard]] bool CompanionRightRelease(CompanionAiStage& ai, const CompanionPointer& at,
                                         std::optional<ro::sim::AiCompanion> ordered,
                                         bool shift_held, bool second_of_double);

}  // namespace moonclient
