#include "ro/render/effect_screen_node.h"

#include <algorithm>
#include <cmath>

#include "ro/render/effect_node.h"
#include "ro/render/map_effect_math.h"

namespace ro::render {
namespace {

// LE PLAFOND D'UN CANAL DE COULEUR, et le plancher.
constexpr std::int32_t kChannelMax = 255;
constexpr std::int32_t kChannelMin = 0;
// Les rangs des trois canaux, dans l'ordre où le client les range.
constexpr std::size_t kRed = 0;
constexpr std::size_t kGreen = 1;
constexpr std::size_t kBlue = 2;

// DE COMBIEN LE CAP DÉRIVE À CHAQUE ZIGZAG : un tirage sur seize, moins huit.
constexpr std::int32_t kZigzagSpread = 16;
constexpr float kZigzagBias = 8.0F;
// ET DANS COMBIEN DE TICS LE PROCHAIN : un tirage sur dix, plus dix.
constexpr std::int32_t kZigzagPeriodSpread = 10;
constexpr std::int32_t kZigzagPeriodFloor = 10;

// Ce qu'une valeur « d'après » remplace, quand elle n'est pas la sentinelle.
void RemplacerSiPose(float& current, float after) {
  if (after != kEffectScreenUnchanged) {
    current = after;
  }
}

// LE VA-ET-VIENT DE L'ALPHA : le pas se retourne quand la borne est atteinte.
void RetournerLAlpha(EffectScreenNode& node) {
  if (!HasEffectScreenFlag(node.flags, EffectScreenFlag::kSwingsAlpha)) {
    return;
  }
  const bool within_bounds =
      node.alpha_step >= 0.0F ? node.alpha < node.alpha_max : node.alpha_min < node.alpha;
  if (!within_bounds) {
    node.alpha_step = -node.alpha_step;
  }
}

// CE QUE LE MOUVEMENT DU SOUS-TYPE 1 FAIT DE L'ALPHA,
// et les TROIS formes de cette voie l'appellent — la couronne et l'éclat à la
// fin de leur mouvement, le quad après le sien.
void AdvanceNodeAlpha(EffectScreenNode& node) {
  RetournerLAlpha(node);

  // ⚠ UN NŒUD NE SURVIT PAS À CELUI QUI LE PORTE. Quand il lui reste PLUS de
  // temps qu'à son porteur, son alpha s'éteint sur la durée de celui-ci — et le
  // va-et-vient est retiré, sans quoi il remonterait aussitôt. La comparaison
  // est SIGNÉE.
  if (!node.owner_fade_engaged && node.owner_remaining_ticks > 0 &&
      node.life_ticks - node.age_ticks > node.owner_remaining_ticks + 1) {
    node.owner_fade_engaged = true;
    node.flags &= ~static_cast<std::uint32_t>(EffectScreenFlag::kSwingsAlpha);
    node.alpha_step = -node.alpha / static_cast<float>(node.owner_remaining_ticks);
  }

  // LE FONDU DE SORTIE : au tic dit, le pas devient ce qu'il faut pour atteindre
  // zéro avant la fin de la vie, multiplié par le facteur.
  if (node.age_ticks == node.fade_out_tick) {
    node.flags &= ~static_cast<std::uint32_t>(EffectScreenFlag::kSwingsAlpha);
    const auto remainder = static_cast<float>(node.life_ticks - node.age_ticks);
    node.alpha_step =
        remainder != 0.0F ? -(node.alpha / remainder) * node.fade_factor : -node.alpha;
  }
  node.alpha = std::min(node.alpha_max, std::max(node.alpha_min, node.alpha + node.alpha_step));
}

// LE MOUVEMENT DE LA COURONNE, cas 4 du répartiteur. Trois
// lignes : le rayon accélère, la largeur de bande se recalcule, l'alpha tourne.
void AdvanceRing(EffectScreenNode& node) {
  node.outer_radius_step += node.outer_radius_acceleration;
  node.outer_radius += node.outer_radius_step;
  // ⚠ LA LARGEUR SE RECALCULE ICI ET NULLE PART AILLEURS. Le drapeau un la
  // pousse jusqu'au CENTRE — la couronne est alors un disque plein —, sinon
  // elle est bridée par le rayon lui-même : une bande ne peut pas dépasser le
  // centre. La semer une fois pour toutes la figerait au premier tic.
  node.band_width = HasEffectScreenFlag(node.flags, EffectScreenFlag::kHangsBelowAnchor)
                        ? node.outer_radius
                        : std::min(node.band_width_target, node.outer_radius);
  AdvanceNodeAlpha(node);
}

// LE MOUVEMENT DE L'ÉCLAT, cas 3 du répartiteur.
void AdvanceFlash(EffectScreenNode& node) {
  // ⚠ LE DRAPEAU UN FIGE LA CROISSANCE À RAYON NUL, et il ne dit pas ici ce
  // qu'il dit ailleurs : pour le quad il fait pendre la figure, pour la
  // couronne il la remplit.
  if (HasEffectScreenFlag(node.flags, EffectScreenFlag::kHangsBelowAnchor) &&
      node.half_height <= 0.0F) {
    node.half_height_step = 0.0F;
    node.half_height_acceleration = 0.0F;
  }

  // ⚠ LE SECOND PAS NE REMPLACE QUE DEUX CHOSES — la rotation et le pas de
  // rayon —, et sans AUCUNE sentinelle : les valeurs s'écrivent telles
  // quelles. Employer la règle du quad laisserait la rotation décélérer là où
  // elle doit se figer.
  float radius_step = node.half_height_step;
  if (node.age_ticks == node.second_step_tick) {
    node.turn_speed = node.turn_speed_after;
    node.turn_acceleration = node.turn_acceleration_after;
    radius_step = node.half_height_step_after;
  }

  // ⚠ ET LE RAYON REBONDIT. Passé sous zéro il repart de zéro, son pas
  // retourné : l'éclat qui rentrait ressort. Aucun bâtisseur n'y arrive
  // aujourd'hui — le seul au pas négatif meurt avant —, mais le sauter ferait
  // plonger son rayon dans le négatif si sa vie changeait.
  float radius = node.half_height;
  if (radius < 0.0F) {
    radius_step = -radius_step;
    radius = 0.0F;
  }

  node.turn_speed += node.turn_acceleration;
  node.half_height_step = radius_step + node.half_height_acceleration;
  node.turn_degrees += node.turn_speed;
  node.half_height = node.half_height_step + radius;
  // ⚠ LA DEMI-OUVERTURE CROÎT DU PAS DE DEMI-LARGEUR, un champ que rien
  // d'autre ne lit dans cette figure. Voir `effect_screen_node.h`.
  node.angle_degrees += node.half_width_step;

  AdvanceNodeAlpha(node);
}

// LE MOUVEMENT D'UN TIC, selon la forme ; faux quand l'image tue le quad.
bool AdvanceByShape(EffectScreenNode& node, std::uint32_t& generator) {
  if (node.shape == EffectScreenShape::kRing) {
    AdvanceRing(node);
    return true;
  }
  if (node.shape == EffectScreenShape::kWedge) {
    AdvanceFlash(node);
    return true;
  }

  // ⚠ LE CLIENT COMPARE PAR ÉGALITÉ, pas par dépassement : un tic manqué ne
  // rattrape pas. Écrire « au-delà de » ferait rebasculer à chaque tic ensuite.
  if (node.age_ticks == node.second_step_tick) {
    RemplacerSiPose(node.half_width_step, node.half_width_step_after);
    RemplacerSiPose(node.half_height_step, node.half_height_step_after);
    RemplacerSiPose(node.step, node.step_after);
    RemplacerSiPose(node.half_width_acceleration, node.half_width_acceleration_after);
    RemplacerSiPose(node.half_height_acceleration, node.half_height_acceleration_after);
    RemplacerSiPose(node.step_change, node.step_change_after);
  }

  // LES ACCUMULATIONS : chaque pas gagne sa variation, puis la taille gagne son
  // pas. L'ordre compte — le client emploie le pas du tic COURANT.
  node.turn_speed += node.turn_acceleration;
  node.half_width_step += node.half_width_acceleration;
  node.half_height_step += node.half_height_acceleration;
  node.step += node.step_change;

  if (HasEffectScreenFlag(node.flags, EffectScreenFlag::kStopsGrowing) &&
      node.age_ticks >= node.growth_stop_tick) {
    // ⚠ LES QUATRE TOMBENT ENSEMBLE, pas seulement les deux pas : la croissance
    // s'arrête net au lieu de ralentir.
    node.half_width_step = 0.0F;
    node.half_width_acceleration = 0.0F;
    node.half_height_step = 0.0F;
    node.half_height_acceleration = 0.0F;
  }

  node.turn_degrees += node.turn_speed;
  node.half_width += node.half_width_step;
  node.half_height += node.half_height_step;

  // LE DÉPLACEMENT. ⚠ L'ordonnée d'écran se SOUSTRAIT : ce qui doit s'élever a
  // une ordonnée qui diminue.
  //
  // 🔴 L'ABSCISSE PREND LE SINUS DU CAP ET L'ORDONNÉE SON COSINUS : un cap nul
  // fait MONTER le nœud, un cap de cent quatre-vingts le fait descendre. Le
  // client les lit dans deux tables dont les noms sont échangés — celle qu'on
  // croirait du cosinus rend le sinus —, et l'angle, tronqué, s'y lit un degré
  // de travers quand il est négatif. Voir `NativeTableRadians`.
  const float cap = NativeTableRadians(static_cast<std::int32_t>(std::trunc(node.heading_degrees)));
  node.travel.x += std::sin(cap) * node.step;
  node.travel.y -= std::cos(cap) * node.step;

  // LES TROIS CANAUX. ⚠ LE ROUGE REBOUCLE, les deux autres saturent.
  node.color[kRed] += node.color_step[kRed];
  if (node.color[kRed] > kChannelMax) {
    node.color[kRed] = kChannelMin;
  } else if (node.color[kRed] < kChannelMin) {
    node.color[kRed] = kChannelMax;
  }
  for (const std::size_t canal : {kGreen, kBlue}) {
    node.color[canal] =
        std::clamp(node.color[canal] + node.color_step[canal], kChannelMin, kChannelMax);
  }

  if (HasEffectScreenFlag(node.flags, EffectScreenFlag::kZigzags) && node.growth_stop_tick > 0 &&
      node.age_ticks % node.growth_stop_tick == 0) {
    const auto draw = static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                                static_cast<std::uint32_t>(kZigzagSpread));
    node.heading_degrees += static_cast<float>(draw) - kZigzagBias;
    node.growth_stop_tick =
        static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                  static_cast<std::uint32_t>(kZigzagPeriodSpread)) +
        kZigzagPeriodFloor;
  }

  // ── LE MOUVEMENT DU SOUS-TYPE 1, que celui du zéro appelle ici ──────────────
  //
  // ⚠ LE VA-ET-VIENT S'EXÉCUTE DEUX FOIS PAR TIC POUR CETTE FORME — une fois
  // dans chaque fonction du client —, et le second passage ne défait pas le
  // premier : le pas ayant changé de signe, le test regarde alors l'AUTRE
  // borne. La couronne et l'éclat, qui n'appellent que la seconde fonction, ne
  // le font qu'une fois.
  RetournerLAlpha(node);
  AdvanceNodeAlpha(node);

  // L'AVANCE DE L'IMAGE, et c'est elle qui décide de la mort du nœud.
  if (node.ticks_per_frame > 0 && node.age_ticks % node.ticks_per_frame == 0) {
    ++node.frame;
    if (node.frame_loops) {
      if (node.texture_count > 0) {
        node.frame %= node.texture_count;
      }
    } else if (node.frame == node.texture_count) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool AdvanceEffectScreenNode(EffectScreenNode& node, std::uint32_t& generator) {
  // 🔴 L'ÂGE SE COMPARE AVANT DE CROÎTRE. Toutes les bascules de ce tic — second
  // pas, arrêt de croissance, fondu de sortie, image — lisent l'âge du tic
  // COURANT, zéro à la première mise à jour ; il ne gagne son tic qu'à la fin.
  // Un fondu posé au tic zéro s'engage donc dès la première.
  const bool alive = AdvanceByShape(node, generator);
  ++node.age_ticks;
  if (!alive) {
    return false;
  }
  // ⚠ PUIS TOUT NŒUD MEURT PAR SA VIE, quelle que soit sa forme : il vit
  // VIE + 1 mises à jour, la mort tombe quand l'âge, une fois crû, DÉPASSE la
  // vie. C'est la règle commune à tous les nœuds du client, après leur propre
  // mouvement ; le quad meurt en plus par son image quand elle ne boucle pas,
  // et c'est ce qu'a dit `AdvanceByShape`.
  return node.age_ticks <= node.life_ticks;
}

EffectScreenQuadRequest EffectScreenQuadRequestOf(const EffectScreenNode& node,
                                                  glm::vec2 owner_screen_position) {
  EffectScreenQuadRequest request;
  const glm::vec2 origin = HasEffectScreenFlag(node.flags, EffectScreenFlag::kScreenAbsolute)
                               ? node.screen_offset
                               : owner_screen_position + node.screen_offset;
  request.screen_position = origin + node.travel;
  request.half_width = node.half_width;
  request.half_height = node.half_height;
  // Le client indexe deux tables de sinus par un entier : l'angle se TRONQUE,
  // il ne s'arrondit pas.
  request.turn_degrees = static_cast<std::int32_t>(std::trunc(node.turn_degrees));
  request.tint = {static_cast<std::uint8_t>(node.color[kRed]),
                  static_cast<std::uint8_t>(node.color[kGreen]),
                  static_cast<std::uint8_t>(node.color[kBlue]),
                  static_cast<std::uint8_t>(
                      std::clamp(static_cast<std::int32_t>(node.alpha), kChannelMin, kChannelMax))};
  // ⚠ UN quand le drapeau est absent, et c'est ce que le client passe.
  request.blend_mode = node.uses_blend_mode ? node.blend_mode : 1;
  request.texture = static_cast<std::size_t>(std::max(node.frame, 0));
  request.hangs_below_anchor = HasEffectScreenFlag(node.flags, EffectScreenFlag::kHangsBelowAnchor);
  return request;
}

}  // namespace ro::render
