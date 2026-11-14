#include "ro/sim/skill_banner.h"

#include <cstdint>

#include <catch2/catch_test_macros.hpp>

namespace {

// L'invocation de suivants et leur rappel, que les monstres d'une Bloody
// Branch lancent en nombre.
constexpr std::uint16_t kFollowerSummons = 196;
constexpr std::uint16_t kCallFollowers = 209;

}  // namespace

TEST_CASE("Bannière : un monstre tait ses invocations, un joueur les annonce", "[sim][world]") {
  for (const std::uint16_t skill_id : {kFollowerSummons, kCallFollowers}) {
    CHECK_FALSE(ro::sim::SkillBannerShows(skill_id, true));
    CHECK(ro::sim::SkillBannerShows(skill_id, false));
  }
  // Les autres compétences d'un monstre gardent leur bannière.
  CHECK(ro::sim::SkillBannerShows(kFollowerSummons + 1, true));
}
