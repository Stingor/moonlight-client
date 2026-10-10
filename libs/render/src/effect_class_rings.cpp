// LES CLASSES QUI PEIGNENT DES RUBANS ET DES DÔMES : les quatre rubans spiralés
// de Huuma Ranka, l'explosion de Star Emperor, les anneaux de Leash.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string_view>
#include <vector>

#include "effect_class_internal.h"

namespace ro::render::effect_class {
namespace {

constexpr float kSegmentsPerRibbon = 21.0F;
constexpr std::int32_t kParticlesPerRibbon = 21;

// Le coin d'un segment : les trois composantes, et la particule qui le porte.
struct RibbonCorners {
  glm::vec3 low{0.0F};
  glm::vec3 high{0.0F};
};

// ── Les rubans spiralés (1000) ───────────────────────────────────────────────
//
// QUATRE RUBANS de vingt et un segments, à quatre-vingt-dix degrés l'un de
// l'autre, étagés en hauteur et en rayon, qui s'enroulent de cinq degrés par tic
// et s'ouvrent de trois, bleus et additifs. Chaque particule reprend l'état de la
// tête de son ruban, augmenté d'un pas par rang.
//
// ⚠ LA TÊTE GARDE DEUX COINS DE REPOS : elle dessine un éclat entre son point
// local (−1, ±1) et le début de son ruban.
constexpr std::string_view kSpiralTexture = "effect\\cloud11.tga";
constexpr std::int32_t kSpiralRibbons = 4;
constexpr std::int32_t kSpiralQuarterDegrees = 90;
constexpr std::int32_t kSpiralElevationBase = 80;
constexpr std::int32_t kSpiralElevationSpan = 21;
constexpr std::int32_t kSpiralOpening = 30;
constexpr std::int32_t kSpiralOpeningStep = 3;
constexpr std::int32_t kSpiralOpeningCap = 180;
constexpr std::int32_t kSpiralTurnStep = 5;
constexpr std::int32_t kSpiralAlpha = 50;
constexpr float kSpiralRadius = 7.0F;
constexpr float kSpiralRadiusBonus = 3.0F;
constexpr float kSpiralHeight = 28.0F;
constexpr float kSpiralHeightStep = 5.0F;
constexpr float kSpiralBand = 3.0F;
constexpr std::array<std::int32_t, 3> kSpiralColor = {100, 100, 255};
constexpr std::int32_t kSpiralBrighteningTicks = 12;
constexpr std::int32_t kSpiralDimmingAfter = 50;
constexpr float kSpiralGrowth = 0.1F;

class RotateLine final : public EffectClassInstance {
 public:
  explicit RotateLine(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    for (std::int32_t ribbon = 0; ribbon < kSpiralRibbons; ++ribbon) {
      const std::int32_t elevation =
          kSpiralElevationBase + std::max(0, Rand() % kSpiralElevationSpan);
      for (std::int32_t rank = 0; rank < kParticlesPerRibbon; ++rank) {
        Particle particle;
        particle.rank = rank;
        particle.heading = static_cast<float>(kSpiralQuarterDegrees * ribbon);
        particle.opening = kSpiralOpening;
        particle.elevation = elevation;
        particle.radius = kSpiralRadius - static_cast<float>(ribbon) + kSpiralRadiusBonus;
        particle.height = kSpiralHeight - kSpiralHeightStep * static_cast<float>(ribbon);
        particle.quad.texture = kSpiralTexture;
        particle.quad.alpha = kSpiralAlpha;
        particle.quad.color = kSpiralColor;
        particles_.push_back(particle);
      }
    }
  }

  void Draw(const EffectClassView& view, EffectClassOutput& output) const override {
    for (const Particle& particle : particles_) {
      const QuadUv uv{0.0F, 1.0F, static_cast<float>(particle.rank - 1) / kSegmentsPerRibbon,
                      static_cast<float>(particle.rank) / kSegmentsPerRibbon};
      SubmitQuad(particle.quad, uv, false, view, output);
    }
  }

 protected:
  void Update(const EffectClassTick& /*tick*/, EffectClassOutput& /*output*/) override {
    for (Particle& particle : particles_) {
      particle.quad.position = position_;
      if (particle.rank == 0) {
        angle_ = static_cast<std::int32_t>(particle.heading);
        opening_ = particle.opening;
        alpha_ = particle.quad.alpha;
        radius_ = particle.radius;
        loaded_ = true;
      } else if (!loaded_) {
        ++particle.age;
        continue;
      }
      angle_ += kSpiralTurnStep;
      particle.heading = static_cast<float>(angle_ % static_cast<std::int32_t>(kFullTurnDegrees));
      opening_ = std::min(opening_ + kSpiralOpeningStep, kSpiralOpeningCap);
      particle.opening = opening_;
      if (particle.age <= kSpiralBrighteningTicks) {
        ++alpha_;
        particle.quad.alpha = alpha_;
      } else {
        radius_ += kSpiralGrowth;
        particle.radius = radius_;
        if (particle.age > kSpiralDimmingAfter) {
          alpha_ = std::max(0, alpha_ - 1);
          particle.quad.alpha = alpha_;
        }
      }
      const auto step_index =
          static_cast<std::int32_t>(static_cast<float>(particle.opening) / kSegmentsPerRibbon);
      const std::int32_t angle_degrees =
          static_cast<std::int32_t>(static_cast<float>(particle.rank * step_index) +
                                    particle.heading) %
          static_cast<std::int32_t>(kFullTurnDegrees);
      const float cosinus = EffectClassCosTable(angle_degrees);
      const float sinus = EffectClassSinTable(angle_degrees);
      const float lean_cos = EffectClassCosTable(particle.elevation);
      const float lean_sin = EffectClassSinTable(particle.elevation);
      const glm::vec3 low{cosinus * particle.radius, -particle.height, sinus * particle.radius};
      const glm::vec3 high{low.x + lean_cos * cosinus, (kSpiralBand - lean_sin) - particle.height,
                           low.z + lean_cos * sinus};
      particle.quad.corners[3] = low;
      particle.quad.corners[1] = high;
      if (particle.rank != 0) {
        particle.quad.corners[0] = previous_.high;
        particle.quad.corners[2] = previous_.low;
      }
      previous_ = RibbonCorners{low, high};
      ++particle.age;
    }
  }

 private:
  struct Particle {
    QuadParticle quad;
    std::int32_t rank = 0;
    float heading = 0.0F;
    std::int32_t opening = 0;
    std::int32_t elevation = 0;
    float radius = 0.0F;
    float height = 0.0F;
    std::int32_t age = 0;
  };
  std::vector<Particle> particles_;
  // L'état que la tête recharge et que les suivantes font avancer.
  float radius_ = 0.0F;
  std::int32_t angle_ = 0;
  std::int32_t opening_ = 0;
  std::int32_t alpha_ = 0;
  bool loaded_ = false;
  RibbonCorners previous_;
};

// ── L'explosion de Star Emperor (1203, 1202) ────────────────────────────────
//
// UN DÔME de soixante-douze quads sur la sphère unité, mis à l'échelle et tourné
// selon le temps, et UN ANNEAU AU SOL de dix segments qui s'élargit. Pour 1203,
// le temps se replie autour du milieu de la vie : implosion puis explosion.
//
// ⚠ L'ALPHA DÉBORDE, et seul son octet bas compte : aux âges 0 à 15 et 45 à 59
// de 1203, il vaut 510 à 765, ce qui se lit 254, 15, 32… à l'écran.
constexpr std::uint32_t kStarEmperorFull = 1203;
constexpr std::int32_t kStarEmperorFullTicks = 60;
constexpr std::int32_t kStarEmperorShortTicks = 30;
constexpr std::string_view kDomeTexture = "effect\\star_emperor\\boom.tga";
constexpr std::string_view kRingTexture = "effect\\star_emperor\\ring_white.tga";
constexpr std::string_view kFirstPillarSound = "effect\\wizard_fire_pillar_a.wav";
constexpr std::string_view kSecondPillarSound = "effect\\wizard_fire_pillar_b.wav";
constexpr std::int32_t kFirstPillarTick = 1;
constexpr std::int32_t kSecondPillarTick = 31;
constexpr std::int32_t kDomeLatitudes = 6;
constexpr std::int32_t kDomeLongitudes = 12;
constexpr float kDomeLatitudeStep = 30.0F;
constexpr float kDomePoleDegrees = 90.0F;
constexpr float kDomeLongitudeStep = -30.0F;
constexpr std::uint32_t kDomeRenderWord = 1;
constexpr std::uint32_t kRingRenderWord = 513;
constexpr std::int32_t kRingSegments = 10;
constexpr std::int32_t kRingSegmentDegrees = 36;
constexpr float kDomeGrowth = 1.15F;
constexpr float kRingGrowth = 1.75F;
constexpr float kRingThickness = 12.0F;
constexpr float kRingUvSpan = 0.25F;
constexpr std::int32_t kFullAlpha = 255;
constexpr float kHalf = 0.5F;
constexpr std::int32_t kAlphaQuarters = 4;
constexpr std::int32_t kSpinPerTime = 3;

// Le point de la sphère unité à cette latitude et cette longitude.
glm::vec3 SpherePoint(float latitude_degrees, float longitude_degrees) {
  NativeMatrix matrix = NativeRotationX(Radians(latitude_degrees));
  MultiplyRotationY(matrix, Radians(longitude_degrees));
  return Transform(glm::vec3{0.0F, 0.0F, -1.0F}, matrix);
}

class StarEmperor final : public EffectClassInstance {
 public:
  explicit StarEmperor(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    full_ = effect_id_ == kStarEmperorFull;
    duration_ticks_ = full_ ? kStarEmperorFullTicks : kStarEmperorShortTicks;
    if (full_) {
      QueueSound(kFirstPillarSound, kFirstPillarTick);
      QueueSound(kSecondPillarSound, kSecondPillarTick);
    }
    for (std::int32_t latitude = 0; latitude < kDomeLatitudes; ++latitude) {
      for (std::int32_t longitude = 0; longitude < kDomeLongitudes; ++longitude) {
        DomePiece piece;
        const auto lat = [](std::int32_t r) {
          return kDomeLatitudeStep * static_cast<float>(r) - kDomePoleDegrees;
        };
        const auto lon = [](std::int32_t k) { return kDomeLongitudeStep * static_cast<float>(k); };
        piece.quad.texture = kDomeTexture;
        piece.quad.render_word = kDomeRenderWord;
        piece.quad.corners = {SpherePoint(lat(latitude), lon(longitude)),
                              SpherePoint(lat(latitude), lon(longitude + 1)),
                              SpherePoint(lat(latitude + 1), lon(longitude)),
                              SpherePoint(lat(latitude + 1), lon(longitude + 1))};
        piece.uv = {std::clamp(static_cast<float>(latitude) / kDomeLatitudes, 0.0F, 1.0F),
                    std::clamp(static_cast<float>(latitude + 1) / kDomeLatitudes, 0.0F, 1.0F),
                    std::clamp(static_cast<float>(longitude) / kDomeLongitudes, 0.0F, 1.0F),
                    std::clamp(static_cast<float>(longitude + 1) / kDomeLongitudes, 0.0F, 1.0F)};
        dome_.push_back(piece);
      }
    }
    for (std::int32_t segment = 0; segment < kRingSegments; ++segment) {
      QuadParticle quad;
      quad.texture = kRingTexture;
      quad.render_word = kRingRenderWord;
      ring_.push_back(quad);
    }
  }

  void Draw(const EffectClassView& view, EffectClassOutput& output) const override {
    for (const DomePiece& piece : dome_) {
      SubmitQuad(piece.quad, piece.uv, false, view, output);
    }
    for (const QuadParticle& quad : ring_) {
      SubmitQuad(quad, QuadUv{0.0F, 1.0F, 0.0F, kRingUvSpan}, false, view, output);
    }
  }

 protected:
  void Update(const EffectClassTick& /*tick*/, EffectClassOutput& /*output*/) override {
    // Toutes les particules ont l'âge de l'instance : nées ensemble, elles
    // vieillissent ensemble.
    const std::int32_t time = TimeOf(age_);
    const std::int32_t alpha = AlphaOf(age_, time);
    const std::int32_t triangle = time * (time + 1) / 2;
    const float duration = static_cast<float>(duration_ticks_);
    const float scale = -(kDomeGrowth / duration) * kHalf * static_cast<float>(triangle) +
                        kDomeGrowth * static_cast<float>(time);
    for (DomePiece& piece : dome_) {
      piece.quad.matrix = NativeScale(scale, scale, scale);
      MultiplyRotationY(piece.quad.matrix, Radians(static_cast<float>(kSpinPerTime * time)));
      piece.quad.alpha = alpha;
      piece.quad.position = position_;
    }
    const float outer = -(kRingGrowth / duration) * kHalf * static_cast<float>(triangle) +
                        kRingGrowth * static_cast<float>(time);
    const float inner = outer > kRingThickness ? outer - kRingThickness : 0.0F;
    for (std::size_t segment = 0; segment < ring_.size(); ++segment) {
      QuadParticle& quad = ring_[segment];
      const auto from = static_cast<std::int32_t>(segment) * kRingSegmentDegrees;
      const std::int32_t to = from + kRingSegmentDegrees;
      const auto point = [](std::int32_t degrees, float radius) {
        return glm::vec3{EffectClassCosTable(degrees) * radius,
                         EffectClassSinTable(degrees) * radius, 0.0F};
      };
      quad.corners = {point(from, outer), point(to, outer), point(from, inner), point(to, inner)};
      quad.matrix = NativeRotationX(Radians(kDomePoleDegrees));
      quad.alpha = alpha;
      quad.position = position_;
    }
  }

 private:
  // Le temps d'une particule : son âge plus un, replié autour du milieu de la
  // vie pour 1203.
  [[nodiscard]] std::int32_t TimeOf(std::int32_t age) const {
    return full_ ? std::abs(age - duration_ticks_ / 2) : age + 1;
  }
  [[nodiscard]] std::int32_t AlphaOf(std::int32_t age, std::int32_t time) const {
    if (full_) {
      const std::int32_t quarter = duration_ticks_ / kAlphaQuarters;
      const std::int32_t folded = quarter <= time ? duration_ticks_ - time : time;
      return kFullAlpha / quarter * folded;
    }
    const std::int32_t half = duration_ticks_ / 2;
    return half <= age ? kFullAlpha / half * (duration_ticks_ - time) : kFullAlpha / half * time;
  }

  struct DomePiece {
    QuadParticle quad;
    QuadUv uv;
  };
  bool full_ = true;
  std::vector<DomePiece> dome_;
  std::vector<QuadParticle> ring_;
};

// ── Les anneaux de Leash (1113) ─────────────────────────────────────────────
//
// DEUX RUBANS-CYLINDRES BLANCS teintés de violet, rayons 4,6 et 5,0, dont la
// hauteur ondule jusqu'à soixante, et des étincelles vertes pâles tous les six
// tics jusqu'au quarante-huitième. Les anneaux paraissent en seize tics,
// tiennent jusqu'au soixantième et s'effacent. La tête de chaque ruban ne se
// peint pas.
constexpr std::string_view kLeashRingTexture = "effect\\ring_white.tga";
constexpr std::string_view kSparkTexture = "effect\\pok1.tga";
constexpr std::string_view kSparkBlinkTexture = "effect\\pok3.tga";
constexpr std::int32_t kLeashDurationTicks = 100;
constexpr std::int32_t kLeashRings = 2;
constexpr float kLeashHalfTurn = 180.0F;
constexpr std::array<std::int32_t, 3> kLeashColor = {115, 25, 115};
constexpr float kLeashRadius = 4.6F;
constexpr float kLeashRadiusStep = 0.4F;
constexpr float kLeashWaveHeight = 60.0F;
constexpr std::int32_t kLeashFadeStart = 60;
constexpr std::int32_t kLeashRisingTicks = 16;
constexpr std::int32_t kLeashAlphaRise = 5;
constexpr std::int32_t kLeashAlphaCap = 180;
constexpr std::int32_t kLeashAlphaFall = 2;
constexpr float kLeashWaveShare = 0.25F;
constexpr float kLeashRestShare = 0.75F;
constexpr std::int32_t kLeashWaveStride = 34;
constexpr std::int32_t kLeashWaveSpeed = 4;
constexpr std::int32_t kLeashRiseAgeCap = 90;
constexpr std::int32_t kLeashSegments = 20;
constexpr std::int32_t kLeashElevation = 90;
constexpr std::int32_t kSparkSowingLast = 50;
constexpr std::int32_t kSparkSowingPeriod = 6;
constexpr std::int32_t kSparksPerSowing = 4;
constexpr std::array<std::int32_t, 3> kSparkColor = {220, 250, 220};
constexpr std::int32_t kSparkOffsetSpan = 13;
constexpr std::int32_t kSparkOffsetShift = 6;
constexpr std::int32_t kSparkSizeSpan = 10;
constexpr float kSparkSizeStep = 0.1F;
constexpr float kSparkSizeBase = 2.0F;
constexpr std::int32_t kSparkRiseSpan = 40;
constexpr float kSparkRiseStep = 0.01F;
constexpr float kSparkRiseBase = 0.4F;
constexpr std::int32_t kSparkTurnStep = 5;
constexpr std::int32_t kSparkBrighteningTicks = 10;
constexpr std::int32_t kSparkAlphaRise = 15;
constexpr std::int32_t kSparkAlphaFall = 3;
constexpr std::int32_t kSparkBlinkSteps = 5;
constexpr std::int32_t kSparkBlinkStride = 7;
constexpr std::int32_t kSparkSecondBlink = 36;
constexpr std::int32_t kSquareQuarter = 90;
constexpr std::int32_t kSparkTiltBase = 450;
constexpr std::int32_t kSparkAlphaCapOnWideMap = 100;

class LeashRings final : public EffectClassInstance {
 public:
  explicit LeashRings(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    duration_ticks_ = kLeashDurationTicks;
  }

  void Draw(const EffectClassView& view, EffectClassOutput& output) const override {
    for (const RingParticle& particle : rings_) {
      if (particle.rank == 0) {
        continue;
      }
      const QuadUv uv{0.0F, 1.0F, static_cast<float>(particle.rank - 1) / kSegmentsPerRibbon,
                      static_cast<float>(particle.rank) / kSegmentsPerRibbon};
      SubmitQuad(particle.quad, uv, false, view, output);
    }
    for (const Spark& spark : sparks_) {
      SubmitQuad(SparkQuad(spark, view), QuadUv{}, false, view, output);
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    Sow();
    for (RingParticle& particle : rings_) {
      particle.quad.position = position_;
      const std::int32_t age = particle.age;
      std::int32_t alpha = particle.quad.alpha;
      if (fade_start_ < 0 || fade_start_ > age) {
        if (age < kLeashRisingTicks) {
          alpha = std::min(alpha + kLeashAlphaRise, kLeashAlphaCap);
        }
      } else {
        alpha = std::max(alpha - kLeashAlphaFall, 0);
      }
      particle.quad.alpha = alpha;
      const std::int32_t wave =
          (age * (kLeashWaveSpeed - particle.ring) + kLeashWaveStride * particle.rank) %
          static_cast<std::int32_t>(kFullTurnDegrees);
      float height = kLeashWaveHeight * kLeashWaveShare * EffectClassSinTable(wave) +
                     kLeashWaveHeight * kLeashRestShare;
      if (age <= kLeashRiseAgeCap) {
        height *= EffectClassSinTable(age);
      }
      height = std::max(0.0F, height);
      const std::int32_t step =
          static_cast<std::int32_t>(kFullTurnDegrees / static_cast<float>(kLeashSegments));
      const std::int32_t angle =
          static_cast<std::int32_t>(static_cast<float>(particle.rank * step) + particle.start) %
          static_cast<std::int32_t>(kFullTurnDegrees);
      const float lean = height * EffectClassCosTable(kLeashElevation);
      const float up = height * EffectClassSinTable(kLeashElevation);
      const float cosinus = EffectClassCosTable(angle);
      const float sinus = EffectClassSinTable(angle);
      const glm::vec3 low{particle.radius * cosinus, 0.0F, particle.radius * sinus};
      const glm::vec3 high{low.x + lean * cosinus, 0.0F - up, low.z + lean * sinus};
      particle.quad.corners[3] = low;
      particle.quad.corners[1] = high;
      if (particle.rank != 0) {
        particle.quad.corners[0] = previous_.high;
        particle.quad.corners[2] = previous_.low;
      }
      previous_ = RibbonCorners{low, high};
      ++particle.age;
    }
    for (Spark& spark : sparks_) {
      spark.quad.position = position_;
      spark.turn += spark.turn - kSparkTurnStep >= 0
                        ? -kSparkTurnStep
                        : static_cast<std::int32_t>(kFullTurnDegrees) - kSparkTurnStep;
      std::int32_t alpha = spark.quad.alpha;
      if (spark.age <= kSparkBrighteningTicks) {
        alpha += kSparkAlphaRise;
      } else {
        const bool blink = kSparkBlinkStride * (kSparkBlinkSteps - spark.rank) == spark.age ||
                           kSparkSecondBlink - kSparkBlinkStride * spark.rank == spark.age;
        spark.quad.texture = blink ? kSparkBlinkTexture : kSparkTexture;
      }
      spark.quad.alpha = std::max(0, alpha - kSparkAlphaFall);
      spark.offset.y -= spark.rise;
      // Sur la carte qu'il nomme, le client plafonne l'étincelle à cent.
      if (tick.wide_scatter_map && spark.quad.alpha > kSparkAlphaCapOnWideMap) {
        spark.quad.alpha = kSparkAlphaCapOnWideMap;
      }
      ++spark.age;
    }
  }

 private:
  struct RingParticle {
    QuadParticle quad;
    std::int32_t rank = 0;
    std::int32_t ring = 0;
    float start = 0.0F;
    float radius = 0.0F;
    std::int32_t age = 0;
  };
  struct Spark {
    QuadParticle quad;
    std::int32_t rank = 0;
    std::int32_t turn = 0;
    glm::vec3 offset{0.0F};
    float size = 0.0F;
    float rise = 0.0F;
    std::int32_t age = 0;
  };

  void Sow() {
    if (sowing_age_ == 0) {
      static_cast<void>(Rand());
      for (std::int32_t ring = 0; ring < kLeashRings; ++ring) {
        static_cast<void>(Rand());
        for (std::int32_t rank = 0; rank < kParticlesPerRibbon; ++rank) {
          RingParticle particle;
          particle.rank = rank;
          particle.ring = ring;
          particle.start = kLeashHalfTurn * static_cast<float>(ring);
          particle.radius = kLeashRadius + kLeashRadiusStep * static_cast<float>(ring);
          particle.quad.texture = kLeashRingTexture;
          particle.quad.color = kLeashColor;
          particle.quad.alpha = 0;
          rings_.push_back(particle);
        }
      }
      fade_start_ = kLeashFadeStart;
    }
    if (sowing_age_ <= kSparkSowingLast && sowing_age_ % kSparkSowingPeriod == 0) {
      for (std::int32_t rank = 0; rank < kSparksPerSowing; ++rank) {
        Spark spark;
        spark.rank = rank;
        spark.quad.texture = kSparkTexture;
        spark.quad.color = kSparkColor;
        spark.quad.alpha = 0;
        spark.offset.z = static_cast<float>(Rand() % kSparkOffsetSpan - kSparkOffsetShift);
        spark.offset.x = static_cast<float>(Rand() % kSparkOffsetSpan - kSparkOffsetShift);
        spark.size = static_cast<float>(Rand() % kSparkSizeSpan) * kSparkSizeStep + kSparkSizeBase;
        spark.rise = static_cast<float>(Rand() % kSparkRiseSpan) * kSparkRiseStep + kSparkRiseBase;
        sparks_.push_back(spark);
      }
    }
    ++sowing_age_;
  }

  // LE CARRÉ D'UNE ÉTINCELLE : quatre points à son rayon, tournés de son angle,
  // basculés vers la caméra puis tournés de son lacet, et son décalage.
  [[nodiscard]] static QuadParticle SparkQuad(const Spark& spark, const EffectClassView& view) {
    const auto tilt = (kSparkTiltBase - static_cast<std::int32_t>(view.pitch_degrees)) %
                      static_cast<std::int32_t>(kFullTurnDegrees);
    const std::int32_t turn =
        static_cast<std::int32_t>(kFullTurnDegrees) - static_cast<std::int32_t>(view.yaw_degrees);
    std::array<glm::vec3, 4> points{};
    for (std::int32_t corner = 0; corner < 4; ++corner) {
      const std::int32_t degrees =
          (spark.turn + kSquareQuarter * corner) % static_cast<std::int32_t>(kFullTurnDegrees);
      glm::vec3 point{spark.size * EffectClassCosTable(degrees), 0.0F,
                      spark.size * EffectClassSinTable(degrees)};
      const float z = point.z * EffectClassCosTable(tilt) - point.y * EffectClassSinTable(tilt);
      point.y = point.y * EffectClassCosTable(tilt) + point.z * EffectClassSinTable(tilt);
      point.z = z;
      const float x = point.x * EffectClassCosTable(turn) - point.z * EffectClassSinTable(turn);
      point.z = point.z * EffectClassCosTable(turn) + point.x * EffectClassSinTable(turn);
      point.x = x;
      points[static_cast<std::size_t>(corner)] = point;
    }
    QuadParticle quad = spark.quad;
    quad.corners = {spark.offset + points[3], spark.offset + points[0], spark.offset + points[2],
                    spark.offset + points[1]};
    return quad;
  }

  std::vector<RingParticle> rings_;
  std::vector<Spark> sparks_;
  RibbonCorners previous_;
  std::int32_t fade_start_ = -1;
  std::int32_t sowing_age_ = 0;
};

}  // namespace

std::unique_ptr<EffectClassInstance> MakeRotateLine(const EffectClassBirth& birth,
                                                    const EffectClassView& /*view*/) {
  return std::make_unique<RotateLine>(birth);
}

std::unique_ptr<EffectClassInstance> MakeStarEmperor(const EffectClassBirth& birth,
                                                     const EffectClassView& /*view*/) {
  return std::make_unique<StarEmperor>(birth);
}

std::unique_ptr<EffectClassInstance> MakeLeashRings(const EffectClassBirth& birth,
                                                    const EffectClassView& /*view*/) {
  return std::make_unique<LeashRings>(birth);
}

}  // namespace ro::render::effect_class
