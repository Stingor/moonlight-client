#include "ro/render/effect_shield_boomerang_seed.h"

#include <algorithm>
#include <cmath>
#include <string>

#include "ro/render/effect_life_table.h"
#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// CE QUE LES DEUX BÂTISSEURS POSENT EN DUR, et à l'identique : celui du
// bouclier boomerang et celui du trait de lierre bleu. Ce qui les distingue
// vient du patron.

// L'AVANCE PAR TIC. C'est elle
// que la phase de chute triple, chez celui qui éclate.
constexpr float kSpeed = 2.0F;

// ⚠ DE COMBIEN LE BOUCLIER PART AU-DESSUS DU LANCEUR, et le nom dit le SENS et
// non le signe. Le client RETRANCHE huit à l'ordonnée, dans un repère qui DESCEND : la soustraction
// élève. Recopier le signe le ferait partir des pieds.
constexpr float kLiftAboveThrower = 8.0F;

// Les cases que le mouvement lit, aux mêmes rangs que dans le client.
constexpr std::size_t kSlotOfSpeed = 1;
constexpr std::size_t kSlotOfRise = 2;
// LE GENRE : la troisième marque de la première piste. Voir le patron.
constexpr std::size_t kMarkOfKind = 2;

// ⚠ LES CHAMPS QUE LES BÂTISSEURS ÉCRIVENT ET QUI NE SONT PAS PORTÉS ICI, parce
// que PERSONNE NE LES LIT — ni le mouvement de traînée à tête chercheuse, ni le
// dessinateur du quad teinté tourné :
//   · l'échelle de sprite, à zéro ;
//   · la case sept de la tête, à zéro (le second bâtisseur seul). Le
//     mouvement lit les cases un, deux et huit ; le dessinateur les cases huit
//     et neuf. Zéro est d'ailleurs la valeur de la piste neuve.

}  // namespace

std::span<const EffectShieldBoomerangPattern> EffectShieldBoomerangPatternsFor(
    std::uint32_t effect_id) {
  const std::span<const EffectShieldBoomerangPattern> table = EffectShieldBoomerangPatternTable();
  const auto [first, last] =
      std::ranges::equal_range(table, effect_id, {}, &EffectShieldBoomerangPattern::effect_id);
  return {first, last};
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectShieldBoomerangTracks(
    const EffectShieldBoomerangPattern& pattern, glm::vec3 anchor, glm::vec3 target,
    std::int32_t effect_param) {
  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  for (EffectTrack& track : tracks) {
    // ⚠ LES QUATRE SONT ALLUMÉES, les trois dernières à alpha NUL : elles ne reçoivent que par la
    // cascade. Les éteindre les priverait de la traînée.
    track.active = true;
  }

  EffectTrack& head = tracks.front();
  head.alpha = pattern.head_alpha;
  head.ticks = pattern.start_ticks;
  head.mark[kMarkOfKind] = pattern.kind;
  // ⚠ LE PARAMÈTRE EST UN ENTIER converti AVANT la multiplication : trois coups donnent 3 × 0,4 + 2
  // = 3,2.
  head.radius_at_anchor =
      pattern.core_radius + static_cast<float>(effect_param) * pattern.radius_per_param;
  head.value[kSlotOfSpeed] = kSpeed;
  head.anchor = {anchor.x, anchor.y + kLiftAboveThrower, anchor.z};

  // LA CLÉ EST LA DISTANCE, et elle se mesure AU SOL : le client ne compte que
  // l'abscisse et la cote. C'est elle que le mouvement compare au chemin
  // parcouru pour décider quand le bouclier revient.
  const float offset_x = target.x - anchor.x;
  const float offset_z = target.z - anchor.z;
  const float distance = std::sqrt(offset_x * offset_x + offset_z * offset_z);
  head.lift_key = distance;

  // ⚠ ET LA MONTÉE SE CALCULE POUR ARRIVER SUR LA CIBLE. Le client la fixe à
  // « vitesse sur distance, fois l'écart d'altitude » : le bouclier met donc
  // exactement le temps du trajet à rattraper la hauteur.
  //
  // ⚠ SON SIGNE EST RETOURNÉ, parce que le mouvement l'est aussi. Le client
  // AJOUTE cette montée à l'aller dans un repère qui DESCEND, et
  // `AdvanceHomingTrailTracks` la RETRANCHE dans le nôtre, qui monte : la valeur
  // rangée ici doit donc être l'opposée de l'écart d'altitude, faute de quoi le
  // bouclier plongerait pour atteindre une cible plus haute.
  if (distance > 0.0F) {
    head.value[kSlotOfRise] = -(kSpeed / distance) * (target.y - anchor.y);
  }
  return tracks;
}

std::optional<PooledEffectNode> SowEffectShieldBoomerangNode(
    const EffectShieldBoomerangPattern& pattern, glm::vec3 anchor,
    const std::optional<glm::vec3>& target, std::int32_t effect_param) {
  // ⚠ PAS DE CIBLE, PAS DE BOUMERANG. Voir l'en-tête : sans victime il n'y a ni
  // distance à parcourir ni altitude à rattraper, et le semer donnerait un carré
  // immobile là où le client ne montre rien.
  if (!target.has_value()) {
    return std::nullopt;
  }

  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kHomingTrail;
  pooled.ground_position = anchor;
  // LES DEUX BOUTS : le mouvement VISE le second et REVIRE vers le premier.
  pooled.owner_position = anchor;
  pooled.target_position = target;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectShieldBoomerangTracks(pattern, anchor, *target, effect_param);
  pooled.node.render_word = kShieldBoomerangRenderWord;
  // 🔴 LA VIE EST CELLE DE L'INSTANCE, que le bâtisseur recopie et que la table
  // des durées porte (ADR 0042). Un défaut de cent tics éteindrait le boomerang
  // lancé loin avant son retour (l'instance vit quatre cent quatre-vingt-dix-neuf
  // tics).
  pooled.node.life_ticks = EffectNodeLifeFor(pattern.effect_id);
  return pooled;
}

}  // namespace ro::render
