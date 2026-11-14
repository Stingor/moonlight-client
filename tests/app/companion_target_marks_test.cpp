// LE MARQUEUR DE CIBLE DES COMPAGNONS : il naît sur l'acteur que le premier
// appui désigne, y reste après l'attaque, suit un nouvel acteur désigné, et
// s'éteint sur un ordre de marche ; son animation part de sa naissance.

#include "game/companion_target_marks.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/companion_ai.h"

namespace {

constexpr std::uint32_t kMonsterId = 110001;
constexpr std::uint32_t kSecondMonsterId = 110002;
constexpr std::uint32_t kStartMs = 5000;
constexpr std::uint32_t kStepMs = 16;

// LES PORTEURS À CETTE IMAGE, pour une seule marque.
std::span<const std::uint32_t> BearersOf(const ro::sim::AttackDesignation& designation,
                                         std::array<std::uint32_t, 1>& storage) {
  storage[0] = designation.marked_id();
  return storage[0] == 0 ? std::span<const std::uint32_t>{} : std::span(storage);
}

}  // namespace

TEST_CASE("Marqueur de cible : la désignation le pose sur le bon acteur, l'ordre de marche l'ôte",
          "[app][companion_ai]") {
  using ro::sim::AttackPress;
  ro::sim::AttackDesignation designation;
  moonclient::CompanionTargetMarkClock clock;
  std::array<std::uint32_t, 1> storage{};

  clock.Update(BearersOf(designation, storage), kStartMs);
  CHECK_FALSE(clock.MarkedSince(kMonsterId).has_value());

  REQUIRE(designation.Press(kMonsterId, false, false) == AttackPress::kDesignated);
  clock.Update(BearersOf(designation, storage), kStartMs);
  CHECK(clock.MarkedSince(kMonsterId) == std::optional<std::uint32_t>(kStartMs));
  CHECK_FALSE(clock.MarkedSince(kSecondMonsterId).has_value());

  // L'ATTAQUE PARTIE, LA CIBLE RESTE MARQUÉE, et son anneau garde son âge.
  REQUIRE(designation.Press(kMonsterId, false, false) == AttackPress::kAttack);
  clock.Update(BearersOf(designation, storage), kStartMs + kStepMs);
  CHECK(clock.MarkedSince(kMonsterId) == std::optional<std::uint32_t>(kStartMs));

  // L'ordre réservé ne la déplace pas.
  REQUIRE(designation.Press(kSecondMonsterId, true, false) == AttackPress::kReserveAttack);
  clock.Update(BearersOf(designation, storage), kStartMs + 2 * kStepMs);
  CHECK(clock.MarkedSince(kMonsterId).has_value());
  CHECK_FALSE(clock.MarkedSince(kSecondMonsterId).has_value());

  // Un autre acteur désigné la prend, avec un anneau neuf.
  REQUIRE(designation.Press(kSecondMonsterId, false, false) == AttackPress::kDesignated);
  clock.Update(BearersOf(designation, storage), kStartMs + 3 * kStepMs);
  CHECK_FALSE(clock.MarkedSince(kMonsterId).has_value());
  CHECK(clock.MarkedSince(kSecondMonsterId) ==
        std::optional<std::uint32_t>(kStartMs + 3 * kStepMs));

  // L'ordre de marche l'éteint.
  designation.Clear();
  clock.Update(BearersOf(designation, storage), kStartMs + 4 * kStepMs);
  CHECK_FALSE(clock.MarkedSince(kSecondMonsterId).has_value());
}

TEST_CASE("Marqueur de cible : deux compagnons sur le même acteur ne font qu'un anneau",
          "[app][companion_ai]") {
  moonclient::CompanionTargetMarkClock clock;
  const std::array<std::uint32_t, 1> homunculus_only = {kMonsterId};
  clock.Update(homunculus_only, kStartMs);
  const std::array<std::uint32_t, 2> both = {kMonsterId, kMonsterId};
  clock.Update(both, kStartMs + kStepMs);
  CHECK(clock.MarkedSince(kMonsterId) == std::optional<std::uint32_t>(kStartMs));
  // Le mercenaire seul le garde quand l'homoncule le quitte.
  const std::array<std::uint32_t, 2> mercenary_kept = {kSecondMonsterId, kMonsterId};
  clock.Update(mercenary_kept, kStartMs + 2 * kStepMs);
  CHECK(clock.MarkedSince(kMonsterId) == std::optional<std::uint32_t>(kStartMs));
  CHECK(clock.MarkedSince(kSecondMonsterId) ==
        std::optional<std::uint32_t>(kStartMs + 2 * kStepMs));
}
