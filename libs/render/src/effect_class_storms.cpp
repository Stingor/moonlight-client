// LES TEMPÊTES QU'UNE COMPÉTENCE FAIT NAÎTRE : Gale Storm et Jupitel Thunder
// Storm — des `.str` semés au fil des tics autour d'un acteur.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "effect_class_internal.h"

namespace ro::render::effect_class {
namespace {

// Le centre d'un tirage entier : la moitié de son étendue.
constexpr float kHalfSpread = 0.5F;

EffectClassStrHost StormHost(std::string_view path, glm::vec3 position) {
  EffectClassStrHost host;
  host.path = std::string(path);
  host.position = position;
  return host;
}

void DrawAll(const std::vector<EffectClassStrHost>& hosts, EffectClassOutput& output) {
  for (const EffectClassStrHost& host : hosts) {
    DrawEffectClassStrHost(host, output);
  }
}

// ── Gale Storm (1973) ────────────────────────────────────────────────────────
//
// SUR LA CIBLE, DES RAFALES ET DES FLÈCHES pendant soixante et un tics : une
// rafale tous les 100/n tics, une flèche tous les 30/n, chacune posée au hasard
// dans le carré de 5n unités de côté centré sur la cible — n vaut 5 aux niveaux
// un à quatre, 7 de cinq à sept, 9 aux huitième et neuvième, 11 au dixième, et
// un niveau au-delà, ou absent, prend celui du dixième. Une flèche sur sept
// fait souffler le vent au tic suivant. Chaque `.str` joue une fois puis s'en
// va ; l'instance meurt quand les semis ont cessé et que le dernier a fini, ou
// dès que la cible s'en va.
//
// ⚠ AU NIVEAU ZÉRO, le client lit une case de pile jamais écrite : rien ne
// naît ici.
constexpr std::string_view kGaleStormPath = "galestorm\\galestorm\\galestorm.str";
constexpr std::string_view kGaleStormArrowPath = "galestorm\\galestorm_arrow\\galestorm_arrow.str";
constexpr std::string_view kGaleStormSound = "effect\\WH_GALESTORM.wav";
constexpr std::array<std::int32_t, 10> kGaleStormSizes = {5, 5, 5, 5, 7, 7, 7, 9, 9, 11};
constexpr std::uint32_t kGaleStormLevelCap = 10;
constexpr std::int32_t kGaleStormSowUntilAge = 60;
constexpr std::int32_t kGaleStormSpreadPerSize = 5;
constexpr std::int32_t kGaleStormPeriodBase = 100;
constexpr std::int32_t kGaleStormArrowPeriodBase = 30;
constexpr std::int32_t kGaleStormSoundChance = 7;

class GaleStorm final : public EffectClassInstance {
 public:
  explicit GaleStorm(const EffectClassBirth& birth) : EffectClassInstance(birth) {
    // Le niveau se borne SANS SIGNE : un niveau absent, moins un, prend le
    // dixième.
    const std::uint32_t level = std::min(static_cast<std::uint32_t>(level_), kGaleStormLevelCap);
    if (level > 0) {
      size_ = kGaleStormSizes[level - 1];
    }
  }
  [[nodiscard]] bool Alive(const EffectClassTick& tick) const override {
    return tick.owner_present && (age_ <= kGaleStormSowUntilAge || !hosts_.empty());
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    DrawAll(hosts_, output);
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    if (age_ <= kGaleStormSowUntilAge && size_ > 0) {
      if (age_ % (kGaleStormPeriodBase / size_) == 0) {
        hosts_.push_back(StormHost(kGaleStormPath, Scattered()));
      }
      if (age_ % (kGaleStormArrowPeriodBase / size_) == 0) {
        hosts_.push_back(StormHost(kGaleStormArrowPath, Scattered()));
        if (Rand() % kGaleStormSoundChance == 0) {
          QueueSound(kGaleStormSound, age_ + 1);
        }
      }
    }
    // Un hôte dont l'image est jouée s'en va ; les autres avancent.
    std::erase_if(hosts_, [](const EffectClassStrHost& host) { return host.finished; });
    for (EffectClassStrHost& host : hosts_) {
      static_cast<void>(AdvanceEffectClassStrHost(host, tick));
    }
  }

 private:
  // Un point du carré autour de la cible : l'abscisse tirée d'abord, puis la
  // profondeur, chacune de zéro à 5n compris, recentrée.
  glm::vec3 Scattered() {
    const std::int32_t spread = kGaleStormSpreadPerSize * size_;
    const float half = static_cast<float>(spread) * kHalfSpread;
    glm::vec3 place = position_;
    place.x += static_cast<float>(std::max(Rand() % (spread + 1), 0)) - half;
    place.z += static_cast<float>(std::max(Rand() % (spread + 1), 0)) - half;
    return place;
  }

  std::vector<EffectClassStrHost> hosts_;
  std::int32_t size_ = 0;
};

// ── Jupitel Thunder Storm (2366) ─────────────────────────────────────────────
//
// UNE BOULE QUI FILE DU LANCEUR À LA CIBLE en dix tics, tournée à l'écran vers
// elle, puis qui reste là où la cible était au lancement tant que son image
// joue ; et une pluie d'éclairs autour de cette place, quinze unités au-dessus
// de son sol — la cible et le lanceur n'en sont lus qu'à la naissance : un toutes
// les quatre-vingts millisecondes jusqu'à 150 × ((niveau − 1) / 2 + 6)
// millisecondes, chacun dans l'un des quatre quarts, tour à tour, à une
// distance tirée de zéro à (10 × ((niveau − 1) / 4) + 25) / 2 par côté, d'une
// des trois images tirées au sort ; le tonnerre gronde avec un éclair sur
// quatre. L'instance meurt quand la boule et tous les éclairs ont fini, que le
// lanceur soit encore là ou non.
constexpr std::string_view kJupitelBallPath =
    "HYPER_NOVICE\\HN_JUPITEL_THUNDER_STORM\\jupitel_thunder_storm_ball\\"
    "jupitel_thunder_storm_ball.str";
constexpr std::string_view kJupitelStrikeFolder =
    "HYPER_NOVICE\\HN_JUPITEL_THUNDER_STORM\\jupitel_thunder_storm\\jupitel_thunder_storm_";
constexpr std::string_view kJupitelSound = "effect\\HYPER_NOVICE\\hu_jupitel_thunder_storm.wav";
constexpr std::int32_t kJupitelTravelTicks = 10;
constexpr std::int32_t kJupitelStrikeStepMs = 80;
constexpr std::int32_t kJupitelStrikeSpanUnitMs = 150;
constexpr std::int32_t kJupitelStrikeSpanBase = 6;
constexpr std::int32_t kJupitelLevelsPerHalf = 2;
constexpr std::int32_t kJupitelLevelsPerQuarter = 4;
constexpr std::int32_t kJupitelSpreadPerQuarter = 10;
constexpr std::int32_t kJupitelSpreadBase = 25;
constexpr float kJupitelStrikeUp = -15.0F;
constexpr std::int32_t kJupitelVariants = 3;
constexpr std::int32_t kJupitelSoundEvery = 4;
// Les quatre quarts, dans l'ordre où les éclairs les prennent : le signe de
// l'abscisse, puis celui de la profondeur.
constexpr std::array<std::array<float, 2>, 4> kJupitelQuarters = {
    {{-1.0F, 1.0F}, {1.0F, 1.0F}, {-1.0F, -1.0F}, {1.0F, -1.0F}}};

// Le fichier d'une des trois images d'éclair, par son numéro sur deux chiffres.
std::string StrikePath(std::int32_t variant) {
  constexpr std::size_t kSuffixRoom = 16;
  std::array<char, kSuffixRoom> suffix{};
  static_cast<void>(std::snprintf(suffix.data(), suffix.size(), "%02d.str", variant));
  return std::string(kJupitelStrikeFolder) + suffix.data();
}

class JupitelThunderStorm final : public EffectClassInstance {
 public:
  JupitelThunderStorm(const EffectClassBirth& birth, const EffectClassView& view)
      : EffectClassInstance(birth), target_(bound_position_.value_or(position_)) {
    const glm::vec3 travel = target_ - position_;
    const float distance = std::sqrt(glm::dot(travel, travel));
    if (distance > 0.0F) {
      velocity_ = travel / distance * (distance / static_cast<float>(kJupitelTravelTicks));
    }
    ball_ = StormHost(kJupitelBallPath, position_);
    ball_->spin_degrees = ScreenAngleDegrees(view, position_, target_).value_or(0.0F);
    const std::int32_t quarter = (level_ - 1) / kJupitelLevelsPerQuarter;
    const std::int32_t total_ms =
        kJupitelStrikeSpanUnitMs * ((level_ - 1) / kJupitelLevelsPerHalf + kJupitelStrikeSpanBase);
    const std::int32_t spread = (kJupitelSpreadPerQuarter * quarter + kJupitelSpreadBase) / 2;
    std::int32_t rank = 0;
    for (std::int32_t delay_ms = kJupitelStrikeStepMs; delay_ms <= total_ms;
         delay_ms += kJupitelStrikeStepMs, ++rank) {
      const std::array<float, 2>& signs = kJupitelQuarters[static_cast<std::size_t>(
          rank % static_cast<std::int32_t>(kJupitelQuarters.size()))];
      // La profondeur se tire avant l'abscisse ; sans étendue, chacune vaut un
      // demi.
      std::int32_t depth_draw = 0;
      std::int32_t across_draw = 0;
      if (spread > 0) {
        depth_draw = std::max(Rand() % spread, 0);
        across_draw = std::max(Rand() % spread, 0);
      }
      const std::int32_t variant = std::max(Rand() % kJupitelVariants, 0);
      EffectClassStrHost strike =
          StormHost(StrikePath(variant),
                    target_ + glm::vec3{signs[0] * (static_cast<float>(across_draw) + kHalfSpread),
                                        kJupitelStrikeUp,
                                        signs[1] * (static_cast<float>(depth_draw) + kHalfSpread)});
      strike.delay_ms = delay_ms;
      strikes_.push_back(std::move(strike));
      if (rank % kJupitelSoundEvery == 0) {
        QueueSound(kJupitelSound, delay_ms / kTickMs + 1);
      }
    }
  }
  [[nodiscard]] bool Alive(const EffectClassTick& /*tick*/) const override {
    return (ball_.has_value() && !ball_->finished) ||
           std::ranges::any_of(strikes_,
                               [](const EffectClassStrHost& strike) { return !strike.finished; });
  }
  void Draw(const EffectClassView& /*view*/, EffectClassOutput& output) const override {
    if (ball_.has_value()) {
      DrawEffectClassStrHost(*ball_, output);
    }
    DrawAll(strikes_, output);
  }

 protected:
  void Update(const EffectClassTick& tick, EffectClassOutput& /*output*/) override {
    if (ball_.has_value()) {
      if (ball_->finished) {
        ball_.reset();
      } else {
        // Dix tics de vol, puis la boule se tient là où la cible ÉTAIT au
        // lancement : le gestionnaire ne suit que le porteur, pas l'acteur lié.
        if (age_ >= kJupitelTravelTicks) {
          ball_->position = target_;
        } else {
          ball_->position += velocity_;
        }
        static_cast<void>(AdvanceEffectClassStrHost(*ball_, tick));
      }
    }
    // Un éclair fini garde sa dernière image jusqu'à la mort de l'instance.
    for (EffectClassStrHost& strike : strikes_) {
      static_cast<void>(AdvanceEffectClassStrHost(strike, tick));
    }
  }

 private:
  glm::vec3 target_;
  glm::vec3 velocity_{0.0F};
  std::optional<EffectClassStrHost> ball_;
  std::vector<EffectClassStrHost> strikes_;
};

}  // namespace

std::unique_ptr<EffectClassInstance> MakeGaleStorm(const EffectClassBirth& birth,
                                                   const EffectClassView& /*view*/) {
  return std::make_unique<GaleStorm>(birth);
}

std::unique_ptr<EffectClassInstance> MakeJupitelThunderStorm(const EffectClassBirth& birth,
                                                             const EffectClassView& view) {
  return std::make_unique<JupitelThunderStorm>(birth, view);
}

}  // namespace ro::render::effect_class
