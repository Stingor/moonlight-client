#include "ro/render/skill_damage_timing.h"

#include <cstddef>
#include <cstdint>

#include <catch2/catch_test_macros.hpp>

TEST_CASE("Dégâts : Tetra Vortex fait arriver ses quatre éléments une demi-seconde à part",
          "[render][degats]") {
  constexpr std::uint16_t kFeu = 2218;
  constexpr std::uint16_t kEau = 2219;
  constexpr std::uint16_t kVent = 2220;
  constexpr std::uint16_t kTerre = 2221;
  CHECK(ro::render::SkillDamageDelayMs(kFeu) == 0);
  CHECK(ro::render::SkillDamageDelayMs(kEau) == 500);
  CHECK(ro::render::SkillDamageDelayMs(kVent) == 1000);
  CHECK(ro::render::SkillDamageDelayMs(kTerre) == 1500);
}

TEST_CASE("Dégâts : une compétence ordinaire n'ajoute rien", "[render][degats]") {
  constexpr std::uint16_t kBash = 5;
  CHECK(ro::render::SkillDamageDelayMs(kBash) == 0);
  CHECK(ro::render::SkillHitStepExtraMs(kBash) == 0);
}

TEST_CASE("Dégâts : Pierce allonge le pas entre ses coups", "[render][degats]") {
  constexpr std::uint16_t kPierce = 56;
  CHECK(ro::render::SkillHitStepExtraMs(kPierce) == 80);
}

TEST_CASE("Dégâts : chaque compétence n'a qu'une entrée par table", "[render][degats]") {
  const auto unique = [](const auto& table) {
    for (std::size_t i = 0; i < table.size(); ++i) {
      for (std::size_t j = i + 1; j < table.size(); ++j) {
        if (table[i].skill_id == table[j].skill_id) {
          return false;
        }
      }
    }
    return true;
  };
  CHECK(unique(ro::render::kSkillDamageDelays));
  CHECK(unique(ro::render::kSkillHitStepExtras));
}
