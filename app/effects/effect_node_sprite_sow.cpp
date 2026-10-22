// LE SEMIS DES SPRITES D'UNE TROUPE : un nœud de sprite naît ici, qu'il vienne
// d'un patron à son tic, du banc, d'un tir ou d'une animation jouée une fois.
//
// La fenêtre (`effect_node_view.cpp`) garde ce qu'elle charge et ce qu'elle
// répartit entre les familles ; ce fichier-ci ne fait que POSER des sprites —
// leur place, leur lancer, leur voie — dans la troupe.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "ro/formats/act.h"
#include "ro/render/effect_chased_drift.h"
#include "ro/render/effect_launch.h"
#include "ro/render/effect_marching_sower.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_sprite_seed.h"
#include "ro/render/effect_ring_flight.h"
#include "ro/render/effect_trail_history.h"
#include "ro/render/element_glow.h"
#include "ro/render/wand_orb.h"

#include "effects/effect_node_view.h"

namespace moonclient {
namespace {

// Le dossier des sprites d'EFFET, celui où le client range ceux de ses nœuds :
// « 이팩트 » veut dire effet.
constexpr std::string_view kEffectSpriteFolder = "data\\sprite\\이팩트\\";

// Le patron d'une troupe, celui des particules de Heal : cinquante tics de vie,
// une échelle de cinquante-six centièmes, une sortie sur le dernier tiers.
//
// Le client ne pose sur ces nœuds-là ni plafond ni montée d'alpha : ils
// paraissent d'emblée, et seule la sortie les efface.
constexpr std::int32_t kBenchLifeTicks = 50;
constexpr std::int32_t kBenchFadeOutTicks = kBenchLifeTicks / 3;
constexpr float kBenchSpriteScale = 0.56F;
// Combien de nœuds la troupe porte, et sur quel rayon ils se posent. Le client
// en sème un tous les neuf tics tant que l'effet dure ; le banc les pose tous
// d'un coup, ce qui suffit à juger la taille, la teinte et le fondu.
constexpr float kBenchScatterWorldUnits = 3.0F;

// L'angle de base d'un semis SANS PORTEUR — le banc, le juge, une commande.
//
// ⚠ CE N'EST PAS « pas d'orientation », et la nuance décide de ce qu'on voit :
// l'angle de base vaut zéro pour un porteur tourné vers le NORD, si bien qu'un
// semis sans porteur s'ouvre exactement comme celui d'un acteur en direction
// zéro. C'est aussi ce qui rend le juge reproductible.
constexpr float kNoOwnerFacingDegrees = ro::render::EffectOwnerFacingDegrees(0);

// COMBIEN de nœuds une troupe porte, et à quelle hauteur ils se posent.
//
// ⚠ NI L'UN NI L'AUTRE NE VIENT DU CLIENT. Le client sème un nœud tous les
// quelques tics tant que l'effet dure, et nous les posons tous d'un coup ; la
// hauteur vient de ce qui se voit. Ce sont nos valeurs, pas les siennes.
//
// ⚠ ET LE COMPTE D'EXEMPLAIRES NE REMPLACE PAS CELUI-CI — il passe DEVANT quand
// il existe. Ce sont deux questions différentes : `EffectNodePattern::copies`
// dit combien de nœuds le client sème EN UNE PASSE, tel que le fixe la boucle
// qui entoure son bâtisseur ; ce nombre-ci tient lieu des passes SUCCESSIVES,
// que nous ne jouons pas. Les confondre ramènerait toute troupe à un seul nœud,
// et chaque nuée deviendrait une particule.
//
// Ce nombre-ci ne sert donc plus qu'aux patrons dont la boucle est inconnue —
// trente des cinquante-deux, qui rendent un exemplaire sans qu'on sache si
// c'est un compte ou une absence.
constexpr std::size_t kNodesPerGroup = 12;

// Le tour complet, en degrés : chaque nœud tire son azimut dedans.
constexpr float kFullTurnDegrees = 359.0F;
// Le quart de tour qui sépare le cap du client de celui d'une orbite.
constexpr float kQuarterTurnDegrees = 90.0F;

float NodeRadians(float degrees) { return degrees * std::numbers::pi_v<float> / 180.0F; }

}  // namespace

bool EffectNodesInPlay::Sow(const std::string& sprite_path, const ro::render::EffectNodeSeed& seed,
                            ro::render::EffectNodeRange sprite_scale, float scatter,
                            glm::vec3 ground_position, std::size_t count,
                            float owner_facing_degrees,
                            const std::optional<glm::vec3>& target_position) {
  const ro::render::LoadedActorSprite* charge = LoadedSprite(sprite_path);
  if (charge == nullptr) {
    return false;
  }
  // OÙ LE NŒUD NAÎT ET CE QU'IL VISE : un vol retourné part de la cible.
  const ro::render::EffectFlightEnds bouts =
      ro::render::EffectNodeFlightEnds(seed, ground_position, target_position);
  // 🔴 LE SPRITE D'UN NŒUD S'ANIME : le client avance l'image de son action à
  // chaque délai d'image écoulé, et la ramène à zéro au bout. Le compte
  // d'images vient donc du `.act`, jamais du patron — le laisser à un gelait
  // l'impact de feu sur la première de ses vingt et une. Il se lit par nœud :
  // l'action peut se tirer.
  const auto images_de_l_action = [charge, &seed](std::int32_t action) {
    const auto rang = static_cast<std::size_t>(std::max(action, 0));
    return rang < charge->animation.actions().size()
               ? charge->animation.actions()[rang].frames.size()
               : seed.texture_count;
  };

  // La graine vient de l'instant et de ce qui joue déjà : deux lancements du
  // même sprite ne se ressemblent pas, comme deux soins du jeu.
  std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
  for (std::size_t sown = 0; sown < count; ++sown) {
    // LE ROULIS DE CET EXEMPLAIRE, selon son rang dans la boucle du bâtisseur.
    // Voir `EffectNodePattern::copy_roll_start_degrees`.
    ro::render::EffectNodeSeed own = seed;
    if (seed.copy_roll_start_degrees != 0.0F || seed.copy_roll_step_degrees != 0.0F) {
      own.roll_degrees +=
          seed.copy_roll_start_degrees + seed.copy_roll_step_degrees * static_cast<float>(sown);
    }
    // ET SON CAP, quand le bâtisseur le fixe par rang au lieu de le tirer : la
    // rosace de Sight Trasher. L'angle du client est tronqué à l'entier, puis
    // lu dans ses tables — voir `EffectClientSinCosHeadingInWorld`.
    if (seed.copy_heading_step_degrees != 0.0F) {
      const auto angle =
          static_cast<std::int32_t>(seed.copy_heading_step_degrees * static_cast<float>(sown));
      const float cap = ro::render::EffectClientSinCosHeadingInWorld(angle);
      own.orbit_degrees = ro::render::EffectNodeRange{cap, cap};
    }
    // ET LE CAP DE DÉPART D'UNE ORBITE, fixé par rang : l'angle du client
    // `rang × pas` porte le bras par la troisième ligne de sa rotation de lacet,
    // `(sin, 0, cos)` chez lui — soit, dans notre repère, le cap `angle − 90`.
    if (seed.copy_orbit_step_degrees != 0.0F) {
      const float cap =
          seed.copy_orbit_step_degrees * static_cast<float>(sown) - kQuarterTurnDegrees;
      own.orbit_degrees = ro::render::EffectNodeRange{cap, cap};
    }
    ro::render::PooledEffectNode pooled;
    // 🔴 LA DÉRIVE D'UN PÉTALE TIRE AVANT TOUT LE RESTE, dans l'ordre du client :
    // montée, échelle, cercle, angles, action. Ce qu'elle tire de l'échelle et
    // de l'action remplace ce que le nœud tire ensuite. Voir
    // `EffectChasedDriftPattern`.
    std::optional<ro::render::EffectChasedDriftSowing> derive;
    if (seed.chased_drift != nullptr) {
      derive = ro::render::SowEffectChasedDrift(*seed.chased_drift, pooled.chased_drift, generator);
    }
    // ET LE CAP SUR LA GRILLE DU CLIENT, tiré lui aussi avant le reste. Voir
    // `DrawEffectClientGridHeading`.
    std::optional<glm::vec3> grid_heading;
    if (seed.launch_heading_on_client_grid) {
      grid_heading = ro::render::DrawEffectClientGridHeading(generator);
    }
    // CHAQUE SPRITE TIRE SA TAILLE. Une borne haute qui ne dépasse pas la basse
    // ne dérange pas le générateur : les nœuds suivants gardent alors la même
    // suite de tirages, qu'une troupe tire sa taille ou non.
    // ⚠ EN PAS ÉGAUX quand le client la tire en entiers : voir
    // `EffectNodePattern::sprite_scale_steps`.
    ro::render::EffectNodeRange drawn_scale = sprite_scale;
    if (seed.sprite_scale_steps > 1) {
      drawn_scale.values = seed.sprite_scale_steps;
    }
    // 🔴 L'ORDRE DU BÂTISSEUR, quand le patron le nomme : tout se tire d'abord,
    // dans cet ordre, puis se lit à sa place. Voir `PredrawEffectSprite`.
    ro::render::PredrawEffectSprite(own, drawn_scale, generator);
    pooled.node = ro::render::SowEffectNode(own, generator);
    if (derive.has_value()) {
      pooled.node.action = derive->action;
      pooled.node.alpha_zero_below = seed.chased_drift->alpha_zero_below;
    }
    pooled.node.texture_count = std::max<std::size_t>(images_de_l_action(pooled.node.action), 1);
    const std::optional<float> picked_scale =
        ro::render::EffectSpritePredrawnValue(own.predrawn, ro::render::EffectSpriteDraw::kScale);
    pooled.node.sprite_scale = picked_scale.has_value()
                                   ? *picked_scale
                                   : ro::render::DrawEffectNodeRange(drawn_scale, generator);
    if (derive.has_value()) {
      pooled.node.sprite_scale = derive->sprite_scale;
    }
    // LE PAS D'ÉCHELLE DÉRIVÉ se calcule APRÈS l'échelle et la vie, qui sont
    // tirées : voir `EffectNodePattern::sprite_scale_step_share`.
    if (seed.sprite_scale_step_share != 0.0F && pooled.node.life_ticks > 0) {
      pooled.node.sprite_scale_step = seed.sprite_scale_step_share * pooled.node.sprite_scale /
                                      static_cast<float>(pooled.node.life_ticks);
    }
    pooled.resource_paths = {sprite_path};
    // Les nœuds se DISPERSENT en cercle autour du point, chacun selon l'angle
    // qu'il a tiré : les poser tous au même endroit ferait une seule image
    // épaisse au lieu d'une nuée.
    const float heading = NodeRadians(pooled.node.orbit_degrees);
    const glm::vec3 arm{std::cos(heading) * scatter, 0.0F, std::sin(heading) * scatter};
    // 🔴 ET CE BRAS PEUT COMPTER DEUX FOIS. Le client range la place d'un sprite
    // en trois morceaux — centre, décalage fixe, bras qui tourne —, et certains
    // bâtisseurs écrivent le bras DANS le décalage à la naissance. Le nœud part
    // alors du double de sa dispersion, et son cercle est centré sur le point
    // baké, non sur le centre de l'effet : il spirale au lieu de tourner à plat.
    const glm::vec3 offset = seed.scatter_counts_twice ? arm : glm::vec3{0.0F, 0.0F, 0.0F};
    pooled.orbit_anchor = bouts.birth + offset;
    pooled.ground_position = pooled.orbit_anchor + arm;
    // ET SI LE PATRON LE DIT, CETTE PLACE N'EST QUE LA PREMIÈRE : le nœud tourne
    // autour de son ancre, et la troupe recalcule sa position à chaque tic. Sans
    // dispersion le cercle est un point, et rien ne bouge.
    pooled.orbit_radius = scatter;
    pooled.orbit_degrees_per_tick = seed.orbit_degrees_per_tick;
    pooled.orbit_radius_step_per_tick = seed.orbit_radius_step_per_tick;
    pooled.orbit_acceleration_per_tick = seed.orbit_acceleration_per_tick;
    pooled.orbit_radius_step_switch_tick = seed.orbit_radius_step_switch_tick;
    pooled.orbit_radius_step_after_switch = seed.orbit_radius_step_after_switch;
    pooled.orbit_start_degrees = pooled.node.orbit_degrees;
    // UNE APPARITION TOURNE AUTOUR DE SA NAISSANCE, par sa propre ronde.
    pooled.stays_where_sown = seed.nodes_stay_where_sown;
    if (seed.haunt_flight != ro::render::EffectHauntFlight::kNone) {
      pooled.haunt = ro::render::StartEffectHaunt(seed.haunt_flight);
      pooled.haunt_center = pooled.ground_position;
      pooled.node.alpha = pooled.haunt.alpha;
    }
    // LE LANCER, s'il y en a un : la particule part dans une direction au lieu
    // de rester où elle naît.
    //
    // 🔴 LE CAP S'OUVRE DEVANT LE PORTEUR, et non du même côté du monde pour
    // tout le jeu : la plage du patron ne porte que l'ÉCART tiré, auquel
    // s'ajoute l'angle de base de celui sur qui l'effet naît.
    const ro::render::EffectLaunchDraw lance =
        ro::render::DrawEffectLaunch(own, seed.launch_yaw_is_absolute ? 0.0F : owner_facing_degrees,
                                     pooled.node.life_ticks, pooled.node.step_length, generator);
    pooled.launch_direction = grid_heading.value_or(lance.direction);
    // L'IMAGE SE COUCHE-T-ELLE LE LONG DE SON VOL ? La composition le demandera,
    // et elle seule connaît la caméra dont l'angle dépend.
    pooled.turns_with_flight = seed.turns_with_flight;
    pooled.turns_with_flight_heading = seed.turns_with_flight_heading;
    pooled.flight_heading_turn_degrees = seed.flight_heading_turn_degrees;
    pooled.casts_shadow = seed.casts_shadow;
    pooled.step_decay = lance.step_decay;
    pooled.fall_speed = lance.fall_speed;
    pooled.fall_acceleration = lance.fall_acceleration;
    // LE SOUS-TYPE 8 : le frein le long de l'axe et le balancement roulé —
    // voir `PooledEffectNode::swing_amount`.
    pooled.brakes_along_axis = seed.brakes_along_axis;
    pooled.swing_speed = lance.swing_speed;
    pooled.swing_acceleration = lance.swing_acceleration;
    pooled.roll_degrees = own.roll_degrees;
    // LA VOIE RADIALE, au cap même du bras : le nœud file droit hors de sa rosace.
    pooled.radial = ro::render::StartEffectRadialLane(own, pooled.node.orbit_degrees);
    // LE NŒUD NE SORT DU SOL QUE S'IL Y EST NÉ : voir `SownLiftFor`.
    // ⚠ PAS UN PROJECTILE D'ACTEUR : il naît dix unités au-dessus du sol, hors
    // du plan, et part de ce point EXACT vers sa cible ; le lever de six
    // dixièmes fausserait sa pente — voir `EffectNodeSeed::aim_keeps_height`.
    if (!seed.aim_keeps_height) {
      pooled.ground_position.y += SownLiftFor(lance.offset.y);
    }
    // LA TRAÎNÉE NAÎT REPLIÉE SUR LE NŒUD, au point d'AVANT le lancer.
    ro::render::SeedEffectTrailHistory(pooled.trail, seed.trail_trace_count, pooled.node,
                                       pooled.ground_position);
    pooled.ground_position += lance.offset;
    // 🔴 UN PROJECTILE VISÉ NE TIRE PAS SA DIRECTION : elle va de sa naissance à
    // la cible, À PLAT, et son pas est la distance divisée par sa vie — il
    // arrive au dernier tic, quelle que soit la portée. Le client tourne le
    // nœud vers la cible ; la hauteur, elle, reste à la chute. Voir
    // `EffectNodeSeed::aims_at_target`.
    //
    // ⚠ SANS CIBLE, IL NE PART PAS — et il ne consomme rien de plus : la
    // direction tirée au-dessus reste celle d'un lancer ordinaire.
    if (seed.aims_at_target && bouts.aim.has_value()) {
      const glm::vec3 vers = *bouts.aim - pooled.ground_position;
      const glm::vec3 a_plat{vers.x, 0.0F, vers.z};
      // 🔴 À PLAT D'ORDINAIRE, la hauteur restant à la chute ; EN TROIS
      // DIMENSIONS pour un projectile d'acteur, qui suit la ligne droite
      // jusqu'à sa cible, plus haute ou plus basse que lui. Voir
      // `EffectNodeSeed::aim_keeps_height`.
      const glm::vec3 axe = seed.aim_keeps_height ? vers : a_plat;
      const float distance = glm::length(axe);
      if (distance > 0.0F) {
        // LE CAP DU CLIENT, quand il passe par sa table : tronqué au degré et
        // écarté de la face — voir `EffectClientAimedHeading`.
        pooled.launch_direction =
            seed.aim_heading_on_client_table
                ? ro::render::EffectClientAimedHeading(a_plat, seed.aim_face_offset_degrees)
                : axe / distance;
        pooled.node.step_length = seed.aim_step_factor * distance /
                                  static_cast<float>(std::max(pooled.node.life_ticks, 1));
        pooled.step_decay = 0.0F;
      }
      // ET IL MEURT PRÈS DU POINT VISÉ, s'il y passe.
      if (seed.dies_near_target) {
        pooled.dies_near_target = true;
        pooled.target_position = bouts.aim;
      }
    }
    // SON IMAGE MONTRE LA DIRECTION DU POINT VISÉ, vue de la caméra de cette
    // image-ci — qu'il vole vers lui ou non : les types 8 et 28 de la fabrique
    // restent sur place. Voir `EffectAimQuadrantAction`.
    if (seed.action_from_aim_quadrant && bouts.aim.has_value()) {
      pooled.node.action = ro::render::EffectAimQuadrantAction(
          ground_position, *bouts.aim, pool_.billboard_turn_degrees(), seed.aim_quadrant_actions);
    }
    // ⚠ PUIS IL SE POSE SUR LE POINT VISÉ, et seulement après : l'action garde
    // la direction du point de l'effet vers le point visé. Le poser d'abord
    // annulerait cette direction. Voir `EffectNodeSupplement::sits_on_aim_point`.
    // ET IL Y RESTE, même quand son effet suit le lanceur : le client l'y repose
    // à chaque tic, et ce point ne bouge pas.
    if (seed.sits_on_aim_point && bouts.aim.has_value()) {
      pooled.ground_position = *bouts.aim;
      pooled.stays_where_sown = true;
    }
    // LE REBOND AU SOL ET SON BRUIT, que la graine porte depuis le patron.
    pooled.bounces_at_floor = seed.bounces_at_floor;
    pooled.bounce_sound = std::string(seed.bounce_sound);
    pooled.bounce_sound_draws = seed.bounce_sound_draws;
    // LE PÉTALE NAÎT SUR SON CERCLE, autour du point de l'effet.
    if (derive.has_value()) {
      pooled.ground_position += derive->birth_offset;
    }
    pool_.Add(std::move(pooled));
  }
  return true;
}

bool EffectNodesInPlay::Play(std::string_view sprite_base, glm::vec3 ground_position) {
  ro::render::EffectNodeSeed seed;
  seed.life_ticks = kBenchLifeTicks;
  seed.fade_out_ticks = kBenchFadeOutTicks;
  seed.orbit_degrees = ro::render::EffectNodeRange{0.0F, kFullTurnDegrees};
  return Sow(std::string(kEffectSpriteFolder) + std::string(sprite_base), seed,
             ro::render::EffectNodeRange{kBenchSpriteScale, 0.0F}, kBenchScatterWorldUnits,
             ground_position, kNodesPerGroup, kNoOwnerFacingDegrees);
}

bool EffectNodesInPlay::ShootSprite(std::string_view sprite_base, glm::vec3 from, glm::vec3 to,
                                    std::int32_t life_ticks,
                                    const ro::render::ProjectileFlight& flight) {
  // 🔴 UNE HORLOGE DIVISÉE ALLONGE TOUT ENSEMBLE : la vie, donc le vol, et la
  // tenue de chaque image. Le pas reste la distance divisée par la vie, si bien
  // que le projectile va d'autant moins vite sans arriver ailleurs.
  const std::int32_t slowdown = std::max(flight.clock_slowdown, 1);
  // LA BOULE D'ÉNERGIE prend la place du sprite : ses pièces, chacune un nœud
  // du sprite fait en code, toutes ajoutées au décor. Voir `ro/render/wand_orb.h`.
  if (flight.wand_orb_rgb.has_value()) {
    const std::string orb_path(ro::render::kWandOrbSpritePath);
    if (!sprites_.contains(orb_path)) {
      sprites_.emplace(orb_path, ro::render::WandOrbSprite());
    }
    std::uint32_t generator = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
    bool shot = false;
    for (const ro::render::WandOrbPiece& piece : ro::render::WandOrbPieces(
             from, to, std::max(life_ticks, 1) * slowdown, *flight.wand_orb_rgb, generator)) {
      ro::render::EffectNodeSeed seed;
      seed.life_ticks = piece.life_ticks;
      seed.fade_out_ticks = piece.fade_out_ticks;
      seed.orbit_degrees = ro::render::EffectNodeRange{0.0F, 0.0F};
      seed.action = ro::render::WandOrbActionOf(piece.part);
      seed.first_frame_hold_ticks = piece.hidden_ticks;
      seed.frame_hold_ticks = piece.frame_hold_ticks;
      seed.frame_hold_ticks_high = 0;
      seed.loops_frames = piece.loops_frames;
      seed.render_word = ro::render::kEffectRenderWordOfAdditiveSprite;
      seed.alpha_start = piece.alpha;
      seed.color = {static_cast<float>(piece.rgb[0]), static_cast<float>(piece.rgb[1]),
                    static_cast<float>(piece.rgb[2])};
      seed.casts_shadow = piece.casts_shadow;
      seed.aims_at_target = piece.flies_to.has_value();
      // TOUTES NAISSENT AU POINT EXACT DU VOL, dix unités au-dessus du sol, sans
      // le lever d'un nœud semé au sol : une étincelle et l'éclat restent ainsi
      // à la hauteur où la boule est passée.
      seed.aim_keeps_height = true;
      shot = Sow(orb_path, seed, ro::render::EffectNodeRange{1.0F, 0.0F}, 0.0F, piece.birth, 1,
                 kNoOwnerFacingDegrees, piece.flies_to) ||
             shot;
    }
    return shot;
  }
  ro::render::EffectNodeSeed seed;
  seed.life_ticks = std::max(life_ticks, 1) * slowdown;
  // Ni dispersion ni orbite : il n'y a qu'un projectile, et il part d'un point.
  seed.orbit_degrees = ro::render::EffectNodeRange{0.0F, 0.0F};
  seed.aims_at_target = true;
  // 🔴 ET IL VOLE EN LIGNE DROITE, HAUTEUR COMPRISE : le client mène ses deux
  // classes de projectile des pieds du tireur aux pieds de la cible en trois
  // dimensions. À plat, une flèche tirée d'en bas arrivait sous sa cible.
  seed.aim_keeps_height = true;
  // 🔴 SES IMAGES DÉFILENT PENDANT LE VOL, et chaque classe a sa règle : la
  // flèche tient sa première image huit mises à jour puis avance d'une par
  // mise à jour jusqu'à la dernière ; la munition en sprite change d'image à
  // chaque mise à jour, en boucle, et tourne d'un angle réel. Sans règle, une
  // munition à plusieurs images — les quatre du boulet de canon — défilait à
  // la retenue par défaut des nœuds, quatre tics par image, en boucle.
  seed.frame_hold_ticks = slowdown;
  seed.frame_hold_ticks_high = 0;
  if (flight.image_rule == ro::render::ProjectileImageRule::kArrow) {
    seed.first_frame_hold_ticks = ro::render::kArrowFirstImageHoldTicks * slowdown;
    seed.loops_frames = false;
    seed.holds_last_frame = true;
  } else {
    seed.loops_frames = true;
    seed.spins_by_exact_degrees = true;
  }
  // 🔴 L'IMAGE TOURNE DE LA FACE DE SON VOL, vue de la caméra et recalculée à
  // chaque image — le lacet seul, comme le client : voir
  // `EffectNodeSeed::turns_with_flight_heading`. Un angle figé au départ
  // restait en arrière d'une caméra qui tourne ; un angle pris sur la
  // projection, avec l'image supposée pointer vers le haut, faisait voler la
  // flèche à l'envers — sa pointe est en bas.
  seed.turns_with_flight_heading = true;
  seed.flight_heading_turn_degrees = flight.turn_degrees;
  // 🔴 UN PROJECTILE EST UN SPRITE D'ACTEUR, ET SE PEINT COMME LUI : mélange
  // ordinaire, épreuve de profondeur, sous les `.str`. Sans mot de rendu, le
  // lot se peignait en ADDITION : la hampe sombre de la flèche s'y effaçait, et
  // il n'en restait qu'une traînée pâle, sans image.
  seed.render_word = ro::render::kEffectRenderWordOfActorSprite;
  // ET IL NAÎT OPAQUE : le client le dessine plein dès sa première image, sans
  // montée d'alpha. La montée d'un tic le laissait invisible sur le premier de
  // ses douze.
  seed.alpha_start = ro::render::kEffectNodeChannelMax;
  // ET IL PORTE L'OMBRE D'UN ACTEUR : voir `ProjectileCasters`.
  seed.casts_shadow = true;
  const std::string path(sprite_base);
  const bool shot = Sow(path, seed, ro::render::EffectNodeRange{1.0F, 0.0F}, 0.0F, from, 1,
                        kNoOwnerFacingDegrees, to);
  // LA LUEUR ÉLÉMENTAIRE, s'il en porte une : la même image sur le même vol, un
  // peu plus grande, teinte et AJOUTÉE au décor. Elle n'a pas d'ombre : c'est
  // de la lumière.
  if (shot && flight.glow_tint.has_value()) {
    const std::array<std::uint8_t, 4>& tint = *flight.glow_tint;
    seed.render_word = ro::render::kEffectRenderWordOfAdditiveSprite;
    seed.casts_shadow = false;
    seed.color = {static_cast<float>(tint[0]), static_cast<float>(tint[1]),
                  static_cast<float>(tint[2])};
    seed.alpha_start = static_cast<float>(tint[3]);
    static_cast<void>(Sow(path, seed,
                          ro::render::EffectNodeRange{ro::render::kArrowGlowScale, 0.0F}, 0.0F,
                          from, 1, kNoOwnerFacingDegrees, to));
  }
  return shot;
}

bool EffectNodesInPlay::PlayActOnce(std::string_view sprite_base, glm::vec3 ground_position,
                                    float frame_period_units, std::size_t action_index) {
  const std::string path(sprite_base);
  const ro::render::LoadedActorSprite* loaded = LoadedSprite(path);
  if (loaded == nullptr || loaded->animation.actions().size() <= action_index) {
    return false;
  }
  const ro::formats::ActAction& action = loaded->animation.actions()[action_index];
  if (action.frames.empty()) {
    return false;
  }
  // La cadence du fichier, sauf quand l'appelant la force — et le client la
  // compte en tics de vingt-quatre millisecondes, là où les nôtres en font
  // seize : une cadence impaire donne une période fractionnaire.
  const float period_units = frame_period_units > 0.0F ? frame_period_units : action.speed_units;
  const float period_ticks = period_units * ro::formats::kMillisecondsPerSpeedUnit /
                             static_cast<float>(ro::render::kEffectTickMs);
  if (period_ticks <= 0.0F) {
    return false;
  }
  ro::render::EffectNodeSeed seed;
  seed.action = static_cast<std::int32_t>(action_index);
  seed.texture_count = action.frames.size();
  seed.loops_frames = false;
  seed.frame_period_ticks = period_ticks;
  // La vie couvre toutes les images : c'est la dernière qui tue, et la vie
  // n'est là que pour qu'aucun nœud ne lui survive.
  seed.life_ticks =
      static_cast<std::int32_t>(std::ceil(static_cast<float>(action.frames.size()) * period_ticks));
  seed.orbit_degrees = ro::render::EffectNodeRange{0.0F, 0.0F};
  // 🔴 C'EST UN SPRITE D'ACTEUR, ET IL SE PEINT COMME LUI : mélange ordinaire,
  // épreuve de profondeur, plein dès sa première image. Sans mot de rendu, le
  // lot s'AJOUTAIT au décor : la fumée grise de Hiding ne faisait qu'éclaircir
  // un sol déjà clair, et ne se voyait pas.
  seed.render_word = ro::render::kEffectRenderWordOfActorSprite;
  seed.alpha_start = ro::render::kEffectNodeChannelMax;
  return Sow(path, seed, ro::render::EffectNodeRange{1.0F, 0.0F}, 0.0F, ground_position, 1,
             kNoOwnerFacingDegrees);
}

ro::render::EffectProjectileAngles EffectNodesInPlay::LastProjectileOf(float owner_facing_degrees,
                                                                       glm::vec3& place) const {
  // AVANT LE PREMIER PROJECTILE, l'instance garde ce que sa création y a mis :
  // l'angle du CORPS de son porteur pour cap, et une inclinaison que rien
  // n'écrit — zéro, celle d'une mémoire neuve, que le client montre presque
  // toujours ; le reste n'est qu'un résidu de la mémoire réutilisée.
  ro::render::EffectProjectileAngles angles{
      ro::render::kEffectBodyAngleMirrorDegrees - owner_facing_degrees, 0.0F};
  if (sowing_clock_ == nullptr) {
    return angles;
  }
  if (sowing_clock_->last_projectile.has_value()) {
    angles = *sowing_clock_->last_projectile;
  }
  // LA PLACE EST CELLE DU PLUS RÉCENT PROJECTILE ENCORE EN VIE : chacun écrit
  // la sienne dans l'instance à chaque tic, le dernier semé en dernier.
  if (sowing_clock_->projectile_serial != 0) {
    const auto& nodes = pool_.nodes();
    for (auto it = nodes.rbegin(); it != nodes.rend(); ++it) {
      if (it->projectile_serial == sowing_clock_->projectile_serial) {
        place = it->ground_position;
        break;
      }
    }
  }
  return angles;
}

bool EffectNodesInPlay::PlaySeedsDueAt(std::span<const ro::render::EffectNodePattern> patterns,
                                       glm::vec3 ground_position, std::int32_t tick,
                                       float owner_facing_degrees,
                                       const std::optional<glm::vec3>& target_position,
                                       std::int32_t param, float instance_scale,
                                       float instance_second) {
  bool sown = false;
  for (const ro::render::EffectNodePattern& pattern : patterns) {
    // SOUS `/mineffect`, CERTAINS BÂTISSEURS CHANGENT DE CADENCE OU SE TAISENT :
    // leur garde lit le réglage des effets allégés. Voir
    // `EffectNodeSupplement::period_ticks_with_minimal_effects`.
    const std::int32_t minimal_effects_period_ticks =
        pattern.supplement.period_ticks_with_minimal_effects;
    const bool cadence_allegee = minimal_effects_ && minimal_effects_period_ticks != 0;
    if (cadence_allegee &&
        (minimal_effects_period_ticks == ro::render::kEffectNodeNotSownWithMinimalEffects ||
         !ro::render::EffectNodeScheduleSowsAt(
             {pattern.first_tick, pattern.last_tick, minimal_effects_period_ticks,
              pattern.period_phase_ticks},
             tick))) {
      continue;
    }
    // 🔴 UN SEMEUR QUI MARCHE PORTE SON PROPRE HORAIRE, ET IL PASSE DEVANT CELUI
    // DU PATRON. Le client rappelle son bâtisseur à CHAQUE tic tant qu'il n'est
    // pas arrivé ; une table qui ne lit que des conditions sur le compteur
    // rend « une seule fois » quand elle n'en trouve aucune. Voir
    // `EffectMarchingSower`, qui dit pourquoi cette table-là se tient à la main.
    //
    // ⚠ SANS CIBLE, IL NE MARCHE PAS. La fenêtre ne connaît alors aucune
    // direction et l'arrivée ne se mesure contre rien ; marcher quand même
    // coucherait cent cinquante pointes dans la direction du regard. C'est la
    // convention des autres semis sans cible — l'essaim, la colonne, le trait
    // gainé —, et c'est un écart NOMMÉ au client, dont l'instance se vise
    // elle-même et marche jusqu'au bout de sa vie.
    const ro::render::EffectMarchingSower* march =
        ro::render::EffectMarchingSowerFor(pattern.effect_id);
    const bool is_marching = march != nullptr && target_position.has_value();
    if (is_marching) {
      const float distance_to_target =
          ro::render::EffectGroundDistance(ground_position, *target_position);
      if (!ro::render::EffectMarchingSowerSowsAt(*march, tick, distance_to_target)) {
        continue;
      }
      // ⚠ LE PARAMÈTRE DE L'INSTANCE NE SE LIT PAS ICI, et il n'a rien à y
      // faire : aucun semeur qui marche ne suit son paramètre.
    } else if (cadence_allegee) {
      // LA CADENCE ALLÉGÉE A DÉJÀ TRANCHÉ, ci-dessus.
    } else if (pattern.sowings_follow_param &&
               ro::render::EffectParamCounterIsShared(pattern.effect_id)) {
      // ⚠ LE COMPTE EST CELUI DE TOUTES LES INSTANCES, et non le rang du semis :
      // voir `EffectParamCounterIsShared`.
      if (ro::render::EffectNodePatternSowingRank(pattern, tick) <= 0 ||
          shared_param_sowings_ >= param) {
        continue;
      }
      ++shared_param_sowings_;
    } else if (!ro::render::EffectNodePatternSowsAtParam(pattern, tick, param)) {
      // ⚠ LE PARAMÈTRE DE L'INSTANCE BORNE LES SEMIS QUI LE SUIVENT — cinq
      // boules à Soul Strike 10, une à `@effect 15` —, et ne change rien aux
      // autres.
      continue;
    }
    // LE POINT DE CE SEMIS-CI : celui de l'effet, ou celui que la marche a
    // atteint. Tout ce qui suit s'y rapporte.
    const glm::vec3 sowing = is_marching ? ro::render::EffectMarchingSowerPoint(
                                               *march, ground_position, *target_position, tick)
                                         : ground_position;
    // LE SON DU SEMIS, une fois par boule et au point de l'INSTANCE ;
    // `TakeSounds` le forme. Voir `EffectNodePattern::sound_at_sowing`.
    //
    // ⚠ AU POINT DE L'INSTANCE, ET NON À CELUI DE LA MARCHE : le client AVANCE
    // la place de son instance, mesure la distance, puis la REMET là où elle
    // était. Le semeur marche ; l'effet, lui, ne bouge pas.
    if (pattern.sound_at_sowing) {
      sowing_sounds_.push_back({pattern.effect_id, ground_position});
    }
    // 🔴 UN NŒUD NE SURVIT PAS À SON EFFET, ni au rendez-vous que son bâtisseur
    // lui donne. La règle vit dans `render`, avec les autres traductions du
    // patron : voir `EffectNodePatternTicksLeft`.
    const std::int32_t remainder = ro::render::EffectNodePatternTicksLeft(pattern, tick);
    if (pattern.shape != ro::render::EffectNodeShape::kSprite) {
      sown =
          SowRing(pattern, sowing, remainder, tick, owner_facing_degrees, param, target_position) ||
          sown;
      continue;
    }
    ro::render::EffectNodeSeed seed =
        ro::render::EffectNodeSeedFromPattern(pattern, remainder, tick, param);
    // LA CADENCE DU SEMEUR : le second paramètre de son objet du RSW, tronqué,
    // un tic au moins — un aussi sans semeur. Voir
    // `EffectNodePattern::sprite_frame_hold_from_instance`.
    if (pattern.sprite_frame_hold_from_instance) {
      seed.frame_hold_ticks =
          instance_second >= 1.0F ? static_cast<std::int32_t>(instance_second) : 1;
      seed.frame_hold_ticks_high = 0;
    }
    // UN SPRITE N'EN A QU'UN, et c'est le premier : le couple .spr/.act porte
    // ses images lui-même, là où une figure du monde fait défiler un tableau de
    // textures.
    const std::span<const std::string_view> couple = ro::render::EffectNodePatternTextures(pattern);
    if (couple.empty()) {
      continue;
    }
    // ⚠ UN PATRON QUI A UN HORAIRE SÈME UN SEUL NŒUD, et le sème à chaque tic
    // dû. `kNodesPerTroupe` tenait lieu des passes successives que nous ne
    // jouions pas ; depuis que l'horloge les joue, l'appliquer en plus
    // multiplierait la nuée par douze — cent quarante-quatre étincelles là où le
    // client en lâche douze.
    //
    // SANS DISPERSION, UN SEUL NŒUD AUSSI. Douze exemplaires au même point ne
    // font pas une nuée : ils font une image douze fois plus lourde. Une
    // dispersion nulle veut dire que nous ne la connaissons pas, pas que le
    // client empile.
    //
    // 🔴 MAIS UN COMPTE CONNU PASSE DEVANT LES DEUX. `copies` vient de la
    // boucle qui entoure le bâtisseur chez le client ; les deux règles
    // ci-dessus ne sont que ce qu'on fait à défaut. Un veut dire « une fois »
    // OU « boucle illisible », et c'est pourquoi le défaut lui survit — voir
    // `EffectNodePattern::copies`.
    std::size_t count = pattern.copies > 1                                      ? pattern.copies
                        : (pattern.period_ticks > 0 || pattern.scatter <= 0.0F) ? 1
                                                                                : kNodesPerGroup;
    // ET UN COMPTE CONNU PEUT SE TIRER, bornes comprises — voir
    // `EffectNodePattern::copies_high`. La graine est celle du semis.
    if (pattern.copies_high > pattern.copies) {
      std::uint32_t draw = static_cast<std::uint32_t>(pool_.size()) + beat_ms_ + 1U;
      count = pattern.copies +
              ro::render::NextEffectNodeDraw(draw) % (pattern.copies_high - pattern.copies + 1U);
    }
    // LE BALAYAGE DE NAISSANCE : le nœud semé à ce tic naît un peu plus loin
    // sur son cercle que celui du tic d'avant. C'est la succession des
    // naissances qui tourne autour du porteur, pas le nœud. Voir
    // `EffectNodePattern::birth_sweep_radius`.
    glm::vec3 birth =
        sowing + ro::render::EffectBirthSweepOffset(pattern.birth_sweep_radius,
                                                    pattern.birth_sweep_degrees_per_tick, tick);
    // 🔴 ET UN NŒUD QUI PART DU DERNIER PROJECTILE naît à sa place et prend ses
    // angles : voir `LastProjectileOf`.
    if (pattern.supplement.launches_from_last_projectile) {
      seed.last_projectile = LastProjectileOf(owner_facing_degrees, birth);
    }
    // ⚠ ET LES TEXTURES PEUVENT SE DISTRIBUER SUR LES EXEMPLAIRES au lieu de
    // défiler dans un seul nœud : chacun garde alors la sienne pour toute sa
    // vie, et c'est son RANG qui la choisit. Semer d'un coup donnerait la même
    // à tous.
    // L'ÉCHELLE DU SEMEUR REMPLACE CELLE DU PATRON, et ne la multiplie pas : le
    // client ÉCRIT le paramètre du RSW dans le champ, il ne le compose avec
    // rien. Sans semeur — un effet lancé par une compétence — le paramètre est
    // nul, et le patron garde la sienne.
    const ro::render::EffectNodeRange scale =
        pattern.sprite_scale_from_instance && instance_scale > 0.0F
            ? ro::render::EffectNodeRange{instance_scale, 0.0F}
            : ro::render::EffectNodePatternRange(
                  pattern, ro::render::EffectDrawnRange::kSpriteScale,
                  ro::render::EffectNodePatternSpriteScaleAt(pattern, tick),
                  pattern.sprite_scale_high);
    // LA DISPERSION DE CE SEMIS : une vague plus tardive naît plus loin. Voir
    // `EffectNodePattern::scatter_sowing_tick_divisor`.
    const float dispersion = ro::render::EffectNodePatternScatterAt(pattern, tick);
    if (pattern.textures_per_copy && couple.size() > 1) {
      for (std::size_t rang = 0; rang < count; ++rang) {
        sown = Sow(std::string(couple[rang % couple.size()]), seed, scale, dispersion, birth, 1,
                   owner_facing_degrees, target_position) ||
               sown;
      }
      continue;
    }
    sown = Sow(std::string(couple.front()), seed, scale, dispersion, birth, count,
               owner_facing_degrees, target_position) ||
           sown;
  }
  return sown;
}

}  // namespace moonclient
