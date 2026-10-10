#include "ro/render/map_wind.h"

#include <cmath>
#include <cstdint>
#include <utility>

#include "ro/render/effect_node.h"
#include "ro/render/map_emitter.h"

namespace ro::render {
namespace {

// Le pas angulaire d'un segment : l'arc divisé par VINGT, pour vingt et un
// segments — le premier et le dernier se superposent à 360.
constexpr float kSegmentSteps = 20.0F;
constexpr std::int32_t kDegreesPerTurn = 360;
constexpr float kFullTurnDegrees = 360.0F;
constexpr double kDegreesPerHalfTurn = 180.0;
// Le pi du client, à six décimales.
constexpr double kClientPi = 3.141592;
// La coordonnée de texture le long du ruban : le segment i va de (i−1)/21 à
// i/21. Le dernier vingt et unième de l'image ne sert jamais.
constexpr double kUvSegments = 21.0;

// Les mélanges d'une particule neuve, qu'un réglage de −1 laisse en place.
constexpr std::int32_t kParticleBlendSource = 5;
constexpr std::int32_t kParticleBlendDestination = 2;
constexpr std::int32_t kUnsetBlend = -1;

// Le quad d'un segment, dans l'ordre de la bande du client : bord extérieur
// précédent, bord extérieur, bord intérieur
// précédent, bord intérieur. L'ordonnée de texture vaut zéro à l'extérieur.
// Notre renderer les veut EN TOUR : 0, 1, 3, 2.
constexpr std::array<std::size_t, 4> kStripToCycle = {0, 1, 3, 2};

// Des degrés aux radians, avec les arrondis du client : le quotient repasse par
// un flottant avant d'être multiplié par son pi.
[[nodiscard]] float RadiansOf(float degrees) {
  const auto fraction = static_cast<float>(static_cast<double>(degrees) / kDegreesPerHalfTurn);
  return static_cast<float>(static_cast<double>(fraction) * kClientPi);
}

// La coordonnée de texture d'un bord, divisée en double comme chez le client.
[[nodiscard]] float UvOf(std::int32_t edge) {
  return static_cast<float>(static_cast<double>(edge) / kUvSegments);
}

}  // namespace

MapWind::MapWind(glm::vec3 native_position, MapWindSetting setting, std::uint32_t& draws)
    : position_(setting.position.value_or(native_position)), setting_(std::move(setting)) {
  const float step = static_cast<float>(setting_.full_angle) / kSegmentSteps;
  segments_.reserve(static_cast<std::size_t>(setting_.rings) * kWindSegmentsPerRing);
  for (std::uint16_t ring = 0; ring < setting_.rings; ++ring) {
    // 🔴 L'ANGLE ET LA PHASE SONT TIRÉS PAR ANNEAU, pas par segment : les
    // vingt et un segments d'un anneau respirent ensemble.
    const auto start = static_cast<float>(NextEffectNodeDraw(draws) % kDegreesPerTurn);
    std::int32_t alpha = 0;
    if (setting_.alpha_peak != 0) {
      alpha = static_cast<std::int32_t>(NextEffectNodeDraw(draws) % setting_.alpha_peak);
    }
    // L'étagement : un seul anneau ne monte pas.
    const float lift =
        setting_.rings <= 1
            ? 0.0F
            : setting_.height / static_cast<float>(setting_.rings - 1) * static_cast<float>(ring);
    for (std::size_t index = 0; index < kWindSegmentsPerRing; ++index) {
      Segment segment;
      segment.ring = ring;
      segment.index = static_cast<std::uint16_t>(index);
      segment.angle_degrees = static_cast<float>(index) * step + start;
      segment.lift = lift;
      segment.alpha = alpha;
      segments_.push_back(segment);
    }
  }
}

void MapWind::Tick(glm::vec3 native_player) {
  ticked_ = true;
  hidden_ = kMapEffectReachUnits <= glm::distance(native_player, position_);
  if (hidden_) {
    return;
  }
  // La matrice de l'anneau : la bascule autour de l'abscisse, puis de la
  // profondeur, puis la place. ⚠ DES DEGRÉS EXACTS ici, pas la table entière
  // des émetteurs.
  NativeMatrix matrix = NativeRotationX(RadiansOf(setting_.tilt_x_degrees));
  MultiplyRotationZ(matrix, RadiansOf(setting_.tilt_z_degrees));
  MultiplyTranslation(matrix, position_);

  glm::vec3 last_inner{0.0F, 0.0F, 0.0F};
  glm::vec3 last_outer{0.0F, 0.0F, 0.0F};
  for (Segment& segment : segments_) {
    if (segment.breath == 0) {
      segment.alpha += 1;
      if (setting_.alpha_peak <= segment.alpha) {
        segment.alpha = setting_.alpha_peak;
        segment.breath = 1;
      }
    } else {
      segment.alpha -= 1;
      if (segment.alpha <= 0) {
        segment.alpha = 0;
        segment.breath = 0;
      }
    }
    float angle = setting_.speed + segment.angle_degrees;
    if (angle > kFullTurnDegrees) {
      angle -= kFullTurnDegrees;
    }
    segment.angle_degrees = angle;
    const float cosinus = std::cos(RadiansOf(angle));
    const float sinus = std::sin(RadiansOf(angle));
    const float radius = setting_.radius;
    const float thickness = setting_.thickness;
    const glm::vec3 inner{radius * cosinus, -segment.lift, sinus * radius};
    const glm::vec3 outer{cosinus * thickness + radius * cosinus, thickness - segment.lift,
                          sinus * thickness + sinus * radius};
    // ⚠ LE PREMIER SEGMENT D'UN ANNEAU NE REPREND PAS le bord du précédent : il
    // ne se peint pas, il ne fait que donner le sien au deuxième.
    if (segment.index != 0) {
      segment.previous_outer = last_outer;
      segment.previous_inner = last_inner;
    }
    segment.outer = Transform(outer, matrix);
    segment.inner = Transform(inner, matrix);
    last_inner = segment.inner;
    last_outer = segment.outer;
  }
}

void MapWind::AppendQuads(const MapEffectFrame& frame, std::vector<EffectWorldQuad>& quads) const {
  if (!drawn()) {
    return;
  }
  const std::int32_t source =
      setting_.blend_source != kUnsetBlend ? setting_.blend_source : kParticleBlendSource;
  const std::int32_t destination = setting_.blend_destination != kUnsetBlend
                                       ? setting_.blend_destination
                                       : kParticleBlendDestination;
  for (const Segment& segment : segments_) {
    if (segment.index == 0) {
      continue;
    }
    const float first_u = UvOf(segment.index - 1);
    const float last_u = UvOf(segment.index);
    const std::array<glm::vec3, 4> strip = {segment.previous_outer, segment.outer,
                                            segment.previous_inner, segment.inner};
    const std::array<glm::vec2, 4> strip_uv = {glm::vec2{first_u, 0.0F}, glm::vec2{last_u, 0.0F},
                                               glm::vec2{first_u, 1.0F}, glm::vec2{last_u, 1.0F}};
    EffectWorldQuad quad;
    for (std::size_t corner = 0; corner < kStripToCycle.size(); ++corner) {
      quad.corners[corner] = frame.ToWorld(strip[kStripToCycle[corner]]);
      quad.texture_uv[corner] = strip_uv[kStripToCycle[corner]];
    }
    quad.tint = {setting_.rgb[0], setting_.rgb[1], setting_.rgb[2],
                 static_cast<std::uint8_t>(segment.alpha)};
    quad.blend_source = source;
    quad.blend_destination = destination;
    quads.push_back(quad);
  }
}

}  // namespace ro::render
