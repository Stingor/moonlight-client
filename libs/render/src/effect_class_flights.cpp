// LES CLASSES QUI FONT VOLER UN `.str` : Triangle Shot, Cart Cannon, Axe
// Boomerang et Shield Shooting, les jets d'Acidified Zone.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "effect_class_internal.h"

namespace ro::render::effect_class {
namespace {

constexpr float kFlightLift = 10.0F;
constexpr float kShortFlight = 40.0F;
constexpr float kFlightHeadStart = 4.0F;

// UN TRAIT DROIT : du lanceur à la place de la cible au coup, relevés de dix,
// en `steps` pas — ou, sous quarante unités, à `40 / steps` unités par pas. Le
// `.str` boucle, invisible pendant le retard, tourné une fois à l'écran.
struct StraightFlightShape {
  std::string_view path;
  std::string_view minimal_path;
  float steps = 0.0F;
  // L'angle du fichier au repos : l'image pointe vers la droite (360) ou vers
  // le bas (270).
  float rest_degrees = 0.0F;
};

class StraightFlight final : public EffectClassInstance {
 public:
  StraightFlight(const EffectClassBirth& birth, const EffectClassView& view,
                 const StraightFlightShape& shape)
      : EffectClassInstance(birth), steps_(shape.steps), delay_ms_(birth.delay_ms) {
    host_.path = std::string(minimal_effects_ ? shape.minimal_path : shape.path);
    host_.allows_min_variant = false;
    host_.loops = true;
    glm::vec3 start = position_;
    start.y -= kFlightLift;
    const glm::vec3 target =
        bound_position_.value_or(position_) - glm::vec3{0.0F, kFlightLift, 0.0F};
    const glm::vec3 delta = target - start;
    const float length = std::sqrt(glm::dot(delta, delta));
    step_ = delta / steps_;
    if (length < kShortFlight && length > 0.0F) {
      reach_ = kShortFlight / length;
      step_ *= reach_;
    }
    if (length > 0.0F) {
      start += kFlightHeadStart * (delta / length);
    }
    origin_ = start;
    position_ = start;
    float angle =
        std::atan2(target.z - start.z, target.x - start.x) / kClientPi * kHalfTurnDegrees +
        view.yaw_degrees;
    if (angle < 0.0F) {
      angle += kFullTurnDegrees;
    }
    float spin = shape.rest_degrees - angle;
    if (spin < 0.0F) {
      spin += kFullTurnDegrees;
    }
    host_.spin_degrees = spin;
  }
  [[nodiscard]] bool Alive(const EffectClassTick& /*tick*/) const override {
    return static_cast<float>(step_count_) <= steps_ / reach_;
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    DrawEffectClassStrHost(host_, output);
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    if (delay_ms_ > 0) {
      delay_ms_ -= kTickMs;
      return;
    }
    position_ = origin_ + static_cast<float>(step_count_) * step_;
    host_.position = position_;
    static_cast<void>(AdvanceEffectClassStrHost(host_, tick));
    ++step_count_;
  }

 private:
  EffectClassStrHost host_;
  float steps_ = 1.0F;
  float reach_ = 1.0F;
  glm::vec3 step_{0.0F};
  glm::vec3 origin_{0.0F};
  std::int32_t step_count_ = 0;
  std::int32_t delay_ms_ = 0;
};

constexpr StraightFlightShape kTriangleShot = {
    "new_triangleshot\\new_triangleshot_arrow\\new_triangleshot_arrow.str",
    "mineffect\\new_triangleshot\\new_triangleshot_arrow\\new_triangleshot_arrow.str", 8.0F,
    360.0F};
constexpr StraightFlightShape kCartCannon = {
    "new_cart_cannon\\new_cart_cannon\\new_cart_cannon.str",
    "mineffect\\new_cart_cannon\\new_cart_cannon\\new_cart_cannon.str", 10.0F, 270.0F};

// ── Axe Boomerang (1657) et Shield Shooting (2183) ──────────────────────────
//
// UNE COURBE DE BÉZIER entre le lanceur et la cible, relus à chaque tic : à
// l'aller, le paramètre descend d'un vingtième par tic et le point de contrôle
// est à vingt unités de la cible, la direction tournée de cent degrés ; au
// retour, il remonte d'un cinquantième, contrôle à quarante unités, deux cent
// soixante degrés. L'instance meurt au retour, ou quand le lanceur part.
constexpr std::uint32_t kShieldShootingId = 2183;
constexpr float kOutwardStep = 0.05F;
constexpr float kReturnStep = 0.02F;
constexpr float kOutwardReach = 20.0F;
constexpr float kReturnReach = 40.0F;
constexpr float kOutwardRadians = 1.745329F;
constexpr float kReturnRadians = 4.5378551F;

class AxeBoomerang final : public EffectClassInstance {
 public:
  explicit AxeBoomerang(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    if (effect_id_ == kShieldShootingId) {
      host_.path = "shield_shooting\\shield_shooting\\shield_shooting.str";
    } else {
      host_.path = minimal_effects_
                       ? "mineffect\\new_axeboomerang\\new_axeboomerang\\new_axeboomerang.str"
                       : "new_axeboomerang\\new_axeboomerang\\new_axeboomerang.str";
      host_.allows_min_variant = false;
    }
    host_.loops = true;
  }
  [[nodiscard]] bool Alive(const EffectClassTick& /*tick*/) const override { return alive_; }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    DrawEffectClassStrHost(host_, output);
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    if (!tick.owner_present || !tick.owner_position.has_value()) {
      alive_ = false;
      return;
    }
    const glm::vec3 from = *tick.owner_position;
    if (tick.bound_position.has_value()) {
      target_ = *tick.bound_position;
    }
    float reach = kOutwardReach;
    float radians = kOutwardRadians;
    if (!returning_) {
      progress_ -= kOutwardStep;
      if (progress_ <= 0.0F) {
        returning_ = true;
      }
    } else {
      progress_ += kReturnStep;
      reach = kReturnReach;
      radians = kReturnRadians;
    }
    const glm::vec3 delta = target_ - from;
    const float length = std::sqrt(glm::dot(delta, delta));
    const glm::vec3 lever = length > 0.0F ? delta / length * reach : glm::vec3{0.0F};
    const float cosinus = std::cos(radians);
    const float sinus = std::sin(radians);
    const glm::vec3 control{target_.x + lever.x * cosinus + lever.z * sinus, target_.y + lever.y,
                            target_.z - lever.x * sinus + lever.z * cosinus};
    const float t = progress_;
    host_.position = std::pow(t, 2.0F) * from + 2.0F * t * (1.0F - t) * control +
                     std::pow(1.0F - t, 2.0F) * target_;
    if (progress_ >= 1.0F) {
      alive_ = false;
    }
    static_cast<void>(AdvanceEffectClassStrHost(host_, tick));
  }

 private:
  EffectClassStrHost host_;
  glm::vec3 target_{0.0F};
  float progress_ = 1.0F;
  bool returning_ = false;
  bool alive_ = true;
};

// ── Les jets d'Acidified Zone (2206, 2214, 2222, 2230) ───────────────────────
//
// UN JET BOUCLÉ qui court en quinze tics du lanceur — relevé de dix unités vers
// le haut de l'écran — à la cible, sur une courbe dont le contrôle est la cible
// plus l'écart tourné de cent vingt ou deux cent quarante degrés, selon que la
// cible est à gauche ou à droite à l'écran. À l'arrivée, deux sons à la cible,
// puis trois `.str` d'impact qui la suivent, un peu à gauche et quatorze plus
// haut.
constexpr float kJetStep = 0.07F;
constexpr float kLeftRadians = 2.0943947F;
constexpr float kRightRadians = 4.1887894F;
constexpr glm::vec3 kImpactOffset{-1.0F, -14.0F, 0.0F};
constexpr std::string_view kAcidifiedSound = "effect\\BO_ACIDIFIED_ZONE.wav";

struct AcidifiedElement {
  std::uint32_t effect_id;
  std::string_view jet;
  std::string_view burst;
  std::string_view bottom;
  std::string_view water;
  std::string_view sound;
};

constexpr std::array<AcidifiedElement, 4> kAcidifiedElements = {{
    {2206, "acidified_zone_fire\\acidified_zone_fire_c\\acidified_zone_fire_c.str",
     "acidified_zone_fire\\acidified_zone_fire_b\\acidified_zone_fire_b.str",
     "acidified_zone_fire\\acidified_zone_fire_b_bottom\\acidified_zone_fire_b_bottom.str",
     "acidified_zone_fire\\acidified_zone_fire_water\\acidified_zone_fire_water.str",
     "effect\\BO_ACIDIFIED_ZONE_FIRE.wav"},
    {2214, "acidified_zone_water\\acidified_zone_water_c\\acidified_zone_water_c.str",
     "acidified_zone_water\\acidified_zone_water_b\\acidified_zone_water_b.str",
     "acidified_zone_water\\acidified_zone_water_b_bottom\\acidified_zone_water_b_bottom.str",
     "acidified_zone_water\\acidified_zone_water_water\\acidified_zone_water_water.str",
     "effect\\BO_ACIDIFIED_ZONE_WATER.wav"},
    {2222, "acidified_zone_wind\\acidified_zone_wind_c\\acidified_zone_wind_c.str",
     "acidified_zone_wind\\acidified_zone_wind_b\\acidified_zone_wind_b.str",
     "acidified_zone_wind\\acidified_zone_wind_b_bottom\\acidified_zone_wind_b_bottom.str",
     "acidified_zone_wind\\acidified_zone_wind_water\\acidified_zone_wind_water.str",
     "effect\\BO_ACIDIFIED_ZONE_WIND.wav"},
    {2230, "acidified_zone_ground\\acidified_zone_ground_c\\acidified_zone_ground_c.str",
     "acidified_zone_ground\\acidified_zone_ground_b\\acidified_zone_ground_b.str",
     "acidified_zone_ground\\acidified_zone_ground_b_bottom\\acidified_zone_ground_b_bottom.str",
     "acidified_zone_ground\\acidified_zone_ground_water\\acidified_zone_ground_water.str",
     "effect\\BO_ACIDIFIED_ZONE_GROUND.wav"},
}};

class AcidifiedThrow final : public EffectClassInstance {
 public:
  explicit AcidifiedThrow(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    element_ = &kAcidifiedElements[0];
    for (const AcidifiedElement& element : kAcidifiedElements) {
      if (element.effect_id == effect_id_) {
        element_ = &element;
      }
    }
    jet_.path = std::string(element_->jet);
    jet_.loops = true;
    for (std::string_view path : {element_->burst, element_->bottom, element_->water}) {
      EffectClassStrHost host;
      host.path = std::string(path);
      host.behind_actors = path == element_->bottom;
      impacts_.push_back(host);
    }
  }
  [[nodiscard]] bool Alive(const EffectClassTick& /*tick*/) const override { return alive_; }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    if (flying_) {
      DrawEffectClassStrHost(jet_, output);
      return;
    }
    for (const EffectClassStrHost& host : impacts_) {
      DrawEffectClassStrHost(host, output);
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& output) override {
    if (!tick.owner_present || !tick.owner_position.has_value()) {
      alive_ = false;
      return;
    }
    if (tick.bound_position.has_value()) {
      target_ = *tick.bound_position;
    }
    if (flying_) {
      Fly(*tick.owner_position, tick, output);
      return;
    }
    std::erase_if(impacts_, [](const EffectClassStrHost& host) { return host.finished; });
    for (EffectClassStrHost& host : impacts_) {
      host.position = target_ + kImpactOffset;
      static_cast<void>(AdvanceEffectClassStrHost(host, tick));
    }
    alive_ = !impacts_.empty();
  }

 private:
  void Fly(glm::vec3 from, const EffectClassTick& tick, EffectClassOutput& output) {
    const EffectClassView& view = *tick.view;
    const glm::vec3 from_ours = EffectClassFromClient(from);
    const float side = glm::dot(EffectClassFromClient(target_) - from_ours, view.right);
    const float radians = side < 0.0F ? kLeftRadians : kRightRadians;
    const glm::vec3 start = EffectClassToClient(from_ours + kFlightLift * view.up);
    progress_ -= kJetStep;
    const glm::vec3 delta = target_ - start;
    const float cosinus = std::cos(radians);
    const float sinus = std::sin(radians);
    const glm::vec3 control = target_ + glm::vec3{delta.x * cosinus + delta.z * sinus, delta.y,
                                                  -delta.x * sinus + delta.z * cosinus};
    const float t = progress_;
    jet_.position = std::pow(t, 2.0F) * start + 2.0F * t * (1.0F - t) * control +
                    std::pow(1.0F - t, 2.0F) * target_;
    static_cast<void>(AdvanceEffectClassStrHost(jet_, tick));
    if (t <= 0.0F) {
      flying_ = false;
      output.sounds.push_back(EffectClassSound{kAcidifiedSound, OursOf(target_)});
      output.sounds.push_back(EffectClassSound{element_->sound, OursOf(target_)});
    }
  }

  const AcidifiedElement* element_ = nullptr;
  EffectClassStrHost jet_;
  std::vector<EffectClassStrHost> impacts_;
  glm::vec3 target_{0.0F};
  float progress_ = 1.0F;
  bool flying_ = true;
  bool alive_ = true;
};

}  // namespace

std::unique_ptr<EffectClassInstance> MakeTriangleShot(const EffectClassBirth& birth,
                                                      const EffectClassView& view) {
  return std::make_unique<StraightFlight>(birth, view, kTriangleShot);
}

std::unique_ptr<EffectClassInstance> MakeCartCannon(const EffectClassBirth& birth,
                                                    const EffectClassView& view) {
  return std::make_unique<StraightFlight>(birth, view, kCartCannon);
}

std::unique_ptr<EffectClassInstance> MakeAxeBoomerang(const EffectClassBirth& birth,
                                                      const EffectClassView& /*view*/) {
  return std::make_unique<AxeBoomerang>(birth);
}

std::unique_ptr<EffectClassInstance> MakeAcidifiedThrow(const EffectClassBirth& birth,
                                                        const EffectClassView& /*view*/) {
  return std::make_unique<AcidifiedThrow>(birth);
}

}  // namespace ro::render::effect_class
