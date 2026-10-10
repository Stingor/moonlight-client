// LA CANNE DE L'ŒIL MALÉFIQUE (2307) : huit yeux autour de l'unité, et des
// rayons du centre vers chacun, relancés chaque seconde.

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include "effect_class_internal.h"

namespace ro::render::effect_class {
namespace {

// LA PHASE d'un œil ou d'un rayon : il naît, il dure, il s'éteint.
enum class Phase : std::uint8_t { kStart, kLoop, kEnd };

constexpr std::array<std::string_view, 3> kEyePaths = {
    "NPC_CANE_OF_EVIL_EYE\\npc_cane_of_evil_eye\\npc_cane_of_evil_eye_start.str",
    "NPC_CANE_OF_EVIL_EYE\\npc_cane_of_evil_eye\\npc_cane_of_evil_eye.str",
    "NPC_CANE_OF_EVIL_EYE\\npc_cane_of_evil_eye\\npc_cane_of_evil_eye_end.str"};
constexpr std::array<std::string_view, 3> kLightPaths = {
    "NPC_CANE_OF_EVIL_EYE\\npc_cane_of_evil_eye_light\\npc_cane_of_evil_eye_light_03c_start.str",
    "NPC_CANE_OF_EVIL_EYE\\npc_cane_of_evil_eye_light_loop\\npc_cane_of_evil_eye_light_03c_loop."
    "str",
    "NPC_CANE_OF_EVIL_EYE\\npc_cane_of_evil_eye_light\\npc_cane_of_evil_eye_light_03c_end.str"};

constexpr float kEyeSpacing = 20.0F;
constexpr float kEyeHeight = -12.0F;
constexpr float kLightEndLift = 3.5F;
constexpr std::uint32_t kVolleyPeriodMs = 1000;
constexpr float kLightStretch = 0.0044F;

struct Piece {
  EffectClassStrHost host;
  Phase phase = Phase::kStart;
  glm::vec3 offset{0.0F};
};

// UN ŒIL : son `.str` de la phase, qui boucle au milieu seulement.
Piece Eye(Phase phase, glm::vec3 offset) {
  Piece eye;
  eye.phase = phase;
  eye.offset = offset;
  eye.host.path = std::string(kEyePaths[static_cast<std::size_t>(phase)]);
  eye.host.allows_min_variant = false;
  eye.host.loops = phase == Phase::kLoop;
  return eye;
}

// UN RAYON vers un œil : son extrémité est l'œil relevé de trois et demi.
Piece Light(Phase phase, glm::vec3 offset) {
  Piece light;
  light.phase = phase;
  light.offset = offset;
  light.host.path = std::string(kLightPaths[static_cast<std::size_t>(phase)]);
  light.host.allows_min_variant = false;
  return light;
}

class CaneOfEvilEye final : public EffectClassInstance {
 public:
  explicit CaneOfEvilEye(const EffectClassBirth& birth)
      : EffectClassInstance(birth), volley_ms_(birth.now_ms) {
    reused_on_same_owner_ = true;
    for (const glm::vec2 cell :
         {glm::vec2{-1, -1}, glm::vec2{0, -1}, glm::vec2{1, -1}, glm::vec2{-1, 0}, glm::vec2{1, 0},
          glm::vec2{-1, 1}, glm::vec2{0, 1}, glm::vec2{1, 1}}) {
      eyes_.push_back(Eye(Phase::kStart, {cell.x * kEyeSpacing, kEyeHeight, cell.y * kEyeSpacing}));
    }
    SpawnLights();
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present && (!eyes_.empty() || !lights_.empty());
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    for (const Piece& eye : eyes_) {
      DrawEffectClassStrHost(eye.host, output);
    }
    for (const Piece& light : lights_) {
      DrawEffectClassStrHost(light.host, output);
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    UpdateEyes(tick);
    if (volley_ms_ + kVolleyPeriodMs <= tick.now_ms) {
      SpawnLights();
      volley_ms_ += kVolleyPeriodMs;
    }
    UpdateLights(tick);
  }

 private:
  void SpawnLights() {
    for (const Piece& eye : eyes_) {
      lights_.push_back(Light(Phase::kStart, eye.offset + glm::vec3{0.0F, kLightEndLift, 0.0F}));
    }
  }

  void UpdateEyes(const EffectClassTick& tick) {
    std::vector<Piece> kept;
    for (Piece& eye : eyes_) {
      if (!eye.host.finished) {
        kept.push_back(eye);
      } else if (eye.phase == Phase::kStart) {
        kept.push_back(Eye(Phase::kLoop, eye.offset));
      }
    }
    eyes_ = std::move(kept);
    for (Piece& eye : eyes_) {
      eye.host.position = position_ + eye.offset;
      static_cast<void>(AdvanceEffectClassStrHost(eye.host, tick));
    }
  }

  void UpdateLights(const EffectClassTick& tick) {
    std::vector<Piece> kept;
    for (Piece& light : lights_) {
      if (!light.host.finished) {
        kept.push_back(light);
      } else if (light.phase != Phase::kEnd) {
        kept.push_back(Light(Phase::kEnd, light.offset));
      }
    }
    lights_ = std::move(kept);
    for (Piece& light : lights_) {
      // L'ANGLE ET LA LONGUEUR À L'ÉCRAN, de l'extrémité vers le centre : le
      // rayon, posé au centre, se tourne vers l'œil et s'étire de la distance.
      const glm::vec3 end = position_ + light.offset;
      if (tick.view != nullptr) {
        light.host.spin_degrees =
            ScreenAngleDegrees(*tick.view, end, position_).value_or(light.host.spin_degrees);
        light.host.scale = ScreenLength(*tick.view, end, position_) * kLightStretch;
      }
      light.host.position = position_;
      static_cast<void>(AdvanceEffectClassStrHost(light.host, tick));
    }
  }

  [[nodiscard]] static float ScreenLength(const EffectClassView& view, glm::vec3 from,
                                          glm::vec3 to) {
    if (!view.project) {
      return 0.0F;
    }
    const std::optional<glm::vec2> a = view.project(EffectClassFromClient(from));
    const std::optional<glm::vec2> b = view.project(EffectClassFromClient(to));
    return a.has_value() && b.has_value() ? glm::length(*b - *a) : 0.0F;
  }

  std::uint32_t volley_ms_ = 0;
  std::vector<Piece> eyes_;
  std::vector<Piece> lights_;
};

}  // namespace

std::unique_ptr<EffectClassInstance> MakeCaneOfEvilEye(const EffectClassBirth& birth,
                                                       const EffectClassView& /*view*/) {
  return std::make_unique<CaneOfEvilEye>(birth);
}

}  // namespace ro::render::effect_class
