#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "ro/core/log.h"
#include "ro/render/effect_arc_ribbon.h"
#include "ro/render/effect_aura_column.h"
#include "ro/render/effect_breathing_quad.h"
#include "ro/render/effect_concentric_square.h"
#include "ro/render/effect_cone_wedge.h"
#include "ro/render/effect_cross_spikes.h"
#include "ro/render/effect_dark_veil.h"
#include "ro/render/effect_double_ribbon.h"
#include "ro/render/effect_drifting_mote.h"
#include "ro/render/effect_electric_arc.h"
#include "ro/render/effect_fading_streak.h"
#include "ro/render/effect_flying_shard.h"
#include "ro/render/effect_frame_and_bar.h"
#include "ro/render/effect_gliding_slash.h"
#include "ro/render/effect_grand_cross.h"
#include "ro/render/effect_grid_plane.h"
#include "ro/render/effect_ground_box.h"
#include "ro/render/effect_ground_curtain.h"
#include "ro/render/effect_ground_gust.h"
#include "ro/render/effect_ground_ribbon.h"
#include "ro/render/effect_homing_spark.h"
#include "ro/render/effect_job_aura.h"
#include "ro/render/effect_lightning_fan.h"
#include "ro/render/effect_node_world.h"
#include "ro/render/effect_oriented_pair.h"
#include "ro/render/effect_palette_billboard.h"
#include "ro/render/effect_particle_figures.h"
#include "ro/render/effect_pentagon_trail.h"
#include "ro/render/effect_pie_wedge.h"
#include "ro/render/effect_quad_burst.h"
#include "ro/render/effect_rain_particle.h"
#include "ro/render/effect_rainbow_trail.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_ring_flight.h"
#include "ro/render/effect_scrolling_band.h"
#include "ro/render/effect_sheathed_bolt.h"
#include "ro/render/effect_snaking_arc.h"
#include "ro/render/effect_spark_tube.h"
#include "ro/render/effect_sphere_shell.h"
#include "ro/render/effect_spike_beam.h"
#include "ro/render/effect_spin_ribbon.h"
#include "ro/render/effect_spinning_round.h"
#include "ro/render/effect_spiral_column.h"
#include "ro/render/effect_spiral_fan.h"
#include "ro/render/effect_spiral_stack.h"
#include "ro/render/effect_spiral_trail.h"
#include "ro/render/effect_tethered_beam.h"
#include "ro/render/effect_tinted_quad.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_figures.h"
#include "ro/render/effect_tri_plane.h"
#include "ro/render/effect_triple_wall.h"
#include "ro/render/effect_typed_ribbon.h"
#include "ro/render/effect_wandering_motes.h"

#include "effects/effect_node_view.h"

// CE QUE LA TROUPE MONTRE à cette image, et dans quel ordre.
//
// La moitié SEMIS de `EffectNodesInPlay` vit dans `effect_node_view.cpp` : elle
// décide QUOI et QUAND poser. Celle-ci ne décide rien ; elle prend ce qui vit
// et le rend en sprites et en quads du monde.
//
// ⚠ LES DEUX MOITIÉS NE PARTAGENT QUE LA CLASSE, et c'est ce qui permet de les
// séparer : aucune constante n'est employée des deux côtés. Le fichier unique
// passait le plafond de huit cents lignes le jour où l'horloge des effets y est
// entrée.
namespace moonclient {

// LA SUITE DU SWITCH DES FORMES — voir `effect_node_compose_shapes.cpp`.
//
// ⚠ HORS DU NAMESPACE ANONYME, et ce n'est pas un détail de rangement : dedans
// elle prendrait une liaison INTERNE, et sa définition, qui vit dans une autre
// unité, ne la satisferait jamais. Le compilateur le dit — C5046 —, et il a
// raison de le traiter en erreur.
[[nodiscard]] std::vector<ro::render::EffectWorldQuad> ComposeRemainingNodeShape(
    const ro::render::PooledEffectNode& pooled, const ro::render::EffectRingOrientation& facing);

// LE CARRÉ TEINTÉ ET LES QUATRE FORMES QUI PARTAGENT SA FIGURE — même raison,
// même endroit. Ce fichier était à huit cents lignes PILE, et chaque forme qui
// rejoint le groupe y ajoute deux lignes : la figure déménage pour que la
// prochaine puisse entrer.
[[nodiscard]] std::vector<ro::render::EffectWorldQuad> ComposeTintedQuadNodeShape(
    const ro::render::PooledEffectNode& pooled);

namespace {

// LES DEUX MARQUES QUE LE CARRÉ TEINTÉ LIT, et que la fenêtre doit prendre sur
// la PREMIÈRE piste — voir `effect_tinted_quad.h`.
constexpr std::size_t kMarkOfQuadDrawing = 2;
constexpr std::size_t kMarkOfQuadSubDrawing = 3;

// ⚠ ET LA PREMIÈRE MARQUE EST UN INDEX DE TEXTURE, piste par piste. Le client
// passe au traceur de quads la marque de la piste QU'IL DESSINE — un déplacement
// que sa boucle décale de cent quatre-vingts, à la différence des deux autres —,
// et le mouvement l'allume à deux tics propres à chaque piste. C'est ainsi qu'une
// étincelle ÉCLATE : elle change de texture, le temps de deux tics.
constexpr std::size_t kMarkOfSparkTexture = 0;

// Combien d'images de suite une troupe vivante peut ne rien composer avant que
// ce soit une panne. Une seule est normale — un nœud naît sans aire —, trente
// sont une demi-seconde d'écran vide.
constexpr std::uint32_t kSilentFramesBeforeWarning = 30;

}  // namespace

std::span<const ro::render::DrawnActor> EffectNodesInPlay::Drawn(std::uint32_t now_ms,
                                                                 const ro::render::Camera& camera) {
  // LES TICS DUS BATTENT D'ABORD, et la composition lit ce qu'ils ont laissé :
  // voir `EffectNodeSteps::Beat`.
  static_cast<void>(steps_.Beat(*this, now_ms, camera));

  composed_.clear();
  placements_.clear();
  drawn_.clear();
  ring_quads_.clear();
  world_quads_.clear();
  ground_cell_lots_.clear();
  ground_cells_.clear();
  // ⚠ DEUX FAÇONS DE SORTIR SANS RIEN RENDRE, et elles accusent des pièces
  // opposées : une texture que la fenêtre n'a pas chargée, ou une figure qui ne
  // rend aucun quad. Les compter coûte deux entiers et évite de deviner.
  std::size_t sans_texture = 0;
  std::size_t sans_quad = 0;
  // LES TIRAGES QU'UNE FIGURE FAIT À CHAQUE IMAGE : la graine est l'instant.
  std::uint32_t tirages_de_l_image = now_ms;
  for (const ro::render::PooledEffectNode& pooled : pool_.nodes()) {
    // CE QUI SE DESSINE : la porte, et un nœud déjà mis à jour — voir
    // `EffectNodePool::IsShown`.
    if (ro::render::EffectNodeShapeOnlySpawns(pooled.shape) || !pool_.IsShown(pooled)) {
      continue;
    }
    // CE QUE LE NŒUD COMPOSE SE RETIENT pour le lissage : voir `EffectNodeSteps`.
    const EffectNodeSteps::NodeOutputs outputs_of_node(steps_, *this, pooled);
    // ⚠ TOUTES LES IMAGES DU NŒUD ENTRENT AU CACHE, pas seulement la première :
    // un semis ne juge que celle de tête, et une figure qui en peint plusieurs
    // perdait en silence les traînées des autres — quatre des six glyphes de
    // l'Asura. Le cache retient aussi l'échec : ce n'est qu'une recherche par
    // image dès le second passage.
    if (!ro::render::EffectNodeShapeDrawsASprite(pooled.shape)) {
      for (const std::string& path : pooled.resource_paths) {
        TextureFor(path);
      }
    }
    // ⚠ DES FIGURES NE TIENNENT PAS DANS UN SEUL LOT, et elles sortent
    // d'ici : leurs quads ne partagent pas tous la même image. Voir
    // `effect_node_compose_multi.cpp`.
    if (ComposeNodeInSeveralLots(pooled, sans_texture, sans_quad)) {
      continue;
    }
    // ⚠ LA GRILLE DE VISÉE SORT PAR UNE VOIE À ELLE, seule de toutes les
    // formes : elle ne rend pas des quads mais des CASES, dont la géométrie
    // appartient au terrain et non à la figure. Voir `effect_magic_target.h`.
    if (pooled.shape == ro::render::PooledNodeShape::kMagicTargetGrid) {
      ComposeGroundCellsOf(pooled, sans_texture, sans_quad);
      continue;
    }
    // ⚠ DEUX FORMES SE DESSINENT PAR UN SPRITE, et non une seule. Ce test ne
    // nommait que la première, si bien que l'éventail de sprites cherchait son
    // `.spr` dans le cache des TEXTURES : il ne l'y trouvait jamais, se comptait
    // « sans texture » et ne paraissait pas. Trois identifiants étaient en table
    // sans qu'aucun soit à l'écran. Voir `EffectNodeShapeDrawsASprite`.
    if (!ro::render::EffectNodeShapeDrawsASprite(pooled.shape)) {
      // L'INDEX D'IMAGE DU NŒUD CHOISIT, et il tourne : c'est ce qui fait
      // battre un éclair. Il déborde quand la troupe avance plus vite que le
      // tableau ne compte d'images ; le ramener dans ses bornes est plus sûr
      // que de compter sur l'horloge.
      const std::size_t count = pooled.resource_paths.size();
      const std::size_t rang =
          count == 0 ? 0 : static_cast<std::size_t>(std::max(pooled.node.frame, 0)) % count;
      const auto texture =
          count == 0 ? textures_.end() : textures_.find(pooled.resource_paths[rang]);
      if (texture == textures_.end() || texture->second.empty()) {
        ++sans_texture;
        continue;
      }
      // ⚠ CHAQUE FIGURE SON ORIENTATION. Le mouvement du quad orienté bâtit sa
      // matrice à partir de la CAMÉRA ; les anneaux, eux, la tirent des DEUX
      // ANGLES de leur nœud.
      //
      // 🔴 ET UN ANNEAU N'EST PAS TOUJOURS COUCHÉ. Tant que ces angles n'étaient
      // pas portés, toute figure du monde restait à plat : l'anneau bleu d'un
      // impact, que le client dresse d'un quart de tour, se voyait par la
      // tranche. ⚠ SANS GARDE — le jeu ORIENTE toujours, il ne « mesure » rien ;
      // deux angles nuls rendent l'identité, qui EST l'orientation couchée.
      //
      // 🔴 MAIS LE TRONC DE CÔNE EST DÉJÀ DEBOUT, ET SON QUART DE TOUR LE
      // COUCHERAIT. Le client écrit ses deux cercles dans un plan que la matrice
      // du nœud redresse ensuite — une rotation autour de l'abscisse,
      // d'exactement quatre-vingt-dix degrés. Notre figure, elle,
      // est bâtie DEBOUT : deux cercles horizontaux, l'un au plan du nœud,
      // l'autre à sa demi-hauteur. Lui appliquer l'inclinaison de la table la
      // recouche, et l'anneau devient un cercle VERTICAL centré aux pieds —
      // dont la moitié passe sous le sol. Vu en jeu sur EF_WARP.
      //
      // ⚠ LES ONZE PATRONS DE CETTE FIGURE PORTENT QUATRE-VINGT-DIX, SANS
      // EXCEPTION. C'est la signature d'une convention de repère, non d'un choix
      // par effet : le quart de tour se replie donc dans la figure, comme celui
      // des pointes en croix (`effect_cross_spikes.h`), et l'inclinaison du
      // client reste dans la table — c'est une donnée, elle ne se retouche pas.
      const float quart_de_tour = pooled.shape == ro::render::PooledNodeShape::kRingCylinder
                                      ? ro::render::kEffectRingCylinderUprightDegrees
                                      : 0.0F;
      //
      // ⚠ ET LES ANGLES PASSENT PAR LES TABLES DU CLIENT, tronqués, un degré de
      // travers quand ils sont négatifs : voir `EffectNodeOrientedAsClient`.
      // 🔴 LE QUAD ORIENTÉ A TROIS RÈGLES, dans cet ordre : altitude de la caméra (0x200), face à
      // la caméra (0x40), angles du nœud. Voir `kEffectOrientedQuadLeansByCameraAltitude`.
      const bool oriented_quad = pooled.shape == ro::render::PooledNodeShape::kOrientedQuad;
      const bool leans_by_camera_altitude =
          oriented_quad && ro::render::EffectOrientedQuadLeansByCameraAltitude(pooled.node.flags);
      const bool face_camera =
          oriented_quad && ro::render::EffectNodeFacesCamera(pooled.node.flags);
      // Sous 0x400, le lacet suit l'angle du corps : voir `EffectOrientedQuadYawDegrees`.
      const ro::render::EffectRingOrientation facing =
          leans_by_camera_altitude
              ? ro::render::EffectNodeLeaningByCameraAltitude(camera, pooled.node_body_degrees)
          : face_camera ? ro::render::EffectNodeFacingCamera(camera, pooled.node_pitch_degrees)
          : oriented_quad
              ? ro::render::EffectNodeOrientedAsClient(
                    ro::render::EffectOrientedQuadYawDegrees(
                        pooled.node.flags, pooled.node_yaw_degrees, pooled.node_body_degrees,
                        pooled.node_yaw_step),
                    pooled.node_pitch_degrees, pooled.node_roll_degrees)
              : ro::render::EffectNodeOrientedAsClient(
                    pooled.node_yaw_degrees, pooled.node_pitch_degrees, 0.0F, quart_de_tour);
      std::vector<ro::render::EffectWorldQuad> quads;
      switch (pooled.shape) {
        // ⚠ DEUX FORMES, UNE FIGURE : le cône du sous-type 23 se dessine
        // exactement comme l'anneau du trente et n'en diffère que par son
        // mouvement. C'est la forme qui porte les deux, et c'est ici qu'elles se
        // rejoignent.
        // ⚠ ET UNE CINQUIÈME FORME LES REJOINT : les anneaux du sous-type 63
        // se dessinent avec la même jupe et n'en diffèrent que par leur
        // mouvement. Sa quatrième piste est éteinte, si bien que la boucle
        // ci-dessous l'écarte d'elle-même.
        // ⚠ ET UNE SIXIÈME : les anneaux de téléportation du sous-type 57 se dessinent de la même
        // jupe, et n'en diffèrent que par leur mouvement — un rayon qui respire au lieu de croître.
        // ⚠ ET UNE HUITIÈME, les anneaux d'un portail (65), ET LA GERBE (107) : la même jupe, un
        // rayon qui se referme puis se rembobine pour l'un, un cercle qui grandit pour l'autre.
        case ro::render::PooledNodeShape::kWarpRings:
        case ro::render::PooledNodeShape::kPulsingRings:
        case ro::render::PooledNodeShape::kRadialParticleGroups:
        case ro::render::PooledNodeShape::kTeleportRings:
        case ro::render::PooledNodeShape::kDualModeSprayRings:
        case ro::render::PooledNodeShape::kParticleConeRings:
        case ro::render::PooledNodeShape::kRingQuartet:
        case ro::render::PooledNodeShape::kRingStarburst:
        // ⚠ ET UNE SEPTIÈME : l'anneau du sous-type 139 se dessine de la même
        // jupe et n'en diffère que par son mouvement — une onde qui bat autour
        // de sa clé, et une échelle qui saute d'un cran par quart de cycle.
        case ro::render::PooledNodeShape::kTurningArch:
        case ro::render::PooledNodeShape::kSpiralParticles:
        case ro::render::PooledNodeShape::kRadialSpreadParticles:
        case ro::render::PooledNodeShape::kOscillatingRadiusParticles:
        case ro::render::PooledNodeShape::kRingTracks:
          // LES QUATRE PISTES DANS LE MÊME LOT : elles partagent leur texture
          // et leur teinte, et les séparer paierait quatre lots de rendu là où
          // le client en paie un.
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            const std::vector<ro::render::EffectWorldQuad> skirt =
                ro::render::ComposeEffectTrackSkirt(track, pooled.trail_variant,
                                                    pooled.ground_position);
            quads.insert(quads.end(), skirt.begin(), skirt.end());
          }
          break;
        // ⚠ LE RANG DE LA PISTE ENTRE DANS LA FIGURE DU RUBAN, et c'est le seul
        // cas de ce fichier : les quatre bras de Grand Cross ne diffèrent pas
        // que par leur azimut, le client choisit l'angle du bras sur le rang.
        // Une boucle par valeur les mettrait tous les quatre au même endroit.
        case ro::render::PooledNodeShape::kGrandCrossRibbon:
          for (std::size_t arm_index = 0; arm_index < pooled.tracks.size(); ++arm_index) {
            const std::vector<ro::render::EffectWorldQuad> arm =
                ro::render::ComposeGrandCrossRibbonQuads(pooled.tracks[arm_index], arm_index,
                                                         pooled.trail_variant,
                                                         pooled.ground_position);
            quads.insert(quads.end(), arm.begin(), arm.end());
          }
          break;
        case ro::render::PooledNodeShape::kGrandCrossFrame:
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            const std::vector<ro::render::EffectWorldQuad> frame =
                ro::render::ComposeGrandCrossFrameQuads(track, pooled.ground_position);
            quads.insert(quads.end(), frame.begin(), frame.end());
          }
          break;
        case ro::render::PooledNodeShape::kRisingParticles: {
          // LES QUATRE PARTICULES DANS LE MÊME LOT : elles partagent leur
          // texture, et les séparer paierait quatre lots de rendu là où le
          // client en paie un.
          const std::int16_t tour = ro::render::EffectBillboardTurnDegrees(camera);
          const std::int16_t tilt = ro::render::EffectBillboardTiltDegrees(camera);
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            if (const std::optional<ro::render::EffectWorldQuad> quad =
                    ro::render::ComposeRisingParticleQuad(track, tour, tilt);
                quad.has_value()) {
              quads.push_back(*quad);
            }
          }
          break;
        }
        case ro::render::PooledNodeShape::kArcRibbons: {
          // ⚠ UNE SEULE PISTE EST VIVE ici, mais on les parcourt toutes : la
          // figure rend un lot vide pour une piste éteinte, et compter sur le
          // rang lierait la fenêtre à un détail du bâtisseur.
          const std::int16_t tour = ro::render::EffectBillboardTurnDegrees(camera);
          const std::int16_t tilt = ro::render::EffectBillboardTiltDegrees(camera);
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            const std::vector<ro::render::EffectWorldQuad> arc =
                ro::render::ComposeArcRibbonQuads(track, tour, tilt, pooled.ground_position);
            quads.insert(quads.end(), arc.begin(), arc.end());
          }
          break;
        }
        case ro::render::PooledNodeShape::kFallingItem:
        case ro::render::PooledNodeShape::kScatterField:
        // ⚠ `kRisingSparks` NE PARVIENT JAMAIS JUSQU'ICI — la branche en tête
        // de boucle l'a pris, parce que ses pistes ne partagent pas toujours
        // leur texture. Il y figure pour que la couverture reste complète et que
        // le compilateur puisse la vérifier.
        case ro::render::PooledNodeShape::kRisingSparks:
        // LE PROJECTILE GUIDÉ partage la figure du carré teinté : c'est son
        // MOUVEMENT qui diffère, pas son dessin. Voir `kHomingTrail`.
        case ro::render::PooledNodeShape::kHomingTrail:
        case ro::render::PooledNodeShape::kThrownItem:
        // L'ÉTINCELLE DU SOIN partage la figure du carré teinté : c'est son
        // MOUVEMENT qui diffère, pas son dessin. Voir `kHealSparkle`.
        case ro::render::PooledNodeShape::kHealSparkle:
        // LES CARRÉS QUI MONTENT aussi. Voir `kRisingQuads`.
        case ro::render::PooledNodeShape::kRisingQuads:
        // LE CARRÉ QUI FILE (116) ET LES ICÔNES QUI FLOTTENT (123) aussi : même
        // dessinateur, seuls leurs mouvements diffèrent.
        case ro::render::PooledNodeShape::kSwirlTrail:
        case ro::render::PooledNodeShape::kDriftingIcons:
        case ro::render::PooledNodeShape::kTintedQuads:
          quads = ComposeTintedQuadNodeShape(pooled);
          break;
        case ro::render::PooledNodeShape::kSpiralFan: {
          // ⚠ LES DEUX ANGLES SE LISENT DANS LES PISTES, comme pour le carré
          // teinté : le mouvement les y a rangés au dernier tic, et les
          // recalculer ici donnerait un éventail en avance d'un tic.
          //
          // ⚠ ET UNE SEULE PISTE EST VIVE, mais on les parcourt toutes : la
          // figure rend un lot vide pour une piste éteinte, et compter sur le
          // rang lierait la fenêtre à un détail du bâtisseur.
          const std::int16_t tour = pooled.tracks.front().style;
          const std::int16_t tilt = pooled.tracks[1].style;
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            const std::vector<ro::render::EffectWorldQuad> fan =
                ro::render::ComposeSpiralFanQuads(track, tour, tilt, pooled.ground_position);
            quads.insert(quads.end(), fan.begin(), fan.end());
          }
          break;
        }
        case ro::render::PooledNodeShape::kRingCylinder:
          quads = ro::render::ComposeEffectNodeRingCylinder(pooled.node, pooled.ground_position,
                                                            facing);
          break;
        // ⚠ LE PAS LES POUSSE LE LONG DE LEUR PROFONDEUR, jamais de la caméra :
        // l'axe se bâtit des angles du nœud. Voir `EffectNodeDepthAxis`.
        case ro::render::PooledNodeShape::kCrossedPlanes:
          quads = ro::render::ComposeEffectNodeCrossedPlanes(
              pooled.node, pooled.ground_position + ro::render::EffectNodeDepthAdvance(pooled),
              facing);
          break;
        case ro::render::PooledNodeShape::kOrientedQuad:
          quads = ro::render::ComposeEffectNodeOrientedQuad(
              pooled.node, pooled.ground_position + ro::render::EffectNodeDepthAdvance(pooled),
              facing);
          break;
        // ⚠ PAS DE `default`, ET C'EST VOULU. Celui qui vivait ici retombait sur
        // le tronc de cône : une forme ajoutée sans figure s'y avalait en
        // silence, et l'on cherchait la faute dans la figure au lieu du câblage.
        // Sans lui, une forme oubliée est une erreur de compilation.
        //
        // `kSprite` ne parvient jamais jusqu'ici — le test qui enveloppe ce
        // `switch` l'écarte —, mais il y figure pour que la couverture soit
        // complète et que le compilateur puisse la vérifier.
        // ⚠ `kScrollingBand` NE PARVIENT JAMAIS JUSQU'ICI — la branche en tête
        // de boucle l'a prise, parce que ses segments ne partagent pas leur
        // image. Elle y figure pour que la couverture reste complète et que le
        // compilateur puisse la vérifier ; lui donner une figure ici en
        // donnerait une FAUSSE le jour où elle y tomberait.
        // ⚠ NI CELLE-CI NI `kDarkVeil` NE PARVIENNENT JUSQU'ICI — les branches en
        // tête de boucle les ont prises, parce que leurs quads ne partagent pas
        // leur image. Elles y figurent pour que la couverture reste complète et
        // que le compilateur puisse la vérifier. La nappe non plus : ce client
        // la peint à l'écran, avec le voile.
        case ro::render::PooledNodeShape::kScrollingBand:
        case ro::render::PooledNodeShape::kDarkVeil:
        case ro::render::PooledNodeShape::kGridPlane:
        case ro::render::PooledNodeShape::kTypedRibbon:
        case ro::render::PooledNodeShape::kSphereShell:
        case ro::render::PooledNodeShape::kGlidingSlash:
        case ro::render::PooledNodeShape::kAimedGlidingSlash:
          break;
        case ro::render::PooledNodeShape::kCrossSpikes:
          // ⚠ ELLE LIT LE NŒUD, PAS LES PISTES — sauf une marque, que la
          // première porte pour les quatre. Voir `effect_cross_spikes.h`.
          // ⚠ ELLE PENCHE, et c'est le nœud qui le dit. Le client tire une
          // inclinaison et un lacet à la naissance de chaque pointe : les
          // poser toutes droites donnait une forêt trop régulière pour être la
          // sienne. Voir `EffectNodeLeaningOrientation`.
          // ⚠ ET ELLE MONTE LE LONG DE SON PROPRE AXE, pas de la verticale : le
          // pas du nœud la pousse dans la direction où pointe son sommet. Elle
          // naît sous terre, jaillit, s'arrête, puis REPLONGE trente tics avant
          // sa fin — sans quoi elle s'éteindrait sur place, ce qui n'est pas ce
          // que le client montre. Voir `EffectNode::translates_along_axis`.
          quads = ro::render::ComposeCrossSpikeQuads(
              ro::render::EffectCrossSpikeRequestOf(pooled.node, pooled.tracks.front().mark[0],
                                                    tirages_de_l_image),
              pooled.ground_position + ro::render::EffectNodeLeanAxis(pooled.node.lean_degrees,
                                                                      pooled.node.yaw_degrees) *
                                           pooled.node.axial_translation,
              ro::render::EffectNodeLeaningOrientation(pooled.node.lean_degrees,
                                                       pooled.node.yaw_degrees));
          break;
        case ro::render::PooledNodeShape::kGroundRibbon:
          // LES QUATRE RUBANS DANS LE MÊME LOT : ils partagent leur image.
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            if (const std::optional<ro::render::EffectWorldQuad> ribbon =
                    ro::render::ComposeGroundRibbonQuad(track, pooled.ground_position);
                ribbon.has_value()) {
              quads.push_back(*ribbon);
            }
          }
          break;
        case ro::render::PooledNodeShape::kAuraColumn:
          // ⚠ LES DEUX ANGLES SE LISENT DANS LES PISTES, comme partout dans cette
          // famille : le mouvement les y a ranges au dernier tic.
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            const std::vector<ro::render::EffectWorldQuad> column =
                ro::render::ComposeAuraColumnQuads(track, pooled.tracks[0].style,
                                                   pooled.tracks[1].style, pooled.ground_position);
            quads.insert(quads.end(), column.begin(), column.end());
          }
          break;
        case ro::render::PooledNodeShape::kRainParticle:
          // LES QUATRE PISTES SONT VIVES ICI, et chacune rend son grain puis son
          // halo — ou le halo seul, quand sa marque sort des trois nommées.
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            const std::vector<ro::render::EffectWorldQuad> grains =
                ro::render::ComposeRainParticleQuads(
                    track, pooled.tracks[0].style, pooled.tracks[1].style, pooled.ground_position);
            quads.insert(quads.end(), grains.begin(), grains.end());
          }
          break;
        case ro::render::PooledNodeShape::kFadingStreak:
          // LES QUATRE PISTES DANS LE MEME LOT : elles partagent leurs images,
          // et chacune rend CINQ copies.
          //
          // ⚠ LA TEINTE ET LA CLE VIENNENT DE LA PISTE ZERO pour les quatre, et
          // c est le client qui le veut — voir effect_fading_streak.h.
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            const std::vector<ro::render::EffectWorldQuad> trail =
                ro::render::ComposeFadingStreakQuads(
                    track, pooled.tracks[0].style, pooled.tracks[1].style, pooled.ground_position,
                    pooled.tracks[0].mark[ro::render::kMarkOfFadingStreakTint],
                    pooled.tracks[0].mark[ro::render::kMarkOfFadingStreakSpeedKey]);
            quads.insert(quads.end(), trail.begin(), trail.end());
          }
          break;
        case ro::render::PooledNodeShape::kRainbowTrail:
          // UNE SEULE PISTE VIVE : les trois autres rendent un lot vide
          // d'elles-memes.
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            const std::vector<ro::render::EffectWorldQuad> arc =
                ro::render::ComposeRainbowTrailQuads(track, pooled.ground_position);
            quads.insert(quads.end(), arc.begin(), arc.end());
          }
          break;
        case ro::render::PooledNodeShape::kSpiralTrail:
          // LES QUATRE PISTES DANS LE MEME LOT : elles partagent leurs trois
          // images, et chacune rend son grain et sa trace.
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            const std::vector<ro::render::EffectWorldQuad> spray =
                ro::render::ComposeSpiralTrailQuads(track, pooled.tracks[0].style,
                                                    pooled.tracks[1].style);
            quads.insert(quads.end(), spray.begin(), spray.end());
          }
          break;
        case ro::render::PooledNodeShape::kDoubleRibbon:
          // LES QUATRE PISTES DANS LE MÊME LOT : elles partagent leur image et
          // leur variante, et chacune rend jusqu'à quarante quads — deux par
          // segment, les deux identiques.
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            const std::vector<ro::render::EffectWorldQuad> aura =
                ro::render::ComposeDoubleRibbonQuads(track, pooled.ground_position,
                                                     pooled.trail_variant);
            quads.insert(quads.end(), aura.begin(), aura.end());
          }
          break;
        case ro::render::PooledNodeShape::kSpikeBeam:
          // LES QUATRE PISTES DANS LE MÊME LOT : elles partagent leur image, et
          // chacune rend jusqu'à six segments — moins dès que sa portée a passé
          // cent.
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            const std::vector<ro::render::EffectWorldQuad> faisceau =
                ro::render::ComposeSpikeBeamQuads(track, pooled.ground_position);
            quads.insert(quads.end(), faisceau.begin(), faisceau.end());
          }
          break;
        case ro::render::PooledNodeShape::kPieWedge:
          // LES QUATRE PISTES DANS LE MÊME LOT : elles partagent leur image, et
          // chacune rend UN triangle.
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            const std::vector<ro::render::EffectWorldQuad> quartier =
                ro::render::ComposePieWedgeQuads(track, pooled.ground_position);
            quads.insert(quads.end(), quartier.begin(), quartier.end());
          }
          break;
        case ro::render::PooledNodeShape::kConeWedge:
          // LES QUATRE PISTES DANS LE MÊME LOT : elles partagent leur image.
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            const std::vector<ro::render::EffectWorldQuad> corner =
                ro::render::ComposeConeWedgeQuads(track, pooled.ground_position);
            quads.insert(quads.end(), corner.begin(), corner.end());
          }
          break;
        case ro::render::PooledNodeShape::kBreathingQuad:
        case ro::render::PooledNodeShape::kFootprint:
        case ro::render::PooledNodeShape::kGroundDisc:
        case ro::render::PooledNodeShape::kGrowingSprite:
          // ⚠ LE STYLE VIENT DU NŒUD, et le RANG de la piste entre dans l'angle
          // de départ : c'est lui qui écarte les pistes pour les styles qui ne
          // lisent pas l'azimut.
          //
          // ⚠ ET LES DEUX FORMES PARTAGENT CETTE FIGURE, parce que la table des
          // sous-types du client les mène au même dessinateur. Ce qui les sépare
          // est en amont — leur bâtisseur et leur mouvement —, et les fondre ici
          // serait les fondre partout.
          for (std::size_t layer = 0; layer < pooled.tracks.size(); ++layer) {
            const std::vector<ro::render::EffectWorldQuad> quad = ro::render::ComposeBreathingQuad(
                pooled.tracks[layer], pooled.trail_variant, layer, pooled.ground_position);
            quads.insert(quads.end(), quad.begin(), quad.end());
          }
          break;
        case ro::render::PooledNodeShape::kSparkTube:
          // LES QUATRE PISTES DANS LE MÊME LOT : elles partagent leur image, et
          // chacune rend jusqu'à quinze quads — trois faces par tronçon, et un
          // second tube pour un style.
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            const std::vector<ro::render::EffectWorldQuad> tube =
                ro::render::ComposeSparkTubeQuads(track);
            quads.insert(quads.end(), tube.begin(), tube.end());
          }
          break;
        case ro::render::PooledNodeShape::kGroundCurtain:
          // LES QUATRE PISTES DANS LE MÊME LOT : elles partagent leur image, et
          // chacune rend UN rectangle debout — ou rien.
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            if (const std::optional<ro::render::EffectWorldQuad> tenture =
                    ro::render::ComposeGroundCurtainQuad(track, pooled.ground_position);
                tenture.has_value()) {
              quads.push_back(*tenture);
            }
          }
          break;
        case ro::render::PooledNodeShape::kDriftingMote:
          // LES QUATRE PISTES DANS LE MÊME LOT : elles partagent leur image, et
          // chacune rend UN carré — ou rien.
          //
          // ⚠ LES DEUX ANGLES DE CAMÉRA VIENNENT DES STYLES DES PISTES ZÉRO ET
          // UN, où le mouvement les a rangés.
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            if (const std::optional<ro::render::EffectWorldQuad> grain =
                    ro::render::ComposeDriftingMoteQuad(track, pooled.tracks[0].style,
                                                        pooled.tracks[1].style,
                                                        pooled.ground_position);
                grain.has_value()) {
              quads.push_back(*grain);
            }
          }
          break;
        case ro::render::PooledNodeShape::kDriftingTriPlane:
        case ro::render::PooledNodeShape::kFlyingTriPlane:
          // DEUX FORMES, UNE FIGURE : seuls leurs mouvements diffèrent.
          //
          // ⚠ LES DEUX ANGLES DE CAMÉRA VIENNENT DES STYLES DES PISTES ZÉRO ET
          // UN, où le mouvement les a rangés — comme la rafale et le grain.
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            const std::vector<ro::render::EffectWorldQuad> plans = ro::render::ComposeTriPlaneQuads(
                track, pooled.tracks[0].style, pooled.tracks[1].style, pooled.ground_position);
            quads.insert(quads.end(), plans.begin(), plans.end());
          }
          break;
        case ro::render::PooledNodeShape::kHomingSpark:
          // LES QUATRE PISTES DANS LE MÊME LOT : elles partagent leur image, et
          // chacune rend DEUX lames — ou rien.
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            const std::vector<ro::render::EffectWorldQuad> lames =
                ro::render::ComposeHomingSparkQuads(track, pooled.ground_position);
            quads.insert(quads.end(), lames.begin(), lames.end());
          }
          break;
        case ro::render::PooledNodeShape::kLightningFan:
          // ⚠ LE GÉNÉRATEUR ENTRE DANS LA FIGURE, seule du lot à en vouloir
          // un : le client re-tire son tremblement à chaque image. Le lui
          // refuser figerait le grésillement de l'éclair.
          for (const ro::render::EffectTrack& track : pooled.tracks) {
            const std::vector<ro::render::EffectWorldQuad> faces =
                ro::render::ComposeLightningFanQuads(track, shimmer_);
            quads.insert(quads.end(), faces.begin(), faces.end());
          }
          break;
        case ro::render::PooledNodeShape::kElectricArc:
          // ⚠ LES QUATRE PISTES D'UN COUP, et non une par une : les nappes
          // s'accrochent au bord de la piste zéro. Même générateur que l'éclair.
          {
            const std::vector<ro::render::EffectWorldQuad> arc =
                ro::render::ComposeElectricArcQuads(pooled.tracks, shimmer_);
            quads.insert(quads.end(), arc.begin(), arc.end());
          }
          break;
        default:
          // ⚠ LA SUITE DES FORMES VIT DANS `effect_node_compose_shapes.cpp`, au
          // plafond de huit cents lignes. Ce `default:` n'avale rien en
          // silence : le second `switch` les nomme TOUTES, celles-ci
          // comprises, et une forme neuve y reste une erreur de compilation.
          quads = ComposeRemainingNodeShape(pooled, facing);
          break;
      }
      if (quads.empty()) {
        ++sans_quad;
        continue;
      }
      ring_quads_.push_back(std::move(quads));
      // Le chemin et l'image sont pris DANS LA TABLE, dont les entrées ne
      // bougent pas ; les quads, eux, n'ont pas fini de grandir et se
      // rattachent plus bas.
      world_quads_.push_back(ro::render::DrawnWorldQuads{
          {}, &texture->first, &texture->second, pooled.ground_position, pooled.node.render_word});
      continue;
    }
    // LE SPRITE, dernier cas de la boucle : il ne compose pas des quads du
    // MONDE mais une image d'ACT, et il vit donc avec les autres figures.
    ComposeNodeAsSprite(pooled, camera);
  }
  // LE LISSAGE MÊLE avant que les poses ne deviennent des acteurs : voir `EffectNodeSteps`.
  steps_.Smooth(*this, now_ms, camera);
  // Les pointeurs se prennent APRÈS que la liste a fini de grandir : les
  // prendre au fil des ajouts les laisserait pendre à la première
  // réallocation. C'est vrai des quads d'anneau comme des acteurs composés.
  for (std::size_t index = 0; index < world_quads_.size(); ++index) {
    world_quads_[index].quads = ring_quads_[index];
  }
  for (std::size_t index = 0; index < ground_cells_.size(); ++index) {
    ground_cells_[index].cells = ground_cell_lots_[index];
  }
  // ⚠ CE COMPTE NE SE REMET JAMAIS À ZÉRO, et c'est ce qui le rend utile : à
  // zéro, AUCUN lot n'a jamais été composé depuis le lancement, et la panne
  // n'est pas celle d'un effet mais de toute la voie des quads du monde.
  lots_composes_ += world_quads_.size();
  // Gardés pour l'appelant : c'est ce qui permet au juge des effets de dire
  // POURQUOI un effet est resté muet, sans rouvrir son cas à la main.
  sans_texture_ = sans_texture;
  sans_quad_ = sans_quad;
  // ⚠ UNE TROUPE MUETTE À LA PREMIÈRE IMAGE EST NORMALE, et le dire alors est
  // une fausse alerte : un nœud naît à alpha nul et à segments de longueur
  // nulle, et c'est le mouvement qui lui donne son aire. Muette TRENTE images
  // de suite, elle ne l'est plus.
  //
  // ⚠ ET LA LIGNE PORTE L'ÉTAT, pas seulement le compte : savoir qu'aucune
  // figure ne sort ne dit pas POURQUOI. L'âge, l'alpha et le rayon de la
  // première piste le disent d'un coup — un alpha qui reste nul accuse le
  // mouvement, un rayon nul accuse le semis, un âge qui ne monte pas accuse
  // l'horloge.
  if (!pool_.nodes().empty() && world_quads_.empty() && composed_.empty() &&
      ground_cells_.empty()) {
    ++silent_frames_;
    // ⚠ ET SEULEMENT SI RIEN N'A JAMAIS ÉTÉ COMPOSÉ. Une troupe se tait
    // légitimement à la fin d'un effet — la jupe de l'anneau de Heal se referme
    // au soixante-seizième tic alors que son nœud en vit cent —, et crier au loup
    // là-dessus a coûté deux allers-retours. À zéro lot composé depuis le
    // lancement, en revanche, il n'y a pas d'explication innocente.
    if (silent_frames_ == kSilentFramesBeforeWarning && lots_composes_ == 0) {
      ro::core::LogWarning(
          "{} lot(s) composés DEPUIS LE LANCEMENT ; troupe de {} nœud(s), {} images SANS"
          " AUCUNE figure : {} sans texture, {} sans quad",
          lots_composes_, pool_.size(), silent_frames_, sans_texture, sans_quad);
      // ⚠ TOUS LES NOEUDS, PAS LE PREMIER : deux nœuds d'un même effet n'ont ni
      // la même figure ni le même mouvement, et n'accuser que le premier laisse
      // l'autre hors de vue.
      //
      // ⚠ ET TOUT CE QUE LA FIGURE LIT. La jupe renonce sur TROIS conditions —
      // piste éteinte, balayage négatif, alpha nul une fois la palette
      // consultée —, et une ligne qui n'en montre aucune laisse deviner. Les
      // longueurs de segment disent si le mouvement a ouvert la jupe ; les
      // marques disent si le carré teinté a une parure qui dessine.
      for (const ro::render::PooledEffectNode& node : pool_.nodes()) {
        const ro::render::EffectTrack& own = node.tracks.front();
        // AU DÉTAIL, et non en alerte : la dégradation est déjà dite au-dessus.
        // Ceci en est l'état interne, une ligne par nœud de la troupe, et il ne
        // se lit que du renderer.
        ro::core::LogDebug(
            "  nœud forme {}, âge {} tics, variante {} ; piste 0 active {} degrés {} azimut {}"
            " inclinaison {} clé {:.1f} rayon {:.2f} alpha {} tics {} marques {}/{}/{}/{}"
            " longueurs {:.2f} {:.2f} {:.2f}",
            static_cast<int>(node.shape), node.node.age_ticks, node.trail_variant, own.active,
            own.degrees, own.azimuth_degrees, own.tilt_degrees, own.lift_key, own.radius_at_anchor,
            own.alpha, own.ticks, own.mark[0], own.mark[1], own.mark[2], own.mark[3], own.value[0],
            own.value[1], own.value[2]);
      }
    }
  } else {
    silent_frames_ = 0;
  }
  // LES POSES SE RANGENT PAR LOT, dans l'ordre des passes : sous les `.str` puis
  // non (`ro::render::EffectLotPaintsBeforeStr`), éprouvées puis non, l'addition
  // puis le recouvrement. La liste réservée ne grandit plus : les vues tiennent.
  drawn_.reserve(placements_.size());
  sprite_batches_.clear();
  for (const bool before_str : {true, false}) {
    for (const bool eprouves : {true, false}) {
      for (const bool additifs : {true, false}) {
        const std::size_t start = drawn_.size();
        for (const ComposedPlacement& pose : placements_) {
          // 🔴 LE MOT DE RENDU DÉCIDE, ET LA LECTURE EST COMPLÈTE DES DEUX CÔTÉS.
          // Le sprite écrit sa destination `4 · (mot ≠ 5) + 2`, puis la file de
          // rendu range la primitive du mot un dans l'un de deux seaux triés. Ces
          // deux seaux-là se vident sous les états 19 = 5 et 20 = 6 (mélange
          // ordinaire), que rien ne réécrit ensuite — contrairement à la passe
          // finale de la scène qui, elle, lit le mélange de chaque primitive. Le
          // seau IMPOSE donc le mélange ordinaire.
          //
          // Forcer l'addition ici serait faux : l'ombre de Sight s'AJOUTERAIT au
          // lieu d'assombrir, ce qu'aucune ombre ne fait.
          // ⚠ ET LE CADRE DE `soule.spr` EST UN AUTRE DÉFAUT. S'il reparaît, il se
          // cherche dans NOTRE chaîne de sprite — décodage, bord, alpha — et non
          // ici : le client peint bien ces images-là en mélange ordinaire.
          const bool own_additive = !pose.render_word.has_value() ||
                                    ro::render::EffectSpriteIsAdditive(*pose.render_word);
          const bool own_depth_tested = ro::render::EffectLotIsDepthTested(pose.render_word);
          const bool below_str = ro::render::EffectLotPaintsBeforeStr(pose.render_word);
          if (below_str != before_str || own_depth_tested != eprouves || own_additive != additifs) {
            continue;
          }
          ro::render::DrawnActor actor;
          actor.actor = &composed_[pose.composed_index];
          actor.ground_position = pose.ground_position;
          actor.tint = pose.tint;
          actor.scale = pose.scale;
          actor.grow_screen_pixels = pose.grow_screen_pixels;
          actor.fogged = ro::render::EffectLotIsFogged(pose.render_word);
          drawn_.push_back(actor);
        }
        if (drawn_.size() > start) {
          sprite_batches_.push_back(SpriteNodeBatch{
              std::span<const ro::render::DrawnActor>(drawn_).subspan(start, drawn_.size() - start),
              additifs, eprouves, before_str});
        }
      }
    }
  }
  return drawn_;
}

}  // namespace moonclient
