// LES SPHÈRES QUE DEUX QUATRIÈMES MÉTIERS MONTRENT À LA PLACE DE CELLES DU MONK :
// les armes de servant du Dragon Knight (1694) et les sphères d'ombre de
// l'Abyss Chaser (2109). Le paquet des sphères en donne le NOMBRE ; l'instance
// en fait naître ou partir une à une, chacune par son `.str` d'apparition, de
// boucle et de départ.

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "effect_class_internal.h"

namespace ro::render::effect_class {
namespace {

EffectClassStrHost Host(std::string_view path, bool loops, float scale) {
  EffectClassStrHost host;
  host.path = std::string(path);
  host.loops = loops;
  host.scale = scale;
  return host;
}

// ── Les armes de servant (1694) ─────────────────────────────────────────────
//
// CINQ ARMES AU PLUS, posées en étoile autour du porteur : la n-ième à 90 + 72·n
// degrés, à treize unités de lui et dix au-dessus, sans tourner. Chacune naît
// par son `.str` d'apparition puis boucle ; celle qui part est la DERNIÈRE
// venue, qui joue son tir à sa place puis s'efface. L'une et l'autre sonnent.
constexpr std::string_view kServantCast =
    "DRAGON_KNIGHT\\DK_SERVANTWEAPON\\servantweapon_cast\\servantweapon_cast.str";
constexpr std::string_view kServantLoop =
    "DRAGON_KNIGHT\\DK_SERVANTWEAPON\\servantweapon\\servantweapon.str";
constexpr std::string_view kServantShoot =
    "DRAGON_KNIGHT\\DK_SERVANTWEAPON\\servantweapon_shoot\\servantweapon_shoot.str";
constexpr std::string_view kServantSound = "effect\\DK_SERVANTWEAPON.wav";
constexpr std::size_t kMostServantWeapons = 5;
constexpr float kServantFirstDegrees = 90.0F;
constexpr float kServantStepDegrees = 72.0F;
constexpr float kServantRadius = 13.0F;
constexpr float kServantHeight = 10.0F;
constexpr float kServantScale = 0.12F;

class ServantWeapons final : public EffectClassInstance {
 public:
  explicit ServantWeapons(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    reused_on_same_owner_ = true;
    Retell(level_);
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present;
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    for (const Weapon& weapon : held_) {
      DrawEffectClassStrHost(weapon.host, output);
    }
    for (const Weapon& weapon : leaving_) {
      DrawEffectClassStrHost(weapon.host, output);
    }
  }
  void Retell(std::int32_t level) override {
    const std::size_t wanted = level > 0 ? static_cast<std::size_t>(level) : 0;
    while (held_.size() < wanted && held_.size() < kMostServantWeapons) {
      float degrees = static_cast<float>(held_.size()) * kServantStepDegrees + kServantFirstDegrees;
      if (degrees > kFullTurnDegrees) {
        degrees -= kFullTurnDegrees;
      }
      held_.push_back(Weapon{Host(kServantCast, false, kServantScale), degrees});
      QueueSound(kServantSound, age_);
    }
    while (held_.size() > wanted) {
      leaving_.push_back(Weapon{Host(kServantShoot, false, kServantScale), held_.back().degrees});
      held_.pop_back();
      QueueSound(kServantSound, age_);
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    for (Weapon& weapon : held_) {
      // L'APPARITION FINIE, la boucle prend sa place, au même angle.
      if (!AdvanceEffectClassStrHost(weapon.host, tick) && !weapon.host.loops) {
        weapon.host = Host(kServantLoop, true, kServantScale);
      }
      weapon.host.position = Around(weapon.degrees);
    }
    std::erase_if(leaving_, [&tick](Weapon& weapon) {
      return !AdvanceEffectClassStrHost(weapon.host, tick);
    });
    for (Weapon& weapon : leaving_) {
      weapon.host.position = Around(weapon.degrees);
    }
  }

 private:
  struct Weapon {
    EffectClassStrHost host;
    float degrees = 0.0F;
  };

  [[nodiscard]] glm::vec3 Around(float degrees) const {
    const float radians = Radians(degrees);
    return position_ + glm::vec3{std::cos(radians) * kServantRadius, -kServantHeight,
                                 std::sin(radians) * kServantRadius};
  }

  std::vector<Weapon> held_;
  std::vector<Weapon> leaving_;
};

// ── Les sphères d'ombre (2109) ──────────────────────────────────────────────
//
// CINQ PLACES AU PLUS, chacune à son écart DANS LE PLAN DE LA VUE autour du
// porteur et à sa taille. Une sphère qui vient joue son apparition puis boucle ;
// une qui part joue sa disparition à sa place puis s'efface. Un nombre au-delà
// de cinq n'est pas tenu.
constexpr std::string_view kShadowCreate =
    "from_the_abyss\\from_the_abyss_shadowball_create\\from_the_abyss_shadowball_create.str";
constexpr std::string_view kShadowLoop =
    "from_the_abyss\\from_the_abyss_shadowball\\from_the_abyss_shadowball.str";
constexpr std::string_view kShadowDie =
    "from_the_abyss\\from_the_abyss_shadowball_die\\from_the_abyss_shadowball_die.str";

struct ShadowPlace {
  float scale = 1.0F;
  glm::vec3 offset{0.0F, 0.0F, 0.0F};
};
constexpr std::array<ShadowPlace, 5> kShadowPlaces = {{
    {0.8F, {-4.8F, -9.1F, 0.0F}},
    {0.6F, {-3.4F, -12.8F, 0.0F}},
    {0.5F, {-0.7F, -14.0F, 0.0F}},
    {0.7F, {2.7F, -14.0F, 0.0F}},
    {0.4F, {4.3F, -10.0F, 0.0F}},
}};

class ShadowSpheres final : public EffectClassInstance {
 public:
  explicit ShadowSpheres(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    reused_on_same_owner_ = true;
    Retell(level_);
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present;
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    for (const Sphere& sphere : spheres_) {
      if (sphere.stage != Stage::kNone) {
        DrawEffectClassStrHost(sphere.host, output);
      }
    }
  }
  void Retell(std::int32_t level) override {
    if (level < 0 || static_cast<std::size_t>(level) > kShadowPlaces.size()) {
      return;
    }
    const auto wanted = static_cast<std::size_t>(level);
    for (std::size_t place = shown_; place < wanted; ++place) {
      spheres_[place] =
          Sphere{Host(kShadowCreate, false, kShadowPlaces[place].scale), Stage::kCreating};
    }
    for (std::size_t place = wanted; place < shown_; ++place) {
      if (spheres_[place].stage != Stage::kNone) {
        spheres_[place] =
            Sphere{Host(kShadowDie, false, kShadowPlaces[place].scale), Stage::kDying};
      }
    }
    shown_ = wanted;
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    for (std::size_t place = 0; place < spheres_.size(); ++place) {
      Sphere& sphere = spheres_[place];
      if (sphere.stage == Stage::kNone) {
        continue;
      }
      sphere.host.position = position_ + CameraPlaneOffset(tick.view, kShadowPlaces[place].offset);
      if (AdvanceEffectClassStrHost(sphere.host, tick)) {
        continue;
      }
      // L'APPARITION FINIE, la boucle ; la disparition finie, plus rien.
      if (sphere.stage == Stage::kCreating) {
        sphere = Sphere{Host(kShadowLoop, true, kShadowPlaces[place].scale), Stage::kLooping};
      } else if (sphere.stage == Stage::kDying) {
        sphere.stage = Stage::kNone;
      }
    }
  }

 private:
  enum class Stage : std::uint8_t { kNone, kCreating, kLooping, kDying };
  struct Sphere {
    EffectClassStrHost host;
    Stage stage = Stage::kNone;
  };

  std::array<Sphere, kShadowPlaces.size()> spheres_{};
  std::size_t shown_ = 0;
};

}  // namespace

std::unique_ptr<EffectClassInstance> MakeServantWeapons(const EffectClassBirth& birth,
                                                        const EffectClassView& /*view*/) {
  return std::make_unique<ServantWeapons>(birth);
}

std::unique_ptr<EffectClassInstance> MakeShadowSpheres(const EffectClassBirth& birth,
                                                       const EffectClassView& /*view*/) {
  return std::make_unique<ShadowSpheres>(birth);
}

}  // namespace ro::render::effect_class
