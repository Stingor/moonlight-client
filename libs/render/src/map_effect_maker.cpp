#include "ro/render/map_effect_maker.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>

#include "ro/render/effect_node.h"
#include "ro/render/map_emitter.h"

namespace ro::render {
namespace {

// Le retard de départ d'un semeur ordinaire : `vitesse - rand() % 24`.
constexpr std::int32_t kStartLagSpread = 24;
// Les trois diviseurs de la cadence des lucioles.
constexpr float kFireflyJitterDivisor = 10.0F;
constexpr float kFireflyPeriodDivisor = 20.0F;
constexpr float kFireflyCounterBias = 1.0F;

// `rand() % borne` du client, avec le zéro qu'il rend quand la borne est nulle.
//
// ⚠ ET IL NE TIRE PAS DU TOUT dans ce cas : le `rand()` est à l'intérieur du
// `if`. Tirer quand même décalerait tout le reste de la suite partagée.
[[nodiscard]] std::int32_t DrawBelow(std::uint32_t& draws, std::int32_t bound) {
  if (bound == 0) {
    return 0;
  }
  const auto drawn = static_cast<std::int32_t>(NextEffectNodeDraw(draws)) % bound;
  return drawn >= 0 ? drawn : 0;
}

}  // namespace

std::uint32_t NativeFloatToUnsigned(float value) {
  // La troncature signée du client déborde au-delà de l'étendue d'un entier ;
  // les cadences d'une carte sont bien en deçà, mais un flottant lu dans un
  // fichier n'est pas une valeur que l'on contrôle.
  constexpr auto kLowest = static_cast<float>(std::numeric_limits<std::int32_t>::min());
  constexpr auto kHighest = static_cast<float>(std::numeric_limits<std::int32_t>::max());
  if (!(value > kLowest)) {
    return static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::min());
  }
  if (value >= kHighest) {
    return static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
  }
  return static_cast<std::uint32_t>(static_cast<std::int32_t>(value));
}

MapEffectMaker::MapEffectMaker(glm::vec3 native_position, std::int32_t effect_id, float emit_speed,
                               std::array<float, 4> param, std::uint32_t& draws)
    : position_(native_position), effect_id_(effect_id), param_(param) {
  // La mise en place d'un semeur à partir de son objet du RSW.
  if (effect_id_ == kMapFireflyEffect) {
    // 🔴 LES LUCIOLES ONT LEUR PROPRE CADENCE, et c'est le seul numéro pour
    // lequel le client la calcule autrement : une période bruitée vers le HAUT
    // d'un dixième de la vitesse, puis raccourcie d'un vingtième — et un départ
    // tiré dans toute la période, non pas dans les vingt-quatre derniers pas.
    // Deux semeurs voisins ne battent donc jamais ensemble.
    const auto jitter = static_cast<std::int32_t>(emit_speed / kFireflyJitterDivisor);
    const auto drawn = static_cast<float>(DrawBelow(draws, jitter));
    period_ = NativeFloatToUnsigned(drawn + emit_speed - emit_speed / kFireflyPeriodDivisor);
    const auto span = static_cast<std::int32_t>(emit_speed + kFireflyCounterBias);
    counter_ = static_cast<std::uint32_t>(DrawBelow(draws, span));
    return;
  }
  period_ = NativeFloatToUnsigned(emit_speed);
  counter_ =
      NativeFloatToUnsigned(emit_speed - static_cast<float>(DrawBelow(draws, kStartLagSpread)));
}

bool MapEffectMaker::Tick(glm::vec3 native_player) {
  // ⚠ LA DISTANCE EST PLANE, en X et Z seulement, et non celle
  // d'une cheminée : la hauteur ne compte pas. Un semeur au sommet d'une tour
  // est donc à portée de qui passe à ses pieds.
  const float along_x = native_player.x - position_.x;
  const float along_z = native_player.z - position_.z;
  const float planar = std::sqrt(along_x * along_x + along_z * along_z);

  bool sows = false;
  if (planar < kMapEffectReachUnits && counter_ >= period_) {
    counter_ = 0;
    sows = true;
  }
  ++counter_;
  return sows;
}

}  // namespace ro::render
