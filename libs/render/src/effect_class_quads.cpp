// LES CLASSES QUI PEIGNENT DES QUADS : le chant de Frigg, le cercle du Bind
// Trap, la boule de plasma de Creating Star, les kunaïs qui tournent.

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "ro/render/effect_duration.h"

#include "effect_class_internal.h"

namespace ro::render::effect_class {
namespace {

// ── Le chant de Frigg (1059) ────────────────────────────────────────────────
//
// UN CYLINDRE DE VINGT SEGMENTS, rayon deux et demi, hauteur cent vingt, bleu
// teinté de vert : vingt-deux particules posées sur un cercle, chaque segment
// tendu entre la précédente et la sienne. La première et la dernière gardent
// leur carré de repos, et se peignent quand même.
//
// Il TOURNE d'un degré par tic, et sa hauteur suit l'âge : aplati pendant vingt
// tics, il se lève selon le sinus de l'âge, puis s'affaisse d'un demi plus un
// centième de sa hauteur par tic passé cent quarante.
constexpr std::string_view kFriggTexture = "effect\\ring_blue.tga";
constexpr std::string_view kFriggSound = "effect\\wm_frigg_song.wav";
constexpr std::int32_t kFriggParticles = 22;
constexpr std::int32_t kFriggSegments = 20;
constexpr std::int32_t kFriggLastSegment = 20;
constexpr std::array<std::int32_t, 3> kFriggColor = {50, 200, 50};
constexpr std::int32_t kFriggAlpha = 50;
constexpr float kFriggHeight = 120.0F;
constexpr std::int32_t kFriggElevationDegrees = 90;
constexpr float kFriggRadius = 2.5F;
constexpr std::int32_t kFriggFlatTicks = 20;
constexpr std::int32_t kFriggRisingUntil = 140;
constexpr float kFriggSinkStep = 0.5F;
constexpr float kFriggSinkRatio = 0.01F;
constexpr float kFriggHeightPercent = 100.0F;
constexpr float kFriggHeightBoost = 1.1F;
constexpr float kFriggSegmentUv = 21.0F;

class FriggSong final : public EffectClassInstance {
 public:
  explicit FriggSong(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    duration_ticks_ = EffectDurationTicks(effect_id_);
    QueueSound(kFriggSound, 0);
    const auto step = static_cast<std::int32_t>(kFullTurnDegrees / kFriggSegments);
    glm::vec3 inner_previous{0.0F};
    glm::vec3 outer_previous{0.0F};
    for (std::int32_t index = 0; index < kFriggParticles; ++index) {
      Particle particle;
      particle.index = index;
      std::int32_t angle = (index * step) % static_cast<std::int32_t>(kFullTurnDegrees);
      if (index == kFriggLastSegment) {
        angle = 0;
      }
      const float cosinus = EffectClassCosTable(angle);
      const float sinus = EffectClassSinTable(angle);
      const float high = kFriggHeight * EffectClassCosTable(kFriggElevationDegrees);
      const float up = kFriggHeight * EffectClassSinTable(kFriggElevationDegrees);
      const glm::vec3 inner{kFriggRadius * cosinus, 0.0F, kFriggRadius * sinus};
      const glm::vec3 outer{inner.x + high * cosinus, 0.0F - up, inner.z + high * sinus};
      if (index >= 1 && index <= kFriggLastSegment) {
        particle.quad.corners = {outer_previous, outer, inner_previous, inner};
      }
      inner_previous = inner;
      outer_previous = outer;
      particle.quad.texture = kFriggTexture;
      particle.quad.color = kFriggColor;
      particle.quad.alpha = kFriggAlpha;
      particles_.push_back(particle);
    }
  }

  void Draw(const EffectClassView& view, EffectClassOutput& output) const override {
    for (const Particle& particle : particles_) {
      const QuadUv uv{0.0F, 1.0F, static_cast<float>(particle.index - 1) / kFriggSegmentUv,
                      static_cast<float>(particle.index) / kFriggSegmentUv};
      SubmitQuad(particle.quad, uv, false, view, output);
    }
  }

 protected:
  void Update(const EffectClassTick& /*tick*/, EffectClassOutput& /*output*/) override {
    for (Particle& particle : particles_) {
      particle.quad.position = position_;
      float height = 1.0F;
      if (particle.age >= kFriggFlatTicks) {
        if (particle.age > kFriggRisingUntil) {
          particle.sink =
              std::max(0.0F, particle.sink - kFriggSinkStep - particle.sink * kFriggSinkRatio);
          height = particle.sink;
        } else {
          height =
              std::max(0.0F, EffectClassSinTable(particle.age - kFriggFlatTicks) * particle.sink);
        }
      }
      // La rotation, puis la hauteur : l'âge retranché du cap, en degrés.
      NativeMatrix matrix = kNativeIdentity;
      MultiplyRotationY(matrix, Radians(static_cast<float>(-particle.age)));
      matrix = Multiply(matrix,
                        NativeScale(1.0F, height / kFriggHeightPercent * kFriggHeightBoost, 1.0F));
      particle.quad.matrix = matrix;
      ++particle.age;
    }
  }

 private:
  struct Particle {
    QuadParticle quad;
    std::int32_t index = 0;
    std::int32_t age = 0;
    // La hauteur de référence : cent vingt, puis elle s'affaisse.
    float sink = kFriggHeight;
  };
  std::vector<Particle> particles_;
};

// ── Le cercle du Bind Trap (1069, 1070, 1071) ───────────────────────────────
//
// UN CARRÉ COUCHÉ AU SOL, d'orientation fixe dans le monde, une unité au-dessus
// du terrain : côté dix-huit, trente ou quarante-deux selon l'identifiant, que
// le site choisit par le niveau. Additif, blanc, opaque.
constexpr std::string_view kBindTrapTexture = "effect\\hero_011.tga";
constexpr std::uint32_t kBindTrapFirstId = 1069;
constexpr float kBindTrapHalfSideStep = 6.0F;
constexpr float kBindTrapHalfSideBase = 9.0F;
constexpr float kQuarterTurnDegrees = 90.0F;

class BindTrap final : public EffectClassInstance {
 public:
  explicit BindTrap(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    half_side_ = static_cast<float>(effect_id_ - kBindTrapFirstId) * kBindTrapHalfSideStep +
                 kBindTrapHalfSideBase;
    quad_.texture = kBindTrapTexture;
  }

  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present;
  }

  void Draw(const EffectClassView& view, EffectClassOutput& output) const override {
    // Le lacet de la caméra que la mise à jour retranche, le dessin le rajoute :
    // le carré garde son orientation dans le monde.
    QuadParticle layer = quad_;
    layer.matrix = NativeScale(half_side_, half_side_, 1.0F);
    MultiplyRotationX(layer.matrix, Radians(kQuarterTurnDegrees));
    MultiplyTranslation(layer.matrix, position_);
    SubmitQuad(layer, QuadUv{}, true, view, output);
  }

 protected:
  void Update(const EffectClassTick& /*tick*/, EffectClassOutput& /*output*/) override {
    quad_.position = position_;
  }

 private:
  QuadParticle quad_;
  float half_side_ = kBindTrapHalfSideBase;
};

// ── La boule de plasma de Creating Star (1200) ──────────────────────────────
//
// UN PANNEAU ADDITIF DE QUINZE SUR QUINZE, face à la caméra, cinq unités
// au-dessus de l'unité. Invisible dix tics, il change ensuite de texture à
// chaque tic sur un cycle de cinq, sans fin, par-dessus tout.
constexpr std::array<std::string_view, 5> kPlasmaTextures = {
    "effect\\thunder_plazma_blast_a.bmp", "effect\\thunder_plazma_blast_b.bmp",
    "effect\\thunder_ball_d.bmp", "effect\\thunder_ball_e.bmp", "effect\\thunder_ball_f.bmp"};
constexpr std::int32_t kPlasmaWaitTicks = 10;
constexpr float kPlasmaHalfSide = 7.5F;
constexpr float kPlasmaLift = 5.0F;
constexpr std::uint32_t kPlasmaRenderWord = 515;

class JupitelThunderHit final : public EffectClassInstance {
 public:
  explicit JupitelThunderHit(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    quad_.render_word = kPlasmaRenderWord;
    quad_.texture = kPlasmaTextures[0];
  }

  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present;
  }

  void Draw(const EffectClassView& view, EffectClassOutput& output) const override {
    if (particle_age_ < 0) {
      return;
    }
    QuadParticle layer = quad_;
    layer.texture = kPlasmaTextures[texture_];
    layer.matrix = NativeScale(kPlasmaHalfSide, kPlasmaHalfSide, 1.0F);
    MultiplyRotationX(layer.matrix, Radians(view.pitch_degrees));
    SubmitQuad(layer, QuadUv{}, false, view, output);
  }

 protected:
  void Update(const EffectClassTick& /*tick*/, EffectClassOutput& /*output*/) override {
    ++particle_age_;
    if (particle_age_ < 0) {
      return;
    }
    texture_ = (texture_ + 1) % kPlasmaTextures.size();
    quad_.position = position_ - glm::vec3{0.0F, kPlasmaLift, 0.0F};
  }

 private:
  QuadParticle quad_;
  std::int32_t particle_age_ = -kPlasmaWaitTicks;
  std::size_t texture_ = 0;
};

// ── Les kunaïs qui tournent (984) ────────────────────────────────────────────
//
// QUATRE KUNAÏS en orbite de treize unités, huit au-dessus du porteur, qui
// avancent de dix-huit degrés par tic : le premier opaque, les trois suivants
// en traînée d'alpha dégressif, chacun cinq degrés derrière le précédent. Figés
// en carrés au porteur pendant cinq tics. L'orbite est prise dans le repère de
// la caméra : elle tourne avec elle.
//
// ⚠ UN TIC SUR VINGT LES TRAÎNÉES SAUTENT : quand la tête passe à zéro, leurs
// angles deviennent négatifs et se lisent dans les tables voisines.
constexpr std::string_view kKunaiTexture = "유저인터페이스\\item\\쿠나이_독.bmp";
constexpr std::int32_t kKunaiCount = 4;
constexpr std::int32_t kKunaiStillTicks = 5;
constexpr float kKunaiScale = 2.5F;
constexpr std::array<std::int32_t, 3> kKunaiColor = {255, 200, 255};
constexpr float kKunaiStartDegrees = 360.0F;
constexpr float kKunaiSpeed = 0.4F;
constexpr float kKunaiSpeedFactor = 45.0F;
constexpr float kKunaiRadius = 13.0F;
constexpr float kKunaiHeight = -8.0F;
constexpr float kKunaiTrailDegrees = 5.0F;
constexpr float kKunaiHeadingOffsetDegrees = 45.0F;
constexpr std::int32_t kKunaiFirstTrailFade = 150;
constexpr std::int32_t kKunaiTrailFade = 25;

class RotateItem final : public EffectClassInstance {
 public:
  explicit RotateItem(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    for (std::int32_t index = 0; index < kKunaiCount; ++index) {
      Kunai kunai;
      kunai.index = index;
      kunai.quad.texture = kKunaiTexture;
      kunai.quad.color = kKunaiColor;
      kunai.quad.blend_destination = kBlendInverseSourceAlpha;
      kunais_.push_back(kunai);
    }
  }

  void Draw(const EffectClassView& view, EffectClassOutput& output) const override {
    for (const Kunai& kunai : kunais_) {
      QuadParticle layer = kunai.quad;
      if (kunai.turning && layer.alpha > 0) {
        layer.matrix = NativeScale(kKunaiScale, kKunaiScale, 1.0F);
        MultiplyRotationX(layer.matrix, Radians(view.pitch_degrees));
        MultiplyRotationY(
            layer.matrix,
            Radians(static_cast<float>(
                static_cast<std::int32_t>(kunai.heading + kKunaiHeadingOffsetDegrees) %
                static_cast<std::int32_t>(kFullTurnDegrees))));
        MultiplyTranslation(layer.matrix, kunai.orbit);
      }
      SubmitQuad(layer, QuadUv{}, false, view, output);
    }
  }

 protected:
  void Update(const EffectClassTick& /*tick*/, EffectClassOutput& /*output*/) override {
    std::int32_t previous_alpha = 0;
    float previous_heading = 0.0F;
    for (Kunai& kunai : kunais_) {
      kunai.quad.position = position_;
      if (age_ < kKunaiStillTicks) {
        continue;
      }
      float angle = 0.0F;
      if (kunai.index == 0) {
        kunai.heading = static_cast<float>(
            static_cast<std::int32_t>(kKunaiSpeed * kKunaiSpeedFactor + kunai.heading) %
            static_cast<std::int32_t>(kFullTurnDegrees));
        angle = kunai.heading;
      } else {
        previous_alpha -= kunai.index == 1 ? kKunaiFirstTrailFade : kKunaiTrailFade;
        kunai.quad.alpha = previous_alpha;
        kunai.heading = previous_heading;
        angle = previous_heading;
      }
      const auto degrees = static_cast<std::int32_t>(angle);
      kunai.orbit = {kKunaiRadius * EffectClassCosTable(degrees), kKunaiHeight,
                     kKunaiRadius * EffectClassSinTable(degrees)};
      previous_heading =
          static_cast<float>(static_cast<std::int32_t>(kunai.heading - kKunaiTrailDegrees) %
                             static_cast<std::int32_t>(kFullTurnDegrees));
      previous_alpha = kunai.quad.alpha;
      kunai.turning = true;
    }
  }

 private:
  struct Kunai {
    QuadParticle quad;
    std::int32_t index = 0;
    float heading = kKunaiStartDegrees;
    glm::vec3 orbit{0.0F};
    bool turning = false;
  };
  std::vector<Kunai> kunais_;
};

}  // namespace

std::unique_ptr<EffectClassInstance> MakeFriggSong(const EffectClassBirth& birth,
                                                   const EffectClassView& /*view*/) {
  return std::make_unique<FriggSong>(birth);
}

std::unique_ptr<EffectClassInstance> MakeBindTrap(const EffectClassBirth& birth,
                                                  const EffectClassView& /*view*/) {
  return std::make_unique<BindTrap>(birth);
}

std::unique_ptr<EffectClassInstance> MakeJupitelThunderHit(const EffectClassBirth& birth,
                                                           const EffectClassView& /*view*/) {
  return std::make_unique<JupitelThunderHit>(birth);
}

std::unique_ptr<EffectClassInstance> MakeRotateItem(const EffectClassBirth& birth,
                                                    const EffectClassView& /*view*/) {
  return std::make_unique<RotateItem>(birth);
}

}  // namespace ro::render::effect_class
