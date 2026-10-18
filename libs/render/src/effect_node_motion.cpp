// LE MOUVEMENT DE CHAQUE FORME DE NŒUD, un cas par forme.
//
// ⚠ SORTI DE `effect_node_pool.cpp`, QUI ÉTAIT À HUIT CENTS LIGNES PILE. La
// coupure suit une frontière réelle : d'un côté la troupe, de l'autre ce
// catalogue. Voir `effect_node_motion.h`.

#include "effect_node_motion.h"

#include "ro/render/effect_aimed_spoke.h"
#include "ro/render/effect_anchored_skirt.h"
#include "ro/render/effect_arc_ribbon_seed.h"
#include "ro/render/effect_aura_column.h"
#include "ro/render/effect_aura_column_seed.h"
#include "ro/render/effect_break_ring.h"
#include "ro/render/effect_camera_fan.h"
#include "ro/render/effect_cloud_ring_seed.h"
#include "ro/render/effect_drifting_icon.h"
#include "ro/render/effect_electric_arc.h"
#include "ro/render/effect_expanding_ring.h"
#include "ro/render/effect_fading_sprite.h"
#include "ro/render/effect_fading_twin_disc_seed.h"
#include "ro/render/effect_frame_and_bar.h"
#include "ro/render/effect_grand_cross.h"
#include "ro/render/effect_ground_box.h"
#include "ro/render/effect_ground_gust.h"
#include "ro/render/effect_heal_sparkle.h"
#include "ro/render/effect_homing_gust.h"
#include "ro/render/effect_homing_spark.h"
#include "ro/render/effect_job_aura.h"
#include "ro/render/effect_lightning_fan.h"
#include "ro/render/effect_magic_target.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_rainstorm_cylinder.h"
#include "ro/render/effect_rising_quad.h"
#include "ro/render/effect_rolling_gust.h"
#include "ro/render/effect_sheathed_bolt.h"
#include "ro/render/effect_snaking_arc.h"
#include "ro/render/effect_spin_ribbon.h"
#include "ro/render/effect_spinning_round.h"
#include "ro/render/effect_sprite_fan.h"
#include "ro/render/effect_swirl_trail.h"
#include "ro/render/effect_teleport_rings.h"
#include "ro/render/effect_tethered_beam.h"
#include "ro/render/effect_tetra_vortex.h"
#include "ro/render/effect_thunder_freeze.h"
#include "ro/render/effect_thunder_scatter.h"
#include "ro/render/effect_track_burst.h"
#include "ro/render/effect_track_spread.h"
#include "ro/render/effect_trail_history.h"
#include "ro/render/effect_tri_plane.h"
#include "ro/render/effect_twin_disc_seed.h"
#include "ro/render/effect_wandering_motes.h"
#include "ro/render/effect_warp_ring_seed.h"

namespace ro::render {

// CHAQUE FORME A SON MOUVEMENT, et c'est la forme qui le choisit.
//
// Les faire battre toutes par le même donnerait à l'une le rythme de l'autre :
// l'anneau ondule et se rallume, la particule monte et s'efface une fois, l'arc
// tourne et respire. Une forme sans piste n'en paie rien — ses quatre pistes
// sont éteintes.
//
// ⚠ CE `switch` N'A PAS DE `default`, ET C'EST VOULU. Une forme ajoutée sans
// mouvement doit être une ERREUR DE COMPILATION, pas un nœud qui bat au rythme
// d'un autre. C'est la leçon des `default:` qui retombaient sur `kRingBand` :
// une forme oubliée s'y avalait en silence, et l'on cherchait la faute dans la
// figure.
void AdvanceTracksOf(PooledEffectNode& pooled, const EffectScatterWorld& world,
                     std::uint32_t& generator) {
  switch (pooled.shape) {
    case PooledNodeShape::kSprite:
      // ⚠ UNE PARTICULE LANCÉE SE DÉPLACE ICI, ET UNE SEULE FOIS PAR TIC : rien
      // d'autre n'avance le point d'un sprite de troupe. Ce cas ne CHUTE plus
      // dans ceux des anneaux — ils ont reçu le même appel avec les traits
      // d'EF_ENHANCE, et chaque sprite lancé avançait alors deux fois par tic :
      // une boule de Soul Strike doublait sa cible au lieu de s'y arrêter.
      AdvanceLaunchedNode(pooled, world);
      AdvanceRadialNode(pooled);
      // PUIS SA TRAÎNÉE RECULE D'UN CRAN, comme chez le client en fin de mise à
      // jour.
      AdvanceEffectTrailHistory(pooled.trail, pooled.node, pooled.ground_position);
      return;
    case PooledNodeShape::kRingBand:
    case PooledNodeShape::kRingCylinder:
    case PooledNodeShape::kCrossedPlanes:
    case PooledNodeShape::kOrientedQuad:
    case PooledNodeShape::kUvSphere:
    case PooledNodeShape::kRingTracks:
      // 🔴 UNE FIGURE D'ANNEAU PEUT PARTIR, ELLE AUSSI, et seul le sprite le
      // faisait. Les traits d'EF_ENHANCE montent de deux unités par tic — un pas
      // que le client envoie vers le haut par la matrice du nœud —, et ils
      // restaient cloués à leur naissance : le lancer était tiré, jamais avancé.
      //
      // ⚠ SANS GARDE : `AdvanceLaunchedNode` s'en va devant une direction nulle,
      // qui est celle de tout nœud qui ne part nulle part.
      AdvanceLaunchedNode(pooled, world);
      // Le rang entre dans la phase de l'onde : les quatre pistes d'un anneau
      // n'ondulent pas ensemble.
      for (std::size_t rang = 0; rang < pooled.tracks.size(); ++rang) {
        AdvanceUndulatingRingTrack(pooled.tracks[rang], rang, pooled.node.life_ticks);
      }
      return;
    case PooledNodeShape::kRisingParticles:
      for (EffectTrack& track : pooled.tracks) {
        AdvanceRisingParticleTrack(track);
      }
      return;
    case PooledNodeShape::kArcRibbons:
      for (EffectTrack& track : pooled.tracks) {
        AdvanceStyledBillboardTrack(track);
      }
      FollowArcRibbonOwner(pooled);
      return;
    case PooledNodeShape::kGlowSprite:
      // ⚠ CE MOUVEMENT MÈNE L'ALPHA DU NŒUD, pas celui d'une piste, et fait
      // reculer la traînée d'un cran. Voir `AdvanceFadingTrailNode`.
      AdvanceFadingTrailNode(pooled.tracks, pooled.node, pooled.trail, pooled.ground_position);
      return;
    case PooledNodeShape::kFallingItem:
      // ⚠ LE MOUVEMENT DIT QUAND LE CLIENT SONNE, et nous l'ignorons ici : la
      // troupe ne fait pas de bruit. Le tic est celui-là et non un autre, et
      // l'appelant qui voudra le jouer n'aura pas à le deviner.
      (void)AdvanceFallingTrailTracks(pooled.tracks, pooled.ground_position,
                                      pooled.billboard_turn_degrees, pooled.billboard_tilt_degrees);
      return;
    case PooledNodeShape::kRadialParticleGroups:
    case PooledNodeShape::kHeartRings:
      // ⚠ MÊME MOUVEMENT, DEUX FIGURES : le premier se dessine en jupe d'anneau,
      // le second en ruban de traînée. C'est la forme qui les sépare, et c'est
      // à la composition qu'on le voit.
      pooled.ground_position = pooled.owner_position;
      AdvanceRadialParticleGroupTracks(pooled.tracks, pooled.billboard_turn_degrees);
      return;
    case PooledNodeShape::kFadingSprite:
      // ⚠ CE MOUVEMENT NE TOUCHE À AUCUNE PISTE, et c'est pourquoi il prend le
      // nœud entier : il mène son alpha, son image et sa position, puis fait
      // reculer sa traînée d'un cran.
      AdvanceFadingSpriteNode(pooled);
      return;
    case PooledNodeShape::kTeleportRings:
      // ⚠ CE MOUVEMENT NE SUIT PAS SON PORTEUR : le cas 57 ne recopie aucune
      // position dans le nœud. Les anneaux restent où la téléportation a eu
      // lieu, ce qui est le contraire de ce que fait celui du trente.
      AdvanceTeleportRingTracks(pooled.tracks);
      return;
    case PooledNodeShape::kRisingSparks:
      // ⚠ CE MOUVEMENT AUSSI SUIT LE PORTEUR : le client recopie la position de
      // l'effet dans le nœud à chaque tic, avant toute autre chose.
      pooled.ground_position = pooled.owner_position;
      // ⚠ ET IL TIRE UN CAP AU PREMIER TIC. La graine du nœud est celle-là même
      // que le semis lui a donnée : deux étincelles semées à la suite partent
      // donc dans deux directions, et la troupe entière se rejoue depuis la
      // graine d'origine.
      AdvanceRisingSparkTracks(pooled.tracks, pooled.billboard_turn_degrees,
                               pooled.billboard_tilt_degrees, pooled.node.wander_seed);
      return;
    case PooledNodeShape::kDualModeSprayRings:
      // ⚠ CE MOUVEMENT AUSSI SUIT LE PORTEUR, avant toute autre chose.
      pooled.ground_position = pooled.owner_position;
      // ⚠ LE MODE VIENT DE LA GRAINE : voir `EffectParticleRingStyleSeed::spray_mode`.
      AdvanceDualModeSprayTracks(pooled.tracks, pooled.spray_mode);
      return;
    case PooledNodeShape::kParticleConeRings:
      // ⚠ CE MOUVEMENT SUIT LE PORTEUR, sans condition : le client recopie sa
      // position dans le nœud à chaque tic, avant toute autre chose.
      pooled.ground_position = pooled.owner_position;
      AdvanceParticleConeSpawnTracks(pooled.tracks, pooled.node.life_ticks,
                                     static_cast<float>(pooled.trail_variant),
                                     pooled.waves_last_track);
      return;
    case PooledNodeShape::kSpiralFan:
      // ⚠ CE MOUVEMENT RANGE LES DEUX ANGLES DE CAMÉRA DANS LES PISTES, et la
      // figure les y relit. Voir `AdvanceFanBladeTracks`.
      AdvanceFanBladeTracks(pooled.tracks, pooled.billboard_turn_degrees,
                            pooled.billboard_tilt_degrees);
      // ⚠ ET UNE MARQUE SUR DEUX FAIT SUIVRE LE PORTEUR. Le client relit sa
      // position dans le nœud à chaque tic ; l'omettre planterait les sept
      // lunes là où le lanceur se tenait au premier tic.
      if (FanTracksFollowTheirOwner(pooled.tracks)) {
        pooled.ground_position = pooled.owner_position;
      }
      return;
    case PooledNodeShape::kCameraFan:
      // ⚠ MÊME FIGURE QUE L'ÉVENTAIL CI-DESSUS, AUTRE MOUVEMENT : quatre
      // constantes et un seuil les séparent. Voir `effect_camera_fan.h`.
      AdvanceCameraFanTracks(pooled.tracks, pooled.billboard_turn_degrees,
                             pooled.billboard_tilt_degrees);
      if (CameraFanFollowsItsOwner(pooled.tracks)) {
        pooled.ground_position = pooled.owner_position;
      }
      return;
    case PooledNodeShape::kScatterField:
      // ⚠ CE MOUVEMENT A BESOIN DU MONDE, seul de la famille : l'altitude du sol
      // pour poser ses particules sur le relief, et la carte courante pour
      // élargir sa dispersion. `render` ne les connaît pas — c'est `app` qui les
      // pose sur la troupe, comme les angles de la caméra.
      AdvanceScatteringParticleTracks(pooled.tracks, pooled.ground_position,
                                      pooled.billboard_turn_degrees, pooled.billboard_tilt_degrees,
                                      world, generator);
      return;
    case PooledNodeShape::kHomingTrail:
      // LES DEUX POINTS NE SONT PAS LE MÊME, et le mouvement les distingue : il
      // VISE le premier et VIRE vers le second.
      //
      // ⚠ LE POINT VISÉ EST LA CIBLE. Le bâtisseur recopie la position DEMANDÉE
      // à la création de l'effet dans le nœud, que le mouvement vise au premier
      // tic. Le bouclier part donc du lanceur et file vers sa victime,
      // puis revire vers celui qui l'a jeté.
      //
      // ⚠ ET LA TROUPE PORTE DÉJÀ LES DEUX. `target_position` est venu avec le
      // trait tendu, qui a le même besoin : aller d'un acteur à un autre. Sans
      // cible, on retombe sur le point du semis — le vol s'achève alors au
      // premier tic, ce qui est exactement ce que fait le client quand sa
      // recherche d'acteur échoue.
      AdvanceHomingTrailTracks(pooled.tracks, pooled.owner_position,
                               pooled.target_position.value_or(pooled.ground_position),
                               pooled.billboard_turn_degrees, pooled.billboard_tilt_degrees,
                               generator);
      return;
    case PooledNodeShape::kCyclingSkirt:
      // ⚠ CE MOUVEMENT NE SUIT PAS SON PORTEUR : le cas 144 recopie bien sa
      // position dans le noeud, mais la jupe reste ou l'effet est ne — comme
      // celle du cent trente-neuf dont il reprend le corps.
      pooled.ground_position = pooled.owner_position;
      AdvanceGravityWaveTracks(pooled.tracks, pooled.node.life_ticks);
      // L'IMAGE EST CELLE DU COMPTEUR DE LA PISTE, modulo le nombre d'images :
      // une image par tic, et non une tous les quatre.
      pooled.node.frame = pooled.tracks.front().ticks;
      return;
    case PooledNodeShape::kGridPlane:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR et range les deux angles de camera dans
      // les pistes, que la figure y relit.
      pooled.ground_position = pooled.owner_position;
      AdvanceParticleFrameTracks(pooled.tracks, pooled.billboard_turn_degrees,
                                 pooled.billboard_tilt_degrees);
      return;
    case PooledNodeShape::kGrowingSprite:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR et ne tire rien : tout son hasard a ete
      // consomme au semis, dans la hauteur du style qui tire.
      pooled.ground_position = pooled.owner_position;
      AdvanceGrowingSpriteTracks(pooled.tracks, pooled.trail_variant);
      return;
    case PooledNodeShape::kAuraColumn:
      // ⚠ CE MOUVEMENT NE SUIT SON PORTEUR QUE SI LA MARQUE DE FORME VAUT UN, et
      // le client teste la marque de la PREMIERE piste pour les quatre. Nos
      // batisseurs y posent un, si bien qu il le suit.
      if (pooled.tracks.front().mark[kMarkOfAuraColumnShape] == kAuraColumnShapeMark) {
        pooled.ground_position = pooled.owner_position;
      }
      AdvanceRingExpansionTracks(pooled.tracks, pooled.billboard_turn_degrees,
                                 pooled.billboard_tilt_degrees);
      return;
    case PooledNodeShape::kRainParticle:
      // ⚠ CE MOUVEMENT NE SUIT PAS SON PORTEUR, et c'est le seul de sa
      // generation : le cas du repartiteur appelle la fonction sans recopier la
      // position. Chaque bouffee reste ou son emetteur l'a semee ; la faire
      // suivre collerait toute la pluie au joueur.
      AdvanceCameraFacingRadialTracks(pooled.tracks, pooled.billboard_turn_degrees,
                                      pooled.billboard_tilt_degrees);
      return;
    case PooledNodeShape::kFadingStreak:
      // ⚠ CE MOUVEMENT NE SUIT PAS SON PORTEUR : le flocon part du point ou le
      // sort a ete lance, et y reste accroche par sa figure.
      AdvanceFadingStreakTracks(pooled.tracks, pooled.billboard_turn_degrees,
                                pooled.billboard_tilt_degrees);
      return;
    case PooledNodeShape::kRainbowTrail:
      // ⚠ CE MOUVEMENT NE SUIT RIEN : ni le porteur, ni la camera. L'arc garde le
      // plan que son batisseur lui a donne.
      AdvanceCameraAlignedArcTracks(pooled.tracks);
      return;
    case PooledNodeShape::kSpiralTrail:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR ET TIRE : ses pistes renaissent au deux
      // millieme tic, et retirent alors leur direction et leur rayon.
      pooled.ground_position = pooled.owner_position;
      AdvanceSwirlOrbitTracks(pooled.tracks, pooled.billboard_turn_degrees,
                              pooled.billboard_tilt_degrees, pooled.ground_position, generator);
      return;
    case PooledNodeShape::kDoubleRibbon:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR et lui demande son ANGLE, non celui de
      // la camera : l'anneau se dresse derriere lui et ne tourne pas quand le
      // joueur fait pivoter l'ecran.
      pooled.ground_position = pooled.owner_position;
      AdvanceRadialFanTracks(pooled.tracks, pooled.owner_facing_degrees);
      return;
    case PooledNodeShape::kSpikeBeam:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR et ne tire rien. Il lui faut le NOMBRE
      // d images du noeud : c est la borne ou l indice revient a zero.
      pooled.ground_position = pooled.owner_position;
      AdvanceFourLayerTimerTracks(pooled.tracks, pooled.resource_paths.size());
      return;
    case PooledNodeShape::kPieWedge:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR et ne tire rien : le client recopie sa
      // position avant d'avancer les compteurs.
      pooled.ground_position = pooled.owner_position;
      AdvanceLiveTrackTicks(pooled.tracks);
      return;
    case PooledNodeShape::kThrownItem:
      // ⚠ CE MOUVEMENT NE SUIT PAS SON PORTEUR ET NE TIRE RIEN. Le client recopie
      // bien la position du lanceur dans le nœud, mais la tête vole depuis SON
      // ancre : la recopie ne sert qu'à orienter le dessin, et l'appliquer au
      // point du sol ferait suivre l'objet au lieu de le laisser partir.
      //
      // SANS CIBLE, LE VOL S'ACHÈVE AU PREMIER TIC, ce qui est exactement ce que
      // fait le client quand sa recherche d'acteur échoue.
      AdvanceThrownItemTracks(pooled.tracks,
                              pooled.target_position.value_or(pooled.ground_position),
                              pooled.billboard_turn_degrees, pooled.billboard_tilt_degrees);
      return;
    case PooledNodeShape::kHealSparkle:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR et ne travaille que sur la piste ZÉRO :
      // le client écrit à des places FIXES du nœud, que sa boucle des quatre
      // pistes ne décale pas. Une boucle sur les quatre ferait
      // paraître quatre étincelles là où il n'y en a qu'une.
      pooled.ground_position = pooled.owner_position;
      AdvanceHealSparkleTracks(pooled.tracks, pooled.billboard_turn_degrees,
                               pooled.billboard_tilt_degrees);
      return;
    case PooledNodeShape::kRisingQuads:
      // Il suit son porteur, et c'est de sa hauteur qu'une piste éteinte repart.
      pooled.ground_position = pooled.owner_position;
      AdvanceRisingQuadTracks(pooled.tracks, pooled.owner_position.y, pooled.billboard_turn_degrees,
                              pooled.billboard_tilt_degrees);
      return;
    case PooledNodeShape::kTintedQuads:
      // ⚠ LES QUATRE PISTES ENSEMBLE, et non chacune son tour : la cascade les
      // fait se passer leur état, si bien qu'une boucle piste par piste
      // copierait la tête dans les quatre d'un coup.
      //
      // ⚠ ET LE PROPRIÉTAIRE EST LE POINT DU SEMIS, faute de mieux. Le client
      // recale la tête sur un acteur qui MARCHE ; nos nœuds gardent leur point
      // de naissance, pour toute la famille. L'effet ne suivra donc pas encore
      // celui qui le porte.
      AdvanceCascadingTrailTracks(pooled.tracks, pooled.ground_position,
                                  pooled.billboard_turn_degrees, pooled.billboard_tilt_degrees);
      return;
    case PooledNodeShape::kScrollingBand:
      // ⚠ NI LA CAMÉRA, NI LE PORTEUR, NI LE MONDE — seul mouvement de la
      // famille à ne rien demander de tel. Une cascade est posée sur la carte et
      // n'appartient à personne : la faire suivre un acteur la décrocherait de
      // la falaise où elle coule.
      AdvanceLiveTrackTicks(pooled.tracks);
      return;
    case PooledNodeShape::kDarkVeil:
      // ⚠ LE VOILE SUIT CELUI QUI LE PORTE : le client recopie la position de
      // son propriétaire dans le nœud à chaque tic. L'omettre laisserait le
      // joueur sortir de sa propre ombre en trois pas.
      pooled.ground_position = pooled.owner_position;
      AdvanceDarkVeilTracks(pooled.tracks);
      return;
    case PooledNodeShape::kTypedRibbon:
      // ⚠ CE MOUVEMENT SUIT LE PORTEUR, et il a besoin de la VIE DU NŒUD : c'est
      // elle qui lui dit quand commencer à éteindre. Le seul de la famille à la
      // consulter.
      pooled.ground_position = pooled.owner_position;
      AdvanceSpinningRibbonTracks(pooled.tracks, pooled.node.life_ticks);
      return;
    case PooledNodeShape::kPentagonTrail:
      // ⚠ CE MOUVEMENT NE SUIT PAS SON PORTEUR, seul avec la cascade : les deux
      // centres du tube sont posés une fois pour toutes par le semis. Le faire
      // suivre étirerait la traînée derrière celui qui l'a lancée.
      AdvancePentagonTrailTracks(pooled.tracks);
      return;
    case PooledNodeShape::kPaletteBillboard:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR et TIRE AU SORT : ses pistes renaissent
      // à un azimut nouveau. La graine vit dans la troupe, si bien qu'un effet
      // entier se rejoue depuis la sienne.
      pooled.ground_position = pooled.owner_position;
      AdvancePaletteBillboardTracks(pooled.tracks, generator);
      return;
    case PooledNodeShape::kSpiralStack:
      // ⚠ CE MOUVEMENT TIRE À CHAQUE TIC, et pas seulement au semis : son pas de
      // rotation est aléatoire. La graine vit dans la troupe, si bien qu'un
      // tourbillon rejoué depuis la même graine tourne de la même façon.
      AdvanceSpiralStackTracks(pooled.tracks, generator);
      return;
    case PooledNodeShape::kBreathingQuad:
    case PooledNodeShape::kFootprint:
      // ⚠ CES MOUVEMENTS SUIVENT LEUR PORTEUR et NE TIRENT RIEN — l'empreinte, celui
      // d'une instance posée sur le pas, qui ne bouge pas.
      pooled.ground_position = pooled.owner_position;
      pooled.shape == PooledNodeShape::kFootprint ? AdvanceFootprintTracks(pooled.tracks)
                                                  : AdvanceBreathingQuadTracks(pooled.tracks);
      return;
    case PooledNodeShape::kGroundDisc:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR et ne tire rien : le cas du répartiteur
      // recopie la position avant de tourner, et son semis n'a rien tiré non
      // plus. Un cercle magique rejoué depuis n'importe quelle graine est le
      // même.
      pooled.ground_position = pooled.owner_position;
      AdvanceGroundDiscTracks(pooled.tracks);
      return;
    case PooledNodeShape::kConeWedge:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR et ne tire rien : son hasard a été
      // consommé au semis, dans l'azimut de chaque piste.
      pooled.ground_position = pooled.owner_position;
      AdvanceConeWedgeTracks(pooled.tracks);
      return;
    case PooledNodeShape::kCrossSpikes:
      // ⚠ CELLE-CI N'A PAS DE MOUVEMENT DE PISTE, et c'est une DÉCISION, pas un
      // oubli : sa figure tire tout des champs du nœud et ne lit qu'une marque,
      // que son bâtisseur pose une fois pour toutes. Elle était jusqu'ici
      // absente de ce switch, et le compilateur ne l'a pas dit — une forme
      // oubliée y serait AVALÉE EN SILENCE, ce que le default supprimé devait
      // justement empêcher. L'écrire ici rend le silence lisible.
      return;
    case PooledNodeShape::kTripleWall:
      // ⚠ CE MOUVEMENT RECOPIE LA PLACE DE SON INSTANCE à chaque tic. C'est
      // l'horaire de l'instance qui décide si elle suit le porteur.
      pooled.ground_position = pooled.owner_position;
      AdvanceTripleWallTracks(pooled.tracks);
      return;
    case PooledNodeShape::kConcentricSquare:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR et veut DEUX ANGLES DE CAMÉRA, que la
      // troupe reçoit déjà pour les billboards. Le champ du nœud qui le fait
      // plonger n'est pas connu par identifiant : on le laisse absent, ce qui
      // donne la variante qui NE plonge pas — celle dont la hauteur et le rayon
      // sont fixes. C'est le cas le plus sobre, et il ne masque rien.
      pooled.ground_position = pooled.owner_position;
      AdvanceOrbitJitterTracks(pooled.tracks, pooled.billboard_turn_degrees,
                               pooled.billboard_tilt_degrees, false, generator);
      return;
    case PooledNodeShape::kSpiralColumn:
      // ⚠ CE MOUVEMENT NE SUIT PAS SON PORTEUR : le bâtisseur recopie la
      // position une fois, au semis, et la colonne reste où elle est née. La
      // faire suivre traînerait un pilier de feu derrière celui qui l'a levé.
      AdvanceSpiralColumnTracks(pooled.tracks);
      return;
    case PooledNodeShape::kPulsingRings:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR : le client recopie la position du
      // propriétaire dans le nœud avant d'avancer les pistes, et c'est ainsi
      // qu'un portail reste sous celui qui l'a ouvert.
      pooled.ground_position = pooled.owner_position;
      AdvanceRadialBurstTracks(pooled.tracks);
      return;
    case PooledNodeShape::kWarpRings:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR, comme celui du sous-type 63 : le
      // client recopie la position du propriétaire dans le nœud avant
      // d'avancer les pistes.
      //
      // ⚠ ET IL A BESOIN DE LA VIE DU NŒUD, que les autres n'exigent pas :
      // l'alpha des quatre anneaux retombe dans les quarante derniers tics, et
      // c'est la seule façon qu'a la figure de s'éteindre — son rayon, lui, ne
      // s'arrête jamais.
      pooled.ground_position = pooled.owner_position;
      AdvanceWarpRingTracks(pooled.tracks, pooled.node.life_ticks);
      return;
    case PooledNodeShape::kDriftingMote:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR et TIRE À CHAQUE TIC : ses deux paires
      // d'angles renouvellent leur cible dès qu'elles l'atteignent.
      pooled.ground_position = pooled.owner_position;
      AdvanceDriftingMoteTracks(pooled.tracks, pooled.billboard_turn_degrees,
                                pooled.billboard_tilt_degrees, generator);
      return;
    case PooledNodeShape::kDriftingTriPlane:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR et TIRE À CHAQUE TIC : ses six paires
      // d'angles renouvellent leur cible dès qu'elles l'atteignent.
      pooled.ground_position = pooled.owner_position;
      AdvanceDriftingTriPlaneTracks(pooled.tracks, pooled.billboard_turn_degrees,
                                    pooled.billboard_tilt_degrees, generator);
      return;
    case PooledNodeShape::kFlyingTriPlane:
      // ⚠ CELUI-CI SUIT AUSSI SON PORTEUR, mais ne tire rien : ses caps sont
      // posés au semis et ne changent plus.
      pooled.ground_position = pooled.owner_position;
      AdvanceFlyingTriPlaneTracks(pooled.tracks, pooled.billboard_turn_degrees,
                                  pooled.billboard_tilt_degrees);
      return;
    case PooledNodeShape::kHomingSpark:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR : le client recopie sa position dans
      // le nœud avant d'avancer les pistes, et c'est ainsi que la charge reste
      // autour de celui qui la lève.
      pooled.ground_position = pooled.owner_position;
      AdvanceHomingSparkTracks(pooled.tracks);
      return;
    case PooledNodeShape::kLightningFan:
      // ⚠ CE MOUVEMENT NE SUIT PAS SON PORTEUR, et il TIRE : son alpha monte
      // d'un pas tiré au sort tant que le vingt-cinquième tic n'est pas passé.
      AdvanceLightningFanTracks(pooled.tracks, generator);
      return;
    case PooledNodeShape::kAnchoredSkirt:
      // ⚠ CE MOUVEMENT NE SUIT PAS SON PORTEUR : les deux bâtisseurs recopient
      // la position une fois, et la gerbe dérive d'elle-même.
      AdvanceAnchoredSkirtTracks(pooled.tracks);
      return;
    case PooledNodeShape::kRingQuartet:
    case PooledNodeShape::kRingStarburst:
    case PooledNodeShape::kTurningRing:
      // LES ANNEAUX QUI SUIVENT LEUR PORTEUR : voir `effect_node_motion_rings.cpp`.
      AdvanceFollowingRingTracksOf(pooled);
      return;
    case PooledNodeShape::kRibbonWave:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR et RÉÉCRIT ses longueurs à chaque tic,
      // au lieu de les ouvrir une fois. Voir `AdvanceRibbonWaveTracks`.
      pooled.ground_position = pooled.owner_position;
      AdvanceRibbonWaveTracks(pooled.tracks);
      return;
    case PooledNodeShape::kTwinDisc:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR PAR L'INSTANCE, que le bâtisseur recale
      // sur le porteur à chaque tic. C'est le pas que la troupe applique à
      // `ground_position` (`MoveAnchored`) ; le reposer sur `owner_position`,
      // que rien ne rafraîchit, ramènerait les disques là où ils sont nés. Il
      // ne touche ensuite que les DEUX premières inclinaisons, qui mènent le
      // souffle.
      AdvanceTwinDiscTracks(pooled.tracks);
      return;
    case PooledNodeShape::kCloudRing:
      // ⚠ CE MOUVEMENT OUVRE LE BALAYAGE au lieu de faire onduler les
      // longueurs : le ruban se déroule. Voir `AdvanceCloudRingTracks`.
      AdvanceCloudRingTracks(pooled.tracks);
      return;
    case PooledNodeShape::kSpriteFan:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR, et c'est tout ce qu'il fait de plus
      // que d'écarter ses pistes : les autres branches du mouvement générique
      // du client sont gardées par des bits que son bâtisseur ne pose pas.
      pooled.ground_position = pooled.owner_position;
      AdvanceSpriteFanTracks(pooled.tracks);
      return;
    case PooledNodeShape::kLayeredAura:
    case PooledNodeShape::kPlainAura:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR et TIRE : chaque particule qui meurt
      // renaît sous un autre angle, sur une autre image et à une autre
      // hauteur. Les deux sous-types le partagent à l'identique.
      pooled.ground_position = pooled.owner_position;
      AdvanceJobAuraTracks(pooled.tracks, pooled.billboard_turn_degrees,
                           pooled.billboard_tilt_degrees, pooled.owner_aura_height, generator);
      return;
    case PooledNodeShape::kWanderingMote:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR et TIRE À CHAQUE TIC : huit paires
      // d'angles, quatre replacements et quatre scintillements.
      pooled.ground_position = pooled.owner_position;
      AdvanceWanderingMoteTracks(pooled.tracks, pooled.billboard_turn_degrees,
                                 pooled.billboard_tilt_degrees, generator);
      return;
    case PooledNodeShape::kSpinningRound:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR et TIRE À CHAQUE TIC : ses deux
      // paires d'angles renouvellent leur cible dès qu'elles l'atteignent.
      pooled.ground_position = pooled.owner_position;
      AdvanceSpinningRoundTracks(pooled.tracks, pooled.billboard_turn_degrees,
                                 pooled.billboard_tilt_degrees, generator);
      return;
    case PooledNodeShape::kSnakingArc:
      // ⚠ CE MOUVEMENT NE SUIT PAS SON PORTEUR : le bâtisseur recopie la
      // position une fois, au semis, et l'arc reste accroché là. Le faire
      // suivre traînerait l'éclair derrière celui qui l'a lancé.
      AdvanceSnakingArcTracks(pooled.tracks);
      return;
    case PooledNodeShape::kSpinRibbon:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR : le client recopie sa position
      // dans le nœud avant de faire tourner les rubans.
      pooled.ground_position = pooled.owner_position;
      AdvanceSpinRibbonTracks(pooled.tracks);
      return;
    case PooledNodeShape::kRisingSpinRibbon:
      // ⚠ CELUI-CI SUIT AUSSI SON PORTEUR, ET IL TIRE À CHAQUE TIC : son
      // inclinaison avance d'un degré ou de deux, au sort.
      pooled.ground_position = pooled.owner_position;
      AdvanceRisingSpinRibbonTracks(pooled.tracks, generator);
      return;
    case PooledNodeShape::kTetheredBeam:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR ET SA CIBLE : le client recopie les
      // deux positions dans le nœud à chaque tic, et c'est ce qui garde le
      // trait accroché à ses deux bouts.
      pooled.ground_position = pooled.owner_position;
      AdvanceTetheredBeamTracks(pooled.tracks, pooled.owner_position, pooled.target_position);
      return;
    case PooledNodeShape::kGroundGust:
      // ⚠ CE MOUVEMENT NE SUIT PAS SON PORTEUR : le bâtisseur recopie la
      // position une fois, au semis, et la bourrasque part de là. La faire
      // suivre la traînerait derrière celui qui l'a lancée au lieu de
      // l'envoyer vers sa cible.
      AdvanceGroundGustTracks(pooled.tracks);
      return;
    case PooledNodeShape::kHomingGustSteered:
      // ⚠ CES DEUX-LÀ NE SUIVENT PAS LEUR PORTEUR NON PLUS, mais ils relisent
      // LEUR CIBLE à chaque tic : le client cherche l'acteur par son GID, et
      // n'en prend que la position. La place du nœud reste celle du semis —
      // c'est d'elle que part le premier cap, et c'est là que le 122 rentre.
      AdvanceHomingGustSteeredTracks(pooled.tracks, pooled.ground_position, pooled.target_position);
      return;
    case PooledNodeShape::kHomingGustSpread:
      AdvanceHomingGustSpreadTracks(pooled.tracks, pooled.ground_position, pooled.target_position,
                                    generator);
      return;
    case PooledNodeShape::kRollingGust:
      // ⚠ CELUI-CI NE SUIT RIEN : son cap se prend au semis, comme le 119.
      AdvanceRollingGustTracks(pooled.tracks);
      return;
    case PooledNodeShape::kGroundBox:
      // ⚠ CE MOUVEMENT NE SUIT PAS SON PORTEUR : c'est une marque POSÉE AU
      // SOL, pas une aura. La faire suivre la traînerait sous les pas de
      // celui qui l'a posée.
      AdvanceGroundBoxTracks(pooled.tracks);
      return;
    case PooledNodeShape::kFrameAndBar:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR, et il a besoin de savoir OÙ LE NŒUD
      // ÉTAIT pour s'en apercevoir : c'est la différence entre les deux qui
      // coupe la marque quand son porteur s'en va. Recopier la position avant
      // de l'avancer effacerait le seul témoin.
      AdvanceFrameAndBarTracks(pooled.tracks, pooled.ground_position, pooled.owner_position);
      pooled.ground_position = pooled.owner_position;
      return;
    case PooledNodeShape::kSheathedBolt:
      // ⚠ CE MOUVEMENT NE SUIT PAS SON PORTEUR : le bâtisseur recopie la
      // position une fois, au semis. Le faire suivre traînerait le trait
      // derrière celui qui l'a lancé, alors qu'il est planté où il est né.
      AdvanceSheathedBoltTracks(pooled.tracks);
      return;
    case PooledNodeShape::kFlyingShard:
      // ⚠ CE MOUVEMENT NE SUIT PAS SON PORTEUR : l'éclat part et s'éloigne. Le
      // faire suivre le ramènerait sous celui qui l'a reçu à chaque tic.
      AdvanceFlyingShardTracks(pooled.tracks);
      return;
    case PooledNodeShape::kQuadBurst:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR et TIRE À CHAQUE TIC : chaque fois
      // qu'un angle atteint sa cible, une neuve est tirée, et chaque fois
      // qu'une particule tombe trop bas elle renaît ailleurs.
      pooled.ground_position = pooled.owner_position;
      AdvanceQuadBurstTracks(pooled.tracks, pooled.billboard_turn_degrees,
                             pooled.billboard_tilt_degrees, generator);
      return;
    case PooledNodeShape::kGlidingSlash:
    case PooledNodeShape::kAimedGlidingSlash:
      // ⚠ CE MOUVEMENT NE SUIT PAS SON PORTEUR : la taillade part et file. Il
      // vise la cible que l'appelant a posée au semis ; sans elle, le vecteur
      // nul donne un cap de zéro — arbitraire, jamais inventé.
      AdvanceGlidingSlashTracks(
          pooled.tracks, pooled.shape == PooledNodeShape::kAimedGlidingSlash,
          pooled.target_position.value_or(pooled.ground_position) - pooled.ground_position,
          generator);
      return;
    case PooledNodeShape::kSparkTube:
      // ⚠ CE MOUVEMENT REÇOIT LA POSITION DU PORTEUR et la range dans CHAQUE
      // piste, au lieu de la poser sur le nœud : le dessinateur pose ses coins
      // en absolu, et c'est l'ancre de la piste qui les situe. La poser sur le
      // nœud ne servirait à rien, puisque rien ne la lirait.
      pooled.ground_position = pooled.owner_position;
      AdvanceSparkTubeTracks(pooled.tracks, pooled.owner_position);
      return;
    case PooledNodeShape::kGroundCurtain:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR : le client recopie sa position dans le
      // nœud avant d'avancer les pistes, et c'est ainsi qu'une marque de chant
      // reste sous le barde qui la tient.
      pooled.ground_position = pooled.owner_position;
      AdvanceGroundCurtainTracks(pooled.tracks);
      return;
    case PooledNodeShape::kBillboardSwarm:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR et TIRE À CHAQUE TIC : chaque fois
      // qu'une mouche atteint sa cible, une neuve est tirée. La graine vit dans
      // la troupe, si bien qu'un essaim rejoué depuis la même graine poursuit
      // de la même façon.
      //
      // ⚠ ET LE RÉGLAGE SE RELIT DANS LES PISTES : le large en allume QUATRE,
      // l'étroit une seule. Ranger le genre dans le nœud le dirait deux fois,
      // et les deux pourraient diverger.
      AdvanceBillboardSwarmTracks(
          pooled.tracks, pooled.tracks[1].active ? kEffectWideSwarm : kEffectNarrowSwarm,
          pooled.billboard_turn_degrees, pooled.billboard_tilt_degrees, generator);
      pooled.ground_position = pooled.owner_position;
      return;
    case PooledNodeShape::kTurningArch:
      AdvanceTurningArchTracks(pooled.tracks, pooled.node.life_ticks);
      return;
    case PooledNodeShape::kSpiralParticles:
      // CE MOUVEMENT SUIT SON PORTEUR, dont il recopie la position à chaque tic.
      pooled.ground_position = pooled.owner_position;
      AdvanceSpiralParticleTracks(pooled.tracks, generator);
      return;
    case PooledNodeShape::kRadialSpreadParticles:
      AdvanceRadialSpreadParticleTracks(pooled.tracks);
      return;
    case PooledNodeShape::kOscillatingRadiusParticles:
      // ⚠ IL REÇOIT L'ÉCHELLE DU NŒUD EN RÉFÉRENCE, et c'est le seul mouvement
      // de cette famille à écrire hors des pistes.
      AdvanceOscillatingRadiusParticleTracks(pooled.tracks, pooled.node.life_ticks,
                                             pooled.trail_variant);
      return;
    case PooledNodeShape::kGrandCrossRibbon:
      AdvanceGrandCrossRibbonTracks(pooled.tracks);
      return;
    case PooledNodeShape::kGrandCrossFrame:
      AdvanceGrandCrossFrameTracks(pooled.tracks);
      return;
    case PooledNodeShape::kSphereShell:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR EN POSITION ET EN CAP : le client
      // recopie sa position dans le nœud, puis relit son angle pour en refaire
      // l'azimut. Le bouclier reste donc devant celui qui le porte, et pivote
      // avec lui.
      AdvanceSphereShellTracks(pooled.tracks, pooled.owner_facing_degrees);
      pooled.ground_position = pooled.owner_position;
      return;
    case PooledNodeShape::kGroundRibbon:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR : le client recopie sa position dans le
      // nœud avant d'avancer les pistes.
      pooled.ground_position = pooled.owner_position;
      AdvanceGroundRibbonTracks(pooled.tracks);
      return;
    case PooledNodeShape::kMagicTargetGrid:
      // ⚠ CE MOUVEMENT NE SUIT PAS SON PORTEUR : son cas du répartiteur ne
      // recopie aucune position. La grille de visée est posée là où elle est
      // née, et celui qui l'a lancée s'en va sans elle.
      AdvanceMagicTargetGridTracks(pooled.tracks);
      return;
    case PooledNodeShape::kSwirlTrail:
      // ⚠ CE MOUVEMENT RECOPIE LE PORTEUR DANS LE NŒUD, mais aucune piste ne le
      // relit : la meneuse file depuis l'ancre posée au semis. La place du nœud
      // n'est donc pas touchée ici.
      AdvanceSwirlTrailTracks(pooled.tracks, pooled.billboard_turn_degrees,
                              pooled.billboard_tilt_degrees);
      return;
    case PooledNodeShape::kDriftingIcons:
      // ⚠ MÊME RECOPIE DU PORTEUR, que rien ne relit : les icônes restent où
      // elles sont nées et dérivent de là. Le mouvement TIRE quand un angle
      // atteint sa cible — la graine du nœud le rejoue.
      AdvanceDriftingIconTracks(pooled.tracks, pooled.billboard_turn_degrees,
                                pooled.billboard_tilt_degrees, pooled.node.wander_seed);
      return;
    case PooledNodeShape::kElectricArc:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR, et c'est la place du NŒUD qu'il lit :
      // celle que la troupe translate du pas du porteur (`MoveAnchored`).
      // `owner_position`, que rien ne rafraîchit, laisserait l'arc là où il est
      // né.
      AdvanceElectricArcTracks(pooled.tracks, pooled.ground_position);
      return;
    case PooledNodeShape::kFadingTwinDisc:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR PAR L'INSTANCE : il recopie la place de
      // l'instance dans le nœud, et le bâtisseur, appelé à chaque tic par le
      // cas, recale l'instance sur son porteur avant sa garde d'âge. C'est le
      // pas que la troupe applique à `ground_position` (`MoveAnchored`) ; le
      // reposer sur `owner_position`, que rien ne rafraîchit, ramènerait les
      // disques là où ils sont nés.
      AdvanceFadingTwinDiscTracks(pooled.tracks, pooled.node.life_ticks);
      return;
    case PooledNodeShape::kExpandingRing:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR PAR L'INSTANCE : le cas 169 recopie la
      // place de l'instance dans le nœud, que le bâtisseur, rappelé à chaque
      // tic, recale sur le porteur. `MoveAnchored` translate `owner_position`
      // du pas du porteur.
      pooled.ground_position = pooled.owner_position;
      AdvanceExpandingRingTracks(pooled.tracks);
      return;
    case PooledNodeShape::kRainstormCylinder:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR PAR L'INSTANCE : il recopie la place de
      // l'instance dans le nœud, que le bâtisseur, rappelé à chaque tic, recale
      // sur le porteur. `MoveAnchored` translate `owner_position` du pas du
      // porteur.
      pooled.ground_position = pooled.owner_position;
      AdvanceRainstormCylinderTracks(pooled.tracks, pooled.node.life_ticks);
      // L'IMAGE EST CELLE DU COMPTEUR, modulo le nombre d'images :
      // les quatre pistes naissent au même compteur et avancent ensemble, si
      // bien que le lot unique du nœud porte l'image de chacune.
      pooled.node.frame = pooled.tracks.front().ticks;
      return;
    case PooledNodeShape::kThunderScatter:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR PAR L'INSTANCE : il recopie la place de
      // l'instance dans le nœud, que le bâtisseur, rappelé à chaque tic, recale
      // sur le porteur. `MoveAnchored` translate `owner_position` du pas du
      // porteur. Seul le cap en dépend : les ancres sont déjà dans le monde.
      pooled.ground_position = pooled.owner_position;
      AdvanceThunderScatterTracks(pooled.tracks, pooled.ground_position,
                                  pooled.target_position.value_or(pooled.ground_position),
                                  pooled.billboard_turn_degrees, pooled.billboard_tilt_degrees,
                                  generator);
      return;
    case PooledNodeShape::kThunderFreeze:
      // ⚠ CE MOUVEMENT SUIT SON PORTEUR PAR L'INSTANCE : il recopie la place de
      // l'instance dans le nœud, que le bâtisseur, rappelé à chaque tic, recale
      // sur le porteur. `MoveAnchored` translate `owner_position` du pas du
      // porteur.
      pooled.ground_position = pooled.owner_position;
      AdvanceThunderFreezeTracks(pooled.tracks, pooled.billboard_turn_degrees,
                                 pooled.billboard_tilt_degrees, generator);
      return;
    case PooledNodeShape::kTetraVortex:  // le nombre d'images du nœud
      AdvanceTetraVortexTracks(pooled.tracks, pooled.billboard_turn_degrees,
                               pooled.billboard_tilt_degrees,
                               static_cast<std::int32_t>(pooled.resource_paths.size()));
      return;
    case PooledNodeShape::kAimedSpokes:  // ne recopie PAS la place du porteur
      AdvanceAimedSpokeTracks(pooled.tracks);
      return;
    // CE QUI NE TIRE RIEN : voir `effect_node_motion_panels.cpp`.
    case PooledNodeShape::kBreakRings:
    case PooledNodeShape::kSpinningBox:
    case PooledNodeShape::kRadialPetal:
    case PooledNodeShape::kArcingLight:
    case PooledNodeShape::kShockwaveRays:
    case PooledNodeShape::kTaperedSheets:
    case PooledNodeShape::kSpiralEmitter:
    case PooledNodeShape::kWalkingSower:
    case PooledNodeShape::kFloorSpinner:
    case PooledNodeShape::kBobbingNote:
    case PooledNodeShape::kColoredVeil:
      AdvancePanelTracksOf(pooled);
      return;
  }
}

}  // namespace ro::render
