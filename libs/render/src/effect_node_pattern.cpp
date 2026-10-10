#include <algorithm>
#include <cmath>
#include <limits>

#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node_pattern_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_ring_flight.h"

namespace ro::render {
namespace {

// Le tour complet, en degrés : le lacet d'un nœud s'y tire.
constexpr std::uint32_t kEffectNodeYawTurnDegrees = 360;

// Une extension rendue à sa fourchette. Une borne haute qui ne dépasse pas la
// basse dit « pas de tirage » : la valeur est celle-là, et le générateur n'est
// même pas dérangé — deux patrons voisins gardent ainsi la même suite de
// tirages, que l'un d'eux tire ou non.
// ⚠ AVEC SON NOMBRE DE VALEURS quand le patron le porte : voir
// `EffectNodeRange::values`.
[[nodiscard]] float DrawnExtent(const EffectNodePattern& pattern, EffectDrawnRange which, float low,
                                float high, std::uint32_t& generator) {
  return DrawEffectNodeRange(EffectNodePatternRange(pattern, which, low, high), generator);
}

}  // namespace

EffectNode EffectNodeFromPattern(const EffectNodePattern& pattern, std::uint32_t& generator) {
  EffectNode node;
  node.render_word = pattern.render_word;
  // ⚠ LA VIE SE TIRE QUAND ELLE A UNE BORNE HAUTE, et le tirage se fait ici,
  // au semis : le client l'écrit une fois à la naissance du nœud. La tirer à
  // chaque tic ferait clignoter la fin.
  // ⚠ LA VIE VIENT PARFOIS DE L'EFFET, ET NON DU PATRON. Le bâtisseur recopie
  // alors la durée de l'instance dans son nœud, et cette durée n'est pas une
  // constante du client — voir `EffectNodeLifeFor` et l'ADR 0042.
  //
  // 🔴 MAIS ELLE N'EN VIENT PAS TOUJOURS TELLE QUELLE : la grille de l'effet 142
  // écrit
  // `vie = durée de l'instance − 10 + tirage(30)`, si bien que ses vingt-cinq
  // pointes retombent l'une après l'autre. La durée de l'effet en est la BASE,
  // pas le résultat ; écraser la fenêtre les éteindrait toutes ensemble, ce qui
  // est le contraire de ce que le client montre.
  //
  // ⚠ « CALCULÉ » DIT SEULEMENT « PAS UNE CONSTANTE ». La base vient de l'effet,
  // la fenêtre vient du patron : on garde donc les deux.
  // ⚠ ET L'ÉCART QUE LE BÂTISSEUR AJOUTE, s'il y en a un : voir
  // `EffectNodePattern::life_offset_from_effect`.
  const auto base =
      static_cast<float>(pattern.life_from_effect ? EffectNodeLifeFor(pattern.effect_id) +
                                                        pattern.life_offset_from_effect
                                                  : pattern.life_ticks);
  const auto window = static_cast<float>(pattern.life_ticks_high - pattern.life_ticks);
  node.life_ticks = static_cast<std::int32_t>(DrawEffectNodeInteger(
      static_cast<std::int32_t>(base), window > 0.0F ? static_cast<std::int32_t>(base + window) : 0,
      generator));
  // LA SORTIE, et « pas de sortie » est MOINS UN, comme au constructeur du
  // client. Zéro est un tic : la descente part de la naissance et dure toute la
  // vie, puisque le client compare l'âge à ce tic. Sans sortie, on la repousse à
  // la fin de la vie : le nœud tient sa pose jusqu'au bout.
  //
  // ⚠ ET ELLE SUIT LA VIE TIRÉE, elle ne reste pas sur la borne basse : un nœud
  // qui vit plus longtemps fondrait sinon bien avant sa fin. Ce que le patron
  // dit est l'ÉCART entre les deux, et c'est lui qui se reporte.
  node.fade_out_tick =
      pattern.fade_out_tick >= 0
          ? std::max(0, node.life_ticks - (pattern.life_ticks - pattern.fade_out_tick))
          : node.life_ticks;
  node.alpha_max = pattern.alpha_max;
  // L'ALPHA EST CELUI QUE LE PATRON POSE, et il monte s'il a un pas. Le client
  // écrit les deux champs séparément, et `StepAlpha` borne déjà la montée au
  // plafond : la traduction n'a rien à arbitrer.
  node.alpha = pattern.alpha_start;
  node.alpha_step_per_tick = pattern.alpha_step;
  // LE DEMI-CÔTÉ D'UNE CROIX DE POINTES SE TIRE, comme ses deux extensions : le
  // client tire entre trois et trois virgule quarante-neuf. N'en prendre que la
  // borne basse donnerait des pointes toutes de la même taille là où il en
  // montre une grille de tailles mêlées.
  //
  // Les autres formes n'ont pas de borne haute, et `DrawnExtent` ne dérange
  // alors pas le générateur : leur suite de tirages est inchangée.
  // ⚠ APRÈS LES TIRAGES QUE LE BÂTISSEUR JETTE À CETTE PLACE : voir
  // `EffectNodeSupplement::draws_discarded_before_scale`.
  for (std::uint8_t jete = 0; jete < pattern.supplement.draws_discarded_before_scale; ++jete) {
    static_cast<void>(NextEffectNodeDraw(generator));
  }
  node.sprite_scale = DrawnExtent(pattern, EffectDrawnRange::kSpriteScale, pattern.sprite_scale,
                                  pattern.sprite_scale_high, generator);
  // L'INDEX D'IMAGE CHOISIT DANS LE TABLEAU, et l'horloge le fait avancer : un
  // nœud à une seule texture ne bouge donc pas, et un nœud à onze bat.
  node.texture_count = pattern.texture_count;
  // ⚠ ET L'ACTION EST UN AUTRE CHOIX QUE L'IMAGE. L'index d'image parcourt le
  // tableau de textures d'une figure du monde ; celui-ci choisit QUELLE
  // animation du `.act` un sprite joue. Deux effets peuvent partager leur
  // couple et ne différer que par lui — 363 et 364 sont dans ce cas.
  node.action = pattern.action;
  for (std::size_t canal = 0; canal < node.color.size(); ++canal) {
    node.color[canal] = static_cast<float>(pattern.color[canal]);
    node.color_step_per_tick[canal] = pattern.color_step[canal];
  }

  node.radius_at_anchor = {pattern.ring.radius_at_anchor, pattern.ring.radius_at_anchor_growth,
                           pattern.ring.radius_at_anchor_acceleration};
  node.radius_at_height = {pattern.ring.radius_at_height, pattern.ring.radius_at_height_growth,
                           pattern.ring.radius_at_height_acceleration};
  node.ring_radius = {pattern.ring.ring_radius, pattern.ring.ring_radius_growth,
                      pattern.ring.ring_radius_acceleration};
  node.half_height = {DrawnExtent(pattern, EffectDrawnRange::kHalfHeight, pattern.ring.height,
                                  pattern.ring.height_high, generator),
                      pattern.ring.height_growth, pattern.ring.height_acceleration};
  node.half_height_ceiling = pattern.ring.height_ceiling;
  // La demi-largeur ne sert qu'aux plans croisés et au quad orienté ; ailleurs
  // elle reste nulle et personne ne la lit.
  node.half_width = {DrawnExtent(pattern, EffectDrawnRange::kHalfWidth, pattern.ring.half_width,
                                 pattern.ring.half_width_high, generator),
                     pattern.ring.half_width_growth, pattern.ring.half_width_acceleration};
  node.step_length = pattern.ring.step_length;
  node.step_length_change_per_tick = pattern.ring.step_length_change;
  node.second_keyframe_tick = pattern.ring.second_keyframe_tick;
  if (pattern.ring.frame_hold_ticks > 0) {
    node.frame_hold_ticks = pattern.ring.frame_hold_ticks;
  }
  node.height_flip_period_ticks = pattern.ring.height_flip_period_ticks;
  node.second_step_length = pattern.ring.second_step_length;
  node.second_step_length_change = pattern.ring.second_step_length_change;
  node.second_half_width_growth = pattern.ring.second_half_width_growth;
  node.second_half_width_acceleration = pattern.ring.second_half_width_acceleration;
  node.second_half_height_growth = pattern.ring.second_height_growth;
  node.second_half_height_acceleration = pattern.ring.second_height_acceleration;
  node.motion_stop_tick = pattern.ring.motion_stop_tick;
  node.sinking_step_length = pattern.ring.sinking_step_length;
  node.sinking_step_length_change = pattern.ring.sinking_step_length_change;
  node.second_ring_radius_growth = pattern.supplement.second_ring_radius_growth;
  node.second_ring_radius_acceleration = pattern.supplement.second_ring_radius_acceleration;
  node.alpha_keyframes = pattern.supplement.alpha_keyframes;
  // ⚠ LE MOUVEMENT SE LIT DANS LA FIGURE, et c'est le client qui le veut : son
  // répartiteur tire du MÊME sous-type la fonction qui dessine et celle qui met
  // à jour. La croix de pointes est le sous-type 9, donc le mouvement de
  // translation en boucle ; en faire un champ à part offrirait de les
  // contredire, ce que le client ne permet pas.
  // 🔴 DEUX FIGURES AVANCENT LE LONG DE LEUR AXE, et par deux rangées
  // DIFFÉRENTES de la même matrice. La pointe de terre (sous-type 9) suit la
  // troisième ; l'anneau (sous-type 13) suit la DEUXIÈME, et d'un pas NÉGATIF :
  // l'opposé du pas, multiplié par cette rangée, s'accumule dans sa position.
  //
  // ⚠ SANS CELA L'ANNEAU RESTE COLLÉ À SON PORTEUR. Le client le fait naître
  // dans le sprite et s'en écarter d'environ une cellule sur ses dix tics ; posé
  // sans mouvement, il ne se distingue pas d'un anneau qui ne serait pas semé.
  //
  // ⚠ ET L'ORBITE N'EST PAS UNE SOLUTION DE REPLI ICI : elle n'est consommée que
  // par les nœuds d'ÉCRAN (`effect_scene.cpp`). Une figure du monde laissée sur
  // la branche d'orbite accumule un déplacement que personne ne lit.
  node.translates_along_axis = pattern.shape == EffectNodeShape::kCrossSpikes ||
                               pattern.shape == EffectNodeShape::kRingBand ||
                               pattern.ring.advances_along_depth_axis;
  // L'AVANCE DÉJÀ FAITE À LA NAISSANCE. Voir `EffectRingPattern::axial_translation_start`.
  node.axial_translation = pattern.ring.axial_translation_start;
  // ⚠ LE CYCLE, LUI, NE SE DÉDUIT PAS DE LA FIGURE. Deux sous-types mènent à la
  // croix de pointes — le 9 et le 117 —, et seul le second relance son pas. Le
  // patron le porte parce que la forme ne suffit pas à le dire.
  node.step_cycles = pattern.ring.step_cycles;
  node.ring_step_degrees = pattern.ring.step_degrees;
  node.texture_v_min = pattern.ring.texture_v_min;
  node.texture_v_max = pattern.ring.texture_v_max;
  // ⚠ LE PENCHANT ET LE LACET SE TIRENT UNE FOIS, au semis : le client les
  // écrit dans la matrice du nœud à sa naissance et ne les retouche plus. Les
  // retirer à chaque tic ferait trembler la figure.
  // ⚠ DANS L'ORDRE DU BÂTISSEUR : voir `EffectNodeSupplement::yaw_drawn_before_lean`.
  const auto draw_yaw = [&pattern, &generator] {
    return pattern.ring.draws_its_yaw
               ? static_cast<float>(NextEffectNodeDraw(generator) % kEffectNodeYawTurnDegrees)
               : 0.0F;
  };
  if (pattern.supplement.yaw_drawn_before_lean) {
    node.yaw_degrees = draw_yaw();
  }
  node.lean_degrees = DrawnExtent(pattern, EffectDrawnRange::kLean, pattern.ring.lean_degrees,
                                  pattern.ring.lean_degrees_high, generator);
  if (!pattern.supplement.yaw_drawn_before_lean) {
    node.yaw_degrees = draw_yaw();
  }
  // ⚠ LES DRAPEAUX DU CLIENT, TELS QUELS : chaque dessinateur lit les siens, et
  // le même bit ne veut pas dire la même chose de l'un à l'autre. Les traduire
  // ici demanderait de savoir quel dessinateur les lira, ce que le patron dit
  // mais que ce champ, lui, ne dit pas.
  node.flags = pattern.ring.flags;
  // ⚠ SAUF UN, que seul le mouvement des sous-types quinze et seize lit : sur
  // une autre figure, le même bit dit autre chose. Voir `EffectNode::cycles_color`.
  node.cycles_color = (pattern.ring.flags & kEffectNodeCyclesColor) != 0U &&
                      (pattern.shape == EffectNodeShape::kOrientedQuad ||
                       pattern.shape == EffectNodeShape::kCrossedPlanes);
  node.color_hold_ticks = pattern.ring.color_hold_ticks;
  // LA TEINTE TIRÉE, en dernier : un patron qui ne la tire pas garde la même
  // suite de tirages. Voir `EffectRingPattern::color_high`.
  DrawEffectRingTint(pattern.ring, node.color, generator);
  // 🔴 UN NŒUD QUI PLONGE SE PEINT AUX COULEURS D'UN NŒUD NEUF : sa rampe de
  // couleur ne tourne jamais (voir `EffectNodeDives`), et le mot de couleur que
  // le dessinateur lit reste blanc et opaque. L'alpha et la teinte que le
  // bâtisseur écrit n'atteignent donc pas l'écran.
  if (EffectNodeDives(node)) {
    node.alpha = kEffectNodeChannelMax;
    node.alpha_max = kEffectNodeChannelMax;
    node.color.fill(kEffectNodeChannelMax);
  }
  return node;
}

EffectNode EffectNodeFromPattern(const EffectNodePattern& pattern) {
  std::uint32_t generator = pattern.effect_id;
  return EffectNodeFromPattern(pattern, generator);
}

// ⚠ LA RÈGLE N'EXISTE QU'UNE FOIS, et ces deux-ci la déléguent. Trois familles
// de patrons portent désormais un horaire ; trois copies de la même règle
// divergeraient à la première correction, et le décalage ne se verrait que sur
// l'une d'elles.
bool EffectNodePatternSowsAt(const EffectNodePattern& pattern, std::int32_t tick) {
  return EffectNodeScheduleSowsAt({pattern.first_tick, pattern.last_tick, pattern.period_ticks,
                                   pattern.period_phase_ticks, pattern.supplement.age_wrap_ticks},
                                  tick);
}

std::int32_t EffectNodePatternTicksLeft(const EffectNodePattern& pattern, std::int32_t tick) {
  if (pattern.life_until_tick > 0) {
    return std::max(1, pattern.life_until_tick - tick);
  }
  if (!EffectInstanceLifeIsKnown(pattern.effect_id)) {
    return std::numeric_limits<std::int32_t>::max();
  }
  return std::max(1, EffectInstanceLifeFor(pattern.effect_id) - tick);
}

std::int32_t EffectNodePatternLastSowTick(const EffectNodePattern& pattern) {
  return EffectNodeScheduleLastSowTick({pattern.first_tick, pattern.last_tick, pattern.period_ticks,
                                        pattern.period_phase_ticks,
                                        pattern.supplement.age_wrap_ticks});
}

std::int32_t EffectNodePatternSowingRank(const EffectNodePattern& pattern, std::int32_t tick) {
  if (!EffectNodePatternSowsAt(pattern, tick)) {
    return 0;
  }
  if (pattern.period_ticks <= 0) {
    return 1;
  }
  // LE RANG SE COMPTE DANS LE CYCLE, quand l'âge repart : le bâtisseur ne voit
  // que son âge.
  const std::int32_t age =
      pattern.supplement.age_wrap_ticks > 0 ? tick % pattern.supplement.age_wrap_ticks : tick;
  return (age - pattern.first_tick) / pattern.period_ticks + 1;
}

float EffectNodePatternScatterAt(const EffectNodePattern& pattern, std::int32_t tick) {
  // ⚠ UNE DIVISION ENTIÈRE, avant la conversion : voir le patron.
  if (pattern.scatter_sowing_tick_divisor <= 0) {
    return pattern.scatter;
  }
  return pattern.scatter + static_cast<float>(tick / pattern.scatter_sowing_tick_divisor);
}

float EffectNodePatternSpriteScaleAt(const EffectNodePattern& pattern, std::int32_t tick) {
  return pattern.sprite_scale + pattern.sprite_scale_per_sowing_tick * static_cast<float>(tick);
}

bool EffectNodePatternSowsAtParam(const EffectNodePattern& pattern, std::int32_t tick,
                                  std::int32_t param) {
  const std::int32_t rang = EffectNodePatternSowingRank(pattern, tick);
  if (rang <= 0) {
    return false;
  }
  if (!pattern.sowings_follow_param) {
    return true;
  }
  // Un cas par valeur du paramètre jusqu'au plafond, et le défaut ne sème rien — ni à zéro, ni
  // au-delà des cas écrits.
  if (param <= 0 || param > static_cast<std::int32_t>(pattern.param_cap)) {
    return false;
  }
  return rang <= param;
}

bool EffectParamCounterIsShared(std::uint32_t effect_id) {
  constexpr std::uint32_t kBlitzBeat = 115;
  return effect_id == kBlitzBeat;
}

const EffectRollByParam* EffectRollByParamFor(std::uint32_t effect_id, std::int32_t param) {
  for (const EffectRollByParam& roll : EffectRollByParamTable()) {
    if (roll.effect_id == effect_id && static_cast<std::int32_t>(roll.param) == param) {
      return &roll;
    }
  }
  return nullptr;
}

bool EffectSoundPlaysAtSowing(std::uint32_t effect_id) {
  return std::ranges::any_of(
      EffectNodePatternsFor(effect_id),
      [](const EffectNodePattern& node_pattern) { return node_pattern.sound_at_sowing; });
}

}  // namespace ro::render
