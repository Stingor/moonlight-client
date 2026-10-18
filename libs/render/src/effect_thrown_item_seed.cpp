#include "ro/render/effect_thrown_item_seed.h"

#include <cmath>
#include <string>

#include "ro/core/sorted_table.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_track.h"

namespace ro::render {
namespace {

// LES STYLES QUE LE BÂTISSEUR NOMME, un à un et par égalité : ce ne sont pas des
// bornes, et un style entre deux ne se déduit pas de ses voisins.
constexpr std::uint8_t kStyleOfPlainBottle = 0;
constexpr std::uint8_t kStyleOfPotion = 1;
constexpr std::uint8_t kStyleOfStone = 2;
constexpr std::uint8_t kStyleOfBlade = 3;
constexpr std::uint8_t kStyleBornAtFifthTick = 10;
constexpr std::uint8_t kStyleBornAtTenthTick = 11;

// LES DEUX TICS DE NAISSANCE QUE DEUX STYLES IMPOSENT. C'est ainsi qu'un seul
// identifiant jette deux flacons l'un après l'autre.
constexpr std::int32_t kFifthTick = 5;
constexpr std::int32_t kTenthTick = 10;

// LES TROIS DEMI-TAILLES, une par famille d'objet.
constexpr float kHalfSizeOfStone = 2.0F;
constexpr float kHalfSizeOfBlade = 4.0F;
constexpr float kHalfSizeOfOthers = 3.0F;

// LES TROIS PAS D'AVANCE. Les deux premiers sont écrits en dur ; le troisième se
// calcule, et c'est lui qui fait qu'un objet lancé loin va plus vite.
constexpr float kStepOfStone = 2.0F;
constexpr float kStepOfBlade = 3.0F;
// ⚠ VINGT-CINQ PAS POUR LE VOYAGE, quelle que soit la distance : c'est le client
// qui fixe ce compte, et c'est lui qui rend la durée du vol à peu près constante.
constexpr float kStepsAcrossAnyThrow = 25.0F;

// LE GENRE, que le mouvement relit pour choisir sa rotation et la hauteur de son
// arc. Les quatre valeurs sont celles du client, et elles ne se suivent pas.
constexpr std::uint8_t kKindOfPlainBottle = 8;
constexpr std::uint8_t kKindOfPotion = 9;
constexpr std::uint8_t kKindOfStone = 0;
constexpr std::uint8_t kKindOfBlade = 15;
// LA MARQUE QUE LE STYLE UN PREND DE SON INSTANCE, et que les styles dix et onze
// posent en dur.
constexpr std::uint8_t kExtraOfLateStyles = 4;

// L'ANGLE DE DÉPART DE LA LAME, le seul style qui ne parte pas de zéro.
constexpr std::int16_t kBladeStartDegrees = 330;

// DE COMBIEN LA TÊTE PART AU-DESSUS DE SON LANCEUR. Le client retranche cette
// hauteur dans son repère, qui descend ; elle s'ajoute dans le nôtre, qui monte.
constexpr float kHeightAboveThrower = 8.0F;

constexpr std::int16_t kFullAlpha = 255;

// ⚠ LE COMPTE DE PAS S'ARRÊTE À CENT, ET N'EST ALORS PAS ÉCRIT. Le client sort
// de sa boucle sans ranger le compte, si bien que la marque garde le zéro du
// nœud neuf — et le mouvement juge alors le vol fini au premier tic. C'est sa
// mesure, pas un oubli : un jet plus long que cent pas ne se voit pas.
constexpr std::int32_t kStepCountCeiling = 100;

[[nodiscard]] float HorizontalDistance(glm::vec3 source, glm::vec3 target) {
  const float dx = target.x - source.x;
  const float dz = target.z - source.z;
  return std::sqrt(dx * dx + dz * dz);
}

}  // namespace

std::span<const EffectThrownItemPattern> EffectThrownItemPatternsFor(std::uint32_t effect_id) {
  return core::RowsWithKey(EffectThrownItemPatternTable(), effect_id,
                           &EffectThrownItemPattern::effect_id);
}

std::int32_t EffectThrownItemBirthTick(std::uint8_t builder_argument) {
  if (builder_argument == kStyleBornAtFifthTick) {
    return kFifthTick;
  }
  if (builder_argument == kStyleBornAtTenthTick) {
    return kTenthTick;
  }
  return 0;
}

std::array<EffectTrack, kEffectTracksPerNode> SeedEffectThrownItemTracks(
    const EffectThrownItemPattern& pattern, glm::vec3 source, glm::vec3 target,
    std::int32_t instance_param) {
  const std::uint8_t style = pattern.builder_argument;
  const float distance = HorizontalDistance(source, target);

  std::array<EffectTrack, kEffectTracksPerNode> tracks;
  EffectTrack& head = tracks.front();
  head.active = true;
  head.ticks = 0;
  head.alpha = kFullAlpha;
  // ⚠ LA CLÉ PORTE ICI LA DISTANCE DU JET, et c'est l'ADR 0017 à la lettre : le
  // même champ qu'un anneau lit comme une hauteur de décalage.
  head.lift_key = distance;

  if (style == kStyleOfStone) {
    head.radius_at_anchor = kHalfSizeOfStone;
  } else if (style == kStyleOfBlade) {
    head.radius_at_anchor = kHalfSizeOfBlade;
  } else {
    head.radius_at_anchor = kHalfSizeOfOthers;
  }

  // ⚠ L'ANCRE PART AU SOL, PAS À LA HAUTEUR DU LANCEUR. Le client met sa hauteur
  // à zéro et range la vraie hauteur de départ dans la queue ; c'est de là que le
  // mouvement élève son arc à chaque tic.
  head.anchor = {source.x, 0.0F, source.z};
  head.tail[kTailSlotOfThrownBaseHeight] = source.y + kHeightAboveThrower;
  head.azimuth_degrees = 0;

  if (style == kStyleOfStone) {
    head.value[kSlotOfThrownSpeed] = kStepOfStone;
  } else if (style == kStyleOfBlade) {
    head.value[kSlotOfThrownSpeed] = kStepOfBlade;
  } else {
    head.value[kSlotOfThrownSpeed] = distance / kStepsAcrossAnyThrow;
  }
  // LA PENTE : ce que la hauteur gagne à chaque pas, pour que l'objet arrive à
  // la hauteur de sa cible. L'arc du mouvement s'y ajoute par-dessus.
  head.value[kSlotOfThrownRise] =
      (head.value[kSlotOfThrownSpeed] / distance) * (target.y - source.y);

  if (style == kStyleOfBlade) {
    head.degrees = kBladeStartDegrees;
    head.mark[kMarkOfThrownKindSeed] = kKindOfBlade;
  } else {
    head.degrees = 0;
    if (style == kStyleOfPlainBottle) {
      head.mark[kMarkOfThrownKindSeed] = kKindOfPlainBottle;
    } else if (style == kStyleOfPotion) {
      head.mark[kMarkOfThrownKindSeed] = kKindOfPotion;
      // 🔴 LA SOUS-MARQUE DE CE STYLE EST LE PARAMÈTRE DE L'INSTANCE, tronqué à
      // l'octet : un le teinte de rouge, et zéro ne se dessine pas du tout.
      head.mark[kMarkOfThrownExtra] = static_cast<std::uint8_t>(instance_param);
    } else if (style == kStyleOfStone) {
      head.mark[kMarkOfThrownKindSeed] = kKindOfStone;
    } else if (style >= kStyleBornAtFifthTick) {
      head.mark[kMarkOfThrownKindSeed] = kKindOfPotion;
      head.mark[kMarkOfThrownExtra] = kExtraOfLateStyles;
    }
    // ⚠ LES STYLES DE QUATRE À NEUF NE POSENT AUCUN GENRE, et le client ne leur
    // en pose pas non plus : la marque garde le zéro du nœud neuf. Aucun des onze
    // identifiants relevés n'en emploie, mais leur donner le genre zéro par
    // défaut serait leur donner celui de la pierre.
  }

  // LE NOMBRE DE PAS DU VOL : le premier compte dont la longueur dépasse la
  // distance. C'est lui qui donne l'échelle du temps au mouvement.
  std::int32_t step = 0;
  bool count = true;
  while (static_cast<float>(step) * head.value[kSlotOfThrownSpeed] <= distance) {
    if (++step >= kStepCountCeiling) {
      count = false;
      break;
    }
  }
  if (count) {
    head.mark[kMarkOfThrownSteps] = static_cast<std::uint8_t>(step);
  }
  // ⚠ LES TROIS CRANS SONT ALLUMÉS ET VIDES. Le bâtisseur les allume avec un
  // alpha nul : ils ne se voient pas tant que la cascade ne leur a rien donné,
  // mais les laisser éteints les empêcherait de recevoir.
  for (std::size_t rang = 1; rang < tracks.size(); ++rang) {
    tracks[rang].active = true;
    tracks[rang].alpha = 0;
  }
  return tracks;
}

PooledEffectNode SowEffectThrownItemNode(const EffectThrownItemPattern& pattern, glm::vec3 source,
                                         glm::vec3 target, std::int32_t effect_life_ticks,
                                         std::int32_t instance_param) {
  PooledEffectNode pooled = PooledNodeAt(PooledNodeShape::kThrownItem, source);
  // OÙ L'OBJET TOMBE : le mouvement y prend son cap au premier tic.
  pooled.target_position = target;
  pooled.node.life_ticks = effect_life_ticks;
  pooled.resource_paths = {std::string(pattern.texture)};
  pooled.tracks = SeedEffectThrownItemTracks(pattern, source, target, instance_param);
  return pooled;
}

}  // namespace ro::render
