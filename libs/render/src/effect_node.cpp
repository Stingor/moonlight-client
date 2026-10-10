#include "ro/render/effect_node.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ro::render {
namespace {

// Le générateur du nœud.
//
// Les constantes sont celles du générateur congruentiel de la bibliothèque C ;
// seuls les bits de poids fort sont lus, les bits bas étant trop réguliers.
constexpr std::uint32_t kDrawMultiplier = 1103515245U;
constexpr std::uint32_t kDrawIncrement = 12345U;
constexpr int kDrawDiscardedLowBits = 16;

[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& generator, std::int32_t bound) {
  const std::uint32_t draw = NextEffectNodeDraw(generator);
  return static_cast<std::int32_t>(draw % static_cast<std::uint32_t>(bound));
}

[[nodiscard]] float DegreesToRadians(float degrees) {
  return degrees * std::numbers::pi_v<float> / 180.0F;
}

// Fait avancer une composante de couleur d'un tic.
//
// Le vert et le bleu se BORNENT ; le rouge s'ENROULE. Voir l'avertissement de
// l'en-tête : l'asymétrie est celle du client, pas la nôtre.
[[nodiscard]] float StepColorChannel(float channel, float step, bool wraps) {
  const float raised = channel + step;
  if (raised > kEffectNodeChannelMax) {
    return wraps ? 0.0F : kEffectNodeChannelMax;
  }
  if (raised < 0.0F) {
    return wraps ? kEffectNodeChannelMax : 0.0F;
  }
  return raised;
}

// LE HAUT DE LA TEINTE QUI TOURNE : un canal qui l'atteint retient les pas, et
// un canal qui le dépasse retombe à zéro.
constexpr float kCyclingChannelTop = 254.0F;

// Fait avancer d'un tic la teinte qui tourne. Voir `EffectNode::cycles_color`.
void StepCyclingColor(EffectNode& node) {
  const bool at_peak =
      std::ranges::any_of(node.color, [](float channel) { return channel == kCyclingChannelTop; });
  bool advance = true;
  if (at_peak) {
    // ⚠ UNE RETENUE NULLE NE COMPTE PAS : le compteur reste à zéro, égal à la
    // retenue, et les pas s'appliquent à chaque tic.
    if (node.color_hold_ticks != 0) {
      ++node.color_hold_count;
    }
    advance = node.color_hold_count == node.color_hold_ticks;
    if (advance) {
      node.color_hold_count = 0;
    }
  }
  for (std::size_t channel = 0; channel < node.color.size(); ++channel) {
    float value = node.color[channel];
    if (advance) {
      value += node.color_step_per_tick[channel];
    }
    if (value > kCyclingChannelTop) {
      value = 0.0F;
    } else if (value < 0.0F) {
      value = kCyclingChannelTop;
    }
    node.color[channel] = value;
  }
}

// Les deux étages s'avancent SÉPARÉMENT, et l'ordre compte : le client fige la
// croissance entre les deux, si bien qu'un quad qui gèle ne grandit pas même du
// dernier pas que sa croissance lui promettait.
void StepExtentGrowth(EffectNodeExtent& extent) {
  extent.growth_per_tick += extent.growth_acceleration_per_tick;
}

void StepExtentSize(EffectNodeExtent& extent) { extent.half_size += extent.growth_per_tick; }

void FreezeExtent(EffectNodeExtent& extent) {
  extent.growth_per_tick = 0.0F;
  extent.growth_acceleration_per_tick = 0.0F;
}

// La rampe d'alpha, telle que le client la conduit.
//
// LE FONDU DE SORTIE SE CALCULE, il ne se stocke pas : au tic de sortie, le pas
// est refait pour que l'alpha COURANT atteigne zéro pile à la fin de vie. Un
// nœud dont l'alpha n'était pas encore monté sort donc plus doucement qu'un
// nœud à plein — et aucun ne se coupe net.
void StepAlpha(EffectNode& node) {
  // 🔴 UN NŒUD QUI PLONGE N'A PAS D'ALPHA À METTRE À JOUR : son mouvement
  // retourne avant d'y venir. Voir `EffectNodeDives`. Sans cette garde, les
  // pointes de terre s'éteindraient en fondu pendant qu'elles montent, et
  // auraient disparu au tic où elles redescendent.
  if (EffectNodeDives(node)) {
    return;
  }
  // LES IMAGES-CLÉS D'ABORD : l'alpha et le plafond repris, la rampe du tic
  // s'applique ensuite à partir d'eux.
  for (const EffectAlphaKeyframe& key : node.alpha_keyframes) {
    if (key.tick >= 0 && key.tick == node.age_ticks) {
      node.alpha = key.alpha;
      node.alpha_max = key.alpha_max;
    }
  }
  if ((node.flags & kEffectNodeAlphaBounces) != 0) {
    const bool still_rising = node.alpha_step_per_tick >= 0.0F ? node.alpha < node.alpha_max
                                                               : node.alpha > node.alpha_min;
    if (!still_rising) {
      node.alpha_step_per_tick = -node.alpha_step_per_tick;
    }
  }

  // LE FONDU QUE L'INSTANCE IMPOSE, jugé à chaque passage tant qu'il ne s'est
  // pas engagé, puis la sortie propre du nœud, qui l'emporte si elle tombe au
  // même tic. Voir `EffectNode::fades_with_its_instance`.
  if (node.fades_with_its_instance && node.owner_fade_at_age == kEffectNodeOwnerFadeUnjudged &&
      node.instance_ticks_left > 0 &&
      node.life_ticks - node.age_ticks - node.instance_ticks_left > 1) {
    node.owner_fade_at_age = node.age_ticks;
    node.owner_fade_over_ticks = node.instance_ticks_left;
  }
  if (node.age_ticks == node.owner_fade_at_age && node.owner_fade_over_ticks > 0) {
    node.flags &= ~kEffectNodeAlphaBounces;
    node.alpha_step_per_tick = -node.alpha / static_cast<float>(node.owner_fade_over_ticks);
  }

  const std::int32_t remaining_ticks = node.life_ticks - node.age_ticks;
  if (node.age_ticks == node.fade_out_tick && remaining_ticks > 0) {
    // La sortie l'emporte sur le va-et-vient : le nœud descend, et ne remonte
    // plus.
    node.flags &= ~kEffectNodeAlphaBounces;
    node.alpha_step_per_tick = -node.alpha / static_cast<float>(remaining_ticks);
  }

  node.alpha = std::clamp(node.alpha + node.alpha_step_per_tick, node.alpha_min, node.alpha_max);
  if (node.alpha < node.alpha_zero_below) {
    node.alpha = 0.0F;
  }
}

// Avance le compteur d'images, et dit si le nœud a épuisé ses textures.
[[nodiscard]] bool FramesExhausted(EffectNode& node) {
  const auto texture_count = static_cast<std::int32_t>(node.texture_count);
  // LA PÉRIODE FRACTIONNAIRE : l'image se déduit de l'âge au lieu de se
  // compter. L'âge lu est celui que le nœud aura APRÈS ce tic — le même que
  // celui contre lequel sa vie est jugée —, et la dernière image le tue.
  if (node.frame_period_ticks > 0.0F) {
    if (texture_count <= 0) {
      return true;
    }
    const auto frame = static_cast<std::int32_t>(
        std::floor(static_cast<float>(node.age_ticks + 1) / node.frame_period_ticks));
    if (node.loops_frames) {
      node.frame = frame % texture_count;
      return false;
    }
    node.frame = std::min(frame, texture_count - 1);
    return frame >= texture_count;
  }
  // LA PREMIÈRE IMAGE SE TIENT D'ABORD, quand le nœud le demande : rien
  // n'avance avant ce tic-là, et la retenue compte ensuite depuis l'âge, comme
  // sans tenue. Voir `EffectNode::first_frame_hold_ticks`.
  if (node.age_ticks < node.first_frame_hold_ticks || node.rested_on_first_frame) {
    return false;
  }
  if (node.frame_hold_ticks <= 0 || node.age_ticks % node.frame_hold_ticks != 0) {
    return false;
  }
  ++node.frame;
  if (texture_count <= 0) {
    return true;
  }
  if (node.loops_frames) {
    node.frame %= texture_count;
    return false;
  }
  if (node.holds_last_frame && node.frame >= texture_count) {
    node.frame = texture_count - 1;
    return false;
  }
  if (node.rests_on_first_frame && node.frame >= texture_count) {
    node.frame = 0;
    node.rested_on_first_frame = true;
    return false;
  }
  return node.frame >= texture_count;
}

// Pose une valeur de la réserve, si elle a été posée.
void ApplyKept(float& live, float kept) {
  if (kept != kEffectNodeKeyframeUnset) {
    live = kept;
  }
}

// LA SECONDE IMAGE-CLÉ, au tic dit et une seule fois.
//
// ⚠ AVANT L'INTÉGRATION DU TIC, comme le client la joue : il compare l'âge au
// tic de bascule en tête de son mouvement, puis intègre. La jouer après la
// ferait porter d'un tic trop tard, ce qui se voit sur un quad qui s'ouvre vite.
void ApplySecondKeyframe(EffectNode& node) {
  if (node.second_keyframe_tick < 0 || node.age_ticks != node.second_keyframe_tick) {
    return;
  }
  ApplyKept(node.half_width.growth_per_tick, node.second_half_width_growth);
  ApplyKept(node.half_width.growth_acceleration_per_tick, node.second_half_width_acceleration);
  ApplyKept(node.half_height.growth_per_tick, node.second_half_height_growth);
  ApplyKept(node.half_height.growth_acceleration_per_tick, node.second_half_height_acceleration);
  // ⚠ LE PAS EST REMPLACÉ, PAS AJOUTÉ. La pointe de terre passe d'un pas de UN
  // à un pas de MOINS UN ET DEUX DIXIÈMES : elle jaillit puis redescend. Ajouter
  // le second au premier l'aurait à peine ralentie.
  ApplyKept(node.step_length, node.second_step_length);
  ApplyKept(node.step_length_change_per_tick, node.second_step_length_change);
  ApplyKept(node.ring_radius.growth_per_tick, node.second_ring_radius_growth);
  ApplyKept(node.ring_radius.growth_acceleration_per_tick, node.second_ring_radius_acceleration);
  // Le client remet la sentinelle dans sa réserve ; nous éteignons le tic, ce
  // qui dit la même chose et se lit mieux : la bascule ne joue qu'une fois.
  node.second_keyframe_tick = -1;
}

// LE CYCLE QUI RELANCE LE PAS, tous les six tics.
//
// ⚠ DEUX PHASES SEULEMENT SUR SIX ÉCRIVENT. Les quatre autres laissent le pas
// courir sous sa propre variation, et c'est ce qui donne à la pierre ses trois
// tics de descente puis ses trois tics de montée. Écrire à chaque tic la
// figerait ; n'écrire qu'une fois en ferait une pointe de terre.
void ApplyStepCycle(EffectNode& node) {
  if (!node.step_cycles || node.age_ticks > kEffectNodeStepCycleLastTick) {
    return;
  }
  const std::int32_t phase = node.age_ticks % kEffectNodeStepCycleTicks;
  // ⚠ LE DERNIER TIC DU CYCLE PORTE LE RETOUR, quelle que soit sa phase : le
  // client le teste à part. Voir `kEffectNodeStepCycleLastTick`.
  if (phase == kEffectNodeStepCycleReturnPhase || node.age_ticks == kEffectNodeStepCycleLastTick) {
    node.step_length = node.second_step_length;
    node.step_length_change_per_tick = node.second_step_length_change;
    return;
  }
  if (phase == kEffectNodeStepCycleLaunchPhase) {
    node.step_length = kEffectNodeStepCycleLaunchStep;
    node.step_length_change_per_tick = kEffectNodeStepCycleLaunchChange;
  }
}

// L'ARRÊT NET DU MOUVEMENT, au tic dit.
//
// ⚠ LE PAS ET SA VARIATION TOMBENT ENSEMBLE. N'annuler que le pas le laisserait
// repartir au tic suivant, poussé par une variation restée en place — c'est ce
// que le client écrit, et ce que la même règle dit déjà du gel de croissance.
void ApplyMotionStop(EffectNode& node) {
  if (node.motion_stop_tick < 0 || node.age_ticks != node.motion_stop_tick) {
    return;
  }
  node.step_length = 0.0F;
  node.step_length_change_per_tick = 0.0F;
}

// LA PLONGÉE, trente tics avant la fin de vie.
//
// ⚠ LE TIC SE CALCULE, il ne se range pas. La vie d'une pointe est TIRÉE : la
// figer dans un champ au semis ferait plonger les longues trop tôt.
void ApplySinkingKeyframe(EffectNode& node) {
  if (!EffectNodeDives(node)) {
    return;
  }
  if (node.age_ticks != node.life_ticks - kEffectNodeSinkingLeadTicks) {
    return;
  }
  ApplyKept(node.step_length, node.sinking_step_length);
  ApplyKept(node.step_length_change_per_tick, node.sinking_step_length_change);
}

}  // namespace

std::uint32_t NextEffectNodeDraw(std::uint32_t& generator) {
  generator = generator * kDrawMultiplier + kDrawIncrement;
  return (generator >> kDrawDiscardedLowBits) & kEffectNodeDrawSpan;
}

bool AdvanceEffectNode(EffectNode& node) {
  // ⚠ L'UN OU L'AUTRE, JAMAIS LES DEUX : ils se disputeraient la même réserve.
  // Le sous-type 117 ne lit pas le tic de bascule, et le 9 n'a pas de cycle.
  if (node.step_cycles) {
    ApplyStepCycle(node);
  } else {
    ApplySecondKeyframe(node);
  }
  node.spin_speed_per_tick += node.spin_acceleration_per_tick;
  StepExtentGrowth(node.half_width);
  StepExtentGrowth(node.half_height);
  node.step_length += node.step_length_change_per_tick;

  // La croissance se FIGE passé son échéance, elle ne s'inverse pas : le pas ET
  // son accélération tombent à zéro, sans quoi la seconde relancerait le
  // premier au tic suivant. La ROTATION, elle, continue.
  if ((node.flags & kEffectNodeFreezesGrowth) != 0 && node.age_ticks >= node.motion_period_ticks) {
    FreezeExtent(node.half_width);
    FreezeExtent(node.half_height);
  }

  StepExtentSize(node.half_width);
  StepExtentSize(node.half_height);
  // La demi-hauteur BUTE sur son plafond, et y reste : c'est ainsi qu'une
  // colonne de lumière s'élève puis s'arrête au lieu de monter sans fin.
  node.half_height.half_size = std::min(node.half_height.half_size, node.half_height_ceiling);
  // LE BATTEMENT DE LA HAUTEUR — voir `kEffectNodeHeightGrowthFlips`. APRÈS
  // l'intégration du tic, comme le client l'écrit : le tic zéro intègre un pas
  // montant, puis retourne le signe.
  if ((node.flags & kEffectNodeHeightGrowthFlips) != 0 && node.height_flip_period_ticks > 0 &&
      node.age_ticks % node.height_flip_period_ticks == 0) {
    node.half_height.growth_per_tick = -node.half_height.growth_per_tick;
  }
  node.spin_degrees += node.spin_speed_per_tick;
  // L'ÉCHELLE D'UN SPRITE AVANCE DE SON PAS, `792 += 796` : c'est ce qui fait
  // rétrécir les boules de feu de Sight le long de leur traînée.
  node.sprite_scale += node.sprite_scale_step;

  // LES RAYONS D'UN ANNEAU ont le même intégrateur à trois étages que les
  // extensions, et le client le leur applique dans le même tic. Le GEL, lui, ne
  // les touche pas : il ne concerne que les deux extensions.
  StepExtentGrowth(node.radius_at_anchor);
  StepExtentGrowth(node.radius_at_height);
  StepExtentGrowth(node.ring_radius);
  StepExtentSize(node.radius_at_anchor);
  StepExtentSize(node.radius_at_height);
  StepExtentSize(node.ring_radius);

  // L'AVANCE S'ACCUMULE, et le mouvement dit dans quoi.
  //
  // ⚠ L'UN OU L'AUTRE, JAMAIS LES DEUX. Le répartiteur du client choisit un
  // mouvement par sous-type : celui de la pointe de terre translate le long de
  // l'axe du nœud, les autres avancent sur leur orbite. Nourrir les deux ferait
  // dériver une pointe dans le plan par-dessus sa montée.
  if (node.translates_along_axis) {
    node.axial_translation += node.step_length;
  } else {
    // L'orbite s'ACCUMULE : le nœud avance chaque tic d'un pas dans la direction
    // de son angle. L'ordonnée descend, comme à l'écran.
    const float heading = DegreesToRadians(node.orbit_degrees);
    node.orbit_x += std::cos(heading) * node.step_length;
    node.orbit_y -= std::sin(heading) * node.step_length;
  }

  // L'ARRÊT, puis la PLONGÉE — dans cet ordre, et APRÈS que l'avance du tic a
  // été comptée, comme le client les joue. Les avancer d'un cran ferait perdre
  // au nœud le dernier pas de sa montée.
  ApplyMotionStop(node);
  ApplySinkingKeyframe(node);

  if (node.cycles_color) {
    StepCyclingColor(node);
  } else {
    for (std::size_t channel = 0; channel < node.color.size(); ++channel) {
      const bool wraps = channel == 0;
      node.color[channel] =
          StepColorChannel(node.color[channel], node.color_step_per_tick[channel], wraps);
    }
  }

  if ((node.flags & kEffectNodeWandersAngle) != 0 && node.motion_period_ticks > 0 &&
      node.age_ticks % node.motion_period_ticks == 0) {
    const std::int32_t deviation = DrawBelow(node.wander_seed, kEffectNodeWanderSpanDegrees) -
                                   kEffectNodeWanderSpanDegrees / 2;
    node.orbit_degrees += static_cast<float>(deviation);
    node.motion_period_ticks = DrawBelow(node.wander_seed, kEffectNodeWanderPeriodSpanTicks) +
                               kEffectNodeWanderPeriodFloorTicks;
  }

  StepAlpha(node);

  const bool exhausted = FramesExhausted(node);
  ++node.age_ticks;

  // Deux fins, et la seconde se DÉDUIT de la rampe : le pas de sortie est
  // calculé pour annuler l'alpha à la fin de vie, donc un nœud ne peut pas
  // survivre à la sienne — la division qui le calcule n'aurait plus de sens.
  // 🔴 IL MEURT QUAND SON ÂGE, UNE FOIS CRÛ, DÉPASSE SA VIE, et non quand il
  // l'atteint : un nœud de vie N se montre N fois, la dernière à l'alpha que
  // sa sortie a mené à zéro. C'est la fin commune à tous les nœuds du client.
  // ⚠ ET L'INSTANCE PEUT L'EMPORTER AVANT : c'est la troupe qui le retire, voir
  // `PooledEffectNode::instance_beats_left`.
  return !exhausted && node.age_ticks <= node.life_ticks;
}

std::uint8_t EffectNodeAlpha(const EffectNode& node) {
  const float bounded = std::clamp(node.alpha, 0.0F, kEffectNodeChannelMax);
  return static_cast<std::uint8_t>(bounded + 0.5F);
}

}  // namespace ro::render
