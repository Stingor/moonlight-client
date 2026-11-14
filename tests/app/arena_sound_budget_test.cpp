// LE LIMITEUR DE VOIX DE L'ARÈNE : vingt voix au plus, un départ par nom
// toutes les 60 ms, et un rang inférieur qui cède sa place, jamais l'inverse.

#include "survivors/arena_sound_budget.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "ro/audio/audio_engine.h"

using moonclient::ArenaSoundBudget;
using moonclient::ArenaSoundRank;
using moonclient::ArenaSoundVerdict;
using moonclient::kArenaInterfaceVoices;
using moonclient::kArenaSoundNameCadenceMs;
using moonclient::kArenaSoundVoices;

namespace {

constexpr std::uint64_t kStartMs = 1000;
constexpr std::uint64_t kLongMs = 5000;
constexpr std::uint64_t kShortMs = 200;
constexpr std::uint64_t kOneMs = 1;

// Remplit toutes les voix de bruits distincts du rang `rank`, longs de
// `length_ms`, au même instant.
void Fill(ArenaSoundBudget& budget, ArenaSoundRank rank, std::uint64_t length_ms,
          std::uint64_t now_ms) {
  for (std::size_t voice = 0; voice < kArenaSoundVoices; ++voice) {
    const ArenaSoundVerdict verdict =
        budget.Ask("fill_" + std::to_string(voice), rank, length_ms, now_ms);
    REQUIRE(verdict.play);
  }
}

}  // namespace

TEST_CASE("Sons de l'arène : douze voix restent à l'interface", "[app][survivors]") {
  STATIC_REQUIRE(kArenaSoundVoices + kArenaInterfaceVoices == ro::audio::kSoundVoices);
}

TEST_CASE("Sons de l'arène : vingt voix au plus, la vingt et unième est refusée",
          "[app][survivors]") {
  ArenaSoundBudget budget;
  Fill(budget, ArenaSoundRank::kSkill, kLongMs, kStartMs);
  CHECK(budget.busy(kStartMs) == kArenaSoundVoices);

  const ArenaSoundVerdict extra = budget.Ask("extra", ArenaSoundRank::kSkill, kLongMs, kStartMs);
  CHECK_FALSE(extra.play);
  CHECK(extra.silenced.empty());
  CHECK(budget.busy(kStartMs) == kArenaSoundVoices);
  CHECK(budget.refused_total() == 1);
}

TEST_CASE("Sons de l'arène : un départ par nom toutes les 60 ms", "[app][survivors]") {
  ArenaSoundBudget budget;
  REQUIRE(budget.Ask("hit", ArenaSoundRank::kSkill, kShortMs, kStartMs).play);
  CHECK_FALSE(budget.Ask("hit", ArenaSoundRank::kSkill, kShortMs, kStartMs).play);
  CHECK_FALSE(budget
                  .Ask("hit", ArenaSoundRank::kSkill, kShortMs,
                       kStartMs + kArenaSoundNameCadenceMs - kOneMs)
                  .play);
  // Un autre nom ne partage pas la cadence.
  CHECK(budget.Ask("other", ArenaSoundRank::kSkill, kShortMs, kStartMs).play);
  CHECK(budget.Ask("hit", ArenaSoundRank::kSkill, kShortMs, kStartMs + kArenaSoundNameCadenceMs)
            .play);
  // Un rang supérieur ne passe pas la cadence non plus.
  CHECK_FALSE(budget
                  .Ask("hit", ArenaSoundRank::kHeroHurt, kShortMs,
                       kStartMs + kArenaSoundNameCadenceMs + kOneMs)
                  .play);
}

TEST_CASE("Sons de l'arène : un rang supérieur prend la voix du plus bas", "[app][survivors]") {
  ArenaSoundBudget budget;
  Fill(budget, ArenaSoundRank::kSkill, kLongMs, kStartMs);
  // Toutes les voix sont prises : un skill cède la sienne au coup encaissé.
  const ArenaSoundVerdict hurt =
      budget.Ask("hurt", ArenaSoundRank::kHeroHurt, kShortMs, kStartMs + kOneMs);
  CHECK(hurt.play);
  CHECK_FALSE(hurt.silenced.empty());
  CHECK(budget.busy(kStartMs + kOneMs) == kArenaSoundVoices);
  CHECK(budget.silenced_total() == 1);
}

TEST_CASE("Sons de l'arène : le plus bas rang cède d'abord, puis celui qui finit le premier",
          "[app][survivors]") {
  ArenaSoundBudget budget;
  for (std::size_t voice = 0; voice + 2 < kArenaSoundVoices; ++voice) {
    REQUIRE(budget.Ask("skill_" + std::to_string(voice), ArenaSoundRank::kSkill, kLongMs, kStartMs)
                .play);
  }
  REQUIRE(budget.Ask("cry_long", ArenaSoundRank::kCry, kLongMs, kStartMs).play);
  REQUIRE(budget.Ask("cry_short", ArenaSoundRank::kCry, kShortMs, kStartMs).play);

  const ArenaSoundVerdict boss = budget.Ask("boss", ArenaSoundRank::kBoss, kLongMs, kStartMs);
  REQUIRE(boss.play);
  CHECK(boss.silenced == "cry_short");
  const ArenaSoundVerdict rise = budget.Ask("rise", ArenaSoundRank::kRise, kLongMs, kStartMs);
  REQUIRE(rise.play);
  CHECK(rise.silenced == "cry_long");
  // Plus aucun cri : un skill cède au coup encaissé.
  const ArenaSoundVerdict hurt = budget.Ask("hurt", ArenaSoundRank::kHeroHurt, kLongMs, kStartMs);
  REQUIRE(hurt.play);
  CHECK(hurt.silenced.starts_with("skill_"));
}

TEST_CASE("Sons de l'arène : jamais l'inverse, ni à rang égal", "[app][survivors]") {
  ArenaSoundBudget budget;
  Fill(budget, ArenaSoundRank::kRise, kLongMs, kStartMs);
  CHECK_FALSE(budget.Ask("cry", ArenaSoundRank::kCry, kShortMs, kStartMs).play);
  CHECK_FALSE(budget.Ask("skill", ArenaSoundRank::kSkill, kShortMs, kStartMs).play);
  CHECK_FALSE(budget.Ask("boss", ArenaSoundRank::kBoss, kShortMs, kStartMs).play);
  CHECK_FALSE(budget.Ask("rise", ArenaSoundRank::kRise, kShortMs, kStartMs).play);
  CHECK(budget.silenced_total() == 0);
  CHECK(budget.Ask("hurt", ArenaSoundRank::kHeroHurt, kShortMs, kStartMs).play);
}

TEST_CASE("Sons de l'arène : un nom qui sonne aussi à un rang supérieur ne cède pas",
          "[app][survivors]") {
  ArenaSoundBudget budget;
  for (std::size_t voice = 0; voice + 2 < kArenaSoundVoices; ++voice) {
    REQUIRE(budget.Ask("skill_" + std::to_string(voice), ArenaSoundRank::kSkill, kLongMs, kStartMs)
                .play);
  }
  // Le même bruit, cri d'une espèce puis coup qu'elle porte au héros.
  REQUIRE(budget.Ask("atk", ArenaSoundRank::kCry, kShortMs, kStartMs).play);
  REQUIRE(budget.Ask("atk", ArenaSoundRank::kHeroHurt, kLongMs, kStartMs + kArenaSoundNameCadenceMs)
              .play);
  // Le cri est le plus bas rang, mais le faire taire ferait taire le coup
  // encaissé : un skill cède à sa place.
  const ArenaSoundVerdict boss =
      budget.Ask("boss", ArenaSoundRank::kBoss, kLongMs, kStartMs + kArenaSoundNameCadenceMs);
  REQUIRE(boss.play);
  CHECK(boss.silenced.starts_with("skill_"));
  CHECK(budget.busy(kStartMs + kArenaSoundNameCadenceMs) == kArenaSoundVoices);
}

TEST_CASE("Sons de l'arène : une voix se libère à la fin estimée du son", "[app][survivors]") {
  ArenaSoundBudget budget;
  Fill(budget, ArenaSoundRank::kSkill, kShortMs, kStartMs);
  CHECK_FALSE(
      budget.Ask("late", ArenaSoundRank::kCry, kShortMs, kStartMs + kShortMs - kOneMs).play);
  CHECK(budget.busy(kStartMs + kShortMs - kOneMs) == kArenaSoundVoices);
  CHECK(budget.busy(kStartMs + kShortMs) == 0);
  CHECK(budget.Ask("late", ArenaSoundRank::kCry, kShortMs, kStartMs + kShortMs).play);
}

TEST_CASE("Sons de l'arène : un son de durée inconnue ne prend pas de voix", "[app][survivors]") {
  ArenaSoundBudget budget;
  CHECK_FALSE(budget.Ask("unknown", ArenaSoundRank::kHeroHurt, std::nullopt, kStartMs).play);
  CHECK(budget.busy(kStartMs) == 0);
  // Le refus ne pose pas de cadence : le même nom, connu, part aussitôt.
  CHECK(budget.Ask("unknown", ArenaSoundRank::kHeroHurt, kShortMs, kStartMs).play);
}

TEST_CASE("Sons de l'arène : une partie relancée repart sans voix ni cadence", "[app][survivors]") {
  ArenaSoundBudget budget;
  Fill(budget, ArenaSoundRank::kSkill, kLongMs, kStartMs);
  budget.Clear();
  CHECK(budget.busy(kStartMs) == 0);
  CHECK(budget.Ask("fill_0", ArenaSoundRank::kSkill, kLongMs, kStartMs).play);
}

TEST_CASE("Sons de l'arène : une carte passe devant le boss, pas devant le héros touché",
          "[app][survivors]") {
  ArenaSoundBudget budget;
  Fill(budget, ArenaSoundRank::kBoss, kLongMs, kStartMs);
  const ArenaSoundVerdict card = budget.Ask("card", ArenaSoundRank::kTreasure, kLongMs, kStartMs);
  CHECK(card.play);
  CHECK_FALSE(card.silenced.empty());

  ArenaSoundBudget hurt;
  Fill(hurt, ArenaSoundRank::kHeroHurt, kLongMs, kStartMs);
  CHECK_FALSE(hurt.Ask("card", ArenaSoundRank::kTreasure, kLongMs, kStartMs).play);
}
