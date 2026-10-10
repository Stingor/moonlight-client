// LES CLASSES QUI POSENT DES SPRITES D'ACTEUR : les pétales de Huuma Ranka, les
// lances de Stem Spear, l'esprit de Silvervine, la poudre à chat, et les
// sprites que les états des Doram posent sur leur porteur. Les sprites d'une
// seule particule que le client décrit par une table vivent à part : voir
// `effect_class_sprite_effect.cpp`.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "ro/render/effect_render_word.h"

#include "effect_class_internal.h"

namespace ro::render::effect_class {

void AdvanceSpriteFrame(SpriteParticle& particle, const EffectClassTick& tick) {
  if (particle.spent || particle.play == SpritePlay::kManual || !tick.sprite_frame_count) {
    return;
  }
  const std::int32_t count = tick.sprite_frame_count(particle.sprite, particle.action);
  const auto period = static_cast<std::int64_t>(particle.period);
  if (period == 0 || (particle.age >= period && particle.age % period == 0)) {
    ++particle.frame;
  }
  if (particle.frame < count) {
    return;
  }
  switch (particle.play) {
    case SpritePlay::kLoop:
      particle.frame = 0;
      break;
    case SpritePlay::kOnce:
      particle.spent = true;
      break;
    case SpritePlay::kHold:
      particle.frame = std::max(count - 1, 0);
      break;
    case SpritePlay::kManual:
      break;
  }
}

void DrawSprite(const SpriteParticle& particle, EffectClassOutput& output) {
  if (particle.spent) {
    return;
  }
  EffectClassSpriteDraw draw;
  draw.sprite = particle.sprite;
  draw.ground_position = EffectClassFromClient(particle.position + particle.offset);
  draw.action = particle.action;
  draw.frame = particle.frame;
  draw.scale = particle.scale * particle.owner_scale;
  draw.spin_degrees = particle.spin_degrees;
  draw.tint = {255, 255, 255, LowByte(particle.alpha)};
  draw.keeps_layer_color = particle.keeps_layer_color;
  draw.additive = EffectSpriteIsAdditive(particle.render_word);
  draw.render_word = particle.render_word;
  draw.depth_bias = particle.depth_bias;
  output.sprites.push_back(draw);
}

namespace {

// ── Les pétales de Huuma Ranka (1002) ────────────────────────────────────────
//
// CINQUANTE PÉTALES nés quarante unités au-dessus du porteur, qui tombent de
// trois à six dixièmes par tic — la période d'image sert aussi de vitesse —, en
// dérivant d'un cinquième par tic dans une direction propre, et s'effacent en
// quatre-vingt-quatre tics. L'instance vit trois cents tics.
constexpr std::string_view kPetalSprite = "이팩트\\sakura01";
constexpr std::int32_t kPetals = 50;
constexpr std::int32_t kPetalDurationTicks = 300;
constexpr std::int32_t kPetalScaleBase = 22;
constexpr std::int32_t kPetalScaleSpan = 6;
constexpr float kPetalScaleUnit = 70.0F;
constexpr std::int32_t kPetalSpeedBase = 2;
constexpr std::int32_t kPetalSpeedSpan = 3;
constexpr float kPetalSpeedUnit = 0.15000001F;
constexpr std::int32_t kPetalAlpha = 250;
constexpr std::int32_t kPetalFade = 3;
constexpr float kPetalDrift = 0.2F;
constexpr float kPetalHeight = 40.0F;
constexpr std::uint32_t kPetalRenderWord = 1;

class Scatter final : public EffectClassInstance {
 public:
  explicit Scatter(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    duration_ticks_ = kPetalDurationTicks;
    for (std::int32_t rank = 0; rank < kPetals; ++rank) {
      Petal petal;
      petal.sprite.sprite = kPetalSprite;
      petal.sprite.render_word = kPetalRenderWord;
      petal.sprite.scale =
          static_cast<float>(kPetalScaleBase + std::max(0, Rand() % kPetalScaleSpan)) /
          kPetalScaleUnit;
      petal.sprite.period =
          static_cast<float>(kPetalSpeedBase + std::max(0, Rand() % kPetalSpeedSpan)) *
          kPetalSpeedUnit;
      petal.sprite.spin_degrees =
          static_cast<float>(std::max(0, Rand() % static_cast<std::int32_t>(kFullTurnDegrees)));
      petal.sprite.alpha = kPetalAlpha;
      petal.heading = std::max(0, Rand() % static_cast<std::int32_t>(kFullTurnDegrees));
      petals_.push_back(petal);
    }
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    for (const Petal& petal : petals_) {
      DrawSprite(petal.sprite, output);
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    for (Petal& petal : petals_) {
      petal.sprite.position = position_;
      petal.sprite.alpha = std::max(0, petal.sprite.alpha - kPetalFade);
      petal.fall += petal.sprite.period;
      petal.drift.x += EffectClassCosTable(petal.heading) * kPetalDrift;
      petal.drift.z += EffectClassSinTable(petal.heading) * kPetalDrift;
      petal.sprite.offset = {petal.drift.x, petal.fall - kPetalHeight, petal.drift.z};
      ++petal.sprite.age;
      AdvanceSpriteFrame(petal.sprite, tick);
    }
  }

 private:
  struct Petal {
    SpriteParticle sprite;
    std::int32_t heading = 0;
    float fall = 0.0F;
    glm::vec3 drift{0.0F};
  };
  std::vector<Petal> petals_;
};

// ── Les lances de Stem Spear (1100) et l'esprit de Silvervine (1128) ────────
//
// DES SPRITES QUI VOLENT DU LANCEUR VERS LA CIBLE, relue à chaque tic, dix
// unités au-dessus du lanceur, à la distance horizontale divisée par seize par
// tic ; ils pointent vers la cible à l'écran et s'effacent de quarante par tic
// passé le dixième. Quatre lances décalées de quatre tics, ou un seul esprit.
//
// ⚠ L'ALPHA DES LANCES N'A PAS DE PLANCHER : la quatrième repasse sous zéro, et
// seul l'octet bas compte — elle réapparaît.
constexpr std::int32_t kFlightLifeTicks = 16;
constexpr float kFlightHeight = 10.0F;
constexpr std::int32_t kFlightFadeFrom = 10;
constexpr std::int32_t kFlightFade = 40;
constexpr std::int32_t kSpearCount = 4;
constexpr std::int32_t kSpearStagger = 4;
constexpr std::uint32_t kFlightRenderWord = 257;

class FlyingSprites final : public EffectClassInstance {
 public:
  FlyingSprites(const EffectClassBirth& birth, std::string_view sprite, std::int32_t count)
      : EffectClassInstance(birth) {
    duration_ticks_ = kFlightLifeTicks;
    const glm::vec3 target = bound_position_.value_or(position_);
    const float dx = target.x - position_.x;
    const float dz = target.z - position_.z;
    float course = std::atan2(dx, -dz) / kClientPi * kHalfTurnDegrees;
    if (course < 0.0F) {
      course += kFullTurnDegrees;
    }
    float heading = kHalfTurnDegrees - course;
    if (heading < 0.0F) {
      heading += kFullTurnDegrees;
    }
    for (std::int32_t rank = 0; rank < count; ++rank) {
      Flyer flyer;
      flyer.sprite.sprite = sprite;
      flyer.sprite.render_word = kFlightRenderWord;
      flyer.sprite.period = std::sqrt(dx * dx + dz * dz) / static_cast<float>(kFlightLifeTicks);
      flyer.sprite.age = -kSpearStagger * rank;
      flyer.heading = static_cast<std::int32_t>(heading);
      flyers_.push_back(flyer);
    }
  }
  [[nodiscard]] bool Alive(const EffectClassTick& /*tick*/) const override {
    return std::ranges::any_of(
        flyers_, [](const Flyer& flyer) { return flyer.sprite.age < kFlightLifeTicks; });
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    for (const Flyer& flyer : flyers_) {
      if (flyer.sprite.age > 0 && flyer.sprite.age < kFlightLifeTicks) {
        DrawSprite(flyer.sprite, output);
      }
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    if (tick.bound_position.has_value()) {
      bound_position_ = tick.bound_position;
    }
    for (Flyer& flyer : flyers_) {
      if (flyer.sprite.age >= 0) {
        flyer.sprite.spin_degrees = static_cast<float>(flyer.heading) - tick.view->yaw_degrees;
        const glm::vec3 delta = bound_position_.value_or(position_) - position_;
        const float length = std::sqrt(glm::dot(delta, delta));
        if (length > 0.0F) {
          flyer.travelled += delta / length * flyer.sprite.period;
        }
        flyer.sprite.position = {position_.x + flyer.travelled.x, position_.y - kFlightHeight,
                                 position_.z + flyer.travelled.z};
        if (age_ >= kFlightFadeFrom) {
          flyer.sprite.alpha -= kFlightFade;
        }
      }
      ++flyer.sprite.age;
      if (flyer.sprite.age > 0 && flyer.sprite.age < kFlightLifeTicks) {
        AdvanceSpriteFrame(flyer.sprite, tick);
      }
    }
  }

 private:
  struct Flyer {
    SpriteParticle sprite;
    std::int32_t heading = 0;
    glm::vec3 travelled{0.0F};
  };
  std::vector<Flyer> flyers_;
};

// ── La poudre à chat (1104) ──────────────────────────────────────────────────
//
// UNE GRILLE DE FUMÉES VERTES qui grandissent et respirent, et autant de
// paillettes qui scintillent en descendant, trois, cinq ou sept de côté selon
// le niveau. Chaque particule renaît tant que l'unité est là, puis s'éteint.
constexpr std::string_view kPowderSmoke = "smoke2.bmp";
constexpr std::string_view kPowderGlitter = "이팩트\\particle2";
constexpr std::string_view kPowderSound = "effect\\su_cn_powdering.wav";
constexpr std::int32_t kPowderCellWidth = 5;
constexpr float kPowderSmokeLift = 10.0F;
constexpr std::int32_t kPowderGrowthSpan = 50;
constexpr std::int32_t kPowderCycleSpan = 21;
constexpr std::int32_t kPowderCycleBase = 30;
constexpr std::int32_t kPowderAlphaSpan = 100;
constexpr std::int32_t kPowderAlphaBase = 50;
constexpr float kPowderGrowthUnit = 0.1F;
constexpr float kPowderGrowthBase = 6.0F;
constexpr std::int32_t kPowderRedBase = 115;
constexpr std::int32_t kPowderBlueBase = 150;
constexpr std::int32_t kPowderGreen = 255;
constexpr std::int32_t kGlitterRiseSpan = 7;
constexpr std::int32_t kGlitterRiseBase = 7;
constexpr std::int32_t kGlitterScaleSpan = 3;
constexpr float kGlitterScaleBase = 0.5F;
constexpr float kGlitterScaleStep = 0.1F;
constexpr std::int32_t kGlitterAlphaStep = 30;
constexpr std::int32_t kGlitterAlphaTop = 255;
constexpr std::int32_t kGlitterAlphaHeld = 254;
constexpr std::int32_t kGlitterFallSpan = 10;
constexpr float kGlitterFallUnit = 0.01F;
constexpr std::uint32_t kGlitterRenderWord = 1;

enum class Breath : std::uint8_t { kRising, kFalling, kOut };

class Powdering final : public EffectClassInstance {
 public:
  explicit Powdering(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    QueueSound(kPowderSound, 0);
    const std::int32_t side = SideOf(level_);
    span_ = kPowderCellWidth * side;
    for (std::int32_t cell = 0; cell < side * side; ++cell) {
      Smoke smoke;
      smoke.offset.x = static_cast<float>(Rand() % span_ - span_ / 2 + 1);
      smoke.offset.z = static_cast<float>(Rand() % span_ - span_ / 2 + 1);
      smoke.quad.texture = kPowderSmoke;
      Respawn(smoke);
      smokes_.push_back(smoke);
    }
    for (std::int32_t cell = 0; cell < side * side; ++cell) {
      Glitter glitter;
      glitter.sprite.sprite = kPowderGlitter;
      glitter.sprite.render_word = kGlitterRenderWord;
      Respawn(glitter);
      glitters_.push_back(glitter);
    }
  }
  [[nodiscard]] bool Alive(const EffectClassTick& /*tick*/) const override {
    return std::ranges::any_of(smokes_, [](const Smoke& s) { return s.breath != Breath::kOut; }) ||
           std::ranges::any_of(glitters_,
                               [](const Glitter& g) { return g.breath != Breath::kOut; });
  }
  void Draw(const EffectClassView& view, EffectClassOutput& output) const override {
    for (const Smoke& smoke : smokes_) {
      if (smoke.breath == Breath::kOut) {
        continue;
      }
      QuadParticle quad = smoke.quad;
      quad.matrix = NativeScale(smoke.scale, smoke.scale, 1.0F);
      MultiplyRotationX(quad.matrix, Radians(view.pitch_degrees));
      SubmitQuad(quad, QuadUv{}, false, view, output);
    }
    for (const Glitter& glitter : glitters_) {
      if (glitter.breath != Breath::kOut) {
        DrawSprite(glitter.sprite, output);
      }
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    for (Smoke& smoke : smokes_) {
      if (smoke.breath == Breath::kOut) {
        continue;
      }
      const float scale = smoke.growth + smoke.scale;
      const std::int32_t step = smoke.peak / smoke.half_cycle;
      std::int32_t alpha = smoke.quad.alpha;
      if (smoke.breath == Breath::kRising) {
        alpha += step;
        if (alpha >= smoke.peak) {
          alpha = smoke.peak;
          smoke.breath = Breath::kFalling;
        }
      } else {
        alpha -= step;
        if (alpha < 0) {
          if (tick.owner_present) {
            smoke.breath = Breath::kRising;
            Respawn(smoke);
            continue;
          }
          alpha = 0;
          smoke.breath = Breath::kOut;
        }
      }
      smoke.quad.alpha = alpha;
      smoke.scale = scale;
      ++smoke.age;
      smoke.quad.position =
          position_ + glm::vec3{smoke.offset.x, -kPowderSmokeLift, smoke.offset.z};
    }
    for (Glitter& glitter : glitters_) {
      if (glitter.breath == Breath::kOut) {
        continue;
      }
      std::int32_t alpha = glitter.sprite.alpha;
      if (glitter.breath == Breath::kRising) {
        alpha += Rand() % kGlitterAlphaStep;
        if (alpha >= kGlitterAlphaTop) {
          alpha = kGlitterAlphaHeld;
          glitter.breath = Breath::kFalling;
        }
      } else {
        alpha -= Rand() % kGlitterAlphaStep;
        if (alpha < 0) {
          if (tick.owner_present) {
            glitter.breath = Breath::kRising;
            Respawn(glitter);
            continue;
          }
          alpha = 0;
          glitter.breath = Breath::kOut;
        }
      }
      glitter.sprite.alpha = alpha;
      glitter.sprite.offset.y += static_cast<float>(Rand() % kGlitterFallSpan) * kGlitterFallUnit;
      ++glitter.sprite.age;
      glitter.sprite.position = position_;
      AdvanceSpriteFrame(glitter.sprite, tick);
    }
  }

 private:
  struct Smoke {
    QuadParticle quad;
    glm::vec3 offset{0.0F};
    float scale = 0.0F;
    float growth = 0.0F;
    std::int32_t half_cycle = 1;
    std::int32_t peak = 0;
    std::int32_t age = 0;
    Breath breath = Breath::kRising;
  };
  struct Glitter {
    SpriteParticle sprite;
    Breath breath = Breath::kRising;
  };

  [[nodiscard]] static std::int32_t SideOf(std::int32_t level) {
    constexpr std::int32_t kSmall = 3;
    constexpr std::int32_t kMedium = 5;
    constexpr std::int32_t kLarge = 7;
    constexpr std::int32_t kSmallUpTo = 2;
    constexpr std::int32_t kMediumUpTo = 4;
    constexpr std::int32_t kLargeAt = 5;
    if (level >= 1 && level <= kSmallUpTo) {
      return kSmall;
    }
    if (level > kSmallUpTo && level <= kMediumUpTo) {
      return kMedium;
    }
    return level == kLargeAt ? kLarge : 0;
  }

  void Respawn(Smoke& smoke) {
    const std::int32_t growth = Rand() % kPowderGrowthSpan;
    const std::int32_t cycle = Rand() % kPowderCycleSpan;
    const std::int32_t peak = Rand() % kPowderAlphaSpan;
    const std::int32_t blue = Rand() % kPowderAlphaSpan;
    const std::int32_t red = Rand() % kPowderGrowthSpan;
    smoke.scale = 0.0F;
    smoke.quad.alpha = 0;
    smoke.half_cycle = kPowderCycleBase + cycle;
    smoke.growth = (static_cast<float>(growth + 1) * kPowderGrowthUnit + kPowderGrowthBase) /
                   static_cast<float>(2 * smoke.half_cycle);
    smoke.peak = kPowderAlphaBase + peak;
    smoke.quad.color = {kPowderRedBase - red, kPowderGreen, kPowderBlueBase - blue};
    smoke.breath = Breath::kRising;
  }

  void Respawn(Glitter& glitter) {
    const auto spot = [this]() { return static_cast<float>(Rand() % span_ - span_ / 2 + 1); };
    glitter.sprite.offset.x = spot();
    glitter.sprite.offset.y = -static_cast<float>(kGlitterRiseBase + Rand() % kGlitterRiseSpan);
    glitter.sprite.offset.z = spot();
    glitter.sprite.alpha = 0;
    glitter.breath = Breath::kRising;
    glitter.sprite.scale =
        kGlitterScaleBase - static_cast<float>(Rand() % kGlitterScaleSpan + 1) * kGlitterScaleStep;
  }

  std::int32_t span_ = 1;
  std::vector<Smoke> smokes_;
  std::vector<Glitter> glitters_;
};

// Le lacet de la caméra que la classe lit, zéro sans vue.
float CameraYaw(const EffectClassTick& tick) {
  return tick.view != nullptr ? tick.view->yaw_degrees : 0.0F;
}

// ── Les billes d'âme d'un Doram (1096) ──────────────────────────────────────
//
// TROIS BILLES — verte, rouge, bleue — en orbite de six unités autour du
// porteur, à un tiers de tour l'une de l'autre, qui avancent de deux centièmes
// de radian par tic en montant et descendant de trois unités autour d'une
// hauteur de six. Leur image n'avance pas seule : c'est leur direction vue de
// la caméra qui la choisit, parmi douze.
constexpr std::array<std::string_view, 3> kMableSprites = {
    "이팩트\\su_spritemable\\su_spritemable_green", "이팩트\\su_spritemable\\su_spritemable_red",
    "이팩트\\su_spritemable\\su_spritemable_blue"};
constexpr float kMableThirdTurn = 2.0943947F;
constexpr float kMableOrbitStep = 0.02F;
constexpr float kMableBobStep = 0.050000001F;
constexpr float kMableRadius = 6.0F;
constexpr float kMableBob = 3.0F;
constexpr float kMableHeight = 6.0F;
constexpr float kMableScale = 0.30000001F;
constexpr std::int32_t kMableAlpha = 200;
constexpr std::uint32_t kMableRenderWord = 1;
constexpr std::int32_t kWholeTurn = 360;
constexpr std::int32_t kMableSectorStart = 75;
constexpr std::int32_t kMableSectorSpan = 30;
constexpr std::int32_t kMableSectors = 12;

// L'image d'une bille : son cap en degrés entiers, moins le lacet de la caméra,
// découpé en secteurs de trente degrés qui commencent à soixante-quinze.
//
// ⚠ LA DIVISION TRONQUE VERS ZÉRO : de 46 à 74 degrés, le secteur vaut zéro et
// non moins un.
std::int32_t MableFrame(float orbit_radians, float camera_yaw_degrees) {
  const std::int32_t heading =
      static_cast<std::int32_t>(orbit_radians / kClientPi * kHalfTurnDegrees) % kWholeTurn;
  const auto facing = static_cast<std::int32_t>(static_cast<float>(heading) - camera_yaw_degrees);
  const std::int32_t in_turn = facing < 0 ? facing + kWholeTurn : facing;
  const std::int32_t sector = (in_turn - kMableSectorStart) / kMableSectorSpan;
  return sector >= 0 ? sector : sector + kMableSectors;
}

class SpriteMable final : public EffectClassInstance {
 public:
  explicit SpriteMable(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    for (std::size_t rank = 0; rank < kMableSprites.size(); ++rank) {
      Bead bead;
      bead.sprite.sprite = kMableSprites[rank];
      bead.sprite.play = SpritePlay::kManual;
      bead.sprite.scale = kMableScale;
      bead.sprite.alpha = kMableAlpha;
      bead.sprite.render_word = kMableRenderWord;
      bead.orbit = static_cast<float>(rank) * kMableThirdTurn;
      beads_.push_back(bead);
    }
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present;
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    for (const Bead& bead : beads_) {
      DrawSprite(bead.sprite, output);
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    for (Bead& bead : beads_) {
      bead.bob += kMableBobStep;
      bead.orbit += kMableOrbitStep;
      bead.sprite.position = position_;
      bead.sprite.offset = {std::sin(bead.orbit) * kMableRadius,
                            std::sin(bead.bob) * kMableBob - kMableHeight,
                            std::cos(bead.orbit) * kMableRadius};
      bead.sprite.frame = MableFrame(bead.orbit, CameraYaw(tick));
      ++bead.sprite.age;
    }
  }

 private:
  struct Bead {
    SpriteParticle sprite;
    float orbit = 0.0F;
    float bob = 0.0F;
  };
  std::vector<Bead> beads_;
};

// ── Le thon de Tuna Party (1097) ─────────────────────────────────────────────
//
// UN THON en orbite de dix unités, dix au-dessus du porteur, qui avance de cinq
// centièmes de radian par tic ; son image est sa direction vue de la caméra,
// parmi huit secteurs de quarante-cinq degrés qui commencent à 157,5.
constexpr std::string_view kTunaSprite = "이팩트\\su_tunaparty\\su_tunaparty";
constexpr float kTunaOrbitStep = 0.050000001F;
constexpr float kTunaRadius = 10.0F;
constexpr float kTunaHeight = 10.0F;
constexpr std::uint32_t kTunaRenderWord = 257;
constexpr float kTunaSectorStart = 157.5F;
constexpr float kTunaSectorSpan = 45.0F;
constexpr std::int32_t kTunaSectors = 8;

std::int32_t TunaFrame(float orbit_radians, float camera_yaw_degrees) {
  float heading = orbit_radians / kClientPi * kHalfTurnDegrees;
  if (heading > kFullTurnDegrees) {
    heading -= kFullTurnDegrees *
               static_cast<float>(static_cast<std::int32_t>(heading / kFullTurnDegrees));
  }
  float facing = heading - camera_yaw_degrees;
  if (facing < 0.0F) {
    facing += kFullTurnDegrees;
  }
  const auto sector = static_cast<std::int32_t>(
      (static_cast<double>(facing) - static_cast<double>(kTunaSectorStart)) /
      static_cast<double>(kTunaSectorSpan));
  return sector >= 0 ? sector : sector + kTunaSectors;
}

class TunaParty final : public EffectClassInstance {
 public:
  explicit TunaParty(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    sprite_.sprite = kTunaSprite;
    sprite_.play = SpritePlay::kManual;
    sprite_.render_word = kTunaRenderWord;
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present;
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    DrawSprite(sprite_, output);
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    orbit_ += kTunaOrbitStep;
    sprite_.position = position_;
    sprite_.offset = {std::sin(orbit_) * kTunaRadius, -kTunaHeight, std::cos(orbit_) * kTunaRadius};
    sprite_.frame = TunaFrame(orbit_, CameraYaw(tick));
    ++sprite_.age;
  }

 private:
  SpriteParticle sprite_;
  float orbit_ = 0.0F;
};

// ── Les racines de Root Twist (1110) ────────────────────────────────────────
//
// UN SPRITE qui boucle son action dans les couleurs de ses calques et suit son
// porteur tant qu'il est là, trois unités sous ses pieds, une image tous les
// huit tics.
constexpr std::string_view kRootTwistSprite = "이팩트\\su_sv_roottwist\\su_sv_roottwist";
constexpr float kRootTwistDepth = 3.0F;
constexpr float kRootTwistPeriod = 8.0F;
constexpr std::uint32_t kRootTwistRenderWord = 257;

class BearerSprite final : public EffectClassInstance {
 public:
  explicit BearerSprite(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    sprite_.keeps_layer_color = true;
    sprite_.sprite = kRootTwistSprite;
    sprite_.period = kRootTwistPeriod;
    sprite_.render_word = kRootTwistRenderWord;
    lift_ = -kRootTwistDepth;
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present;
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    if (sprite_.age > 0) {
      DrawSprite(sprite_, output);
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    ++sprite_.age;
    sprite_.position = position_ - glm::vec3{0.0F, lift_, 0.0F};
    AdvanceSpriteFrame(sprite_, tick);
  }

 private:
  SpriteParticle sprite_;
  // Au-dessus des pieds du porteur, en unités ; négatif en dessous.
  float lift_ = 0.0F;
};

}  // namespace

std::unique_ptr<EffectClassInstance> MakeScatter(const EffectClassBirth& birth,
                                                 const EffectClassView& /*view*/) {
  return std::make_unique<Scatter>(birth);
}

std::unique_ptr<EffectClassInstance> MakeStemSpear(const EffectClassBirth& birth,
                                                   const EffectClassView& /*view*/) {
  return std::make_unique<FlyingSprites>(birth, "이팩트\\sv_stemspear\\sv_stemspear", kSpearCount);
}

std::unique_ptr<EffectClassInstance> MakeSvgSpirit(const EffectClassBirth& birth,
                                                   const EffectClassView& /*view*/) {
  return std::make_unique<FlyingSprites>(birth, "몬스터\\svg_spirit", 1);
}

std::unique_ptr<EffectClassInstance> MakePowdering(const EffectClassBirth& birth,
                                                   const EffectClassView& /*view*/) {
  return std::make_unique<Powdering>(birth);
}

std::unique_ptr<EffectClassInstance> MakeSpriteMable(const EffectClassBirth& birth,
                                                     const EffectClassView& /*view*/) {
  return std::make_unique<SpriteMable>(birth);
}

std::unique_ptr<EffectClassInstance> MakeTunaParty(const EffectClassBirth& birth,
                                                   const EffectClassView& /*view*/) {
  return std::make_unique<TunaParty>(birth);
}

std::unique_ptr<EffectClassInstance> MakeBearerSprite(const EffectClassBirth& birth,
                                                      const EffectClassView& /*view*/) {
  return std::make_unique<BearerSprite>(birth);
}

}  // namespace ro::render::effect_class
