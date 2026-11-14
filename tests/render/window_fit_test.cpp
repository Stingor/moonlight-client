// LA TAILLE D'UNE FENÊTRE QUI TIENT SUR L'ÉCRAN.
//
// 🔴 CE QUE CE BANC TIENT : une taille qui tient ne bouge pas, une taille qui
// déborde se réduit aux mêmes proportions, rien ne descend à zéro.

#include "ro/render/window_fit.h"

#include <catch2/catch_test_macros.hpp>

using ro::render::FitWindowSize;
using ro::render::Resolution;

namespace {

constexpr Resolution kFullHdRoom{.width = 1920, .height = 1032};
constexpr Resolution kLaptopRoom{.width = 1366, .height = 728};
constexpr Resolution kFactory{.width = 1600, .height = 900};

}  // namespace

TEST_CASE("une taille qui tient revient telle quelle", "[render][window]") {
  CHECK(FitWindowSize(kFactory, kFullHdRoom) == kFactory);
  constexpr Resolution kChosen{.width = 1760, .height = 990};
  CHECK(FitWindowSize(kChosen, kFullHdRoom) == kChosen);
  // Exactement la place : elle tient.
  CHECK(FitWindowSize(kFullHdRoom, kFullHdRoom) == kFullHdRoom);
}

TEST_CASE("une taille qui déborde se réduit aux mêmes proportions", "[render][window]") {
  const Resolution fitted = FitWindowSize(kFactory, kLaptopRoom);
  CHECK(fitted.width <= kLaptopRoom.width);
  CHECK(fitted.height <= kLaptopRoom.height);
  // La hauteur borne : toute la place.
  CHECK(fitted.height == kLaptopRoom.height);
  // Seize neuvièmes, à l'arrondi près.
  CHECK(fitted.width == 1294);
}

TEST_CASE("une demande plus grande ne rend jamais une fenêtre plus petite", "[render][window]") {
  // Un écran 1920×1080 : barre des tâches et bords de la fenêtre ôtés.
  constexpr Resolution kRoomWithoutBorders{.width = 1904, .height = 993};
  constexpr Resolution kFits{.width = 1760, .height = 990};
  constexpr Resolution kOverflows{.width = 1920, .height = 1080};
  CHECK(FitWindowSize(kFits, kRoomWithoutBorders) == kFits);
  const Resolution fitted = FitWindowSize(kOverflows, kRoomWithoutBorders);
  CHECK(fitted.height == kRoomWithoutBorders.height);
  CHECK(fitted.width == 1765);
  CHECK(fitted.width >= kFits.width);
  CHECK(fitted.height >= kFits.height);
}

TEST_CASE("une demande ou une place vide revient telle quelle", "[render][window]") {
  CHECK(FitWindowSize(Resolution{}, kLaptopRoom) == Resolution{});
  CHECK(FitWindowSize(kFactory, Resolution{}) == kFactory);
}

TEST_CASE("rien ne descend sous le plancher, sauf un écran plus petit", "[render][window]") {
  constexpr Resolution kHuge{.width = 8000, .height = 200};
  const Resolution fitted = FitWindowSize(kHuge, kFullHdRoom);
  CHECK(fitted.height == ro::render::kSmallestWindow.height);
  constexpr Resolution kTinyRoom{.width = 640, .height = 400};
  const Resolution tiny = FitWindowSize(kFactory, kTinyRoom);
  CHECK(tiny.width == kTinyRoom.width);
  CHECK(tiny.height == kTinyRoom.height);
}
