// LES TOUCHES DU MODE SURVIVORS : le cap de chaque combinaison, la souris qui
// prime, les lettres de l'aide, la garde de 400 ms.

#include "survivors/survivors_keys.h"

#include <cstdint>
#include <initializer_list>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/survivors_geometry.h"

using moonclient::HeadingOf;
using moonclient::InputGuard;
using moonclient::kInputGuardMs;
using moonclient::kScancodeA;
using moonclient::kScancodeD;
using moonclient::kScancodeDown;
using moonclient::kScancodeLeft;
using moonclient::kScancodeRight;
using moonclient::kScancodeS;
using moonclient::kScancodeUp;
using moonclient::kScancodeW;
using moonclient::MouseAim;
using moonclient::MoveKeys;
using ro::sim::kNoHeading;

namespace {

// Les caps de la simulation : 0 au nord, sens trigonométrique.
constexpr std::uint8_t kNorth = 0;
constexpr std::uint8_t kNorthWest = 8;
constexpr std::uint8_t kWest = 16;
constexpr std::uint8_t kSouthWest = 24;
constexpr std::uint8_t kSouth = 32;
constexpr std::uint8_t kSouthEast = 40;
constexpr std::uint8_t kEast = 48;
constexpr std::uint8_t kNorthEast = 56;
// Un scancode étranger à la marche : la touche Q physique.
constexpr std::uint32_t kScancodeQ = 20;
constexpr std::uint32_t kScancodeEscape = 41;

[[nodiscard]] std::uint8_t HeadingHolding(std::initializer_list<std::uint32_t> scancodes) {
  MoveKeys keys;
  for (const std::uint32_t scancode : scancodes) keys.Take(scancode, true);
  return keys.Heading();
}

}  // namespace

TEST_CASE("MoveKeys : chaque combinaison donne son cap", "[app][survivors]") {
  CHECK(HeadingHolding({}) == kNoHeading);
  CHECK(HeadingHolding({kScancodeW}) == kNorth);
  CHECK(HeadingHolding({kScancodeA}) == kWest);
  CHECK(HeadingHolding({kScancodeS}) == kSouth);
  CHECK(HeadingHolding({kScancodeD}) == kEast);
  CHECK(HeadingHolding({kScancodeW, kScancodeA}) == kNorthWest);
  CHECK(HeadingHolding({kScancodeS, kScancodeA}) == kSouthWest);
  CHECK(HeadingHolding({kScancodeS, kScancodeD}) == kSouthEast);
  CHECK(HeadingHolding({kScancodeW, kScancodeD}) == kNorthEast);
  // Les flèches valent les lettres, et se mêlent à elles.
  CHECK(HeadingHolding({kScancodeUp}) == kNorth);
  CHECK(HeadingHolding({kScancodeLeft}) == kWest);
  CHECK(HeadingHolding({kScancodeDown}) == kSouth);
  CHECK(HeadingHolding({kScancodeRight}) == kEast);
  CHECK(HeadingHolding({kScancodeUp, kScancodeD}) == kNorthEast);
}

TEST_CASE("MoveKeys : des touches opposées arrêtent leur axe", "[app][survivors]") {
  CHECK(HeadingHolding({kScancodeW, kScancodeS}) == kNoHeading);
  CHECK(HeadingHolding({kScancodeA, kScancodeD}) == kNoHeading);
  CHECK(HeadingHolding({kScancodeW, kScancodeS, kScancodeA, kScancodeD}) == kNoHeading);
  CHECK(HeadingHolding({kScancodeW, kScancodeS, kScancodeA}) == kWest);
  CHECK(HeadingHolding({kScancodeUp, kScancodeDown}) == kNoHeading);
}

TEST_CASE("MoveKeys : relâcher, tenir deux fois une direction, perdre le focus",
          "[app][survivors]") {
  MoveKeys keys;
  CHECK(keys.Take(kScancodeW, true));
  CHECK(keys.Take(kScancodeUp, true));
  CHECK(keys.Take(kScancodeW, false));
  // La flèche tient encore le nord.
  CHECK(keys.Heading() == kNorth);
  CHECK(keys.Take(kScancodeUp, false));
  CHECK(keys.Heading() == kNoHeading);
  // Une touche étrangère n'est pas prise.
  CHECK_FALSE(keys.Take(kScancodeQ, true));
  CHECK_FALSE(keys.Take(kScancodeEscape, true));
  keys.Take(kScancodeD, true);
  keys.ReleaseAll();
  CHECK(keys.Heading() == kNoHeading);
}

TEST_CASE("HeadingOf : la souris prime tant qu'elle est tenue", "[app][survivors]") {
  MoveKeys keys;
  keys.Take(kScancodeW, true);
  const ro::sim::FieldPoint hero{ro::sim::Cells(10), ro::sim::Cells(10)};
  const ro::sim::FieldPoint east_of_hero{ro::sim::Cells(14), ro::sim::Cells(10)};
  CHECK(HeadingOf(keys, MouseAim{.held = true, .hero = hero, .target = east_of_hero}, 0.0F) ==
        kEast);
  // Relâchée, les touches reprennent la main.
  CHECK(HeadingOf(keys, MouseAim{.held = false, .hero = hero, .target = east_of_hero}, 0.0F) ==
        kNorth);
  // Sur le héros lui-même, la souris tenue arrête la marche.
  CHECK(HeadingOf(keys, MouseAim{.held = true, .hero = hero, .target = hero}, 0.0F) == kNoHeading);
}

TEST_CASE("LabelsOf : les lettres du clavier du joueur aux places de W A S D", "[app][survivors]") {
  // Un clavier AZERTY : Z et Q aux places de W et A.
  const auto azerty = [](std::uint32_t scancode) -> std::string {
    if (scancode == kScancodeW) return "Z";
    if (scancode == kScancodeA) return "Q";
    if (scancode == kScancodeS) return "S";
    if (scancode == kScancodeD) return "D";
    return "?";
  };
  const moonclient::MoveKeyLabels labels = moonclient::LabelsOf(azerty);
  CHECK(labels.north == "Z");
  CHECK(labels.west == "Q");
  CHECK(labels.south == "S");
  CHECK(labels.east == "D");
}

TEST_CASE("InputGuard : une réponse dans les 400 ms est refusée", "[app][survivors]") {
  constexpr std::uint64_t kOpenedAtMs = 5000;
  InputGuard guard;
  guard.Arm(kOpenedAtMs);
  CHECK_FALSE(guard.Accepts(kOpenedAtMs));
  CHECK_FALSE(guard.Accepts(kOpenedAtMs + kInputGuardMs - 1));
  CHECK(guard.Accepts(kOpenedAtMs + kInputGuardMs));
}

TEST_CASE("HeadingOf : sous une caméra tournée, les touches suivent l'écran", "[app][survivors]") {
  constexpr float kQuarterTurnDegrees = 90.0F;
  constexpr float kEighthTurnDegrees = 45.0F;
  MoveKeys keys;
  keys.Take(kScancodeW, true);
  const MouseAim released;
  // Tournée d'un quart dans le sens trigonométrique, l'écran regarde l'ouest.
  CHECK(HeadingOf(keys, released, kQuarterTurnDegrees) == kWest);
  CHECK(HeadingOf(keys, released, -kQuarterTurnDegrees) == kEast);
  CHECK(HeadingOf(keys, released, 2.0F * kQuarterTurnDegrees) == kSouth);
  keys.Take(kScancodeD, true);
  // « Haut-droite » d'un huitième de tour : le nord.
  CHECK(HeadingOf(keys, released, kEighthTurnDegrees) == kNorth);
  // Rien de tenu ne marche, à tout lacet.
  keys.ReleaseAll();
  CHECK(HeadingOf(keys, released, kQuarterTurnDegrees) == kNoHeading);
  // La souris ignore le lacet : elle vise déjà un point du sol.
  keys.Take(kScancodeW, true);
  const ro::sim::FieldPoint hero{ro::sim::Cells(10), ro::sim::Cells(10)};
  const ro::sim::FieldPoint east_of_hero{ro::sim::Cells(14), ro::sim::Cells(10)};
  CHECK(HeadingOf(keys, MouseAim{.held = true, .hero = hero, .target = east_of_hero},
                  kQuarterTurnDegrees) == kEast);
}
