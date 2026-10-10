// LES CLASSES QUE POSE UNE UNITÉ AU SOL : Floral Flare Road, Astral Strike,
// Pneumaticus Procella, Rain of Crystal. Chacune vit tant que son unité est là.

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "effect_class_internal.h"

namespace ro::render::effect_class {
namespace {

// Ce qu'un `.str` d'unité demande à l'ancre : quatre-vingts pixels plus haut,
// comme un effet en fichier.
constexpr float kUnitLiftPixels = 80.0F;

EffectClassStrHost UnitHost(std::string_view path, glm::vec3 position, bool ground,
                            float lift_pixels) {
  EffectClassStrHost host;
  host.path = std::string(path);
  host.position = position;
  host.behind_actors = ground;
  host.lift_pixels = lift_pixels;
  return host;
}

// RETIRE LES HÔTES QUI ONT FINI AU TIC D'AVANT, puis fait jouer les autres : un
// hôte qui finit se peint encore une fois, sur sa dernière image.
void AdvanceOrDrop(std::vector<EffectClassStrHost>& hosts, const EffectClassTick& tick) {
  std::erase_if(hosts, [](const EffectClassStrHost& host) { return host.finished; });
  for (EffectClassStrHost& host : hosts) {
    static_cast<void>(AdvanceEffectClassStrHost(host, tick));
  }
}

void DrawAll(const std::vector<EffectClassStrHost>& hosts, EffectClassOutput& output) {
  for (const EffectClassStrHost& host : hosts) {
    DrawEffectClassStrHost(host, output);
  }
}

// ── Floral Flare Road (1804) ─────────────────────────────────────────────────
//
// UN FOND AU SOL ET QUINZE FLEURS posées au hasard autour de l'unité, la
// i-ème retardée de trois cents millisecondes par rang, chacune avec son bruit.
// Le sinus tire l'abscisse et le cosinus la profondeur, de deux tirages
// indépendants : un point du carré, non d'un cercle.
constexpr std::string_view kFloralBottom =
    "froral_flareroad\\froral_flareroad_bottom\\froral_flareroad_bottom.str";
constexpr std::string_view kFloralFlower =
    "froral_flareroad\\froral_flareroad\\froral_flareroad.str";
constexpr std::string_view kFloralSound = "effect\\AG_ALL_BLOOM.wav";
constexpr std::int32_t kFloralDelayStepMs = 300;
constexpr std::int32_t kFloralDelayEndMs = 4500;
constexpr std::int32_t kFloralRadiusSpan = 20;

class FloralFlareRoad final : public EffectClassInstance {
 public:
  explicit FloralFlareRoad(const EffectClassBirth& birth)
      : EffectClassInstance(birth),
        ground_(UnitHost(kFloralBottom, position_, true, kUnitLiftPixels)) {
    for (std::int32_t delay = 0; delay < kFloralDelayEndMs; delay += kFloralDelayStepMs) {
      glm::vec3 place = position_;
      const std::int32_t radius_x = std::max(Rand() % kFloralRadiusSpan, 0);
      const std::int32_t angle_x =
          std::max(Rand() % static_cast<std::int32_t>(kFullTurnDegrees), 0);
      place.x += EffectClassSinTable(angle_x) * static_cast<float>(radius_x);
      const std::int32_t radius_z = std::max(Rand() % kFloralRadiusSpan, 0);
      const std::int32_t angle_z =
          std::max(Rand() % static_cast<std::int32_t>(kFullTurnDegrees), 0);
      place.z += EffectClassCosTable(angle_z) * static_cast<float>(radius_z);
      EffectClassStrHost flower = UnitHost(kFloralFlower, place, false, kUnitLiftPixels);
      flower.delay_ms = delay;
      flowers_.push_back(flower);
      QueueSound(kFloralSound, delay / kTickMs);
    }
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present;
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    DrawEffectClassStrHost(ground_, output);
    DrawAll(flowers_, output);
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    static_cast<void>(AdvanceEffectClassStrHost(ground_, tick));
    AdvanceOrDrop(flowers_, tick);
  }

 private:
  EffectClassStrHost ground_;
  std::vector<EffectClassStrHost> flowers_;
};

// ── Astral Strike (1768) ─────────────────────────────────────────────────────
//
// UNE PAIRE D'OUVERTURE, puis, une fois finie, une paire au hasard parmi trois
// tous les onze, neuf, sept ou cinq tics selon le niveau, en un point du carré
// autour de l'unité. Chaque paire suivante sonne au tic d'après.
//
// ⚠ AU NIVEAU ZÉRO, le client lit une case de pile jamais écrite pour sa période
// et son étendue : rien n'y est défini, et rien ne naît ici.
constexpr std::string_view kAstralBottom =
    "astralstrike\\astralstrike_bottom\\astralstrike_bottom.str";
constexpr std::string_view kAstralMain = "astralstrike\\astralstrike\\astralstrike.str";
constexpr std::string_view kAstralOpeningSound = "effect\\AG_ASTRAL_STRIKE1.wav";
constexpr std::string_view kAstralWaveSound = "effect\\AG_ASTRAL_STRIKE2.wav";
constexpr std::array<std::int32_t, 11> kAstralPeriods = {0, 11, 11, 11, 9, 9, 9, 7, 7, 7, 5};
constexpr std::array<std::int32_t, 11> kAstralSpans = {0, 5, 5, 5, 7, 7, 7, 9, 9, 9, 11};
constexpr std::uint32_t kAstralLevelCap = 10;
constexpr std::int32_t kAstralSpanUnit = 50;
constexpr float kAstralSpread = 0.1F;
constexpr float kHalfSpan = 0.5F;
constexpr std::int32_t kAstralVariants = 3;
constexpr std::array<std::string_view, 3> kAstralSubBottoms = {
    "astralstrike\\astralstrike_sub_1_bottom\\astralstrike_sub_1_bottom.str",
    "astralstrike\\astralstrike_sub_2_bottom\\astralstrike_sub_2_bottom.str",
    "astralstrike\\astralstrike_sub_3_bottom\\astralstrike_sub_3_bottom.str"};
constexpr std::array<std::string_view, 3> kAstralSubs = {
    "astralstrike\\astralstrike_sub_1\\astralstrike_sub_1.str",
    "astralstrike\\astralstrike_sub_2\\astralstrike_sub_2.str",
    "astralstrike\\astralstrike_sub_3\\astralstrike_sub_3.str"};

class AstralStrike final : public EffectClassInstance {
 public:
  explicit AstralStrike(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    opening_.push_back(UnitHost(kAstralBottom, position_, true, 0.0F));
    QueueSound(kAstralOpeningSound, 0);
    opening_.push_back(UnitHost(kAstralMain, position_, false, 0.0F));
    level_index_ = std::min(static_cast<std::uint32_t>(level_), kAstralLevelCap);
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present;
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    DrawAll(opening_, output);
    DrawAll(waves_, output);
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    AdvanceOrDrop(opening_, tick);
    const std::int32_t period = kAstralPeriods[level_index_];
    if (tick.owner_present && opening_.empty() && period > 0 && age_ % period == 0) {
      SpawnWave();
    }
    AdvanceOrDrop(waves_, tick);
  }

 private:
  void SpawnWave() {
    const std::int32_t span = kAstralSpanUnit * kAstralSpans[level_index_];
    const float half = static_cast<float>(span) * kHalfSpan;
    glm::vec3 place = position_;
    place.x += (static_cast<float>(std::max(Rand() % (span + 1), 0)) - half) * kAstralSpread;
    place.z += (static_cast<float>(std::max(Rand() % (span + 1), 0)) - half) * kAstralSpread;
    const auto variant = static_cast<std::size_t>(Rand() % kAstralVariants);
    waves_.push_back(UnitHost(kAstralSubBottoms[variant], place, true, 0.0F));
    waves_.push_back(UnitHost(kAstralSubs[variant], place, false, 0.0F));
    QueueSound(kAstralWaveSound, age_ + 1);
  }

  std::vector<EffectClassStrHost> opening_;
  std::vector<EffectClassStrHost> waves_;
  std::uint32_t level_index_ = 0;
};

// ── Pneumaticus Procella (1957) ──────────────────────────────────────────────
//
// UN CERCLE CIBLE au sol, bouclé et mis à l'échelle du niveau, et QUARANTE
// TOURBILLONS autour de l'unité en quatre salves de dix, chacun retardé et
// décalé de vingt-cinq unités vers le haut. L'instance vit tant que l'unité.
constexpr std::string_view kProcellaTarget =
    "pneumaticusprocella\\pneumaticusprocella_target\\pneumaticusprocella_target.str";
constexpr const char* kProcellaSwirlPattern =
    "pneumaticusprocella\\pneumaticusprocella\\pneumaticusprocella_%d.str";
constexpr std::string_view kProcellaSound = "effect\\CD_PNEUMATICUS_PROCELLA.wav";
constexpr std::array<float, 10> kProcellaScales = {0.6F, 0.6F, 0.6F, 0.8F, 0.8F,
                                                   0.8F, 1.0F, 1.0F, 1.0F, 1.2F};
constexpr float kProcellaDefaultScale = 1.0F;
constexpr float kProcellaSpreadBonus = 0.6F;
constexpr std::array<float, 6> kProcellaOffsetsX = {-12.0F, -6.0F, 0.0F, 0.0F, 6.0F, 12.0F};
constexpr std::array<float, 9> kProcellaOffsetsZ = {-12.0F, -10.0F, -6.0F, -4.0F, 0.0F,
                                                    4.0F,   6.0F,   10.0F, 12.0F};
constexpr std::int32_t kProcellaSwirls = 40;
constexpr std::int32_t kProcellaSwirlStepMs = 100;
constexpr std::int32_t kProcellaVolley = 10;
constexpr std::int32_t kProcellaVolleyGapMs = 2000;
constexpr std::int32_t kProcellaJitterMs = 200;
constexpr std::int32_t kProcellaVariants = 4;
constexpr float kProcellaSwirlUp = -25.0F;
constexpr std::size_t kProcellaPathCapacity = 260;

class Pneumaticus final : public EffectClassInstance {
 public:
  explicit Pneumaticus(const EffectClassBirth& birth)
      : EffectClassInstance(birth),
        target_(UnitHost(kProcellaTarget, position_, true, kUnitLiftPixels)) {
    const bool known = level_ >= 1 && level_ <= static_cast<std::int32_t>(kProcellaScales.size());
    const float scale =
        known ? kProcellaScales[static_cast<std::size_t>(level_ - 1)] : kProcellaDefaultScale;
    target_.loops = true;
    target_.scale = scale;
    const float spread = scale + kProcellaSpreadBonus;
    for (std::int32_t rank = 0; rank < kProcellaSwirls; ++rank) {
      const float offset_x =
          spread * kProcellaOffsetsX[static_cast<std::size_t>(
                       std::max(Rand() % static_cast<std::int32_t>(kProcellaOffsetsX.size()), 0))];
      const float offset_z =
          spread * kProcellaOffsetsZ[static_cast<std::size_t>(
                       std::max(Rand() % static_cast<std::int32_t>(kProcellaOffsetsZ.size()), 0))];
      const std::int32_t jitter = std::max(Rand() % kProcellaJitterMs, 0);
      const std::int32_t delay =
          kProcellaSwirlStepMs * rank + kProcellaVolleyGapMs * (rank / kProcellaVolley) + jitter;
      const std::int32_t variant = std::max(Rand() % kProcellaVariants, 1);
      std::array<char, kProcellaPathCapacity> path{};
      std::snprintf(path.data(), path.size(), kProcellaSwirlPattern, variant);
      EffectClassStrHost swirl = UnitHost(path.data(), position_, false, kUnitLiftPixels);
      swirl.delay_ms = delay;
      swirl.offset = {offset_x, kProcellaSwirlUp, offset_z};
      swirls_.push_back(swirl);
    }
    QueueSound(kProcellaSound, 0);
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present;
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    DrawEffectClassStrHost(target_, output);
    DrawAll(swirls_, output);
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    static_cast<void>(AdvanceEffectClassStrHost(target_, tick));
    for (EffectClassStrHost& swirl : swirls_) {
      swirl.position = position_;
    }
    AdvanceOrDrop(swirls_, tick);
  }

 private:
  EffectClassStrHost target_;
  std::vector<EffectClassStrHost> swirls_;
};

// ── Rain of Crystal (1752) ───────────────────────────────────────────────────
//
// UNE PAIRE DE CRISTAUX tous les trois, deux ou un tic selon le niveau, en un
// point du carré autour de l'unité, à la hauteur de l'unité ; une chance sur
// sept de sonner au tic d'après. Un cristal fini se retire.
//
// ⚠ AU NIVEAU ZÉRO, l'étendue se lit dans une case de pile jamais écrite : rien
// ne naît ici.
constexpr std::string_view kCrystalBottom =
    "rain_of_crystal\\rain_of_crystal_attack_bottom\\rain_of_crystal_attack_bottom.str";
constexpr std::string_view kCrystalMain =
    "rain_of_crystal\\rain_of_crystal_attack\\rain_of_crystal_attack.str";
constexpr std::string_view kCrystalSound = "effect\\AG_RAIN_OF_CRYSTAL.wav";
constexpr std::array<std::int32_t, 5> kCrystalSpans = {11, 13, 15, 17, 19};
constexpr std::uint32_t kCrystalLevelCap = 5;
constexpr std::int32_t kCrystalPeriodBase = 4;
constexpr std::int32_t kCrystalSpanUnit = 50;
constexpr float kCrystalSpread = 0.1F;
constexpr std::int32_t kCrystalSoundChance = 7;

class RainOfCrystal final : public EffectClassInstance {
 public:
  explicit RainOfCrystal(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    level_index_ = std::min(static_cast<std::uint32_t>(level_), kCrystalLevelCap);
    period_ = kCrystalPeriodBase - (level_ + 1) / 2;
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present;
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    DrawAll(crystals_, output);
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    if (period_ > 0 && level_index_ > 0 && ticks_ % period_ == 0) {
      const std::int32_t span = kCrystalSpanUnit * kCrystalSpans[level_index_ - 1];
      const float half = static_cast<float>(span) * kHalfSpan;
      glm::vec3 place = position_;
      place.x += (static_cast<float>(std::max(Rand() % (span + 1), 0)) - half) * kCrystalSpread;
      place.z += (static_cast<float>(std::max(Rand() % (span + 1), 0)) - half) * kCrystalSpread;
      crystals_.push_back(UnitHost(kCrystalBottom, place, true, 0.0F));
      crystals_.push_back(UnitHost(kCrystalMain, place, false, 0.0F));
      if (Rand() % kCrystalSoundChance == 0) {
        QueueSound(kCrystalSound, age_ + 1);
      }
    }
    AdvanceOrDrop(crystals_, tick);
    ++ticks_;
  }

 private:
  std::vector<EffectClassStrHost> crystals_;
  std::uint32_t level_index_ = 0;
  std::int32_t period_ = 0;
  std::int32_t ticks_ = 0;
};

}  // namespace

std::unique_ptr<EffectClassInstance> MakeFloralFlareRoad(const EffectClassBirth& birth,
                                                         const EffectClassView& /*view*/) {
  return std::make_unique<FloralFlareRoad>(birth);
}

std::unique_ptr<EffectClassInstance> MakeAstralStrike(const EffectClassBirth& birth,
                                                      const EffectClassView& /*view*/) {
  return std::make_unique<AstralStrike>(birth);
}

std::unique_ptr<EffectClassInstance> MakePneumaticus(const EffectClassBirth& birth,
                                                     const EffectClassView& /*view*/) {
  return std::make_unique<Pneumaticus>(birth);
}

std::unique_ptr<EffectClassInstance> MakeRainOfCrystal(const EffectClassBirth& birth,
                                                       const EffectClassView& /*view*/) {
  return std::make_unique<RainOfCrystal>(birth);
}

}  // namespace ro::render::effect_class
