#include "ro/render/effect_glow_sprite_seed.h"

#include <algorithm>
#include <array>
#include <string>

#include "ro/render/effect_node.h"
#include "ro/render/effect_node_pool.h"
#include "ro/render/effect_pooled_node_shape.h"
#include "ro/render/effect_render_word.h"
#include "ro/render/effect_track.h"
#include "ro/render/effect_trail_history.h"

#include "effect_track_angles.h"

namespace ro::render {
namespace {

// LES CASES QUE LE BÂTISSEUR REMPLIT : sa marque de dessin, et celle du mélange
// que le dessinateur relit.
constexpr std::size_t kMarkOfDrawing = 0;
constexpr std::size_t kMarkOfAddedBlend = 2;

// COMBIEN DE CRANS DE TRAÎNÉE ce nœud déclare.
//
// ⚠ UN SEUL, et c'est le constructeur de nœud qui le pose — ce
// bâtisseur ne le change pas. Le sprite se dessine donc une fois. Lui en donner
// quatre d'autorité montrerait une rémanence que le client n'a pas.
constexpr std::size_t kTrailSteps = 1;

// LES STYLES QUE LE BÂTISSEUR NOMME.
constexpr std::uint8_t kStyleOfWarg = 2;
constexpr std::uint8_t kFirstStyleOfFullScale = 3;
constexpr std::uint8_t kStyleOfSuccubus = 4;
constexpr std::uint8_t kStyleOfGarm = 8;
constexpr std::uint8_t kStyleOfGardenKeeper = 9;
constexpr std::uint8_t kFirstStyleOfIfrit = 10;
constexpr std::uint8_t kStyleOfGinseng = 13;
constexpr std::uint8_t kStyleOfNepenthes = 14;
constexpr std::uint8_t kStyleOfImp = 15;

// LES SEUILS DE SEMIS, en tics d'âge de l'effet.
constexpr std::int32_t kSowingTicksOfDrawnHeading = 50;
constexpr std::int32_t kSowingTicksOfRing = 10;
constexpr std::int32_t kSowingTicksOfTriple = 30;
constexpr std::int32_t kSowingTicksOfOthers = 20;

// LES PAS DU CAP, en degrés par tic d'âge.
constexpr std::int32_t kHeadingPerTickOfRing = 36;
constexpr std::int32_t kHeadingPerTickOfHalfRing = 18;
constexpr std::int32_t kHeadingPerTickOfTriple = 12;

// LE RAYON DE LA RONDE des styles deux à huit, et les échelles fixes.
constexpr float kRingRadius = 40.0F;
constexpr float kScaleOfWarg = 0.6F;
constexpr float kFullScale = 1.0F;
constexpr float kScaleOfDrawnHeading = 0.7F;

// LES STYLES ZÉRO ET UN tirent leur cap sur le tour, et leur rayon de cinq à
// quarante-neuf.
constexpr std::uint32_t kRadiusDraws = 45;
constexpr std::int32_t kRadiusBase = 5;

// LE STYLE NEUF : un rayon et une échelle selon la parité de l'âge.
constexpr float kGardenKeeperOddRadius = 24.0F;
constexpr float kGardenKeeperEvenRadius = 12.0F;
constexpr float kGardenKeeperOddScale = 0.95F;
constexpr float kGardenKeeperEvenScale = 0.85F;

// LES STYLES DIX À DOUZE : une échelle tirée sur onze crans de trois centièmes.
constexpr float kIfritOddRadius = 12.0F;
constexpr float kIfritEvenRadius = 6.0F;
constexpr float kIfritScaleStep = 0.03F;
constexpr float kIfritOddScaleBase = 0.5F;
constexpr float kIfritEvenScaleBase = 0.4F;

// LE STYLE QUINZE : onze crans de cinq centièmes, sur une base de deux ou d'un et
// demi.
constexpr float kImpOddRadius = 10.0F;
constexpr float kImpEvenRadius = 4.0F;
constexpr float kImpScaleStep = 0.05F;
constexpr float kImpOddScaleBase = 2.0F;
constexpr float kImpEvenScaleBase = 1.5F;
constexpr std::uint32_t kScaleDraws = 11;

// LES STYLES TREIZE ET QUATORZE : trois rayons et trois échelles, selon l'âge
// modulo trois.
constexpr std::int32_t kTripleCycle = 3;
constexpr std::array<float, 3> kGinsengRadii = {8.0F, 15.0F, 22.0F};
constexpr std::array<float, 3> kGinsengScales = {0.9F, 1.0F, 1.1F};
constexpr std::array<float, 3> kNepenthesRadii = {8.0F, 14.0F, 20.0F};
constexpr std::array<float, 3> kNepenthesScales = {0.5F, 0.65F, 0.8F};

// LE TIRAGE QUE PERSONNE NE RELIT, entre l'écart et l'action.
constexpr std::uint32_t kUnreadDraws = 3;

// LA CADENCE D'IMAGE : tirée de six à huit, ou fixe.
constexpr std::uint32_t kHoldDraws = 3;
constexpr std::int32_t kHoldBase = 6;
constexpr std::int32_t kHoldOfConstructor = 4;
constexpr std::int32_t kHoldOfGinseng = 3;

// LES ACTIONS : huit directions tirées, au-dessus d'une base par style.
constexpr std::uint32_t kDirections = 8;
constexpr std::array<std::int32_t, 3> kSuccubusActionBases = {56, 48, 40};
constexpr std::int32_t kActionBaseOfGarm = 48;
constexpr std::int32_t kActionBaseOfGardenKeeper = 16;
constexpr std::int32_t kActionBaseOfIfrit = 32;

// LE MOT DE RENDU que le dessinateur passe pour un calque ordinaire.
constexpr std::uint32_t kOrdinarySpriteWord = 0x101;

[[nodiscard]] std::uint32_t Draw(std::uint32_t& generator, std::uint32_t below) {
  return NextEffectNodeDraw(generator) % below;
}

[[nodiscard]] bool IsOdd(std::int32_t tick) { return (tick & 1) != 0; }

// L'ÉCHELLE, LE RAYON ET LE CAP, premiers tirages du semis.
void DrawPlacement(std::uint8_t style, std::int32_t tick, std::uint32_t& generator,
                   EffectGlowSpriteDraws& rendered) {
  const auto rang_triple = static_cast<std::size_t>(tick % kTripleCycle);
  switch (style) {
    case kStyleOfWarg:
      rendered.sprite_scale = kScaleOfWarg;
      rendered.radius = kRingRadius;
      rendered.heading_degrees = kHeadingPerTickOfRing * tick;
      return;
    case kStyleOfGinseng:
      rendered.heading_degrees = kHeadingPerTickOfTriple * tick;
      rendered.radius = kGinsengRadii[rang_triple];
      rendered.sprite_scale = kGinsengScales[rang_triple];
      return;
    case kStyleOfNepenthes:
      rendered.heading_degrees = kHeadingPerTickOfTriple * tick;
      rendered.radius = kNepenthesRadii[rang_triple];
      rendered.sprite_scale = kNepenthesScales[rang_triple];
      return;
    case kStyleOfImp:
      rendered.heading_degrees = kHeadingPerTickOfHalfRing * tick;
      rendered.radius = IsOdd(tick) ? kImpOddRadius : kImpEvenRadius;
      rendered.sprite_scale = static_cast<float>(Draw(generator, kScaleDraws)) * kImpScaleStep +
                              (IsOdd(tick) ? kImpOddScaleBase : kImpEvenScaleBase);
      return;
    case kStyleOfGardenKeeper:
      rendered.heading_degrees = kHeadingPerTickOfHalfRing * tick;
      rendered.radius = IsOdd(tick) ? kGardenKeeperOddRadius : kGardenKeeperEvenRadius;
      rendered.sprite_scale = IsOdd(tick) ? kGardenKeeperOddScale : kGardenKeeperEvenScale;
      return;
    default:
      break;
  }
  if (style < kFirstStyleOfFullScale) {
    // ⚠ LE CAP SE TIRE AVANT LE RAYON.
    rendered.sprite_scale = kScaleOfDrawnHeading;
    rendered.heading_degrees = static_cast<std::int32_t>(
        Draw(generator, static_cast<std::uint32_t>(kEffectTrackFullTurnDegrees)));
    rendered.radius =
        static_cast<float>(static_cast<std::int32_t>(Draw(generator, kRadiusDraws)) + kRadiusBase);
    return;
  }
  if (style < kFirstStyleOfIfrit) {
    rendered.sprite_scale = kFullScale;
    rendered.radius = kRingRadius;
    rendered.heading_degrees = kHeadingPerTickOfRing * tick;
    return;
  }
  rendered.heading_degrees = kHeadingPerTickOfHalfRing * tick;
  rendered.radius = IsOdd(tick) ? kIfritOddRadius : kIfritEvenRadius;
  rendered.sprite_scale = static_cast<float>(Draw(generator, kScaleDraws)) * kIfritScaleStep +
                          (IsOdd(tick) ? kIfritOddScaleBase : kIfritEvenScaleBase);
}

// LA CADENCE ET L'ACTION, derniers tirages du semis.
void DrawAnimation(std::uint8_t style, std::uint32_t& generator, EffectGlowSpriteDraws& rendered) {
  const auto tiree = [&generator]() {
    return static_cast<std::int32_t>(Draw(generator, kHoldDraws)) + kHoldBase;
  };
  const auto direction = [&generator]() {
    return static_cast<std::int32_t>(Draw(generator, kDirections));
  };
  switch (style) {
    case kStyleOfSuccubus: {
      rendered.frame_hold_ticks = tiree();
      const auto base = static_cast<std::size_t>(
          Draw(generator, static_cast<std::uint32_t>(kSuccubusActionBases.size())));
      rendered.action = direction() + kSuccubusActionBases[base];
      return;
    }
    case kStyleOfGarm:
      rendered.frame_hold_ticks = tiree();
      rendered.action = direction() + kActionBaseOfGarm;
      return;
    case kStyleOfGardenKeeper:
      rendered.frame_hold_ticks = kHoldOfConstructor;
      rendered.action = direction() + kActionBaseOfGardenKeeper;
      return;
    case kStyleOfGinseng:
      rendered.frame_hold_ticks = kHoldOfGinseng;
      rendered.action = direction();
      return;
    case kStyleOfNepenthes:
      rendered.frame_hold_ticks = kHoldOfConstructor;
      rendered.action = 0;
      return;
    case kStyleOfImp:
      rendered.frame_hold_ticks = kHoldOfConstructor;
      rendered.action = direction();
      return;
    default:
      break;
  }
  if (style >= kFirstStyleOfIfrit) {
    rendered.frame_hold_ticks = kHoldOfConstructor;
    rendered.action = direction() + kActionBaseOfIfrit;
    return;
  }
  if (style >= kStyleOfWarg) {
    // DEUX À SEPT, le quatre et le huit mis à part : la cadence, puis la direction.
    rendered.frame_hold_ticks = tiree();
    rendered.action = direction();
    return;
  }
  // ZÉRO ET UN : ni tirage ni direction.
  rendered.frame_hold_ticks = kHoldOfConstructor;
  rendered.action = 0;
}

}  // namespace

const EffectGlowSpriteStyleSeed* EffectGlowSpriteStyleSeedFor(std::uint8_t style) {
  const std::span<const EffectGlowSpriteStyleSeed> table = EffectGlowSpriteStyleSeedTable();
  const auto found = std::ranges::find(table, style, &EffectGlowSpriteStyleSeed::style);
  return found == table.end() ? nullptr : &*found;
}

std::int32_t EffectGlowSpriteSowingTicks(std::uint8_t style) {
  if (style == kStyleOfGinseng || style == kStyleOfNepenthes) {
    return kSowingTicksOfTriple;
  }
  if (style < kStyleOfGardenKeeper) {
    return style < kStyleOfWarg ? kSowingTicksOfDrawnHeading : kSowingTicksOfRing;
  }
  return kSowingTicksOfOthers;
}

EffectGlowSpriteDraws DrawEffectGlowSprite(std::uint8_t style, std::int32_t effect_tick,
                                           std::uint32_t& generator) {
  EffectGlowSpriteDraws rendered;
  DrawPlacement(style, effect_tick, generator, rendered);
  (void)Draw(generator, kUnreadDraws);
  DrawAnimation(style, generator, rendered);
  return rendered;
}

std::uint32_t EffectGlowSpriteRenderWord(std::uint8_t style) {
  return style == kStyleOfImp ? kEffectRenderWordOfAdditiveSprite : kOrdinarySpriteWord;
}

PooledEffectNode SowEffectGlowSpriteNode(const EffectGlowSpriteNodePattern& pattern,
                                         glm::vec3 instance_position, std::int32_t effect_tick,
                                         std::int32_t effect_life_ticks,
                                         const EffectScatterWorld& world,
                                         std::uint32_t& generator) {
  PooledEffectNode pooled;
  pooled.shape = PooledNodeShape::kGlowSprite;
  // ⚠ LE NŒUD NE SUIT PAS SON PORTEUR : son mouvement ne relit jamais sa place.
  // C'est l'instance qui suit, et chaque monstre naît là où elle se trouve.
  pooled.stays_where_sown = true;
  // IL VIT CE QUI RESTE À SON INSTANCE.
  pooled.node.life_ticks = std::max(effect_life_ticks - effect_tick, 1);
  // ⚠ L'ALPHA NAÎT NUL, et c'est celui du NŒUD : le mouvement le monte de cinq
  // par tic jusqu'à cent cinquante. Le naître plein montrerait le monstre d'un
  // coup, là où le client le fait paraître.
  pooled.node.alpha = 0.0F;
  pooled.node.alpha_step_per_tick = 0.0F;
  pooled.node.render_word = EffectGlowSpriteRenderWord(pattern.style);

  const EffectGlowSpriteDraws tirages = DrawEffectGlowSprite(pattern.style, effect_tick, generator);
  // L'ÉCART DE LA RONDE est `(R·sin cap, 0, R·cos cap)` dans le repère du
  // client : sa profondeur se nie chez nous, sans quoi la ronde tourne en miroir.
  const float x = instance_position.x + tirages.radius * SinOfDegrees(tirages.heading_degrees);
  const float z = instance_position.z - tirages.radius * CosOfDegrees(tirages.heading_degrees);
  const float sol = world.ground_height ? world.ground_height(x, z) : instance_position.y;
  pooled.ground_position = {x, sol, z};
  pooled.owner_position = instance_position;
  pooled.node.sprite_scale = tirages.sprite_scale;
  pooled.node.action = tirages.action;
  pooled.node.frame_hold_ticks = tirages.frame_hold_ticks;

  const EffectGlowSpriteStyleSeed* seed = EffectGlowSpriteStyleSeedFor(pattern.style);
  if (seed == nullptr) {
    return pooled;
  }
  pooled.resource_paths = {std::string(seed->sprite_base)};
  EffectTrack& leader = pooled.tracks.front();
  leader.active = true;
  leader.mark[kMarkOfDrawing] = seed->draw_mark;
  leader.mark[kMarkOfAddedBlend] = pattern.style == kStyleOfImp ? 1 : 0;
  pooled.trail.assign(kTrailSteps, EffectTrailStep{});
  return pooled;
}

}  // namespace ro::render
