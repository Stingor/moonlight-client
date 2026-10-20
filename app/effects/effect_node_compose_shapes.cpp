// LA SUITE DU SWITCH DES FORMES — voir `effect_node_compose.cpp`.
//
// ⚠ CE N'EST PAS UNE FRONTIÈRE DE NATURE, c'est le plafond de huit cents
// lignes. La fonction qui compose les lots avait atteint sept cent quinze
// lignes à elle seule — un `switch` de soixante formes —, et c'est exactement
// ce que le plafond vise : une FONCTION, pas un fichier, qu'on ne relit plus.
//
// ⚠ LA DISPENSE DE CATALOGUE NE S'Y APPLIQUE PAS, et c'est délibéré. Un
// catalogue est une liste d'ENTRÉES INDÉPENDANTES au premier niveau ; des
// `case` dans une fonction n'en sont pas. Se donner raison en l'appelant
// catalogue aurait vidé la règle de son sens le jour même où on l'a écrite.
//
// ⚠ ET LA COUVERTURE RESTE GARANTIE, mais il a fallu la chercher. Si les DEUX
// `switch` prennent un `default:`, plus aucun ne contrôle l'exhaustivité et une
// forme neuve passe en silence — c'est justement ce que le lot 8 avait corrigé.
// Celui-ci nomme donc TOUTES les formes : celles qu'il compose, et celles que
// l'autre compose, groupées sans corps à la fin.
//
// UNE FORME NEUVE S'AJOUTE ICI.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "ro/render/effect_aimed_spoke.h"
#include "ro/render/effect_arcing_light.h"
#include "ro/render/effect_bobbing_note.h"
#include "ro/render/effect_break_ring.h"
#include "ro/render/effect_breathing_quad.h"
#include "ro/render/effect_colored_veil.h"
#include "ro/render/effect_concentric_square.h"
#include "ro/render/effect_floor_spinner.h"
#include "ro/render/effect_flying_shard.h"
#include "ro/render/effect_frame_and_bar.h"
#include "ro/render/effect_glow_sprite_seed.h"
#include "ro/render/effect_ground_box.h"
#include "ro/render/effect_ground_gust.h"
#include "ro/render/effect_launch.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_oriented_pair.h"
#include "ro/render/effect_palette_billboard.h"
#include "ro/render/effect_pentagon_trail.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_quad_burst.h"
#include "ro/render/effect_radial_petal.h"
#include "ro/render/effect_rainstorm_cylinder.h"
#include "ro/render/effect_rolling_gust.h"
#include "ro/render/effect_sheathed_bolt.h"
#include "ro/render/effect_shockwave_ray.h"
#include "ro/render/effect_snaking_arc.h"
#include "ro/render/effect_spin_ribbon.h"
#include "ro/render/effect_spinning_box.h"
#include "ro/render/effect_spinning_round.h"
#include "ro/render/effect_spiral_column.h"
#include "ro/render/effect_spiral_stack.h"
#include "ro/render/effect_sprite_fan.h"
#include "ro/render/effect_tapered_sheet.h"
#include "ro/render/effect_tethered_beam.h"
#include "ro/render/effect_tinted_quad.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_figures.h"
#include "ro/render/effect_triple_wall.h"
#include "ro/render/effect_uv_sphere.h"
#include "ro/render/effect_wandering_motes.h"
#include "ro/render/sprite_catalog.h"

#include "effects/effect_node_view.h"

namespace moonclient {

std::vector<ro::render::EffectWorldQuad> ComposeTintedQuadNodeShape(
    const ro::render::PooledEffectNode& pooled) {
  // LES DEUX MARQUES QUE LE CARRÉ TEINTÉ LIT, et qu'il faut prendre sur la
  // PREMIÈRE piste — voir `effect_tinted_quad.h`.
  constexpr std::size_t kMarkOfQuadDrawing = 2;
  constexpr std::size_t kMarkOfQuadSubDrawing = 3;

  // ⚠ LES DEUX ANGLES SE LISENT DANS LES PISTES, pas dans la caméra, et c'est le
  // client qui en décide : son mouvement les y a rangés au dernier tic. Les
  // recalculer ici donnerait le même quad la plupart du temps, et un quad en
  // avance d'un tic le reste du temps — l'écart le plus difficile à voir sur une
  // capture.
  const std::int16_t tour = pooled.tracks.front().style;
  const std::int16_t tilt = pooled.tracks[1].style;
  // ⚠ LA MARQUE VIENT DE LA PREMIÈRE PISTE pour les quatre, et c'est le client
  // qui le veut : il la lit à un déplacement que sa boucle ne décale pas.
  const std::uint8_t mark = pooled.tracks.front().mark[kMarkOfQuadDrawing];
  const std::uint8_t sub_mark = pooled.tracks.front().mark[kMarkOfQuadSubDrawing];

  std::vector<ro::render::EffectWorldQuad> quads;
  for (const ro::render::EffectTrack& track : pooled.tracks) {
    if (const std::optional<ro::render::EffectWorldQuad> world_quad =
            ro::render::ComposeTintedRotatedQuad(track, mark, sub_mark, tour, tilt);
        world_quad.has_value()) {
      quads.push_back(*world_quad);
    }
  }
  return quads;
}

std::vector<ro::render::EffectWorldQuad> ComposeRemainingNodeShape(
    const ro::render::PooledEffectNode& pooled, const ro::render::EffectRingOrientation& facing) {
  std::vector<ro::render::EffectWorldQuad> quads;
  switch (pooled.shape) {
    case ro::render::PooledNodeShape::kSpinningBox:
      // LA BOÎTE QUI TOURNE, six faces par piste vive.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> box_quads =
            ro::render::ComposeSpinningBoxQuads(track, pooled.ground_position);
        quads.insert(quads.end(), box_quads.begin(), box_quads.end());
      }
      break;
    case ro::render::PooledNodeShape::kFloorSpinner:
      // UNE PISTE VIVE, un carré à plat ; son image est celle du nœud.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        if (const std::optional<ro::render::EffectWorldQuad> world_quad =
                ro::render::ComposeFloorSpinnerQuad(track, pooled.ground_position);
            world_quad.has_value()) {
          quads.push_back(*world_quad);
        }
      }
      break;
    case ro::render::PooledNodeShape::kBobbingNote:
      // LA NOTE ET SON HALO, de la première piste ; les angles de caméra sont
      // rangés dans les styles des pistes zéro et un.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> note = ro::render::ComposeBobbingNoteQuads(
            track, pooled.tracks[0].style, pooled.tracks[1].style, pooled.node.wander_seed);
        quads.insert(quads.end(), note.begin(), note.end());
      }
      break;
    case ro::render::PooledNodeShape::kColoredVeil:
      // LE VOILE, teinté par le NŒUD et non par la piste.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        if (const std::optional<ro::render::EffectWorldQuad> voile =
                ro::render::ComposeColoredVeilQuad(track, pooled.tracks[0].style,
                                                   pooled.tracks[1].style, pooled.node.color,
                                                   pooled.ground_position);
            voile.has_value()) {
          quads.push_back(*voile);
        }
      }
      break;
    case ro::render::PooledNodeShape::kUvSphere:
      // LA SPHÈRE UV, le dôme de feu de Magnum Break — voir `effect_uv_sphere.h`.
      // Son lacet la tourne sur son axe ; la caméra n'entre pas dans sa figure.
      quads = ro::render::ComposeEffectNodeUvSphere(pooled.node, pooled.ground_position,
                                                    pooled.node_yaw_degrees);
      break;
    case ro::render::PooledNodeShape::kTwinDisc:
    case ro::render::PooledNodeShape::kFadingTwinDisc:
      // ⚠ LA MÊME FIGURE QUE LE SOUS-TYPE 39, et deux pistes vives sur
      // quatre : la boucle écarte les deux autres d'elle-même. Le 161 y
      // mène aussi ; ce qui le sépare du 28
      // est son mouvement, pas sa figure.
      for (std::size_t track = 0; track < pooled.tracks.size(); ++track) {
        const std::vector<ro::render::EffectWorldQuad> disque = ro::render::ComposeBreathingQuad(
            pooled.tracks[track], pooled.trail_variant, track, pooled.ground_position);
        quads.insert(quads.end(), disque.begin(), disque.end());
      }
      break;
    case ro::render::PooledNodeShape::kExpandingRing:
      // LE RUBAN DE TRAÎNÉE, sur les quatre pistes dans le même lot. Le cap
      // qu'il passe est celui que la sentinelle relit au style de la TROISIÈME
      // piste ; le bâtisseur
      // ne pose ni ce style ni l'ancre, si bien que la sentinelle ne joue pas.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> ribbon =
            ro::render::ComposeEffectTrackRibbon(track, pooled.trail_variant,
                                                 pooled.ground_position,
                                                 ro::render::EffectRibbonTurnOf(pooled.tracks));
        quads.insert(quads.end(), ribbon.begin(), ribbon.end());
      }
      break;
    case ro::render::PooledNodeShape::kThunderScatter:
      // LE CARRÉ TEINTÉ, comme les autres formes qui le partagent :
      // seul le mouvement du sous-type 45 diffère. ⚠ Nommée ici et non dans le
      // premier `switch`, dont le fichier est au plafond : son `default:` l'y
      // renvoie.
      quads = ComposeTintedQuadNodeShape(pooled);
      break;
    case ro::render::PooledNodeShape::kRainstormCylinder:
      // LA JUPE QUI DÉFILE, abaissée de cinq, sur les quatre pistes dans le
      // même lot : elles partagent
      // leur compteur, donc leur image. Voir `effect_rainstorm_cylinder.h`.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> arc =
            ro::render::ComposeRainstormCylinderQuads(
                track, pooled.trail_variant, pooled.resource_paths.size(), pooled.ground_position);
        quads.insert(quads.end(), arc.begin(), arc.end());
      }
      break;
    case ro::render::PooledNodeShape::kCloudRing:
      // LES QUATRE PISTES DANS LE MÊME LOT, comme la jupe : elles
      // partagent leur image et leur variante de palette.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> ribbon =
            ro::render::ComposeCloudRingQuads(track, pooled.trail_variant, pooled.ground_position);
        quads.insert(quads.end(), ribbon.begin(), ribbon.end());
      }
      break;
    case ro::render::PooledNodeShape::kWanderingMote:
      // LES QUATRE PISTES DANS LE MÊME LOT : chacune rend QUATRE carrés,
      // sans aucune garde de hauteur.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> lucioles =
            ro::render::ComposeWanderingMoteQuads(track, pooled.tracks[0].style,
                                                  pooled.tracks[1].style, pooled.ground_position);
        quads.insert(quads.end(), lucioles.begin(), lucioles.end());
      }
      break;
    case ro::render::PooledNodeShape::kSpinningRound:
      // LES QUATRE PISTES DANS LE MÊME LOT : chacune rend QUATRE carrés.
      //
      // ⚠ LES DEUX ANGLES DE CAMÉRA VIENNENT DES STYLES DES PISTES ZÉRO
      // ET UN, où le mouvement les a rangés — comme la rafale.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> carres =
            ro::render::ComposeSpinningRoundQuads(track, pooled.tracks[0].style,
                                                  pooled.tracks[1].style, pooled.ground_position);
        quads.insert(quads.end(), carres.begin(), carres.end());
      }
      break;
    case ro::render::PooledNodeShape::kSnakingArc:
      // LES QUATRE PISTES DANS LE MÊME LOT : chacune rend jusqu'à
      // soixante quads — vingt segments de trois faces —, et moins dès
      // que son alpha s'épuise.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> faces =
            ro::render::ComposeSnakingArcQuads(track);
        quads.insert(quads.end(), faces.begin(), faces.end());
      }
      break;
    case ro::render::PooledNodeShape::kSpinRibbon:
    case ro::render::PooledNodeShape::kRisingSpinRibbon:
      // DEUX FORMES, UNE FIGURE : seuls leurs mouvements diffèrent.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> bands =
            ro::render::ComposeSpinRibbonQuads(track, pooled.trail_variant, pooled.ground_position);
        quads.insert(quads.end(), bands.begin(), bands.end());
      }
      break;
    case ro::render::PooledNodeShape::kTetheredBeam:
      // UNE SEULE PISTE VIVE, et elle rend VINGT quads — dix faces, deux
      // fois. Les trois autres rendent un lot vide d'elles-mêmes.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> faces =
            ro::render::ComposeTetheredBeamQuads(track);
        quads.insert(quads.end(), faces.begin(), faces.end());
      }
      break;
    case ro::render::PooledNodeShape::kRollingGust:
      // LA ROUE DE VENT : un carré DEBOUT par piste, ou rien.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        if (const std::optional<ro::render::EffectWorldQuad> wheel =
                ro::render::ComposeRollingGustQuad(track);
            wheel.has_value()) {
          quads.push_back(*wheel);
        }
      }
      break;
    case ro::render::PooledNodeShape::kGroundGust:
    // ⚠ LES BOURRASQUES QUI POURSUIVENT ONT LA MÊME FIGURE : même
    // dessinateur, seul le mouvement les sépare.
    case ro::render::PooledNodeShape::kHomingGustSteered:
    case ro::render::PooledNodeShape::kHomingGustSpread:
      // LES QUATRE PISTES DANS LE MÊME LOT : la meneuse et ses trois
      // échos partagent leur image, et chacune rend UN carré — ou rien.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        if (const std::optional<ro::render::EffectWorldQuad> world_quad =
                ro::render::ComposeGroundGustQuad(track);
            world_quad.has_value()) {
          quads.push_back(*world_quad);
        }
      }
      break;
    case ro::render::PooledNodeShape::kGroundBox:
      // LES QUATRE PISTES DANS LE MÊME LOT, dont une seule est vive : les
      // trois autres rendent un lot vide d'elles-mêmes.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> box_quads =
            ro::render::ComposeGroundBoxQuads(track, pooled.ground_position);
        quads.insert(quads.end(), box_quads.begin(), box_quads.end());
      }
      break;
    case ro::render::PooledNodeShape::kFrameAndBar:
      // ⚠ DEUX FIGURES DANS LE MÊME LOT, et c'est le RANG de la piste qui
      // dit laquelle : les deux premières dressent le cadre, les deux
      // dernières font courir la barre. Les traiter pareil peindrait
      // quatre cadres ou quatre barres.
      for (std::size_t etage = 0; etage < pooled.tracks.size(); ++etage) {
        const std::vector<ro::render::EffectWorldQuad> part =
            etage < ro::render::kFrameAndBarFrameTracks
                ? ro::render::ComposeFrameWallQuads(pooled.tracks[etage], pooled.ground_position)
                : ro::render::ComposePerimeterBarQuads(pooled.tracks[etage],
                                                       pooled.ground_position);
        quads.insert(quads.end(), part.begin(), part.end());
      }
      break;
    case ro::render::PooledNodeShape::kSheathedBolt:
      // LES QUATRE PISTES DANS LE MÊME LOT : elles partagent leur image, et
      // chacune rend DIX quads — cinq pour l'âme, cinq pour la gaine.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> trait =
            ro::render::ComposeSheathedBoltQuads(track);
        quads.insert(quads.end(), trait.begin(), trait.end());
      }
      break;
    case ro::render::PooledNodeShape::kFlyingShard:
      // LES QUATRE PISTES DANS LE MÊME LOT : elles partagent leur image, et
      // chacune rend UN barreau — ou rien.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        if (const std::optional<ro::render::EffectWorldQuad> shard =
                ro::render::ComposeFlyingShardQuad(track, pooled.ground_position);
            shard.has_value()) {
          quads.push_back(*shard);
        }
      }
      break;
    case ro::render::PooledNodeShape::kQuadBurst:
      // LES QUATRE PISTES DANS LE MÊME LOT : elles partagent leur image, et
      // chacune rend jusqu'à HUIT quads — quatre particules, dont deux
      // marques peignent la leur deux fois.
      //
      // ⚠ LES DEUX ANGLES DE CAMÉRA VIENNENT DES STYLES DES PISTES ZÉRO ET
      // UN, où le mouvement les a rangés — comme le carré concentrique.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> rafale = ro::render::ComposeQuadBurstQuads(
            track, pooled.tracks[0].style, pooled.tracks[1].style, pooled.ground_position);
        quads.insert(quads.end(), rafale.begin(), rafale.end());
      }
      break;
    case ro::render::PooledNodeShape::kRadialPetal:
      // LES QUATRE PISTES DANS LE MÊME LOT : elles partagent leur image, et
      // chacune rend UN carré. Les deux angles de panneau viennent
      // des styles des pistes zéro et un, où le mouvement les a rangés.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        if (const std::optional<ro::render::EffectWorldQuad> petale =
                ro::render::ComposeRadialPetalQuad(track, pooled.tracks[0].style,
                                                   pooled.tracks[1].style, pooled.ground_position);
            petale.has_value()) {
          quads.push_back(*petale);
        }
      }
      break;
    case ro::render::PooledNodeShape::kArcingLight:
      // UNE IMAGE, UN CARRÉ PAR PISTE ALLUMÉE — la première seule l'est. Les deux
      // angles de panneau viennent des styles des pistes zéro et un.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        if (const std::optional<ro::render::EffectWorldQuad> lueur =
                ro::render::ComposeArcingLightQuad(track, pooled.tracks[0].style,
                                                   pooled.tracks[1].style, pooled.ground_position);
            lueur.has_value()) {
          quads.push_back(*lueur);
        }
      }
      break;
    case ro::render::PooledNodeShape::kAimedSpokes:
      // UNE IMAGE, DEUX RAYONS PAR PISTE ALLUMÉE, chacun en deux passages qui ne
      // se mélangent pas de la même façon : chaque quad porte le sien.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        ro::render::ComposeAimedSpokeQuads(track, pooled.ground_position, quads);
      }
      break;
    case ro::render::PooledNodeShape::kShockwaveRays:
      // UNE IMAGE, DEUX TRIANGLES PAR PISTE. Les deux angles de panneau viennent
      // des styles des pistes zéro et un, où le mouvement les a rangés.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        ro::render::ComposeShockwaveRayQuads(track, pooled.tracks[0].style, pooled.tracks[1].style,
                                             pooled.ground_position, quads);
      }
      break;
    case ro::render::PooledNodeShape::kBreakRings:
      // UNE IMAGE, UNE COURONNE PAR PISTE ALLUMÉE, sous la variante du nœud.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        ro::render::ComposeBreakRingQuads(track, pooled.trail_variant, pooled.ground_position,
                                          quads);
      }
      break;
    case ro::render::PooledNodeShape::kTaperedSheets:
      // UNE IMAGE, UN RECTANGLE PAR PISTE, aux coins ABSOLUS : l'ancre de chaque
      // piste porte sa propre hauteur.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        if (const std::optional<ro::render::EffectWorldQuad> feuille =
                ro::render::ComposeTaperedSheetQuad(track, pooled.tracks[0].style,
                                                    pooled.tracks[1].style);
            feuille.has_value()) {
          quads.push_back(*feuille);
        }
      }
      break;
    case ro::render::PooledNodeShape::kBillboardSwarm:
      // ⚠ LES DEUX ANGLES DE ROTATION VIVENT DANS LES STYLES DES PISTES
      // ZÉRO ET UN, où le mouvement les a rangés — et le client les y relit
      // de même. Les recalculer ici donnerait un quad d'un tic de retard sur
      // sa propre piste.
      //
      // ⚠ ET CHAQUE PISTE REND DEUX POSITIONS de un ou deux quads : une
      // autour de sa queue, une autour de son ancre. Une piste éteinte n'a
      // ni alpha ni rayon, et ne rend donc rien.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        for (const ro::render::EffectOrientedPairRequest& request :
             ro::render::EffectOrientedPairRequestsOf(track, pooled.tracks[0].style,
                                                      pooled.tracks[1].style)) {
          const std::vector<ro::render::EffectWorldQuad> pair =
              ro::render::ComposeOrientedPairQuads(request, pooled.ground_position);
          quads.insert(quads.end(), pair.begin(), pair.end());
        }
      }
      break;
    case ro::render::PooledNodeShape::kTripleWall:
      // LES QUATRE PISTES DANS LE MÊME LOT : elles partagent leur image.
      //
      // ⚠ ET CHACUNE REND TROIS QUADS, pas un : la figure peint son mur
      // trois fois pour en épaissir le bord.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> murs =
            ro::render::ComposeTripleWallQuads(track, pooled.ground_position);
        quads.insert(quads.end(), murs.begin(), murs.end());
      }
      break;
    case ro::render::PooledNodeShape::kConcentricSquare:
      // ⚠ DEUX PISTES, PAS QUATRE : la figure lit tout dans la première,
      // sauf son second angle de rotation, qu'elle prend dans la seconde.
      quads = ro::render::ComposeConcentricSquareQuads(
          ro::render::EffectConcentricSquareRequestOf(pooled.tracks[0], pooled.tracks[1], false),
          pooled.ground_position);
      break;
    case ro::render::PooledNodeShape::kSpiralColumn:
      // LES QUATRE COLONNES DANS LE MÊME LOT : elles partagent leur image.
      //
      // ⚠ ET CHACUNE REND HUIT QUADS, pas quatre : la figure sort par DEUX
      // voies, et les deux passent par le même lot puisqu'elles peignent la
      // même image.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> column =
            ro::render::ComposeSpiralColumnQuads(track, pooled.trail_variant,
                                                 pooled.ground_position);
        quads.insert(quads.end(), column.begin(), column.end());
      }
      break;
    case ro::render::PooledNodeShape::kSpiralStack:
      // UNE SEULE PISTE EST VIVE, mais on les parcourt toutes : la figure
      // rend un lot vide pour une piste éteinte, et compter sur le rang
      // lierait la fenêtre à un détail du bâtisseur.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> pile =
            ro::render::ComposeSpiralStackQuads(track, pooled.ground_position);
        quads.insert(quads.end(), pile.begin(), pile.end());
      }
      break;
    case ro::render::PooledNodeShape::kPaletteBillboard:
      // LES QUATRE ÉCLATS DANS LE MÊME LOT : ils partagent leur image.
      //
      // ⚠ ET LA FIGURE EN ÉCARTE PLUS D'UN AU DÉBUT, parce que trois pistes
      // sur quatre sont retardées. Un lot vide ici n'est pas une figure
      // manquante mais une spirale qui s'ouvre.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        if (const std::optional<ro::render::EffectWorldQuad> shard =
                ro::render::ComposePaletteBillboardQuad(track, pooled.ground_position);
            shard.has_value()) {
          quads.push_back(*shard);
        }
      }
      break;
    case ro::render::PooledNodeShape::kPentagonTrail:
      // LES QUATRE COUCHES DANS LE MÊME LOT : elles partagent leur image, et
      // les séparer paierait quatre lots de rendu là où le client en paie un.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> tube =
            ro::render::ComposePentagonTrailQuads(track);
        quads.insert(quads.end(), tube.begin(), tube.end());
      }
      break;
    case ro::render::PooledNodeShape::kSprite:
    case ro::render::PooledNodeShape::kFadingSprite:
    case ro::render::PooledNodeShape::kRingBand:
      // 🔴 L'ORIENTATION DU NŒUD, ET NON L'IDENTITÉ. Elle se calcule pour tout
      // nœud dans l'autre fichier, et n'arrivait jusqu'ici à AUCUNE figure : un
      // anneau que le client dresse d'un quart de tour restait couché à plat,
      // et se voyait comme une ellipse au sol au lieu d'un cerceau debout.
      //
      // ⚠ UN NŒUD QUI NE PORTE PAS CES DEUX ANGLES REÇOIT L'IDENTITÉ, qui EST
      // l'orientation couchée : les sprites et les fondus qui partagent ce cas
      // ne changent pas de figure pour autant.
      // ⚠ ET L'ANCRE AVANCE LE LONG DE L'AXE DU NŒUD. Le composeur est seul à
      // connaître cet axe — c'est la deuxième colonne de l'orientation, l'image
      // de la hauteur locale —, et le mouvement ne lui donne qu'une LONGUEUR
      // cumulée. Voir `EffectNode::translates_along_axis`.
      //
      // ⚠ UN SPRITE N'AVANCE PAS POUR AUTANT : il partage ce cas mais pas le
      // mouvement, et sa longueur cumulée reste nulle.
      quads = ro::render::ComposeEffectNodeRingBand(
          pooled.node, pooled.ground_position + facing[1] * pooled.node.axial_translation, facing);
      break;
    // LES RUBANS : trois formes pour UNE figure, `ComposeEffectTrackRibbon`.
    // Elles ne diffèrent que par le CAP qu'elles lui donnent, et c'est pourquoi
    // elles voyagent ensemble.
    case ro::render::PooledNodeShape::kHeartRings:
      // ⚠ LE CAP N'EST PAS CELUI DE LA PISTE, ici : le mouvement des groupes
      // radiaux le range dans le style de la TROISIÈME, et la figure l'y relit.
      // Prendre l'azimut de la piste courante ferait tourner chaque anneau d'un
      // cap différent — et le cœur se disloquerait.
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> ribbon =
            ro::render::ComposeEffectTrackRibbon(track, pooled.trail_variant,
                                                 pooled.ground_position,
                                                 ro::render::EffectRibbonTurnOf(pooled.tracks));
        quads.insert(quads.end(), ribbon.begin(), ribbon.end());
      }
      break;
    case ro::render::PooledNodeShape::kRibbonWave:
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> ribbon =
            ro::render::ComposeEffectTrackRibbon(track, pooled.trail_variant,
                                                 pooled.ground_position, track.azimuth_degrees);
        quads.insert(quads.end(), ribbon.begin(), ribbon.end());
      }
      break;
    // 🔴 L'ANNEAU QUI TOURNE, sous-type 50, SE DESSINE EN JUPE : la table de
    // répartition du dessin le mène au même cas que le sous-type 30, et non au
    // ruban des sous-types 36 et 110. Vingt segments sur tout son balayage, et
    // son onde lue en entier — les effets 280, 285, 783 et 784. UNE piste vive
    // sur quatre : la jupe écarte les trois autres d'elle-même.
    case ro::render::PooledNodeShape::kTurningRing:
      for (const ro::render::EffectTrack& track : pooled.tracks) {
        const std::vector<ro::render::EffectWorldQuad> skirt = ro::render::ComposeEffectTrackSkirt(
            track, pooled.trail_variant, pooled.ground_position);
        quads.insert(quads.end(), skirt.begin(), skirt.end());
      }
      break;
    // LES FORMES QUE L'AUTRE FICHIER COMPOSE. Elles sont nommées une à une,
    // et c'est ce qui garde le contrôle d'exhaustivité : un `default:` les
    // avalerait, et une forme neuve avec elles.
    case ro::render::PooledNodeShape::kTurningArch:
    case ro::render::PooledNodeShape::kSpiralParticles:
    case ro::render::PooledNodeShape::kRadialSpreadParticles:
    case ro::render::PooledNodeShape::kOscillatingRadiusParticles:
    case ro::render::PooledNodeShape::kGrandCrossRibbon:
    case ro::render::PooledNodeShape::kGrandCrossFrame:
    case ro::render::PooledNodeShape::kHealSparkle:
    case ro::render::PooledNodeShape::kRisingQuads:
    // Les sprites, que `ComposeNodeAsSprite` compose en image d'ACT.
    case ro::render::PooledNodeShape::kGlowSprite:
    case ro::render::PooledNodeShape::kSpriteFan:
    case ro::render::PooledNodeShape::kWarpRings:
    case ro::render::PooledNodeShape::kPulsingRings:
    case ro::render::PooledNodeShape::kRadialParticleGroups:
    case ro::render::PooledNodeShape::kTeleportRings:
    case ro::render::PooledNodeShape::kDualModeSprayRings:
    case ro::render::PooledNodeShape::kParticleConeRings:
    case ro::render::PooledNodeShape::kRingQuartet:
    case ro::render::PooledNodeShape::kRingStarburst:
    case ro::render::PooledNodeShape::kRingTracks:
    case ro::render::PooledNodeShape::kRisingParticles:
    case ro::render::PooledNodeShape::kArcRibbons:
    case ro::render::PooledNodeShape::kFallingItem:
    case ro::render::PooledNodeShape::kScatterField:
    case ro::render::PooledNodeShape::kRisingSparks:
    case ro::render::PooledNodeShape::kHomingTrail:
    case ro::render::PooledNodeShape::kThrownItem:
    case ro::render::PooledNodeShape::kTintedQuads:
    case ro::render::PooledNodeShape::kSwirlTrail:
    case ro::render::PooledNodeShape::kDriftingIcons:
    case ro::render::PooledNodeShape::kSpiralFan:
    case ro::render::PooledNodeShape::kCameraFan:
    case ro::render::PooledNodeShape::kRingCylinder:
    case ro::render::PooledNodeShape::kCrossedPlanes:
    case ro::render::PooledNodeShape::kOrientedQuad:
    case ro::render::PooledNodeShape::kScrollingBand:
    case ro::render::PooledNodeShape::kDarkVeil:
    case ro::render::PooledNodeShape::kTypedRibbon:
    case ro::render::PooledNodeShape::kSphereShell:
    case ro::render::PooledNodeShape::kThunderFreeze:
    case ro::render::PooledNodeShape::kTetraVortex:
    case ro::render::PooledNodeShape::kLayeredAura:
    case ro::render::PooledNodeShape::kPlainAura:
    case ro::render::PooledNodeShape::kGlidingSlash:
    case ro::render::PooledNodeShape::kAimedGlidingSlash:
    case ro::render::PooledNodeShape::kCrossSpikes:
    case ro::render::PooledNodeShape::kGroundRibbon:
    case ro::render::PooledNodeShape::kCyclingSkirt:
    case ro::render::PooledNodeShape::kGridPlane:
    case ro::render::PooledNodeShape::kAuraColumn:
    case ro::render::PooledNodeShape::kRainParticle:
    case ro::render::PooledNodeShape::kDoubleRibbon:
    case ro::render::PooledNodeShape::kFadingStreak:
    case ro::render::PooledNodeShape::kRainbowTrail:
    case ro::render::PooledNodeShape::kSpiralTrail:
    case ro::render::PooledNodeShape::kSpikeBeam:
    case ro::render::PooledNodeShape::kPieWedge:
    case ro::render::PooledNodeShape::kConeWedge:
    case ro::render::PooledNodeShape::kBreathingQuad:
    case ro::render::PooledNodeShape::kFootprint:
    case ro::render::PooledNodeShape::kGroundDisc:
    case ro::render::PooledNodeShape::kGrowingSprite:
    case ro::render::PooledNodeShape::kSparkTube:
    case ro::render::PooledNodeShape::kGroundCurtain:
    case ro::render::PooledNodeShape::kDriftingMote:
    case ro::render::PooledNodeShape::kDriftingTriPlane:
    case ro::render::PooledNodeShape::kFlyingTriPlane:
    case ro::render::PooledNodeShape::kHomingSpark:
    case ro::render::PooledNodeShape::kLightningFan:
    case ro::render::PooledNodeShape::kElectricArc:
    case ro::render::PooledNodeShape::kAnchoredSkirt:
      // ⚠ CES DEUX-CI SONT PEINTS PAR LE PREMIER `switch` (effect_node_compose.cpp) :
      // les nommer ici garde la couverture entière, sans dire qu'ils se peignent ici.
    // ⚠ ET LA GRILLE DE VISEE NE PASSE NI PAR L'UN NI PAR L'AUTRE : elle ne
    // rend pas de quads du tout, mais des CASES de terrain, et elle sort de la
    // troupe avant ce switch. Elle est nommee ici pour que la couverture reste
    // entiere — voir `EffectNodesInPlay::ComposeGroundCellsOf`.
    case ro::render::PooledNodeShape::kMagicTargetGrid:
    // ⚠ ET LES DEUX FORMES QUI NE PEIGNENT RIEN : elles font naître d'autres
    // effets, et la troupe les écarte avant ce switch — voir
    // `ro::render::EffectNodeShapeOnlySpawns`.
    case ro::render::PooledNodeShape::kSpiralEmitter:
    case ro::render::PooledNodeShape::kWalkingSower:
      break;
  }
  return quads;
}

bool EffectNodesInPlay::ComposeGroundCellsOf(const ro::render::PooledEffectNode& pooled,
                                             std::size_t& sans_texture, std::size_t& sans_quad) {
  const auto texture =
      pooled.resource_paths.empty() ? textures_.end() : textures_.find(pooled.resource_paths[0]);
  if (texture == textures_.end() || texture->second.empty()) {
    ++sans_texture;
    return false;
  }
  std::vector<ro::render::EffectGroundCellPaint> cases;
  // LES QUATRE PISTES DANS LE MÊME LOT, dont une seule est vive : les trois
  // autres rendent un lot vide d'elles-mêmes.
  for (const ro::render::EffectTrack& track : pooled.tracks) {
    const std::vector<ro::render::EffectGroundCellPaint> grid =
        ro::render::ComposeMagicTargetGridCells(track);
    cases.insert(cases.end(), grid.begin(), grid.end());
  }
  if (cases.empty()) {
    ++sans_quad;
    return false;
  }
  ground_cell_lots_.push_back(std::move(cases));
  // Le chemin et l'image viennent de la table, dont les entrées ne bougent
  // pas ; les cases, elles, se rattachent une fois la liste finie de grandir.
  ground_cells_.push_back(
      DrawnGroundCells{{}, pooled.ground_position, &texture->first, &texture->second});
  return true;
}

// UN NŒUD QUI SE DESSINE EN SPRITE : son image, ses sources, et où il se pose.
//
// ⚠ VENU DE `effect_node_compose.cpp` AU PLAFOND DE HUIT CENTS LIGNES, et la
// couture est celle que le reste de ce fichier suit déjà : tout ce qui est
// resté là-bas compose des QUADS DU MONDE, celui-ci compose une image d'ACT.
// Deux dessinateurs, deux repères, deux voies de sortie.
void EffectNodesInPlay::ComposeNodeAsSprite(const ro::render::PooledEffectNode& pooled,
                                            const ro::render::Camera& camera) {
  const auto source =
      pooled.resource_paths.empty() ? sprites_.end() : sprites_.find(pooled.resource_paths[0]);
  if (source == sprites_.end()) {
    return;
  }
  // 🔴 L'IMAGE SE COUCHE LE LONG DE SON VOL, et l'angle se calcule ICI parce
  // qu'il dépend de la CAMÉRA. Voir `EffectNodePattern::turns_with_flight`.
  ro::render::EffectNode live = pooled.node;
  if (pooled.turns_with_flight && pooled.launch_direction != glm::vec3{0.0F, 0.0F, 0.0F}) {
    live.draws_rotated = true;
    live.spin_degrees = ro::render::ScreenRotationOfDirection(
        glm::normalize(pooled.launch_direction), ro::render::MakeBillboardBasis(camera));
  }
  // 🔴 UN PROJECTILE D'ACTEUR TOURNE DE LA FACE DE SON VOL vue de la caméra, du
  // lacet seul, et le client la recompose à chaque mise à jour : une caméra qui
  // tourne pendant le vol tourne la flèche avec elle. Voir
  // `PooledEffectNode::turns_with_flight_heading`.
  if (pooled.turns_with_flight_heading && pooled.launch_direction != glm::vec3{0.0F, 0.0F, 0.0F}) {
    live.draws_rotated = true;
    // L'angle RÉEL pour la munition en sprite d'un monstre, TRONQUÉ au degré
    // pour la flèche : voir `EffectNode::spins_by_exact_degrees`. Et chacun
    // tourné de ce que le dessin du projectile demande en plus, ramené sur le
    // tour pour l'angle au degré : voir
    // `PooledEffectNode::flight_heading_turn_degrees`.
    constexpr std::int32_t kFullTurnDegrees = 360;
    const std::int32_t turn_degrees = pooled.flight_heading_turn_degrees;
    live.spin_degrees =
        pooled.node.spins_by_exact_degrees
            ? ro::render::EffectProjectileScreenAngle(pooled.launch_direction, camera.yaw_degrees) +
                  static_cast<float>(turn_degrees)
            : static_cast<float>((ro::render::EffectProjectileScreenAngleDegrees(
                                      pooled.launch_direction, camera.yaw_degrees) +
                                  turn_degrees) %
                                 kFullTurnDegrees);
  }
  ro::render::ComposedActor actor;
  actor.quads =
      ro::render::ComposeEffectNodeSprite(live, source->second.sprite, source->second.animation);
  if (actor.quads.empty()) {
    return;
  }
  actor.sources.reserve(actor.quads.size());
  for (const ro::render::SpriteQuad& quad : actor.quads) {
    actor.sources.push_back(ro::render::SpriteImageKey{&source->second.sprite, nullptr,
                                                       quad.section, quad.image_index});
  }
  // UN PROJECTILE PORTE L'OMBRE D'UN ACTEUR, au sol sous son vol : voir
  // `ProjectileCasters`.
  if (pooled.casts_shadow) {
    if (!projectile_casters_.round_shadow_known()) {
      projectile_casters_.ComposeRoundShadow(
          LoadedSprite(std::string(ro::render::kShadowSpritePath)));
    }
    const auto& ground_height = pool_.world().ground_height;
    projectile_casters_.Add(actor, pooled.ground_position,
                            ground_height ? std::optional<float>(ground_height(
                                                pooled.ground_position.x, pooled.ground_position.z))
                                          : std::nullopt);
  }
  composed_.push_back(std::move(actor));
  // Chaque pose emporte le mot de rendu de son nœud : voir `SpriteBatches`.
  //
  // 🔴 LE SPRITE ÉCLATANT SE PEINT DEUX FOIS : la seconde, chaque calque élargi
  // d'un pixel d'écran de chaque côté et toujours AJOUTÉ, quel que soit le
  // mélange de la première. C'est son halo.
  const bool with_halo = pooled.shape == ro::render::PooledNodeShape::kGlowSprite;
  const auto place = [&](ComposedPlacement pose) {
    pose.render_word = pooled.node.render_word;
    placements_.push_back(pose);
    if (with_halo) {
      pose.render_word = ro::render::kEffectGlowSpriteHaloRenderWord;
      pose.grow_screen_pixels = ro::render::kEffectGlowSpriteHaloScreenPixels;
      placements_.push_back(pose);
    }
  };

  // ⚠ L'ÉVENTAIL CROISE SES CRANS AVEC SES QUATRE PISTES, seul de la
  // famille : une pose par piste vive et par cran, à la somme des deux
  // points. Le traiter comme les autres sprites n'en montrerait qu'un.
  //
  // 🔴 ET CHAQUE POSE A SON HALO, peint juste après elle : la même image élargie
  // de quatre pixels d'écran, à demi-alpha et dans sa propre teinte.
  if (pooled.shape == ro::render::PooledNodeShape::kSpriteFan) {
    const auto placement = [&](const ro::render::EffectTrailStep& pose) {
      return ComposedPlacement{composed_.size() - 1,
                               pose.ground_position,
                               {static_cast<std::uint8_t>(std::clamp(pose.color[0], 0.0F, 255.0F)),
                                static_cast<std::uint8_t>(std::clamp(pose.color[1], 0.0F, 255.0F)),
                                static_cast<std::uint8_t>(std::clamp(pose.color[2], 0.0F, 255.0F)),
                                static_cast<std::uint8_t>(std::clamp(pose.alpha, 0.0F, 255.0F))},
                               pose.sprite_scale};
    };
    const std::vector<ro::render::EffectTrailStep> poses =
        ro::render::ComposeSpriteFanPoses(pooled.tracks, pooled.trail, pooled.ground_position);
    const std::vector<ro::render::EffectTrailStep> halos =
        ro::render::ComposeSpriteFanHaloPoses(pooled.tracks, pooled.trail, pooled.ground_position);
    for (std::size_t rang = 0; rang < poses.size(); ++rang) {
      place(placement(poses[rang]));
      ComposedPlacement halo = placement(halos[rang]);
      halo.render_word = pooled.node.render_word;
      halo.grow_screen_pixels = ro::render::kSpriteFanHaloScreenPixels;
      placements_.push_back(halo);
    }
    return;
  }

  // UN SPRITE SANS TRAÎNÉE SE DESSINE UNE FOIS, au point de son nœud. Avec
  // traînée, il se dessine une fois PAR CRAN — même acteur, points et alphas
  // différents. C'est le seul endroit où les deux se distinguent.
  if (pooled.trail.empty()) {
    place(ComposedPlacement{composed_.size() - 1, pooled.ground_position});
    return;
  }
  // ⚠ L'ÉCHELLE D'UN CRAN EST ABSOLUE, celle d'une pose un FACTEUR : voir
  // `EffectTrailStepScaleFactor`, sans quoi la tête se dessine au carré.
  for (const ro::render::EffectTrailStep& notch : pooled.trail) {
    place(ComposedPlacement{composed_.size() - 1,
                            notch.ground_position,
                            {static_cast<std::uint8_t>(std::clamp(notch.color[0], 0.0F, 255.0F)),
                             static_cast<std::uint8_t>(std::clamp(notch.color[1], 0.0F, 255.0F)),
                             static_cast<std::uint8_t>(std::clamp(notch.color[2], 0.0F, 255.0F)),
                             static_cast<std::uint8_t>(std::clamp(notch.alpha, 0.0F, 255.0F))},
                            ro::render::EffectTrailStepScaleFactor(notch, pooled.node)});
  }
}

}  // namespace moonclient
