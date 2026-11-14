#include "effects/effect_named_sounds.h"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_frame.h"

namespace {

using moonclient::NamedEffectLifeTicks;
using moonclient::NamedMethodFamily;
using moonclient::PlayNamedMethodSounds;
using moonclient::QueueNamedMethodSounds;
using moonclient::SownNamedSound;

constexpr std::uint16_t kStormSlash = 5213;
constexpr std::uint16_t kLightningJudgement = 791;
constexpr std::uint16_t kChargingPierceCount = 1175;
constexpr std::uint8_t kOnUseSkill = 11;
constexpr std::uint8_t kOnGained = 0;
constexpr std::uint32_t kCaster = 7;
constexpr ro::sim::CellPosition kCasterCell{10, 20};
constexpr ro::sim::CellPosition kTargetCell{30, 40};

}  // namespace

TEST_CASE("Bruits de méthode : Storm Slash empile un coup par niveau, tous les quinze tics",
          "[app][named_sounds]") {
  constexpr std::uint32_t kStartedMs = 1000;
  constexpr std::int32_t kLevel = 3;
  constexpr std::int32_t kLongLife = 100;
  std::vector<SownNamedSound> heard;
  QueueNamedMethodSounds(kStormSlash, kOnUseSkill, kLevel, kCaster, kCasterCell, kStartedMs,
                         kLongLife, heard);
  constexpr std::int32_t kStepTicks = 15;
  constexpr std::uint8_t kVariants = 4;
  REQUIRE(heard.size() == 3);
  for (std::int32_t rank = 0; rank < kLevel; ++rank) {
    const SownNamedSound& queued = heard[static_cast<std::size_t>(rank)];
    CHECK(queued.sound == "effect\\DK_STORMSLASH%d.wav");
    CHECK(queued.variants == kVariants);
    CHECK(queued.carrier_id == kCaster);
    CHECK(queued.due_ms ==
          kStartedMs + static_cast<std::uint32_t>(rank * kStepTicks * ro::render::kEffectTickMs));
  }
  // L'instance meurt au tic trente : le troisième coup, dû à ce tic, se perd.
  constexpr std::int32_t kDiesAtThirdStrike = 2 * kStepTicks;
  heard.clear();
  QueueNamedMethodSounds(kStormSlash, kOnUseSkill, kLevel, kCaster, kCasterCell, kStartedMs,
                         kDiesAtThirdStrike, heard);
  CHECK(heard.size() == 2);
  // Rien n'est empilé tout de suite, ni pour une autre compétence.
  heard.clear();
  PlayNamedMethodSounds(NamedMethodFamily::kSkill, kStormSlash, kOnUseSkill, kCaster, kCasterCell,
                        kTargetCell, heard);
  QueueNamedMethodSounds(kLightningJudgement, kOnUseSkill, kLevel, kCaster, kCasterCell, kStartedMs,
                         kLongLife, heard);
  CHECK(heard.empty());
}

TEST_CASE("Bruits de méthode : la vie d'un effet nommé compte son délai de départ",
          "[app][named_sounds]") {
  // Deux cent trente millisecondes font quinze tics entamés, puis les images.
  CHECK(NamedEffectLifeTicks(230, 51) == 66);
  CHECK(NamedEffectLifeTicks(0, 16) == 16);
  CHECK(NamedEffectLifeTicks(-5, 16) == 16);
}

TEST_CASE("Bruits de méthode : l'éclair tonne sur la cible, la charge sur son porteur",
          "[app][named_sounds]") {
  std::vector<SownNamedSound> heard;
  PlayNamedMethodSounds(NamedMethodFamily::kSkill, kLightningJudgement, kOnUseSkill, kCaster,
                        kCasterCell, kTargetCell, heard);
  REQUIRE(heard.size() == 1);
  CHECK(heard[0].sound == "effect\\NPC_LIGHTNING_JUDGEMENT.wav");
  CHECK(heard[0].cell.x == kTargetCell.x);
  CHECK(heard[0].cell.y == kTargetCell.y);
  CHECK(heard[0].carrier_id == 0);
  CHECK(heard[0].due_ms == 0);

  heard.clear();
  PlayNamedMethodSounds(NamedMethodFamily::kState, kChargingPierceCount, kOnGained, kCaster,
                        kCasterCell, kTargetCell, heard);
  REQUIRE(heard.size() == 1);
  CHECK(heard[0].sound == "effect\\DK_CHARGINGPIERCE3.wav");
  CHECK(heard[0].cell.x == kCasterCell.x);
  // La même action à la levée ne sonne pas.
  heard.clear();
  PlayNamedMethodSounds(NamedMethodFamily::kState, kChargingPierceCount, 1, kCaster, kCasterCell,
                        kTargetCell, heard);
  CHECK(heard.empty());
}
