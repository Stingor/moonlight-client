// CE QUE LA PAGE « GRAPHICS » DEMANDE À UNE SCÈNE DE CARTE : la même lecture
// pour la partie et pour la ville de fond de l'écran de connexion.

#include "world/scene_graphics.h"

#include <array>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/ground_light.h"
#include "ro/render/terrain_renderer.h"

#include "game/settings_file.h"
#include "options/graphics_options.h"
#include "support/temp_directory.h"
#include "world/actor_view.h"

using moonclient::ActorLighting;
using moonclient::ActorLightingOf;
using moonclient::ComposedScene;
using moonclient::GraphicsOptions;
using moonclient::SettingsFile;
using moonclient::SunShadowsApply;
using moonclient::TerrainStagesOf;
using moonclient::TimeComposedScene;
using ro::render::TerrainStages;
using ro::test_support::TempDirectory;

namespace {

// UN JOUEUR QUI A TOUT CHANGÉ : chaque valeur s'écarte de son défaut, si bien
// qu'une ligne oubliée laisse le défaut et se voit.
constexpr std::string_view kEverythingChanged =
    "shadows_of_the_map: false\n"
    "light_cold: true\n"
    "corpses_duration: 250\n"
    "spawn_fade_ms: 0\n"
    "exit_of_view_fade_ms: 0\n"
    "gesture_of_skill_min_ms: 300\n"
    "cast_on_the_bar: true\n";

void LoadInto(GraphicsOptions& graphics, const TempDirectory& folder, std::string_view text) {
  folder.WriteText("settings.yml", text);
  SettingsFile file(folder.path() / "settings.yml");
  graphics.Declare(file);
  file.Load();
}

}  // namespace

TEST_CASE("Scène de carte : par défaut, ombres et lumière de la carte, sans lumière froide",
          "[app][scene_graphics]") {
  const TerrainStages stages = TerrainStagesOf(GraphicsOptions(), /*lightmap_hidden=*/false);
  CHECK(stages.shadowmap);
  CHECK(stages.lightmap);
  CHECK_FALSE(stages.cold_light);
  // Les étages que le joueur ne règle pas gardent leur défaut.
  CHECK(stages.vertex_light == TerrainStages{}.vertex_light);
  CHECK(stages.tile_color == TerrainStages{}.tile_color);
}

TEST_CASE("Scène de carte : le sol uni de l'établi n'en sort jamais", "[app][scene_graphics]") {
  // 🔴 LA VILLE DE FOND PREND CE RÉSULTAT TEL QUEL : une couleur plate posée
  // ici remplacerait la ville derrière le formulaire.
  const TempDirectory folder;
  GraphicsOptions graphics;
  LoadInto(graphics, folder, kEverythingChanged);
  CHECK_FALSE(TerrainStagesOf(GraphicsOptions(), false).flat_color.has_value());
  CHECK_FALSE(TerrainStagesOf(graphics, true).flat_color.has_value());
}

TEST_CASE("Scène de carte : les ombres suivent le réglage, et GreyWorld les éteint",
          "[app][scene_graphics]") {
  const TempDirectory folder;
  GraphicsOptions changed;
  LoadInto(changed, folder, kEverythingChanged);
  const TerrainStages off = TerrainStagesOf(changed, false);
  CHECK_FALSE(off.shadowmap);
  CHECK_FALSE(off.lightmap);
  CHECK(off.cold_light);

  // Allumées par le joueur, mais cachées : éteintes.
  const TerrainStages hidden = TerrainStagesOf(GraphicsOptions(), true);
  CHECK_FALSE(hidden.shadowmap);
  CHECK_FALSE(hidden.lightmap);
}

TEST_CASE("Scène de carte : l'ombre du soleil, demandée, remplace celle de la carte en plein air",
          "[app][scene_graphics]") {
  // Éteinte par défaut, comme le client d'origine.
  CHECK_FALSE(SunShadowsApply(GraphicsOptions(), TerrainStagesOf(GraphicsOptions(), false),
                              /*indoor=*/false));

  const TempDirectory folder;
  GraphicsOptions asked;
  LoadInto(asked, folder, "render_shadows_of_the_sun: true\n");
  CHECK(SunShadowsApply(asked, TerrainStagesOf(asked, false), false));
  // Un intérieur n'a pas de soleil.
  CHECK_FALSE(SunShadowsApply(asked, TerrainStagesOf(asked, false), /*indoor=*/true));
  // GreyWorld qui cache l'ombre de la carte cache aussi celle du soleil.
  CHECK_FALSE(SunShadowsApply(asked, TerrainStagesOf(asked, true), false));
  // Le sol uni ne reçoit aucune ombre.
  TerrainStages flat = TerrainStagesOf(asked, false);
  flat.flat_color = std::array<float, 3>{};
  CHECK_FALSE(SunShadowsApply(asked, flat, false));

  // Sans les ombres de la carte, le soleil n'en peint pas non plus.
  const TempDirectory other_folder;
  GraphicsOptions unshadowed;
  LoadInto(unshadowed, other_folder,
           "render_shadows_of_the_sun: true\nshadows_of_the_map: false\n");
  CHECK_FALSE(SunShadowsApply(unshadowed, TerrainStagesOf(unshadowed, false), false));
}

TEST_CASE("Scène de carte : la lumière des corps suit les ombres et la lumière froide",
          "[app][scene_graphics]") {
  const ro::render::GroundLight ground;
  const ActorLighting lit = ActorLightingOf(GraphicsOptions(), &ground, nullptr);
  CHECK(lit.ground == &ground);
  CHECK_FALSE(lit.cold);
  CHECK(lit.ailment_tints == nullptr);

  const TempDirectory folder;
  GraphicsOptions changed;
  LoadInto(changed, folder, kEverythingChanged);
  const ActorLighting unlit = ActorLightingOf(changed, &ground, nullptr);
  CHECK(unlit.ground == nullptr);
  CHECK(unlit.cold);
}

TEST_CASE("Scène de carte : fondus et geste lisible passent à la scène composée",
          "[app][scene_graphics]") {
  const TempDirectory folder;
  GraphicsOptions graphics;
  LoadInto(graphics, folder, kEverythingChanged);
  ComposedScene scene;
  TimeComposedScene(graphics, scene);

  const ComposedScene untouched;
  CHECK(scene.corpse_fade_ms == graphics.corpse_fade_ms());
  CHECK(scene.corpse_fade_ms != untouched.corpse_fade_ms);
  CHECK(scene.appear_fade_ms == graphics.appear_fade_ms());
  CHECK(scene.appear_fade_ms != untouched.appear_fade_ms);
  CHECK(scene.leave_fade_ms == graphics.leave_fade_ms());
  CHECK(scene.leave_fade_ms != untouched.leave_fade_ms);
  CHECK(scene.readable_motion.skill_gesture_floor_ms ==
        static_cast<float>(graphics.skill_gesture_floor_ms()));
  CHECK(scene.readable_motion.skill_gesture_floor_ms > 0.0F);
  CHECK(scene.readable_motion.cast_spans_bar);
}
