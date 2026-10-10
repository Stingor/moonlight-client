// COMMENT L'AFFICHAGE S'ÉCRIT DANS LE FICHIER, ET COMMENT IL SE RELIT.
//
// ⚠ RIEN D'AUTRE ICI : que des chaînes et des nombres. C'est ce qui permet à un
// essai d'atteindre ces quatre aides sans monter le jeu entier — et l'aller-
// retour « je l'écris, je le relis » est précisément ce qui se prouve hors jeu,
// là où tout le reste de l'affichage demande un écran.

#include "options/display_options.h"

#include <charconv>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

namespace moonclient {
std::string PlaceText(ro::render::WindowPlace where) {
  return std::to_string(where.x) + "," + std::to_string(where.y);
}

std::optional<ro::render::WindowPlace> PlaceOf(std::string_view text) {
  const std::size_t comma = text.find(',');
  if (comma == std::string_view::npos) {
    return std::nullopt;
  }
  ro::render::WindowPlace where;
  const auto* const begin = text.data();
  // ⚠ `from_chars` LIT LE SIGNE pour un entier : une abscisse négative est une
  // place valable, pas une saisie fautive.
  if (std::from_chars(begin, begin + comma, where.x).ec != std::errc{}) {
    return std::nullopt;
  }
  if (std::from_chars(begin + comma + 1, begin + text.size(), where.y).ec != std::errc{}) {
    return std::nullopt;
  }
  return where;
}

std::string ResolutionText(ro::render::Resolution size) {
  return std::to_string(size.width) + "x" + std::to_string(size.height);
}

ro::render::Resolution ResolutionOf(std::string_view text) {
  const std::size_t cross = text.find('x');
  if (cross == std::string_view::npos) {
    return {};
  }
  ro::render::Resolution size;
  const auto* const begin = text.data();
  if (std::from_chars(begin, begin + cross, size.width).ec != std::errc{}) {
    return {};
  }
  if (std::from_chars(begin + cross + 1, begin + text.size(), size.height).ec != std::errc{}) {
    return {};
  }
  return size.width > 0 && size.height > 0 ? size : ro::render::Resolution{};
}

ro::render::DisplayMode DisplayModeOf(int rank) {
  switch (rank) {
    case 1:
      return ro::render::DisplayMode::kBorderless;
    case 2:
      return ro::render::DisplayMode::kFullscreen;
    default:
      return ro::render::DisplayMode::kWindowed;
  }
}

namespace {
// Cent pour cent, écrit une fois.
constexpr float kWholePercent = 100.0F;
// Ce qu'on ajoute avant de tronquer pour obtenir un arrondi au plus proche.
constexpr float kHalf = 0.5F;
}  // namespace

float InterfaceScaleOf(int percent) { return static_cast<float>(percent) / kWholePercent; }

int InterfacePercentOf(float scale) {
  // ⚠ ARRONDI ET NON TRONCATURE : 1,499 vaut 150 % et non 149 %. Une troncature
  // ferait manquer son palier à qui vient de le choisir, d'un pour cent.
  //
  // ⚠ ET LES NÉGATIFS NE SE PRÉSENTENT PAS : l'échelle est bornée bien au-dessus
  // de zéro par le moteur. Ajouter un demi suffit donc.
  return static_cast<int>(scale * kWholePercent + kHalf);
}

std::string InterfaceScaleLabel(int percent) {
  // U+00A0, l'espace insécable, en UTF-8.
  return std::to_string(percent) + " %";
}

}  // namespace moonclient
