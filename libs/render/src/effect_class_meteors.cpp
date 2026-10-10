// LES MÉTÉORES DES SUMMONERS : les carottes de Lunatic Carrot Beat (1099, 1111)
// et la boule de Catnip Meteor (1106, 1112). Des sprites qui jouent leur chute
// une fois, là où la compétence tombe, et secouent l'écran à leur impact.

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "ro/render/str_effect_setup.h"

#include "effect_class_internal.h"

namespace ro::render::effect_class {
namespace {

constexpr std::uint32_t kCarrotBeatId = 1099;
constexpr std::uint32_t kCatnipMeteorId = 1106;
constexpr std::uint32_t kCarrotBeatBrightId = 1111;
constexpr std::string_view kCarrotSprite = "이팩트\\su_lunaticcarrotbeat\\su_lunaticcarrotbeat";
constexpr std::string_view kCatnipSprite = "이팩트\\su_cn_meteor\\su_cn_meteor";

// LE NOMBRE DE CAROTTES PAR NIVEAU, de un à cinq ; au-delà du cinquième, et pour
// un niveau inconnu, celui du cinquième.
//
// ⚠ UN NIVEAU NUL LIT HORS DE SA TABLE chez le client, et ce qu'il y trouve
// n'est pas une mesure : nous ne semons rien.
constexpr std::array<std::int32_t, 6> kCarrotsByLevel = {0, 3, 3, 5, 5, 7};
constexpr std::uint32_t kHighestCarrotLevel = 5;
// L'ÉTENDUE DU SEMIS, en unités par carotte, centrée sur le point de chute.
constexpr std::int32_t kSpreadPerCarrot = 5;
// Une image tous les quatre tics, l'action jouée une fois.
constexpr float kMeteorFramePeriod = 4.0F;
// Les deux variantes sombres se peignent à cent d'alpha, les claires pleines.
constexpr std::int32_t kDimMeteorAlpha = 100;
constexpr std::uint32_t kMeteorRenderWord = 1;
// L'IMAGE DE L'IMPACT : chaque tic où un météore la montre secoue l'écran.
constexpr std::int32_t kImpactFrame = 12;

[[nodiscard]] bool IsCarrotBeat(std::uint32_t effect_id) {
  return effect_id == kCarrotBeatId || effect_id == kCarrotBeatBrightId;
}

[[nodiscard]] bool IsDim(std::uint32_t effect_id) {
  return effect_id == kCarrotBeatId || effect_id == kCatnipMeteorId;
}

// Le nombre de carottes pour le niveau reçu — le niveau se lit NON SIGNÉ, si
// bien qu'un niveau négatif, celui d'une naissance sans paramètre, vaut le
// cinquième.
[[nodiscard]] std::int32_t CarrotsFor(std::int32_t level) {
  const std::uint32_t clamped = std::min(static_cast<std::uint32_t>(level), kHighestCarrotLevel);
  return kCarrotsByLevel[clamped];
}

class Meteor final : public EffectClassInstance {
 public:
  explicit Meteor(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    if (IsCarrotBeat(effect_id_)) {
      const std::int32_t carrots = CarrotsFor(level_);
      const std::int32_t spread = kSpreadPerCarrot * carrots;
      const float half_spread = static_cast<float>(spread) * 0.5F;
      for (std::int32_t rank = 0; rank < carrots; ++rank) {
        glm::vec3 spot = position_;
        spot.x += static_cast<float>(Rand() % (spread + 1)) - half_spread;
        spot.z += static_cast<float>(Rand() % (spread + 1)) - half_spread;
        meteors_.push_back(MeteorAt(kCarrotSprite, spot));
      }
    } else {
      meteors_.push_back(MeteorAt(kCatnipSprite, position_));
    }
  }
  [[nodiscard]] bool Alive(const EffectClassTick& /*tick*/) const override {
    return !meteors_.empty();
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    for (const SpriteParticle& meteor : meteors_) {
      DrawSprite(meteor, output);
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& output) override {
    // Un météore qui a fini sa chute s'en va au tic qui suit.
    std::erase_if(meteors_, [](const SpriteParticle& meteor) { return meteor.spent; });
    for (SpriteParticle& meteor : meteors_) {
      AdvanceSpriteFrame(meteor, tick);
      if (meteor.frame == kImpactFrame) {
        output.quake_ms = std::max(output.quake_ms, kEffectQueuedQuakeMs);
      }
      ++meteor.age;
    }
  }

 private:
  [[nodiscard]] SpriteParticle MeteorAt(std::string_view sprite, glm::vec3 spot) const {
    SpriteParticle meteor;
    meteor.sprite = sprite;
    meteor.position = spot;
    meteor.period = kMeteorFramePeriod;
    meteor.play = SpritePlay::kOnce;
    meteor.render_word = kMeteorRenderWord;
    meteor.keeps_layer_color = true;
    if (IsDim(effect_id_)) {
      meteor.alpha = kDimMeteorAlpha;
    }
    return meteor;
  }

  std::vector<SpriteParticle> meteors_;
};

}  // namespace

std::unique_ptr<EffectClassInstance> MakeMeteor(const EffectClassBirth& birth,
                                                const EffectClassView& /*view*/) {
  return std::make_unique<Meteor>(birth);
}

}  // namespace ro::render::effect_class
