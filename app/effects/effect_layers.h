#pragma once

#include <span>

#include "ro/render/camera.h"
#include "ro/render/effect_renderer.h"
#include "ro/render/ground_quad_renderer.h"
#include "ro/render/scene_fog.h"
#include "ro/render/sprite_renderer.h"

#include "effects/effect_actor_change.h"
#include "effects/effect_node_view.h"
#include "effects/effect_paint.h"
#include "effects/effect_view.h"
#include "world/actor_footprints.h"
#include "world/actor_sounds.h"
#include "world/item_drops.h"

// L'ORDRE dans lequel les effets entrent dans l'image.
namespace moonclient {

// Repère ce que le monde vient de lancer, fait battre les deux familles
// d'effets, et peint tout ce qu'elles montrent à cette image.
//
// ⚠ APRÈS LES ACTEURS, jamais avant : un effet ÉCLAIRCIT ce qui est déjà
// peint, et n'aurait rien à éclaircir plus tôt.
//
// Les deux familles se peignent par deux renderers et dans cet ordre-là : les
// effets en FICHIER d'abord, les nœuds procéduraux ensuite. Le reste des règles
// qui tiennent ce passage est sur place, à l'endroit que chacune gouverne.
//
// ⚠ ET UNE CINQUIEME VOIE, celle des CASES DE TERRAIN : la grille de visee ne
// rend aucun quad, elle nomme des cellules que seul le fichier de carte sait
// poser. D'ou le renderer de sol, qui est celui du curseur au sol et non celui
// des effets — ces quads epousent le relief et passent devant lui d'un biais de
// profondeur, ce que la voie des quads du monde ne sait pas faire.
//
// `fog` va à ce que le client peint sous le brouillard : les cases de terrain,
// et les nœuds dont le mot de rendu le demande. Voir `EffectLotIsFogged`.
void DrawEffectLayers(EffectsInPlay& effects, EffectNodesInPlay& nodes, ActorSounds& sounds,
                      ActorFootprints& footprints, ItemDrops& drops, const SoundStage& scene,
                      ro::render::SpriteRenderer& sprites,
                      ro::render::EffectRenderer& effect_renderer,
                      ro::render::GroundQuadRenderer& ground_quads, float aspect_ratio,
                      float viewport_width, float viewport_height, const ro::render::SceneFog& fog);

}  // namespace moonclient
