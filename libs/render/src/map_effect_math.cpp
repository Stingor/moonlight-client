#include "ro/render/map_effect_math.h"

#include <cmath>
#include <cstdlib>

#include "ro/render/effect_node.h"

namespace ro::render {
namespace {

// Les conventions de la place d'un modèle : un demi-plateau par case du sol,
// et un retrait constant de la profondeur.
constexpr float kHalfCube = 5.0F;
constexpr float kDepthBias = 10.0F;

// La table du client compte un tour ENTIER plus un : de zéro à 360 inclus.
constexpr std::int32_t kDegreesPerTurn = 360;
constexpr std::int32_t kDegreeTableLength = kDegreesPerTurn + 1;
constexpr double kDegreesPerHalfTurn = 180.0;
// ⚠ LE PI DU CLIENT, à six décimales : c'est lui qui remplit la table.
constexpr double kClientPi = 3.141592;

// La grille du tirage : un dixième.
constexpr double kDrawGrid = 10.0;
constexpr double kDrawStep = 0.1;

}  // namespace

NativeMatrix NativeScale(float x, float y, float z) {
  NativeMatrix matrix = kNativeIdentity;
  matrix[0] = x;
  matrix[4] = y;
  matrix[8] = z;
  return matrix;
}

NativeMatrix NativeRotationX(float radians) {
  NativeMatrix matrix = kNativeIdentity;
  const float cosinus = std::cos(radians);
  const float sinus = std::sin(radians);
  matrix[4] = cosinus;
  matrix[5] = sinus;
  matrix[7] = -sinus;
  matrix[8] = cosinus;
  return matrix;
}

NativeMatrix Multiply(const NativeMatrix& first, const NativeMatrix& then) {
  NativeMatrix out{};
  for (std::size_t row = 0; row < 4; ++row) {
    for (std::size_t column = 0; column < 3; ++column) {
      out[row * 3 + column] = first[row * 3] * then[column] +
                              first[row * 3 + 1] * then[3 + column] +
                              first[row * 3 + 2] * then[6 + column];
    }
  }
  // La ligne de translation reçoit en plus celle du second facteur.
  out[9] += then[9];
  out[10] += then[10];
  out[11] += then[11];
  return out;
}

void MultiplyRotationX(NativeMatrix& matrix, float radians) {
  const float cosinus = std::cos(radians);
  const float sinus = std::sin(radians);
  NativeMatrix rotation = kNativeIdentity;
  rotation[4] = cosinus;
  rotation[5] = sinus;
  rotation[7] = -sinus;
  rotation[8] = cosinus;
  matrix = Multiply(matrix, rotation);
}

void MultiplyRotationY(NativeMatrix& matrix, float radians) {
  const float cosinus = std::cos(radians);
  const float sinus = std::sin(radians);
  NativeMatrix rotation = kNativeIdentity;
  rotation[0] = cosinus;
  rotation[2] = -sinus;
  rotation[6] = sinus;
  rotation[8] = cosinus;
  matrix = Multiply(matrix, rotation);
}

void MultiplyRotationZ(NativeMatrix& matrix, float radians) {
  const float cosinus = std::cos(radians);
  const float sinus = std::sin(radians);
  NativeMatrix rotation = kNativeIdentity;
  rotation[0] = cosinus;
  rotation[1] = sinus;
  rotation[3] = -sinus;
  rotation[4] = cosinus;
  matrix = Multiply(matrix, rotation);
}

void MultiplyTranslation(NativeMatrix& matrix, glm::vec3 offset) {
  matrix[9] += offset.x;
  matrix[10] += offset.y;
  matrix[11] += offset.z;
}

glm::vec3 Transform(glm::vec3 point, const NativeMatrix& matrix) {
  return {point.x * matrix[0] + point.y * matrix[3] + point.z * matrix[6] + matrix[9],
          point.x * matrix[1] + point.y * matrix[4] + point.z * matrix[7] + matrix[10],
          point.x * matrix[2] + point.y * matrix[5] + point.z * matrix[8] + matrix[11]};
}

float NativeTableRadians(std::int32_t degrees) {
  std::int32_t reduced = degrees;
  if (degrees > kDegreesPerTurn || degrees < -kDegreesPerTurn) {
    reduced = degrees % kDegreesPerTurn;
  }
  // La lecture À REBOURS des négatifs : voir l'en-tête.
  const std::int32_t entry = reduced >= 0 ? reduced : kDegreeTableLength + reduced;
  const auto fraction =
      static_cast<float>(static_cast<double>(static_cast<float>(entry)) / kDegreesPerHalfTurn);
  return static_cast<float>(static_cast<double>(fraction) * kClientPi);
}

float EffectToolRandomBetween(std::uint32_t& draws, float low, float high) {
  if (low == high) {
    return low;
  }
  // ⚠ LE PASSAGE PAR UN FLOTTANT COMPTE : 0,7 donne 6,99999988 en double, que
  // le flottant arrondit à 7 avant que l'entier ne tronque.
  const auto start =
      static_cast<std::int32_t>(static_cast<float>(static_cast<double>(low) * kDrawGrid));
  const auto negated_end =
      static_cast<std::int32_t>(static_cast<float>(static_cast<double>(high) * -kDrawGrid));
  const std::int32_t span = std::abs(-start - negated_end);
  const auto draw =
      static_cast<std::int32_t>(NextEffectNodeDraw(draws) % static_cast<std::uint32_t>(span + 1));
  return static_cast<float>(static_cast<double>(static_cast<float>(start + draw)) * kDrawStep);
}

glm::vec3 MapEffectFrame::ToWorld(glm::vec3 native) const {
  return {kHalfCube * static_cast<float>(gnd_width) + native.x, -native.y,
          kDepthBias + kHalfCube * static_cast<float>(gnd_height) - native.z};
}

glm::vec3 MapEffectFrame::ToNative(glm::vec3 world) const {
  return {world.x - kHalfCube * static_cast<float>(gnd_width), -world.y,
          kDepthBias + kHalfCube * static_cast<float>(gnd_height) - world.z};
}

}  // namespace ro::render
