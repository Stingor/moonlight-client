#include "survivors/hero_frame_sound.h"

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include <catch2/catch_test_macros.hpp>

#include "ro/render/actor_sprite.h"

// LE SON QU'INSCRIT L'IMAGE DU HÉROS : une fois à l'arrivée sur l'image.
using moonclient::HeroFrameSound;
using ro::render::ComposedActor;

namespace {

constexpr std::size_t kShootAction = 88;
constexpr std::size_t kReleaseFrame = 3;

ComposedActor HeroAt(std::size_t action, std::size_t frame, std::string_view sound) {
  ComposedActor hero;
  hero.action_index = action;
  hero.frame_index = frame;
  hero.sound = sound;
  return hero;
}

}  // namespace

TEST_CASE("Son d'image du héros : la corde sonne une fois à l'image du tir",
          "[survivors][hero_frame_sound]") {
  HeroFrameSound heard;
  const ComposedActor drawing = HeroAt(kShootAction, kReleaseFrame - 1, {});
  const ComposedActor release = HeroAt(kShootAction, kReleaseFrame, "attack_bow.wav");
  CHECK_FALSE(heard.Hear(&drawing).has_value());
  const std::optional<std::string> twang = heard.Hear(&release);
  REQUIRE(twang.has_value());
  CHECK(*twang == "attack_bow.wav");
  // Il se tient sur l'image pendant plusieurs peintures : un seul bruit.
  CHECK_FALSE(heard.Hear(&release).has_value());
  // Le tir suivant repasse par l'image : il sonne de nouveau.
  CHECK_FALSE(heard.Hear(&drawing).has_value());
  CHECK(heard.Hear(&release).has_value());
}

TEST_CASE("Son d'image du héros : le héros qui reparaît sonne de nouveau",
          "[survivors][hero_frame_sound]") {
  HeroFrameSound heard;
  const ComposedActor release = HeroAt(kShootAction, kReleaseFrame, "attack_bow.wav");
  CHECK(heard.Hear(&release).has_value());
  CHECK_FALSE(heard.Hear(nullptr).has_value());
  CHECK(heard.Hear(&release).has_value());
}
