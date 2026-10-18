// LE LANCER D'UNE PARTICULE : le tirage de sa direction, et son avance.
//
// ⚠ IL VIT À PART DU RÉSERVOIR, comme la description d'un semis : décrire un
// MOUVEMENT et faire vivre une TROUPE sont deux affaires. Le réservoir est un
// catalogue de formes qu'on parcourt ; ceci se lit d'un bout à l'autre.

#include "ro/render/effect_launch.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>

#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_node_seed.h"
#include "ro/render/map_effect_math.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// Un tirage rendu à sa plage. ⚠ LA MÊME RÈGLE QUE LE RÉSERVOIR — une plage vide
// donne sa borne basse plutôt qu'un tirage inutile, faute de quoi un nœud
// immobile ferait avancer le générateur et changerait tous ses voisins.
[[nodiscard]] float DrawInRange(const EffectNodeRange& plage, std::uint32_t& rng_state) {
  return DrawEffectNodeRange(plage, rng_state);
}

// La valeur tirée d'avance dans l'ordre du bâtisseur, ou un tirage maintenant.
// Voir `PredrawEffectSprite`.
[[nodiscard]] float PriseOuTiree(const EffectNodeSeed& style_seed, EffectSpriteDraw kind,
                                 const EffectNodeRange& plage, std::uint32_t& rng_state) {
  const std::optional<float> prise = EffectSpritePredrawnValue(style_seed.predrawn, kind);
  return prise.has_value() ? *prise : DrawInRange(plage, rng_state);
}

}  // namespace

EffectLaunchDraw DrawEffectLaunch(const EffectNodeSeed& seed, float facing_degrees,
                                  std::int32_t life_ticks, float step_length,
                                  std::uint32_t& generator) {
  EffectLaunchDraw lance;
  // RIEN N'EST TIRÉ QUAND RIEN NE PART. Voir l'en-tête : un tirage inutile
  // décalerait toute la suite de la troupe.
  //
  // 🔴 MAIS « PARTIR » N'EST PAS « AVANCER AU SOL ». Une garde qui ne
  // regarderait que le pas horizontal refuserait le lancer d'une particule qui
  // ne fait que MONTER : les étincelles du soin resteraient posées en cercle
  // autour du personnage, leur vitesse verticale jamais appliquée.
  const bool no_step = seed.step_length.high <= 0.0F && seed.step_length.low <= 0.0F &&
                       seed.step_change_per_tick <= 0.0F;
  const bool sans_chute = seed.fall_speed.low == 0.0F && seed.fall_speed.high == 0.0F;
  // ⚠ LA DESCENTE DU DÉPART EST UNE MONTÉE CHEZ NOUS. Le client écrit moins dix,
  // ce qui ÉLÈVE la particule de deux cellules puisque son ordonnée descend.
  // Recopier son signe l'enterre d'autant.
  //
  // ⚠ ET ELLE NE SE TIRE PAS : c'est une PLACE, pas un lancer. Un nœud qui ne
  // part nulle part mais naît en l'air — la boule de feu de Sight, à vingt
  // unités — la garde sans que rien ne soit tiré. Elle se pose donc AVANT la
  // porte, qui la perdrait sinon.
  const glm::vec3 descent = EffectClientOffsetInWorld(0.0F, seed.launch_drop, 0.0F);
  if (no_step && sans_chute) {
    lance.offset = descent;
    return lance;
  }
  // LES TIRAGES QUE LE BÂTISSEUR JETTE, puis L'ACCÉLÉRATION VERTICALE QU'IL
  // TIRE : tous deux avant le cap, dans son ordre. Voir `EffectNodeSupplement`.
  // ⚠ UN ORDRE DE BÂTISSEUR PORTE SES PROPRES TIRAGES JETÉS.
  if (seed.draw_order.count == 0) {
    for (std::uint8_t jete = 0; jete < seed.launch_draws_discarded; ++jete) {
      static_cast<void>(NextEffectNodeDraw(generator));
    }
  }
  const bool acceleration_tiree =
      seed.fall_acceleration.low != 0.0F || seed.fall_acceleration.high != 0.0F;
  const float acceleration_du_client = acceleration_tiree
                                           ? PriseOuTiree(seed, EffectSpriteDraw::kFallAcceleration,
                                                          seed.fall_acceleration, generator)
                                           : 0.0F;
  // LES DEUX ANGLES, dans l'ordre où le client les tire : le cap d'abord —
  // relatif au porteur —, l'inclinaison ensuite.
  const float cap = facing_degrees + PriseOuTiree(seed, EffectSpriteDraw::kLaunchYaw,
                                                  seed.launch_yaw_degrees, generator);
  const float tilt =
      PriseOuTiree(seed, EffectSpriteDraw::kLaunchPitch, seed.launch_pitch_degrees, generator);
  const float cap_rad = DegreesToRadians(cap);
  const float pitch_rad = DegreesToRadians(tilt);
  // 🔴 LA CONVENTION N'EST PAS CELLE QU'ON ÉCRIT DE TÊTE. Pour un cap de 211°
  // et une inclinaison de 13°, la direction du client vaut
  // (−0,515038 ; +0,192821 ; −0,835198).
  //
  //     x =  sin(cap)
  //     y = −cos(cap) × sin(inclinaison)
  //     z =  cos(cap) × cos(inclinaison)
  //
  // ⚠ TROIS PIÈGES, ET CHACUN SUFFIT À TOUT FAUSSER.
  //
  //   · LE CAP NE SE MODULE PAS. L'abscisse vaut `sin(cap)` tout court : lui
  //     appliquer le cosinus de l'inclinaison écrase la gerbe dès qu'elle
  //     s'incline.
  //   · L'INCLINAISON N'ÉLÈVE PAS, ELLE BASCULE. Elle agit sur l'ordonnée ET
  //     sur la profondeur — elle couche le vecteur dans le plan que le cap a
  //     choisi, au lieu de le soulever verticalement.
  //   · ET L'ORDONNÉE DESCEND, comme partout ailleurs dans ce client. Une
  //     inclinaison POSITIVE fait donc PLONGER la particule ; écrire le sinus
  //     sans son moins envoie toute la gerbe à l'envers.
  //
  // ⚠ UNE FORMULE FAUSSE SE RELIT JUSTE. Les trois nombres qu'elle produit sont
  // LES MÊMES, à l'ordre et à un signe près : c'est l'affectation aux axes qui
  // ment, et seul l'exemple chiffré ci-dessus la tranche.
  //
  // 🔴 ET CES TROIS NOMBRES SONT DANS LE REPÈRE DU CLIENT. Le nôtre a
  // l'ordonnée et la profondeur opposées aux siennes ; laissés tels quels, ils
  // envoient toute gerbe SOUS TERRE et du mauvais côté.
  //
  // ⚠ UN VECTEUR ET UN REPÈRE NE CHANGENT PAS DE LA MÊME FAÇON, et c'est la
  // subtilité de cet endroit : une DIRECTION se réfléchit — on lui applique le
  // changement une fois —, là où l'ORIENTATION d'une figure se conjugue, parce
  // que ses coordonnées locales sont retournées elles aussi. D'où un vecteur
  // ici, et une simple négation du cap pour un anneau.
  //
  // 🔴 ET QUAND L'INCLINAISON SE COMPOSE AVANT LE CAP, les rôles s'échangent :
  //
  //     x =  cos(inclinaison) × sin(cap)
  //     y = −sin(inclinaison)
  //     z =  cos(inclinaison) × cos(cap)
  //
  // L'ordonnée ne dépend plus du cap : une inclinaison proche du quart de tour
  // fait monter la particule quel que soit le cap, qui ne choisit plus que le
  // côté vers lequel elle penche. Voir `EffectNodePattern::launch_pitch_before_yaw`.
  lance.direction =
      seed.launch_pitch_before_yaw
          ? EffectClientOffsetInWorld(std::cos(pitch_rad) * std::sin(cap_rad), -std::sin(pitch_rad),
                                      std::cos(pitch_rad) * std::cos(cap_rad))
          : EffectClientOffsetInWorld(std::sin(cap_rad), -std::cos(cap_rad) * std::sin(pitch_rad),
                                      std::cos(cap_rad) * std::cos(pitch_rad));

  // 🔴 UN NŒUD QUI PART DU DERNIER PROJECTILE ajoute ses deux écarts tirés aux
  // angles du projectile, et sa direction passe par les tables du client —
  // tronquée au degré, un degré de travers sous zéro. Voir
  // `EffectNodeSeed::last_projectile`.
  if (seed.last_projectile.has_value()) {
    lance.direction = EffectClientPitchedHeading(seed.last_projectile->yaw_degrees + cap,
                                                 seed.last_projectile->pitch_degrees + tilt);
  }
  const float distance =
      PriseOuTiree(seed, EffectSpriteDraw::kLaunchDistance, seed.launch_distance, generator);
  lance.offset = lance.direction * distance + descent;
  // ⚠ L'INCLINAISON PEUT NE PLACER QUE LE DÉPART : le bâtisseur rebâtit alors
  // la matrice du vol sur le seul cap, et la particule file à plat. Voir
  // `EffectNodeSupplement::launch_pitch_places_only`.
  if (seed.launch_pitch_places_only) {
    lance.direction = EffectClientOffsetInWorld(std::sin(cap_rad), 0.0F, std::cos(cap_rad));
  }

  // LE PAS PERD UNE PART DE LUI-MÊME PAR TIC, rapportée à sa vie. Une vie nulle
  // ne divise pas : le pas garde alors sa valeur.
  const float life = static_cast<float>(std::max(life_ticks, 1));
  lance.step_decay = -(step_length / life) * seed.step_decay_share + seed.step_change_per_tick;

  if (seed.fall_speed.low != 0.0F || seed.fall_speed.high != 0.0F) {
    // ⚠ ET LA VITESSE VERTICALE CHANGE DE SIGNE ELLE AUSSI. Le client la tire
    // NÉGATIVE pour une particule qui MONTE ; c'est ce qui fait la gerbe qui
    // retombe, et non une chute immédiate. L'accélération en découle, donc elle
    // suit d'elle-même.
    lance.fall_speed =
        -PriseOuTiree(seed, EffectSpriteDraw::kFallSpeed, seed.fall_speed, generator);
    lance.fall_acceleration = -(lance.fall_speed / life) * seed.fall_acceleration_share;
    // ⚠ SAUF QUAND LE BÂTISSEUR TIRE L'ACCÉLÉRATION ELLE-MÊME : elle ne se
    // rapporte alors à rien, et change de signe comme la vitesse.
    if (acceleration_tiree) {
      lance.fall_acceleration = -acceleration_du_client;
    }
  }
  if (seed.swing_speed.low != 0.0F || seed.swing_speed.high != 0.0F) {
    // LE BALANCEMENT garde le signe du client — négatif vers le haut — et se
    // retourne à l'application, comme un décalage. Son freinage s'écrit comme
    // celui de la chute : `−(vitesse / vie) · part`.
    lance.swing_speed = DrawInRange(seed.swing_speed, generator);
    lance.swing_acceleration = -(lance.swing_speed / life) * seed.swing_acceleration_share;
  }
  return lance;
}

namespace {

constexpr float kTurnDegrees = 360.0F;

// Ramène une face sur le tour, comme le client : une soustraction ou une
// addition, pas un reste.
[[nodiscard]] float FacingWithinTurn(float face) {
  if (face >= kTurnDegrees) {
    face -= kTurnDegrees;
  }
  if (face < 0.0F) {
    face += kTurnDegrees;
  }
  return face;
}

// LA FACE QUI TOURNE UN NŒUD VERS UN ÉCART DU CLIENT (abscisse, profondeur).
// SANS DIRECTION, le nœud garde la face de son constructeur : zéro.
[[nodiscard]] float FacingTowardOffset(float dx, float dz) {
  // L'approximation de π du client, et le demi-tour en degrés.
  constexpr float kClientPi = 3.141592F;
  constexpr float kHalfTurnDegrees = 180.0F;
  if (dx == 0.0F && dz == 0.0F) {
    return 0.0F;
  }
  const auto brut =
      static_cast<float>(std::atan2(static_cast<double>(dx), static_cast<double>(-dz)));
  return FacingWithinTurn(brut / kClientPi * kHalfTurnDegrees);
}

// LE CAP EST L'OPPOSÉ DE LA FACE, tronqué, puis lu dans les tables : la ligne
// de la rotation, dans notre repère.
[[nodiscard]] glm::vec3 OppositeHeadingLine(float face) {
  const float radians = NativeTableRadians(static_cast<std::int32_t>(-face));
  return EffectClientOffsetInWorld(std::sin(radians), 0.0F, std::cos(radians));
}

}  // namespace

glm::vec3 DrawEffectClientGridHeading(std::uint32_t& generator) {
  constexpr std::uint32_t kGridSide = 7;
  constexpr float kGridCenter = 3.0F;
  // LA PROFONDEUR D'ABORD, l'abscisse ensuite : l'ordre des deux tirages.
  const float depth = static_cast<float>(NextEffectNodeDraw(generator) % kGridSide);
  const float abscisse = static_cast<float>(NextEffectNodeDraw(generator) % kGridSide);
  return OppositeHeadingLine(FacingTowardOffset(abscisse - kGridCenter, depth - kGridCenter));
}

std::int32_t EffectAimQuadrantAction(glm::vec3 instance, glm::vec3 aim,
                                     std::int16_t billboard_turn_degrees,
                                     const EffectActionsByQuarter& actions) {
  constexpr std::int32_t kTurn = 360;
  constexpr std::int32_t kQuarter = 90;
  // La profondeur du client est l'opposée de la nôtre ; l'abscisse, non.
  const std::int32_t vers_l_instance = HeadingOfVector(instance.x - aim.x, -(instance.z - aim.z));
  const std::int32_t camera = (kTurn - static_cast<std::int32_t>(billboard_turn_degrees)) % kTurn;
  std::int32_t angle = (vers_l_instance + camera) % kTurn;
  if (angle < 0) {
    angle += kTurn;
  }
  return actions[static_cast<std::size_t>(angle / kQuarter)];
}

EffectProjectileAngles EffectClientArrowAngles(float start_x, float start_z, float height,
                                               float aim_x, float aim_z) {
  constexpr float kClientPi = 3.141592F;
  constexpr float kHalfTurnDegrees = 180.0F;
  EffectProjectileAngles angles;
  angles.yaw_degrees = -FacingTowardOffset(aim_x - start_x, aim_z - start_z);
  const float distance = std::sqrt(start_x * start_x + start_z * start_z);
  const auto brut =
      static_cast<float>(std::atan2(static_cast<double>(height), static_cast<double>(distance)));
  angles.pitch_degrees = FacingWithinTurn(brut / kClientPi * kHalfTurnDegrees);
  return angles;
}

float EffectProjectileScreenAngle(glm::vec3 flight_direction, float camera_yaw_degrees) {
  // La profondeur du client est l'opposée de la nôtre ; l'abscisse, non.
  const float facing_degrees = FacingTowardOffset(flight_direction.x, -flight_direction.z);
  // Le lacet du client est l'opposé du nôtre : son `−lacet − face` s'écrit
  // `lacet − face`, en flottants.
  return camera_yaw_degrees - facing_degrees;
}

std::int32_t EffectProjectileScreenAngleDegrees(glm::vec3 flight_direction,
                                                float camera_yaw_degrees) {
  constexpr std::int32_t kTurn = 360;
  // L'angle réel se tronque vers zéro comme chez lui, puis revient sur le tour.
  const auto truncated =
      static_cast<std::int32_t>(EffectProjectileScreenAngle(flight_direction, camera_yaw_degrees));
  const std::int32_t remainder = truncated % kTurn;
  return remainder < 0 ? remainder + kTurn : remainder;
}

glm::vec3 EffectClientPitchedHeading(float yaw_degrees, float pitch_degrees) {
  const float cap = NativeTableRadians(static_cast<std::int32_t>(yaw_degrees));
  const float pitch = NativeTableRadians(static_cast<std::int32_t>(pitch_degrees));
  return EffectClientOffsetInWorld(std::cos(pitch) * std::sin(cap), -std::sin(pitch),
                                   std::cos(pitch) * std::cos(cap));
}

glm::vec3 EffectClientAimedHeading(glm::vec3 toward, float face_offset_degrees) {
  // La profondeur du client est l'opposée de la nôtre ; l'abscisse, non.
  const float face =
      FacingWithinTurn(FacingTowardOffset(toward.x, -toward.z) + face_offset_degrees);
  return -OppositeHeadingLine(face);
}

EffectFlightEnds EffectNodeFlightEnds(const EffectNodeSeed& seed, glm::vec3 effect_point,
                                      const std::optional<glm::vec3>& target) {
  if (seed.flies_from_target && target.has_value()) {
    return EffectFlightEnds{*target, effect_point};
  }
  return EffectFlightEnds{effect_point, target};
}

namespace {

// `Rot(axe, angle) · v`, par la formule de Rodrigues — la matrice que le client
// bâtit d'un axe et d'un angle est la rotation standard, rangée pour un vecteur
// LIGNE, ce qui revient au même.
// Un changement de repère à déterminant positif garde le sens des angles ; ce
// qui se retourne, c'est l'AXE, et l'appelant le dit.
[[nodiscard]] glm::vec3 RotatedAroundAxis(glm::vec3 axis, float angle_degrees, glm::vec3 v) {
  const float angle = DegreesToRadians(angle_degrees);
  const float c = std::cos(angle);
  const float s = std::sin(angle);
  return v * c + glm::cross(axis, v) * s + axis * glm::dot(axis, v) * (1.0F - c);
}

}  // namespace

void AdvanceLaunchedNode(PooledEffectNode& pooled, const EffectScatterWorld& world) {
  if (pooled.launch_direction == glm::vec3{0.0F, 0.0F, 0.0F}) {
    return;
  }
  // ⚠ LE PAS NE DEVIENT JAMAIS NÉGATIF : le client le borne par un `fmaxf` avant
  // de s'en servir. Sans la borne, une particule ralentie
  // repartirait EN ARRIÈRE, ce qui se voit tout de suite sur une gerbe.
  pooled.node.step_length = std::max(0.0F, pooled.node.step_length + pooled.step_decay);
  pooled.fall_speed += pooled.fall_acceleration;
  // L'AVANCE S'ACCUMULE : le client rajoute chaque tic le produit de sa
  // direction par son pas, il ne replace pas la particule.
  if (pooled.brakes_along_axis) {
    // LE SOUS-TYPE 8 RETRANCHE SA VITESSE VERTICALE AU PAS — `avance = pas −
    // vitesse` — et ne l'ajoute jamais à l'ordonnée. Notre vitesse
    // est déjà celle du client au signe près, d'où la somme.
    pooled.ground_position +=
        pooled.launch_direction * (pooled.node.step_length + pooled.fall_speed);
  } else {
    pooled.ground_position += pooled.launch_direction * pooled.node.step_length;
    pooled.ground_position.y += pooled.fall_speed;
  }
  if (pooled.swing_speed != 0.0F || pooled.swing_acceleration != 0.0F) {
    // LE BALANCEMENT : le tangage cumule sa vitesse, elle-même freinée, et
    // l'écart se recalcule depuis le cumul — `Rot(axe, roulis) · (0, tangage,
    // 0)`. Le client REMPLACE cet écart à chaque tic ; nous posons
    // sa différence, ce qui revient au même.
    //
    // ⚠ LE TANGAGE PREND LA VITESSE D'AVANT SON FREINAGE, à la différence du pas
    // et de la chute : le client copie la vitesse avant de lui ajouter son
    // freinage, et c'est la copie qu'il cumule.
    pooled.swing_amount += pooled.swing_speed;
    pooled.swing_speed += pooled.swing_acceleration;
    // 🔴 L'AXE DU ROULIS EST À L'ENVERS DU VOL. C'est la troisième ligne de la
    // rotation du cap, et l'avance se RETRANCHE le long de lui : il pointe de
    // la cible vers le nœud. Tourner autour de la direction du vol retournerait
    // l'éventail — la cinquième boule, à cent trente-cinq degrés, sortirait du
    // mauvais côté.
    const glm::vec3 offset =
        RotatedAroundAxis(-pooled.launch_direction, pooled.roll_degrees,
                          EffectClientOffsetInWorld(0.0F, pooled.swing_amount, 0.0F));
    pooled.ground_position += offset - pooled.swing_offset;
    pooled.swing_offset = offset;
  }
  // 🔴 ET UNE FLÈCHE SE PLANTE. Passé sous le sol, le nœud s'arrête net — son
  // pas tombe à zéro — et son âge saute à la vie moins vingt-cinq : il reste
  // planté ce temps-là, puis s'efface. C'est le bit deux du mouvement des
  // sous-types quinze et seize, qui s'efface en même temps ; sans lui la flèche
  // traversait le sol et continuait dessous. Voir `kEffectNodeStopsAtGround`.
  if (pooled.stops_at_floor && pooled.ground_position.y < pooled.floor_y) {
    pooled.stops_at_floor = false;
    pooled.node.step_length = 0.0F;
    pooled.step_decay = 0.0F;
    pooled.fall_speed = 0.0F;
    pooled.fall_acceleration = 0.0F;
    pooled.node.age_ticks =
        std::max(pooled.node.age_ticks, pooled.node.life_ticks - kEffectNodeRestAfterImpactTicks);
  }
  // 🔴 ET UNE PIÈCE REBONDIT. Le client ne teste le sol que si la particule
  // DESCEND — `748 > 0` chez lui, notre ordonnée étant opposée à la sienne —,
  // puis retourne sa vitesse divisée par un et demi. Sans la garde du sens, une
  // particule qui MONTE depuis un point déjà sous le terrain rebondirait à
  // chaque tic et resterait collée au sol.
  if (!pooled.bounces_at_floor || pooled.fall_speed >= 0.0F || !world.ground_height) {
    return;
  }
  // ⚠ LE SOL SE RELIT SOUS LA PARTICULE, pas sous son point de naissance :
  // c'est ce que fait le client, et sur une pente les deux diffèrent.
  const float sol = world.ground_height(pooled.ground_position.x, pooled.ground_position.z);
  if (pooled.ground_position.y >= sol) {
    return;
  }
  pooled.fall_speed /= kEffectNodeBounceDivisor;
  pooled.bounced_this_tick = true;
}

EffectRadialLane StartEffectRadialLane(const EffectNodeSeed& seed, float heading_degrees) {
  EffectRadialLane voie;
  if (seed.radial_speed == 0.0F) {
    return voie;
  }
  // LE CAP EST DÉJÀ DANS NOTRE REPÈRE — voir `EffectClientSinCosHeadingInWorld`
  // —, et la vitesse suit la direction même du bras de dispersion tiré au même
  // angle. La vitesse d'après la bascule est une fraction de cette direction ;
  // la dérive, elle, ne dépend pas du cap.
  const float cap = DegreesToRadians(heading_degrees);
  const glm::vec3 direction{std::cos(cap), 0.0F, std::sin(cap)};
  voie.velocity = direction * seed.radial_speed;
  voie.velocity_after_switch = direction * seed.radial_speed_after_switch;
  voie.acceleration_after_switch = EffectClientOffsetInWorld(seed.radial_drift_after_switch[0],
                                                             seed.radial_drift_after_switch[1],
                                                             seed.radial_drift_after_switch[2]);
  voie.ticks_to_switch = seed.radial_ticks_to_switch;
  return voie;
}

void AdvanceRadialNode(PooledEffectNode& pooled) {
  EffectRadialLane& voie = pooled.radial;
  if (voie.ticks_to_switch == 0) {
    voie.velocity = voie.velocity_after_switch;
    voie.acceleration = voie.acceleration_after_switch;
  }
  if (voie.ticks_to_switch >= 0) {
    --voie.ticks_to_switch;
  }
  voie.velocity += voie.acceleration;
  pooled.ground_position += voie.velocity;
}

}  // namespace ro::render
