#include <algorithm>
#include <cmath>

#include "ro/render/effect_node.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_track_motion.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// COMMENT LA DISPERSION LIT SA PISTE.
//
// ⚠ TROIS COUPLES D'ANGLES OCCUPENT LES SIX PREMIÈRES CASES, et c'est la forme
// la plus inhabituelle de la famille : chaque couple est un angle COURANT suivi
// de sa CIBLE, et le mouvement rapproche l'un de l'autre d'un degré par tic.
// Les lire comme six valeurs indépendantes ferait dériver la nuée n'importe où.
constexpr std::size_t kWanderPairCount = 3;
constexpr std::size_t kSlotOfFirstWander = 0;
// La case où le bâtisseur — ou le premier passage — laisse la hauteur au-dessus
// de laquelle la particule change de comportement.
constexpr std::size_t kSlotOfHeightGate = 6;
// LA CASE DE STYLE : elle décide de tout, et le nom de la piste n'en dit rien.
constexpr std::size_t kSlotOfStyle = 7;
// L'angle propre, qui tourne d'un degré par tic et ne sert qu'à respirer.
constexpr std::size_t kSlotOfSpinDegrees = 8;
constexpr std::size_t kMarkOfTexture = 0;
constexpr std::size_t kMarkOfDrawing = 2;

// PASSÉ CE STYLE, LA PISTE EST GELÉE : seule sa respiration continue. Le client
// s'en sert pour poser une particule et l'oublier.
constexpr float kStyleThatFreezes = 99.0F;

// LES STYLES QUI ONT LEUR PROPRE MONTÉE D'ALPHA, et ce qu'elle vaut. Tous les
// autres montent de trois par tic pendant quatre-vingts tics.
constexpr float kStyleGatedByHeight = 0.0F;
constexpr float kStyleThatRisesByTwo = 2.0F;
constexpr float kStyleOnTheGround = 3.0F;
constexpr float kStyleThatDrifts = 4.0F;
constexpr float kStyleThatRisesSlowly = 5.0F;
constexpr float kFirstStyleThatCreeps = 11.0F;
constexpr float kLastStyleThatCreeps = 14.0F;

// Le compteur que le style à porte reçoit quand sa porte est fermée. Il n'a pas
// de sens en tics : il est simplement plus grand que toute durée de vie.
constexpr std::int32_t kTicksOfClosedGate = 300;

constexpr std::int32_t kTicksOfOrdinaryRise = 80;
constexpr std::int32_t kTicksOfLongRise = 170;
constexpr std::int32_t kTicksOfShortRise = 70;
constexpr std::int16_t kAlphaRiseOrdinary = 3;
constexpr std::int16_t kAlphaRiseByTwo = 2;
constexpr std::int16_t kAlphaRiseByOne = 1;

// LA MARQUE QUI FAIT DÉFILER SON IMAGE, tous les combien, et sur combien.
constexpr std::uint8_t kMarkThatCyclesFrames = 23;
constexpr std::int32_t kTicksBetweenFrames = 14;
constexpr std::uint8_t kLastFrame = 5;

// LA BOÎTE OÙ UNE PARTICULE RENAÎT, selon son style. La première est centrée sur
// le nœud ; la seconde l'ÉVITE — elle tire une distance minimale puis choisit un
// côté, ce qui creuse un trou au milieu. C'est de là que vient le champ d'étoiles.
constexpr std::int32_t kCentredBoxSpan = 300;
constexpr std::int32_t kCentredBoxHalf = 150;
constexpr std::int32_t kRingBoxSpan = 200;
constexpr std::int32_t kWideRingBoxSpan = 400;
constexpr std::int32_t kRingBoxInnerEdge = 25;

// DE COMBIEN LE STYLE AU SOL S'ENFONCE sous le relief, et la part tirée au sort.
constexpr float kDepthBelowGround = 20.0F;
constexpr std::int32_t kExtraDepthSpan = 5;

// À QUELLE ALTITUDE LES AUTRES STYLES SE POSENT, et c'est une altitude ABSOLUE.
//
// 🔴 LE CLIENT N'AJOUTE JAMAIS L'ORIGINE DU NŒUD À L'ORDONNÉE. Il ajoute bien
// l'abscisse et la cote — `x += origine.x ; z += origine.z`, à la fin de son
// semis, pour tous les styles —, mais l'ordonnée reste telle qu'il l'a tirée.
// Un ciel se tient donc à une HAUTEUR DU MONDE, la même où que soit le joueur,
// et c'est ce qui l'empêche de monter et descendre avec le relief sous ses pas.
//
// ⚠ ET LE SIGNE SE RETOURNE : son ordonnée descend, la nôtre monte. Quarante
// chez lui est quarante SOUS le plan d'origine ; quarante chez nous serait
// au-dessus, et le ciel passerait sous la carte.
constexpr float kHeightOfGatedStyle = 125.0F;
constexpr float kHeightOfRisingByTwo = 0.0F;
constexpr float kHeightOfRisingSlowly = -20.0F;
constexpr float kHeightOfStarField = -80.0F;
constexpr float kHeightOfOrdinaryCloud = -40.0F;
constexpr std::int32_t kHeightSpread = 10;

// LES DEUX STYLES DU CHAMP D'ÉTOILES, qui se posent plus haut que les nuages.
constexpr float kFirstStyleOfStars = 9.0F;
constexpr float kSecondStyleOfStars = 15.0F;

// DE COMBIEN LA PARTICULE DÉRIVE À CHAQUE TIC, au rythme de ses angles errants.
constexpr float kDriftOnTheGround = 0.015F;
constexpr float kDriftWhenSlow = 0.035F;
constexpr float kDriftOrdinary = 0.050000001F;
constexpr float kDriftSideways = 0.2F;

constexpr float kLastDegreeOfTurn = 359.0F;

// Un tirage entier dans [0, bound), comme le modulo du client.
[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& generator, std::int32_t bound) {
  return static_cast<std::int32_t>(NextEffectNodeDraw(generator) %
                                   static_cast<std::uint32_t>(bound));
}

// Le sinus d'un angle rangé en flottant, comme le client indexe sa table.
[[nodiscard]] float SineOfSlot(float degrees) { return std::sin(DegreesToRadians(degrees)); }

// L'altitude absolue d'un style, hors celui qui suit le relief.
[[nodiscard]] float AbsoluteHeightOfStyle(float style, std::uint32_t& generator) {
  const auto draw = static_cast<float>(DrawBelow(generator, kHeightSpread));
  if (style == kStyleGatedByHeight) {
    return kHeightOfGatedStyle - draw;
  }
  if (style == kStyleThatRisesByTwo) {
    return kHeightOfRisingByTwo - draw;
  }
  if (style == kStyleThatRisesSlowly) {
    return kHeightOfRisingSlowly - draw;
  }
  if (style == kFirstStyleOfStars || style == kSecondStyleOfStars) {
    return kHeightOfStarField - draw;
  }
  return kHeightOfOrdinaryCloud - draw;
}

// DE COMBIEN L'ALPHA MONTE, et jusqu'à quel tic. C'est ici que le style parle.
void RaiseAlpha(EffectTrack& track, float style, float node_height) {
  const std::int32_t ticks = track.ticks;
  if (style == kStyleGatedByHeight) {
    // ⚠ CE STYLE NE MONTE QUE SI SA PORTE EST PLUS HAUTE QUE LE NŒUD. C'est le
    // seul de la famille dont l'alpha dépend d'une position, et l'ignorer
    // ferait paraître une nuée que le client garde éteinte.
    // ⚠ LA PORTE EST ÉCRITE DANS LE REPÈRE DU CLIENT, qui descend : la hauteur
    // du nœud y passe avant la comparaison. Comparer à la nôtre ouvrait la
    // porte sous le sol au lieu de l'ouvrir au-dessus.
    if (ticks < kTicksOfOrdinaryRise && track.value[kSlotOfHeightGate] > -node_height) {
      track.alpha += kAlphaRiseByTwo;
    }
    return;
  }
  if (style == kStyleThatRisesByTwo || style == kStyleThatRisesSlowly) {
    if (ticks < kTicksOfOrdinaryRise) {
      track.alpha += kAlphaRiseByTwo;
    }
    return;
  }
  if (style == kStyleOnTheGround) {
    if (ticks < kTicksOfLongRise) {
      track.alpha += kAlphaRiseByOne;
    }
    return;
  }
  if (style >= kFirstStyleThatCreeps && style <= kLastStyleThatCreeps) {
    if (ticks < kTicksOfShortRise) {
      track.alpha += kAlphaRiseByOne;
    }
    return;
  }
  if (ticks < kTicksOfOrdinaryRise) {
    track.alpha += kAlphaRiseOrdinary;
  }
}

// OÙ LA PARTICULE RENAÎT, autour de l'origine — le nœud s'ajoute après.
void ScatterAgain(EffectTrack& track, float style, glm::vec3 node_position,
                  const EffectScatterWorld& world, std::uint32_t& generator) {
  if (style == kStyleGatedByHeight || style == kStyleThatRisesByTwo ||
      style == kStyleThatRisesSlowly || style == kStyleOnTheGround) {
    if (style == kStyleGatedByHeight && -node_position.y > track.value[kSlotOfHeightGate]) {
      // ⚠ LA PORTE SE REFERME EN RENAISSANT, et c'est la seule façon qu'a ce
      // style de rester invisible. Le client repose le compteur de tics à trois
      // cents — bien au-delà de toute durée de vie —, si bien que la particule
      // recommence aussitôt à mourir sans avoir rien montré. Poser zéro comme
      // pour les autres la ferait paraître là où le client la cache.
      track.ticks = kTicksOfClosedGate;
    }
    track.anchor.x = static_cast<float>(DrawBelow(generator, kCentredBoxSpan) - kCentredBoxHalf);
    track.anchor.z = static_cast<float>(DrawBelow(generator, kCentredBoxSpan) - kCentredBoxHalf);
    if (style == kStyleOnTheGround && world.ground_height) {
      // ⚠ ELLE SE POSE SOUS LE RELIEF, pas dessus : le client retranche vingt,
      // plus un tirage, à une altitude lue dans un repère qui DESCEND. Chez nous
      // l'ordonnée monte, donc la particule s'élève d'autant au-dessus du sol.
      const float sol =
          world.ground_height(node_position.x + track.anchor.x, node_position.z + track.anchor.z);
      track.anchor.y =
          sol + kDepthBelowGround + static_cast<float>(DrawBelow(generator, kExtraDepthSpan));
    } else if (style != kStyleOnTheGround) {
      // ⚠ LE STYLE AU SOL SANS SOL À LIRE GARDE LA HAUTEUR DE SON NŒUD, et ne
      // se rabat pas sur l'altitude des nuages : c'est visiblement faux sur un
      // relief, et préférable à une valeur inventée. Voir l'essai qui le tient.
      track.anchor.y = AbsoluteHeightOfStyle(style, generator);
    }
    return;
  }

  // LA BOÎTE QUI ÉVITE SON CENTRE : une distance minimale, puis un côté tiré au
  // sort. Tirer simplement dans un carré centré donnerait une nuée dense au
  // milieu, là où le client en creuse le cœur.
  const std::int32_t etendue = world.wide_scatter ? kWideRingBoxSpan : kRingBoxSpan;
  track.anchor.x = static_cast<float>(DrawBelow(generator, etendue) + kRingBoxInnerEdge);
  track.anchor.z = static_cast<float>(DrawBelow(generator, etendue) + kRingBoxInnerEdge);
  if (DrawBelow(generator, 2) == 0) {
    track.anchor.x = -track.anchor.x;
  }
  if (DrawBelow(generator, 2) == 0) {
    track.anchor.z = -track.anchor.z;
  }
  track.anchor.y = AbsoluteHeightOfStyle(style, generator);
}

// LES TROIS ANGLES ERRANTS avancent d'un degré vers leur cible, et s'en donnent
// une nouvelle quand ils l'atteignent.
void WanderAngles(EffectTrack& track, std::uint32_t& generator) {
  for (std::size_t pair = 0; pair < kWanderPairCount; ++pair) {
    const std::size_t current = kSlotOfFirstWander + 2 * pair;
    const std::size_t target = current + 1;
    if (track.value[target] <= track.value[current]) {
      track.value[current] = std::max(track.value[current] - 1.0F, 0.0F);
      if (track.value[target] >= track.value[current]) {
        const auto borne = static_cast<std::int32_t>(track.value[current]);
        const std::int32_t draw = borne != 0 ? DrawBelow(generator, borne) : 0;
        track.value[target] = kLastDegreeOfTurn - static_cast<float>(draw);
      }
      continue;
    }
    track.value[current] = std::min(track.value[current] + 1.0F, kLastDegreeOfTurn);
    if (track.value[current] > track.value[target]) {
      const auto borne = static_cast<std::int32_t>(track.value[current]);
      track.value[target] = borne != 0 ? static_cast<float>(DrawBelow(generator, borne)) : 0.0F;
    }
  }
}

// DE COMBIEN LA PARTICULE DÉRIVE, le style choisissant le pas.
void Drift(EffectTrack& track, float style) {
  const float le_long = SineOfSlot(track.value[kSlotOfFirstWander]);
  const float en_travers = SineOfSlot(track.value[kSlotOfFirstWander + 2]);
  if (style == kStyleOnTheGround) {
    track.anchor.x += le_long * kDriftOnTheGround;
    track.anchor.z += en_travers * kDriftOnTheGround;
    return;
  }
  if (style == kStyleThatDrifts) {
    // ⚠ CE STYLE DÉRIVE TOUJOURS DU MÊME CÔTÉ : le client prend la valeur
    // ABSOLUE de son sinus. Le laisser signé le ferait osciller sur place au
    // lieu de s'éloigner.
    track.anchor.x += std::abs(le_long) * kDriftSideways;
    track.anchor.z += en_travers * kDriftOrdinary;
    return;
  }
  if (style == kStyleThatRisesSlowly) {
    track.anchor.x += le_long * kDriftWhenSlow;
    track.anchor.z += en_travers * kDriftWhenSlow;
    return;
  }
  track.anchor.x += le_long * kDriftOrdinary;
  track.anchor.z += en_travers * kDriftOrdinary;
}

}  // namespace

void AdvanceScatteringParticleTracks(std::array<EffectTrack, kEffectTracksPerNode>& tracks,
                                     glm::vec3 node_position, std::int16_t billboard_turn_degrees,
                                     std::int16_t billboard_tilt_degrees,
                                     const EffectScatterWorld& world, std::uint32_t& generator) {
  // Les deux angles se rangent avant tout, comme dans la cascade.
  tracks[0].style = billboard_turn_degrees;
  tracks[1].style = billboard_tilt_degrees;

  for (EffectTrack& effect_track : tracks) {
    if (!effect_track.active) {
      continue;
    }
    const float style = effect_track.value[kSlotOfStyle];
    if (style < kStyleThatFreezes) {
      ++effect_track.ticks;
      if (effect_track.mark[kMarkOfDrawing] == kMarkThatCyclesFrames &&
          effect_track.ticks % kTicksBetweenFrames == 0) {
        effect_track.mark[kMarkOfTexture] =
            effect_track.mark[kMarkOfTexture] >= kLastFrame
                ? 0
                : static_cast<std::uint8_t>(effect_track.mark[kMarkOfTexture] + 1);
      }
      RaiseAlpha(effect_track, style, node_position.y);

      // ⚠ L'AZIMUT EST UNE DURÉE ICI. Passé ce tic, l'alpha redescend d'un par
      // tic, et la particule RENAÎT quand il touche zéro.
      if (effect_track.ticks > effect_track.azimuth_degrees) {
        --effect_track.alpha;
        if (effect_track.alpha <= 0) {
          effect_track.ticks = 0;
          effect_track.alpha = 0;
          ScatterAgain(effect_track, style, node_position, world, generator);
          // L'abscisse et la cote deviennent absolues ; l'ordonnée l'est déjà,
          // et le client n'y ajoute jamais l'origine de son nœud.
          effect_track.anchor.x += node_position.x;
          effect_track.anchor.z += node_position.z;
        }
      }
      WanderAngles(effect_track, generator);
      Drift(effect_track, style);
    }

    // ⚠ LA RESPIRATION CONTINUE MÊME GELÉE : le client la fait tourner hors de
    // tout test de style. Une piste figée respire donc encore, et c'est ce qui
    // l'empêche de paraître morte.
    effect_track.value[kSlotOfSpinDegrees] += 1.0F;
    if (effect_track.value[kSlotOfSpinDegrees] >= static_cast<float>(kEffectTrackFullTurnDegrees)) {
      effect_track.value[kSlotOfSpinDegrees] -= static_cast<float>(kEffectTrackFullTurnDegrees);
    }
  }
}

}  // namespace ro::render
