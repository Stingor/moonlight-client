#pragma once

#include <span>
#include <vector>

#include "ro/render/camera.h"
#include "ro/render/effect_lights.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/effect_track_motion.h"
#include "ro/render/ground_quad_renderer.h"
#include "ro/render/scene_fog.h"
#include "ro/render/sprite_renderer.h"

#include "effects/effect_node_view.h"
#include "effects/effect_view.h"
#include "world/map_stage.h"

// CE QUI PEINT LES EFFETS D'UNE IMAGE, une fois qu'ils ont vécu : la partie, le
// pantin d'un aperçu et l'arène survivors les enchaînent dans le même ordre.
namespace moonclient {

// LE SOL QUE LA TROUPE PROCÉDURALE INTERROGE, tiré de la carte : un nœud qui
// rebondit cherche le sol à chaque tic, et sans réponse il le traverse.
[[nodiscard]] ro::render::EffectScatterWorld EffectScatterWorldOf(const MapStage& stage);

// ── CE QUI PEINT LES EFFETS D'UNE IMAGE, une fois qu'ils ont vécu ───────────
//
// 🔴 TROIS TEMPS, ET UN SEUL ORDRE pour tous ceux qui peignent des effets : la
// partie les enchaîne dans `DrawEffectLayers`, en semant entre le deuxième et le
// troisième ce que les nœuds viennent de demander ; le pantin d'un aperçu les
// enchaîne dans sa bulle. Deux ordres finiraient par ne plus montrer la même
// chose.
//
// ⚠ ILS NE FONT RIEN VIVRE : `EffectsInPlay::Drawn` et `EffectNodesInPlay::Drawn`
// ont tourné à cette image, et `played` est ce que le premier a rendu.
struct EffectPainters {
  ro::render::SpriteRenderer* sprites = nullptr;
  ro::render::EffectRenderer* effects = nullptr;
  // Les cases de terrain. ⚠ NUL, OU UNE CARTE SANS GRILLE : elles ne se
  // peignent pas.
  ro::render::GroundQuadRenderer* ground_quads = nullptr;
  const MapStage* stage = nullptr;
  const ro::render::Camera* camera = nullptr;
  float aspect_ratio = 1.0F;
  float viewport_width = 0.0F;
  float viewport_height = 0.0F;
  ro::render::SceneFog fog;
};

// Ce qui passe SOUS les fichiers `.str`.
// LES SPRITES DE CLASSE QUI PASSENT SOUS LES CORPS — la file du sol : à peindre
// AVANT les acteurs, qui les recouvrent. Ce sont ceux de l'image précédente,
// comme les fichiers qui passent derrière les corps.
void PaintClassSpritesBehindActors(EffectsInPlay& effects, ro::render::SpriteRenderer& sprites,
                                   const ro::render::Camera& camera, float aspect_ratio,
                                   const ro::render::SceneFog& fog);

void PaintEffectsUnderStr(EffectsInPlay& effects, EffectNodesInPlay& nodes,
                          const EffectPainters& to);
// Les fichiers `.str`, puis ce qui se peint juste après eux.
void PaintStrEffects(EffectsInPlay& effects, EffectNodesInPlay& nodes,
                     std::span<const ro::render::DrawnEffect> played, const EffectPainters& to);
// Le reste : les quads du monde tardifs, les cases de terrain, ce qui passe
// par-dessus tout, et ce qui se peint en pixels d'écran.
void PaintEffectsLast(EffectsInPlay& effects, EffectNodesInPlay& nodes, const EffectPainters& to);

// LES LUMIÈRES DES EFFETS que cette image vient de peindre, rangées dans `out`
// qu'elle vide d'abord : les fichiers des trois files, ce qu'une méthode nommée
// couche au sol, et les quads du monde des nœuds. Voir docs/adr/0120.
void GatherEffectLights(const EffectsInPlay& effects, const EffectNodesInPlay& nodes,
                        const ro::render::Camera& camera, ro::render::TextureGlowCache& glows,
                        std::vector<ro::render::PointLight>& out);

}  // namespace moonclient
