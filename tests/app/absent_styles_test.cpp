// Le demandeur des styles hors de vue : qui il demande, et quand il se tait.

#include "world/absent_styles.h"

#include <cstdint>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/sim/style.h"
#include "ro/sim/world.h"

using moonclient::AbsentStyles;
using ro::sim::World;

namespace {

constexpr std::uint32_t kCadenceMs = 1000;
constexpr std::uint32_t kGuildmate = 150001;
constexpr std::uint32_t kFriend = 150002;

[[nodiscard]] World ServedWorld() {
  World world;
  world.session().style_absent_cadence_ms = kCadenceMs;
  world.session().style_absent_ids_most = 16;
  return world;
}

}  // namespace

TEST_CASE("Rien ne se demande avant que le serveur dise la cadence", "[app][absent_styles]") {
  World world;
  AbsentStyles absent;
  absent.Want(kGuildmate, 0);
  CHECK_FALSE(absent.MaySend(world, 0));
  world.session().style_absent_cadence_ms = kCadenceMs;
  CHECK(absent.MaySend(world, 0));
}

TEST_CASE("On demande ce qui est affiché, une fois, puis à la cadence", "[app][absent_styles]") {
  World world = ServedWorld();
  AbsentStyles absent;
  absent.Want(kFriend, 100);
  absent.Want(kGuildmate, 100);
  const std::vector<std::uint32_t> due = absent.Due(world, 100);
  REQUIRE(due == std::vector<std::uint32_t>{kGuildmate, kFriend});
  absent.NoteSent(due, 100);
  // Demandé, pas encore répondu : on ne redemande pas tout de suite.
  CHECK(absent.Due(world, 200).empty());
  CHECK_FALSE(absent.MaySend(world, 200));
  CHECK(absent.MaySend(world, 100 + kCadenceMs));
}

TEST_CASE("Une réponse fraîche épargne une demande, une vieille la refait",
          "[app][absent_styles]") {
  World world = ServedWorld();
  AbsentStyles absent;
  world.styles().RememberAbsent(kGuildmate, ro::sim::AbsentStyle{.heard_at_ms = 1000});
  absent.Want(kGuildmate, 2000);
  CHECK(absent.Due(world, 2000).empty());
  absent.Want(kGuildmate, 70000);
  CHECK(absent.Due(world, 70000) == std::vector<std::uint32_t>{kGuildmate});
}

TEST_CASE("Un membre qu'on n'affiche plus ne se demande plus", "[app][absent_styles]") {
  World world = ServedWorld();
  AbsentStyles absent;
  absent.Want(kGuildmate, 0);
  CHECK(absent.Due(world, 3000).empty());
}

TEST_CASE("Une demande sans réponse se refait après le délai", "[app][absent_styles]") {
  World world = ServedWorld();
  AbsentStyles absent;
  absent.Want(kGuildmate, 0);
  absent.NoteSent({kGuildmate}, 0);
  absent.Want(kGuildmate, 4000);
  CHECK(absent.Due(world, 4000).empty());
  absent.Want(kGuildmate, 5500);
  CHECK(absent.Due(world, 5500) == std::vector<std::uint32_t>{kGuildmate});
}

TEST_CASE("Un refus du serveur fait attendre ce qu'il dit", "[app][absent_styles]") {
  World world = ServedWorld();
  AbsentStyles absent;
  absent.Want(kGuildmate, 0);
  absent.NoteSent({kGuildmate}, 0);
  world.styles().HearAbsentRefusal(
      ro::sim::AbsentRefusal{.result = 2, .wait_ms = 3000, .heard_at_ms = 1000});
  absent.HearRefusals(world);
  CHECK_FALSE(absent.MaySend(world, 2000));
  CHECK(absent.MaySend(world, 4000));
  // Ce qui était demandé se redemande sans attendre le délai de réponse.
  absent.Want(kGuildmate, 4000);
  CHECK(absent.Due(world, 4000) == std::vector<std::uint32_t>{kGuildmate});
}

TEST_CASE("Une demande ne porte pas plus que ce que le serveur accepte", "[app][absent_styles]") {
  World world = ServedWorld();
  world.session().style_absent_ids_most = 3;
  AbsentStyles absent;
  for (std::uint32_t char_id = 1; char_id <= 10; ++char_id) {
    absent.Want(char_id, 0);
  }
  CHECK(absent.Due(world, 0).size() == 3);
}
