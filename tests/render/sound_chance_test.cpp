#include "ro/render/sound_chance.h"

#include <cstddef>
#include <cstdint>
#include <span>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/effect_sound_table.h"

using ro::render::SoundChance;

namespace {

// Un bourbier : vingt-cinq cellules, toutes lancées dans la même image.
constexpr std::size_t kQuagmireCells = 25;
constexpr std::uint32_t kQuagmireOneIn = 12;
constexpr std::size_t kImages = 4000;
// La proportion attendue est un douzième ; l'écart toléré laisse passer le
// bruit d'un tirage de cent mille lancements, pas une loi différente.
constexpr double kTolerance = 0.01;
constexpr std::uint32_t kSeed = 7;
// La hauteur au-dessus de l'auditeur que le client passe au bruit du bourbier.
constexpr std::uint8_t kQuagmireHeight = 100;
constexpr std::uint32_t kQuagmireEffect = 95;
constexpr std::uint32_t kQuagmireStrEffect = 1161;

}  // namespace

TEST_CASE("Tirage d'un bruit : les lancements d'une même image tirent chacun", "[render][sound]") {
  // Le tirage tiré de l'instant rendait le même résultat aux vingt-cinq
  // cellules d'une image : toutes sonnaient, ou aucune. Le client tire à chaque
  // lancement.
  SoundChance chance(kSeed);
  std::size_t heard = 0;
  std::size_t mixed_images = 0;
  for (std::size_t image = 0; image < kImages; ++image) {
    std::size_t heard_here = 0;
    for (std::size_t cell = 0; cell < kQuagmireCells; ++cell) {
      heard_here += chance.Heard(kQuagmireOneIn) ? 1U : 0U;
    }
    heard += heard_here;
    mixed_images += (heard_here > 0 && heard_here < kQuagmireCells) ? 1U : 0U;
  }
  const double share = static_cast<double>(heard) / static_cast<double>(kImages * kQuagmireCells);
  CHECK(share > 1.0 / kQuagmireOneIn - kTolerance);
  CHECK(share < 1.0 / kQuagmireOneIn + kTolerance);
  // Une image sur (11/12)^25 ≈ 11 % n'entend rien, et pratiquement aucune
  // n'entend tout : la grande majorité est mêlée. Un tirage partagé par
  // l'image n'en rendrait AUCUNE.
  CHECK(mixed_images > kImages / 2);
}

TEST_CASE("Tirage d'un bruit : un ou zéro veut dire à coup sûr, sans tirer", "[render][sound]") {
  SoundChance chance(kSeed);
  SoundChance witness(kSeed);
  CHECK(chance.Heard(0));
  CHECK(chance.Heard(1));
  // La suite n'a pas avancé : les deux rendent le même tirage suivant.
  for (std::size_t draw = 0; draw < kQuagmireCells; ++draw) {
    CHECK(chance.Heard(kQuagmireOneIn) == witness.Heard(kQuagmireOneIn));
  }
}

TEST_CASE("Tirage d'un bruit : le bourbier sonne une fois sur douze, dans ses deux effets",
          "[render][sound]") {
  // L'effet complet et celui des effets réduits tirent tous deux `rand() % 12`
  // à la naissance de l'instance, et placent le bruit cent unités au-dessus de
  // l'auditeur.
  for (const std::uint32_t effect_id : {kQuagmireEffect, kQuagmireStrEffect}) {
    const std::span<const ro::render::EffectSoundEntry> sounds =
        ro::render::EffectSounds(effect_id);
    REQUIRE(sounds.size() == 1);
    CHECK(sounds.front().sound == "effect\\wizard_quagmire.wav");
    CHECK(sounds.front().delay_ticks == 0);
    CHECK(sounds.front().one_in == kQuagmireOneIn);
    CHECK(sounds.front().listener_height == kQuagmireHeight);
    CHECK_FALSE(sounds.front().at_listener);
  }
}
