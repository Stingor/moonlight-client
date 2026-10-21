#include "ro/mapedit/model_light.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <type_traits>
#include <utility>

namespace ro::mapedit {
namespace {

constexpr std::size_t kAxes = 3;
constexpr std::size_t kAxisY = 1;
using Vector = std::array<float, kAxes>;
// Une matrice rangée par lignes.
using Matrix = std::array<Vector, kAxes>;

// Une échelle plus petite que cela, en valeur absolue, écrase son axe.
constexpr float kFlatScale = 1.0e-6F;

constexpr float kRadiansPerDegree = std::numbers::pi_v<float> / 180.0F;

Matrix Multiply(const Matrix& left, const Matrix& right) {
  Matrix product{};
  for (std::size_t row = 0; row < kAxes; ++row) {
    for (std::size_t column = 0; column < kAxes; ++column) {
      for (std::size_t inner = 0; inner < kAxes; ++inner) {
        product[row][column] += left[row][inner] * right[inner][column];
      }
    }
  }
  return product;
}

Vector Apply(const Matrix& matrix, const Vector& vector) {
  Vector result{};
  for (std::size_t row = 0; row < kAxes; ++row) {
    for (std::size_t inner = 0; inner < kAxes; ++inner) {
      result[row] += matrix[row][inner] * vector[inner];
    }
  }
  return result;
}

// Par la transposée : l'inverse d'une rotation.
Vector ApplyTransposed(const Matrix& matrix, const Vector& vector) {
  Vector result{};
  for (std::size_t row = 0; row < kAxes; ++row) {
    for (std::size_t inner = 0; inner < kAxes; ++inner) {
      result[row] += matrix[inner][row] * vector[inner];
    }
  }
  return result;
}

Matrix AboutX(float radians) {
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  return {{{1.0F, 0.0F, 0.0F}, {0.0F, cosine, -sine}, {0.0F, sine, cosine}}};
}

Matrix AboutY(float radians) {
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  return {{{cosine, 0.0F, sine}, {0.0F, 1.0F, 0.0F}, {-sine, 0.0F, cosine}}};
}

Matrix AboutZ(float radians) {
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  return {{{cosine, -sine, 0.0F}, {sine, cosine, 0.0F}, {0.0F, 0.0F, 1.0F}}};
}

// LA ROTATION DE LA POSE, DANS LE REPÈRE DU RSW. Le décor tourne une pose,
// dans le repère du monde, autour de y d'abord, puis de x, puis de z, les deux
// derniers angles comptés à rebours (`ModelInstanceMatrix`). Le repère du RSW
// a l'axe y retourné par rapport à celui du monde, où la rotation s'écrit sur
// des vecteurs retournés de même : elle s'y conjugue par ce retournement.
Matrix PoseRotation(const formats::RswModel& placement) {
  const Matrix in_world = Multiply(Multiply(AboutZ(-placement.rotation[2] * kRadiansPerDegree),
                                            AboutX(-placement.rotation[0] * kRadiansPerDegree)),
                                   AboutY(placement.rotation[1] * kRadiansPerDegree));
  Matrix in_rsw = in_world;
  for (std::size_t row = 0; row < kAxes; ++row) {
    for (std::size_t column = 0; column < kAxes; ++column) {
      // Le retournement de y, de part et d'autre : le signe change quand une
      // seule des deux, ligne ou colonne, est y.
      if ((row == kAxisY) != (column == kAxisY)) {
        in_rsw[row][column] = -in_world[row][column];
      }
    }
  }
  return in_rsw;
}

}  // namespace

std::string ModelFileKey(std::string_view model_file) {
  std::string key(model_file);
  for (char& letter : key) {
    if (letter >= 'A' && letter <= 'Z') {
      letter = static_cast<char>(letter - 'A' + 'a');
    } else if (letter == '/') {
      letter = '\\';
    }
  }
  return key;
}

std::array<float, 3> ModelLightPosition(const formats::RswModel& placement,
                                        const std::array<float, 3>& offset) {
  const Vector scaled{offset[0] * placement.scale[0], offset[1] * placement.scale[1],
                      offset[2] * placement.scale[2]};
  const Vector turned = Apply(PoseRotation(placement), scaled);
  return {placement.position[0] + turned[0], placement.position[1] + turned[1],
          placement.position[2] + turned[2]};
}

std::array<float, 3> ModelLightOffset(const formats::RswModel& placement,
                                      const std::array<float, 3>& position) {
  const Vector apart{position[0] - placement.position[0], position[1] - placement.position[1],
                     position[2] - placement.position[2]};
  const Vector unturned = ApplyTransposed(PoseRotation(placement), apart);
  Vector offset{};
  for (std::size_t axis = 0; axis < kAxes; ++axis) {
    offset[axis] = std::abs(placement.scale[axis]) < kFlatScale
                       ? 0.0F
                       : unturned[axis] / placement.scale[axis];
  }
  return offset;
}

namespace {

// CE QUE PORTENT LES POSES DE LA CARTE : pour chaque pose ni masquée ni
// éteinte, chaque entrée `carried` de même fichier, `held` en tirant l'objet —
// lumière ou effet —, posé dans le repère du RSW.
template <typename Carried, typename Held>
auto CarriedOn(const std::vector<Carried>& carried, Held held,
               std::span<const formats::RswModel> models, std::span<const std::string> model_keys,
               std::span<const std::size_t> hidden, std::span<const std::size_t> unlit) {
  using Object = std::remove_cvref_t<decltype(carried.front().*held)>;
  std::vector<Object> placed;
  if (carried.empty()) {
    return placed;
  }
  std::vector<std::string> book_keys;
  book_keys.reserve(carried.size());
  for (const Carried& entry : carried) {
    book_keys.push_back(ModelFileKey(entry.model_file));
  }
  const std::size_t count = std::min(models.size(), model_keys.size());
  for (std::size_t rank = 0; rank < count; ++rank) {
    if (std::ranges::binary_search(hidden, rank) || std::ranges::binary_search(unlit, rank)) {
      continue;
    }
    for (std::size_t slot = 0; slot < book_keys.size(); ++slot) {
      if (book_keys[slot] != model_keys[rank]) {
        continue;
      }
      Object object = carried[slot].*held;
      object.position = ModelLightPosition(models[rank], object.position);
      placed.push_back(std::move(object));
    }
  }
  return placed;
}

}  // namespace

std::vector<AddedLight> ModelLightsOn(const MapEdit& book,
                                      std::span<const formats::RswModel> models,
                                      std::span<const std::string> model_keys,
                                      std::span<const std::size_t> hidden,
                                      std::span<const std::size_t> unlit) {
  return CarriedOn(book.model_lights(), &ModelLight::light, models, model_keys, hidden, unlit);
}

std::vector<AddedEffect> ModelEffectsOn(const MapEdit& book,
                                        std::span<const formats::RswModel> models,
                                        std::span<const std::string> model_keys,
                                        std::span<const std::size_t> hidden,
                                        std::span<const std::size_t> unlit) {
  return CarriedOn(book.model_effects(), &ModelEffect::effect, models, model_keys, hidden, unlit);
}

}  // namespace ro::mapedit
