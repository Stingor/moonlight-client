#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include <glm/glm.hpp>

#include "ro/render/point_lights.h"
#include "ro/render/sprite_renderer.h"

#include "survivors/horde_composer.h"

// LES LUMIÈRES DE LA NUIT DE L'ARÈNE : ce qui garde la partie lisible quand le
// ciel tombe — la lanterne qui suit le héros, et la lueur des objets au sol.
// Toutes deux grandissent avec la part de nuit (`ArenaNightShare`). Données
// pures : la scène les pose, le rendu ne connaît pas la simulation.
namespace moonclient {

// LA LANTERNE DU HÉROS paraît au premier palier de la nuit (7:00) avec ce
// rayon, et l'élargit palier après palier jusqu'à celui de la nuit entière
// (15:00), en cases.
inline constexpr float kArenaLanternDuskRadiusCells = 4.0F;
inline constexpr float kArenaLanternNightRadiusCells = 8.0F;
// SON INTENSITÉ, de même : faible quand le ciel est encore clair, où elle
// ne ferait que blanchir le sol, pleine sous le ciel du soir. Elle S'AJOUTE à la
// lumière de la carte et décroît par le carré de ce qui reste de sa portée :
// le sol sous le héros est éclairé, et le bord de son rayon se fond.
inline constexpr float kArenaLanternDuskStrength = 0.3F;
inline constexpr float kArenaLanternNightStrength = 1.0F;
// Un blanc chaud de lanterne, intensité un.
inline constexpr glm::vec3 kArenaLanternColor{1.0F, 0.88F, 0.66F};
// Elle se tient à hauteur de tête au-dessus des pieds du héros, en unités du
// monde : assez haut pour que le sol autour de lui soit éclairé d'égal à égal.
inline constexpr float kArenaLanternHeightWorld = 10.0F;

// LE RAYON DE LA LANTERNE à `presentation_ms`, en cases ; zéro avant la nuit.
[[nodiscard]] float ArenaLanternRadiusCells(std::uint64_t presentation_ms) noexcept;

// LA LANTERNE à `presentation_ms` autour des pieds `hero_feet_world` ; rien
// avant la nuit. ⚠ SANS OMBRE : elle suit le héros au milieu de la horde, et
// chaque corps repeindrait sa carte d'ombre à chaque image.
[[nodiscard]] std::optional<ro::render::PointLight> ArenaLantern(std::uint64_t presentation_ms,
                                                                 glm::vec3 hero_feet_world);

// Les pieds du héros parmi les corps composés, appariés à leurs étiquettes ;
// rien s'il n'a pas de corps.
[[nodiscard]] std::optional<glm::vec3> ArenaHeroFeet(std::span<const ro::render::DrawnActor> drawn,
                                                     std::span<const HordeTag> tags);

// LA LUEUR D'UN OBJET AU SOL — gemme, objet, coffre, carte — : son sprite peint
// une seconde fois, élargi de tant de pixels d'écran de chaque côté et AJOUTÉ,
// parmi les halos qui évitent la silhouette des corps. Seule la frange qui
// déborde de l'objet se voit : sa couleur propre ne change pas, et la frange
// garde ses teintes. Elle n'est pas assombrie par l'heure.
inline constexpr float kArenaItemHaloScreenPixels = 5.0F;
// SON OPACITÉ, de zéro à 255 : discrète de jour, franche à la nuit entière.
inline constexpr float kArenaItemHaloDayAlpha = 40.0F;
inline constexpr float kArenaItemHaloNightAlpha = 200.0F;
inline constexpr float kArenaOpaqueAlpha = 255.0F;

// L'opacité de la lueur à `presentation_ms`, avant celle de l'objet lui-même.
[[nodiscard]] float ArenaItemHaloAlpha(std::uint64_t presentation_ms) noexcept;

// LES LUEURS DE L'IMAGE, une par gemme ou objet au sol de `drawn` (apparié à
// `tags`), rangées dans `out` (vidé). Un objet qui fond emporte sa lueur.
void ArenaItemHalos(std::span<const ro::render::DrawnActor> drawn, std::span<const HordeTag> tags,
                    std::uint64_t presentation_ms, std::vector<ro::render::DrawnActor>& out);

}  // namespace moonclient
