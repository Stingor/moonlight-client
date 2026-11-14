#include "ro/core/pcg32.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::core::Pcg32;
using ro::core::ShuffleWith;

// Les suites attendues sont celles de l'algorithme de référence de PCG,
// recalculées par un programme indépendant : jamais relevées sur ce code.

TEST_CASE("Pcg32 : la suite publiée de la démonstration de référence", "[core][random]") {
  Pcg32 random(42, 54);
  constexpr std::array<std::uint32_t, 6> kExpected = {0xa15c02b7, 0x7b47f409, 0xba1d3330,
                                                      0x83d2f293, 0xbfa4784b, 0xcbed606e};
  for (const std::uint32_t expected : kExpected) CHECK(random.Next() == expected);
}

TEST_CASE("Pcg32 : graine et flux nuls", "[core][random]") {
  Pcg32 random(0, 0);
  CHECK(random.Next() == 0xe4c14788U);
  CHECK(random.Next() == 0x379c6516U);
  CHECK(random.Next() == 0x5c4ab3bbU);
  // Le constructeur par défaut est celui de graine et flux nuls.
  CHECK(Pcg32() == Pcg32(0, 0));
}

TEST_CASE("Pcg32 : dix tirages de Below(6)", "[core][random]") {
  Pcg32 random(42, 54);
  constexpr std::array<std::uint32_t, 10> kExpected = {3, 3, 2, 1, 1, 4, 5, 3, 0, 2};
  for (const std::uint32_t expected : kExpected) CHECK(random.Below(6) == expected);
}

TEST_CASE("Pcg32 : Below rejette sous le seuil", "[core][random]") {
  // Pour cette borne, le seuil vaut 0x7FFFFFFF : près d'une sortie sur deux est
  // rejetée. Un Below sans rejet (Next() % bound) rendrait 0x7b47f409 en
  // deuxième, et n'aurait consommé que dix sorties.
  constexpr std::uint32_t kHalfRejectedBound = 0x80000001U;
  constexpr std::array<std::uint32_t, 10> kExpected = {
      0x215c02b6, 0x3a1d332f, 0x03d2f292, 0x3fa4784a, 0x4bed606d,
      0x3fc6a3ac, 0x012fff6c, 0x661f3059, 0x79384b8f, 0x6d786825};
  constexpr int kOutputsConsumed = 13;
  Pcg32 random(42, 54);
  for (const std::uint32_t expected : kExpected) {
    CHECK(random.Below(kHalfRejectedBound) == expected);
  }
  Pcg32 advanced(42, 54);
  for (int output = 0; output < kOutputsConsumed; ++output) (void)advanced.Next();
  CHECK(random == advanced);
}

TEST_CASE("Pcg32 : Below(0) ne tire pas, Below(1) tire une fois", "[core][random]") {
  Pcg32 random(42, 54);
  CHECK(random.Below(0) == 0U);
  CHECK(random == Pcg32(42, 54));

  Pcg32 once(42, 54);
  (void)once.Next();
  CHECK(random.Below(1) == 0U);
  CHECK(random == once);
}

TEST_CASE("Pcg32 : Between et ses bornes", "[core][random]") {
  Pcg32 random(7, 3);
  CHECK(random.Between(5, 5) == 5);

  Pcg32 untouched(7, 3);
  (void)untouched.Below(1);
  CHECK(random == untouched);

  const Pcg32 before = random;
  CHECK(random.Between(7, 3) == 7);
  CHECK(random == before);

  Pcg32 whole = random;
  Pcg32 single = random;
  const auto drawn = whole.Between(std::numeric_limits<std::int32_t>::min(),
                                   std::numeric_limits<std::int32_t>::max());
  CHECK(drawn == static_cast<std::int32_t>(single.Next()));
  CHECK(whole == single);

  for (int draw = 0; draw < 1000; ++draw) {
    const std::int32_t value = random.Between(-3, 4);
    CHECK(value >= -3);
    CHECK(value <= 4);
  }
}

TEST_CASE("Pcg32 : Chance consomme exactement un tirage", "[core][random]") {
  Pcg32 random(11, 5);
  const Pcg32 before = random;
  CHECK_FALSE(random.Chance(0, 0));
  CHECK_FALSE(random.Chance(1, 0));
  CHECK(random == before);

  Pcg32 once = random;
  (void)once.Below(1);
  CHECK(random.Chance(1, 1));
  CHECK(random == once);

  Pcg32 over = random;
  (void)over.Below(3);
  CHECK(random.Chance(5, 3));
  CHECK(random == over);
}

TEST_CASE("Pcg32 : PickWeighted", "[core][random]") {
  Pcg32 random(3, 9);
  const Pcg32 before = random;
  constexpr std::array<std::uint32_t, 2> kNothing = {0, 0};
  CHECK(random.PickWeighted(kNothing) == 2U);
  CHECK(random.PickWeighted(std::span<const std::uint32_t>{}) == 0U);
  constexpr std::array<std::uint32_t, 2> kTooMuch = {0xFFFFFFFFU, 1};
  CHECK(random.PickWeighted(kTooMuch) == 2U);
  CHECK(random == before);

  constexpr std::array<std::uint32_t, 3> kMiddle = {0, 5, 0};
  for (int draw = 0; draw < 1000; ++draw) CHECK(random.PickWeighted(kMiddle) == 1U);

  // Un tirage par Below(somme) : l'index suit la somme cumulée.
  Pcg32 mirror(3, 9);
  Pcg32 picker(3, 9);
  constexpr std::array<std::uint32_t, 3> kWeights = {2, 0, 3};
  for (int draw = 0; draw < 100; ++draw) {
    const std::uint32_t below = mirror.Below(5);
    const std::size_t expected = below < 2 ? 0 : 2;
    CHECK(picker.PickWeighted(kWeights) == expected);
  }
}

TEST_CASE("Pcg32 : Below(3) est uniforme", "[core][random]") {
  Pcg32 random(42, 54);
  std::array<std::uint32_t, 3> counts = {};
  for (int draw = 0; draw < 300000; ++draw) ++counts[random.Below(3)];
  for (const std::uint32_t count : counts) {
    CHECK(count >= 98500U);
    CHECK(count <= 101500U);
  }
}

TEST_CASE("ShuffleWith : une permutation rejouable", "[core][random]") {
  std::vector<int> items(50);
  for (std::size_t index = 0; index < items.size(); ++index) items[index] = static_cast<int>(index);
  std::vector<int> again = items;

  Pcg32 random(5, 1);
  Pcg32 twin(5, 1);
  ShuffleWith(random, std::span<int>(items));
  ShuffleWith(twin, std::span<int>(again));
  CHECK(items == again);
  CHECK(random == twin);

  std::vector<int> sorted = items;
  std::stable_sort(sorted.begin(), sorted.end());
  for (std::size_t index = 0; index < sorted.size(); ++index) {
    CHECK(sorted[index] == static_cast<int>(index));
  }
  CHECK_FALSE(std::is_sorted(items.begin(), items.end()));

  const Pcg32 before = random;
  std::vector<int> nothing;
  ShuffleWith(random, std::span<int>(nothing));
  std::vector<int> alone = {7};
  ShuffleWith(random, std::span<int>(alone));
  CHECK(alone == std::vector<int>{7});
  CHECK(random == before);
}

TEST_CASE("Pcg32 : deux flux d'une même graine divergent", "[core][random]") {
  Pcg32 first(42, 1);
  Pcg32 second(42, 2);
  CHECK(first.Next() != second.Next());
}
