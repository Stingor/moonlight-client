// LES SPRITES DE DÉCOR DU DOCUMENT D'ÉDITION : lus, écrits, refusés, et leur
// image, leur direction et leur roulis à un instant donné.

#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ro/mapedit/map_edit.h"
#include "ro/mapedit/map_sprite.h"

using ro::mapedit::MapEdit;
using ro::mapedit::MapSprite;

namespace {

constexpr const char* kMap = "gonryun";

// Un document qui ne porte qu'un sprite, écrit à la main sous `fields`.
std::string OneSprite(const std::string& fields) {
  return std::string("version: 1\nmap: 'gonryun'\nmodels: []\nsprites:\n  - ") + fields;
}

}  // namespace

TEST_CASE("Sprites de décor : le document les écrit et les relit à l'identique", "[mapedit]") {
  MapEdit edit;
  edit.AddSprite(MapSprite{.sprite = "SAVAGE",
                           .position = {10.0F, -2.5F, 20.0F},
                           .action = 3,
                           .held_frame = 1,
                           .frame_speed = 0.5F,
                           .direction = 2,
                           .turn_degrees_per_second = 0.0F,
                           .roll_degrees = 90.0F,
                           .roll_degrees_per_second = 45.0F,
                           .scale = 1.5F,
                           .opacity = 0.75F,
                           .hidden = true,
                           .why = "le rôti du campement"});
  edit.AddSprite(
      MapSprite{.sprite = "1002", .turn_degrees_per_second = -30.0F, .why = "un poring"});
  CHECK_FALSE(edit.empty());
  const std::string text = ro::mapedit::MapEditToYaml(edit, kMap);
  auto reread = ro::mapedit::ParseMapEdit(text, kMap);
  REQUIRE(reread.has_value());
  CHECK(*reread == edit);

  // Retirer le premier fait remonter le second.
  edit.RemoveSprite(0);
  REQUIRE(edit.sprites().size() == 1);
  CHECK(edit.sprites()[0].why == "un poring");
  edit.SetSprite(0, MapSprite{.sprite = "PORING", .why = "renommé"});
  CHECK(edit.sprites()[0].sprite == "PORING");
  edit.SetSprite(1, MapSprite{.sprite = "hors de la liste", .why = "rien"});
  CHECK(edit.sprites().size() == 1);
}

TEST_CASE("Sprites de décor : un sprite malformé refuse le document", "[mapedit]") {
  const char* const kFlawed[] = {
      "{sprite: 'SAVAGE', position: [0, 0, 0]}",
      "{sprite: '', position: [0, 0, 0], why: 'x'}",
      "{sprite: 'SAVAGE', position: [0, 0], why: 'x'}",
      "{sprite: 'SAVAGE', position: [0, 0, 0], direction: 8, why: 'x'}",
      "{sprite: 'SAVAGE', position: [0, 0, 0], speed: 0, why: 'x'}",
      "{sprite: 'SAVAGE', position: [0, 0, 0], scale: -1, why: 'x'}",
      "{sprite: 'SAVAGE', position: [0, 0, 0], opacity: 2, why: 'x'}",
      "{sprite: 'SAVAGE', position: [0, 0, 0], why: ''}",
  };
  for (const char* fields : kFlawed) {
    INFO(fields);
    CHECK_FALSE(ro::mapedit::ParseMapEdit(OneSprite(fields), kMap).has_value());
  }
  const auto good = ro::mapedit::ParseMapEdit(
      OneSprite("{sprite: 'SAVAGE', position: [1, 2, 3], why: 'le rôti'}"), kMap);
  REQUIRE(good.has_value());
  REQUIRE(good->sprites().size() == 1);
  // Les champs absents prennent leur défaut.
  CHECK(good->sprites()[0] ==
        MapSprite{.sprite = "SAVAGE", .position = {1.0F, 2.0F, 3.0F}, .why = "le rôti"});
}

TEST_CASE("Sprites de décor : un numéro de classe se reconnaît", "[mapedit]") {
  CHECK(ro::mapedit::SpriteClassNumber("1166") == 1166U);
  CHECK_FALSE(ro::mapedit::SpriteClassNumber("SAVAGE").has_value());
  CHECK_FALSE(ro::mapedit::SpriteClassNumber("").has_value());
  CHECK_FALSE(ro::mapedit::SpriteClassNumber("12a").has_value());
  CHECK_FALSE(ro::mapedit::SpriteClassNumber("99999999999").has_value());
}

TEST_CASE("Sprites de décor : l'image boucle à sa cadence, ou reste figée", "[mapedit]") {
  constexpr std::size_t kFrames = 4;
  constexpr float kIntervalMs = 100.0F;
  MapSprite looping{.sprite = "SAVAGE"};
  CHECK(ro::mapedit::SpriteFrameAt(looping, 0.0, kFrames, kIntervalMs) == 0);
  CHECK(ro::mapedit::SpriteFrameAt(looping, 0.25, kFrames, kIntervalMs) == 2);
  // Au bout de la boucle, elle repart.
  CHECK(ro::mapedit::SpriteFrameAt(looping, 0.45, kFrames, kIntervalMs) == 0);
  // Deux fois plus vite : deux fois plus loin au même instant.
  looping.frame_speed = 2.0F;
  CHECK(ro::mapedit::SpriteFrameAt(looping, 0.15, kFrames, kIntervalMs) == 3);
  // Figée, elle ne bouge plus ; au-delà de la dernière image, elle s'y tient.
  MapSprite frozen{.sprite = "SAVAGE", .held_frame = 2};
  CHECK(ro::mapedit::SpriteFrameAt(frozen, 12.0, kFrames, kIntervalMs) == 2);
  frozen.held_frame = 9;
  CHECK(ro::mapedit::SpriteFrameAt(frozen, 0.0, kFrames, kIntervalMs) == kFrames - 1);
  // Sans image, l'image zéro.
  CHECK(ro::mapedit::SpriteFrameAt(frozen, 1.0, 0, kIntervalMs) == 0);
}

TEST_CASE("Sprites de décor : l'orientation tourne d'une direction à l'autre", "[mapedit]") {
  MapSprite still{.sprite = "SAVAGE", .direction = 3};
  CHECK(ro::mapedit::SpriteFacingDegreesAt(still, 10.0) == Catch::Approx(135.0F));
  CHECK(ro::mapedit::SpriteDirectionOfFacing(ro::mapedit::SpriteFacingDegreesAt(still, 10.0)) == 3);
  // Un tour par huit secondes : une direction par seconde.
  MapSprite turning{.sprite = "SAVAGE", .direction = 0, .turn_degrees_per_second = 45.0F};
  CHECK(ro::mapedit::SpriteDirectionOfFacing(ro::mapedit::SpriteFacingDegreesAt(turning, 1.0)) ==
        1);
  CHECK(ro::mapedit::SpriteDirectionOfFacing(ro::mapedit::SpriteFacingDegreesAt(turning, 7.0)) ==
        7);
  // Le tour se boucle.
  CHECK(ro::mapedit::SpriteDirectionOfFacing(ro::mapedit::SpriteFacingDegreesAt(turning, 8.0)) ==
        0);
  // À l'envers, la direction décroît.
  turning.turn_degrees_per_second = -45.0F;
  CHECK(ro::mapedit::SpriteDirectionOfFacing(ro::mapedit::SpriteFacingDegreesAt(turning, 1.0)) ==
        7);
  // Le huitième le plus proche : 20° montre encore la face, 25° la suivante.
  CHECK(ro::mapedit::SpriteDirectionOfFacing(20.0F) == 0);
  CHECK(ro::mapedit::SpriteDirectionOfFacing(25.0F) == 1);
  CHECK(ro::mapedit::SpriteDirectionOfFacing(350.0F) == 0);
}

TEST_CASE("Sprites de décor : le roulis pivote l'image, fixe ou en continu", "[mapedit]") {
  MapSprite spit{.sprite = "SAVAGE", .roll_degrees = 90.0F};
  CHECK(ro::mapedit::SpriteRollDegreesAt(spit, 5.0) == Catch::Approx(90.0F));
  spit.roll_degrees_per_second = 120.0F;
  CHECK(ro::mapedit::SpriteRollDegreesAt(spit, 1.0) == Catch::Approx(210.0F));
  // Ramené dans le tour.
  CHECK(ro::mapedit::SpriteRollDegreesAt(spit, 3.0) == Catch::Approx(90.0F));
  spit.roll_degrees_per_second = -120.0F;
  CHECK(ro::mapedit::SpriteRollDegreesAt(spit, 1.0) == Catch::Approx(330.0F));
}
