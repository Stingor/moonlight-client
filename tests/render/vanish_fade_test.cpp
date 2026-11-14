#include "ro/render/vanish_fade.h"

#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

using ro::render::kClientCorpseFadeMs;
using ro::render::VanishAlphaAt;
using ro::render::VanishIsOver;

namespace {

// L'agonie d'une créature d'essai, en millisecondes. Le chiffre n'a rien de
// remarquable : ce qui compte est qu'il précède le fondu.
constexpr float kAgonyMs = 408.0F;
constexpr std::uint32_t kAgonyEndMs = 408;

// L'opacité pleine.
constexpr int kOpaque = 255;

}  // namespace

TEST_CASE("Cadavre : le corps reste entier tant que l'agonie se joue", "[render][vanish]") {
  // 🔴 LE FONDU NE COMMENCE QU'À LA FIN DE L'ANIMATION, et non au paquet de
  // mort. Le faire partir plus tôt effacerait la chute elle-même.
  CHECK(VanishAlphaAt(kAgonyMs, 0, kClientCorpseFadeMs) == kOpaque);
  CHECK(VanishAlphaAt(kAgonyMs, 200, kClientCorpseFadeMs) == kOpaque);
  CHECK(VanishAlphaAt(kAgonyMs, kAgonyEndMs, kClientCorpseFadeMs) == kOpaque);
  CHECK_FALSE(VanishIsOver(kAgonyMs, kAgonyEndMs, kClientCorpseFadeMs));
}

TEST_CASE("Cadavre : un cran d'opacité toutes les vingt-quatre millisecondes", "[render][vanish]") {
  // 🔴 SIX SECONDES, ET NON UNE DEMI : c'est le cadavre du client, que l'on
  // effaçait auparavant en cinq cent dix millisecondes.
  CHECK(kClientCorpseFadeMs == 6120);
  CHECK(VanishAlphaAt(kAgonyMs, kAgonyEndMs + 1, kClientCorpseFadeMs) == kOpaque - 1);
  CHECK(VanishAlphaAt(kAgonyMs, kAgonyEndMs + 24, kClientCorpseFadeMs) == kOpaque - 1);
  CHECK(VanishAlphaAt(kAgonyMs, kAgonyEndMs + 25, kClientCorpseFadeMs) == kOpaque - 2);
  CHECK(VanishAlphaAt(kAgonyMs, kAgonyEndMs + 3060, kClientCorpseFadeMs) == 127);
  CHECK(VanishAlphaAt(kAgonyMs, kAgonyEndMs + 6119, kClientCorpseFadeMs) == 0);
}

TEST_CASE("Cadavre : il s'en va quand son opacité est nulle", "[render][vanish]") {
  const std::uint32_t end_ms = kAgonyEndMs + kClientCorpseFadeMs;
  CHECK_FALSE(VanishIsOver(kAgonyMs, end_ms - 1, kClientCorpseFadeMs));
  CHECK(VanishIsOver(kAgonyMs, end_ms, kClientCorpseFadeMs));
  CHECK(VanishAlphaAt(kAgonyMs, end_ms + 10000, kClientCorpseFadeMs) == 0);
}

TEST_CASE("Cadavre : sa durée se règle, le fondu reste linéaire", "[render][vanish]") {
  constexpr std::uint32_t kOneSecond = 1000;
  CHECK(VanishAlphaAt(kAgonyMs, kAgonyEndMs + 500, kOneSecond) == 127);
  CHECK_FALSE(VanishIsOver(kAgonyMs, kAgonyEndMs + 999, kOneSecond));
  CHECK(VanishIsOver(kAgonyMs, kAgonyEndMs + 1000, kOneSecond));
  // Une durée nulle ne fait pas disparaître le corps pendant sa chute.
  CHECK(VanishAlphaAt(kAgonyMs, 200, 0) == kOpaque);
  CHECK(VanishIsOver(kAgonyMs, kAgonyEndMs + 1, 0));
}

TEST_CASE("Apparition : de l'opacité 68 à la pleine, en 384 ms", "[render][vanish]") {
  using ro::render::AppearAlphaAt;
  using ro::render::kClientAppearFadeMs;
  CHECK(AppearAlphaAt(0, kClientAppearFadeMs) == 68);
  CHECK(AppearAlphaAt(192, kClientAppearFadeMs) == 161);
  CHECK(AppearAlphaAt(kClientAppearFadeMs, kClientAppearFadeMs) == kOpaque);
  CHECK(AppearAlphaAt(10'000, kClientAppearFadeMs) == kOpaque);
  // Zéro l'éteint : la créature paraît d'un coup.
  CHECK(AppearAlphaAt(0, 0) == kOpaque);
}

TEST_CASE("Sortie de vue : de la pleine opacité à rien, en 510 ms", "[render][vanish]") {
  using ro::render::kClientLeaveFadeMs;
  using ro::render::LeaveAlphaAt;
  CHECK(LeaveAlphaAt(0, kClientLeaveFadeMs) == kOpaque);
  // Le reste à vivre divisé par deux, tronqué : 509 / 2.
  CHECK(LeaveAlphaAt(1, kClientLeaveFadeMs) == 254);
  CHECK(LeaveAlphaAt(255, kClientLeaveFadeMs) == 127);
  CHECK(LeaveAlphaAt(kClientLeaveFadeMs, kClientLeaveFadeMs) == 0);
  // Zéro : il disparaît d'un coup.
  CHECK(LeaveAlphaAt(0, 0) == 0);
}

TEST_CASE("Cadavre : une agonie absente laisse quand même le fondu entier", "[render][vanish]") {
  // Une tenue dont le .act ne porte pas le rang de la mort rend une durée nulle.
  // Le corps doit alors s'effacer tout de même, et non disparaître d'un coup.
  CHECK(VanishAlphaAt(0.0F, 0, kClientCorpseFadeMs) == kOpaque);
  CHECK(VanishAlphaAt(0.0F, kClientCorpseFadeMs - 24, kClientCorpseFadeMs) == 1);
  CHECK(VanishAlphaAt(0.0F, kClientCorpseFadeMs - 23, kClientCorpseFadeMs) == 0);
  CHECK(VanishIsOver(0.0F, kClientCorpseFadeMs, kClientCorpseFadeMs));
}

TEST_CASE("Agonie : images fois délai fois vingt-quatre millisecondes", "[render][vanish]") {
  using ro::render::AgonyDurationMs;
  constexpr std::size_t kFrames = 5;
  constexpr float kDeclaredDelay = 6.0F;
  CHECK(AgonyDurationMs(kFrames, kDeclaredDelay) == 720.0F);
  // Hors de la table du .act, le délai vaut quatre.
  CHECK(AgonyDurationMs(kFrames, ro::render::kUndeclaredActionDelayUnits) == 480.0F);
  // Jamais moins d'un pas par image, même déclaré plus vif ou nul.
  constexpr float kTooQuick = 0.5F;
  CHECK(AgonyDurationMs(kFrames, kTooQuick) == 120.0F);
  CHECK(AgonyDurationMs(kFrames, 0.0F) == 120.0F);
}
