// LES CLASSES QU'UN ÉTAT POSE SUR SON PORTEUR et qui le suivent tant qu'il est
// là : les astres des postures du Star Emperor, l'âme de Soul Unity, Climax, la
// machine d'attaque du Meister, le compte d'Eternal Slash, les cubes maudits.

#include <algorithm>
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

// Un hôte de `.str` neuf ; `ground` le range dans la file du sol.
EffectClassStrHost StrHost(std::string path, bool ground, bool loops) {
  EffectClassStrHost host;
  host.path = std::move(path);
  host.behind_actors = ground;
  host.loops = loops;
  return host;
}

// ── Les postures du Star Emperor (1214 à 1217) ───────────────────────────────
//
// TROIS ASTRES en orbite de huit unités, dix au-dessus du porteur, à un tiers
// de tour l'un de l'autre, qui avancent de deux centièmes de radian par tic :
// trois soleils, trois lunes ou trois étoiles, ou les trois ensemble pour
// l'Univers. Panneaux additifs de trois de côté, face à la caméra.
constexpr std::uint32_t kSunStanceId = 1214;
constexpr std::uint32_t kLunarStanceId = 1215;
constexpr std::uint32_t kStarStanceId = 1216;
constexpr std::string_view kSunTexture = "effect\\sun_stance\\sun_stance.tga";
constexpr std::string_view kLunarTexture = "effect\\sun_stance\\lunar_stance.tga";
constexpr std::string_view kStarTexture = "effect\\sun_stance\\star_stance.tga";
constexpr std::int32_t kStanceBodies = 3;
constexpr float kStanceThirdTurn = 2.0943947F;
constexpr float kStanceOrbitStep = 0.02F;
constexpr float kStanceRadius = 8.0F;
constexpr float kStanceHeight = 10.0F;
constexpr float kStanceScale = 3.0F;

std::string_view StanceTexture(std::uint32_t effect_id, std::int32_t rank) {
  switch (effect_id) {
    case kSunStanceId:
      return kSunTexture;
    case kLunarStanceId:
      return kLunarTexture;
    case kStarStanceId:
      return kStarTexture;
    default:
      return rank == 0 ? kSunTexture : rank == 1 ? kLunarTexture : kStarTexture;
  }
}

class SunStance final : public EffectClassInstance {
 public:
  explicit SunStance(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    for (std::int32_t rank = 0; rank < kStanceBodies; ++rank) {
      Body body;
      body.quad.texture = StanceTexture(effect_id_, rank);
      body.orbit = static_cast<float>(rank) * kStanceThirdTurn;
      bodies_.push_back(body);
    }
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present;
  }
  void Draw(const EffectClassView& view, EffectClassOutput& output) const override {
    for (const Body& body : bodies_) {
      QuadParticle layer = body.quad;
      layer.matrix = NativeScale(kStanceScale, kStanceScale, 1.0F);
      MultiplyRotationX(layer.matrix, Radians(view.pitch_degrees));
      SubmitQuad(layer, QuadUv{}, false, view, output);
    }
  }

 protected:
  void Update(const EffectClassTick& /*tick*/, EffectClassOutput& /*output*/) override {
    for (Body& body : bodies_) {
      body.orbit += kStanceOrbitStep;
      body.quad.position =
          position_ + glm::vec3{std::sin(body.orbit) * kStanceRadius, -kStanceHeight,
                                std::cos(body.orbit) * kStanceRadius};
    }
  }

 private:
  struct Body {
    QuadParticle quad;
    float orbit = 0.0F;
  };
  std::vector<Body> bodies_;
};

// ── L'âme de Soul Unity (1222) ───────────────────────────────────────────────
//
// UN PANNEAU ADDITIF de deux et demi, face à la caméra, quinze unités au-dessus
// du porteur dans le plan de la caméra.
constexpr std::string_view kSoulUnityTexture = "effect\\soul_unity\\soul_unity.bmp";
constexpr float kSoulUnityScale = 2.5F;
constexpr float kSoulUnityHeight = 15.0F;
constexpr std::uint32_t kSoulUnityRenderWord = 0;

class SoulUnity final : public EffectClassInstance {
 public:
  explicit SoulUnity(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    quad_.texture = kSoulUnityTexture;
    quad_.render_word = kSoulUnityRenderWord;
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present;
  }
  void Draw(const EffectClassView& view, EffectClassOutput& output) const override {
    QuadParticle layer = quad_;
    layer.matrix = NativeScale(kSoulUnityScale, kSoulUnityScale, 1.0F);
    MultiplyRotationX(layer.matrix, Radians(view.pitch_degrees));
    MultiplyRotationY(layer.matrix, Radians(view.yaw_degrees));
    MultiplyTranslation(layer.matrix,
                        position_ + CameraPlaneOffset(&view, {0.0F, -kSoulUnityHeight, 0.0F}));
    SubmitQuad(layer, QuadUv{}, true, view, output);
  }

 protected:
  void Update(const EffectClassTick& /*tick*/, EffectClassOutput& /*output*/) override {}

 private:
  QuadParticle quad_;
};

// ── Les `.str` qui suivent leur porteur, décalés dans le plan de la caméra ──
//
// Climax (1800) : l'éclat en boucle et, au sol, le fond du niveau reçu — de un
// à cinq, rien au-delà —, dix unités au-dessus du porteur, et un bruit à la
// naissance. La machine d'attaque du Meister (1839) : son attente d'attaque
// joue une fois, puis son attente boucle, huit unités au-dessus. Les cubes
// maudits (2305 rouge, 2306 bleu) : leur apparition, au sol et en l'air, puis
// leur boucle une fois l'apparition finie, douze unités au-dessus. Une seule
// instance par porteur pour chacune.
constexpr std::uint32_t kClimaxId = 1800;
constexpr std::uint32_t kAttackMachineId = 1839;
constexpr std::uint32_t kRedCubeId = 2305;
constexpr std::string_view kClimaxMain = "climax\\climax\\climax.str";
constexpr std::string_view kClimaxBottomPrefix = "climax\\climax_bottom\\climax_bottom_";
constexpr std::string_view kClimaxSound = "effect\\AG_CLIMAX.wav";
constexpr std::size_t kClimaxDigits = 1;
constexpr std::int32_t kClimaxFirstLevel = 1;
constexpr std::int32_t kClimaxLastLevel = 5;
constexpr float kClimaxHeight = 10.0F;
constexpr std::array<std::string_view, 3> kMachineStrs = {
    "a_machine\\a_machine\\a_machine_attack_waiting.str",
    "a_machine\\a_machine\\a_machine_attack.str", "a_machine\\a_machine\\a_machine_waiting.str"};
constexpr std::size_t kMachineWaiting = 2;
constexpr float kMachineHeight = 8.0F;
constexpr std::array<std::string_view, 4> kRedCubeStrs = {
    "NPC_CURSE_OF_RED_CUBE\\npc_cur_r_cube\\npc_cur_r_cube_bottom_start.str",
    "NPC_CURSE_OF_RED_CUBE\\npc_cur_r_cube\\npc_cur_r_cube_start.str",
    "NPC_CURSE_OF_RED_CUBE\\npc_cur_r_cube\\npc_cur_r_cube_bottom_loop.str",
    "NPC_CURSE_OF_RED_CUBE\\npc_cur_r_cube\\npc_cur_r_cube_loop.str"};
constexpr std::array<std::string_view, 4> kBlueCubeStrs = {
    "NPC_CURSE_OF_BLUE_CUBE\\npc_cur_b_cube\\npc_cur_b_cube_bottom_start.str",
    "NPC_CURSE_OF_BLUE_CUBE\\npc_cur_b_cube\\npc_cur_b_cube_start.str",
    "NPC_CURSE_OF_BLUE_CUBE\\npc_cur_b_cube\\npc_cur_b_cube_bottom_loop.str",
    "NPC_CURSE_OF_BLUE_CUBE\\npc_cur_b_cube\\npc_cur_b_cube_loop.str"};
constexpr float kCubeHeight = 12.0F;

// Le chemin d'un `.str` numéroté par le niveau reçu : le préfixe, le nombre
// complété de zéros jusqu'à `digits` chiffres, l'extension.
std::string NumberedPath(std::string_view prefix, std::int32_t level, std::size_t digits) {
  std::string number = std::to_string(level);
  if (number.size() < digits) {
    number.insert(0, digits - number.size(), '0');
  }
  return std::string(prefix) + number + ".str";
}

class Climax final : public EffectClassInstance {
 public:
  explicit Climax(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    reused_on_same_owner_ = true;
    hosts_.push_back(StrHost(std::string(kClimaxMain), false, true));
    if (level_ >= kClimaxFirstLevel && level_ <= kClimaxLastLevel) {
      hosts_.push_back(
          StrHost(NumberedPath(kClimaxBottomPrefix, level_, kClimaxDigits), true, true));
    }
    QueueSound(kClimaxSound, 0);
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present;
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    for (const EffectClassStrHost& host : hosts_) {
      DrawEffectClassStrHost(host, output);
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    const glm::vec3 lifted = position_ + CameraPlaneOffset(tick.view, {0.0F, -kClimaxHeight, 0.0F});
    for (EffectClassStrHost& host : hosts_) {
      host.position = lifted;
      static_cast<void>(AdvanceEffectClassStrHost(host, tick));
    }
  }

 private:
  std::vector<EffectClassStrHost> hosts_;
};

class AttackMachine final : public EffectClassInstance {
 public:
  explicit AttackMachine(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    reused_on_same_owner_ = true;
    for (std::size_t rank = 0; rank < kMachineStrs.size(); ++rank) {
      hosts_.push_back(StrHost(std::string(kMachineStrs[rank]), false, rank == kMachineWaiting));
    }
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present;
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    DrawEffectClassStrHost(hosts_[shown_], output);
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    EffectClassStrHost& host = hosts_[shown_];
    host.position = position_ + CameraPlaneOffset(tick.view, {0.0F, -kMachineHeight, 0.0F});
    if (!AdvanceEffectClassStrHost(host, tick)) {
      // La fin d'une attente d'attaque rend la main à l'attente, reprise à sa
      // première image.
      hosts_[kMachineWaiting] = StrHost(std::string(kMachineStrs[kMachineWaiting]), false, true);
      shown_ = kMachineWaiting;
    }
  }

 private:
  std::vector<EffectClassStrHost> hosts_;
  std::size_t shown_ = 0;
};

class CurseOfCube final : public EffectClassInstance {
 public:
  explicit CurseOfCube(const EffectClassBirth& birth)
      : EffectClassInstance(birth), strs_(effect_id_ == kRedCubeId ? kRedCubeStrs : kBlueCubeStrs) {
    reused_on_same_owner_ = true;
    hosts_.push_back(StrHost(std::string(strs_[0]), true, false));
    hosts_.push_back(StrHost(std::string(strs_[1]), false, false));
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present && !hosts_.empty();
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    for (const EffectClassStrHost& host : hosts_) {
      DrawEffectClassStrHost(host, output);
    }
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    // Un `.str` fini au tic d'avant s'en va ; ceux qui restent suivent.
    std::erase_if(hosts_, [](const EffectClassStrHost& host) { return host.finished; });
    const glm::vec3 lifted = position_ + CameraPlaneOffset(tick.view, {0.0F, -kCubeHeight, 0.0F});
    for (EffectClassStrHost& host : hosts_) {
      host.position = lifted;
      static_cast<void>(AdvanceEffectClassStrHost(host, tick));
    }
    // L'apparition finie, la boucle prend sa place, pour toujours.
    if (!looping_ && hosts_.empty()) {
      hosts_.push_back(StrHost(std::string(strs_[2]), true, true));
      hosts_.push_back(StrHost(std::string(strs_[3]), false, true));
      looping_ = true;
    }
  }

 private:
  const std::array<std::string_view, 4>& strs_;
  std::vector<EffectClassStrHost> hosts_;
  bool looping_ = false;
};

// ── Le compte d'Eternal Slash (1913) ─────────────────────────────────────────
//
// LE CHIFFRE DU COMPTE REÇU, de un à cinq, vingt-cinq unités au-dessus du
// porteur : un `.str` joué une fois, et l'instance meurt avec lui. Hors de ces
// bornes, rien.
constexpr std::string_view kSlashCountPrefix =
    "eternal_slash\\eternal_slash_count\\eternal_slash_count_";
constexpr std::size_t kSlashCountDigits = 2;
constexpr std::int32_t kSlashCountFirst = 1;
constexpr std::int32_t kSlashCountLast = 5;
constexpr float kSlashCountHeight = 25.0F;

class EternalSlashCount final : public EffectClassInstance {
 public:
  explicit EternalSlashCount(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    if (level_ >= kSlashCountFirst && level_ <= kSlashCountLast) {
      host_ = StrHost(NumberedPath(kSlashCountPrefix, level_, kSlashCountDigits), false, false);
      host_.offset = {0.0F, -kSlashCountHeight, 0.0F};
    } else {
      host_.finished = true;
    }
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

}  // namespace

std::unique_ptr<EffectClassInstance> MakeSunStance(const EffectClassBirth& birth,
                                                   const EffectClassView& /*view*/) {
  return std::make_unique<SunStance>(birth);
}

std::unique_ptr<EffectClassInstance> MakeSoulUnity(const EffectClassBirth& birth,
                                                   const EffectClassView& /*view*/) {
  return std::make_unique<SoulUnity>(birth);
}

std::unique_ptr<EffectClassInstance> MakeBearerStr(const EffectClassBirth& birth,
                                                   const EffectClassView& /*view*/) {
  switch (birth.effect_id) {
    case kClimaxId:
      return std::make_unique<Climax>(birth);
    case kAttackMachineId:
      return std::make_unique<AttackMachine>(birth);
    default:
      return std::make_unique<CurseOfCube>(birth);
  }
}

std::unique_ptr<EffectClassInstance> MakeEternalSlashCount(const EffectClassBirth& birth,
                                                           const EffectClassView& /*view*/) {
  return std::make_unique<EternalSlashCount>(birth);
}

}  // namespace ro::render::effect_class
