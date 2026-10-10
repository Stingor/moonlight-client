// LES CLASSES QUI JOUENT DES `.str` SUR UN ACTEUR : Petitio, Savage Impact,
// Eternal Slash, les flèches d'Abyss Chaser, Storm Cannon, Shadow Exceed.

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "effect_class_internal.h"

namespace ro::render::effect_class {
namespace {

constexpr float kTwelveUp = -12.0F;
constexpr float kThirteenUp = -13.0F;

// Un hôte neuf, à la place de sa classe.
EffectClassStrHost Host(std::string_view path, glm::vec3 position, bool ground) {
  EffectClassStrHost host;
  host.path = std::string(path);
  host.position = position;
  host.behind_actors = ground;
  return host;
}

bool AnyAlive(const std::vector<EffectClassStrHost>& hosts) {
  return std::ranges::any_of(hosts, [](const EffectClassStrHost& h) { return !h.finished; });
}

// ── Petitio (1911) ───────────────────────────────────────────────────────────
//
// DEUX `.str` SUR LA CIBLE, douze unités plus haut : le fond au sol et
// l'éclat. Un son au premier tic, un tremblement d'écran au sixième ; l'instance
// meurt quand les deux ont fini.
constexpr std::string_view kPetitioBottom = "petitio\\petitio_bottom\\petitio_bottom.str";
constexpr std::string_view kPetitioMain = "petitio\\petitio\\petitio.str";
constexpr std::string_view kPetitioSound = "effect\\CD_PETITIO.wav";
constexpr std::int32_t kPetitioQuakeTick = 6;

class Petitio final : public EffectClassInstance {
 public:
  explicit Petitio(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    hosts_.push_back(Host(kPetitioBottom, position_, true));
    hosts_.push_back(Host(kPetitioMain, position_, false));
    QueueQuake(kPetitioQuakeTick);
    QueueSound(kPetitioSound, 0);
  }
  [[nodiscard]] bool Alive(const EffectClassTick& /*tick*/) const override {
    return AnyAlive(hosts_);
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    for (const EffectClassStrHost& host : hosts_) {
      DrawEffectClassStrHost(host, output);
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    for (EffectClassStrHost& host : hosts_) {
      host.position = position_ + glm::vec3{0.0F, kTwelveUp, 0.0F};
      static_cast<void>(AdvanceEffectClassStrHost(host, tick));
    }
  }

 private:
  std::vector<EffectClassStrHost> hosts_;
};

// ── Savage Impact (1837) ─────────────────────────────────────────────────────
//
// UN `.str` QUI SUIT LE LANCEUR, douze unités plus haut, tourné une fois pour
// toutes à l'écran dans la direction qui va du lanceur à la cible.
constexpr std::string_view kSavageImpactPath = "savage_impact\\savage_impact\\savage_impact.str";
constexpr std::string_view kSavageImpactSound = "effect\\SHC_SAVAGE_IMPACT.wav";

class SavageImpact final : public EffectClassInstance {
 public:
  SavageImpact(const EffectClassBirth& birth, const EffectClassView& view)
      : EffectClassInstance(birth), host_(Host(kSavageImpactPath, position_, false)) {
    host_.spin_degrees = bound_position_.has_value()
                             ? ScreenAngleDegrees(view, position_, *bound_position_).value_or(0.0F)
                             : 0.0F;
    QueueSound(kSavageImpactSound, 0);
  }
  [[nodiscard]] bool Alive(const EffectClassTick& /*tick*/) const override {
    return !host_.finished;
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    DrawEffectClassStrHost(host_, output);
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    host_.position = position_ + glm::vec3{0.0F, kTwelveUp, 0.0F};
    static_cast<void>(AdvanceEffectClassStrHost(host_, tick));
  }

 private:
  EffectClassStrHost host_;
};

// ── Eternal Slash (1912) ─────────────────────────────────────────────────────
//
// DEUX `.str` SUR LE LANCEUR, treize unités plus haut, sans son : l'instance
// meurt quand les deux ont fini, et le premier fini tient sa dernière image
// jusque-là.
constexpr std::string_view kEternalSlashBottom =
    "eternal_slash\\eternal_slash_bottom\\eternal_slash_bottom.str";
constexpr std::string_view kEternalSlashMain = "eternal_slash\\eternal_slash\\eternal_slash.str";

class EternalSlash final : public EffectClassInstance {
 public:
  explicit EternalSlash(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    hosts_.push_back(Host(kEternalSlashBottom, position_, true));
    hosts_.push_back(Host(kEternalSlashMain, position_, false));
    for (EffectClassStrHost& host : hosts_) {
      host.offset = {0.0F, kThirteenUp, 0.0F};
    }
  }
  [[nodiscard]] bool Alive(const EffectClassTick& /*tick*/) const override {
    return AnyAlive(hosts_);
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    for (const EffectClassStrHost& host : hosts_) {
      DrawEffectClassStrHost(host, output);
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    for (EffectClassStrHost& host : hosts_) {
      host.position = position_;
      static_cast<void>(AdvanceEffectClassStrHost(host, tick));
    }
  }

 private:
  std::vector<EffectClassStrHost> hosts_;
};

// ── Les flèches de l'Abyss Chaser (1756, 2114, 2154) ─────────────────────────
//
// UN `.str` SUR LE LANCEUR, tourné une fois à l'écran vers la cible : la flèche
// cramoisie (treize unités plus haut), le tir frénétique (quatre), la ruée
// malchanceuse (sans décalage ni son).
constexpr std::uint32_t kCrimsonArrowId = 1756;
constexpr std::uint32_t kFrenzyShotId = 2114;
constexpr float kFrenzyShotUp = -4.0F;

class CrimsonArrow final : public EffectClassInstance {
 public:
  CrimsonArrow(const EffectClassBirth& birth, const EffectClassView& view)
      : EffectClassInstance(birth) {
    if (effect_id_ == kCrimsonArrowId) {
      host_ = Host("crimsonarrow\\crimsonarrow\\crimsonarrow.str", position_, false);
      host_.offset = {0.0F, kThirteenUp, 0.0F};
      QueueSound("effect\\AG_CRIMSON_ARROW1.wav", 0);
    } else if (effect_id_ == kFrenzyShotId) {
      host_ = Host("frenzy_shot\\frenzy_shot\\frenzy_shot.str", position_, false);
      host_.offset = {0.0F, kFrenzyShotUp, 0.0F};
      QueueSound("effect\\ABC_FRENZY_SHOT.wav", 0);
    } else {
      host_ = Host("unlucky_rush\\unlucky_rush\\unlucky_rush.str", position_, false);
    }
    host_.spin_degrees = bound_position_.has_value()
                             ? ScreenAngleDegrees(view, position_, *bound_position_).value_or(0.0F)
                             : 0.0F;
  }
  [[nodiscard]] bool Alive(const EffectClassTick& /*tick*/) const override {
    return !host_.finished;
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    DrawEffectClassStrHost(host_, output);
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    host_.position = position_;
    static_cast<void>(AdvanceEffectClassStrHost(host_, tick));
  }

 private:
  EffectClassStrHost host_;
};

// ── Storm Cannon (1755) ──────────────────────────────────────────────────────
//
// DEUX `.str` BOUCLÉS qui filent d'une unité sept dixièmes par tic vers la place
// de la cible au lancement, et au-delà. Passé cette distance, ils s'éteignent de
// dix sur deux cent cinquante-cinq par tic ; l'instance meurt éteinte.
constexpr std::string_view kStormCannonBottom =
    "stormcannon\\stormcannon_bottom\\stormcannon_bottom.str";
constexpr std::string_view kStormCannonMain = "stormcannon\\stormcannon\\stormcannon.str";
constexpr std::string_view kStormCannonSound = "effect\\AG_STORM_CANNON.wav";
constexpr float kStormCannonSpeed = 1.7F;
constexpr std::int32_t kStormCannonOpacity = 255;
constexpr std::int32_t kStormCannonFade = 10;
constexpr float kStormCannonOpacityScale = 255.0F;
constexpr glm::vec3 kStormCannonOffset{-1.5F, -17.0F, 0.0F};

class StormCannon final : public EffectClassInstance {
 public:
  explicit StormCannon(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    for (EffectClassStrHost host :
         {Host(kStormCannonBottom, position_, true), Host(kStormCannonMain, position_, false)}) {
      host.loops = true;
      hosts_.push_back(host);
    }
    origin_ = position_;
    if (bound_position_.has_value()) {
      const glm::vec3 delta = *bound_position_ - position_;
      reach_ = std::sqrt(glm::dot(delta, delta));
      if (reach_ > 0.0F) {
        direction_ = delta / reach_;
      }
    }
    QueueSound(kStormCannonSound, 0);
  }
  [[nodiscard]] bool Alive(const EffectClassTick& /*tick*/) const override { return opacity_ > 0; }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    for (const EffectClassStrHost& host : hosts_) {
      DrawEffectClassStrHost(host, output);
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    position_ += kStormCannonSpeed * direction_;
    const glm::vec3 travelled = position_ - origin_;
    if (std::sqrt(glm::dot(travelled, travelled)) > reach_) {
      opacity_ = std::max(0, opacity_ - kStormCannonFade);
    }
    for (EffectClassStrHost& host : hosts_) {
      host.alpha = static_cast<float>(opacity_) / kStormCannonOpacityScale;
      host.position = position_ + kStormCannonOffset;
      static_cast<void>(AdvanceEffectClassStrHost(host, tick));
    }
  }

 private:
  std::vector<EffectClassStrHost> hosts_;
  glm::vec3 origin_{0.0F};
  glm::vec3 direction_{0.0F};
  float reach_ = 0.0F;
  std::int32_t opacity_ = kStormCannonOpacity;
};

// ── Shadow Exceed (1867) ─────────────────────────────────────────────────────
//
// L'OMBRE QUI DOUBLE LE LANCEUR : quatre `.str` aux quatorze centièmes, cinq
// unités derrière lui sur l'axe de son corps, en deux paires — celle DE FACE,
// pour une image qui regarde la caméra (sud, sud-ouest, est, sud-est), et celle
// DE DOS sinon. Les quatre avancent à chaque tic, mais seule la paire du moment
// se peint, retournée à l'écran selon la direction dessinée : la paire de face
// quand l'image regarde le sud ou le sud-ouest, celle de dos quand elle regarde
// le nord ou le nord-est. La paire de dos passe SOUS les corps — le client
// l'enfonce d'un biais de profondeur, et c'est la file du sol qui le rend ici.
//
// Le son part au premier tic, et une seconde naissance sur le même porteur rend
// la première. Une image finie quitte sa paire ; l'instance meurt dès qu'une
// paire est vide, ou que le porteur s'en va — et, s'il est encore là, elle lui
// pose l'effet 1004, à moins qu'il ne le porte déjà. Sans porteur, rien ne bouge.
constexpr std::string_view kShadowExceedFrontBottom =
    "SHADOW_CROSS\\SHC_SHADOW_EXCEED\\shadow_exceed\\shadow_exceed_f_shadow_bottom.str";
constexpr std::string_view kShadowExceedFront =
    "SHADOW_CROSS\\SHC_SHADOW_EXCEED\\shadow_exceed\\shadow_exceed_f_shadow.str";
constexpr std::string_view kShadowExceedBackBottom =
    "SHADOW_CROSS\\SHC_SHADOW_EXCEED\\shadow_exceed\\shadow_exceed_b_shadow_bottom.str";
constexpr std::string_view kShadowExceedBack =
    "SHADOW_CROSS\\SHC_SHADOW_EXCEED\\shadow_exceed\\shadow_exceed_b_shadow.str";
constexpr std::string_view kShadowExceedSound = "effect\\SHC_SHADOW_EXCEED.wav";
constexpr float kShadowExceedScale = 0.14F;
constexpr float kShadowExceedBehindUnits = 5.0F;
constexpr float kShadowExceedBottomUp = -12.0F;
constexpr float kShadowExceedShadowUp = -10.0F;
constexpr std::uint32_t kShadowExceedAuraId = 1004;
// LES DIRECTIONS DESSINÉES, dans l'ordre du client : zéro face à la caméra.
constexpr std::uint8_t kDrawnDirections = 8;
constexpr std::uint8_t kDrawnSouthWest = 1;
constexpr std::uint8_t kDrawnWest = 2;
constexpr std::uint8_t kDrawnNorth = 4;
constexpr std::uint8_t kDrawnNorthEast = 5;

class ShadowExceed final : public EffectClassInstance {
 public:
  explicit ShadowExceed(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    reused_on_same_owner_ = true;
    QueueSound(kShadowExceedSound, 0);
    front_ = {Shadow(kShadowExceedFrontBottom, kShadowExceedBottomUp, false),
              Shadow(kShadowExceedFront, kShadowExceedShadowUp, false)};
    back_ = {Shadow(kShadowExceedBackBottom, kShadowExceedBottomUp, true),
             Shadow(kShadowExceedBack, kShadowExceedShadowUp, true)};
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present && !dead_;
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    if (dead_) {
      return;
    }
    for (const EffectClassStrHost& host : shows_back_ ? back_ : front_) {
      DrawEffectClassStrHost(host, output);
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& output) override {
    if (!tick.owner_present || dead_) {
      return;
    }
    const float heading = Radians(tick.owner_body_degrees.value_or(0.0F));
    const glm::vec3 anchor =
        position_ + glm::vec3{-std::sin(heading) * kShadowExceedBehindUnits, 0.0F,
                              std::cos(heading) * kShadowExceedBehindUnits};
    const std::uint8_t direction = tick.owner_sprite_direction.value_or(0) % kDrawnDirections;
    shows_back_ = direction >= kDrawnWest && direction <= kDrawnNorthEast;
    AdvancePair(front_, anchor, direction <= kDrawnSouthWest, tick);
    AdvancePair(back_, anchor, direction == kDrawnNorth || direction == kDrawnNorthEast, tick);
    if (front_.empty() || back_.empty()) {
      dead_ = true;
      output.spawns_on_owner.push_back(kShadowExceedAuraId);
    }
  }

 private:
  static EffectClassStrHost Shadow(std::string_view path, float up, bool ground) {
    EffectClassStrHost host = Host(path, glm::vec3{0.0F}, ground);
    host.offset = {0.0F, up, 0.0F};
    host.scale = kShadowExceedScale;
    return host;
  }
  static void AdvancePair(std::vector<EffectClassStrHost>& pair, glm::vec3 anchor, bool flipped,
                          const EffectClassTick& tick) {
    for (EffectClassStrHost& host : pair) {
      host.position = anchor;
      host.flip_x = flipped;
      static_cast<void>(AdvanceEffectClassStrHost(host, tick));
    }
    std::erase_if(pair, [](const EffectClassStrHost& host) { return host.finished; });
  }

  std::vector<EffectClassStrHost> front_;
  std::vector<EffectClassStrHost> back_;
  bool shows_back_ = false;
  bool dead_ = false;
};

}  // namespace

std::unique_ptr<EffectClassInstance> MakePetitio(const EffectClassBirth& birth,
                                                 const EffectClassView& /*view*/) {
  return std::make_unique<Petitio>(birth);
}

std::unique_ptr<EffectClassInstance> MakeSavageImpact(const EffectClassBirth& birth,
                                                      const EffectClassView& view) {
  return std::make_unique<SavageImpact>(birth, view);
}

std::unique_ptr<EffectClassInstance> MakeEternalSlash(const EffectClassBirth& birth,
                                                      const EffectClassView& /*view*/) {
  return std::make_unique<EternalSlash>(birth);
}

std::unique_ptr<EffectClassInstance> MakeCrimsonArrow(const EffectClassBirth& birth,
                                                      const EffectClassView& view) {
  return std::make_unique<CrimsonArrow>(birth, view);
}

std::unique_ptr<EffectClassInstance> MakeStormCannon(const EffectClassBirth& birth,
                                                     const EffectClassView& /*view*/) {
  return std::make_unique<StormCannon>(birth);
}

std::unique_ptr<EffectClassInstance> MakeShadowExceed(const EffectClassBirth& birth,
                                                      const EffectClassView& /*view*/) {
  return std::make_unique<ShadowExceed>(birth);
}

}  // namespace ro::render::effect_class
