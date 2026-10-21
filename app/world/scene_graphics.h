#pragma once

#include <cstdint>

#include "ro/render/camera.h"
#include "ro/render/model_clock.h"
#include "ro/render/spin_blur.h"
#include "ro/render/terrain_renderer.h"

#include "options/graphics_options.h"
#include "world/actor_view.h"

namespace moonclient {

// ── CE QUE LA PAGE « GRAPHICS » DEMANDE À UNE SCÈNE DE CARTE ─────────────────
//
// 🔴 UNE SEULE LECTURE POUR LES DEUX SCÈNES : la partie et la ville de fond de
// l'écran de connexion. Le joueur règle sur l'une ce qu'il verra sur l'autre ;
// deux lectures parallèles finiraient par ne plus dire la même chose, et c'est
// l'aperçu qui mentirait.

// LES ÉTAGES DU TERRAIN : la lumière froide, l'ombre et la lumière que la carte
// a peintes.
//
// `lightmap_hidden` éteint ces deux dernières même quand le joueur les veut :
// GreyWorld le demande en partie.
//
// ⚠ LE SOL UNI N'EN EST PAS : c'est un outil de l'établi du staff, et seule la
// partie le pose, par-dessus ce résultat.
[[nodiscard]] ro::render::TerrainStages TerrainStagesOf(const GraphicsOptions& graphics,
                                                        bool lightmap_hidden);

// L'OMBRE DU SOLEIL REMPLACE-T-ELLE CELLE DE LA CARTE : demandée, là où l'ombre
// cuite se peindrait — `stages`, tels que les rend `TerrainStagesOf` —, hors du
// sol uni, et en plein air. Un intérieur n'a pas de soleil, quoi que dise sa
// lumière.
[[nodiscard]] bool SunShadowsApply(const GraphicsOptions& graphics,
                                   const ro::render::TerrainStages& stages, bool indoor);

// LA POSE DU DÉCOR ANIMÉ ENTRE DEUX PAS : glissée sous l'interpolation d'images,
// en crans entiers sinon.
[[nodiscard]] ro::render::ModelClockBlend ModelClockBlendOf(const GraphicsOptions& graphics);

// LA LUMIÈRE DE LA CARTE SUR LES CORPS : le sol éclairé `ground` quand le
// joueur garde les ombres de carte, aucun sinon ; la lumière froide comme sur
// le terrain. `ailment_tints` est nul hors de la partie.
[[nodiscard]] ActorLighting ActorLightingOf(const GraphicsOptions& graphics,
                                            const ro::render::GroundLight* ground,
                                            const AilmentTints* ailment_tints);

// LES FONDUS ET LE GESTE LISIBLE, posés sur la scène avant sa composition :
// cadavre, apparition, sortie de vue, plancher du geste d'une compétence,
// incantation déroulée sur sa barre.
void TimeComposedScene(const GraphicsOptions& graphics, ComposedScene& scene);

// LE FLOU DE MOUVEMENT DE L'IMAGE : la rotation de la caméra depuis l'image
// précédente, mesurée par `meter`, puis le balayage que la page en demande.
//
// ⚠ LA SECOUSSE N'EST PAS UNE ROTATION : `shake_pitch_degrees`, ce qu'elle a posé
// sur l'élévation, se retire avant la mesure. Et seul un glisser — `dragged` —
// fait tourner la caméra continûment : un cran ou une remise ne floute rien.
//
// ⚠ MESURÉE À CHAQUE IMAGE, flou éteint compris : l'allumer ne doit pas compter
// d'un coup toute la rotation faite depuis.
[[nodiscard]] ro::render::SpinBlurFrame SpinBlurOfFrame(ro::render::CameraSpinMeter& meter,
                                                        const GraphicsOptions& graphics,
                                                        const ro::render::Camera& camera,
                                                        float shake_pitch_degrees, bool dragged,
                                                        std::uint32_t now_ms, float aspect_ratio);

}  // namespace moonclient
