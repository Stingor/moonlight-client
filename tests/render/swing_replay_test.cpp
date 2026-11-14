#include <cstdint>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/floating_number.h"
#include "ro/render/skill_hit_extra.h"

// L'ENCHAÎNEMENT QUI REJOUE SON COUP : le lanceur de CH_CHAINCRUSH repart dans
// sa pose d'attaque à chaque frappe au-delà de la première.

TEST_CASE("Reprise d'attaque : une par frappe après la première, pour Chain Crush",
          "[render][swing]") {
  using ro::render::kMultipleBlows;
  using ro::render::kSwingReplayingSkill;
  using ro::render::SwingReplaysOf;
  constexpr std::uint8_t kSkillBlow = 6;
  // Cinq frappes : quatre reprises.
  CHECK(SwingReplaysOf(kSwingReplayingSkill, kMultipleBlows, 5) == 4);
  CHECK(SwingReplaysOf(kSwingReplayingSkill, kMultipleBlows, 1) == 0);
  // Une sorte qui ne compte pas ses frappes n'en rejoue aucune.
  CHECK(SwingReplaysOf(kSwingReplayingSkill, kSkillBlow, 5) == 0);
  // Un compte que le client ne lit pas non plus.
  CHECK(SwingReplaysOf(kSwingReplayingSkill, kMultipleBlows, ro::render::kLargestHitCount + 1) ==
        0);
  // Aucune autre compétence : Tiger Fist porte son total, pas de reprise.
  constexpr std::uint16_t kTigerFist = 371;
  CHECK(SwingReplaysOf(kTigerFist, kMultipleBlows, 5) == 0);
}

TEST_CASE("Reprise d'attaque : le client de référence y renvoie Sonic Blow et Arrow Vulcan",
          "[render][swing]") {
  // Son patch les fait passer par le chemin de Chain Crush : chaque frappe
  // après la première remet le lanceur dans sa pose d'attaque.
  using ro::render::kMultipleBlows;
  using ro::render::SwingReplaysOf;
  constexpr std::uint16_t kSonicBlow = 136;
  constexpr std::uint16_t kArrowVulcan = 394;
  constexpr std::uint16_t kBlows = 8;
  CHECK(SwingReplaysOf(kSonicBlow, kMultipleBlows, kBlows) == kBlows - 1);
  CHECK(SwingReplaysOf(kArrowVulcan, kMultipleBlows, kBlows) == kBlows - 1);
}
