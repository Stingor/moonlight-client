#include "ro/render/effect_class.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "ro/render/effect_node.h"
#include "ro/render/sprite_effect_table.h"
#include "ro/render/str_effect_setup.h"

#include "effect_class_internal.h"

namespace ro::render {
namespace {

using effect_class::kClientPi;
using effect_class::kHalfTurnDegrees;
using effect_class::kTickMs;

// La table de degrés compte un tour entier plus un.
constexpr std::int32_t kDegreeTableEntries = 361;
constexpr std::int32_t kLastDegree = 360;

// L'angle en radians que la table du client range pour un degré entier.
float TableRadians(std::int32_t degrees) {
  return static_cast<float>(static_cast<float>(degrees) / kHalfTurnDegrees) * kClientPi;
}

struct Registered {
  std::uint32_t effect_id;
  effect_class::Maker make;
};

// LES IDENTIFIANTS PORTÉS, et la classe qui répond pour chacun.
// ⚠ LES SPRITES D'UNE SEULE PARTICULE N'Y SONT PAS : leur table les tient —
// voir `ro/render/sprite_effect_table.h` — et `Find` la consulte ensuite.
constexpr std::array<Registered, 80> kRegistry = {{
    {984, effect_class::MakeRotateItem},
    {1000, effect_class::MakeRotateLine},
    {1002, effect_class::MakeScatter},
    {1059, effect_class::MakeFriggSong},
    {1069, effect_class::MakeBindTrap},
    {1070, effect_class::MakeBindTrap},
    {1071, effect_class::MakeBindTrap},
    {1099, effect_class::MakeMeteor},
    {1100, effect_class::MakeStemSpear},
    {1104, effect_class::MakePowdering},
    {1111, effect_class::MakeMeteor},
    {1113, effect_class::MakeLeashRings},
    {1128, effect_class::MakeSvgSpirit},
    {1200, effect_class::MakeJupitelThunderHit},
    {1202, effect_class::MakeStarEmperor},
    {1203, effect_class::MakeStarEmperor},
    {1324, effect_class::MakeTriangleShot},
    {1357, effect_class::MakeCartCannon},
    {1657, effect_class::MakeAxeBoomerang},
    {1752, effect_class::MakeRainOfCrystal},
    {1755, effect_class::MakeStormCannon},
    {1756, effect_class::MakeCrimsonArrow},
    {1768, effect_class::MakeAstralStrike},
    {1804, effect_class::MakeFloralFlareRoad},
    {1837, effect_class::MakeSavageImpact},
    {1867, effect_class::MakeShadowExceed},
    {1911, effect_class::MakePetitio},
    {1912, effect_class::MakeEternalSlash},
    {1957, effect_class::MakePneumaticus},
    {2114, effect_class::MakeCrimsonArrow},
    {2154, effect_class::MakeCrimsonArrow},
    {2183, effect_class::MakeAxeBoomerang},
    {2206, effect_class::MakeAcidifiedThrow},
    {2214, effect_class::MakeAcidifiedThrow},
    {2222, effect_class::MakeAcidifiedThrow},
    {2230, effect_class::MakeAcidifiedThrow},
    {2307, effect_class::MakeCaneOfEvilEye},
    {1106, effect_class::MakeMeteor},
    {1112, effect_class::MakeMeteor},
    // Ce que les cases d'emploi des actions de compétence sèment par numéro.
    {1973, effect_class::MakeGaleStorm},
    {2366, effect_class::MakeJupitelThunderStorm},
    // Ce que les états posent sur leur porteur.
    {1005, effect_class::MakeEnergyOrb},
    // Ce que le paquet des sphères élémentaires fait naître, par élément.
    {228, effect_class::MakeEnergyOrb},
    {735, effect_class::MakeEnergyOrb},
    {736, effect_class::MakeEnergyOrb},
    {737, effect_class::MakeEnergyOrb},
    {738, effect_class::MakeEnergyOrb},
    {1013, effect_class::MakeCherryBlossom},
    {2272, effect_class::MakeFullScreen},
    {1096, effect_class::MakeSpriteMable},
    {1097, effect_class::MakeTunaParty},
    {1110, effect_class::MakeBearerSprite},
    {1214, effect_class::MakeSunStance},
    {1215, effect_class::MakeSunStance},
    {1216, effect_class::MakeSunStance},
    {1217, effect_class::MakeSunStance},
    {1222, effect_class::MakeSoulUnity},
    {1800, effect_class::MakeBearerStr},
    {1839, effect_class::MakeBearerStr},
    {1913, effect_class::MakeEternalSlashCount},
    {2305, effect_class::MakeBearerStr},
    {2306, effect_class::MakeBearerStr},
    {1694, effect_class::MakeServantWeapons},
    {2109, effect_class::MakeShadowSpheres},
    // Les objets que lancent les compétences.
    {298, effect_class::MakeThrowItem},
    {299, effect_class::MakeThrowItem},
    {541, effect_class::MakeThrowItem},
    {539, effect_class::MakeThrowItem},
    {980, effect_class::MakeThrowItem},
    {308, effect_class::MakeThrowItem},
    {600, effect_class::MakeThrowItem},
    {613, effect_class::MakeThrowItem},
    {614, effect_class::MakeThrowItem},
    {615, effect_class::MakeThrowItem},
    {616, effect_class::MakeThrowItem},
    {914, effect_class::MakeThrowItem},
    {981, effect_class::MakeThrowItem},
    {982, effect_class::MakeThrowItem},
    {983, effect_class::MakeThrowItem},
    {989, effect_class::MakeThrowItem},
}};

// La fabrique d'un identifiant : celle du registre, puis celle des sprites d'une
// seule particule ; rien sinon.
effect_class::Maker Find(std::uint32_t effect_id) {
  const auto found = std::ranges::find(kRegistry, effect_id, &Registered::effect_id);
  if (found != kRegistry.end()) {
    return found->make;
  }
  return SpriteEffectRecipeFor(effect_id) != nullptr ? effect_class::MakeSpriteEffect : nullptr;
}

}  // namespace

float EffectClassSinTable(std::int32_t degrees) {
  const std::int32_t clamped = std::clamp(degrees, -kDegreeTableEntries, kLastDegree);
  if (clamped >= 0) {
    return static_cast<float>(std::sin(static_cast<double>(TableRadians(clamped))));
  }
  // La table qui précède : les radians opposés.
  return -TableRadians(kDegreeTableEntries + clamped);
}

float EffectClassCosTable(std::int32_t degrees) {
  const std::int32_t clamped = std::clamp(degrees, -kDegreeTableEntries, kLastDegree);
  if (clamped >= 0) {
    return static_cast<float>(std::cos(static_cast<double>(TableRadians(clamped))));
  }
  // La table qui précède : les sinus opposés.
  return -static_cast<float>(
      std::sin(static_cast<double>(TableRadians(kDegreeTableEntries + clamped))));
}

bool AdvanceEffectClassStrHost(EffectClassStrHost& host, const EffectClassTick& tick) {
  if (host.finished) {
    return false;
  }
  const std::optional<std::int32_t> last =
      tick.str_last_frame ? tick.str_last_frame(host.path) : std::nullopt;
  if (!last.has_value()) {
    host.finished = true;
    return false;
  }
  if (host.delay_ms > 0) {
    host.delay_ms -= kTickMs;
    return true;
  }
  if (host.counter > *last) {
    if (!host.loops) {
      host.finished = true;
      return false;
    }
    host.counter = 0;
  }
  host.shown_frame = host.counter;
  ++host.counter;
  return true;
}

void DrawEffectClassStrHost(const EffectClassStrHost& host, EffectClassOutput& output) {
  if (!host.shown_frame.has_value()) {
    return;
  }
  EffectClassStrDraw peint;
  peint.path = host.path;
  peint.allows_min_variant = host.allows_min_variant;
  peint.ground_position = EffectClassFromClient(host.position + host.offset);
  peint.frame = *host.shown_frame;
  peint.behind_actors = host.behind_actors;
  peint.lift_pixels = host.lift_pixels;
  peint.spin_degrees = host.spin_degrees;
  peint.scale = host.scale;
  peint.alpha = host.alpha;
  peint.flip_x = host.flip_x;
  output.strs.push_back(std::move(peint));
}

EffectClassInstance::EffectClassInstance(const EffectClassBirth& birth)
    : effect_id_(birth.effect_id),
      position_(birth.position),
      bound_position_(birth.bound_position),
      level_(birth.level),
      minimal_effects_(birth.minimal_effects),
      generator_(birth.seed) {}

void EffectClassInstance::Advance(const EffectClassTick& tick, EffectClassOutput& output) {
  if (tick.owner_position.has_value()) {
    position_ = *tick.owner_position;
  }
  culled_ = tick.culled;
  if (culled_) {
    return;
  }
  for (const Queued& queued : queued_) {
    if (queued.at_age != age_) {
      continue;
    }
    if (queued.quake) {
      output.quake_ms = std::max(output.quake_ms, kEffectQueuedQuakeMs);
    } else {
      output.sounds.push_back(EffectClassSound{queued.file, OursOf(position_)});
    }
  }
  Update(tick, output);
  ++age_;
}

bool EffectClassInstance::Alive(const EffectClassTick& /*tick*/) const {
  return age_ < duration_ticks_;
}

void EffectClassInstance::QueueSound(std::string_view file, std::int32_t at_age) {
  queued_.push_back(Queued{file, at_age, false});
}

void EffectClassInstance::QueueQuake(std::int32_t at_age) {
  queued_.push_back(Queued{{}, at_age, true});
}

std::int32_t EffectClassInstance::Rand() {
  return static_cast<std::int32_t>(NextEffectNodeDraw(generator_));
}

bool IsPortedEffectClass(std::uint32_t effect_id) { return Find(effect_id) != nullptr; }

std::vector<std::uint32_t> PortedEffectClassIds() {
  std::vector<std::uint32_t> ids;
  ids.reserve(kRegistry.size() + SpriteEffectRecipes().size());
  for (const Registered& entry : kRegistry) {
    ids.push_back(entry.effect_id);
  }
  for (const SpriteEffectRecipe& recipe : SpriteEffectRecipes()) {
    ids.push_back(recipe.effect_id);
  }
  std::ranges::sort(ids);
  return ids;
}

std::unique_ptr<EffectClassInstance> SpawnEffectClass(const EffectClassBirth& birth,
                                                      const EffectClassView& view) {
  const effect_class::Maker make = Find(birth.effect_id);
  return make == nullptr ? nullptr : make(birth, view);
}

namespace effect_class {

void SubmitQuad(const QuadParticle& particle, const QuadUv& uv, bool world_space,
                const EffectClassView& view, EffectClassOutput& output) {
  NativeMatrix matrix = particle.matrix;
  if (!world_space) {
    MultiplyRotationY(matrix, Radians(view.yaw_degrees));
    MultiplyTranslation(matrix, particle.position);
  }
  const std::array<glm::vec2, 4> texture_uv = {
      glm::vec2{uv.u_left, uv.v_top}, glm::vec2{uv.u_right, uv.v_top},
      glm::vec2{uv.u_left, uv.v_bottom}, glm::vec2{uv.u_right, uv.v_bottom}};
  // Le client range ses coins en BANDE ; le renderer les veut en boucle.
  constexpr std::array<std::size_t, 4> kLoop = {0, 1, 3, 2};
  EffectClassQuadDraw draw;
  draw.texture = particle.texture;
  draw.render_word = particle.render_word;
  for (std::size_t corner = 0; corner < kLoop.size(); ++corner) {
    draw.quad.corners[corner] =
        EffectClassFromClient(Transform(particle.corners[kLoop[corner]], matrix));
    draw.quad.texture_uv[corner] = texture_uv[kLoop[corner]];
  }
  draw.quad.tint = {LowByte(particle.color[0]), LowByte(particle.color[1]),
                    LowByte(particle.color[2]), LowByte(particle.alpha)};
  draw.quad.blend_source = particle.blend_source;
  draw.quad.blend_destination = particle.blend_destination;
  output.quads.push_back(draw);
}

std::optional<float> ScreenAngleDegrees(const EffectClassView& view, glm::vec3 from, glm::vec3 to) {
  if (!view.project) {
    return std::nullopt;
  }
  const std::optional<glm::vec2> a = view.project(EffectClassFromClient(from));
  const std::optional<glm::vec2> b = view.project(EffectClassFromClient(to));
  if (!a.has_value() || !b.has_value()) {
    return std::nullopt;
  }
  const glm::vec2 delta = *b - *a;
  const float length = std::sqrt(delta.x * delta.x + delta.y * delta.y);
  if (!(length > 0.0F)) {
    return std::nullopt;
  }
  const float theta = std::acos(delta.x / length) / kClientPi * kHalfTurnDegrees;
  return a->y > b->y ? kFullTurnDegrees - theta : theta;
}

}  // namespace effect_class
}  // namespace ro::render
