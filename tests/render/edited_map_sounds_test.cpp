// LES SOURCES DE SON À LEVER : celles du RSW telles que le document les
// retouche, puis celles qu'il ajoute, les « nuit seulement » à la nuit.

#include "ro/render/edited_map_sounds.h"

#include <array>
#include <optional>
#include <vector>

#include <catch2/catch_test_macros.hpp>

using ro::formats::RswSound;
using ro::mapedit::AddedSound;
using ro::mapedit::SoundEdit;
using ro::render::ActiveMapSounds;

namespace {

constexpr float kDay = 0.0F;
constexpr float kNight = 1.0F;

std::vector<RswSound> TwoSources() {
  return {RswSound{.name = "eau",
                   .wave_name = "eau.wav",
                   .position = {1.0F, 0.0F, 2.0F},
                   .volume = 1.0F,
                   .width = 10,
                   .height = 10,
                   .range = 50.0F,
                   .cycle_seconds = 4.0F},
          RswSound{.name = "vent",
                   .wave_name = "vent.wav",
                   .position = {30.0F, 0.0F, 40.0F},
                   .volume = 0.5F,
                   .width = 20,
                   .height = 20,
                   .range = 80.0F,
                   .cycle_seconds = 8.0F}};
}

}  // namespace

TEST_CASE("Sons du document : la retouche pose sa source, un son ajouté la suit",
          "[render][mapedit]") {
  const std::vector<RswSound> sources = TwoSources();
  ro::mapedit::MapEdit document;
  document.SetSoundEdit(SoundEdit{.sound_index = 0,
                                  .wave_file = "eau.wav",
                                  .origin = {1.0F, 0.0F, 2.0F},
                                  .position = std::array<float, 3>{5.0F, 0.0F, 6.0F},
                                  .volume = 0.25F,
                                  .range = 20.0F,
                                  .cycle_seconds = 2.0F,
                                  .why = "x"});
  document.AddSound(AddedSound{.wave_file = "grillon.wav",
                               .position = {7.0F, 0.0F, 8.0F},
                               .volume = 0.5F,
                               .width = 4,
                               .height = 6,
                               .range = 30.0F,
                               .cycle_seconds = 3.0F,
                               .why = "y"});
  const std::vector<RswSound> raised = ActiveMapSounds(sources, document, kDay);
  REQUIRE(raised.size() == 3);
  CHECK(raised[0].position == std::array<float, 3>{5.0F, 0.0F, 6.0F});
  CHECK(raised[0].volume == 0.25F);
  CHECK(raised[0].range == 20.0F);
  CHECK(raised[0].cycle_seconds == 2.0F);
  // L'emprise d'une source ne se retouche pas.
  CHECK(raised[0].width == 10);
  CHECK(raised[1].wave_name == "vent.wav");
  CHECK(raised[1].volume == 0.5F);
  CHECK(raised[2].wave_name == "grillon.wav");
  CHECK(raised[2].width == 4);
  CHECK(raised[2].height == 6);
  CHECK(raised[2].range == 30.0F);
}

TEST_CASE("Sons du document : une retouche qui ne vise plus sa source ne s'applique pas",
          "[render][mapedit]") {
  const std::vector<RswSound> sources = TwoSources();
  ro::mapedit::MapEdit document;
  // Le fichier a changé sous la retouche : la source garde ce que dit le RSW.
  document.SetSoundEdit(SoundEdit{.sound_index = 0,
                                  .wave_file = "autre.wav",
                                  .origin = {1.0F, 0.0F, 2.0F},
                                  .hidden = true,
                                  .why = "x"});
  const std::vector<RswSound> raised = ActiveMapSounds(sources, document, kDay);
  REQUIRE(raised.size() == 2);
  CHECK(raised[0].wave_name == "eau.wav");
}

TEST_CASE("Sons du document : masqués, la nuit seulement, et leur plage d'heures",
          "[render][mapedit]") {
  const std::vector<RswSound> sources = TwoSources();
  ro::mapedit::MapEdit document;
  document.SetSoundEdit(SoundEdit{.sound_index = 0,
                                  .wave_file = "eau.wav",
                                  .origin = {1.0F, 0.0F, 2.0F},
                                  .hidden = true,
                                  .why = "x"});
  document.SetSoundEdit(SoundEdit{.sound_index = 1,
                                  .wave_file = "vent.wav",
                                  .origin = {30.0F, 0.0F, 40.0F},
                                  .night_only = true,
                                  .why = "x"});
  document.AddSound(AddedSound{.wave_file = "grillon.wav",
                               .position = {},
                               .night_only = true,
                               .hours = ro::mapedit::LitHours{.from_hour = 20.0F, .to_hour = 6.0F},
                               .why = "y"});
  // Le jour : la source masquée se tait, les deux « nuit seulement » aussi.
  CHECK(ActiveMapSounds(sources, document, kDay).empty());
  // La nuit sans heure du ciel : les deux sonnent.
  const std::vector<RswSound> night = ActiveMapSounds(sources, document, kNight);
  REQUIRE(night.size() == 2);
  CHECK(night[0].wave_name == "vent.wav");
  CHECK(night[1].wave_name == "grillon.wav");
  // Le ciel a une heure : la plage d'heures décide du son ajouté, la nuit de
  // l'autre.
  constexpr float kNoon = 12.0F;
  constexpr float kMidnight = 0.0F;
  const std::vector<RswSound> noon = ActiveMapSounds(sources, document, kNight, kNoon);
  REQUIRE(noon.size() == 1);
  CHECK(noon[0].wave_name == "vent.wav");
  CHECK(ActiveMapSounds(sources, document, kDay, kMidnight).size() == 1);
  // Une liste qui change se relève ; une liste égale se laisse sonner.
  CHECK_FALSE(ro::render::SameMapSounds(night, noon));
  CHECK(ro::render::SameMapSounds(night, ActiveMapSounds(sources, document, kNight)));
}
