#pragma once

#include <cstdint>
#include <optional>

#include <glm/glm.hpp>

#include "ro/render/point_lights.h"

#include "survivors/arena_ground_look.h"
#include "survivors/arena_wall_lamps.h"

// LA PANNE DE COURANT d'un biome sans ciel (`ArenaDarkness::kBlackout`) : sur
// l'horloge de la nuit des champs (`ArenaNightShare`), les lampes murales
// s'éteignent par paliers au lieu que le ciel tombe ; la lanterne du héros
// prend le relais comme ailleurs. À l'arrivée du boss final, celles qui
// restent passent au rouge et clignotent. Données pures : la scène pose les
// lumières, le rendu ne connaît pas la simulation.
namespace moonclient {

// LA PART DE LAMPES ÉTEINTES à la nuit entière (15:00) : celles qui restent
// sonnent l'alarme.
inline constexpr float kArenaBlackoutMostShare = 0.75F;

// LA LUMIÈRE D'UNE LAMPE, un blanc froid de laboratoire, et sa portée en
// cases. Son intensité dépasse un : la lumière de la carte reste, et une
// lampe doit faire, en s'éteignant, un trou qui se voit.
inline constexpr glm::vec3 kWallLampColor{0.82F, 1.0F, 0.88F};
inline constexpr float kWallLampStrength = 1.2F;
inline constexpr float kWallLampRangeCells = 10.0F;

// L'ALARME : un rouge de gyrophare, qui bat sur cette période ; au creux du
// battement, il garde cette part de son éclat. Chaque lampe bat décalée de
// son rang dans la panne : les gyrophares d'une salle ne tournent pas
// ensemble.
inline constexpr glm::vec3 kWallLampAlarmColor{1.0F, 0.12F, 0.06F};
inline constexpr std::uint64_t kWallLampAlarmPeriodMs = 1200;
inline constexpr float kWallLampAlarmLeastShare = 0.1F;

// La part de lampes éteintes à `presentation_ms` : rien avant 7:00, puis les
// paliers de la nuit jusqu'à `kArenaBlackoutMostShare`.
[[nodiscard]] float ArenaLampsOutShare(std::uint64_t presentation_ms) noexcept;

// Vrai dès l'arrivée du boss final : les lampes qui restent sonnent l'alarme.
[[nodiscard]] bool ArenaAlarm(std::uint64_t presentation_ms) noexcept;

// Vrai si `lamp` luit encore à `presentation_ms`.
[[nodiscard]] bool WallLampLit(const WallLamp& lamp, std::uint64_t presentation_ms) noexcept;

// LA LUMIÈRE DE `lamp` à `presentation_ms`, `floor_world` étant le sol sous
// sa case et `light_height_world` la hauteur où elle luit
// (`WallLampLook::light_height_world`) ; rien une fois éteinte.
// ⚠ SANS OMBRE : une salle en porte des dizaines.
[[nodiscard]] std::optional<ro::render::PointLight> WallLampLight(const WallLamp& lamp,
                                                                  glm::vec3 floor_world,
                                                                  float light_height_world,
                                                                  std::uint64_t presentation_ms);

// L'HEURE DU CIEL d'un biome à `presentation_ms` : celle des champs
// (`ArenaSkyHour`) quand la nuit y tombe, aucune dans un biome en panne, qui
// garde la lumière de sa carte.
[[nodiscard]] std::optional<float> ArenaSkyHourOf(ArenaDarkness darkness,
                                                  std::uint64_t presentation_ms) noexcept;

}  // namespace moonclient
