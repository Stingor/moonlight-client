
#include "ro/render/effect_node_pool.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>

#include "ro/render/effect_cross_spikes.h"
#include "ro/render/effect_launch.h"
#include "ro/render/effect_life_table.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_ring_flight.h"
#include "ro/render/effect_tinted_quad.h"

#include "effect_node_motion.h"

namespace ro::render {

void EffectNodePool::MoveAnchored(std::uint32_t anchor_id, glm::vec3 ground_position,
                                  std::optional<float> owner_body_degrees) {
  if (anchor_id == 0) {
    return;
  }
  for (PooledEffectNode& node : nodes_) {
    if (node.anchor_id != anchor_id) {
      continue;
    }
    // 🔴 LE BOUCLIER ET L'ANNEAU DOUBLE RELISENT L'ANGLE DU CORPS À CHAQUE TIC,
    // avant toute question de place : leur mouvement refait l'azimut depuis le
    // porteur, et la figure pivote avec lui même quand l'effet ne le suit pas
    // pas à pas.
    if (owner_body_degrees.has_value() && (node.shape == PooledNodeShape::kSphereShell ||
                                           node.shape == PooledNodeShape::kDoubleRibbon)) {
      node.owner_facing_degrees = *owner_body_degrees;
    }
    // 🔴 UN NŒUD NE SUIT SON PORTEUR QU'AUX TICS OÙ SON EFFET LE FAIT. Un impact,
    // une bouffée de poison restent où ils sont nés, même accrochés à l'acteur
    // qui les a reçus ; une aura le suit pas à pas ; quelques effets ne le
    // rejoignent qu'à un tic précis, et tout ce qu'ils ont déjà semé saute alors
    // avec eux. Entre deux recalages, la place de l'ancre ne bouge pas non plus :
    // le prochain rattrape tout le chemin parcouru depuis.
    const std::int32_t effect_tick = node.sown_at_effect_tick + node.node.age_ticks;
    if (!EffectFollowsBearerStepByStep(node.bearer_follow, effect_tick) &&
        !EffectRejoinsBearerBetween(node.bearer_follow, node.followed_through_tick, effect_tick)) {
      continue;
    }
    node.followed_through_tick = effect_tick;
    // 🔴 LE POINT DU NŒUD ET CELUI DE SON PORTEUR, du même pas. Quarante-cinq
    // mouvements reposent le nœud sur `owner_position` à chaque tic, comme le
    // client recopie la place de l'instance — que le bâtisseur recale sur le
    // porteur. Laissé à sa place de naissance, ce porteur ramènerait le nœud en
    // arrière à chaque tic : un effet accroché à un acteur qui marche resterait
    // derrière lui.
    //
    // 🔴 ON TRANSLATE DU PAS DU PORTEUR, ON NE REPOSE PAS LE NŒUD. Recalculer sa
    // place — `ancre + écart de naissance` — le ramène à son point de départ à
    // chaque image et efface TOUT ce que son mouvement a accumulé depuis : la
    // montée d'une étincelle de téléportation ne dépasserait jamais un tic, sa
    // vitesse s'intégrant pendant que sa hauteur serait réécrite. L'orbite,
    // elle, survivrait — elle se recalcule depuis son ancre —, ce qui rendrait
    // la panne invisible à l'œil.
    //
    // ⚠ ET LA TRANSLATION GARDE CE QUE L'ÉCART GARDAIT DÉJÀ : la place du nœud
    // dans sa couronne, sa sortie du terrain et son ENFOUISSEMENT. Les pointes de
    // Frost Diver naissent trente unités sous terre et montent de vingt-huit ;
    // recollées au sol, elles montaient de vingt-huit AU-DESSUS de leur cible.
    const glm::vec3 step = ground_position - node.anchor_ground_position;
    node.anchor_ground_position = ground_position;
    if (step == glm::vec3{0.0F, 0.0F, 0.0F}) {
      continue;
    }
    // ⚠ SAUF LA PLUIE QUI RENAÎT : sa place est absolue, et seule sa
    // renaissance relit le porteur. Voir `EffectChasedDriftIsReborn`.
    if (!EffectChasedDriftIsReborn(node.chased_drift)) {
      node.ground_position += step;
    }
    node.owner_position += step;
    // ⚠ ET L'ANCRE DE L'ORBITE AVEC, sans quoi un nœud qui tourne autour d'un
    // point reviendrait à son ancien cercle au tic suivant : sa place se
    // RECALCULE depuis cette ancre, elle ne s'intègre pas.
    node.orbit_anchor += step;
    node.haunt_center += step;
  }
}

void EffectNodePool::MoveTargeted(std::uint32_t target_actor_id, glm::vec3 ground_position) {
  if (target_actor_id == 0) {
    return;
  }
  for (PooledEffectNode& node : nodes_) {
    if (node.target_actor_id == target_actor_id) {
      node.target_position = ground_position;
    }
  }
}

std::size_t EffectNodePool::DropOldestFromEffect(std::uint32_t effect_id, std::uint32_t anchor_id,
                                                 std::size_t count) {
  // ⚠ LA TROUPE GARDE L'ORDRE DES NAISSANCES : `Add` pousse en queue et le
  // compactage d'`Advance` ne réordonne rien. Les premiers trouvés sont donc les
  // plus anciens, et le compactage d'ici garde l'ordre des survivants.
  std::size_t dropped = 0;
  std::size_t kept = 0;
  for (std::size_t index = 0; index < nodes_.size(); ++index) {
    const PooledEffectNode& node = nodes_[index];
    if (dropped < count && node.sown_by_effect_id == effect_id && node.anchor_id == anchor_id) {
      ++dropped;
      continue;
    }
    if (kept != index) {
      nodes_[kept] = std::move(nodes_[index]);
    }
    ++kept;
  }
  nodes_.erase(nodes_.begin() + static_cast<std::ptrdiff_t>(kept), nodes_.end());
  return dropped;
}

bool EffectNodePool::HasTargeted(std::uint32_t target_actor_id) const {
  return target_actor_id != 0 &&
         std::ranges::any_of(nodes_, [target_actor_id](const PooledEffectNode& node) {
           return node.target_actor_id == target_actor_id;
         });
}

bool EffectNodePool::HasAnchored(std::uint32_t anchor_id) const {
  return anchor_id != 0 && std::ranges::any_of(nodes_, [anchor_id](const PooledEffectNode& node) {
           return node.anchor_id == anchor_id;
         });
}

bool EffectNodeScheduleSowsAt(const EffectNodeSchedule& schedule, std::int32_t tick) {
  // UN ÂGE QUI REPART À ZÉRO REJOUE L'HORAIRE : le tic se lit dans le cycle.
  if (schedule.cycle_ticks > 0) {
    tick %= schedule.cycle_ticks;
  }
  if (tick < schedule.first_tick) {
    return false;
  }
  if (schedule.period_ticks <= 0) {
    return tick == schedule.first_tick;
  }
  // LA PHASE DÉCALE LE MODULO : le client écrit parfois `(âge + 5) % 11`, et
  // sans elle les boules de Soul Strike naissaient aux multiples de onze au
  // lieu du sixième tic. Nulle presque partout.
  return tick <= schedule.last_tick && (tick + schedule.phase_ticks) % schedule.period_ticks == 0;
}

std::int32_t EffectNodeScheduleLastSowTick(const EffectNodeSchedule& schedule) {
  // ⚠ UN CYCLE NE FINIT PAS : c'est le retrait de l'effet, ou le départ de son
  // porteur, qui arrête son horloge. L'infini s'écrit comme le client l'écrit.
  if (schedule.cycle_ticks > 0) {
    return kEffectLifeForever;
  }
  return schedule.period_ticks > 0 ? schedule.last_tick : schedule.first_tick;
}
namespace {

// Un tirage rendu à sa plage. Une plage vide donne sa borne basse plutôt qu'un
// tirage inutile : c'est ainsi qu'un patron dit « toujours cette valeur ».
// UN ENTIER DE `bas` À `haut` COMPRIS, comme `rand() % (haut − bas + 1) + bas`.
[[nodiscard]] float DrawInclusiveInteger(float low, float high, std::uint32_t& generator) {
  const auto bottom = static_cast<std::int32_t>(low);
  const auto etendue = static_cast<std::uint32_t>(static_cast<std::int32_t>(high) - bottom + 1);
  return static_cast<float>(bottom +
                            static_cast<std::int32_t>(NextEffectNodeDraw(generator) % etendue));
}

[[nodiscard]] float DrawWithin(const EffectNodeRange& range, std::uint32_t& generator) {
  return DrawEffectNodeRange(range, generator);
}

// La valeur tirée d'avance pour cette sorte, ou un tirage maintenant.
[[nodiscard]] float PredrawnOrDrawn(const EffectNodeSeed& seed, EffectSpriteDraw kind,
                                    const EffectNodeRange& range, std::uint32_t& generator) {
  const std::optional<float> prise = EffectSpritePredrawnValue(seed.predrawn, kind);
  return prise.has_value() ? *prise : DrawWithin(range, generator);
}

// FAIT TOURNER UN NŒUD AUTOUR DE SON POINT DE NAISSANCE, d'un tic.
//
// 🔴 LA POSITION SE RECALCULE, ELLE NE S'INTÈGRE PAS. Un lancer ajoute un pas à
// la place courante ; une orbite la reconstruit depuis l'angle et le rayon, et
// c'est la seule façon de rester à distance constante — un pas tangent ferait
// spiraler la particule vers l'extérieur.
//
// ⚠ LA HAUTEUR NE SE RECALCULE PAS : ce que le vol a accumulé en ordonnée
// reste. La recomposer ferait retomber au sol une étincelle qui monte.
//
// Un rayon nul ou un pas nul ne fait rien, et c'est le cas de presque tous les
// nœuds de la troupe.
void OrbitPooledNode(PooledEffectNode& pooled) {
  // ⚠ UN BRAS QUI SE RÉTRACTE PEUT PASSER SOUS ZÉRO et continuer de l'autre
  // côté, comme chez le client : seul un bras nul qui ne bouge pas s'arrête.
  if ((pooled.orbit_radius == 0.0F && pooled.orbit_radius_step_per_tick == 0.0F) ||
      (pooled.orbit_degrees_per_tick == 0.0F && pooled.orbit_acceleration_per_tick == 0.0F)) {
    return;
  }
  // LE BRAS S'ALLONGE AVANT QUE LA PLACE NE SE RECOMPOSE, comme chez le client :
  // il intègre le pas du bras dans la distance, puis bâtit sa matrice. Le pas
  // angulaire, lui, accélère avant que le cap n'avance.
  pooled.orbit_radius += pooled.orbit_radius_step_per_tick;
  // LE PAS DU BRAS SE REMPLACE à l'âge dit, après l'avance de ce tic-là. L'âge
  // du nœud a déjà été compté pour ce tic : d'où le un.
  if (pooled.orbit_radius_step_switch_tick >= 0 &&
      pooled.node.age_ticks == pooled.orbit_radius_step_switch_tick + 1) {
    pooled.orbit_radius_step_per_tick = pooled.orbit_radius_step_after_switch;
  }
  pooled.orbit_degrees_per_tick += pooled.orbit_acceleration_per_tick;
  pooled.node.orbit_degrees += pooled.orbit_degrees_per_tick;
  const float travelled = std::trunc(pooled.node.orbit_degrees - pooled.orbit_start_degrees);
  const float cap = (pooled.orbit_start_degrees + travelled) * std::numbers::pi_v<float> / 180.0F;
  const float height = pooled.ground_position.y;
  pooled.ground_position =
      pooled.orbit_anchor +
      glm::vec3{std::cos(cap) * pooled.orbit_radius, 0.0F, std::sin(cap) * pooled.orbit_radius};
  pooled.ground_position.y = height;
}

// FAIT AVANCER LA RONDE D'UNE APPARITION d'un tic, et en recopie l'effet dans
// le nœud : sa place, son alpha, son action.
//
// ⚠ LE CAP DE LA CAMÉRA EST LE TOUR DE PANNEAU que la troupe reçoit, tel quel :
// `360 − lacet` du client, qui vaut notre lacet sur le tour — voir
// `EffectBillboardTurnDegrees`. Le fantôme choisit sa face d'après lui.
void HauntPooledNode(PooledEffectNode& pooled, std::int16_t turn_degrees,
                     std::uint32_t& generator) {
  if (pooled.haunt.flight == EffectHauntFlight::kNone) {
    return;
  }
  const auto cap_de_la_camera = static_cast<std::int32_t>(turn_degrees);
  const glm::vec3 offset = AdvanceEffectHaunt(pooled.haunt, cap_de_la_camera, generator);
  pooled.ground_position =
      pooled.haunt_center + EffectClientOffsetInWorld(offset.x, offset.y, offset.z);
  pooled.node.alpha = pooled.haunt.alpha;
  pooled.node.action = pooled.haunt.action;
}

// Au-delà, la troupe abandonne : voir `kEffectNodeMaxCatchUpTicks`. Borner ici
// évite en outre que le temps dû ne déborde.
constexpr std::uint32_t kMaxElapsedMs = 1000;

// LE MARQUEUR D'UN NOM DE SON FORMÉ, et la première variante que le client
// numérote. Il écrit `rand() % n + 1` : les fichiers commencent à UN.
constexpr std::string_view kSoundVariantMark = "%d";
constexpr std::uint32_t kFirstSoundVariant = 1;

// LA LOI DE L'AMPOULE, telle que l'effet 165 la pose :
// un tirage sous DIX, l'action par sa moitié — cinq couleurs, deux tirages
// chacune —, et l'ampoule ÉTEINTE quand son reste par trois vaut un. Voir
// `EffectNodePattern::draws_bulb`.
constexpr std::uint32_t kBulbDraws = 10;
constexpr std::int32_t kBulbDrawsPerColour = 2;
constexpr std::int32_t kBulbDarkEvery = 3;
constexpr std::int32_t kBulbDarkRemainder = 1;
// Un centième : l'ampoule est là, et ne se voit pas.
constexpr float kBulbDarkScale = 0.01F;

// LE NOM D'UN SON DONT LA VARIANTE SE TIRE. Un nom sans marqueur se rend tel
// quel : c'est un son unique, et le tirage ne doit alors PAS avancer — il
// changerait tout ce que la troupe tire ensuite.
[[nodiscard]] std::string BuildSoundVariant(const std::string& modele, std::uint8_t variantes,
                                            std::uint32_t& generator) {
  const std::size_t mark = modele.find(kSoundVariantMark);
  if (mark == std::string::npos || variantes == 0) {
    return modele;
  }
  const std::uint32_t rang =
      kFirstSoundVariant + NextEffectNodeDraw(generator) % static_cast<std::uint32_t>(variantes);
  return modele.substr(0, mark) + std::to_string(rang) +
         modele.substr(mark + kSoundVariantMark.size());
}

}  // namespace

float DrawEffectNodeRange(const EffectNodeRange& range, std::uint32_t& generator) {
  // ⚠ UNE BORNE HAUTE NULLE DIT « PAS DE TIRAGE », même sous une borne basse
  // négative : c'est ainsi que le générateur écrit une valeur fixe. Aucun
  // tirage relevé du client ne finit à zéro.
  if (range.high <= range.low || range.high == 0.0F) {
    return range.low;
  }
  if (range.values > 1) {
    const auto rang = NextEffectNodeDraw(generator) % static_cast<std::uint32_t>(range.values);
    return range.low + static_cast<float>(rang) * (range.high - range.low) /
                           static_cast<float>(range.values - 1);
  }
  const float part =
      static_cast<float>(NextEffectNodeDraw(generator)) / static_cast<float>(kEffectNodeDrawSpan);
  return range.low + (range.high - range.low) * part;
}

std::int32_t DrawEffectNodeInteger(std::int32_t low, std::int32_t high, std::uint32_t& generator) {
  if (high <= low) {
    return low;
  }
  const auto etendue = static_cast<std::uint32_t>(high - low + 1);
  return low + static_cast<std::int32_t>(NextEffectNodeDraw(generator) % etendue);
}

bool EffectNodeShapeDrawsASprite(PooledNodeShape shape) {
  // ⚠ CETTE LISTE SE TIENT À LA MAIN, et une forme de sprite qu'on oublie d'y
  // mettre ne se dessine PAS — elle cherche son `.spr` parmi les textures. Le
  // signe qui doit alerter est le compteur « sans texture » de la fenêtre : il
  // monte d'un par nœud et par image, sans que rien d'autre ne se plaigne.
  // ⚠ ET LE SPRITE DE MONSTRE EN EST, quoique ajouté en dernier : sa propre
  // fiche le disait — « le premier nœud de la famille qui passe par les
  // ACTEURS » — et il manquait pourtant ici. Quinze identifiants semaient leur
  // nœud, cherchaient leur `.spr` parmi les textures, se comptaient « sans
  // texture » et ne paraissaient jamais. C'est le juge des effets qui l'a levé,
  // par le compteur que ce commentaire désigne.
  return shape == PooledNodeShape::kSprite || shape == PooledNodeShape::kSpriteFan ||
         shape == PooledNodeShape::kFadingSprite || shape == PooledNodeShape::kGlowSprite;
}

std::uint32_t EffectNodeQueuedRenderWord(PooledNodeShape shape, std::uint32_t node_word,
                                         std::uint8_t first_track_mark) {
  switch (shape) {
    case PooledNodeShape::kCrossSpikes:
      return first_track_mark == kCrossSpikeDrawnColor
                 ? kEffectRenderWordTranslucent | kEffectRenderWordOwnBlend
                 : kEffectRenderWordOfConstructor;
    case PooledNodeShape::kRingBand:
    case PooledNodeShape::kCrossedPlanes:
    case PooledNodeShape::kUvSphere:
      return kEffectRenderWordOfConstructor;
    default:
      return node_word;
  }
}

void EffectNodeFadesWithARewritingInstance(EffectNode& node, const EffectInstanceRewrite& rewrite) {
  if (rewrite.base <= 0 || rewrite.period_ticks <= 0) {
    return;
  }
  // 🔴 JUGÉ ICI, ET UNE FOIS : la troupe lirait la vie de la table, que cette
  // instance ne garde pas.
  node.owner_fade_at_age = kEffectNodeOwnerFadeNever;
  const std::int32_t sown = rewrite.sown_at_tick;
  // Au passage p, la vie écrite vaut `base − (dernier semis ≤ p)` et l'instance
  // a déjà l'âge p + 1 quand le nœud la lit. Le reste du nœud,
  // `vie − (p − naissance)`, dépasse le sien plus un dès que ce dernier semis
  // atteint `base + 1 − vie du nœud − naissance`.
  const std::int32_t threshold = rewrite.base + 1 - node.life_ticks - sown;
  const std::int32_t last_rewrite = sown / rewrite.period_ticks * rewrite.period_ticks;
  const std::int32_t engaged_at =
      last_rewrite >= threshold
          ? sown
          : (threshold + rewrite.period_ticks - 1) / rewrite.period_ticks * rewrite.period_ticks;
  // ⚠ LE PASSAGE DE SA MORT NE MET PLUS SES NŒUDS À JOUR : voir
  // `EffectInstanceRewrite::death_tick`.
  if (engaged_at >= rewrite.death_tick - 1) {
    return;
  }
  const std::int32_t written_life =
      rewrite.base - engaged_at / rewrite.period_ticks * rewrite.period_ticks;
  node.owner_fade_at_age = engaged_at - sown;
  node.owner_fade_over_ticks = written_life - (engaged_at + 1);
}

void EffectNodeFadesWithACyclingInstance(EffectNode& node, const EffectInstanceCycle& cycle) {
  if (cycle.life_ticks <= 0) {
    return;
  }
  node.owner_fade_at_age = kEffectNodeOwnerFadeNever;
  // À sa première mise à jour, l'instance a déjà l'âge `naissance + 1`.
  const std::int32_t instance_ticks_left = cycle.life_ticks - (cycle.sown_at_cycle_tick + 1);
  if (instance_ticks_left <= 0 || node.life_ticks - instance_ticks_left <= 1) {
    return;
  }
  node.owner_fade_at_age = 0;
  node.owner_fade_over_ticks = instance_ticks_left;
}

void PredrawEffectSprite(EffectNodeSeed& seed, EffectNodeRange sprite_scale,
                         std::uint32_t& generator) {
  if (seed.draw_order.count == 0) {
    return;
  }
  const auto ranger = [&seed](EffectSpriteDraw kind, float value) {
    seed.predrawn[static_cast<std::size_t>(kind)] = value;
  };
  // Ce que l'ordre ne nomme pas prend sa borne basse, sans tirer.
  ranger(EffectSpriteDraw::kLife, static_cast<float>(seed.life_ticks));
  ranger(EffectSpriteDraw::kStep, seed.step_length.low);
  ranger(EffectSpriteDraw::kArmAngle, seed.orbit_degrees.low);
  ranger(EffectSpriteDraw::kScale, sprite_scale.low);
  ranger(EffectSpriteDraw::kLaunchYaw, seed.launch_yaw_degrees.low);
  ranger(EffectSpriteDraw::kLaunchPitch, seed.launch_pitch_degrees.low);
  ranger(EffectSpriteDraw::kLaunchDistance, seed.launch_distance.low);
  ranger(EffectSpriteDraw::kFallSpeed, seed.fall_speed.low);
  ranger(EffectSpriteDraw::kFallAcceleration, seed.fall_acceleration.low);
  for (std::size_t rang = 0; rang < seed.draw_order.count; ++rang) {
    const EffectSpriteDraw kind = seed.draw_order.draws[rang];
    switch (kind) {
      case EffectSpriteDraw::kLife:
        ranger(kind, static_cast<float>(
                         DrawEffectNodeInteger(seed.life_ticks, seed.life_ticks_high, generator)));
        break;
      case EffectSpriteDraw::kStep:
        ranger(kind, DrawWithin(seed.step_length, generator));
        break;
      case EffectSpriteDraw::kArmAngle:
        ranger(kind, DrawWithin(seed.orbit_degrees, generator));
        break;
      case EffectSpriteDraw::kScale:
        ranger(kind, DrawWithin(sprite_scale, generator));
        break;
      case EffectSpriteDraw::kLaunchYaw:
        ranger(kind, DrawWithin(seed.launch_yaw_degrees, generator));
        break;
      case EffectSpriteDraw::kLaunchPitch:
        ranger(kind, DrawWithin(seed.launch_pitch_degrees, generator));
        break;
      case EffectSpriteDraw::kLaunchDistance:
        ranger(kind, DrawWithin(seed.launch_distance, generator));
        break;
      case EffectSpriteDraw::kFallSpeed:
        ranger(kind, DrawWithin(seed.fall_speed, generator));
        break;
      case EffectSpriteDraw::kFallAcceleration:
        ranger(kind, DrawWithin(seed.fall_acceleration, generator));
        break;
      case EffectSpriteDraw::kDiscarded:
      case EffectSpriteDraw::kCount:
        static_cast<void>(NextEffectNodeDraw(generator));
        break;
    }
  }
}

EffectNode SowEffectNode(const EffectNodeSeed& seed, std::uint32_t& generator) {
  EffectNode node;
  // LA VIE SE TIRE QUAND ELLE A UNE BORNE HAUTE, et elle se tire AVANT tout le
  // reste : c'est l'ordre du client, et deux nœuds tirés dans un autre ordre ne
  // se ressemblent plus.
  // ⚠ ET C'EST UN ENTIER DU CLIENT : chaque vie de la plage a la même chance.
  // ⚠ SAUF QUAND ELLE A ÉTÉ TIRÉE D'AVANCE, dans l'ordre de son bâtisseur.
  const std::optional<float> life =
      EffectSpritePredrawnValue(seed.predrawn, EffectSpriteDraw::kLife);
  node.life_ticks = life.has_value()
                        ? static_cast<std::int32_t>(*life)
                        : DrawEffectNodeInteger(seed.life_ticks, seed.life_ticks_high, generator);
  node.fade_out_tick = std::max(seed.life_ticks - seed.fade_out_ticks, 0);
  // ⚠ SAUF QUAND LE BÂTISSEUR LA RAPPORTE À LA VIE QU'IL VIENT DE TIRER, en
  // entiers : voir `EffectNodeSupplement::fade_out_life_divisor`.
  if (seed.fade_out_life_divisor > 0) {
    node.fade_out_tick = node.life_ticks - node.life_ticks / seed.fade_out_life_divisor;
  }
  node.alpha_max = seed.alpha_max;
  node.color = seed.color;
  // La MONTÉE se dit en durée, et le pas s'en déduit : changer le plafond garde
  // alors la même durée au lieu de changer les deux.
  node.alpha_step_per_tick = seed.alpha_rise_ticks > 0
                                 ? seed.alpha_max / static_cast<float>(seed.alpha_rise_ticks)
                                 : seed.alpha_max;
  // LE DÉPART ET LE PAS DU CLIENT passent devant la durée : voir
  // `EffectNodeSeed::alpha_start`.
  if (seed.alpha_start.has_value()) {
    node.alpha = seed.alpha_start_high > *seed.alpha_start
                     ? DrawWithin(EffectNodeRange{*seed.alpha_start, seed.alpha_start_high,
                                                  seed.alpha_start_values},
                                  generator)
                     : *seed.alpha_start;
    node.alpha_step_per_tick = seed.alpha_step;
  }
  node.half_width.half_size = DrawWithin(seed.half_width, generator);
  node.half_height.half_size = DrawWithin(seed.half_height, generator);
  node.step_length = PredrawnOrDrawn(seed, EffectSpriteDraw::kStep, seed.step_length, generator);
  node.orbit_degrees =
      PredrawnOrDrawn(seed, EffectSpriteDraw::kArmAngle, seed.orbit_degrees, generator);
  node.flags = seed.flags;
  node.action = seed.action;
  // L'ACTION TIRÉE, sur une plage, à pile ou face, ou par la loi de l'AMPOULE :
  // voir `EffectNodePattern::action_high` et `draws_bulb`.
  if (seed.draws_bulb) {
    // ⚠ UN SEUL TIRAGE POUR LES DEUX, et c'est ce qui les lie : l'ampoule
    // éteinte n'est pas tirée à part de sa couleur.
    const auto drawn = static_cast<std::int32_t>(NextEffectNodeDraw(generator) % kBulbDraws);
    node.action = drawn / kBulbDrawsPerColour;
    if (drawn % kBulbDarkEvery == kBulbDarkRemainder) {
      node.sprite_scale = kBulbDarkScale;
    }
  } else if (seed.action_high > seed.action) {
    const auto width = static_cast<std::uint32_t>(seed.action_high - seed.action + 1);
    node.action = seed.action + static_cast<std::int32_t>(NextEffectNodeDraw(generator) % width);
  } else if (seed.action_alternate >= 0 && NextEffectNodeDraw(generator) % 2U != 0U) {
    node.action = seed.action_alternate;
  }
  node.sprite_scale_step = seed.sprite_scale_step;
  node.spin_degrees = seed.spin_degrees;
  node.draws_rotated = seed.draws_rotated;
  // 🔴 LE ROULIS EST AUSSI L'ANGLE D'ÉCRAN DU SPRITE. C'est le même angle :
  // le balancement tourne autour du vol de cet angle, et le dessinateur de
  // sprites l'ajoute à celui de chaque calque.
  // Les âmes d'un Soul Strike à plusieurs coups sont donc tournées chacune de
  // son roulis, et non toutes droites.
  if (seed.roll_degrees != 0.0F) {
    node.spin_degrees += seed.roll_degrees;
    node.draws_rotated = true;
  }
  // ET LE SPRITE PEUT TOURNER DE LUI-MÊME, tic après tic : c'est le même angle,
  // qui avance de sa vitesse — voir `AdvanceEffectNode`.
  if (seed.spin_speed_per_tick != 0.0F) {
    node.spin_speed_per_tick = seed.spin_speed_per_tick;
    node.draws_rotated = true;
  }
  node.draws_layer_angles = seed.draws_layer_angles;
  node.texture_count = seed.texture_count;
  node.loops_frames = seed.loops_frames;
  node.frame_period_ticks = seed.frame_period_ticks;
  node.frame_hold_ticks =
      DrawEffectNodeInteger(seed.frame_hold_ticks, seed.frame_hold_ticks_high, generator);
  node.holds_last_frame = seed.holds_last_frame;
  node.rests_on_first_frame = seed.rests_on_first_frame;
  node.first_frame_hold_ticks = seed.first_frame_hold_ticks;
  node.spins_by_exact_degrees = seed.spins_by_exact_degrees;
  node.render_word = seed.render_word;
  // LA TEINTE TIRÉE, en dernier : un nœud qui ne tire pas sa teinte garde la
  // même suite de tirages qu'avant. Voir `EffectNodePattern::color_high`.
  // ⚠ EN ENTIERS, bornes comprises : le client tire `rand() % n + bas`, et un
  // canal ne prend que des valeurs entières.
  if (seed.color_draw_shared && seed.color_high[0] > seed.color[0]) {
    const float grey = DrawInclusiveInteger(seed.color[0], seed.color_high[0], generator);
    node.color = {grey, grey, grey};
  } else {
    for (std::size_t canal = 0; canal < node.color.size(); ++canal) {
      if (seed.color_high[canal] > seed.color[canal]) {
        node.color[canal] =
            DrawInclusiveInteger(seed.color[canal], seed.color_high[canal], generator);
      }
    }
  }
  // ET IL S'ÉTEINT AVEC SON EFFET s'il lui survivrait : la troupe le juge à
  // chaque mise à jour, voir `EffectNode::fades_with_its_instance`. Une instance
  // qui se récrit une vie, ou dont l'âge reboucle, se juge dès le semis.
  node.fades_with_its_instance = true;
  if (seed.instance_rewrite.base > 0) {
    EffectNodeFadesWithARewritingInstance(node, seed.instance_rewrite);
  } else if (seed.instance_cycle.life_ticks > 0) {
    EffectNodeFadesWithACyclingInstance(node, seed.instance_cycle);
  }
  // Chaque nœud emporte SA graine, prise dans l'état courant du générateur :
  // deux nœuds semés à la suite errent donc différemment, et la troupe entière
  // se rejoue depuis la graine d'origine.
  node.wander_seed = generator;
  return node;
}

std::int32_t EffectNodePool::InstanceLifeBeatsLeft() const {
  // 🔴 L'INSTANCE EMPORTE SES NŒUDS : quand elle meurt, chacun perd son
  // propriétaire et meurt à sa mise à jour suivante, quelle que soit sa propre
  // vie. Elle meurt à la mise à jour où son âge atteint sa vie ; un nœud semé
  // à l'âge t, et mis à jour une première fois dans le même passage, en reçoit
  // donc vie − 1 − t. Une échéance, elle, est une heure et non une vie.
  if (next_effect_ == 0 || next_life_exact_ > 0 || !EffectInstanceLifeIsKnown(next_effect_)) {
    return -1;
  }
  return std::max(
      0, std::max(EffectInstanceLifeFor(next_effect_), next_life_floor_) - 1 - next_effect_tick_);
}

std::int32_t EffectNodePool::InstanceBeatsLeft() const {
  // ⚠ UN PORTEUR QUI MEURT AVANT L'INSTANCE L'EMPORTE PLUS TÔT : la même règle
  // que `InstanceLifeBeatsLeft`, avec la mort du porteur à la place de la vie
  // de l'instance.
  const std::int32_t by_bearer_ticks =
      next_bearer_death_tick_ > 0 ? std::max(0, next_bearer_death_tick_ - 1 - next_effect_tick_)
                                  : -1;
  const std::int32_t by_own_life_ticks = InstanceLifeBeatsLeft();
  if (by_own_life_ticks < 0) {
    return by_bearer_ticks;
  }
  return by_bearer_ticks < 0 ? by_own_life_ticks : std::min(by_own_life_ticks, by_bearer_ticks);
}

std::int32_t EffectNodePool::Advance(std::uint32_t elapsed_ms) {
  const std::int32_t ticks = TakeDueTicks(elapsed_ms);
  // UNE IMAGE SANS TIC DÛ fait tout de même battre les nœuds qui battent à
  // chaque image, et eux seuls, et vieillir les instances qui battent à chaque
  // image. Voir `effect_frame_beat.h`.
  const bool single_frame = ticks == 0;
  for (std::int32_t pass = 0; pass < (single_frame ? 1 : ticks); ++pass) {
    Pass(single_frame);
  }
  FinishFrame();
  // Le compte des TICS, non des passes : une image seule n'en bat aucun.
  return ticks;
}

std::int32_t EffectNodePool::TakeDueTicks(std::uint32_t elapsed_ms) {
  owed_ms_ += std::min(elapsed_ms, kMaxElapsedMs);
  const auto tick_ms = static_cast<std::uint32_t>(kEffectTickMs);
  const auto ticks = static_cast<std::int32_t>(owed_ms_ / tick_ms);
  owed_ms_ %= tick_ms;
  return std::min(ticks, kEffectNodeMaxCatchUpTicks);
}

void EffectNodePool::Pass(bool single_frame) {
  // Les morts se retirent en TASSANT plutôt qu'en effaçant un à un : une
  // troupe de buff meurt par grappes, et chaque effacement ferait glisser
  // tout ce qui suit.
  std::size_t kept = 0;
  for (std::size_t index = 0; index < nodes_.size(); ++index) {
    // L'INSTANCE QUI L'A SEMÉ VIEILLIT, et l'emporte en mourant. Une image
    // sans tic ne fait vieillir que celles qui battent à chaque image.
    std::int32_t& remainder = nodes_[index].instance_beats_left;
    std::int32_t& life_left = nodes_[index].instance_life_beats_left;
    const bool l_instance_bat =
        !single_frame || EffectInstanceBeatsEveryFrame(nodes_[index].sown_by_effect_id);
    if (life_left >= 0 && l_instance_bat) {
      --life_left;
    }
    if (remainder >= 0 && l_instance_bat && --remainder < 0) {
      continue;
    }
    const bool bat = !single_frame || EffectBeatsEveryFrame(nodes_[index].sown_by_effect_id);
    // CE QUI RESTE À L'INSTANCE, TEL QUE LE NŒUD LE LIT : un passage de plus
    // que ce qu'il recevra encore de mises à jour, son âge ayant déjà crû.
    // Voir `EffectNode::instance_ticks_left`.
    nodes_[index].node.instance_ticks_left = life_left >= 0 ? life_left + 1 : 0;
    if (bat && !AdvanceEffectNode(nodes_[index].node)) {
      continue;
    }
    if (!bat) {
      if (kept != index) {
        nodes_[kept] = std::move(nodes_[index]);
      }
      ++kept;
      continue;
    }
    // LE NŒUD PIVOTE SUR LUI-MÊME, quand son patron le dit. Le pas s'ajoute
    // au cap DEUX fois par tic chez le client, et son propre changement au pas
    // une seule. Voir `PooledEffectNode::node_yaw_step`.
    nodes_[index].node_yaw_step += nodes_[index].node_yaw_step_change;
    nodes_[index].node_yaw_degrees +=
        nodes_[index].yaw_steps_per_tick * nodes_[index].node_yaw_step;
    // ET LE NŒUD PEUT TOURNER AUTOUR D'UN POINT au lieu de sur lui-même : sa
    // place se RECALCULE alors depuis son angle, elle ne s'intègre pas. Voir
    // `PooledEffectNode::orbit_radius`.
    OrbitPooledNode(nodes_[index]);
    // OU LA RONDE D'UNE APPARITION, qui fait les trois à la fois.
    HauntPooledNode(nodes_[index], turn_degrees_, draw_);
    // OU LA DÉRIVE D'UN PÉTALE, qui s'intègre. Voir `EffectChasedDrift`.
    nodes_[index].ground_position += AdvanceEffectChasedDrift(nodes_[index].chased_drift, draw_);
    static_cast<void>(RebirthEffectChasedDrift(nodes_[index].chased_drift,
                                               nodes_[index].ground_position,
                                               nodes_[index].owner_position, draw_));
    // LES PISTES BATTENT AU MÊME TIC QUE LE NŒUD, et par ce chemin-là : les
    // faire avancer au moment de composer les figerait dès qu'une image est
    // sautée, et les ferait courir deux fois si l'on compose deux fois.
    AdvanceTracksOf(nodes_[index], world_, draw_);
    // ET UN NŒUD ARRIVÉ À SA CIBLE MEURT DANS LE TIC, sans une image de plus.
    // Voir `EffectNodeArrivedAtTarget`.
    if (EffectNodeArrivedAtTarget(nodes_[index])) {
      continue;
    }
    // LE REBOND SE RAMASSE ICI, au tic où il a lieu : une image en avance
    // parfois plusieurs, et un témoin relu à la fin n'en rendrait qu'un.
    if (nodes_[index].bounced_this_tick) {
      nodes_[index].bounced_this_tick = false;
      if (const std::string& sound = nodes_[index].bounce_sound; !sound.empty()) {
        bounces_.push_back(
            EffectNodeSound{BuildSoundVariant(sound, nodes_[index].bounce_sound_draws, draw_),
                            nodes_[index].ground_position});
      }
    }
    if (kept != index) {
      nodes_[kept] = std::move(nodes_[index]);
    }
    ++kept;
  }
  nodes_.resize(kept);
}

void EffectNodePool::FinishFrame() {
  // LE PLAFOND QUE LE DESSIN DU CARRÉ TEINTÉ ÉCRIT, sur la carte qui le demande.
  //
  // ⚠ UNE FOIS PAR IMAGE, APRÈS TOUS SES TICS, et non à chaque tic : le client
  // l'écrit en dessinant, et la fenêtre compose juste après cette avance. Sur
  // une image qui rattrape deux tics, le second part de ce que le premier a
  // laissé, et le plafond ne tombe qu'ensuite. Un nœud semé depuis l'image
  // précédente, même sans tic dû, ne se montre jamais au-dessus.
  // ⚠ ET SEULEMENT SUR CE QUI SE DESSINE : voir `SetDrawGate`.
  if (world_.caps_tinted_quad_alpha) {
    for (PooledEffectNode& pooled : nodes_) {
      if (DrawnAsTintedQuad(pooled.shape) && IsShown(pooled)) {
        CapTintedQuadAlpha(pooled.tracks);
      }
    }
  }
}

}  // namespace ro::render
